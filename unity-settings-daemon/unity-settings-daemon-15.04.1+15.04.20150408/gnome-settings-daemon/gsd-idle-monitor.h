/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/*
 * Copyright 2013 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef gsd_idle_monitor_H
#define gsd_idle_monitor_H

#include <glib-object.h>

#define GSD_TYPE_IDLE_MONITOR            (gsd_idle_monitor_get_type ())
#define gsd_idle_monitor(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSD_TYPE_IDLE_MONITOR, GsdIdleMonitor))
#define gsd_idle_monitor_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  GSD_TYPE_IDLE_MONITOR, GsdIdleMonitorClass))
#define GSD_IS_IDLE_MONITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSD_TYPE_IDLE_MONITOR))
#define GSD_IS_IDLE_MONITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  GSD_TYPE_IDLE_MONITOR))
#define gsd_idle_monitor_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  GSD_TYPE_IDLE_MONITOR, GsdIdleMonitorClass))

typedef struct _GsdIdleMonitor        GsdIdleMonitor;
typedef struct _GsdIdleMonitorClass   GsdIdleMonitorClass;

GType gsd_idle_monitor_get_type (void);

typedef void (*GsdIdleMonitorWatchFunc) (GsdIdleMonitor *monitor,
                                          guint            watch_id,
                                          gpointer         user_data);

GsdIdleMonitor *gsd_idle_monitor_get_core (void);
GsdIdleMonitor *gsd_idle_monitor_get_for_device (int device_id);

guint         gsd_idle_monitor_add_idle_watch        (GsdIdleMonitor          *monitor,
						       guint64                   interval_msec,
						       GsdIdleMonitorWatchFunc  callback,
						       gpointer                  user_data,
						       GDestroyNotify            notify);

guint         gsd_idle_monitor_add_user_active_watch (GsdIdleMonitor          *monitor,
						       GsdIdleMonitorWatchFunc  callback,
						       gpointer                  user_data,
						       GDestroyNotify            notify);

void          gsd_idle_monitor_remove_watch          (GsdIdleMonitor          *monitor,
						       guint                     id);
gint64        gsd_idle_monitor_get_idletime          (GsdIdleMonitor          *monitor);

#endif
