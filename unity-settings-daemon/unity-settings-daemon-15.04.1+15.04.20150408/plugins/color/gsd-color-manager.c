/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <glib/gi18n.h>
#include <colord.h>
#include <libnotify/notify.h>
#include <gdk/gdk.h>
#include <stdlib.h>
#include <lcms2.h>
#include <canberra-gtk.h>

#include "gnome-settings-plugin.h"
#include "gnome-settings-profile.h"
#include "gnome-settings-session.h"
#include "gsd-color-manager.h"
#include "gcm-profile-store.h"
#include "gcm-dmi.h"
#include "gcm-edid.h"
#include "gsd-rr.h"

#define GSD_COLOR_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GSD_TYPE_COLOR_MANAGER, GsdColorManagerPrivate))

#define GCM_SESSION_NOTIFY_TIMEOUT                      30000 /* ms */
#define GCM_SETTINGS_RECALIBRATE_PRINTER_THRESHOLD      "recalibrate-printer-threshold"
#define GCM_SETTINGS_RECALIBRATE_DISPLAY_THRESHOLD      "recalibrate-display-threshold"

struct GsdColorManagerPrivate
{
        GDBusProxy      *session;
        CdClient        *client;
        GSettings       *settings;
        GcmProfileStore *profile_store;
        GcmDmi          *dmi;
        GsdRRScreen   *x11_screen;
        GHashTable      *edid_cache;
        GdkWindow       *gdk_window;
        gboolean         session_is_active;
        GHashTable      *device_assign_hash;
};

enum {
        PROP_0,
};

static void     gsd_color_manager_class_init  (GsdColorManagerClass *klass);
static void     gsd_color_manager_init        (GsdColorManager      *color_manager);
static void     gsd_color_manager_finalize    (GObject             *object);

G_DEFINE_TYPE (GsdColorManager, gsd_color_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

/* see http://www.oyranos.org/wiki/index.php?title=ICC_Profiles_in_X_Specification_0.3 */
#define GCM_ICC_PROFILE_IN_X_VERSION_MAJOR      0
#define GCM_ICC_PROFILE_IN_X_VERSION_MINOR      3

typedef struct {
        guint32          red;
        guint32          green;
        guint32          blue;
} GsdRROutputClutItem;

GQuark
gsd_color_manager_error_quark (void)
{
        static GQuark quark = 0;
        if (!quark)
                quark = g_quark_from_static_string ("gsd_color_manager_error");
        return quark;
}

static GcmEdid *
gcm_session_get_output_edid (GsdColorManager *manager, GsdRROutput *output, GError **error)
{
        const guint8 *data;
        gsize size;
        GcmEdid *edid = NULL;
        gboolean ret;

        /* can we find it in the cache */
        edid = g_hash_table_lookup (manager->priv->edid_cache,
                                    gsd_rr_output_get_name (output));
        if (edid != NULL) {
                g_object_ref (edid);
                goto out;
        }

        /* parse edid */
        data = gsd_rr_output_get_edid_data (output, &size);
        if (data == NULL || size == 0) {
                g_set_error_literal (error,
                                     GSD_RR_ERROR,
                                     GSD_RR_ERROR_UNKNOWN,
                                     "unable to get EDID for output");
                goto out;
        }
        edid = gcm_edid_new ();
        ret = gcm_edid_parse (edid, data, size, error);
        if (!ret) {
                g_object_unref (edid);
                edid = NULL;
                goto out;
        }

        /* add to cache */
        g_hash_table_insert (manager->priv->edid_cache,
                             g_strdup (gsd_rr_output_get_name (output)),
                             g_object_ref (edid));
out:
        return edid;
}

static gboolean
gcm_session_screen_set_icc_profile (GsdColorManager *manager,
                                    const gchar *filename,
                                    GError **error)
{
        gboolean ret;
        gchar *data = NULL;
        gsize length;
        guint version_data;
        GsdColorManagerPrivate *priv = manager->priv;

        g_return_val_if_fail (filename != NULL, FALSE);

        g_debug ("setting root window ICC profile atom from %s", filename);

        /* get contents of file */
        ret = g_file_get_contents (filename, &data, &length, error);
        if (!ret)
                goto out;

        /* set profile property */
        gdk_property_change (priv->gdk_window,
                             gdk_atom_intern_static_string ("_ICC_PROFILE"),
                             gdk_atom_intern_static_string ("CARDINAL"),
                             8,
                             GDK_PROP_MODE_REPLACE,
                             (const guchar *) data, length);

        /* set version property */
        version_data = GCM_ICC_PROFILE_IN_X_VERSION_MAJOR * 100 +
                        GCM_ICC_PROFILE_IN_X_VERSION_MINOR * 1;
        gdk_property_change (priv->gdk_window,
                             gdk_atom_intern_static_string ("_ICC_PROFILE_IN_X_VERSION"),
                             gdk_atom_intern_static_string ("CARDINAL"),
                             8,
                             GDK_PROP_MODE_REPLACE,
                             (const guchar *) &version_data, 1);
out:
        g_free (data);
        return ret;
}

static gchar *
gcm_session_get_output_id (GsdColorManager *manager, GsdRROutput *output)
{
        const gchar *name;
        const gchar *serial;
        const gchar *vendor;
        GcmEdid *edid = NULL;
        GString *device_id;
        GError *error = NULL;

        /* all output devices are prefixed with this */
        device_id = g_string_new ("xrandr");

        /* get the output EDID if possible */
        edid = gcm_session_get_output_edid (manager, output, &error);
        if (edid == NULL) {
                g_debug ("no edid for %s [%s], falling back to connection name",
                         gsd_rr_output_get_name (output),
                         error->message);
                g_error_free (error);
                g_string_append_printf (device_id,
                                        "-%s",
                                        gsd_rr_output_get_name (output));
                goto out;
        }

        /* check EDID data is okay to use */
        vendor = gcm_edid_get_vendor_name (edid);
        name = gcm_edid_get_monitor_name (edid);
        serial = gcm_edid_get_serial_number (edid);
        if (vendor == NULL && name == NULL && serial == NULL) {
                g_debug ("edid invalid for %s, falling back to connection name",
                         gsd_rr_output_get_name (output));
                g_string_append_printf (device_id,
                                        "-%s",
                                        gsd_rr_output_get_name (output));
                goto out;
        }

        /* use EDID data */
        if (vendor != NULL)
                g_string_append_printf (device_id, "-%s", vendor);
        if (name != NULL)
                g_string_append_printf (device_id, "-%s", name);
        if (serial != NULL)
                g_string_append_printf (device_id, "-%s", serial);
out:
        if (edid != NULL)
                g_object_unref (edid);
        return g_string_free (device_id, FALSE);
}

typedef struct {
        GsdColorManager         *manager;
        CdProfile               *profile;
        CdDevice                *device;
        guint32                  output_id;
} GcmSessionAsyncHelper;

static void
gcm_session_async_helper_free (GcmSessionAsyncHelper *helper)
{
        if (helper->manager != NULL)
                g_object_unref (helper->manager);
        if (helper->profile != NULL)
                g_object_unref (helper->profile);
        if (helper->device != NULL)
                g_object_unref (helper->device);
        g_free (helper);
}

static cmsBool
_cmsWriteTagTextAscii (cmsHPROFILE lcms_profile,
                       cmsTagSignature sig,
                       const gchar *text)
{
        cmsBool ret;
        cmsMLU *mlu = cmsMLUalloc (0, 1);
        cmsMLUsetASCII (mlu, "en", "US", text);
        ret = cmsWriteTag (lcms_profile, sig, mlu);
        cmsMLUfree (mlu);
        return ret;
}

static gboolean
gcm_utils_mkdir_for_filename (const gchar *filename, GError **error)
{
        gboolean ret = FALSE;
        GFile *file;
        GFile *parent_dir = NULL;

        /* get parent directory */
        file = g_file_new_for_path (filename);
        parent_dir = g_file_get_parent (file);
        if (parent_dir == NULL) {
                g_set_error (error,
                             GSD_COLOR_MANAGER_ERROR,
                             GSD_COLOR_MANAGER_ERROR_FAILED,
                             "could not get parent dir %s",
                             filename);
                goto out;
        }

        /* ensure desination does not already exist */
        ret = g_file_query_exists (parent_dir, NULL);
        if (ret)
                goto out;
        ret = g_file_make_directory_with_parents (parent_dir, NULL, error);
        if (!ret)
                goto out;
out:
        if (file != NULL)
                g_object_unref (file);
        if (parent_dir != NULL)
                g_object_unref (parent_dir);
        return ret;
}

#ifdef HAVE_NEW_LCMS
static wchar_t *
utf8_to_wchar_t (const char *src)
{
        size_t len;
        size_t converted;
        wchar_t *buf = NULL;

        len = mbstowcs (NULL, src, 0);
        if (len == (size_t) -1) {
                g_warning ("Invalid UTF-8 in string %s", src);
                goto out;
        }
        len += 1;
        buf = g_malloc (sizeof (wchar_t) * len);
        converted = mbstowcs (buf, src, len - 1);
        g_assert (converted != -1);
        buf[converted] = '\0';
out:
        return buf;
}

static cmsBool
_cmsDictAddEntryAscii (cmsHANDLE dict,
                       const gchar *key,
                       const gchar *value)
{
        cmsBool ret = FALSE;
        wchar_t *mb_key = NULL;
        wchar_t *mb_value = NULL;

        mb_key = utf8_to_wchar_t (key);
        if (mb_key == NULL)
                goto out;
        mb_value = utf8_to_wchar_t (value);
        if (mb_value == NULL)
                goto out;
        ret = cmsDictAddEntry (dict, mb_key, mb_value, NULL, NULL);
out:
        g_free (mb_key);
        g_free (mb_value);
        return ret;
}
#endif /* HAVE_NEW_LCMS */

static gboolean
gcm_apply_create_icc_profile_for_edid (GsdColorManager *manager,
                                       CdDevice *device,
                                       GcmEdid *edid,
                                       const gchar *filename,
                                       GError **error)
{
        const CdColorYxy *tmp;
        cmsCIExyYTRIPLE chroma;
        cmsCIExyY white_point;
        cmsHPROFILE lcms_profile = NULL;
        cmsToneCurve *transfer_curve[3] = { NULL, NULL, NULL };
        const gchar *data;
        gboolean ret = FALSE;
        gchar *str;
        gfloat edid_gamma;
        gfloat localgamma;
#ifdef HAVE_NEW_LCMS
        cmsHANDLE dict = NULL;
#endif
        GsdColorManagerPrivate *priv = manager->priv;

        /* ensure the per-user directory exists */
        ret = gcm_utils_mkdir_for_filename (filename, error);
        if (!ret)
                goto out;

        /* copy color data from our structures */
        tmp = gcm_edid_get_red (edid);
        chroma.Red.x = tmp->x;
        chroma.Red.y = tmp->y;
        tmp = gcm_edid_get_green (edid);
        chroma.Green.x = tmp->x;
        chroma.Green.y = tmp->y;
        tmp = gcm_edid_get_blue (edid);
        chroma.Blue.x = tmp->x;
        chroma.Blue.y = tmp->y;
        tmp = gcm_edid_get_white (edid);
        white_point.x = tmp->x;
        white_point.y = tmp->y;
        white_point.Y = 1.0;

        /* estimate the transfer function for the gamma */
        localgamma = gcm_edid_get_gamma (edid);
        transfer_curve[0] = transfer_curve[1] = transfer_curve[2] = cmsBuildGamma (NULL, localgamma);

        /* create our generated profile */
        lcms_profile = cmsCreateRGBProfile (&white_point, &chroma, transfer_curve);
        if (lcms_profile == NULL) {
                g_set_error (error,
                             GSD_COLOR_MANAGER_ERROR,
                             GSD_COLOR_MANAGER_ERROR_FAILED,
                             "failed to create profile");
                goto out;
        }

        cmsSetColorSpace (lcms_profile, cmsSigRgbData);
        cmsSetPCS (lcms_profile, cmsSigXYZData);
        cmsSetHeaderRenderingIntent (lcms_profile, INTENT_PERCEPTUAL);
        cmsSetDeviceClass (lcms_profile, cmsSigDisplayClass);

        /* copyright */
        ret = _cmsWriteTagTextAscii (lcms_profile,
                                     cmsSigCopyrightTag,
                                     /* deliberately not translated */
                                     "This profile is free of known copyright restrictions.");
        if (!ret) {
                g_set_error_literal (error,
                                     GSD_COLOR_MANAGER_ERROR,
                                     GSD_COLOR_MANAGER_ERROR_FAILED,
                                     "failed to write copyright");
                goto out;
        }

        /* set model */
        data = gcm_edid_get_monitor_name (edid);
        if (data == NULL)
                data = gcm_dmi_get_name (priv->dmi);
        if (data == NULL)
                data = "Unknown monitor";
        ret = _cmsWriteTagTextAscii (lcms_profile,
                                     cmsSigDeviceModelDescTag,
                                     data);
        if (!ret) {
                g_set_error_literal (error,
                                     GSD_COLOR_MANAGER_ERROR,
                                     GSD_COLOR_MANAGER_ERROR_FAILED,
                                     "failed to write model");
                goto out;
        }

        /* write title */
        ret = _cmsWriteTagTextAscii (lcms_profile,
                                     cmsSigProfileDescriptionTag,
                                     data);
        if (!ret) {
                g_set_error_literal (error, GSD_COLOR_MANAGER_ERROR, GSD_COLOR_MANAGER_ERROR_FAILED, "failed to write description");
                goto out;
        }

        /* get manufacturer */
        data = gcm_edid_get_vendor_name (edid);
        if (data == NULL)
                data = gcm_dmi_get_vendor (priv->dmi);
        if (data == NULL)
                data = "Unknown vendor";
        ret = _cmsWriteTagTextAscii (lcms_profile,
                                     cmsSigDeviceMfgDescTag,
                                     data);
        if (!ret) {
                g_set_error_literal (error,
                                     GSD_COLOR_MANAGER_ERROR,
                                     GSD_COLOR_MANAGER_ERROR_FAILED,
                                     "failed to write manufacturer");
                goto out;
        }

#ifdef HAVE_NEW_LCMS
        /* just create a new dict */
        dict = cmsDictAlloc (NULL);

        /* set the framework creator metadata */
        _cmsDictAddEntryAscii (dict,
                               CD_PROFILE_METADATA_CMF_PRODUCT,
                               PACKAGE_NAME);
        _cmsDictAddEntryAscii (dict,
                               CD_PROFILE_METADATA_CMF_BINARY,
                               PACKAGE_NAME);
        _cmsDictAddEntryAscii (dict,
                               CD_PROFILE_METADATA_CMF_VERSION,
                               PACKAGE_VERSION);
        _cmsDictAddEntryAscii (dict,
                               CD_PROFILE_METADATA_MAPPING_DEVICE_ID,
                               cd_device_get_id (device));

        /* set the data source so we don't ever prompt the user to
         * recalibrate (as the EDID data won't have changed) */
        _cmsDictAddEntryAscii (dict,
                               CD_PROFILE_METADATA_DATA_SOURCE,
                               CD_PROFILE_METADATA_DATA_SOURCE_EDID);

        /* set 'ICC meta Tag for Monitor Profiles' data */
        _cmsDictAddEntryAscii (dict, "EDID_md5", gcm_edid_get_checksum (edid));
        data = gcm_edid_get_monitor_name (edid);
        if (data != NULL)
                _cmsDictAddEntryAscii (dict, "EDID_model", data);
        data = gcm_edid_get_serial_number (edid);
        if (data != NULL)
                _cmsDictAddEntryAscii (dict, "EDID_serial", data);
        data = gcm_edid_get_pnp_id (edid);
        if (data != NULL)
                _cmsDictAddEntryAscii (dict, "EDID_mnft", data);
        data = gcm_edid_get_vendor_name (edid);
        if (data != NULL)
                _cmsDictAddEntryAscii (dict, "EDID_manufacturer", data);
        edid_gamma = gcm_edid_get_gamma (edid);
        if (edid_gamma > 0.0 && edid_gamma < 10.0) {
                str = g_strdup_printf ("%f", edid_gamma);
                _cmsDictAddEntryAscii (dict, "EDID_gamma", str);
                g_free (str);
        }

        /* also add the primaries */
        str = g_strdup_printf ("%f", chroma.Red.x);
        _cmsDictAddEntryAscii (dict, "EDID_red_x", str);
        g_free (str);
        str = g_strdup_printf ("%f", chroma.Red.y);
        _cmsDictAddEntryAscii (dict, "EDID_red_y", str);
        g_free (str);
        str = g_strdup_printf ("%f", chroma.Green.x);
        _cmsDictAddEntryAscii (dict, "EDID_green_x", str);
        g_free (str);
        str = g_strdup_printf ("%f", chroma.Green.y);
        _cmsDictAddEntryAscii (dict, "EDID_green_y", str);
        g_free (str);
        str = g_strdup_printf ("%f", chroma.Blue.x);
        _cmsDictAddEntryAscii (dict, "EDID_blue_x", str);
        g_free (str);
        str = g_strdup_printf ("%f", chroma.Blue.y);
        _cmsDictAddEntryAscii (dict, "EDID_blue_y", str);
        g_free (str);

        /* write new tag */
        ret = cmsWriteTag (lcms_profile, cmsSigMetaTag, dict);
        if (!ret) {
                g_set_error_literal (error,
                                     GSD_COLOR_MANAGER_ERROR,
                                     GSD_COLOR_MANAGER_ERROR_FAILED,
                                     "failed to write profile metadata");
                goto out;
        }
#endif /* HAVE_NEW_LCMS */

        /* write profile id */
        ret = cmsMD5computeID (lcms_profile);
        if (!ret) {
                g_set_error_literal (error,
                                     GSD_COLOR_MANAGER_ERROR,
                                     GSD_COLOR_MANAGER_ERROR_FAILED,
                                     "failed to write profile id");
                goto out;
        }

        /* save, TODO: get error */
        cmsSaveProfileToFile (lcms_profile, filename);
        ret = TRUE;
out:
#ifdef HAVE_NEW_LCMS
        if (dict != NULL)
                cmsDictFree (dict);
#endif
        if (*transfer_curve != NULL)
                cmsFreeToneCurve (*transfer_curve);
        return ret;
}

static GPtrArray *
gcm_session_generate_vcgt (CdProfile *profile, guint size)
{
        GsdRROutputClutItem *tmp;
        GPtrArray *array = NULL;
        const cmsToneCurve **vcgt;
        cmsFloat32Number in;
        guint i;
        const gchar *filename;
        cmsHPROFILE lcms_profile = NULL;

        /* invalid size */
        if (size == 0)
                goto out;

        /* not an actual profile */
        filename = cd_profile_get_filename (profile);
        if (filename == NULL)
                goto out;

        /* open file */
        lcms_profile = cmsOpenProfileFromFile (filename, "r");
        if (lcms_profile == NULL)
                goto out;

        /* get tone curves from profile */
        vcgt = cmsReadTag (lcms_profile, cmsSigVcgtTag);
        if (vcgt == NULL || vcgt[0] == NULL) {
                g_debug ("profile does not have any VCGT data");
                goto out;
        }

        /* create array */
        array = g_ptr_array_new_with_free_func (g_free);
        for (i = 0; i < size; i++) {
                in = (gdouble) i / (gdouble) (size - 1);
                tmp = g_new0 (GsdRROutputClutItem, 1);
                tmp->red = cmsEvalToneCurveFloat(vcgt[0], in) * (gdouble) 0xffff;
                tmp->green = cmsEvalToneCurveFloat(vcgt[1], in) * (gdouble) 0xffff;
                tmp->blue = cmsEvalToneCurveFloat(vcgt[2], in) * (gdouble) 0xffff;
                g_ptr_array_add (array, tmp);
        }
out:
        if (lcms_profile != NULL)
                cmsCloseProfile (lcms_profile);
        return array;
}

static guint
gsd_rr_output_get_gamma_size (GsdRROutput *output)
{
        GsdRRCrtc *crtc;
        gint len = 0;

        crtc = gsd_rr_output_get_crtc (output);
        if (crtc == NULL)
                return 0;
        gsd_rr_crtc_get_gamma (crtc,
                                 &len,
                                 NULL, NULL, NULL);
        return (guint) len;
}

static gboolean
gcm_session_output_set_gamma (GsdRROutput *output,
                              GPtrArray *array,
                              GError **error)
{
        gboolean ret = TRUE;
        guint16 *red = NULL;
        guint16 *green = NULL;
        guint16 *blue = NULL;
        guint i;
        GsdRROutputClutItem *data;
        GsdRRCrtc *crtc;

        /* no length? */
        if (array->len == 0) {
                ret = FALSE;
                g_set_error_literal (error,
                                     GSD_COLOR_MANAGER_ERROR,
                                     GSD_COLOR_MANAGER_ERROR_FAILED,
                                     "no data in the CLUT array");
                goto out;
        }

        /* convert to a type X understands */
        red = g_new (guint16, array->len);
        green = g_new (guint16, array->len);
        blue = g_new (guint16, array->len);
        for (i = 0; i < array->len; i++) {
                data = g_ptr_array_index (array, i);
                red[i] = data->red;
                green[i] = data->green;
                blue[i] = data->blue;
        }

        /* send to LUT */
        crtc = gsd_rr_output_get_crtc (output);
        if (crtc == NULL) {
                ret = FALSE;
                g_set_error (error,
                             GSD_COLOR_MANAGER_ERROR,
                             GSD_COLOR_MANAGER_ERROR_FAILED,
                             "failed to get ctrc for %s",
                             gsd_rr_output_get_name (output));
                goto out;
        }
        gsd_rr_crtc_set_gamma (crtc, array->len,
                                 red, green, blue);
out:
        g_free (red);
        g_free (green);
        g_free (blue);
        return ret;
}

static gboolean
gcm_session_device_set_gamma (GsdRROutput *output,
                              CdProfile *profile,
                              GError **error)
{
        gboolean ret = FALSE;
        guint size;
        GPtrArray *clut = NULL;

        /* create a lookup table */
        size = gsd_rr_output_get_gamma_size (output);
        if (size == 0) {
                ret = TRUE;
                goto out;
        }
        clut = gcm_session_generate_vcgt (profile, size);
        if (clut == NULL) {
                g_set_error_literal (error,
                                     GSD_COLOR_MANAGER_ERROR,
                                     GSD_COLOR_MANAGER_ERROR_FAILED,
                                     "failed to generate vcgt");
                goto out;
        }

        /* apply the vcgt to this output */
        ret = gcm_session_output_set_gamma (output, clut, error);
        if (!ret)
                goto out;
out:
        if (clut != NULL)
                g_ptr_array_unref (clut);
        return ret;
}

static gboolean
gcm_session_device_reset_gamma (GsdRROutput *output,
                                GError **error)
{
        gboolean ret;
        guint i;
        guint size;
        guint32 value;
        GPtrArray *clut;
        GsdRROutputClutItem *data;

        /* create a linear ramp */
        g_debug ("falling back to dummy ramp");
        clut = g_ptr_array_new_with_free_func (g_free);
        size = gsd_rr_output_get_gamma_size (output);
        if (size == 0) {
                ret = TRUE;
                goto out;
        }
        for (i = 0; i < size; i++) {
                value = (i * 0xffff) / (size - 1);
                data = g_new0 (GsdRROutputClutItem, 1);
                data->red = value;
                data->green = value;
                data->blue = value;
                g_ptr_array_add (clut, data);
        }

        /* apply the vcgt to this output */
        ret = gcm_session_output_set_gamma (output, clut, error);
        if (!ret)
                goto out;
out:
        g_ptr_array_unref (clut);
        return ret;
}

static GsdRROutput *
gcm_session_get_x11_output_by_id (GsdColorManager *manager,
                                  const gchar *device_id,
                                  GError **error)
{
        gchar *output_id;
        GsdRROutput *output = NULL;
        GsdRROutput **outputs = NULL;
        guint i;
        GsdColorManagerPrivate *priv = manager->priv;

        /* search all X11 outputs for the device id */
        outputs = gsd_rr_screen_list_outputs (priv->x11_screen);
        if (outputs == NULL) {
                g_set_error_literal (error,
                                     GSD_COLOR_MANAGER_ERROR,
                                     GSD_COLOR_MANAGER_ERROR_FAILED,
                                     "Failed to get outputs");
                goto out;
        }
        for (i = 0; outputs[i] != NULL && output == NULL; i++) {
                if (!gsd_rr_output_is_connected (outputs[i]))
                        continue;
                output_id = gcm_session_get_output_id (manager, outputs[i]);
                if (g_strcmp0 (output_id, device_id) == 0)
                        output = outputs[i];
                g_free (output_id);
        }
        if (output == NULL) {
                g_set_error (error,
                             GSD_COLOR_MANAGER_ERROR,
                             GSD_COLOR_MANAGER_ERROR_FAILED,
                             "Failed to find output %s",
                             device_id);
        }
out:
        return output;
}

/* this function is more complicated than it should be, due to the
 * fact that XOrg sometimes assigns no primary devices when using
 * "xrandr --auto" or when the version of RANDR is < 1.3 */
static gboolean
gcm_session_use_output_profile_for_screen (GsdColorManager *manager,
                                           GsdRROutput *output)
{
        gboolean has_laptop = FALSE;
        gboolean has_primary = FALSE;
        GsdRROutput **outputs;
        GsdRROutput *connected = NULL;
        guint i;

        /* do we have any screens marked as primary */
        outputs = gsd_rr_screen_list_outputs (manager->priv->x11_screen);
        if (outputs == NULL || outputs[0] == NULL) {
                g_warning ("failed to get outputs");
                return FALSE;
        }
        for (i = 0; outputs[i] != NULL; i++) {
                if (!gsd_rr_output_is_connected (outputs[i]))
                        continue;
                if (connected == NULL)
                        connected = outputs[i];
                if (gsd_rr_output_get_is_primary (outputs[i]))
                        has_primary = TRUE;
                if (gsd_rr_output_is_laptop (outputs[i]))
                        has_laptop = TRUE;
        }

        /* we have an assigned primary device, are we that? */
        if (has_primary)
                return gsd_rr_output_get_is_primary (output);

        /* choosing the internal panel is probably sane */
        if (has_laptop)
                return gsd_rr_output_is_laptop (output);

        /* we have to choose one, so go for the first connected device */
        if (connected != NULL)
                return gsd_rr_output_get_id (connected) == gsd_rr_output_get_id (output);

        return FALSE;
}

/* TODO: remove when we can dep on a released version of colord */
#ifndef CD_PROFILE_METADATA_SCREEN_BRIGHTNESS
#define CD_PROFILE_METADATA_SCREEN_BRIGHTNESS		"SCREEN_brightness"
#endif

#define GSD_DBUS_NAME_POWER		GSD_DBUS_NAME ".Power"
#define GSD_DBUS_INTERFACE_POWER_SCREEN	GSD_DBUS_BASE_INTERFACE ".Power.Screen"
#define GSD_DBUS_PATH_POWER		GSD_DBUS_PATH "/Power"

static void
gcm_session_set_output_percentage_cb (GObject *source_object,
                                      GAsyncResult *res,
                                      gpointer user_data)
{
        GDBusConnection *connection = G_DBUS_CONNECTION (source_object);
        GError *error = NULL;
        GVariant *retval;
        retval = g_dbus_connection_call_finish (connection,
                                                res,
                                                &error);
        if (retval == NULL) {
                g_warning ("failed to set output brightness: %s",
                           error->message);
                g_error_free (error);
                return;
        }
        g_variant_unref (retval);
}

static void
gcm_session_set_output_percentage (guint percentage)
{
        GDBusConnection *connection;

        /* get a ref to the existing bus connection */
        connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
        if (connection == NULL)
                return;
        g_dbus_connection_call (connection,
                                GSD_DBUS_NAME_POWER,
                                GSD_DBUS_PATH_POWER,
                                GSD_DBUS_INTERFACE_POWER_SCREEN,
                                "SetPercentage",
                                g_variant_new ("(u)", percentage),
                                NULL,
                                G_DBUS_CALL_FLAGS_NONE,
                                -1, NULL,
                                gcm_session_set_output_percentage_cb, NULL);
        g_object_unref (connection);
}

static void
gcm_session_device_assign_profile_connect_cb (GObject *object,
                                              GAsyncResult *res,
                                              gpointer user_data)
{
        CdProfile *profile = CD_PROFILE (object);
        const gchar *brightness_profile;
        const gchar *filename;
        gboolean ret;
        GError *error = NULL;
        GsdRROutput *output;
        guint brightness_percentage;
        GcmSessionAsyncHelper *helper = (GcmSessionAsyncHelper *) user_data;
        GsdColorManager *manager = GSD_COLOR_MANAGER (helper->manager);

        /* get properties */
        ret = cd_profile_connect_finish (profile, res, &error);
        if (!ret) {
                g_warning ("failed to connect to profile: %s",
                           error->message);
                g_error_free (error);
                goto out;
        }

        /* get the filename */
        filename = cd_profile_get_filename (profile);
        g_assert (filename != NULL);

        /* get the output (can't save in helper as GsdRROutput isn't
         * a GObject, just a pointer */
        output = gsd_rr_screen_get_output_by_id (manager->priv->x11_screen,
                                                   helper->output_id);
        if (output == NULL)
                goto out;

        /* if output is a laptop screen and the profile has a
         * calibration brightness then set this new brightness */
        brightness_profile = cd_profile_get_metadata_item (profile,
                                                           CD_PROFILE_METADATA_SCREEN_BRIGHTNESS);
        if (gsd_rr_output_is_laptop (output) &&
            brightness_profile != NULL) {
                /* the percentage is stored in the profile metadata as
                 * a string, not ideal, but it's all we have... */
                brightness_percentage = atoi (brightness_profile);
                gcm_session_set_output_percentage (brightness_percentage);
        }

        /* set the _ICC_PROFILE atom */
        ret = gcm_session_use_output_profile_for_screen (manager, output);
        if (ret) {
                ret = gcm_session_screen_set_icc_profile (manager,
                                                          filename,
                                                          &error);
                if (!ret) {
                        g_warning ("failed to set screen _ICC_PROFILE: %s",
                                   error->message);
                        g_clear_error (&error);
                }
        }

        /* create a vcgt for this icc file */
        ret = cd_profile_get_has_vcgt (profile);
        if (ret) {
                ret = gcm_session_device_set_gamma (output,
                                                    profile,
                                                    &error);
                if (!ret) {
                        g_warning ("failed to set %s gamma tables: %s",
                                   cd_device_get_id (helper->device),
                                   error->message);
                        g_error_free (error);
                        goto out;
                }
        } else {
                ret = gcm_session_device_reset_gamma (output,
                                                      &error);
                if (!ret) {
                        g_warning ("failed to reset %s gamma tables: %s",
                                   cd_device_get_id (helper->device),
                                   error->message);
                        g_error_free (error);
                        goto out;
                }
        }
out:
        gcm_session_async_helper_free (helper);
}

/*
 * Check to see if the on-disk profile has the MAPPING_device_id
 * metadata, and if not, we should delete the profile and re-create it
 * so that it gets mapped by the daemon.
 */
static gboolean
gcm_session_check_profile_device_md (const gchar *filename)
{
        cmsHANDLE dict;
        cmsHPROFILE lcms_profile;
        const cmsDICTentry *entry;
        gboolean ret = FALSE;
        gchar ascii_name[1024];
        gsize len;

        /* parse the ICC file */
        lcms_profile = cmsOpenProfileFromFile (filename, "r");
        if (lcms_profile == NULL)
                goto out;

        /* does profile have metadata? */
        dict = cmsReadTag (lcms_profile, cmsSigMetaTag);
        if (dict == NULL) {
                g_debug ("auto-edid profile is old, and contains no metadata");
                goto out;
        }
        for (entry = cmsDictGetEntryList (dict);
             entry != NULL;
             entry = cmsDictNextEntry (entry)) {
                if (entry->Name == NULL)
                        continue;
                len = wcstombs (ascii_name, entry->Name, sizeof (ascii_name));
                if (len == (gsize) -1)
                        continue;
                if (g_strcmp0 (ascii_name,
                               CD_PROFILE_METADATA_MAPPING_DEVICE_ID) == 0) {
                        ret = TRUE;
                        goto out;
                }
        }
        g_debug ("auto-edid profile is old, and contains no %s data",
                 CD_PROFILE_METADATA_MAPPING_DEVICE_ID);
out:
        if (lcms_profile != NULL)
                cmsCloseProfile (lcms_profile);
        return ret;
}

static void
gcm_session_device_assign_connect_cb (GObject *object,
                                      GAsyncResult *res,
                                      gpointer user_data)
{
        CdDeviceKind kind;
        CdProfile *profile = NULL;
        gboolean ret;
        gchar *autogen_filename = NULL;
        gchar *autogen_path = NULL;
        GcmEdid *edid = NULL;
        GsdRROutput *output = NULL;
        GError *error = NULL;
        const gchar *xrandr_id;
        GcmSessionAsyncHelper *helper;
        CdDevice *device = CD_DEVICE (object);
        GsdColorManager *manager = GSD_COLOR_MANAGER (user_data);
        GsdColorManagerPrivate *priv = manager->priv;

        /* remove from assign array */
        g_hash_table_remove (manager->priv->device_assign_hash,
                             cd_device_get_object_path (device));

        /* get properties */
        ret = cd_device_connect_finish (device, res, &error);
        if (!ret) {
                g_warning ("failed to connect to device: %s",
                           error->message);
                g_error_free (error);
                goto out;
        }

        /* check we care */
        kind = cd_device_get_kind (device);
        if (kind != CD_DEVICE_KIND_DISPLAY)
                goto out;

        g_debug ("need to assign display device %s",
                 cd_device_get_id (device));

        /* get the GsdRROutput for the device id */
        xrandr_id = cd_device_get_id (device);
        output = gcm_session_get_x11_output_by_id (manager,
                                                   xrandr_id,
                                                   &error);
        if (output == NULL) {
                g_warning ("no %s device found: %s",
                           cd_device_get_id (device),
                           error->message);
                g_error_free (error);
                goto out;
        }

        /* create profile from device edid if it exists */
        edid = gcm_session_get_output_edid (manager, output, &error);
        if (edid == NULL) {
                g_warning ("unable to get EDID for %s: %s",
                           cd_device_get_id (device),
                           error->message);
                g_clear_error (&error);

        } else {
                autogen_filename = g_strdup_printf ("edid-%s.icc",
                                                    gcm_edid_get_checksum (edid));
                autogen_path = g_build_filename (g_get_user_data_dir (),
                                                 "icc", autogen_filename, NULL);

                /* check if auto-profile has up-to-date metadata */
                if (gcm_session_check_profile_device_md (autogen_path)) {
                        g_debug ("auto-profile edid %s exists with md", autogen_path);
                } else {
                        g_debug ("auto-profile edid does not exist, creating as %s",
                                 autogen_path);
                        ret = gcm_apply_create_icc_profile_for_edid (manager,
                                                                     device,
                                                                     edid,
                                                                     autogen_path,
                                                                     &error);
                        if (!ret) {
                                g_warning ("failed to create profile from EDID data: %s",
                                             error->message);
                                g_clear_error (&error);
                        }
                }
        }

        /* get the default profile for the device */
        profile = cd_device_get_default_profile (device);
        if (profile == NULL) {
                g_debug ("%s has no default profile to set",
                         cd_device_get_id (device));

                /* the default output? */
                if (gsd_rr_output_get_is_primary (output)) {
                        gdk_property_delete (priv->gdk_window,
                                             gdk_atom_intern_static_string ("_ICC_PROFILE"));
                        gdk_property_delete (priv->gdk_window,
                                             gdk_atom_intern_static_string ("_ICC_PROFILE_IN_X_VERSION"));
                }

                /* reset, as we want linear profiles for profiling */
                ret = gcm_session_device_reset_gamma (output,
                                                      &error);
                if (!ret) {
                        g_warning ("failed to reset %s gamma tables: %s",
                                   cd_device_get_id (device),
                                   error->message);
                        g_error_free (error);
                        goto out;
                }
                goto out;
        }

        /* get properties */
        helper = g_new0 (GcmSessionAsyncHelper, 1);
        helper->output_id = gsd_rr_output_get_id (output);
        helper->manager = g_object_ref (manager);
        helper->device = g_object_ref (device);
        cd_profile_connect (profile,
                            NULL,
                            gcm_session_device_assign_profile_connect_cb,
                            helper);
out:
        g_free (autogen_filename);
        g_free (autogen_path);
        if (edid != NULL)
                g_object_unref (edid);
        if (profile != NULL)
                g_object_unref (profile);
}

static void
gcm_session_device_assign (GsdColorManager *manager, CdDevice *device)
{
        const gchar *key;
        gpointer found;

        /* are we already assigning this device */
        key = cd_device_get_object_path (device);
        found = g_hash_table_lookup (manager->priv->device_assign_hash, key);
        if (found != NULL) {
                g_debug ("assign for %s already in progress", key);
                return;
        }
        g_hash_table_insert (manager->priv->device_assign_hash,
                             g_strdup (key),
                             GINT_TO_POINTER (TRUE));
        cd_device_connect (device,
                           NULL,
                           gcm_session_device_assign_connect_cb,
                           manager);
}

static void
gcm_session_device_added_assign_cb (CdClient *client,
                                    CdDevice *device,
                                    GsdColorManager *manager)
{
        gcm_session_device_assign (manager, device);
}

static void
gcm_session_device_changed_assign_cb (CdClient *client,
                                      CdDevice *device,
                                      GsdColorManager *manager)
{
        g_debug ("%s changed", cd_device_get_object_path (device));
        gcm_session_device_assign (manager, device);
}

static void
gcm_session_create_device_cb (GObject *object,
                              GAsyncResult *res,
                              gpointer user_data)
{
        CdDevice *device;
        GError *error = NULL;

        device = cd_client_create_device_finish (CD_CLIENT (object),
                                                 res,
                                                 &error);
        if (device == NULL) {
                if (error->domain != CD_CLIENT_ERROR ||
                    error->code != CD_CLIENT_ERROR_ALREADY_EXISTS) {
                        g_warning ("failed to create device: %s",
                                   error->message);
                }
                g_error_free (error);
                return;
        }
        g_object_unref (device);
}

static void
gcm_session_add_x11_output (GsdColorManager *manager, GsdRROutput *output)
{
        const gchar *edid_checksum = NULL;
        const gchar *model = NULL;
        const gchar *serial = NULL;
        const gchar *vendor = NULL;
        gboolean ret;
        gchar *device_id = NULL;
        GcmEdid *edid;
        GError *error = NULL;
        GHashTable *device_props = NULL;
        GsdColorManagerPrivate *priv = manager->priv;

        /* try to get edid */
        edid = gcm_session_get_output_edid (manager, output, &error);
        if (edid == NULL) {
                g_warning ("failed to get edid: %s",
                           error->message);
                g_clear_error (&error);
        }

        /* prefer DMI data for the internal output */
        ret = gsd_rr_output_is_laptop (output);
        if (ret) {
                model = gcm_dmi_get_name (priv->dmi);
                vendor = gcm_dmi_get_vendor (priv->dmi);
        }

        /* use EDID data if we have it */
        if (edid != NULL) {
                edid_checksum = gcm_edid_get_checksum (edid);
                if (model == NULL)
                        model = gcm_edid_get_monitor_name (edid);
                if (vendor == NULL)
                        vendor = gcm_edid_get_vendor_name (edid);
                if (serial == NULL)
                        serial = gcm_edid_get_serial_number (edid);
        }

        /* ensure mandatory fields are set */
        if (model == NULL)
                model = gsd_rr_output_get_name (output);
        if (vendor == NULL)
                vendor = "unknown";
        if (serial == NULL)
                serial = "unknown";

        device_id = gcm_session_get_output_id (manager, output);
        g_debug ("output %s added", device_id);
        device_props = g_hash_table_new_full (g_str_hash, g_str_equal,
                                              NULL, NULL);
        g_hash_table_insert (device_props,
                             (gpointer) CD_DEVICE_PROPERTY_KIND,
                             (gpointer) cd_device_kind_to_string (CD_DEVICE_KIND_DISPLAY));
        g_hash_table_insert (device_props,
                             (gpointer) CD_DEVICE_PROPERTY_MODE,
                             (gpointer) cd_device_mode_to_string (CD_DEVICE_MODE_PHYSICAL));
        g_hash_table_insert (device_props,
                             (gpointer) CD_DEVICE_PROPERTY_COLORSPACE,
                             (gpointer) cd_colorspace_to_string (CD_COLORSPACE_RGB));
        g_hash_table_insert (device_props,
                             (gpointer) CD_DEVICE_PROPERTY_VENDOR,
                             (gpointer) vendor);
        g_hash_table_insert (device_props,
                             (gpointer) CD_DEVICE_PROPERTY_MODEL,
                             (gpointer) model);
        g_hash_table_insert (device_props,
                             (gpointer) CD_DEVICE_PROPERTY_SERIAL,
                             (gpointer) serial);
        g_hash_table_insert (device_props,
                             (gpointer) CD_DEVICE_METADATA_XRANDR_NAME,
                             (gpointer) gsd_rr_output_get_name (output));
#if CD_CHECK_VERSION(0,1,25)
        g_hash_table_insert (device_props,
                             (gpointer) CD_DEVICE_METADATA_OUTPUT_PRIORITY,
                             gsd_rr_output_get_is_primary (output) ?
                             (gpointer) CD_DEVICE_METADATA_OUTPUT_PRIORITY_PRIMARY :
                             (gpointer) CD_DEVICE_METADATA_OUTPUT_PRIORITY_SECONDARY);
#endif
#if CD_CHECK_VERSION(0,1,34)
        if (edid_checksum != NULL) {
                g_hash_table_insert (device_props,
                                     (gpointer) CD_DEVICE_METADATA_OUTPUT_EDID_MD5,
                                     (gpointer) edid_checksum);
        }
#endif
#if CD_CHECK_VERSION(0,1,27)
        /* set this so we can call the device a 'Laptop Screen' in the
         * control center main panel */
        if (gsd_rr_output_is_laptop (output)) {
                g_hash_table_insert (device_props,
                                     (gpointer) CD_DEVICE_PROPERTY_EMBEDDED,
                                     NULL);
        }
#endif
        cd_client_create_device (priv->client,
                                 device_id,
                                 CD_OBJECT_SCOPE_TEMP,
                                 device_props,
                                 NULL,
                                 gcm_session_create_device_cb,
                                 manager);
        g_free (device_id);
        if (device_props != NULL)
                g_hash_table_unref (device_props);
        if (edid != NULL)
                g_object_unref (edid);
}


static void
gsd_rr_screen_output_added_cb (GsdRRScreen *screen,
                                GsdRROutput *output,
                                GsdColorManager *manager)
{
        gcm_session_add_x11_output (manager, output);
}

static void
gcm_session_screen_removed_delete_device_cb (GObject *object, GAsyncResult *res, gpointer user_data)
{
        gboolean ret;
        GError *error = NULL;

        /* deleted device */
        ret = cd_client_delete_device_finish (CD_CLIENT (object),
                                              res,
                                              &error);
        if (!ret) {
                g_warning ("failed to delete device: %s",
                           error->message);
                g_error_free (error);
        }
}

static void
gcm_session_screen_removed_find_device_cb (GObject *object, GAsyncResult *res, gpointer user_data)
{
        GError *error = NULL;
        CdDevice *device;
        GsdColorManager *manager = GSD_COLOR_MANAGER (user_data);

        device = cd_client_find_device_finish (manager->priv->client,
                                               res,
                                               &error);
        if (device == NULL) {
                g_warning ("failed to find device: %s",
                           error->message);
                g_error_free (error);
                return;
        }
        g_debug ("output %s found, and will be removed",
                 cd_device_get_object_path (device));
        cd_client_delete_device (manager->priv->client,
                                 device,
                                 NULL,
                                 gcm_session_screen_removed_delete_device_cb,
                                 manager);
        g_object_unref (device);
}

static void
gsd_rr_screen_output_removed_cb (GsdRRScreen *screen,
                                   GsdRROutput *output,
                                   GsdColorManager *manager)
{
        g_debug ("output %s removed",
                 gsd_rr_output_get_name (output));
        g_hash_table_remove (manager->priv->edid_cache,
                             gsd_rr_output_get_name (output));
        cd_client_find_device_by_property (manager->priv->client,
                                           CD_DEVICE_METADATA_XRANDR_NAME,
                                           gsd_rr_output_get_name (output),
                                           NULL,
                                           gcm_session_screen_removed_find_device_cb,
                                           manager);
}

static void
gcm_session_get_devices_cb (GObject *object, GAsyncResult *res, gpointer user_data)
{
        CdDevice *device;
        GError *error = NULL;
        GPtrArray *array;
        guint i;
        GsdColorManager *manager = GSD_COLOR_MANAGER (user_data);

        array = cd_client_get_devices_finish (CD_CLIENT (object), res, &error);
        if (array == NULL) {
                g_warning ("failed to get devices: %s",
                           error->message);
                g_error_free (error);
                goto out;
        }
        for (i = 0; i < array->len; i++) {
                device = g_ptr_array_index (array, i);
                gcm_session_device_assign (manager, device);
        }
out:
        if (array != NULL)
                g_ptr_array_unref (array);
}

static void
gcm_session_profile_gamma_find_device_cb (GObject *object,
                                          GAsyncResult *res,
                                          gpointer user_data)
{
        CdClient *client = CD_CLIENT (object);
        CdDevice *device = NULL;
        GError *error = NULL;
        GsdColorManager *manager = GSD_COLOR_MANAGER (user_data);

        device = cd_client_find_device_by_property_finish (client,
                                                           res,
                                                           &error);
        if (device == NULL) {
                g_warning ("could not find device: %s",
                           error->message);
                g_error_free (error);
                goto out;
        }

        /* get properties */
        cd_device_connect (device,
                           NULL,
                           gcm_session_device_assign_connect_cb,
                           manager);
out:
        if (device != NULL)
                g_object_unref (device);
}

/* We have to reset the gamma tables each time as if the primary output
 * has changed then different crtcs are going to be used.
 * See https://bugzilla.gnome.org/show_bug.cgi?id=660164 for an example */
static void
gsd_rr_screen_output_changed_cb (GsdRRScreen *screen,
                                   GsdColorManager *manager)
{
        GsdRROutput **outputs;
        GsdColorManagerPrivate *priv = manager->priv;
        guint i;

        /* get X11 outputs */
        outputs = gsd_rr_screen_list_outputs (priv->x11_screen);
        if (outputs == NULL) {
                g_warning ("failed to get outputs");
                return;
        }
        for (i = 0; outputs[i] != NULL; i++) {
                if (!gsd_rr_output_is_connected (outputs[i]))
                        continue;

                /* get CdDevice for this output */
                cd_client_find_device_by_property (manager->priv->client,
                                                   CD_DEVICE_METADATA_XRANDR_NAME,
                                                   gsd_rr_output_get_name (outputs[i]),
                                                   NULL,
                                                   gcm_session_profile_gamma_find_device_cb,
                                                   manager);
        }

}

static void
gcm_session_client_connect_cb (GObject *source_object,
                               GAsyncResult *res,
                               gpointer user_data)
{
        gboolean ret;
        GError *error = NULL;
        GsdRROutput **outputs;
        guint i;
        GsdColorManager *manager = GSD_COLOR_MANAGER (user_data);
        GsdColorManagerPrivate *priv = manager->priv;

        /* connected */
        g_debug ("connected to colord");
        ret = cd_client_connect_finish (manager->priv->client, res, &error);
        if (!ret) {
                g_warning ("failed to connect to colord: %s", error->message);
                g_error_free (error);
                return;
        }

#if CD_CHECK_VERSION(0,1,12)
        /* is there an available colord instance? */
        ret = cd_client_get_has_server (manager->priv->client);
        if (!ret) {
                g_warning ("There is no colord server available");
                goto out;
        }
#endif

        /* add profiles */
        gcm_profile_store_search (priv->profile_store);

        /* add screens */
        gsd_rr_screen_refresh (priv->x11_screen, &error);
        if (error != NULL) {
                g_warning ("failed to refresh: %s", error->message);
                g_error_free (error);
                goto out;
        }

        /* get X11 outputs */
        outputs = gsd_rr_screen_list_outputs (priv->x11_screen);
        if (outputs == NULL) {
                g_warning ("failed to get outputs");
                goto out;
        }
        for (i = 0; outputs[i] != NULL; i++) {
                if (gsd_rr_output_is_connected (outputs[i]))
                        gcm_session_add_x11_output (manager, outputs[i]);
        }

        /* only connect when colord is awake */
        g_signal_connect (priv->x11_screen, "output-connected",
                          G_CALLBACK (gsd_rr_screen_output_added_cb),
                          manager);
        g_signal_connect (priv->x11_screen, "output-disconnected",
                          G_CALLBACK (gsd_rr_screen_output_removed_cb),
                          manager);
        g_signal_connect (priv->x11_screen, "changed",
                          G_CALLBACK (gsd_rr_screen_output_changed_cb),
                          manager);

        g_signal_connect (priv->client, "device-added",
                          G_CALLBACK (gcm_session_device_added_assign_cb),
                          manager);
        g_signal_connect (priv->client, "device-changed",
                          G_CALLBACK (gcm_session_device_changed_assign_cb),
                          manager);

        /* set for each device that already exist */
        cd_client_get_devices (priv->client, NULL,
                               gcm_session_get_devices_cb,
                               manager);
out:
        return;
}

gboolean
gsd_color_manager_start (GsdColorManager *manager,
                         GError          **error)
{
        GsdColorManagerPrivate *priv = manager->priv;
        gboolean ret = FALSE;

        g_debug ("Starting color manager");
        gnome_settings_profile_start (NULL);

        /* coldplug the list of screens */
        priv->x11_screen = gsd_rr_screen_new (gdk_screen_get_default (), error);
        if (priv->x11_screen == NULL)
                goto out;

        cd_client_connect (priv->client,
                           NULL,
                           gcm_session_client_connect_cb,
                           manager);

        /* success */
        ret = TRUE;
out:
        gnome_settings_profile_end (NULL);
        return ret;
}

void
gsd_color_manager_stop (GsdColorManager *manager)
{
        g_debug ("Stopping color manager");

        g_clear_object (&manager->priv->settings);
        g_clear_object (&manager->priv->client);
        g_clear_object (&manager->priv->profile_store);
        g_clear_object (&manager->priv->dmi);
        g_clear_object (&manager->priv->session);
        g_clear_pointer (&manager->priv->edid_cache, g_hash_table_destroy);
        g_clear_pointer (&manager->priv->device_assign_hash, g_hash_table_destroy);
        g_clear_object (&manager->priv->x11_screen);
}

static void
gcm_session_exec_control_center (GsdColorManager *manager)
{
        gboolean ret;
        GError *error = NULL;
        GAppInfo *app_info;
        GdkAppLaunchContext *launch_context;

        /* setup the launch context so the startup notification is correct */
        launch_context = gdk_display_get_app_launch_context (gdk_display_get_default ());
        app_info = g_app_info_create_from_commandline (BINDIR "/gnome-control-center color",
                                                       "gnome-control-center",
                                                       G_APP_INFO_CREATE_SUPPORTS_STARTUP_NOTIFICATION,
                                                       &error);
        if (app_info == NULL) {
                g_warning ("failed to create application info: %s",
                           error->message);
                g_error_free (error);
                goto out;
        }

        /* launch gnome-control-center */
        ret = g_app_info_launch (app_info,
                                 NULL,
                                 G_APP_LAUNCH_CONTEXT (launch_context),
                                 &error);
        if (!ret) {
                g_warning ("failed to launch gnome-control-center: %s",
                           error->message);
                g_error_free (error);
                goto out;
        }
out:
        g_object_unref (launch_context);
        if (app_info != NULL)
                g_object_unref (app_info);
}

static void
gcm_session_notify_cb (NotifyNotification *notification,
                       gchar *action,
                       gpointer user_data)
{
        GsdColorManager *manager = GSD_COLOR_MANAGER (user_data);

        if (g_strcmp0 (action, "recalibrate") == 0) {
                notify_notification_close (notification, NULL);
                gcm_session_exec_control_center (manager);
        }
}

static void
closed_cb (NotifyNotification *notification, gpointer data)
{
        g_object_unref (notification);
}

static gboolean
gcm_session_notify_recalibrate (GsdColorManager *manager,
                                const gchar *title,
                                const gchar *message,
                                CdDeviceKind kind)
{
        gboolean ret;
        GError *error = NULL;
        NotifyNotification *notification;
        GsdColorManagerPrivate *priv = manager->priv;

        /* show a bubble */
        notification = notify_notification_new (title, message, "preferences-color");
        notify_notification_set_timeout (notification, GCM_SESSION_NOTIFY_TIMEOUT);
        notify_notification_set_urgency (notification, NOTIFY_URGENCY_LOW);
        notify_notification_set_app_name (notification, _("Color"));

        /* TRANSLATORS: button: this is to open GCM */
        notify_notification_add_action (notification,
                                        "recalibrate",
                                        _("Recalibrate now"),
                                        gcm_session_notify_cb,
                                        priv, NULL);

        g_signal_connect (notification, "closed", G_CALLBACK (closed_cb), NULL);
        ret = notify_notification_show (notification, &error);
        if (!ret) {
                g_warning ("failed to show notification: %s",
                           error->message);
                g_error_free (error);
        }
        return ret;
}

static gchar *
gcm_session_device_get_title (CdDevice *device)
{
        const gchar *vendor;
        const gchar *model;

        model = cd_device_get_model (device);
        vendor = cd_device_get_vendor (device);
        if (model != NULL && vendor != NULL)
                return g_strdup_printf ("%s - %s", vendor, model);
        if (vendor != NULL)
                return g_strdup (vendor);
        if (model != NULL)
                return g_strdup (model);
        return g_strdup (cd_device_get_id (device));
}

static void
gcm_session_notify_device (GsdColorManager *manager, CdDevice *device)
{
        CdDeviceKind kind;
        const gchar *title;
        gchar *device_title = NULL;
        gchar *message;
        guint threshold;
        glong since;
        GsdColorManagerPrivate *priv = manager->priv;

        /* TRANSLATORS: this is when the device has not been recalibrated in a while */
        title = _("Recalibration required");
        device_title = gcm_session_device_get_title (device);

        /* check we care */
        kind = cd_device_get_kind (device);
        if (kind == CD_DEVICE_KIND_DISPLAY) {

                /* get from GSettings */
                threshold = g_settings_get_uint (priv->settings,
                                                 GCM_SETTINGS_RECALIBRATE_DISPLAY_THRESHOLD);

                /* TRANSLATORS: this is when the display has not been recalibrated in a while */
                message = g_strdup_printf (_("The display '%s' should be recalibrated soon."),
                                           device_title);
        } else {

                /* get from GSettings */
                threshold = g_settings_get_uint (priv->settings,
                                                 GCM_SETTINGS_RECALIBRATE_PRINTER_THRESHOLD);

                /* TRANSLATORS: this is when the printer has not been recalibrated in a while */
                message = g_strdup_printf (_("The printer '%s' should be recalibrated soon."),
                                           device_title);
        }

        /* check if we need to notify */
        since = (g_get_real_time () - cd_device_get_modified (device)) / G_USEC_PER_SEC;
        if (threshold > since)
                gcm_session_notify_recalibrate (manager, title, message, kind);
        g_free (device_title);
        g_free (message);
}

static void
gcm_session_profile_connect_cb (GObject *object,
                                GAsyncResult *res,
                                gpointer user_data)
{
        const gchar *filename;
        gboolean ret;
        gchar *basename = NULL;
        const gchar *data_source;
        GError *error = NULL;
        CdProfile *profile = CD_PROFILE (object);
        GcmSessionAsyncHelper *helper = (GcmSessionAsyncHelper *) user_data;
        GsdColorManager *manager = GSD_COLOR_MANAGER (helper->manager);

        ret = cd_profile_connect_finish (profile,
                                         res,
                                         &error);
        if (!ret) {
                g_warning ("failed to connect to profile: %s",
                           error->message);
                g_error_free (error);
                goto out;
        }

        /* ensure it's a profile generated by us */
        data_source = cd_profile_get_metadata_item (profile,
                                                    CD_PROFILE_METADATA_DATA_SOURCE);
        if (data_source == NULL) {

                /* existing profiles from gnome-color-manager < 3.1
                 * won't have the extra metadata values added */
                filename = cd_profile_get_filename (profile);
                if (filename == NULL)
                        goto out;
                basename = g_path_get_basename (filename);
                if (!g_str_has_prefix (basename, "GCM")) {
                        g_debug ("not a GCM profile for %s: %s",
                                 cd_device_get_id (helper->device), filename);
                        goto out;
                }

        /* ensure it's been created from a calibration, rather than from
         * auto-EDID */
        } else if (g_strcmp0 (data_source,
                   CD_PROFILE_METADATA_DATA_SOURCE_CALIB) != 0) {
                g_debug ("not a calib profile for %s",
                         cd_device_get_id (helper->device));
                goto out;
        }

        /* handle device */
        gcm_session_notify_device (manager, helper->device);
out:
        gcm_session_async_helper_free (helper);
        g_free (basename);
}

static void
gcm_session_device_connect_cb (GObject *object,
                               GAsyncResult *res,
                               gpointer user_data)
{
        gboolean ret;
        GError *error = NULL;
        CdDeviceKind kind;
        CdProfile *profile = NULL;
        CdDevice *device = CD_DEVICE (object);
        GsdColorManager *manager = GSD_COLOR_MANAGER (user_data);
        GcmSessionAsyncHelper *helper;

        ret = cd_device_connect_finish (device,
                                        res,
                                        &error);
        if (!ret) {
                g_warning ("failed to connect to device: %s",
                           error->message);
                g_error_free (error);
                goto out;
        }

        /* check we care */
        kind = cd_device_get_kind (device);
        if (kind != CD_DEVICE_KIND_DISPLAY &&
            kind != CD_DEVICE_KIND_PRINTER)
                goto out;

        /* ensure we have a profile */
        profile = cd_device_get_default_profile (device);
        if (profile == NULL) {
                g_debug ("no profile set for %s", cd_device_get_id (device));
                goto out;
        }

        /* connect to the profile */
        helper = g_new0 (GcmSessionAsyncHelper, 1);
        helper->manager = g_object_ref (manager);
        helper->device = g_object_ref (device);
        cd_profile_connect (profile,
                            NULL,
                            gcm_session_profile_connect_cb,
                            helper);
out:
        if (profile != NULL)
                g_object_unref (profile);
}

static void
gcm_session_device_added_notify_cb (CdClient *client,
                                    CdDevice *device,
                                    GsdColorManager *manager)
{
        /* connect to the device to get properties */
        cd_device_connect (device,
                           NULL,
                           gcm_session_device_connect_cb,
                           manager);
}

static gchar *
gcm_session_get_precooked_md5 (cmsHPROFILE lcms_profile)
{
        cmsUInt8Number profile_id[16];
        gboolean md5_precooked = FALSE;
        guint i;
        gchar *md5 = NULL;

        /* check to see if we have a pre-cooked MD5 */
        cmsGetHeaderProfileID (lcms_profile, profile_id);
        for (i = 0; i < 16; i++) {
                if (profile_id[i] != 0) {
                        md5_precooked = TRUE;
                        break;
                }
        }
        if (!md5_precooked)
                goto out;

        /* convert to a hex string */
        md5 = g_new0 (gchar, 32 + 1);
        for (i = 0; i < 16; i++)
                g_snprintf (md5 + i*2, 3, "%02x", profile_id[i]);
out:
        return md5;
}

static gchar *
gcm_session_get_md5_for_filename (const gchar *filename,
                                  GError **error)
{
        gboolean ret;
        gchar *checksum = NULL;
        gchar *data = NULL;
        gsize length;
        cmsHPROFILE lcms_profile = NULL;

        /* get the internal profile id, if it exists */
        lcms_profile = cmsOpenProfileFromFile (filename, "r");
        if (lcms_profile == NULL) {
                g_set_error_literal (error,
                                     GSD_COLOR_MANAGER_ERROR,
                                     GSD_COLOR_MANAGER_ERROR_FAILED,
                                     "failed to load: not an ICC profile");
                goto out;
        }
        checksum = gcm_session_get_precooked_md5 (lcms_profile);
        if (checksum != NULL)
                goto out;

        /* generate checksum */
        ret = g_file_get_contents (filename, &data, &length, error);
        if (!ret)
                goto out;
        checksum = g_compute_checksum_for_data (G_CHECKSUM_MD5,
                                                (const guchar *) data,
                                                length);
out:
        g_free (data);
        if (lcms_profile != NULL)
                cmsCloseProfile (lcms_profile);
        return checksum;
}

static void
gcm_session_create_profile_cb (GObject *object,
                               GAsyncResult *res,
                               gpointer user_data)
{
        CdProfile *profile;
        GError *error = NULL;
        CdClient *client = CD_CLIENT (object);

        profile = cd_client_create_profile_finish (client, res, &error);
        if (profile == NULL) {
                if (error->domain != CD_CLIENT_ERROR ||
                    error->code != CD_CLIENT_ERROR_ALREADY_EXISTS)
                        g_warning ("%s", error->message);
                g_error_free (error);
                return;
        }
        g_object_unref (profile);
}

static void
gcm_session_profile_store_added_cb (GcmProfileStore *profile_store,
                                    const gchar *filename,
                                    GsdColorManager *manager)
{
        gchar *checksum = NULL;
        gchar *profile_id = NULL;
        GError *error = NULL;
        GHashTable *profile_props = NULL;
        GsdColorManagerPrivate *priv = manager->priv;

        g_debug ("profile %s added", filename);

        /* generate ID */
        checksum = gcm_session_get_md5_for_filename (filename, &error);
        if (checksum == NULL) {
                g_debug ("failed to get profile checksum for %s: %s",
                         filename, error->message);
                g_error_free (error);
                goto out;
        }
        profile_id = g_strdup_printf ("icc-%s", checksum);
        profile_props = g_hash_table_new_full (g_str_hash, g_str_equal,
                                               NULL, NULL);
        g_hash_table_insert (profile_props,
                             CD_PROFILE_PROPERTY_FILENAME,
                             (gpointer) filename);
        g_hash_table_insert (profile_props,
                             CD_PROFILE_METADATA_FILE_CHECKSUM,
                             (gpointer) checksum);
        cd_client_create_profile (priv->client,
                                  profile_id,
                                  CD_OBJECT_SCOPE_TEMP,
                                  profile_props,
                                  NULL,
                                  gcm_session_create_profile_cb,
                                  manager);
out:
        g_free (checksum);
        g_free (profile_id);
        if (profile_props != NULL)
                g_hash_table_unref (profile_props);
}

static void
gcm_session_delete_profile_cb (GObject *object,
                               GAsyncResult *res,
                               gpointer user_data)
{
        gboolean ret;
        GError *error = NULL;
        CdClient *client = CD_CLIENT (object);

        ret = cd_client_delete_profile_finish (client, res, &error);
        if (!ret) {
                g_warning ("%s", error->message);
                g_error_free (error);
        }
}

static void
gcm_session_find_profile_by_filename_cb (GObject *object,
                                         GAsyncResult *res,
                                         gpointer user_data)
{
        GError *error = NULL;
        CdProfile *profile;
        CdClient *client = CD_CLIENT (object);
        GsdColorManager *manager = GSD_COLOR_MANAGER (user_data);

        profile = cd_client_find_profile_by_filename_finish (client, res, &error);
        if (profile == NULL) {
                g_warning ("%s", error->message);
                g_error_free (error);
                goto out;
        }

        /* remove it from colord */
        cd_client_delete_profile (manager->priv->client,
                                  profile,
                                  NULL,
                                  gcm_session_delete_profile_cb,
                                  manager);
out:
        if (profile != NULL)
                g_object_unref (profile);
}

static void
gcm_session_profile_store_removed_cb (GcmProfileStore *profile_store,
                                      const gchar *filename,
                                      GsdColorManager *manager)
{
        /* find the ID for the filename */
        g_debug ("filename %s removed", filename);
        cd_client_find_profile_by_filename (manager->priv->client,
                                            filename,
                                            NULL,
                                            gcm_session_find_profile_by_filename_cb,
                                            manager);
}

static void
gcm_session_sensor_added_cb (CdClient *client,
                             CdSensor *sensor,
                             GsdColorManager *manager)
{
        ca_context_play (ca_gtk_context_get (), 0,
                         CA_PROP_EVENT_ID, "device-added",
                         /* TRANSLATORS: this is the application name */
                         CA_PROP_APPLICATION_NAME, _("GNOME Settings Daemon Color Plugin"),
                        /* TRANSLATORS: this is a sound description */
                         CA_PROP_EVENT_DESCRIPTION, _("Color calibration device added"), NULL);

        /* open up the color prefs window */
        gcm_session_exec_control_center (manager);
}

static void
gcm_session_sensor_removed_cb (CdClient *client,
                               CdSensor *sensor,
                               GsdColorManager *manager)
{
        ca_context_play (ca_gtk_context_get (), 0,
                         CA_PROP_EVENT_ID, "device-removed",
                         /* TRANSLATORS: this is the application name */
                         CA_PROP_APPLICATION_NAME, _("GNOME Settings Daemon Color Plugin"),
                        /* TRANSLATORS: this is a sound description */
                         CA_PROP_EVENT_DESCRIPTION, _("Color calibration device removed"), NULL);
}

static gboolean
has_changed (char       **strv,
	     const char  *str)
{
        guint i;
        for (i = 0; strv[i] != NULL; i++) {
                if (g_str_equal (str, strv[i]))
                        return TRUE;
        }
        return FALSE;
}

static void
gcm_session_active_changed_cb (GDBusProxy      *session,
                               GVariant        *changed,
                               char           **invalidated,
                               GsdColorManager *manager)
{
        GsdColorManagerPrivate *priv = manager->priv;
        GVariant *active_v = NULL;
        gboolean is_active;

        if (has_changed (invalidated, "SessionIsActive"))
                return;

        /* not yet connected to the daemon */
        if (!cd_client_get_connected (priv->client))
                return;

        active_v = g_dbus_proxy_get_cached_property (session, "SessionIsActive");
        g_return_if_fail (active_v != NULL);
        is_active = g_variant_get_boolean (active_v);
        g_variant_unref (active_v);

        /* When doing the fast-user-switch into a new account, load the
         * new users chosen profiles.
         *
         * If this is the first time the GnomeSettingsSession has been
         * loaded, then we'll get a change from unknown to active
         * and we want to avoid reprobing the devices for that.
         */
        if (is_active && !priv->session_is_active) {
                g_debug ("Done switch to new account, reload devices");
                cd_client_get_devices (manager->priv->client, NULL,
                                       gcm_session_get_devices_cb,
                                       manager);
        }
        priv->session_is_active = is_active;
}

static void
gsd_color_manager_class_init (GsdColorManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gsd_color_manager_finalize;

        g_type_class_add_private (klass, sizeof (GsdColorManagerPrivate));
}

static void
gsd_color_manager_init (GsdColorManager *manager)
{
        GsdColorManagerPrivate *priv;
        priv = manager->priv = GSD_COLOR_MANAGER_GET_PRIVATE (manager);

        /* track the active session */
        priv->session = gnome_settings_session_get_session_proxy ();
        g_signal_connect (priv->session, "g-properties-changed",
                          G_CALLBACK (gcm_session_active_changed_cb), manager);

        /* set the _ICC_PROFILE atoms on the root screen */
        priv->gdk_window = gdk_screen_get_root_window (gdk_screen_get_default ());

        /* parsing the EDID is expensive */
        priv->edid_cache = g_hash_table_new_full (g_str_hash,
                                                  g_str_equal,
                                                  g_free,
                                                  g_object_unref);

        /* we don't want to assign devices multiple times at startup */
        priv->device_assign_hash = g_hash_table_new_full (g_str_hash,
                                                          g_str_equal,
                                                          g_free,
                                                          NULL);

        /* use DMI data for internal panels */
        priv->dmi = gcm_dmi_new ();

        priv->settings = g_settings_new ("org.gnome.settings-daemon.plugins.color");
        priv->client = cd_client_new ();
        g_signal_connect (priv->client, "device-added",
                          G_CALLBACK (gcm_session_device_added_notify_cb),
                          manager);
        g_signal_connect (priv->client, "sensor-added",
                          G_CALLBACK (gcm_session_sensor_added_cb),
                          manager);
        g_signal_connect (priv->client, "sensor-removed",
                          G_CALLBACK (gcm_session_sensor_removed_cb),
                          manager);

        /* have access to all user profiles */
        priv->profile_store = gcm_profile_store_new ();
        g_signal_connect (priv->profile_store, "added",
                          G_CALLBACK (gcm_session_profile_store_added_cb),
                          manager);
        g_signal_connect (priv->profile_store, "removed",
                          G_CALLBACK (gcm_session_profile_store_removed_cb),
                          manager);
}

static void
gsd_color_manager_finalize (GObject *object)
{
        GsdColorManager *manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GSD_IS_COLOR_MANAGER (object));

        manager = GSD_COLOR_MANAGER (object);

        g_clear_object (&manager->priv->settings);
        g_clear_object (&manager->priv->client);
        g_clear_object (&manager->priv->profile_store);
        g_clear_object (&manager->priv->dmi);
        g_clear_object (&manager->priv->session);
        g_clear_pointer (&manager->priv->edid_cache, g_hash_table_destroy);
        g_clear_pointer (&manager->priv->device_assign_hash, g_hash_table_destroy);
        g_clear_object (&manager->priv->x11_screen);

        G_OBJECT_CLASS (gsd_color_manager_parent_class)->finalize (object);
}

GsdColorManager *
gsd_color_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (GSD_TYPE_COLOR_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
        }

        return GSD_COLOR_MANAGER (manager_object);
}
