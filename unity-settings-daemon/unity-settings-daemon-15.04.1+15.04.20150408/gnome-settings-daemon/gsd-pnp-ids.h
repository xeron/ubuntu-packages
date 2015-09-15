/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009-2010 Richard Hughes <richard@hughsie.com>
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

#ifndef __GSD_PNP_IDS_H
#define __GSD_PNP_IDS_H

#include <glib-object.h>

G_BEGIN_DECLS

#define GSD_TYPE_PNP_IDS                 (gsd_pnp_ids_get_type ())
#define GSD_PNP_IDS(o)                   (G_TYPE_CHECK_INSTANCE_CAST ((o), GSD_TYPE_PNP_IDS, GsdPnpIds))
#define GSD_PNP_IDS_CLASS(k)             (G_TYPE_CHECK_CLASS_CAST((k), GSD_TYPE_PNP_IDS, GsdPnpIdsClass))
#define GSD_IS_PNP_IDS(o)                (G_TYPE_CHECK_INSTANCE_TYPE ((o), GSD_TYPE_PNP_IDS))
#define GSD_IS_PNP_IDS_CLASS(k)          (G_TYPE_CHECK_CLASS_TYPE ((k), GSD_TYPE_PNP_IDS))
#define GSD_PNP_IDS_GET_CLASS(o)         (G_TYPE_INSTANCE_GET_CLASS ((o), GSD_TYPE_PNP_IDS, GsdPnpIdsClass))
#define GSD_PNP_IDS_ERROR                (gsd_pnp_ids_error_quark ())

typedef struct _GsdPnpIdsPrivate        GsdPnpIdsPrivate;
typedef struct _GsdPnpIds               GsdPnpIds;
typedef struct _GsdPnpIdsClass          GsdPnpIdsClass;

struct _GsdPnpIds
{
         GObject          parent;
         GsdPnpIdsPrivate *priv;
};

struct _GsdPnpIdsClass
{
        GObjectClass    parent_class;
};

GType         gsd_pnp_ids_get_type                    (void);
GsdPnpIds     *gsd_pnp_ids_new                         (void);
gchar        *gsd_pnp_ids_get_pnp_id                  (GsdPnpIds   *pnp_ids,
                                                     const gchar *pnp_id);

G_END_DECLS

#endif /* __GSD_PNP_IDS_H */

