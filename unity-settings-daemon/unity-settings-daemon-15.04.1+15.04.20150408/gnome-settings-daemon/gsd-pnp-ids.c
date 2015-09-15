/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2011 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib-object.h>

#include "gsd-pnp-ids.h"

static void gsd_pnp_ids_finalize (GObject *object);

#define GSD_PNP_IDS_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GSD_TYPE_PNP_IDS, GsdPnpIdsPrivate))

struct _GsdPnpIdsPrivate
{
        gchar      *table_data;
        GHashTable *pnp_table;
};

static gpointer gsd_pnp_ids_object = NULL;

G_DEFINE_TYPE (GsdPnpIds, gsd_pnp_ids, G_TYPE_OBJECT)

typedef struct Vendor Vendor;
struct Vendor
{
    const char vendor_id[4];
    const char vendor_name[28];
};

/* This list of vendor codes derived from lshw
 *
 * http://ezix.org/project/wiki/HardwareLiSter
 *
 * Note: we now prefer to use data coming from hwdata (and shipped with
 * gnome-desktop). See
 * http://git.fedorahosted.org/git/?p=hwdata.git;a=blob_plain;f=pnp.ids;hb=HEAD
 * All contributions to the list of vendors should go there.
 */
static const struct Vendor vendors[] =
{
    { "AIC", "AG Neovo" },
    { "ACR", "Acer" },
    { "DEL", "DELL" },
    { "SAM", "SAMSUNG" },
    { "SNY", "SONY" },
    { "SEC", "Epson" },
    { "WAC", "Wacom" },
    { "NEC", "NEC" },
    { "CMO", "CMO" },        /* Chi Mei */
    { "BNQ", "BenQ" },

    { "ABP", "Advansys" },
    { "ACC", "Accton" },
    { "ACE", "Accton" },
    { "ADP", "Adaptec" },
    { "ADV", "AMD" },
    { "AIR", "AIR" },
    { "AMI", "AMI" },
    { "ASU", "ASUS" },
    { "ATI", "ATI" },
    { "ATK", "Allied Telesyn" },
    { "AZT", "Aztech" },
    { "BAN", "Banya" },
    { "BRI", "Boca Research" },
    { "BUS", "Buslogic" },
    { "CCI", "Cache Computers Inc." },
    { "CHA", "Chase" },
    { "CMD", "CMD Technology, Inc." },
    { "COG", "Cogent" },
    { "CPQ", "Compaq" },
    { "CRS", "Crescendo" },
    { "CSC", "Crystal" },
    { "CSI", "CSI" },
    { "CTL", "Creative Labs" },
    { "DBI", "Digi" },
    { "DEC", "Digital Equipment" },
    { "DBK", "Databook" },
    { "EGL", "Eagle Technology" },
    { "ELS", "ELSA" },
    { "ESS", "ESS" },
    { "FAR", "Farallon" },
    { "FDC", "Future Domain" },
    { "HWP", "Hewlett-Packard" },
    { "IBM", "IBM" },
    { "INT", "Intel" },
    { "ISA", "Iomega" },
    { "LEN", "Lenovo" },
    { "MDG", "Madge" },
    { "MDY", "Microdyne" },
    { "MET", "Metheus" },
    { "MIC", "Micronics" },
    { "MLX", "Mylex" },
    { "NVL", "Novell" },
    { "OLC", "Olicom" },
    { "PRO", "Proteon" },
    { "RII", "Racal" },
    { "RTL", "Realtek" },
    { "SCM", "SCM" },
    { "SKD", "SysKonnect" },
    { "SGI", "SGI" },
    { "SMC", "SMC" },
    { "SNI", "Siemens Nixdorf" },
    { "STL", "Stallion Technologies" },
    { "SUN", "Sun" },
    { "SUP", "SupraExpress" },
    { "SVE", "SVEC" },
    { "TCC", "Thomas-Conrad" },
    { "TCI", "Tulip" },
    { "TCM", "3Com" },
    { "TCO", "Thomas-Conrad" },
    { "TEC", "Tecmar" },
    { "TRU", "Truevision" },
    { "TOS", "Toshiba" },
    { "TYN", "Tyan" },
    { "UBI", "Ungermann-Bass" },
    { "USC", "UltraStor" },
    { "VDM", "Vadem" },
    { "VMI", "Vermont" },
    { "WDC", "Western Digital" },
    { "ZDS", "Zeos" },

    /* From http://faydoc.tripod.com/structures/01/0136.htm */
    { "ACT", "Targa" },
    { "ADI", "ADI" },
    { "AOC", "AOC Intl" },
    { "API", "Acer America" },
    { "APP", "Apple Computer" },
    { "ART", "ArtMedia" },
    { "AST", "AST Research" },
    { "CPL", "Compal" },
    { "CTX", "Chuntex Electronic Co." },
    { "DPC", "Delta Electronics" },
    { "DWE", "Daewoo" },
    { "ECS", "ELITEGROUP" },
    { "EIZ", "EIZO" },
    { "FCM", "Funai" },
    { "GSM", "LG Electronics" },
    { "GWY", "Gateway 2000" },
    { "HEI", "Hyundai" },
    { "HIT", "Hitachi" },
    { "HSL", "Hansol" },
    { "HTC", "Hitachi" },
    { "ICL", "Fujitsu ICL" },
    { "IVM", "Idek Iiyama" },
    { "KFC", "KFC Computek" },
    { "LKM", "ADLAS" },
    { "LNK", "LINK Tech" },
    { "LTN", "Lite-On" },
    { "MAG", "MAG InnoVision" },
    { "MAX", "Maxdata" },
    { "MEI", "Panasonic" },
    { "MEL", "Mitsubishi" },
    { "MIR", "miro" },
    { "MTC", "MITAC" },
    { "NAN", "NANAO" },
    { "NEC", "NEC Tech" },
    { "NOK", "Nokia" },
    { "OQI", "OPTIQUEST" },
    { "PBN", "Packard Bell" },
    { "PGS", "Princeton" },
    { "PHL", "Philips" },
    { "REL", "Relisys" },
    { "SDI", "Samtron" },
    { "SMI", "Smile" },
    { "SPT", "Sceptre" },
    { "SRC", "Shamrock Technology" },
    { "STP", "Sceptre" },
    { "TAT", "Tatung" },
    { "TRL", "Royal Information Company" },
    { "TSB", "Toshiba, Inc." },
    { "UNM", "Unisys" },
    { "VSC", "ViewSonic" },
    { "WTC", "Wen Tech" },
    { "ZCM", "Zenith Data Systems" },

    { "???", "Unknown" },
};

static gboolean
gsd_pnp_ids_load (GsdPnpIds *pnp_ids, GError **error)
{
        gchar *retval = NULL;
        GsdPnpIdsPrivate *priv = pnp_ids->priv;
        guint i;

        /* load the contents */
        g_debug ("loading: %s", PNP_IDS);
        if (g_file_get_contents (PNP_IDS, &priv->table_data, NULL, error) == FALSE)
                return FALSE;

        /* parse into lines */
        retval = priv->table_data;
        for (i = 0; priv->table_data[i] != '\0'; i++) {

                /* ignore */
                if (priv->table_data[i] != '\n')
                        continue;

                /* convert newline to NULL */
                priv->table_data[i] = '\0';

                /* the ID to text is a fixed offset */
                if (retval[0] && retval[1] && retval[2] && retval[3] == '\t' && retval[4]) {
                        retval[3] = '\0';
                        g_hash_table_insert (priv->pnp_table,
                                             retval,
                                             retval+4);
                        retval = &priv->table_data[i+1];
                }
        }

        g_debug ("Added %i items to the vendor hashtable", i);

        return TRUE;
}

static const char *
find_vendor (const char *pnp_id)
{
        guint i;

        for (i = 0; i < G_N_ELEMENTS (vendors); i++) {
                if (g_strcmp0 (vendors[i].vendor_id, pnp_id) == 0)
                        return vendors[i].vendor_name;
        }

        return NULL;
}

/**
 * gsd_pnp_ids_get_pnp_id:
 * @pnp_ids: a #GsdPnpIds object
 * @pnp_id: the PNP ID to look for
 *
 * Find the full manufacturer name for the given PNP ID.
 *
 * Returns: (transfer full): a new string representing the manufacturer name,
 * or %NULL when not found.
 */
gchar *
gsd_pnp_ids_get_pnp_id (GsdPnpIds *pnp_ids, const gchar *pnp_id)
{
        GsdPnpIdsPrivate *priv = pnp_ids->priv;
        const char *found;
        GError *error = NULL;
        guint size;

        g_return_val_if_fail (GSD_IS_PNP_IDS (pnp_ids), NULL);
        g_return_val_if_fail (pnp_id != NULL, NULL);

        /* if table is empty, try to load it */
        size = g_hash_table_size (priv->pnp_table);
        if (size == 0) {
                if (gsd_pnp_ids_load (pnp_ids, &error) == FALSE) {
                        g_warning ("Failed to load PNP ids: %s", error->message);
                        g_error_free (error);
                        return NULL;
                }
        }

        /* look this up in the table */
        found = g_hash_table_lookup (priv->pnp_table, pnp_id);
        if (found == NULL) {
                found = find_vendor (pnp_id);
                if (found == NULL)
                        return NULL;
        }

        return g_strdup (found);
}

static void
gsd_pnp_ids_class_init (GsdPnpIdsClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = gsd_pnp_ids_finalize;
        g_type_class_add_private (klass, sizeof (GsdPnpIdsPrivate));
}

static void
gsd_pnp_ids_init (GsdPnpIds *pnp_ids)
{
        pnp_ids->priv = GSD_PNP_IDS_GET_PRIVATE (pnp_ids);

        /* we don't keep malloc'd data in the hash; instead we read it
         * out into priv->table_data and then link to it in the hash */
        pnp_ids->priv->pnp_table = g_hash_table_new_full (g_str_hash,
                                                         g_str_equal,
                                                         NULL,
                                                         NULL);
}

static void
gsd_pnp_ids_finalize (GObject *object)
{
        GsdPnpIds *pnp_ids = GSD_PNP_IDS (object);
        GsdPnpIdsPrivate *priv = pnp_ids->priv;

        g_free (priv->table_data);
        g_hash_table_unref (priv->pnp_table);

        G_OBJECT_CLASS (gsd_pnp_ids_parent_class)->finalize (object);
}

/**
 * gsd_pnp_ids_new:
 *
 * Returns a reference to a #GsdPnpIds object, or creates
 * a new one if none have been created.
 *
 * Returns: (transfer full): a #GsdPnpIds object.
 */
GsdPnpIds *
gsd_pnp_ids_new (void)
{
        if (gsd_pnp_ids_object != NULL) {
                g_object_ref (gsd_pnp_ids_object);
        } else {
                gsd_pnp_ids_object = g_object_new (GSD_TYPE_PNP_IDS, NULL);
                g_object_add_weak_pointer (gsd_pnp_ids_object, &gsd_pnp_ids_object);
        }
        return GSD_PNP_IDS (gsd_pnp_ids_object);
}

