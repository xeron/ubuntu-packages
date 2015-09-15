/* gnome-rr-output-info.c
 * -*- c-basic-offset: 4 -*-
 *
 * Copyright 2010 Giovanni Campagna
 *
 * This file is part of the Gnome Desktop Library.
 *
 * The Gnome Desktop Library is free software; you can redistribute it and/or
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
 * License along with the Gnome Desktop Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <config.h>

#include "gsd-rr-config.h"

#include "edid.h"
#include "gsd-rr-private.h"

G_DEFINE_TYPE (GsdRROutputInfo, gsd_rr_output_info, G_TYPE_OBJECT)

static void
gsd_rr_output_info_init (GsdRROutputInfo *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GSD_TYPE_RR_OUTPUT_INFO, GsdRROutputInfoPrivate);

    self->priv->name = NULL;
    self->priv->on = FALSE;
    self->priv->display_name = NULL;
}

static void
gsd_rr_output_info_finalize (GObject *gobject)
{
    GsdRROutputInfo *self = GSD_RR_OUTPUT_INFO (gobject);

    g_free (self->priv->name);
    g_free (self->priv->display_name);

    G_OBJECT_CLASS (gsd_rr_output_info_parent_class)->finalize (gobject);
}

static void
gsd_rr_output_info_class_init (GsdRROutputInfoClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (GsdRROutputInfoPrivate));

    gobject_class->finalize = gsd_rr_output_info_finalize;
}

/**
 * gsd_rr_output_info_get_name:
 *
 * Returns: (transfer none): the output name
 */
char *gsd_rr_output_info_get_name (GsdRROutputInfo *self)
{
    g_return_val_if_fail (GSD_IS_RR_OUTPUT_INFO (self), NULL);

    return self->priv->name;
}

/**
 * gsd_rr_output_info_is_active:
 *
 * Returns: whether there is a CRTC assigned to this output (i.e. a signal is being sent to it)
 */
gboolean gsd_rr_output_info_is_active (GsdRROutputInfo *self)
{
    g_return_val_if_fail (GSD_IS_RR_OUTPUT_INFO (self), FALSE);

    return self->priv->on;
}

void gsd_rr_output_info_set_active (GsdRROutputInfo *self, gboolean active)
{
    g_return_if_fail (GSD_IS_RR_OUTPUT_INFO (self));

    self->priv->on = active;
}

/**
 * gsd_rr_output_info_get_geometry:
 * @self: a #GsdRROutputInfo
 * @x: (out) (allow-none):
 * @y: (out) (allow-none):
 * @width: (out) (allow-none):
 * @height: (out) (allow-none):
 */
void gsd_rr_output_info_get_geometry (GsdRROutputInfo *self, int *x, int *y, int *width, int *height)
{
    g_return_if_fail (GSD_IS_RR_OUTPUT_INFO (self));

    if (x)
	*x = self->priv->x;
    if (y)
	*y = self->priv->y;
    if (width)
	*width = self->priv->width;
    if (height)
	*height = self->priv->height;
}

void gsd_rr_output_info_set_geometry (GsdRROutputInfo *self, int  x, int  y, int  width, int  height)
{
    g_return_if_fail (GSD_IS_RR_OUTPUT_INFO (self));

    self->priv->x = x;
    self->priv->y = y;
    self->priv->width = width;
    self->priv->height = height;
}

int gsd_rr_output_info_get_refresh_rate (GsdRROutputInfo *self)
{
    g_return_val_if_fail (GSD_IS_RR_OUTPUT_INFO (self), 0);

    return self->priv->rate;
}

void gsd_rr_output_info_set_refresh_rate (GsdRROutputInfo *self, int rate)
{
    g_return_if_fail (GSD_IS_RR_OUTPUT_INFO (self));

    self->priv->rate = rate;
}

GsdRRRotation gsd_rr_output_info_get_rotation (GsdRROutputInfo *self)
{
    g_return_val_if_fail (GSD_IS_RR_OUTPUT_INFO (self), GSD_RR_ROTATION_0);

    return self->priv->rotation;
}

void gsd_rr_output_info_set_rotation (GsdRROutputInfo *self, GsdRRRotation rotation)
{
    g_return_if_fail (GSD_IS_RR_OUTPUT_INFO (self));

    self->priv->rotation = rotation;
}

/**
 * gsd_rr_output_info_is_connected:
 *
 * Returns: whether the output is physically connected to a monitor
 */
gboolean gsd_rr_output_info_is_connected (GsdRROutputInfo *self)
{
    g_return_val_if_fail (GSD_IS_RR_OUTPUT_INFO (self), FALSE);

    return self->priv->connected;
}

/**
 * gsd_rr_output_info_get_vendor:
 * @self: a #GsdRROutputInfo
 * @vendor: (out caller-allocates) (array fixed-size=4):
 */
void gsd_rr_output_info_get_vendor (GsdRROutputInfo *self, gchar* vendor)
{
    g_return_if_fail (GSD_IS_RR_OUTPUT_INFO (self));
    g_return_if_fail (vendor != NULL);

    vendor[0] = self->priv->vendor[0];
    vendor[1] = self->priv->vendor[1];
    vendor[2] = self->priv->vendor[2];
    vendor[3] = self->priv->vendor[3];
}

guint gsd_rr_output_info_get_product (GsdRROutputInfo *self)
{
    g_return_val_if_fail (GSD_IS_RR_OUTPUT_INFO (self), 0);

    return self->priv->product;
}

guint gsd_rr_output_info_get_serial (GsdRROutputInfo *self)
{
    g_return_val_if_fail (GSD_IS_RR_OUTPUT_INFO (self), 0);

    return self->priv->serial;
}

double gsd_rr_output_info_get_aspect_ratio (GsdRROutputInfo *self)
{
    g_return_val_if_fail (GSD_IS_RR_OUTPUT_INFO (self), 0);

    return self->priv->aspect;
}

/**
 * gsd_rr_output_info_get_display_name:
 *
 * Returns: (transfer none): the display name of this output
 */
char *gsd_rr_output_info_get_display_name (GsdRROutputInfo *self)
{
    g_return_val_if_fail (GSD_IS_RR_OUTPUT_INFO (self), NULL);

    return self->priv->display_name;
}

gboolean gsd_rr_output_info_get_primary (GsdRROutputInfo *self)
{
    g_return_val_if_fail (GSD_IS_RR_OUTPUT_INFO (self), FALSE);

    return self->priv->primary;
}

void gsd_rr_output_info_set_primary (GsdRROutputInfo *self, gboolean primary)
{
    g_return_if_fail (GSD_IS_RR_OUTPUT_INFO (self));

    self->priv->primary = primary;
}

int gsd_rr_output_info_get_preferred_width (GsdRROutputInfo *self)
{
    g_return_val_if_fail (GSD_IS_RR_OUTPUT_INFO (self), 0);

    return self->priv->pref_width;
}

int gsd_rr_output_info_get_preferred_height (GsdRROutputInfo *self)
{
    g_return_val_if_fail (GSD_IS_RR_OUTPUT_INFO (self), 0);

    return self->priv->pref_height;
}
