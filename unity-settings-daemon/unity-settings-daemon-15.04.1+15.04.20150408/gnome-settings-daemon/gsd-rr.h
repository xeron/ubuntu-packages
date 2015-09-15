/* gnome-rr.h
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
#ifndef GSD_RR_H
#define GSD_RR_H

#include <glib.h>
#include <gdk/gdk.h>

typedef struct GsdRRScreenPrivate GsdRRScreenPrivate;
typedef struct GsdRROutput GsdRROutput;
typedef struct GsdRRCrtc GsdRRCrtc;
typedef struct GsdRRMode GsdRRMode;

typedef struct {
	GObject parent;

	GsdRRScreenPrivate* priv;
} GsdRRScreen;

typedef struct {
	GObjectClass parent_class;

        void (*changed)                (GsdRRScreen *screen);
        void (*output_connected)       (GsdRRScreen *screen, GsdRROutput *output);
        void (*output_disconnected)    (GsdRRScreen *screen, GsdRROutput *output);
} GsdRRScreenClass;

typedef enum
{
    GSD_RR_ROTATION_NEXT =	0,
    GSD_RR_ROTATION_0 =	(1 << 0),
    GSD_RR_ROTATION_90 =	(1 << 1),
    GSD_RR_ROTATION_180 =	(1 << 2),
    GSD_RR_ROTATION_270 =	(1 << 3),
    GSD_RR_REFLECT_X =	(1 << 4),
    GSD_RR_REFLECT_Y =	(1 << 5)
} GsdRRRotation;

typedef enum {
	GSD_RR_DPMS_ON,
	GSD_RR_DPMS_STANDBY,
	GSD_RR_DPMS_SUSPEND,
	GSD_RR_DPMS_OFF,
	GSD_RR_DPMS_DISABLED,
	GSD_RR_DPMS_UNKNOWN
} GsdRRDpmsMode;

/* Error codes */

#define GSD_RR_ERROR (gsd_rr_error_quark ())

GQuark gsd_rr_error_quark (void);

typedef enum {
    GSD_RR_ERROR_UNKNOWN,		/* generic "fail" */
    GSD_RR_ERROR_NO_RANDR_EXTENSION,	/* RANDR extension is not present */
    GSD_RR_ERROR_RANDR_ERROR,		/* generic/undescribed error from the underlying XRR API */
    GSD_RR_ERROR_BOUNDS_ERROR,	/* requested bounds of a CRTC are outside the maximum size */
    GSD_RR_ERROR_CRTC_ASSIGNMENT,	/* could not assign CRTCs to outputs */
    GSD_RR_ERROR_NO_MATCHING_CONFIG,	/* none of the saved configurations matched the current configuration */
    GSD_RR_ERROR_NO_DPMS_EXTENSION,	/* DPMS extension is not present */
} GsdRRError;

#define GSD_RR_CONNECTOR_TYPE_PANEL "Panel"  /* This is a laptop's built-in LCD */

#define GSD_TYPE_RR_SCREEN                  (gsd_rr_screen_get_type())
#define GSD_RR_SCREEN(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSD_TYPE_RR_SCREEN, GsdRRScreen))
#define GSD_IS_RR_SCREEN(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSD_TYPE_RR_SCREEN))
#define GSD_RR_SCREEN_CLASS(klass)          (G_TYPE_CHECK_CLASS_CAST ((klass), GSD_TYPE_RR_SCREEN, GsdRRScreenClass))
#define GSD_IS_RR_SCREEN_CLASS(klass)       (G_TYPE_CHECK_CLASS_TYPE ((klass), GSD_TYPE_RR_SCREEN))
#define GSD_RR_SCREEN_GET_CLASS(obj)        (G_TYPE_INSTANCE_GET_CLASS ((obj), GSD_TYPE_RR_SCREEN, GsdRRScreenClass))

#define GSD_TYPE_RR_OUTPUT (gsd_rr_output_get_type())
#define GSD_TYPE_RR_CRTC   (gsd_rr_crtc_get_type())
#define GSD_TYPE_RR_MODE   (gsd_rr_mode_get_type())

GType gsd_rr_screen_get_type (void);
GType gsd_rr_output_get_type (void);
GType gsd_rr_crtc_get_type (void);
GType gsd_rr_mode_get_type (void);

/* GsdRRScreen */
GsdRRScreen * gsd_rr_screen_new                (GdkScreen             *screen,
						    GError               **error);
GsdRROutput **gsd_rr_screen_list_outputs       (GsdRRScreen         *screen);
GsdRRCrtc **  gsd_rr_screen_list_crtcs         (GsdRRScreen         *screen);
GsdRRMode **  gsd_rr_screen_list_modes         (GsdRRScreen         *screen);
GsdRRMode **  gsd_rr_screen_list_clone_modes   (GsdRRScreen	  *screen);
void            gsd_rr_screen_set_size           (GsdRRScreen         *screen,
						    int                    width,
						    int                    height,
						    int                    mm_width,
						    int                    mm_height);
GsdRRCrtc *   gsd_rr_screen_get_crtc_by_id     (GsdRRScreen         *screen,
						    guint32                id);
gboolean        gsd_rr_screen_refresh            (GsdRRScreen         *screen,
						    GError               **error);
GsdRROutput * gsd_rr_screen_get_output_by_id   (GsdRRScreen         *screen,
						    guint32                id);
GsdRROutput * gsd_rr_screen_get_output_by_name (GsdRRScreen         *screen,
						    const char            *name);
void            gsd_rr_screen_get_ranges         (GsdRRScreen         *screen,
						    int                   *min_width,
						    int                   *max_width,
						    int                   *min_height,
						    int                   *max_height);
void            gsd_rr_screen_get_timestamps     (GsdRRScreen         *screen,
						    guint32               *change_timestamp_ret,
						    guint32               *config_timestamp_ret);

void            gsd_rr_screen_set_primary_output (GsdRRScreen         *screen,
                                                    GsdRROutput         *output);

GsdRRMode   **gsd_rr_screen_create_clone_modes (GsdRRScreen *screen);

gboolean        gsd_rr_screen_get_dpms_mode      (GsdRRScreen        *screen,
                                                    GsdRRDpmsMode       *mode,
                                                    GError               **error);
gboolean        gsd_rr_screen_set_dpms_mode      (GsdRRScreen         *screen,
                                                    GsdRRDpmsMode        mode,
                                                    GError              **error);

/* GsdRROutput */
guint32         gsd_rr_output_get_id             (GsdRROutput         *output);
const char *    gsd_rr_output_get_name           (GsdRROutput         *output);
const char *    gsd_rr_output_get_display_name   (GsdRROutput         *output);
gboolean        gsd_rr_output_is_connected       (GsdRROutput         *output);
int             gsd_rr_output_get_size_inches    (GsdRROutput         *output);
int             gsd_rr_output_get_width_mm       (GsdRROutput         *outout);
int             gsd_rr_output_get_height_mm      (GsdRROutput         *output);
const guint8 *  gsd_rr_output_get_edid_data      (GsdRROutput         *output,
                                                    gsize                 *size);
gboolean        gsd_rr_output_get_ids_from_edid  (GsdRROutput         *output,
                                                    char                 **vendor,
                                                    int                   *product,
                                                    int                   *serial);

gint            gsd_rr_output_get_backlight_min  (GsdRROutput         *output);
gint            gsd_rr_output_get_backlight_max  (GsdRROutput         *output);
gint            gsd_rr_output_get_backlight      (GsdRROutput         *output,
                                                    GError                **error);
gboolean        gsd_rr_output_set_backlight      (GsdRROutput         *output,
                                                    gint                   value,
                                                    GError                **error);

GsdRRCrtc **  gsd_rr_output_get_possible_crtcs (GsdRROutput         *output);
GsdRRMode *   gsd_rr_output_get_current_mode   (GsdRROutput         *output);
GsdRRCrtc *   gsd_rr_output_get_crtc           (GsdRROutput         *output);
const char *    gsd_rr_output_get_connector_type (GsdRROutput         *output);
gboolean        gsd_rr_output_is_laptop          (GsdRROutput         *output);
void            gsd_rr_output_get_position       (GsdRROutput         *output,
						    int                   *x,
						    int                   *y);
gboolean        gsd_rr_output_can_clone          (GsdRROutput         *output,
						    GsdRROutput         *clone);
GsdRRMode **  gsd_rr_output_list_modes         (GsdRROutput         *output);
GsdRRMode *   gsd_rr_output_get_preferred_mode (GsdRROutput         *output);
gboolean        gsd_rr_output_supports_mode      (GsdRROutput         *output,
						    GsdRRMode           *mode);
gboolean        gsd_rr_output_get_is_primary     (GsdRROutput         *output);

/* GsdRRMode */
guint32         gsd_rr_mode_get_id               (GsdRRMode           *mode);
guint           gsd_rr_mode_get_width            (GsdRRMode           *mode);
guint           gsd_rr_mode_get_height           (GsdRRMode           *mode);
int             gsd_rr_mode_get_freq             (GsdRRMode           *mode);

/* GsdRRCrtc */
guint32         gsd_rr_crtc_get_id               (GsdRRCrtc           *crtc);

gboolean        gsd_rr_crtc_set_config_with_time (GsdRRCrtc           *crtc,
						    guint32                timestamp,
						    int                    x,
						    int                    y,
						    GsdRRMode           *mode,
						    GsdRRRotation        rotation,
						    GsdRROutput        **outputs,
						    int                    n_outputs,
						    GError               **error);
gboolean        gsd_rr_crtc_can_drive_output     (GsdRRCrtc           *crtc,
						    GsdRROutput         *output);
GsdRRMode *   gsd_rr_crtc_get_current_mode     (GsdRRCrtc           *crtc);
void            gsd_rr_crtc_get_position         (GsdRRCrtc           *crtc,
						    int                   *x,
						    int                   *y);
GsdRRRotation gsd_rr_crtc_get_current_rotation (GsdRRCrtc           *crtc);
GsdRRRotation gsd_rr_crtc_get_rotations        (GsdRRCrtc           *crtc);
gboolean        gsd_rr_crtc_supports_rotation    (GsdRRCrtc           *crtc,
						    GsdRRRotation        rotation);

gboolean        gsd_rr_crtc_get_gamma            (GsdRRCrtc           *crtc,
						    int                   *size,
						    unsigned short       **red,
						    unsigned short       **green,
						    unsigned short       **blue);
void            gsd_rr_crtc_set_gamma            (GsdRRCrtc           *crtc,
						    int                    size,
						    unsigned short        *red,
						    unsigned short        *green,
						    unsigned short        *blue);
#endif /* GSD_RR_H */
