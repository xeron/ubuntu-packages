/* gnome-rr.c
 *
 * Copyright 2007, 2008, Red Hat, Inc.
 * 
 * This file is part of the Gnome Library.
 * 
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 * 
 * Author: Soren Sandmann <sandmann@redhat.com>
 */

#include <config.h>
#include <glib/gi18n-lib.h>
#include <string.h>
#include <X11/Xlib.h>

#include <X11/extensions/Xrandr.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#include <X11/extensions/dpms.h>

#undef GNOME_DISABLE_DEPRECATED
#include "gsd-rr.h"
#include "gsd-rr-config.h"

#include "edid.h"
#include "gsd-rr-private.h"

#define DISPLAY(o) ((o)->info->screen->priv->xdisplay)

#define SERVERS_RANDR_IS_AT_LEAST_1_3(priv) (priv->rr_major_version > 1 || (priv->rr_major_version == 1 && priv->rr_minor_version >= 3))

enum {
    SCREEN_PROP_0,
    SCREEN_PROP_GDK_SCREEN,
    SCREEN_PROP_LAST,
};

enum {
    SCREEN_CHANGED,
    SCREEN_OUTPUT_CONNECTED,
    SCREEN_OUTPUT_DISCONNECTED,
    SCREEN_SIGNAL_LAST,
};

gint screen_signals[SCREEN_SIGNAL_LAST];

struct GsdRROutput
{
    ScreenInfo *	info;
    RROutput		id;
    
    char *		name;
    char *		display_name;
    GsdRRCrtc *	current_crtc;
    gboolean		connected;
    gulong		width_mm;
    gulong		height_mm;
    GsdRRCrtc **	possible_crtcs;
    GsdRROutput **	clones;
    GsdRRMode **	modes;
    int			n_preferred;
    guint8 *		edid_data;
    gsize		edid_size;
    char *              connector_type;
    gint		backlight_min;
    gint		backlight_max;
};

struct GsdRROutputWrap
{
    RROutput		id;
};

struct GsdRRCrtc
{
    ScreenInfo *	info;
    RRCrtc		id;
    
    GsdRRMode *	current_mode;
    GsdRROutput **	current_outputs;
    GsdRROutput **	possible_outputs;
    int			x;
    int			y;
    
    GsdRRRotation	current_rotation;
    GsdRRRotation	rotations;
    int			gamma_size;
};

struct GsdRRMode
{
    ScreenInfo *	info;
    RRMode		id;
    char *		name;
    int			width;
    int			height;
    int			freq;		/* in mHz */
};

/* GsdRRCrtc */
static GsdRRCrtc *  crtc_new          (ScreenInfo         *info,
					 RRCrtc              id);
static GsdRRCrtc *  crtc_copy         (const GsdRRCrtc  *from);
static void           crtc_free         (GsdRRCrtc        *crtc);

static gboolean       crtc_initialize   (GsdRRCrtc        *crtc,
					 XRRScreenResources *res,
					 GError            **error);

/* GsdRROutput */
static GsdRROutput *output_new        (ScreenInfo         *info,
					 RROutput            id);

static gboolean       output_initialize (GsdRROutput      *output,
					 XRRScreenResources *res,
					 GError            **error);

static GsdRROutput *output_copy       (const GsdRROutput *from);
static void           output_free       (GsdRROutput      *output);

/* GsdRRMode */
static GsdRRMode *  mode_new          (ScreenInfo         *info,
					 RRMode              id);

static void           mode_initialize   (GsdRRMode        *mode,
					 XRRModeInfo        *info);

static GsdRRMode *  mode_copy         (const GsdRRMode  *from);
static void           mode_free         (GsdRRMode        *mode);

static void gsd_rr_screen_finalize (GObject*);
static void gsd_rr_screen_set_property (GObject*, guint, const GValue*, GParamSpec*);
static void gsd_rr_screen_get_property (GObject*, guint, GValue*, GParamSpec*);
static gboolean gsd_rr_screen_initable_init (GInitable*, GCancellable*, GError**);
static void gsd_rr_screen_initable_iface_init (GInitableIface *iface);
G_DEFINE_TYPE_WITH_CODE (GsdRRScreen, gsd_rr_screen, G_TYPE_OBJECT,
        G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, gsd_rr_screen_initable_iface_init))

G_DEFINE_BOXED_TYPE (GsdRRCrtc, gsd_rr_crtc, crtc_copy, crtc_free)
G_DEFINE_BOXED_TYPE (GsdRROutput, gsd_rr_output, output_copy, output_free)
G_DEFINE_BOXED_TYPE (GsdRRMode, gsd_rr_mode, mode_copy, mode_free)

/* Errors */

/**
 * gsd_rr_error_quark:
 *
 * Returns the #GQuark that will be used for #GError values returned by the
 * GsdRR API.
 *
 * Return value: a #GQuark used to identify errors coming from the GsdRR API.
 */
GQuark
gsd_rr_error_quark (void)
{
    return g_quark_from_static_string ("gsd-rr-error-quark");
}

/* Screen */
static GsdRROutput *
gsd_rr_output_by_id (ScreenInfo *info, RROutput id)
{
    GsdRROutput **output;
    
    g_assert (info != NULL);
    
    for (output = info->outputs; *output; ++output)
    {
	if ((*output)->id == id)
	    return *output;
    }
    
    return NULL;
}

static GsdRRCrtc *
crtc_by_id (ScreenInfo *info, RRCrtc id)
{
    GsdRRCrtc **crtc;
    
    if (!info)
        return NULL;
    
    for (crtc = info->crtcs; *crtc; ++crtc)
    {
	if ((*crtc)->id == id)
	    return *crtc;
    }
    
    return NULL;
}

static GsdRRMode *
mode_by_id (ScreenInfo *info, RRMode id)
{
    GsdRRMode **mode;
    
    g_assert (info != NULL);
    
    for (mode = info->modes; *mode; ++mode)
    {
	if ((*mode)->id == id)
	    return *mode;
    }
    
    return NULL;
}

static void
screen_info_free (ScreenInfo *info)
{
    GsdRROutput **output;
    GsdRRCrtc **crtc;
    GsdRRMode **mode;
    
    g_assert (info != NULL);

    if (info->resources)
    {
	XRRFreeScreenResources (info->resources);
	
	info->resources = NULL;
    }
    
    if (info->outputs)
    {
	for (output = info->outputs; *output; ++output)
	    output_free (*output);
	g_free (info->outputs);
    }
    
    if (info->crtcs)
    {
	for (crtc = info->crtcs; *crtc; ++crtc)
	    crtc_free (*crtc);
	g_free (info->crtcs);
    }
    
    if (info->modes)
    {
	for (mode = info->modes; *mode; ++mode)
	    mode_free (*mode);
	g_free (info->modes);
    }

    if (info->clone_modes)
    {
	/* The modes themselves were freed above */
	g_free (info->clone_modes);
    }
    
    g_free (info);
}

static gboolean
has_similar_mode (GsdRROutput *output, GsdRRMode *mode)
{
    int i;
    GsdRRMode **modes = gsd_rr_output_list_modes (output);
    int width = gsd_rr_mode_get_width (mode);
    int height = gsd_rr_mode_get_height (mode);

    for (i = 0; modes[i] != NULL; ++i)
    {
	GsdRRMode *m = modes[i];

	if (gsd_rr_mode_get_width (m) == width	&&
	    gsd_rr_mode_get_height (m) == height)
	{
	    return TRUE;
	}
    }

    return FALSE;
}

static void
gather_clone_modes (ScreenInfo *info)
{
    int i;
    GPtrArray *result = g_ptr_array_new ();

    for (i = 0; info->outputs[i] != NULL; ++i)
    {
	int j;
	GsdRROutput *output1, *output2;

	output1 = info->outputs[i];
	
	if (!output1->connected)
	    continue;
	
	for (j = 0; output1->modes[j] != NULL; ++j)
	{
	    GsdRRMode *mode = output1->modes[j];
	    gboolean valid;
	    int k;

	    valid = TRUE;
	    for (k = 0; info->outputs[k] != NULL; ++k)
	    {
		output2 = info->outputs[k];
		
		if (!output2->connected)
		    continue;
		
		if (!has_similar_mode (output2, mode))
		{
		    valid = FALSE;
		    break;
		}
	    }

	    if (valid)
		g_ptr_array_add (result, mode);
	}
    }

    g_ptr_array_add (result, NULL);
    
    info->clone_modes = (GsdRRMode **)g_ptr_array_free (result, FALSE);
}

static gboolean
fill_screen_info_from_resources (ScreenInfo *info,
				 XRRScreenResources *resources,
				 GError **error)
{
    int i;
    GPtrArray *a;
    GsdRRCrtc **crtc;
    GsdRROutput **output;

    info->resources = resources;

    /* We create all the structures before initializing them, so
     * that they can refer to each other.
     */
    a = g_ptr_array_new ();
    for (i = 0; i < resources->ncrtc; ++i)
    {
	GsdRRCrtc *crtc = crtc_new (info, resources->crtcs[i]);

	g_ptr_array_add (a, crtc);
    }
    g_ptr_array_add (a, NULL);
    info->crtcs = (GsdRRCrtc **)g_ptr_array_free (a, FALSE);

    a = g_ptr_array_new ();
    for (i = 0; i < resources->noutput; ++i)
    {
	GsdRROutput *output = output_new (info, resources->outputs[i]);

	g_ptr_array_add (a, output);
    }
    g_ptr_array_add (a, NULL);
    info->outputs = (GsdRROutput **)g_ptr_array_free (a, FALSE);

    a = g_ptr_array_new ();
    for (i = 0;  i < resources->nmode; ++i)
    {
	GsdRRMode *mode = mode_new (info, resources->modes[i].id);

	g_ptr_array_add (a, mode);
    }
    g_ptr_array_add (a, NULL);
    info->modes = (GsdRRMode **)g_ptr_array_free (a, FALSE);

    /* Initialize */
    for (crtc = info->crtcs; *crtc; ++crtc)
    {
	if (!crtc_initialize (*crtc, resources, error))
	    return FALSE;
    }

    for (output = info->outputs; *output; ++output)
    {
	if (!output_initialize (*output, resources, error))
	    return FALSE;
    }

    for (i = 0; i < resources->nmode; ++i)
    {
	GsdRRMode *mode = mode_by_id (info, resources->modes[i].id);

	mode_initialize (mode, &(resources->modes[i]));
    }

    gather_clone_modes (info);

    return TRUE;
}

static gboolean
fill_out_screen_info (Display *xdisplay,
		      Window xroot,
		      ScreenInfo *info,
		      gboolean needs_reprobe,
		      GError **error)
{
    XRRScreenResources *resources;
    GsdRRScreenPrivate *priv;
    
    g_assert (xdisplay != NULL);
    g_assert (info != NULL);

    priv = info->screen->priv;

    /* First update the screen resources */

    if (needs_reprobe)
        resources = XRRGetScreenResources (xdisplay, xroot);
    else
    {
	/* XRRGetScreenResourcesCurrent is less expensive than
	 * XRRGetScreenResources, however it is available only
	 * in RandR 1.3 or higher
	 */
        if (SERVERS_RANDR_IS_AT_LEAST_1_3 (priv))
            resources = XRRGetScreenResourcesCurrent (xdisplay, xroot);
        else
            resources = XRRGetScreenResources (xdisplay, xroot);
    }

    if (resources)
    {
	if (!fill_screen_info_from_resources (info, resources, error))
	    return FALSE;
    }
    else
    {
	g_set_error (error, GSD_RR_ERROR, GSD_RR_ERROR_RANDR_ERROR,
		     /* Translators: a CRTC is a CRT Controller (this is X terminology). */
		     _("could not get the screen resources (CRTCs, outputs, modes)"));
	return FALSE;
    }

    /* Then update the screen size range.  We do this after XRRGetScreenResources() so that
     * the X server will already have an updated view of the outputs.
     */

    if (needs_reprobe) {
	gboolean success;

        gdk_error_trap_push ();
	success = XRRGetScreenSizeRange (xdisplay, xroot,
					 &(info->min_width),
					 &(info->min_height),
					 &(info->max_width),
					 &(info->max_height));
	gdk_flush ();
	if (gdk_error_trap_pop ()) {
	    g_set_error (error, GSD_RR_ERROR, GSD_RR_ERROR_UNKNOWN,
			 _("unhandled X error while getting the range of screen sizes"));
	    return FALSE;
	}

	if (!success) {
	    g_set_error (error, GSD_RR_ERROR, GSD_RR_ERROR_RANDR_ERROR,
			 _("could not get the range of screen sizes"));
            return FALSE;
        }
    }
    else
    {
        gsd_rr_screen_get_ranges (info->screen, 
					 &(info->min_width),
					 &(info->max_width),
					 &(info->min_height),
					 &(info->max_height));
    }

    info->primary = None;
    if (SERVERS_RANDR_IS_AT_LEAST_1_3 (priv)) {
        gdk_error_trap_push ();
        info->primary = XRRGetOutputPrimary (xdisplay, xroot);
	gdk_error_trap_pop_ignored ();
    }

    /* can the screen do DPMS? */
    gdk_error_trap_push ();
    priv->dpms_capable = DPMSCapable (priv->xdisplay);
    gdk_error_trap_pop_ignored ();

    return TRUE;
}

static ScreenInfo *
screen_info_new (GsdRRScreen *screen, gboolean needs_reprobe, GError **error)
{
    ScreenInfo *info = g_new0 (ScreenInfo, 1);
    GsdRRScreenPrivate *priv;

    g_assert (screen != NULL);

    priv = screen->priv;

    info->outputs = NULL;
    info->crtcs = NULL;
    info->modes = NULL;
    info->screen = screen;
    
    if (fill_out_screen_info (priv->xdisplay, priv->xroot, info, needs_reprobe, error))
    {
	return info;
    }
    else
    {
	screen_info_free (info);
	return NULL;
    }
}

static GsdRROutput *
find_output_by_id (GsdRROutput **haystack, guint32 id)
{
    guint i;

    for (i = 0; haystack[i] != NULL; i++)
    {
	if (gsd_rr_output_get_id (haystack[i]) == id)
	    return haystack[i];
    }
    return NULL;
}

static void
diff_outputs_and_emit_signals (ScreenInfo *old, ScreenInfo *new)
{
    guint i;
    guint32 id_old, id_new;
    GsdRROutput *output_old;
    GsdRROutput *output_new;

    /* have any outputs been removed or disconnected */
    for (i = 0; old->outputs[i] != NULL; i++)
    {
        id_old = gsd_rr_output_get_id (old->outputs[i]);
        output_new = find_output_by_id (new->outputs, id_old);
	if (output_new == NULL)
	{
	    /* output removed (and disconnected) */
	    if (gsd_rr_output_is_connected (old->outputs[i]))
	     {
	        g_signal_emit (G_OBJECT (new->screen),
			       screen_signals[SCREEN_OUTPUT_DISCONNECTED], 0,
			       old->outputs[i]);
             }
	    continue;
	}
	if (gsd_rr_output_is_connected (old->outputs[i]) &&
	    !gsd_rr_output_is_connected (output_new))
	{
	    /* output disconnected */
	    g_signal_emit (G_OBJECT (new->screen),
			   screen_signals[SCREEN_OUTPUT_DISCONNECTED], 0,
			   old->outputs[i]);
        }
    }

    /* have any outputs been created or connected */
    for (i = 0; new->outputs[i] != NULL; i++)
    {
        id_new = gsd_rr_output_get_id (new->outputs[i]);
        output_old = find_output_by_id (old->outputs, id_new);
	if (output_old == NULL)
	{
	    /* output created */
	    if (gsd_rr_output_is_connected (new->outputs[i]))
	     {
	        g_signal_emit (G_OBJECT (new->screen),
			       screen_signals[SCREEN_OUTPUT_CONNECTED], 0,
			       new->outputs[i]);
            }
	    continue;
	}
	if (!gsd_rr_output_is_connected (output_old) &&
	    gsd_rr_output_is_connected (new->outputs[i]))
	{
	    /* output connected */
	    g_signal_emit (G_OBJECT (new->screen),
			   screen_signals[SCREEN_OUTPUT_CONNECTED], 0,
			   new->outputs[i]);
         }
    }
}

static gboolean
screen_update (GsdRRScreen *screen, gboolean force_callback, gboolean needs_reprobe, GError **error)
{
    ScreenInfo *info;
    gboolean changed = FALSE;
    
    g_assert (screen != NULL);

    info = screen_info_new (screen, needs_reprobe, error);
    if (!info)
	    return FALSE;

    if (info->resources->configTimestamp != screen->priv->info->resources->configTimestamp)
	    changed = TRUE;

    /* work out if any outputs have changed connected state */
    diff_outputs_and_emit_signals (screen->priv->info, info);

    screen_info_free (screen->priv->info);
	
    screen->priv->info = info;

    if (changed || force_callback)
        g_signal_emit (G_OBJECT (screen), screen_signals[SCREEN_CHANGED], 0);
    
    return changed;
}

static GdkFilterReturn
screen_on_event (GdkXEvent *xevent,
		 GdkEvent *event,
		 gpointer data)
{
    GsdRRScreen *screen = data;
    GsdRRScreenPrivate *priv = screen->priv;
    XEvent *e = xevent;
    int event_num;

    if (!e)
	return GDK_FILTER_CONTINUE;

    event_num = e->type - priv->randr_event_base;

    if (event_num == RRScreenChangeNotify) {
	/* We don't reprobe the hardware; we just fetch the X server's latest
	 * state.  The server already knows the new state of the outputs; that's
	 * why it sent us an event!
	 */
        screen_update (screen, TRUE, FALSE, NULL); /* NULL-GError */
#if 0
	/* Enable this code to get a dialog showing the RANDR timestamps, for debugging purposes */
	{
	    GtkWidget *dialog;
	    XRRScreenChangeNotifyEvent *rr_event;
	    static int dialog_num;

	    rr_event = (XRRScreenChangeNotifyEvent *) e;

	    dialog = gtk_message_dialog_new (NULL,
					     0,
					     GTK_MESSAGE_INFO,
					     GTK_BUTTONS_CLOSE,
					     "RRScreenChangeNotify timestamps (%d):\n"
					     "event change: %u\n"
					     "event config: %u\n"
					     "event serial: %lu\n"
					     "----------------------"
					     "screen change: %u\n"
					     "screen config: %u\n",
					     dialog_num++,
					     (guint32) rr_event->timestamp,
					     (guint32) rr_event->config_timestamp,
					     rr_event->serial,
					     (guint32) priv->info->resources->timestamp,
					     (guint32) priv->info->resources->configTimestamp);
	    g_signal_connect (dialog, "response",
			      G_CALLBACK (gtk_widget_destroy), NULL);
	    gtk_widget_show (dialog);
	}
#endif
    }
#if 0
    /* WHY THIS CODE IS DISABLED:
     *
     * Note that in gsd_rr_screen_new(), we only select for
     * RRScreenChangeNotifyMask.  We used to select for other values in
     * RR*NotifyMask, but we weren't really doing anything useful with those
     * events.  We only care about "the screens changed in some way or another"
     * for now.
     *
     * If we ever run into a situtation that could benefit from processing more
     * detailed events, we can enable this code again.
     *
     * Note that the X server sends RRScreenChangeNotify in conjunction with the
     * more detailed events from RANDR 1.2 - see xserver/randr/randr.c:TellChanged().
     */
    else if (event_num == RRNotify)
    {
	/* Other RandR events */

	XRRNotifyEvent *event = (XRRNotifyEvent *)e;

	/* Here we can distinguish between RRNotify events supported
	 * since RandR 1.2 such as RRNotify_OutputProperty.  For now, we
	 * don't have anything special to do for particular subevent types, so
	 * we leave this as an empty switch().
	 */
	switch (event->subtype)
	{
	default:
	    break;
	}

	/* No need to reprobe hardware here */
	screen_update (screen, TRUE, FALSE, NULL); /* NULL-GError */
    }
#endif

    /* Pass the event on to GTK+ */
    return GDK_FILTER_CONTINUE;
}

static gboolean
gsd_rr_screen_initable_init (GInitable *initable, GCancellable *canc, GError **error)
{
    GsdRRScreen *self = GSD_RR_SCREEN (initable);
    GsdRRScreenPrivate *priv = self->priv;
    Display *dpy = GDK_SCREEN_XDISPLAY (self->priv->gdk_screen);
    int event_base;
    int ignore;

    priv->connector_type_atom = XInternAtom (dpy, "ConnectorType", FALSE);

    if (XRRQueryExtension (dpy, &event_base, &ignore))
    {
        priv->randr_event_base = event_base;

        XRRQueryVersion (dpy, &priv->rr_major_version, &priv->rr_minor_version);
        if (priv->rr_major_version < 1 || (priv->rr_major_version == 1 && priv->rr_minor_version < 2)) {
            g_set_error (error, GSD_RR_ERROR, GSD_RR_ERROR_NO_RANDR_EXTENSION,
                    "RANDR extension is too old (must be at least 1.2)");
            return FALSE;
        }

        priv->info = screen_info_new (self, TRUE, error);

        if (!priv->info) {
            return FALSE;
        }

        XRRSelectInput (priv->xdisplay,
                priv->xroot,
                RRScreenChangeNotifyMask);
        gdk_x11_register_standard_event_type (gdk_screen_get_display (priv->gdk_screen),
                          event_base,
                          RRNotify + 1);
        gdk_window_add_filter (priv->gdk_root, screen_on_event, self);

        return TRUE;
    }
    else
    {
      g_set_error (error, GSD_RR_ERROR, GSD_RR_ERROR_NO_RANDR_EXTENSION,
                   _("RANDR extension is not present"));

      return FALSE;
   }
}

void
gsd_rr_screen_initable_iface_init (GInitableIface *iface)
{
    iface->init = gsd_rr_screen_initable_init;
}

void
gsd_rr_screen_finalize (GObject *gobject)
{
    GsdRRScreen *screen = GSD_RR_SCREEN (gobject);

    gdk_window_remove_filter (screen->priv->gdk_root, screen_on_event, screen);

    if (screen->priv->info)
      screen_info_free (screen->priv->info);

    G_OBJECT_CLASS (gsd_rr_screen_parent_class)->finalize (gobject);
}

void
gsd_rr_screen_set_property (GObject *gobject, guint property_id, const GValue *value, GParamSpec *property)
{
    GsdRRScreen *self = GSD_RR_SCREEN (gobject);
    GsdRRScreenPrivate *priv = self->priv;

    switch (property_id)
    {
    case SCREEN_PROP_GDK_SCREEN:
        priv->gdk_screen = g_value_get_object (value);
        priv->gdk_root = gdk_screen_get_root_window (priv->gdk_screen);
        priv->xroot = gdk_x11_window_get_xid (priv->gdk_root);
        priv->xdisplay = GDK_SCREEN_XDISPLAY (priv->gdk_screen);
        priv->xscreen = gdk_x11_screen_get_xscreen (priv->gdk_screen);
        return;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, property_id, property);
        return;
    }
}

void
gsd_rr_screen_get_property (GObject *gobject, guint property_id, GValue *value, GParamSpec *property)
{
    GsdRRScreen *self = GSD_RR_SCREEN (gobject);
    GsdRRScreenPrivate *priv = self->priv;

    switch (property_id)
    {
    case SCREEN_PROP_GDK_SCREEN:
        g_value_set_object (value, priv->gdk_screen);
        return;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, property_id, property);
        return;
    }
}

void
gsd_rr_screen_class_init (GsdRRScreenClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    g_type_class_add_private (klass, sizeof (GsdRRScreenPrivate));

    bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

    gobject_class->set_property = gsd_rr_screen_set_property;
    gobject_class->get_property = gsd_rr_screen_get_property;
    gobject_class->finalize = gsd_rr_screen_finalize;

    g_object_class_install_property(
            gobject_class,
            SCREEN_PROP_GDK_SCREEN,
            g_param_spec_object (
                    "gdk-screen",
                    "GDK Screen",
                    "The GDK Screen represented by this GsdRRScreen",
                    GDK_TYPE_SCREEN,
                    G_PARAM_READWRITE |
		    G_PARAM_CONSTRUCT_ONLY |
		    G_PARAM_STATIC_STRINGS)
            );

    screen_signals[SCREEN_CHANGED] = g_signal_new("changed",
            G_TYPE_FROM_CLASS (gobject_class),
            G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
            G_STRUCT_OFFSET (GsdRRScreenClass, changed),
            NULL,
            NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE,
	    0);

    /**
     * GsdRRScreen::output-connected:
     * @screen: the #GsdRRScreen that emitted the signal
     * @output: the #GsdRROutput that was connected
     *
     * This signal is emitted when a display device is connected to a
     * port, or a port is hotplugged with an active output. The latter
     * can happen if a laptop is docked, and the dock provides a new
     * active output.
     *
     * The @output value is not a #GObject. The returned @output value can
     * only assume to be valid during the emission of the signal (i.e. within
     * your signal handler only), as it may change later when the @screen
     * is modified due to an event from the X server, or due to another
     * place in the application modifying the @screen and the @output.
     * Therefore, deal with changes to the @output right in your signal
     * handler, instead of keeping the @output reference for an async or
     * idle function.
     **/
    screen_signals[SCREEN_OUTPUT_CONNECTED] = g_signal_new("output-connected",
            G_TYPE_FROM_CLASS (gobject_class),
            G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
            G_STRUCT_OFFSET (GsdRRScreenClass, output_connected),
            NULL,
            NULL,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);

    /**
     * GsdRRScreen::output-disconnected:
     * @screen: the #GsdRRScreen that emitted the signal
     * @output: the #GsdRROutput that was disconnected
     *
     * This signal is emitted when a display device is disconnected from
     * a port, or a port output is hot-unplugged. The latter can happen
     * if a laptop is undocked, and the dock provided the output.
     *
     * The @output value is not a #GObject. The returned @output value can
     * only assume to be valid during the emission of the signal (i.e. within
     * your signal handler only), as it may change later when the @screen
     * is modified due to an event from the X server, or due to another
     * place in the application modifying the @screen and the @output.
     * Therefore, deal with changes to the @output right in your signal
     * handler, instead of keeping the @output reference for an async or
     * idle function.
     **/
    screen_signals[SCREEN_OUTPUT_DISCONNECTED] = g_signal_new("output-disconnected",
            G_TYPE_FROM_CLASS (gobject_class),
            G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
            G_STRUCT_OFFSET (GsdRRScreenClass, output_disconnected),
            NULL,
            NULL,
            g_cclosure_marshal_VOID__POINTER,
            G_TYPE_NONE,
            1, G_TYPE_POINTER);
}

void
gsd_rr_screen_init (GsdRRScreen *self)
{
    GsdRRScreenPrivate *priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GSD_TYPE_RR_SCREEN, GsdRRScreenPrivate);
    self->priv = priv;

    priv->gdk_screen = NULL;
    priv->gdk_root = NULL;
    priv->xdisplay = NULL;
    priv->xroot = None;
    priv->xscreen = NULL;
    priv->info = NULL;
    priv->rr_major_version = 0;
    priv->rr_minor_version = 0;
}

/* Weak reference callback set in gsd_rr_screen_new(); we remove the GObject data from the GdkScreen. */
static void
rr_screen_weak_notify_cb (gpointer data, GObject *where_the_object_was)
{
    GdkScreen *screen = GDK_SCREEN (data);

    g_object_set_data (G_OBJECT (screen), "GsdRRScreen", NULL);
}

/**
 * gsd_rr_screen_new:
 * @screen: the #GdkScreen on which to operate
 * @error: will be set if XRandR is not supported
 *
 * Creates a unique #GsdRRScreen instance for the specified @screen.
 *
 * Returns: a unique #GsdRRScreen instance, specific to the @screen, or NULL
 * if this could not be created, for instance if the driver does not support
 * Xrandr 1.2.  Each #GdkScreen thus has a single instance of #GsdRRScreen.
 */
GsdRRScreen *
gsd_rr_screen_new (GdkScreen *screen,
		     GError **error)
{
    GsdRRScreen *rr_screen;

    g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);
    g_return_val_if_fail (error == NULL || *error == NULL, NULL);

    rr_screen = g_object_get_data (G_OBJECT (screen), "GsdRRScreen");
    if (rr_screen)
	g_object_ref (rr_screen);
    else {
	rr_screen = g_initable_new (GSD_TYPE_RR_SCREEN, NULL, error, "gdk-screen", screen, NULL);
	if (rr_screen) {
	    g_object_set_data (G_OBJECT (screen), "GsdRRScreen", rr_screen);
	    g_object_weak_ref (G_OBJECT (rr_screen), rr_screen_weak_notify_cb, screen);
	}
    }

    return rr_screen;
}

void
gsd_rr_screen_set_size (GsdRRScreen *screen,
			  int	      width,
			  int       height,
			  int       mm_width,
			  int       mm_height)
{
    g_return_if_fail (GSD_IS_RR_SCREEN (screen));

    gdk_error_trap_push ();
    XRRSetScreenSize (screen->priv->xdisplay, screen->priv->xroot,
		      width, height, mm_width, mm_height);
    gdk_error_trap_pop_ignored ();
}

/**
 * gsd_rr_screen_get_ranges:
 * @screen: a #GsdRRScreen
 * @min_width: (out): the minimum width
 * @max_width: (out): the maximum width
 * @min_height: (out): the minimum height
 * @max_height: (out): the maximum height
 *
 * Get the ranges of the screen
 */
void
gsd_rr_screen_get_ranges (GsdRRScreen *screen,
			    int	          *min_width,
			    int	          *max_width,
			    int           *min_height,
			    int	          *max_height)
{
    GsdRRScreenPrivate *priv;

    g_return_if_fail (GSD_IS_RR_SCREEN (screen));

    priv = screen->priv;
    
    if (min_width)
	*min_width = priv->info->min_width;
    
    if (max_width)
	*max_width = priv->info->max_width;
    
    if (min_height)
	*min_height = priv->info->min_height;
    
    if (max_height)
	*max_height = priv->info->max_height;
}

/**
 * gsd_rr_screen_get_timestamps:
 * @screen: a #GsdRRScreen
 * @change_timestamp_ret: (out): Location in which to store the timestamp at which the RANDR configuration was last changed
 * @config_timestamp_ret: (out): Location in which to store the timestamp at which the RANDR configuration was last obtained
 *
 * Queries the two timestamps that the X RANDR extension maintains.  The X
 * server will prevent change requests for stale configurations, those whose
 * timestamp is not equal to that of the latest request for configuration.  The
 * X server will also prevent change requests that have an older timestamp to
 * the latest change request.
 */
void
gsd_rr_screen_get_timestamps (GsdRRScreen *screen,
				guint32       *change_timestamp_ret,
				guint32       *config_timestamp_ret)
{
    GsdRRScreenPrivate *priv;

    g_return_if_fail (GSD_IS_RR_SCREEN (screen));

    priv = screen->priv;

    if (change_timestamp_ret)
	*change_timestamp_ret = priv->info->resources->timestamp;

    if (config_timestamp_ret)
	*config_timestamp_ret = priv->info->resources->configTimestamp;
}

static gboolean
force_timestamp_update (GsdRRScreen *screen)
{
    GsdRRScreenPrivate *priv = screen->priv;
    GsdRRCrtc *crtc;
    XRRCrtcInfo *current_info;
    Status status;
    gboolean timestamp_updated;

    timestamp_updated = FALSE;

    crtc = priv->info->crtcs[0];

    if (crtc == NULL)
	goto out;

    current_info = XRRGetCrtcInfo (priv->xdisplay,
				   priv->info->resources,
				   crtc->id);

    if (current_info == NULL)
	goto out;

    gdk_error_trap_push ();
    status = XRRSetCrtcConfig (priv->xdisplay,
			       priv->info->resources,
			       crtc->id,
			       current_info->timestamp,
			       current_info->x,
			       current_info->y,
			       current_info->mode,
			       current_info->rotation,
			       current_info->outputs,
			       current_info->noutput);

    XRRFreeCrtcInfo (current_info);

    gdk_flush ();
    if (gdk_error_trap_pop ())
	goto out;

    if (status == RRSetConfigSuccess)
	timestamp_updated = TRUE;
out:
    return timestamp_updated;
}

/**
 * gsd_rr_screen_refresh:
 * @screen: a #GsdRRScreen
 * @error: location to store error, or %NULL
 *
 * Refreshes the screen configuration, and calls the screen's callback if it
 * exists and if the screen's configuration changed.
 *
 * Return value: TRUE if the screen's configuration changed; otherwise, the
 * function returns FALSE and a NULL error if the configuration didn't change,
 * or FALSE and a non-NULL error if there was an error while refreshing the
 * configuration.
 */
gboolean
gsd_rr_screen_refresh (GsdRRScreen *screen,
			 GError       **error)
{
    gboolean refreshed;

    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    gdk_x11_display_grab (gdk_screen_get_display (screen->priv->gdk_screen));

    refreshed = screen_update (screen, FALSE, TRUE, error);
    force_timestamp_update (screen); /* this is to keep other clients from thinking that the X server re-detected things by itself - bgo#621046 */

    gdk_x11_display_ungrab (gdk_screen_get_display (screen->priv->gdk_screen));

    return refreshed;
}

/**
 * gsd_rr_screen_get_dpms_mode:
 * @mode: (out): The current #GsdRRDpmsMode of this screen
 **/
gboolean
gsd_rr_screen_get_dpms_mode (GsdRRScreen *screen,
                               GsdRRDpmsMode *mode,
                               GError **error)
{
    BOOL enabled = FALSE;
    CARD16 state;
    gboolean ret = FALSE;

    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
    g_return_val_if_fail (mode != NULL, FALSE);

    if (!screen->priv->dpms_capable) {
        g_set_error_literal (error,
                             GSD_RR_ERROR,
                             GSD_RR_ERROR_NO_DPMS_EXTENSION,
                             "Display is not DPMS capable");
        goto out;
    }

    if (!DPMSInfo (screen->priv->xdisplay,
                   &state,
                   &enabled)) {
        g_set_error_literal (error,
                             GSD_RR_ERROR,
                             GSD_RR_ERROR_UNKNOWN,
                             "Unable to get DPMS state");
        goto out;
    }

    /* DPMS not enabled is a valid mode */
    if (!enabled) {
        *mode = GSD_RR_DPMS_DISABLED;
        ret = TRUE;
        goto out;
    }

    switch (state) {
    case DPMSModeOn:
        *mode = GSD_RR_DPMS_ON;
        break;
    case DPMSModeStandby:
        *mode = GSD_RR_DPMS_STANDBY;
        break;
    case DPMSModeSuspend:
        *mode = GSD_RR_DPMS_SUSPEND;
        break;
    case DPMSModeOff:
        *mode = GSD_RR_DPMS_OFF;
        break;
    default:
        g_assert_not_reached ();
        break;
    }
    ret = TRUE;
out:
    return ret;
}

/**
 * gsd_rr_screen_clear_dpms_timeouts:
 **/
static gboolean
gsd_rr_screen_clear_dpms_timeouts (GsdRRScreen *screen,
                                     GError **error)
{
    gdk_error_trap_push ();
    /* DPMSSetTimeouts() return value is often a lie, so ignore it */
    DPMSSetTimeouts (screen->priv->xdisplay, 0, 0, 0);
    if (gdk_error_trap_pop ()) {
        g_set_error_literal (error,
                             GSD_RR_ERROR,
                             GSD_RR_ERROR_UNKNOWN,
                             "Could not set DPMS timeouts");
        return FALSE;
    }
    return TRUE;
}

/**
 * gsd_rr_screen_set_dpms_mode:
 *
 * This method also disables the DPMS timeouts.
 **/
gboolean
gsd_rr_screen_set_dpms_mode (GsdRRScreen *screen,
                               GsdRRDpmsMode mode,
                               GError **error)
{
    CARD16 state = 0;
    gboolean ret;
    GsdRRDpmsMode current_mode;

    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

    /* set, if the new mode is different */
    ret = gsd_rr_screen_get_dpms_mode (screen, &current_mode, error);
    if (!ret)
        goto out;
    if (current_mode == mode) {
        ret = gsd_rr_screen_clear_dpms_timeouts (screen, error);
        goto out;
    }

    switch (mode) {
    case GSD_RR_DPMS_ON:
        state = DPMSModeOn;
        break;
    case GSD_RR_DPMS_STANDBY:
        state = DPMSModeStandby;
        break;
    case GSD_RR_DPMS_SUSPEND:
        state = DPMSModeSuspend;
        break;
    case GSD_RR_DPMS_OFF:
        state = DPMSModeOff;
        break;
    default:
        g_assert_not_reached ();
        break;
    }

    gdk_error_trap_push ();
    /* DPMSForceLevel() return value is often a lie, so ignore it */
    DPMSForceLevel (screen->priv->xdisplay, state);
    XSync (screen->priv->xdisplay, False);
    if (gdk_error_trap_pop ()) {
        ret = FALSE;
        g_set_error_literal (error,
                             GSD_RR_ERROR,
                             GSD_RR_ERROR_UNKNOWN,
                             "Could not change DPMS mode");
        goto out;
    }

    ret = gsd_rr_screen_clear_dpms_timeouts (screen, error);
    if (!ret)
        goto out;
out:
    return ret;
}

/**
 * gsd_rr_screen_list_modes:
 *
 * List available XRandR modes
 *
 * Returns: (array zero-terminated=1) (transfer none):
 */
GsdRRMode **
gsd_rr_screen_list_modes (GsdRRScreen *screen)
{
    g_return_val_if_fail (GSD_IS_RR_SCREEN (screen), NULL);
    g_return_val_if_fail (screen->priv->info != NULL, NULL);
    
    return screen->priv->info->modes;
}

/**
 * gsd_rr_screen_list_clone_modes:
 *
 * List available XRandR clone modes
 *
 * Returns: (array zero-terminated=1) (transfer none):
 */
GsdRRMode **
gsd_rr_screen_list_clone_modes   (GsdRRScreen *screen)
{
    g_return_val_if_fail (GSD_IS_RR_SCREEN (screen), NULL);
    g_return_val_if_fail (screen->priv->info != NULL, NULL);

    return screen->priv->info->clone_modes;
}

/**
 * gsd_rr_screen_list_crtcs:
 *
 * List all CRTCs
 *
 * Returns: (array zero-terminated=1) (transfer none):
 */
GsdRRCrtc **
gsd_rr_screen_list_crtcs (GsdRRScreen *screen)
{
    g_return_val_if_fail (GSD_IS_RR_SCREEN (screen), NULL);
    g_return_val_if_fail (screen->priv->info != NULL, NULL);
    
    return screen->priv->info->crtcs;
}

/**
 * gsd_rr_screen_list_outputs:
 *
 * List all outputs
 *
 * Returns: (array zero-terminated=1) (transfer none):
 */
GsdRROutput **
gsd_rr_screen_list_outputs (GsdRRScreen *screen)
{
    g_return_val_if_fail (GSD_IS_RR_SCREEN (screen), NULL);
    g_return_val_if_fail (screen->priv->info != NULL, NULL);
    
    return screen->priv->info->outputs;
}

/**
 * gsd_rr_screen_get_crtc_by_id:
 *
 * Returns: (transfer none): the CRTC identified by @id
 */
GsdRRCrtc *
gsd_rr_screen_get_crtc_by_id (GsdRRScreen *screen,
				guint32        id)
{
    GsdRRCrtc **crtcs;
    int i;
    
    g_return_val_if_fail (GSD_IS_RR_SCREEN (screen), NULL);
    g_return_val_if_fail (screen->priv->info != NULL, NULL);

    crtcs = screen->priv->info->crtcs;
    
    for (i = 0; crtcs[i] != NULL; ++i)
    {
	if (crtcs[i]->id == id)
	    return crtcs[i];
    }
    
    return NULL;
}

/**
 * gsd_rr_screen_get_output_by_id:
 *
 * Returns: (transfer none): the output identified by @id
 */
GsdRROutput *
gsd_rr_screen_get_output_by_id (GsdRRScreen *screen,
				  guint32        id)
{
    GsdRROutput **outputs;
    int i;
    
    g_return_val_if_fail (GSD_IS_RR_SCREEN (screen), NULL);
    g_return_val_if_fail (screen->priv->info != NULL, NULL);

    outputs = screen->priv->info->outputs;

    for (i = 0; outputs[i] != NULL; ++i)
    {
	if (outputs[i]->id == id)
	    return outputs[i];
    }
    
    return NULL;
}

/* GsdRROutput */
static GsdRROutput *
output_new (ScreenInfo *info, RROutput id)
{
    GsdRROutput *output = g_slice_new0 (GsdRROutput);
    
    output->id = id;
    output->info = info;
    
    return output;
}

static guint8 *
get_property (Display *dpy,
	      RROutput output,
	      Atom atom,
	      gsize *len)
{
    unsigned char *prop;
    int actual_format;
    unsigned long nitems, bytes_after;
    Atom actual_type;
    guint8 *result;
    
    XRRGetOutputProperty (dpy, output, atom,
			  0, 100, False, False,
			  AnyPropertyType,
			  &actual_type, &actual_format,
			  &nitems, &bytes_after, &prop);
    
    if (actual_type == XA_INTEGER && actual_format == 8)
    {
	result = g_memdup (prop, nitems);
	if (len)
	    *len = nitems;
    }
    else
    {
	result = NULL;
    }
    
    XFree (prop);
    
    return result;
}

static guint8 *
read_edid_data (GsdRROutput *output, gsize *len)
{
    Atom edid_atom;
    guint8 *result;

    edid_atom = XInternAtom (DISPLAY (output), "EDID", FALSE);
    result = get_property (DISPLAY (output),
			   output->id, edid_atom, len);

    if (!result)
    {
	edid_atom = XInternAtom (DISPLAY (output), "EDID_DATA", FALSE);
	result = get_property (DISPLAY (output),
			       output->id, edid_atom, len);
    }

    if (!result)
    {
	edid_atom = XInternAtom (DISPLAY (output), "XFree86_DDC_EDID1_RAWDATA", FALSE);
	result = get_property (DISPLAY (output),
			       output->id, edid_atom, len);
    }

    if (result)
    {
	if (*len % 128 == 0)
	    return result;
	else
	    g_free (result);
    }
    
    return NULL;
}

static char *
get_connector_type_string (GsdRROutput *output)
{
    char *result;
    unsigned char *prop;
    int actual_format;
    unsigned long nitems, bytes_after;
    Atom actual_type;
    Atom connector_type;
    char *connector_type_str;

    result = NULL;

    if (XRRGetOutputProperty (DISPLAY (output), output->id, output->info->screen->priv->connector_type_atom,
			      0, 100, False, False,
			      AnyPropertyType,
			      &actual_type, &actual_format,
			      &nitems, &bytes_after, &prop) != Success)
	return NULL;

    if (!(actual_type == XA_ATOM && actual_format == 32 && nitems == 1))
	goto out;

    connector_type = *((Atom *) prop);

    connector_type_str = XGetAtomName (DISPLAY (output), connector_type);
    if (connector_type_str) {
	result = g_strdup (connector_type_str); /* so the caller can g_free() it */
	XFree (connector_type_str);
    }

out:

    XFree (prop);

    return result;
}

static void
update_brightness_limits (GsdRROutput *output)
{
    gint rc;
    Atom atom;
    XRRPropertyInfo *info;

    gdk_error_trap_push ();
    atom = XInternAtom (DISPLAY (output), "Backlight", FALSE);
    info = XRRQueryOutputProperty (DISPLAY (output), output->id, atom);
    rc = gdk_error_trap_pop ();
    if (rc != Success)
    {
        if (rc != BadName)
          g_warning ("could not get output property for %s, rc: %i",
		     output->name, rc);
        goto out;
    }
    if (info == NULL)
    {
        g_warning ("could not get output property for %s",
		   output->name);
        goto out;
    }
    if (!info->range || info->num_values != 2)
    {
        g_debug ("backlight %s was not range", output->name);
        goto out;
    }
    output->backlight_min = info->values[0];
    output->backlight_max = info->values[1];
out:
    if (info != NULL)
    {
        XFree (info);
    }
}

static gboolean
output_initialize (GsdRROutput *output, XRRScreenResources *res, GError **error)
{
    XRROutputInfo *info = XRRGetOutputInfo (
	DISPLAY (output), res, output->id);
    GPtrArray *a;
    int i;
    
#if 0
    g_print ("Output %lx Timestamp: %u\n", output->id, (guint32)info->timestamp);
#endif
    
    if (!info || !output->info)
    {
	/* FIXME: see the comment in crtc_initialize() */
	/* Translators: here, an "output" is a video output */
	g_set_error (error, GSD_RR_ERROR, GSD_RR_ERROR_RANDR_ERROR,
		     _("could not get information about output %d"),
		     (int) output->id);
	return FALSE;
    }
    
    output->name = g_strdup (info->name); /* FIXME: what is nameLen used for? */
    output->display_name = NULL; /* set first time the getter is used */
    output->current_crtc = crtc_by_id (output->info, info->crtc);
    output->width_mm = info->mm_width;
    output->height_mm = info->mm_height;
    output->connected = (info->connection == RR_Connected);
    output->connector_type = get_connector_type_string (output);

    /* Possible crtcs */
    a = g_ptr_array_new ();
    
    for (i = 0; i < info->ncrtc; ++i)
    {
	GsdRRCrtc *crtc = crtc_by_id (output->info, info->crtcs[i]);
	
	if (crtc)
	    g_ptr_array_add (a, crtc);
    }
    g_ptr_array_add (a, NULL);
    output->possible_crtcs = (GsdRRCrtc **)g_ptr_array_free (a, FALSE);
    
    /* Clones */
    a = g_ptr_array_new ();
    for (i = 0; i < info->nclone; ++i)
    {
	GsdRROutput *gsd_rr_output = gsd_rr_output_by_id (output->info, info->clones[i]);
	
	if (gsd_rr_output)
	    g_ptr_array_add (a, gsd_rr_output);
    }
    g_ptr_array_add (a, NULL);
    output->clones = (GsdRROutput **)g_ptr_array_free (a, FALSE);
    
    /* Modes */
    a = g_ptr_array_new ();
    for (i = 0; i < info->nmode; ++i)
    {
	GsdRRMode *mode = mode_by_id (output->info, info->modes[i]);
	
	if (mode)
	    g_ptr_array_add (a, mode);
    }
    g_ptr_array_add (a, NULL);
    output->modes = (GsdRRMode **)g_ptr_array_free (a, FALSE);
    
    output->n_preferred = info->npreferred;
    
    /* Edid data */
    output->edid_data = read_edid_data (output, &output->edid_size);

    /* brightness data */
    if (output->connected)
        update_brightness_limits (output);

    XRRFreeOutputInfo (info);

    return TRUE;
}

static GsdRROutput*
output_copy (const GsdRROutput *from)
{
    GPtrArray *array;
    GsdRRCrtc **p_crtc;
    GsdRROutput **p_output;
    GsdRRMode **p_mode;
    GsdRROutput *output = g_slice_new0 (GsdRROutput);

    output->id = from->id;
    output->info = from->info;
    output->name = g_strdup (from->name);
    output->current_crtc = from->current_crtc;
    output->width_mm = from->width_mm;
    output->height_mm = from->height_mm;
    output->connected = from->connected;
    output->n_preferred = from->n_preferred;
    output->connector_type = g_strdup (from->connector_type);
    output->backlight_min = -1;
    output->backlight_max = -1;

    array = g_ptr_array_new ();
    for (p_crtc = from->possible_crtcs; *p_crtc != NULL; p_crtc++)
    {
        g_ptr_array_add (array, *p_crtc);
    }
    output->possible_crtcs = (GsdRRCrtc**) g_ptr_array_free (array, FALSE);

    array = g_ptr_array_new ();
    for (p_output = from->clones; *p_output != NULL; p_output++)
    {
        g_ptr_array_add (array, *p_output);
    }
    output->clones = (GsdRROutput**) g_ptr_array_free (array, FALSE);

    array = g_ptr_array_new ();
    for (p_mode = from->modes; *p_mode != NULL; p_mode++)
    {
        g_ptr_array_add (array, *p_mode);
    }
    output->modes = (GsdRRMode**) g_ptr_array_free (array, FALSE);

    output->edid_size = from->edid_size;
    output->edid_data = g_memdup (from->edid_data, from->edid_size);

    return output;
}

static void
output_free (GsdRROutput *output)
{
    g_free (output->clones);
    g_free (output->modes);
    g_free (output->possible_crtcs);
    g_free (output->edid_data);
    g_free (output->name);
    g_free (output->display_name);
    g_free (output->connector_type);
    g_slice_free (GsdRROutput, output);
}

guint32
gsd_rr_output_get_id (GsdRROutput *output)
{
    g_assert(output != NULL);
    
    return output->id;
}

const guint8 *
gsd_rr_output_get_edid_data (GsdRROutput *output, gsize *size)
{
    g_return_val_if_fail (output != NULL, NULL);
    if (size)
        *size = output->edid_size;
    return output->edid_data;
}

/**
 * gsd_rr_output_get_ids_from_edid:
 * @output: a #GsdRROutput
 * @vendor: (out) (allow-none):
 * @product: (out) (allow-none):
 * @serial: (out) (allow-none):
 */
gboolean
gsd_rr_output_get_ids_from_edid (GsdRROutput         *output,
                                   char                 **vendor,
                                   int                   *product,
                                   int                   *serial)
{
    MonitorInfo *info;

    g_return_val_if_fail (output != NULL, FALSE);

    if (!output->edid_data)
        return FALSE;
    info = decode_edid (output->edid_data);
    if (!info)
        return FALSE;
    if (vendor)
        *vendor = g_memdup (info->manufacturer_code, 4);
    if (product)
        *product = info->product_code;
    if (serial)
        *serial = info->serial_number;

    g_free (info);

    return TRUE;

}

static void
ensure_display_name (GsdRROutput *output)
{
    if (output->display_name != NULL)
        return;

    if (gsd_rr_output_is_laptop (output))
        output->display_name = g_strdup (_("Built-in Display"));

    if (output->display_name == NULL
        && output->edid_data != NULL) {
        MonitorInfo *info;

        info = decode_edid (output->edid_data);
        if (info != NULL)
            output->display_name = make_display_name (info);

        g_free (info);
    }

    if (output->display_name == NULL) {
        char *inches;
        inches = make_display_size_string (output->width_mm, output->height_mm);
        if (inches != NULL) {
            /* Translators: %s is the size of the monitor in inches */
            output->display_name = g_strdup_printf (_("%s Display"), inches);
        }
        g_free (inches);
    }

    /* last chance on the stairway */
    if (output->display_name == NULL) {
      output->display_name = g_strdup (_("Unknown Display"));
    }
}

const char *
gsd_rr_output_get_display_name (GsdRROutput *output)
{
    g_return_val_if_fail (output != NULL, NULL);

    ensure_display_name (output);

    return output->display_name;
}

/**
 * gsd_rr_output_get_backlight_min:
 *
 * Returns: The mimimum backlight value, or -1 if not supported
 */
gint
gsd_rr_output_get_backlight_min (GsdRROutput *output)
{
    g_return_val_if_fail (output != NULL, -1);
    return output->backlight_min;
}

/**
 * gsd_rr_output_get_backlight_max:
 *
 * Returns: The maximum backlight value, or -1 if not supported
 */
gint
gsd_rr_output_get_backlight_max (GsdRROutput *output)
{
    g_return_val_if_fail (output != NULL, -1);
    return output->backlight_max;
}

/**
 * gsd_rr_output_get_backlight:
 *
 * Returns: The currently set backlight brightness
 */
gint
gsd_rr_output_get_backlight (GsdRROutput *output, GError **error)
{
    guint now = -1;
    unsigned long nitems;
    unsigned long bytes_after;
    guint *prop;
    Atom atom;
    Atom actual_type;
    int actual_format;
    gint retval;

    g_return_val_if_fail (output != NULL, -1);

    gdk_error_trap_push ();
    atom = XInternAtom (DISPLAY (output), "Backlight", FALSE);
    retval = XRRGetOutputProperty (DISPLAY (output), output->id, atom,
				   0, 4, False, False, None,
				   &actual_type, &actual_format,
				   &nitems, &bytes_after, ((unsigned char **)&prop));
    gdk_flush ();
    if (gdk_error_trap_pop ())
    {
        g_set_error_literal (error,
			     GSD_RR_ERROR,
			     GSD_RR_ERROR_UNKNOWN,
			     "unhandled X error while getting the range of backlight values");
        goto out;
    }

    if (retval != Success) {
        g_set_error_literal (error,
			     GSD_RR_ERROR,
			     GSD_RR_ERROR_RANDR_ERROR,
			     "could not get the range of backlight values");
        goto out;
    }
    if (actual_type == XA_INTEGER &&
        nitems == 1 &&
        actual_format == 32)
    {
        memcpy (&now, prop, sizeof (guint));
    }
    else
    {
	g_set_error (error,
		     GSD_RR_ERROR,
		     GSD_RR_ERROR_RANDR_ERROR,
		     "failed to get correct property type, got %lu,%i",
		     nitems, actual_format);
    }
out:
    XFree (prop);
    return now;
}

/**
 * gsd_rr_output_set_backlight:
 * @value: the absolute value which is min >= this <= max
 *
 * Returns: %TRUE for success
 */
gboolean
gsd_rr_output_set_backlight (GsdRROutput *output, gint value, GError **error)
{
    gboolean ret = FALSE;
    Atom atom;

    g_return_val_if_fail (output != NULL, FALSE);

    /* check this is sane */
    if (value < output->backlight_min ||
        value > output->backlight_max)
    {
	g_set_error (error,
		     GSD_RR_ERROR,
		     GSD_RR_ERROR_BOUNDS_ERROR,
		     "out of brightness range: %i, has to be %i -> %i",
		     value,
		     output->backlight_max, output->backlight_min);
	goto out;
    }

    /* don't abort on error */
    gdk_error_trap_push ();
    atom = XInternAtom (DISPLAY (output), "Backlight", FALSE);
    XRRChangeOutputProperty (DISPLAY (output), output->id, atom,
			     XA_INTEGER, 32, PropModeReplace,
			     (unsigned char *) &value, 1);
    if (gdk_error_trap_pop ())
    {
        g_set_error_literal (error,
			     GSD_RR_ERROR,
			     GSD_RR_ERROR_UNKNOWN,
			     "unhandled X error while setting the backlight values");
        goto out;
    }

    /* we assume this succeeded as there's no return value */
    ret = TRUE;
out:
    return ret;
}

/**
 * gsd_rr_screen_get_output_by_name:
 *
 * Returns: (transfer none): the output identified by @name
 */
GsdRROutput *
gsd_rr_screen_get_output_by_name (GsdRRScreen *screen,
				    const char    *name)
{
    int i;
    
    g_return_val_if_fail (GSD_IS_RR_SCREEN (screen), NULL);
    g_return_val_if_fail (screen->priv->info != NULL, NULL);
    
    for (i = 0; screen->priv->info->outputs[i] != NULL; ++i)
    {
	GsdRROutput *output = screen->priv->info->outputs[i];
	
	if (strcmp (output->name, name) == 0)
	    return output;
    }
    
    return NULL;
}

GsdRRCrtc *
gsd_rr_output_get_crtc (GsdRROutput *output)
{
    g_return_val_if_fail (output != NULL, NULL);
    
    return output->current_crtc;
}

/* Returns NULL if the ConnectorType property is not available */
const char *
gsd_rr_output_get_connector_type (GsdRROutput *output)
{
    g_return_val_if_fail (output != NULL, NULL);

    return output->connector_type;
}

gboolean
_gsd_rr_output_name_is_laptop (const char *name)
{
    if (!name)
        return FALSE;

    if (strstr (name, "lvds") ||  /* Most drivers use an "LVDS" prefix... */
	strstr (name, "LVDS") ||
	strstr (name, "Lvds") ||
	strstr (name, "LCD")  ||  /* ... but fglrx uses "LCD" in some versions.  Shoot me now, kthxbye. */
	strstr (name, "eDP")  ||  /* eDP is for internal laptop panel connections */
	strstr (name, "default")) /* Finally, NVidia and all others that don't bother to do RANDR properly */
        return TRUE;

    return FALSE;
}

gboolean
gsd_rr_output_is_laptop (GsdRROutput *output)
{
    g_return_val_if_fail (output != NULL, FALSE);

    if (!output->connected)
	return FALSE;

    /* The ConnectorType property is present in RANDR 1.3 and greater */
    if (g_strcmp0 (output->connector_type, GSD_RR_CONNECTOR_TYPE_PANEL) == 0)
	return TRUE;

    /* Older versions of RANDR - this is a best guess, as @#$% RANDR doesn't have standard output names,
     * so drivers can use whatever they like.
     */
    if (_gsd_rr_output_name_is_laptop (output->name))
        return TRUE;

    return FALSE;
}

GsdRRMode *
gsd_rr_output_get_current_mode (GsdRROutput *output)
{
    GsdRRCrtc *crtc;
    
    g_return_val_if_fail (output != NULL, NULL);
    
    if ((crtc = gsd_rr_output_get_crtc (output)))
	return gsd_rr_crtc_get_current_mode (crtc);
    
    return NULL;
}

/**
 * gsd_rr_output_get_position:
 * @output: a #GsdRROutput
 * @x: (out) (allow-none):
 * @y: (out) (allow-none):
 */
void
gsd_rr_output_get_position (GsdRROutput   *output,
			      int             *x,
			      int             *y)
{
    GsdRRCrtc *crtc;
    
    g_return_if_fail (output != NULL);
    
    if ((crtc = gsd_rr_output_get_crtc (output)))
	gsd_rr_crtc_get_position (crtc, x, y);
}

const char *
gsd_rr_output_get_name (GsdRROutput *output)
{
    g_assert (output != NULL);
    return output->name;
}

int
gsd_rr_output_get_width_mm (GsdRROutput *output)
{
    g_assert (output != NULL);
    return output->width_mm;
}

int
gsd_rr_output_get_height_mm (GsdRROutput *output)
{
    g_assert (output != NULL);
    return output->height_mm;
}

GsdRRMode *
gsd_rr_output_get_preferred_mode (GsdRROutput *output)
{
    g_return_val_if_fail (output != NULL, NULL);
    if (output->n_preferred)
	return output->modes[0];
    
    return NULL;
}

GsdRRMode **
gsd_rr_output_list_modes (GsdRROutput *output)
{
    g_return_val_if_fail (output != NULL, NULL);
    return output->modes;
}

gboolean
gsd_rr_output_is_connected (GsdRROutput *output)
{
    g_return_val_if_fail (output != NULL, FALSE);
    return output->connected;
}

gboolean
gsd_rr_output_supports_mode (GsdRROutput *output,
			       GsdRRMode   *mode)
{
    int i;
    
    g_return_val_if_fail (output != NULL, FALSE);
    g_return_val_if_fail (mode != NULL, FALSE);
    
    for (i = 0; output->modes[i] != NULL; ++i)
    {
	if (output->modes[i] == mode)
	    return TRUE;
    }
    
    return FALSE;
}

gboolean
gsd_rr_output_can_clone (GsdRROutput *output,
			   GsdRROutput *clone)
{
    int i;
    
    g_return_val_if_fail (output != NULL, FALSE);
    g_return_val_if_fail (clone != NULL, FALSE);
    
    for (i = 0; output->clones[i] != NULL; ++i)
    {
	if (output->clones[i] == clone)
	    return TRUE;
    }
    
    return FALSE;
}

gboolean
gsd_rr_output_get_is_primary (GsdRROutput *output)
{
    return output->info->primary == output->id;
}

void
gsd_rr_screen_set_primary_output (GsdRRScreen *screen,
                                    GsdRROutput *output)
{
    GsdRRScreenPrivate *priv;
    RROutput id;

    g_return_if_fail (GSD_IS_RR_SCREEN (screen));

    priv = screen->priv;

    if (output)
        id = output->id;
    else
        id = None;

    if (SERVERS_RANDR_IS_AT_LEAST_1_3 (priv)) {
        gdk_error_trap_push ();
        XRRSetOutputPrimary (priv->xdisplay, priv->xroot, id);
        gdk_error_trap_pop_ignored ();
    }
}

/* GsdRRCrtc */
typedef struct
{
    Rotation xrot;
    GsdRRRotation rot;
} RotationMap;

static const RotationMap rotation_map[] =
{
    { RR_Rotate_0, GSD_RR_ROTATION_0 },
    { RR_Rotate_90, GSD_RR_ROTATION_90 },
    { RR_Rotate_180, GSD_RR_ROTATION_180 },
    { RR_Rotate_270, GSD_RR_ROTATION_270 },
    { RR_Reflect_X, GSD_RR_REFLECT_X },
    { RR_Reflect_Y, GSD_RR_REFLECT_Y },
};

static GsdRRRotation
gsd_rr_rotation_from_xrotation (Rotation r)
{
    int i;
    GsdRRRotation result = 0;
    
    for (i = 0; i < G_N_ELEMENTS (rotation_map); ++i)
    {
	if (r & rotation_map[i].xrot)
	    result |= rotation_map[i].rot;
    }
    
    return result;
}

static Rotation
xrotation_from_rotation (GsdRRRotation r)
{
    int i;
    Rotation result = 0;
    
    for (i = 0; i < G_N_ELEMENTS (rotation_map); ++i)
    {
	if (r & rotation_map[i].rot)
	    result |= rotation_map[i].xrot;
    }
    
    return result;
}

gboolean
gsd_rr_crtc_set_config_with_time (GsdRRCrtc      *crtc,
				    guint32           timestamp,
				    int               x,
				    int               y,
				    GsdRRMode      *mode,
				    GsdRRRotation   rotation,
				    GsdRROutput   **outputs,
				    int               n_outputs,
				    GError          **error)
{
    ScreenInfo *info;
    GArray *output_ids;
    Status status;
    gboolean result;
    int i;
    
    g_return_val_if_fail (crtc != NULL, FALSE);
    g_return_val_if_fail (mode != NULL || outputs == NULL || n_outputs == 0, FALSE);
    g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
    
    info = crtc->info;
    
    if (mode)
    {
	if (x + mode->width > info->max_width
	    || y + mode->height > info->max_height)
	{
	    g_set_error (error, GSD_RR_ERROR, GSD_RR_ERROR_BOUNDS_ERROR,
			 /* Translators: the "position", "size", and "maximum"
			  * words here are not keywords; please translate them
			  * as usual.  A CRTC is a CRT Controller (this is X terminology) */
			 _("requested position/size for CRTC %d is outside the allowed limit: "
			   "position=(%d, %d), size=(%d, %d), maximum=(%d, %d)"),
			 (int) crtc->id,
			 x, y,
			 mode->width, mode->height,
			 info->max_width, info->max_height);
	    return FALSE;
	}
    }
    
    output_ids = g_array_new (FALSE, FALSE, sizeof (RROutput));
    
    if (outputs)
    {
	for (i = 0; i < n_outputs; ++i)
	    g_array_append_val (output_ids, outputs[i]->id);
    }

    gdk_error_trap_push ();
    status = XRRSetCrtcConfig (DISPLAY (crtc), info->resources, crtc->id,
			       timestamp, 
			       x, y,
			       mode ? mode->id : None,
			       xrotation_from_rotation (rotation),
			       (RROutput *)output_ids->data,
			       output_ids->len);
    
    g_array_free (output_ids, TRUE);

    if (gdk_error_trap_pop () || status != RRSetConfigSuccess) {
	/* Translators: CRTC is a CRT Controller (this is X terminology).
	 * It is *very* unlikely that you'll ever get this error, so it is
	 * only listed for completeness. */
	g_set_error (error, GSD_RR_ERROR, GSD_RR_ERROR_RANDR_ERROR,
		     _("could not set the configuration for CRTC %d"),
		     (int) crtc->id);
        return FALSE;
    } else {
        result = TRUE;
    }
    
    return result;
}

GsdRRMode *
gsd_rr_crtc_get_current_mode (GsdRRCrtc *crtc)
{
    g_return_val_if_fail (crtc != NULL, NULL);
    
    return crtc->current_mode;
}

guint32
gsd_rr_crtc_get_id (GsdRRCrtc *crtc)
{
    g_return_val_if_fail (crtc != NULL, 0);
    
    return crtc->id;
}

gboolean
gsd_rr_crtc_can_drive_output (GsdRRCrtc   *crtc,
				GsdRROutput *output)
{
    int i;
    
    g_return_val_if_fail (crtc != NULL, FALSE);
    g_return_val_if_fail (output != NULL, FALSE);
    
    for (i = 0; crtc->possible_outputs[i] != NULL; ++i)
    {
	if (crtc->possible_outputs[i] == output)
	    return TRUE;
    }
    
    return FALSE;
}

/* FIXME: merge with get_mode()? */
/**
 * gsd_rr_crtc_get_position:
 * @crtc: a #GsdRRCrtc
 * @x: (out) (allow-none):
 * @y: (out) (allow-none):
 */
void
gsd_rr_crtc_get_position (GsdRRCrtc *crtc,
			    int         *x,
			    int         *y)
{
    g_return_if_fail (crtc != NULL);
    
    if (x)
	*x = crtc->x;
    
    if (y)
	*y = crtc->y;
}

/* FIXME: merge with get_mode()? */
GsdRRRotation
gsd_rr_crtc_get_current_rotation (GsdRRCrtc *crtc)
{
    g_assert(crtc != NULL);
    return crtc->current_rotation;
}

GsdRRRotation
gsd_rr_crtc_get_rotations (GsdRRCrtc *crtc)
{
    g_assert(crtc != NULL);
    return crtc->rotations;
}

gboolean
gsd_rr_crtc_supports_rotation (GsdRRCrtc *   crtc,
				 GsdRRRotation rotation)
{
    g_return_val_if_fail (crtc != NULL, FALSE);
    return (crtc->rotations & rotation);
}

static GsdRRCrtc *
crtc_new (ScreenInfo *info, RROutput id)
{
    GsdRRCrtc *crtc = g_slice_new0 (GsdRRCrtc);
    
    crtc->id = id;
    crtc->info = info;
    
    return crtc;
}

static GsdRRCrtc *
crtc_copy (const GsdRRCrtc *from)
{
    GsdRROutput **p_output;
    GPtrArray *array;
    GsdRRCrtc *to = g_slice_new0 (GsdRRCrtc);

    to->info = from->info;
    to->id = from->id;
    to->current_mode = from->current_mode;
    to->x = from->x;
    to->y = from->y;
    to->current_rotation = from->current_rotation;
    to->rotations = from->rotations;
    to->gamma_size = from->gamma_size;

    array = g_ptr_array_new ();
    for (p_output = from->current_outputs; *p_output != NULL; p_output++)
    {
        g_ptr_array_add (array, *p_output);
    }
    to->current_outputs = (GsdRROutput**) g_ptr_array_free (array, FALSE);

    array = g_ptr_array_new ();
    for (p_output = from->possible_outputs; *p_output != NULL; p_output++)
    {
        g_ptr_array_add (array, *p_output);
    }
    to->possible_outputs = (GsdRROutput**) g_ptr_array_free (array, FALSE);

    return to;
}

static gboolean
crtc_initialize (GsdRRCrtc        *crtc,
		 XRRScreenResources *res,
		 GError            **error)
{
    XRRCrtcInfo *info = XRRGetCrtcInfo (DISPLAY (crtc), res, crtc->id);
    GPtrArray *a;
    int i;
    
#if 0
    g_print ("CRTC %lx Timestamp: %u\n", crtc->id, (guint32)info->timestamp);
#endif
    
    if (!info)
    {
	/* FIXME: We need to reaquire the screen resources */
	/* FIXME: can we actually catch BadRRCrtc, and does it make sense to emit that? */

	/* Translators: CRTC is a CRT Controller (this is X terminology).
	 * It is *very* unlikely that you'll ever get this error, so it is
	 * only listed for completeness. */
	g_set_error (error, GSD_RR_ERROR, GSD_RR_ERROR_RANDR_ERROR,
		     _("could not get information about CRTC %d"),
		     (int) crtc->id);
	return FALSE;
    }
    
    /* GsdRRMode */
    crtc->current_mode = mode_by_id (crtc->info, info->mode);
    
    crtc->x = info->x;
    crtc->y = info->y;
    
    /* Current outputs */
    a = g_ptr_array_new ();
    for (i = 0; i < info->noutput; ++i)
    {
	GsdRROutput *output = gsd_rr_output_by_id (crtc->info, info->outputs[i]);
	
	if (output)
	    g_ptr_array_add (a, output);
    }
    g_ptr_array_add (a, NULL);
    crtc->current_outputs = (GsdRROutput **)g_ptr_array_free (a, FALSE);
    
    /* Possible outputs */
    a = g_ptr_array_new ();
    for (i = 0; i < info->npossible; ++i)
    {
	GsdRROutput *output = gsd_rr_output_by_id (crtc->info, info->possible[i]);
	
	if (output)
	    g_ptr_array_add (a, output);
    }
    g_ptr_array_add (a, NULL);
    crtc->possible_outputs = (GsdRROutput **)g_ptr_array_free (a, FALSE);
    
    /* Rotations */
    crtc->current_rotation = gsd_rr_rotation_from_xrotation (info->rotation);
    crtc->rotations = gsd_rr_rotation_from_xrotation (info->rotations);
    
    XRRFreeCrtcInfo (info);

    /* get an store gamma size */
    crtc->gamma_size = XRRGetCrtcGammaSize (DISPLAY (crtc), crtc->id);

    return TRUE;
}

static void
crtc_free (GsdRRCrtc *crtc)
{
    g_free (crtc->current_outputs);
    g_free (crtc->possible_outputs);
    g_slice_free (GsdRRCrtc, crtc);
}

/* GsdRRMode */
static GsdRRMode *
mode_new (ScreenInfo *info, RRMode id)
{
    GsdRRMode *mode = g_slice_new0 (GsdRRMode);
    
    mode->id = id;
    mode->info = info;
    
    return mode;
}

guint32
gsd_rr_mode_get_id (GsdRRMode *mode)
{
    g_return_val_if_fail (mode != NULL, 0);
    return mode->id;
}

guint
gsd_rr_mode_get_width (GsdRRMode *mode)
{
    g_return_val_if_fail (mode != NULL, 0);
    return mode->width;
}

int
gsd_rr_mode_get_freq (GsdRRMode *mode)
{
    g_return_val_if_fail (mode != NULL, 0);
    return (mode->freq) / 1000;
}

guint
gsd_rr_mode_get_height (GsdRRMode *mode)
{
    g_return_val_if_fail (mode != NULL, 0);
    return mode->height;
}

static void
mode_initialize (GsdRRMode *mode, XRRModeInfo *info)
{
    g_assert (mode != NULL);
    g_assert (info != NULL);
    
    mode->name = g_strdup (info->name);
    mode->width = info->width;
    mode->height = info->height;
    mode->freq = ((info->dotClock / (double)info->hTotal) / info->vTotal + 0.5) * 1000;
}

static GsdRRMode *
mode_copy (const GsdRRMode *from)
{
    GsdRRMode *to = g_slice_new0 (GsdRRMode);

    to->id = from->id;
    to->info = from->info;
    to->name = g_strdup (from->name);
    to->width = from->width;
    to->height = from->height;
    to->freq = from->freq;

    return to;
}

static void
mode_free (GsdRRMode *mode)
{
    g_free (mode->name);
    g_slice_free (GsdRRMode, mode);
}

void
gsd_rr_crtc_set_gamma (GsdRRCrtc *crtc, int size,
			 unsigned short *red,
			 unsigned short *green,
			 unsigned short *blue)
{
    int copy_size;
    XRRCrtcGamma *gamma;

    g_return_if_fail (crtc != NULL);
    g_return_if_fail (red != NULL);
    g_return_if_fail (green != NULL);
    g_return_if_fail (blue != NULL);

    if (size != crtc->gamma_size)
	return;

    gamma = XRRAllocGamma (crtc->gamma_size);

    copy_size = crtc->gamma_size * sizeof (unsigned short);
    memcpy (gamma->red, red, copy_size);
    memcpy (gamma->green, green, copy_size);
    memcpy (gamma->blue, blue, copy_size);

    XRRSetCrtcGamma (DISPLAY (crtc), crtc->id, gamma);
    XRRFreeGamma (gamma);
}

gboolean
gsd_rr_crtc_get_gamma (GsdRRCrtc *crtc, int *size,
			 unsigned short **red, unsigned short **green,
			 unsigned short **blue)
{
    int copy_size;
    unsigned short *r, *g, *b;
    XRRCrtcGamma *gamma;

    g_return_val_if_fail (crtc != NULL, FALSE);

    gamma = XRRGetCrtcGamma (DISPLAY (crtc), crtc->id);
    if (!gamma)
	return FALSE;

    copy_size = crtc->gamma_size * sizeof (unsigned short);

    if (red) {
	r = g_new0 (unsigned short, crtc->gamma_size);
	memcpy (r, gamma->red, copy_size);
	*red = r;
    }

    if (green) {
	g = g_new0 (unsigned short, crtc->gamma_size);
	memcpy (g, gamma->green, copy_size);
	*green = g;
    }

    if (blue) {
	b = g_new0 (unsigned short, crtc->gamma_size);
	memcpy (b, gamma->blue, copy_size);
	*blue = b;
    }

    XRRFreeGamma (gamma);

    if (size)
	*size = crtc->gamma_size;

    return TRUE;
}

