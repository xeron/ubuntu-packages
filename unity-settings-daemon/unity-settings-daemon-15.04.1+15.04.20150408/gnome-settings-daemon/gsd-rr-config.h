/* gnome-rr-config.h
 * -*- c-basic-offset: 4 -*-
 *
 * Copyright 2007, 2008, Red Hat, Inc.
 * Copyright 2010 Giovanni Campagna
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
#ifndef GSD_RR_CONFIG_H
#define GSD_RR_CONFIG_H

#include <glib.h>
#include <glib-object.h>
#include "gsd-rr.h"

typedef struct _GsdRROutputInfo GsdRROutputInfo;
typedef struct _GsdRROutputInfoClass GsdRROutputInfoClass;
typedef struct _GsdRROutputInfoPrivate GsdRROutputInfoPrivate;

struct _GsdRROutputInfo
{
    GObject parent;

    /*< private >*/
    GsdRROutputInfoPrivate *priv;
};

struct _GsdRROutputInfoClass
{
    GObjectClass parent_class;
};

#define GSD_TYPE_RR_OUTPUT_INFO                  (gsd_rr_output_info_get_type())
#define GSD_RR_OUTPUT_INFO(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSD_TYPE_RR_OUTPUT_INFO, GsdRROutputInfo))
#define GSD_IS_RR_OUTPUT_INFO(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSD_TYPE_RR_OUTPUT_INFO))
#define GSD_RR_OUTPUT_INFO_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GSD_TYPE_RR_OUTPUT_INFO, GsdRROutputInfoClass))
#define GSD_IS_RR_OUTPUT_INFO_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GSD_TYPE_RR_OUTPUT_INFO))
#define GSD_RR_OUTPUT_INFO_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GSD_TYPE_RR_OUTPUT_INFO, GsdRROutputInfoClass))

GType gsd_rr_output_info_get_type (void);

char *gsd_rr_output_info_get_name (GsdRROutputInfo *self);

gboolean gsd_rr_output_info_is_active  (GsdRROutputInfo *self);
void     gsd_rr_output_info_set_active (GsdRROutputInfo *self, gboolean active);

void gsd_rr_output_info_get_geometry (GsdRROutputInfo *self, int *x, int *y, int *width, int *height);
void gsd_rr_output_info_set_geometry (GsdRROutputInfo *self, int  x, int  y, int  width, int  height);

int  gsd_rr_output_info_get_refresh_rate (GsdRROutputInfo *self);
void gsd_rr_output_info_set_refresh_rate (GsdRROutputInfo *self, int rate);

GsdRRRotation gsd_rr_output_info_get_rotation (GsdRROutputInfo *self);
void            gsd_rr_output_info_set_rotation (GsdRROutputInfo *self, GsdRRRotation rotation);

gboolean gsd_rr_output_info_is_connected     (GsdRROutputInfo *self);
void     gsd_rr_output_info_get_vendor       (GsdRROutputInfo *self, gchar* vendor);
guint    gsd_rr_output_info_get_product      (GsdRROutputInfo *self);
guint    gsd_rr_output_info_get_serial       (GsdRROutputInfo *self);
double   gsd_rr_output_info_get_aspect_ratio (GsdRROutputInfo *self);
char    *gsd_rr_output_info_get_display_name (GsdRROutputInfo *self);

gboolean gsd_rr_output_info_get_primary (GsdRROutputInfo *self);
void     gsd_rr_output_info_set_primary (GsdRROutputInfo *self, gboolean primary);

int gsd_rr_output_info_get_preferred_width  (GsdRROutputInfo *self);
int gsd_rr_output_info_get_preferred_height (GsdRROutputInfo *self);

typedef struct _GsdRRConfig GsdRRConfig;
typedef struct _GsdRRConfigClass GsdRRConfigClass;
typedef struct _GsdRRConfigPrivate GsdRRConfigPrivate;

struct _GsdRRConfig
{
    GObject parent;

    /*< private >*/
    GsdRRConfigPrivate *priv;
};

struct _GsdRRConfigClass
{
    GObjectClass parent_class;
};

#define GSD_TYPE_RR_CONFIG                  (gsd_rr_config_get_type())
#define GSD_RR_CONFIG(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSD_TYPE_RR_CONFIG, GsdRRConfig))
#define GSD_IS_RR_CONFIG(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSD_TYPE_RR_CONFIG))
#define GSD_RR_CONFIG_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GSD_TYPE_RR_CONFIG, GsdRRConfigClass))
#define GSD_IS_RR_CONFIG_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GSD_TYPE_RR_CONFIG))
#define GSD_RR_CONFIG_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GSD_TYPE_RR_CONFIG, GsdRRConfigClass))

GType               gsd_rr_config_get_type     (void);

GsdRRConfig      *gsd_rr_config_new_current  (GsdRRScreen  *screen,
						  GError        **error);
GsdRRConfig      *gsd_rr_config_new_stored   (GsdRRScreen  *screen,
						  GError        **error);
gboolean                gsd_rr_config_load_current (GsdRRConfig  *self,
						      GError        **error);
gboolean                gsd_rr_config_load_filename (GsdRRConfig  *self,
						       const gchar    *filename,
						       GError        **error);
gboolean            gsd_rr_config_match        (GsdRRConfig  *config1,
						  GsdRRConfig  *config2);
gboolean            gsd_rr_config_equal	 (GsdRRConfig  *config1,
						  GsdRRConfig  *config2);
gboolean            gsd_rr_config_save         (GsdRRConfig  *configuration,
						  GError        **error);
void                gsd_rr_config_sanitize     (GsdRRConfig  *configuration);
gboolean            gsd_rr_config_ensure_primary (GsdRRConfig  *configuration);

gboolean	    gsd_rr_config_apply_with_time (GsdRRConfig  *configuration,
						     GsdRRScreen  *screen,
						     guint32         timestamp,
						     GError        **error);

gboolean            gsd_rr_config_apply_from_filename_with_time (GsdRRScreen  *screen,
								   const char     *filename,
								   guint32         timestamp,
								   GError        **error);

gboolean            gsd_rr_config_applicable   (GsdRRConfig  *configuration,
						  GsdRRScreen  *screen,
						  GError        **error);

gboolean            gsd_rr_config_get_clone    (GsdRRConfig  *configuration);
void                gsd_rr_config_set_clone    (GsdRRConfig  *configuration, gboolean clone);
GsdRROutputInfo **gsd_rr_config_get_outputs  (GsdRRConfig  *configuration);

char *gsd_rr_config_get_backup_filename (void);
char *gsd_rr_config_get_intended_filename (void);

#endif
