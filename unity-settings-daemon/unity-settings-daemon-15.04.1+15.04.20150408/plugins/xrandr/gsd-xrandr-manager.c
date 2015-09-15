/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2007, 2008 Red Hat, Inc
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

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <libupower-glib/upower.h>
#include <X11/Xatom.h>

#ifdef HAVE_WACOM
#include <libwacom/libwacom.h>
#endif /* HAVE_WACOM */

#include "gsd-enums.h"
#include "gsd-input-helper.h"
#include "gnome-settings-plugin.h"
#include "gnome-settings-profile.h"
#include "gnome-settings-session.h"
#include "gsd-xrandr-manager.h"
#include "gsd-rr-config.h"
#include "gsd-rr.h"
#include "gsd-pnp-ids.h"

#define GSD_XRANDR_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GSD_TYPE_XRANDR_MANAGER, GsdXrandrManagerPrivate))

#define CONF_SCHEMA "org.gnome.settings-daemon.plugins.xrandr"
#define CONF_KEY_DEFAULT_MONITORS_SETUP   "default-monitors-setup"
#define CONF_KEY_DEFAULT_CONFIGURATION_FILE   "default-configuration-file"

/* Number of seconds that the confirmation dialog will last before it resets the
 * RANDR configuration to its old state.
 */
#define CONFIRMATION_DIALOG_SECONDS 30

/* name of the icon files (usd-xrandr.svg, etc.) */
#define GSD_XRANDR_ICON_NAME "usd-xrandr"

#define GSD_XRANDR_DBUS_NAME GSD_DBUS_NAME ".XRANDR"
#define GSD_XRANDR_DBUS_PATH GSD_DBUS_PATH "/XRANDR"

static const gchar introspection_xml[] =
"<node name='/org/gnome/SettingsDaemon/XRANDR'>"
"  <interface name='org.gnome.SettingsDaemon.XRANDR_2'>"
"    <annotation name='org.freedesktop.DBus.GLib.CSymbol' value='gsd_xrandr_manager_2'/>"
"    <method name='ApplyConfiguration'>"
"      <!-- transient-parent window for the confirmation dialog; use 0"
"      for no parent -->"
"      <arg name='parent_window_id' type='x' direction='in'/>"
""
"      <!-- Timestamp used to present the confirmation dialog and (in"
"      the future) for the RANDR calls themselves -->"
"      <arg name='timestamp' type='x' direction='in'/>"
"    </method>"
"    <method name='VideoModeSwitch'>"
"       <!-- Timestamp for the RANDR call itself -->"
"       <arg name='timestamp' type='x' direction='in'/>"
"    </method>"
"    <method name='Rotate'>"
"       <!-- Timestamp for the RANDR call itself -->"
"       <arg name='timestamp' type='x' direction='in'/>"
"    </method>"
"    <method name='RotateTo'>"
"       <arg name='rotation' type='i' direction='in'/>"
"       <!-- Timestamp for the RANDR call itself -->"
"       <arg name='timestamp' type='x' direction='in'/>"
"    </method>"
"  </interface>"
"</node>";

struct GsdXrandrManagerPrivate {
        GsdRRScreen *rw_screen;
        gboolean running;

        UpClient *upower_client;
        gboolean laptop_lid_is_closed;

        GSettings       *settings;
        GDBusNodeInfo   *introspection_data;
        guint            name_id;
        GDBusConnection *connection;
        GCancellable    *bus_cancellable;

        /* fn-F7 status */
        int             current_fn_f7_config;             /* -1 if no configs */
        GsdRRConfig **fn_f7_configs;  /* NULL terminated, NULL if there are no configs */

        /* Last time at which we got a "screen got reconfigured" event; see on_randr_event() */
        guint32 last_config_timestamp;

#ifdef HAVE_WACOM
        WacomDeviceDatabase *wacom_db;
#endif /* HAVE_WACOM */

        int main_touchscreen_id;
        gchar *main_touchscreen_name;
};

static const GsdRRRotation possible_rotations[] = {
        GSD_RR_ROTATION_0,
        GSD_RR_ROTATION_90,
        GSD_RR_ROTATION_180,
        GSD_RR_ROTATION_270
        /* We don't allow REFLECT_X or REFLECT_Y for now, as gnome-display-properties doesn't allow them, either */
};

static void     gsd_xrandr_manager_class_init  (GsdXrandrManagerClass *klass);
static void     gsd_xrandr_manager_init        (GsdXrandrManager      *xrandr_manager);
static void     gsd_xrandr_manager_finalize    (GObject             *object);

static void error_message (GsdXrandrManager *mgr, const char *primary_text, GError *error_to_display, const char *secondary_text);

static void get_allowed_rotations_for_output (GsdRRConfig *config,
                                              GsdRRScreen *rr_screen,
                                              GsdRROutputInfo *output,
                                              int *out_num_rotations,
                                              GsdRRRotation *out_rotations);
static void handle_fn_f7 (GsdXrandrManager *mgr, guint32 timestamp);
static void handle_rotate_windows (GsdXrandrManager *mgr, GsdRRRotation rotation, guint32 timestamp);

G_DEFINE_TYPE (GsdXrandrManager, gsd_xrandr_manager, G_TYPE_OBJECT)

static gpointer manager_object = NULL;

static FILE *log_file;

static GsdRROutput * input_info_find_size_match (GsdXrandrManager *manager, GsdRRScreen *rr_screen);
static int map_touch_to_output (GsdRRScreen *screen, int device_id, GsdRROutputInfo *output);
static void do_touchscreen_mapping (GsdXrandrManager *manager);

static void
log_open (void)
{
        char *toggle_filename;
        char *log_filename;
        struct stat st;

        if (log_file)
                return;

        toggle_filename = g_build_filename (g_get_home_dir (), "gsd-debug-randr", NULL);
        log_filename = g_build_filename (g_get_home_dir (), "gsd-debug-randr.log", NULL);

        if (stat (toggle_filename, &st) != 0)
                goto out;

        log_file = fopen (log_filename, "a");

        if (log_file && ftell (log_file) == 0)
                fprintf (log_file, "To keep this log from being created, please rm ~/gsd-debug-randr\n");

out:
        g_free (toggle_filename);
        g_free (log_filename);
}

static void
log_close (void)
{
        if (log_file) {
                fclose (log_file);
                log_file = NULL;
        }
}

static void
log_msg (const char *format, ...)
{
        if (log_file) {
                va_list args;

                va_start (args, format);
                vfprintf (log_file, format, args);
                va_end (args);
        }
}

static void
log_output (GsdRROutputInfo *output)
{
        gchar *name = gsd_rr_output_info_get_name (output);
        gchar *display_name = gsd_rr_output_info_get_display_name (output);

        log_msg ("        %s: ", name ? name : "unknown");

        if (gsd_rr_output_info_is_connected (output)) {
                if (gsd_rr_output_info_is_active (output)) {
                        int x, y, width, height;
                        gsd_rr_output_info_get_geometry (output, &x, &y, &width, &height);
                        log_msg ("%dx%d@%d +%d+%d",
                                 width,
                                 height,
                                 gsd_rr_output_info_get_refresh_rate (output),
                                 x,
                                 y);
                } else
                        log_msg ("off");
        } else
                log_msg ("disconnected");

        if (display_name)
                log_msg (" (%s)", display_name);

        if (gsd_rr_output_info_get_primary (output))
                log_msg (" (primary output)");

        log_msg ("\n");
}

static void
log_configuration (GsdRRConfig *config)
{
        int i;
        GsdRROutputInfo **outputs = gsd_rr_config_get_outputs (config);

        log_msg ("        cloned: %s\n", gsd_rr_config_get_clone (config) ? "yes" : "no");

        for (i = 0; outputs[i] != NULL; i++)
                log_output (outputs[i]);

        if (i == 0)
                log_msg ("        no outputs!\n");
}

static char
timestamp_relationship (guint32 a, guint32 b)
{
        if (a < b)
                return '<';
        else if (a > b)
                return '>';
        else
                return '=';
}

static void
log_screen (GsdRRScreen *screen)
{
        GsdRRConfig *config;
        int min_w, min_h, max_w, max_h;
        guint32 change_timestamp, config_timestamp;

        if (!log_file)
                return;

        config = gsd_rr_config_new_current (screen, NULL);

        gsd_rr_screen_get_ranges (screen, &min_w, &max_w, &min_h, &max_h);
        gsd_rr_screen_get_timestamps (screen, &change_timestamp, &config_timestamp);

        log_msg ("        Screen min(%d, %d), max(%d, %d), change=%u %c config=%u\n",
                 min_w, min_h,
                 max_w, max_h,
                 change_timestamp,
                 timestamp_relationship (change_timestamp, config_timestamp),
                 config_timestamp);

        log_configuration (config);
        g_object_unref (config);
}

static void
log_configurations (GsdRRConfig **configs)
{
        int i;

        if (!configs) {
                log_msg ("    No configurations\n");
                return;
        }

        for (i = 0; configs[i]; i++) {
                log_msg ("    Configuration %d\n", i);
                log_configuration (configs[i]);
        }
}

static void
show_timestamps_dialog (GsdXrandrManager *manager, const char *msg)
{
#if 1
        return;
#else
        struct GsdXrandrManagerPrivate *priv = manager->priv;
        GtkWidget *dialog;
        guint32 change_timestamp, config_timestamp;
        static int serial;

        gsd_rr_screen_get_timestamps (priv->rw_screen, &change_timestamp, &config_timestamp);

        dialog = gtk_message_dialog_new (NULL,
                                         0,
                                         GTK_MESSAGE_INFO,
                                         GTK_BUTTONS_CLOSE,
                                         "RANDR timestamps (%d):\n%s\nchange: %u\nconfig: %u",
                                         serial++,
                                         msg,
                                         change_timestamp,
                                         config_timestamp);
        g_signal_connect (dialog, "response",
                          G_CALLBACK (gtk_widget_destroy), NULL);
        gtk_widget_show (dialog);
#endif
}

static void
print_output (GsdRROutputInfo *info)
{
        int x, y, width, height;

        g_debug ("  Output: %s attached to %s", gsd_rr_output_info_get_display_name (info), gsd_rr_output_info_get_name (info));
        g_debug ("     status: %s", gsd_rr_output_info_is_active (info) ? "on" : "off");

        gsd_rr_output_info_get_geometry (info, &x, &y, &width, &height);
        g_debug ("     width: %d", width);
        g_debug ("     height: %d", height);
        g_debug ("     rate: %d", gsd_rr_output_info_get_refresh_rate (info));
        g_debug ("     primary: %s", gsd_rr_output_info_get_primary (info) ? "true" : "false");
        g_debug ("     position: %d %d", x, y);
}

static void
print_configuration (GsdRRConfig *config, const char *header)
{
        int i;
        GsdRROutputInfo **outputs;

        g_debug ("=== %s Configuration ===", header);
        if (!config) {
                g_debug ("  none");
                return;
        }

        g_debug ("  Clone: %s", gsd_rr_config_get_clone (config) ? "true" : "false");

        outputs = gsd_rr_config_get_outputs (config);
        for (i = 0; outputs[i] != NULL; ++i)
                print_output (outputs[i]);
}

static gboolean
is_laptop (GsdRRScreen *screen, GsdRROutputInfo *output)
{
        GsdRROutput *rr_output;

        rr_output = gsd_rr_screen_get_output_by_name (screen, gsd_rr_output_info_get_name (output));

        return gsd_rr_output_is_laptop (rr_output);
}

static GsdRROutputInfo *
get_laptop_output_info (GsdRRScreen *screen, GsdRRConfig *config)
{
        int i;
        GsdRROutputInfo **outputs = gsd_rr_config_get_outputs (config);

        for (i = 0; outputs[i] != NULL; i++) {
                if (is_laptop (screen, outputs[i]))
                        return outputs[i];
        }

        return NULL;
}

static gboolean
non_laptop_outputs_are_active (GsdRRConfig *config, GsdRROutputInfo *laptop_info)
{
        GsdRROutputInfo **outputs;
        int i;

        outputs = gsd_rr_config_get_outputs (config);
        for (i = 0; outputs[i] != NULL; i++) {
                if (outputs[i] == laptop_info)
                        continue;

                if (gsd_rr_output_info_is_active (outputs[i]))
                        return TRUE;
        }

        return FALSE;
}

static void
turn_off_laptop_display_in_configuration (GsdRRScreen *screen, GsdRRConfig *config)
{
        GsdRROutputInfo *laptop_info;

        laptop_info = get_laptop_output_info (screen, config);
        if (laptop_info) {
                /* Turn off the laptop's screen only if other displays are on.  This is to avoid an all-black-screens scenario. */
                if (non_laptop_outputs_are_active (config, laptop_info))
                        gsd_rr_output_info_set_active (laptop_info, FALSE);
        }

        /* Adjust the offsets of outputs so they start at (0, 0) */
        gsd_rr_config_sanitize (config);
}

/* This function effectively centralizes the use of gsd_rr_config_apply_from_filename_with_time().
 *
 * Optionally filters out GSD_RR_ERROR_NO_MATCHING_CONFIG from the matching
 * process(), since that is not usually an error.
 */
static gboolean
apply_configuration_from_filename (GsdXrandrManager *manager,
                                   const char       *filename,
                                   gboolean          no_matching_config_is_an_error,
                                   guint32           timestamp,
                                   GError          **error)
{
        struct GsdXrandrManagerPrivate *priv = manager->priv;
        GsdRRConfig *config;
        GError *my_error;
        gboolean success;
        char *str;

        str = g_strdup_printf ("Applying %s with timestamp %d", filename, timestamp);
        show_timestamps_dialog (manager, str);
        g_free (str);

        my_error = NULL;

        config = g_object_new (GSD_TYPE_RR_CONFIG, "screen", priv->rw_screen, NULL);
        if (!gsd_rr_config_load_filename (config, filename, &my_error)) {
                g_object_unref (config);

                if (g_error_matches (my_error, GSD_RR_ERROR, GSD_RR_ERROR_NO_MATCHING_CONFIG)) {
                        if (no_matching_config_is_an_error) {
                                g_propagate_error (error, my_error);
                                return FALSE;
                        } else {
                                /* This is not an error; the user probably changed his monitors
                                 * and so they don't match any of the stored configurations.
                                 */
                                g_error_free (my_error);
                                return TRUE;
                        }
                } else {
                        g_propagate_error (error, my_error);
                        return FALSE;
                }
        }

        if (up_client_get_lid_is_closed (priv->upower_client))
                turn_off_laptop_display_in_configuration (priv->rw_screen, config);

        gsd_rr_config_ensure_primary (config);
	success = gsd_rr_config_apply_with_time (config, priv->rw_screen, timestamp, error);

        g_object_unref (config);

        return success;
}

/* This function centralizes the use of gsd_rr_config_apply_with_time().
 *
 * Applies a configuration and displays an error message if an error happens.
 * We just return whether setting the configuration succeeded.
 */
static gboolean
apply_configuration (GsdXrandrManager *manager, GsdRRConfig *config, guint32 timestamp, gboolean show_error, gboolean save_configuration)
{
        GsdXrandrManagerPrivate *priv = manager->priv;
        GError *error;
        gboolean success;

        gsd_rr_config_ensure_primary (config);

        print_configuration (config, "Applying Configuration");

        error = NULL;
        success = gsd_rr_config_apply_with_time (config, priv->rw_screen, timestamp, &error);
        if (success) {
                if (save_configuration)
                        gsd_rr_config_save (config, NULL); /* NULL-GError - there's not much we can do if this fails */
        } else {
                log_msg ("Could not switch to the following configuration (timestamp %u): %s\n", timestamp, error->message);
                log_configuration (config);
                if (show_error)
                        error_message (manager, _("Could not switch the monitor configuration"), error, NULL);
                g_error_free (error);
        }

        return success;
}

static void
restore_backup_configuration_without_messages (const char *backup_filename, const char *intended_filename)
{
        backup_filename = gsd_rr_config_get_backup_filename ();
        rename (backup_filename, intended_filename);
}

static void
restore_backup_configuration (GsdXrandrManager *manager, const char *backup_filename, const char *intended_filename, guint32 timestamp)
{
        int saved_errno;

        if (rename (backup_filename, intended_filename) == 0) {
                GError *error;

                error = NULL;
                if (!apply_configuration_from_filename (manager, intended_filename, FALSE, timestamp, &error)) {
                        error_message (manager, _("Could not restore the display's configuration"), error, NULL);

                        if (error)
                                g_error_free (error);
                }

                return;
        }

        saved_errno = errno;

        /* ENOENT means the original file didn't exist.  That is *not* an error;
         * the backup was not created because there wasn't even an original
         * monitors.xml (such as on a first-time login).  Note that *here* there
         * is a "didn't work" monitors.xml, so we must delete that one.
         */
        if (saved_errno == ENOENT)
                unlink (intended_filename);
        else {
                char *msg;

                msg = g_strdup_printf ("Could not rename %s to %s: %s",
                                       backup_filename, intended_filename,
                                       g_strerror (saved_errno));
                error_message (manager,
                               _("Could not restore the display's configuration from a backup"),
                               NULL,
                               msg);
                g_free (msg);
        }

        unlink (backup_filename);
}

typedef struct {
        GsdXrandrManager *manager;
        GtkWidget *dialog;

        int countdown;
        int response_id;
} TimeoutDialog;

static void
print_countdown_text (TimeoutDialog *timeout)
{
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (timeout->dialog),
                                                  ngettext ("The display will be reset to its previous configuration in %d second",
                                                            "The display will be reset to its previous configuration in %d seconds",
                                                            timeout->countdown),
                                                  timeout->countdown);
}

static gboolean
timeout_cb (gpointer data)
{
        TimeoutDialog *timeout = data;

        timeout->countdown--;

        if (timeout->countdown == 0) {
                timeout->response_id = GTK_RESPONSE_CANCEL;
                gtk_main_quit ();
        } else {
                print_countdown_text (timeout);
        }

        return TRUE;
}

static void
timeout_response_cb (GtkDialog *dialog, int response_id, gpointer data)
{
        TimeoutDialog *timeout = data;

        if (response_id == GTK_RESPONSE_DELETE_EVENT) {
                /* The user closed the dialog or pressed ESC, revert */
                timeout->response_id = GTK_RESPONSE_CANCEL;
        } else
                timeout->response_id = response_id;

        gtk_main_quit ();
}

static gboolean
user_says_things_are_ok (GsdXrandrManager *manager, GdkWindow *parent_window)
{
        TimeoutDialog timeout;
        guint timeout_id;

        timeout.manager = manager;

        timeout.dialog = gtk_message_dialog_new (NULL,
                                                 GTK_DIALOG_MODAL,
                                                 GTK_MESSAGE_QUESTION,
                                                 GTK_BUTTONS_NONE,
                                                 _("Does the display look OK?"));

        timeout.countdown = CONFIRMATION_DIALOG_SECONDS;

        print_countdown_text (&timeout);

        gtk_window_set_icon_name (GTK_WINDOW (timeout.dialog), "preferences-desktop-display");
        gtk_dialog_add_button (GTK_DIALOG (timeout.dialog), _("_Restore Previous Configuration"), GTK_RESPONSE_CANCEL);
        gtk_dialog_add_button (GTK_DIALOG (timeout.dialog), _("_Keep This Configuration"), GTK_RESPONSE_ACCEPT);
        gtk_dialog_set_default_response (GTK_DIALOG (timeout.dialog), GTK_RESPONSE_ACCEPT); /* ah, the optimism */

        g_signal_connect (timeout.dialog, "response",
                          G_CALLBACK (timeout_response_cb),
                          &timeout);

        gtk_widget_realize (timeout.dialog);

        if (parent_window)
                gdk_window_set_transient_for (gtk_widget_get_window (timeout.dialog), parent_window);

        gtk_widget_show_all (timeout.dialog);
        /* We don't use g_timeout_add_seconds() since we actually care that the user sees "real" second ticks in the dialog */
        timeout_id = g_timeout_add (1000,
                                    timeout_cb,
                                    &timeout);
        gtk_main ();

        gtk_widget_destroy (timeout.dialog);
        g_source_remove (timeout_id);

        if (timeout.response_id == GTK_RESPONSE_ACCEPT)
                return TRUE;
        else
                return FALSE;
}

struct confirmation {
        GsdXrandrManager *manager;
        GdkWindow *parent_window;
        guint32 timestamp;
};

static gboolean
confirm_with_user_idle_cb (gpointer data)
{
        struct confirmation *confirmation = data;
        char *backup_filename;
        char *intended_filename;

        backup_filename = gsd_rr_config_get_backup_filename ();
        intended_filename = gsd_rr_config_get_intended_filename ();

        if (user_says_things_are_ok (confirmation->manager, confirmation->parent_window))
                unlink (backup_filename);
        else
                restore_backup_configuration (confirmation->manager, backup_filename, intended_filename, confirmation->timestamp);

        g_free (confirmation);

        return FALSE;
}

static void
queue_confirmation_by_user (GsdXrandrManager *manager, GdkWindow *parent_window, guint32 timestamp)
{
        struct confirmation *confirmation;

        confirmation = g_new (struct confirmation, 1);
        confirmation->manager = manager;
        confirmation->parent_window = parent_window;
        confirmation->timestamp = timestamp;

        g_idle_add (confirm_with_user_idle_cb, confirmation);
}

static gboolean
try_to_apply_intended_configuration (GsdXrandrManager *manager, GdkWindow *parent_window, guint32 timestamp, GError **error)
{
        char *backup_filename;
        char *intended_filename;
        gboolean result;

        /* Try to apply the intended configuration */

        backup_filename = gsd_rr_config_get_backup_filename ();
        intended_filename = gsd_rr_config_get_intended_filename ();

        result = apply_configuration_from_filename (manager, intended_filename, FALSE, timestamp, error);
        if (!result) {
                error_message (manager, _("The selected configuration for displays could not be applied"), error ? *error : NULL, NULL);
                restore_backup_configuration_without_messages (backup_filename, intended_filename);
                goto out;
        } else {
                /* We need to return as quickly as possible, so instead of
                 * confirming with the user right here, we do it in an idle
                 * handler.  The caller only expects a status for "could you
                 * change the RANDR configuration?", not "is the user OK with it
                 * as well?".
                 */
                queue_confirmation_by_user (manager, parent_window, timestamp);
        }

out:
        g_free (backup_filename);
        g_free (intended_filename);

        return result;
}

/* DBus method for org.gnome.SettingsDaemon.XRANDR_2 ApplyConfiguration; see gsd-xrandr-manager.xml for the interface definition */
static gboolean
gsd_xrandr_manager_2_apply_configuration (GsdXrandrManager *manager,
                                          gint64            parent_window_id,
                                          gint64            timestamp,
                                          GError          **error)
{
        GdkWindow *parent_window;
        gboolean result;

        if (parent_window_id != 0)
                parent_window = gdk_x11_window_foreign_new_for_display (gdk_display_get_default (), (Window) parent_window_id);
        else
                parent_window = NULL;

        result = try_to_apply_intended_configuration (manager, parent_window, (guint32) timestamp, error);

        if (parent_window)
                g_object_unref (parent_window);

        return result;
}

/* DBus method for org.gnome.SettingsDaemon.XRANDR_2 VideoModeSwitch; see gsd-xrandr-manager.xml for the interface definition */
static gboolean
gsd_xrandr_manager_2_video_mode_switch (GsdXrandrManager *manager,
                                        guint32           timestamp,
                                        GError          **error)
{
        handle_fn_f7 (manager, timestamp);
        return TRUE;
}

/* DBus method for org.gnome.SettingsDaemon.XRANDR_2 Rotate; see gsd-xrandr-manager.xml for the interface definition */
static gboolean
gsd_xrandr_manager_2_rotate (GsdXrandrManager *manager,
                             guint32           timestamp,
                             GError          **error)
{
        handle_rotate_windows (manager, GSD_RR_ROTATION_NEXT, timestamp);
        return TRUE;
}

/* DBus method for org.gnome.SettingsDaemon.XRANDR_2 RotateTo; see gsd-xrandr-manager.xml for the interface definition */
static gboolean
gsd_xrandr_manager_2_rotate_to (GsdXrandrManager *manager,
                                GsdRRRotation   rotation,
                                guint32           timestamp,
                                GError          **error)
{
        guint i;
        gboolean found;

        found = FALSE;
        for (i = 0; i < G_N_ELEMENTS (possible_rotations); i++) {
                if (rotation == possible_rotations[i]) {
                        found = TRUE;
                        break;
                }
        }

        if (found == FALSE) {
                g_debug ("Not setting out of bounds rotation '%d'", rotation);
                return FALSE;
        }

        handle_rotate_windows (manager, rotation, timestamp);
        return TRUE;
}

static gboolean
get_clone_size (GsdRRScreen *screen, int *width, int *height)
{
        GsdRRMode **modes = gsd_rr_screen_list_clone_modes (screen);
        int best_w, best_h;
        int i;

        best_w = 0;
        best_h = 0;

        for (i = 0; modes[i] != NULL; ++i) {
                GsdRRMode *mode = modes[i];
                int w, h;

                w = gsd_rr_mode_get_width (mode);
                h = gsd_rr_mode_get_height (mode);

                if (w * h > best_w * best_h) {
                        best_w = w;
                        best_h = h;
                }
        }

        if (best_w > 0 && best_h > 0) {
                if (width)
                        *width = best_w;
                if (height)
                        *height = best_h;

                return TRUE;
        }

        return FALSE;
}

static gboolean
config_is_all_off (GsdRRConfig *config)
{
        int j;
        GsdRROutputInfo **outputs;

        outputs = gsd_rr_config_get_outputs (config);

        for (j = 0; outputs[j] != NULL; ++j) {
                if (gsd_rr_output_info_is_active (outputs[j])) {
                        return FALSE;
                }
        }

        return TRUE;
}

static gboolean
laptop_lid_is_closed (GsdXrandrManager *manager)
{
        return up_client_get_lid_is_closed (manager->priv->upower_client);
}

static gboolean
is_laptop_with_closed_lid (GsdXrandrManager *manager, GsdRRScreen *screen, GsdRROutputInfo *info)
{
        return is_laptop (screen, info) && laptop_lid_is_closed (manager);
}

static GsdRRConfig *
make_clone_setup (GsdXrandrManager *manager, GsdRRScreen *screen)
{
        GsdRRConfig *result;
        GsdRROutputInfo **outputs;
        int width, height;
        int i;

        if (!get_clone_size (screen, &width, &height))
                return NULL;

        result = gsd_rr_config_new_current (screen, NULL);
        gsd_rr_config_set_clone (result, TRUE);

        outputs = gsd_rr_config_get_outputs (result);

        for (i = 0; outputs[i] != NULL; ++i) {
                GsdRROutputInfo *info = outputs[i];

                gsd_rr_output_info_set_active (info, FALSE);
                if (!is_laptop_with_closed_lid (manager, screen, info) && gsd_rr_output_info_is_connected (info)) {
                        GsdRROutput *output =
                                gsd_rr_screen_get_output_by_name (screen, gsd_rr_output_info_get_name (info));
                        GsdRRMode **modes = gsd_rr_output_list_modes (output);
                        int j;
                        int best_rate = 0;

                        for (j = 0; modes[j] != NULL; ++j) {
                                GsdRRMode *mode = modes[j];
                                int w, h;

                                w = gsd_rr_mode_get_width (mode);
                                h = gsd_rr_mode_get_height (mode);

                                if (w == width && h == height) {
                                        int r = gsd_rr_mode_get_freq (mode);
                                        if (r > best_rate)
                                                best_rate = r;
                                }
                        }

                        if (best_rate > 0) {
                                gsd_rr_output_info_set_active (info, TRUE);
                                gsd_rr_output_info_set_rotation (info, GSD_RR_ROTATION_0);
                                gsd_rr_output_info_set_refresh_rate (info, best_rate);
                                gsd_rr_output_info_set_geometry (info, 0, 0, width, height);
                        }
                }
        }

        if (config_is_all_off (result)) {
                g_object_unref (G_OBJECT (result));
                result = NULL;
        }

        print_configuration (result, "clone setup");

        return result;
}

static GsdRRMode *
find_best_mode (GsdRROutput *output)
{
        GsdRRMode *preferred;
        GsdRRMode **modes;
        int best_size;
        int best_width, best_height, best_rate;
        int i;
        GsdRRMode *best_mode;

        preferred = gsd_rr_output_get_preferred_mode (output);
        if (preferred)
                return preferred;

        modes = gsd_rr_output_list_modes (output);
        if (!modes)
                return NULL;

        best_size = best_width = best_height = best_rate = 0;
        best_mode = NULL;

        for (i = 0; modes[i] != NULL; i++) {
                int w, h, r;
                int size;

                w = gsd_rr_mode_get_width (modes[i]);
                h = gsd_rr_mode_get_height (modes[i]);
                r = gsd_rr_mode_get_freq  (modes[i]);

                size = w * h;

                if (size > best_size) {
                        best_size   = size;
                        best_width  = w;
                        best_height = h;
                        best_rate   = r;
                        best_mode   = modes[i];
                } else if (size == best_size) {
                        if (r > best_rate) {
                                best_rate = r;
                                best_mode = modes[i];
                        }
                }
        }

        return best_mode;
}

static gboolean
turn_on (GsdRRScreen *screen,
         GsdRROutputInfo *info,
         int x, int y)
{
        GsdRROutput *output = gsd_rr_screen_get_output_by_name (screen, gsd_rr_output_info_get_name (info));
        GsdRRMode *mode = find_best_mode (output);

        if (mode) {
                gsd_rr_output_info_set_active (info, TRUE);
                gsd_rr_output_info_set_geometry (info, x, y, gsd_rr_mode_get_width (mode), gsd_rr_mode_get_height (mode));
                gsd_rr_output_info_set_rotation (info, GSD_RR_ROTATION_0);
                gsd_rr_output_info_set_refresh_rate (info, gsd_rr_mode_get_freq (mode));

                return TRUE;
        }

        return FALSE;
}

static GsdRRConfig *
make_laptop_setup (GsdXrandrManager *manager, GsdRRScreen *screen)
{
        /* Turn on the laptop, disable everything else */
        GsdRRConfig *result = gsd_rr_config_new_current (screen, NULL);
        GsdRROutputInfo **outputs = gsd_rr_config_get_outputs (result);
        int i;

        gsd_rr_config_set_clone (result, FALSE);

        for (i = 0; outputs[i] != NULL; ++i) {
                GsdRROutputInfo *info = outputs[i];

                if (is_laptop (screen, info) && !laptop_lid_is_closed (manager)) {
                        if (!turn_on (screen, info, 0, 0)) {
                                g_object_unref (G_OBJECT (result));
                                result = NULL;
                                break;
                        }
                }
                else {
                        gsd_rr_output_info_set_active (info, FALSE);
                }
        }


        if (config_is_all_off (result)) {
                g_object_unref (G_OBJECT (result));
                result = NULL;
        }

        print_configuration (result, "Laptop setup");

        /* FIXME - Maybe we should return NULL if there is more than
         * one connected "laptop" screen?
         */
        return result;

}

static int
turn_on_and_get_rightmost_offset (GsdRRScreen *screen, GsdRROutputInfo *info, int x)
{
        if (turn_on (screen, info, x, 0)) {
                int width;
                gsd_rr_output_info_get_geometry (info, NULL, NULL, &width, NULL);
                x += width;
        }

        return x;
}

/* Used from g_ptr_array_sort(); compares outputs based on their X position */
static int
compare_output_positions (gconstpointer a, gconstpointer b)
{
        GsdRROutputInfo **oa = (GsdRROutputInfo **) a;
        GsdRROutputInfo **ob = (GsdRROutputInfo **) b;
        int xa, xb;

        gsd_rr_output_info_get_geometry (*oa, &xa, NULL, NULL, NULL);
        gsd_rr_output_info_get_geometry (*ob, &xb, NULL, NULL, NULL);

        return xb - xa;
}

/* A set of outputs with already-set sizes and positions may not fit in the
 * frame buffer that is available.  Turn off outputs right-to-left until we find
 * a size that fits.  Returns whether something applicable was found
 * (i.e. something that fits and that does not consist of only-off outputs).
 */
static gboolean
trim_rightmost_outputs_that_dont_fit_in_framebuffer (GsdRRScreen *rr_screen, GsdRRConfig *config)
{
        GsdRROutputInfo **outputs;
        int i;
        gboolean applicable;
        GPtrArray *sorted_outputs;

        outputs = gsd_rr_config_get_outputs (config);
        g_return_val_if_fail (outputs != NULL, FALSE);

        /* How many are on? */

        sorted_outputs = g_ptr_array_new ();
        for (i = 0; outputs[i] != NULL; i++) {
                if (gsd_rr_output_info_is_active (outputs[i]))
                        g_ptr_array_add (sorted_outputs, outputs[i]);
        }

        /* Lay them out from left to right */

        g_ptr_array_sort (sorted_outputs, compare_output_positions);

        /* Trim! */

        applicable = FALSE;

        for (i = sorted_outputs->len - 1; i >= 0; i--) {
                GError *error = NULL;
                gboolean is_bounds_error;

                applicable = gsd_rr_config_applicable (config, rr_screen, &error);
                if (applicable)
                        break;

                is_bounds_error = g_error_matches (error, GSD_RR_ERROR, GSD_RR_ERROR_BOUNDS_ERROR);
                g_error_free (error);

                if (!is_bounds_error)
                        break;

                gsd_rr_output_info_set_active (sorted_outputs->pdata[i], FALSE);
        }

        if (config_is_all_off (config))
                applicable = FALSE;

        g_ptr_array_free (sorted_outputs, FALSE);

        return applicable;
}

static gboolean
follow_laptop_lid(GsdXrandrManager *manager)
{
        GsdXrandrBootBehaviour val;
        val = g_settings_get_enum (manager->priv->settings, CONF_KEY_DEFAULT_MONITORS_SETUP);
        return val == GSD_XRANDR_BOOT_BEHAVIOUR_FOLLOW_LID || val == GSD_XRANDR_BOOT_BEHAVIOUR_CLONE;
}

static GsdRRConfig *
make_xinerama_setup (GsdXrandrManager *manager, GsdRRScreen *screen)
{
        /* Turn on everything that has a preferred mode, and
         * position it from left to right
         */
        GsdRRConfig *result = gsd_rr_config_new_current (screen, NULL);
        GsdRROutputInfo **outputs = gsd_rr_config_get_outputs (result);
        int i;
        int x;

        gsd_rr_config_set_clone (result, FALSE);

        x = 0;
        for (i = 0; outputs[i] != NULL; ++i) {
                GsdRROutputInfo *info = outputs[i];

                if (is_laptop (screen, info)) {
                        if (laptop_lid_is_closed (manager) && follow_laptop_lid (manager))
                                gsd_rr_output_info_set_active (info, FALSE);
                        else {
                                gsd_rr_output_info_set_primary (info, TRUE);
                                x = turn_on_and_get_rightmost_offset (screen, info, x);
                        }
                }
        }

        for (i = 0; outputs[i] != NULL; ++i) {
                GsdRROutputInfo *info = outputs[i];

                if (gsd_rr_output_info_is_connected (info) && !is_laptop (screen, info)) {
                        gsd_rr_output_info_set_primary (info, FALSE);
                        x = turn_on_and_get_rightmost_offset (screen, info, x);
                }
        }

        if (!trim_rightmost_outputs_that_dont_fit_in_framebuffer (screen, result)) {
                g_object_unref (G_OBJECT (result));
                result = NULL;
        }

        print_configuration (result, "xinerama setup");

        return result;
}

static GsdRRConfig *
make_other_setup (GsdRRScreen *screen)
{
        /* Turn off all laptops, and make all external monitors clone
         * from (0, 0)
         */

        GsdRRConfig *result = gsd_rr_config_new_current (screen, NULL);
        GsdRROutputInfo **outputs = gsd_rr_config_get_outputs (result);
        int i;

        gsd_rr_config_set_clone (result, FALSE);

        for (i = 0; outputs[i] != NULL; ++i) {
                GsdRROutputInfo *info = outputs[i];

                if (is_laptop (screen, info)) {
                        gsd_rr_output_info_set_active (info, FALSE);
                }
                else {
                        if (gsd_rr_output_info_is_connected (info))
                                turn_on (screen, info, 0, 0);
               }
        }

        if (!trim_rightmost_outputs_that_dont_fit_in_framebuffer (screen, result)) {
                g_object_unref (G_OBJECT (result));
                result = NULL;
        }

        print_configuration (result, "other setup");

        return result;
}

static GPtrArray *
sanitize (GsdXrandrManager *manager, GPtrArray *array)
{
        int i;
        GPtrArray *new;

        g_debug ("before sanitizing");

        for (i = 0; i < array->len; ++i) {
                if (array->pdata[i]) {
                        print_configuration (array->pdata[i], "before");
                }
        }


        /* Remove configurations that are duplicates of
         * configurations earlier in the cycle
         */
        for (i = 0; i < array->len; i++) {
                int j;

                for (j = i + 1; j < array->len; j++) {
                        GsdRRConfig *this = array->pdata[j];
                        GsdRRConfig *other = array->pdata[i];

                        if (this && other && gsd_rr_config_equal (this, other)) {
                                g_debug ("removing duplicate configuration");
                                g_object_unref (this);
                                array->pdata[j] = NULL;
                                break;
                        }
                }
        }

        for (i = 0; i < array->len; ++i) {
                GsdRRConfig *config = array->pdata[i];

                if (config && config_is_all_off (config)) {
                        g_debug ("removing configuration as all outputs are off");
                        g_object_unref (array->pdata[i]);
                        array->pdata[i] = NULL;
                }
        }

        /* Do a final sanitization pass.  This will remove configurations that
         * don't fit in the framebuffer's Virtual size.
         */

        for (i = 0; i < array->len; i++) {
                GsdRRConfig *config = array->pdata[i];

                if (config) {
                        GError *error;

                        error = NULL;
                        if (!gsd_rr_config_applicable (config, manager->priv->rw_screen, &error)) { /* NULL-GError */
                                g_debug ("removing configuration which is not applicable because %s", error->message);
                                g_error_free (error);

                                g_object_unref (config);
                                array->pdata[i] = NULL;
                        }
                }
        }

        /* Remove NULL configurations */
        new = g_ptr_array_new ();

        for (i = 0; i < array->len; ++i) {
                if (array->pdata[i]) {
                        g_ptr_array_add (new, array->pdata[i]);
                        print_configuration (array->pdata[i], "Final");
                }
        }

        if (new->len > 0) {
                g_ptr_array_add (new, NULL);
        } else {
                g_ptr_array_free (new, TRUE);
                new = NULL;
        }

        g_ptr_array_free (array, TRUE);

        return new;
}

static void
generate_fn_f7_configs (GsdXrandrManager *mgr)
{
        GPtrArray *array = g_ptr_array_new ();
        GsdRRScreen *screen = mgr->priv->rw_screen;

        g_debug ("Generating configurations");

        /* Free any existing list of configurations */
        if (mgr->priv->fn_f7_configs) {
                int i;

                for (i = 0; mgr->priv->fn_f7_configs[i] != NULL; ++i)
                        g_object_unref (mgr->priv->fn_f7_configs[i]);
                g_free (mgr->priv->fn_f7_configs);

                mgr->priv->fn_f7_configs = NULL;
                mgr->priv->current_fn_f7_config = -1;
        }

        g_ptr_array_add (array, gsd_rr_config_new_current (screen, NULL));
        g_ptr_array_add (array, make_clone_setup (mgr, screen));
        g_ptr_array_add (array, make_xinerama_setup (mgr, screen));
        g_ptr_array_add (array, make_other_setup (screen));
        g_ptr_array_add (array, make_laptop_setup (mgr, screen));

        array = sanitize (mgr, array);

        if (array) {
                mgr->priv->fn_f7_configs = (GsdRRConfig **)g_ptr_array_free (array, FALSE);
                mgr->priv->current_fn_f7_config = 0;
        }
}

static void
error_message (GsdXrandrManager *mgr, const char *primary_text, GError *error_to_display, const char *secondary_text)
{
        GtkWidget *dialog;

        dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                         "%s", primary_text);
        gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s",
                                                  error_to_display ? error_to_display->message : secondary_text);

        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
}

static void
handle_fn_f7 (GsdXrandrManager *mgr, guint32 timestamp)
{
        GsdXrandrManagerPrivate *priv = mgr->priv;
        GsdRRScreen *screen = priv->rw_screen;
        GsdRRConfig *current;
        GError *error;

        /* Theory of fn-F7 operation
         *
         * We maintain a datastructure "fn_f7_status", that contains
         * a list of GsdRRConfig's. Each of the GsdRRConfigs has a
         * mode (or "off") for each connected output.
         *
         * When the user hits fn-F7, we cycle to the next GsdRRConfig
         * in the data structure. If the data structure does not exist, it
         * is generated. If the configs in the data structure do not match
         * the current hardware reality, it is regenerated.
         *
         */
        g_debug ("Handling fn-f7");

        log_open ();
        log_msg ("Handling XF86Display hotkey - timestamp %u\n", timestamp);

        error = NULL;
        if (!gsd_rr_screen_refresh (screen, &error) && error) {
                char *str;

                str = g_strdup_printf (_("Could not refresh the screen information: %s"), error->message);
                g_error_free (error);

                log_msg ("%s\n", str);
                error_message (mgr, str, NULL, _("Trying to switch the monitor configuration anyway."));
                g_free (str);
        }

        if (!priv->fn_f7_configs) {
                log_msg ("Generating stock configurations:\n");
                generate_fn_f7_configs (mgr);
                log_configurations (priv->fn_f7_configs);
        }

        current = gsd_rr_config_new_current (screen, NULL);

        if (priv->fn_f7_configs &&
            (!gsd_rr_config_match (current, priv->fn_f7_configs[0]) ||
             !gsd_rr_config_equal (current, priv->fn_f7_configs[mgr->priv->current_fn_f7_config]))) {
                    /* Our view of the world is incorrect, so regenerate the
                     * configurations
                     */
                    generate_fn_f7_configs (mgr);
                    log_msg ("Regenerated stock configurations:\n");
                    log_configurations (priv->fn_f7_configs);
            }

        g_object_unref (current);

        if (priv->fn_f7_configs) {
                guint32 server_timestamp;
                gboolean success;

                mgr->priv->current_fn_f7_config++;

                if (priv->fn_f7_configs[mgr->priv->current_fn_f7_config] == NULL)
                        mgr->priv->current_fn_f7_config = 0;

                g_debug ("cycling to next configuration (%d)", mgr->priv->current_fn_f7_config);

                print_configuration (priv->fn_f7_configs[mgr->priv->current_fn_f7_config], "new config");

                g_debug ("applying");

                /* See https://bugzilla.gnome.org/show_bug.cgi?id=610482
                 *
                 * Sometimes we'll get two rapid XF86Display keypress events,
                 * but their timestamps will be out of order with respect to the
                 * RANDR timestamps.  This *may* be due to stupid BIOSes sending
                 * out display-switch keystrokes "to make Windows work".
                 *
                 * The X server will error out if the timestamp provided is
                 * older than a previous change configuration timestamp. We
                 * assume here that we do want this event to go through still,
                 * since kernel timestamps may be skewed wrt the X server.
                 */
                gsd_rr_screen_get_timestamps (screen, NULL, &server_timestamp);
                if (timestamp < server_timestamp)
                        timestamp = server_timestamp;

                success = apply_configuration (mgr, priv->fn_f7_configs[mgr->priv->current_fn_f7_config], timestamp, TRUE, TRUE);

                if (success) {
                        log_msg ("Successfully switched to configuration (timestamp %u):\n", timestamp);
                        log_configuration (priv->fn_f7_configs[mgr->priv->current_fn_f7_config]);
                }
        }
        else {
                g_debug ("no configurations generated");
        }

        log_close ();

        g_debug ("done handling fn-f7");
}

static GsdRRRotation
get_next_rotation (GsdRRRotation allowed_rotations, GsdRRRotation current_rotation)
{
        int i;
        int current_index;

        /* First, find the index of the current rotation */

        current_index = -1;

        for (i = 0; i < G_N_ELEMENTS (possible_rotations); i++) {
                GsdRRRotation r;

                r = possible_rotations[i];
                if (r == current_rotation) {
                        current_index = i;
                        break;
                }
        }

        if (current_index == -1) {
                /* Huh, the current_rotation was not one of the supported rotations.  Bail out. */
                return current_rotation;
        }

        /* Then, find the next rotation that is allowed */

        i = (current_index + 1) % G_N_ELEMENTS (possible_rotations);

        while (1) {
                GsdRRRotation r;

                r = possible_rotations[i];
                if (r == current_rotation) {
                        /* We wrapped around and no other rotation is suported.  Bummer. */
                        return current_rotation;
                } else if (r & allowed_rotations)
                        return r;

                i = (i + 1) % G_N_ELEMENTS (possible_rotations);
        }
}

struct {
        GsdRRRotation rotation;
        /* Coordinate Transformation Matrix */
        gfloat matrix[9];
} evdev_rotations[] = {
        { GSD_RR_ROTATION_0, {1, 0, 0, 0, 1, 0, 0, 0, 1}},
        { GSD_RR_ROTATION_90, {0, -1, 1, 1, 0, 0, 0, 0, 1}},
        { GSD_RR_ROTATION_180, {-1, 0, 1, 0, -1, 1, 0, 0, 1}},
        { GSD_RR_ROTATION_270, {0, 1, 0, -1, 0, 1, 0,  0, 1}}
};

static guint
get_rotation_index (GsdRRRotation rotation)
{
        guint i;

        for (i = 0; i < G_N_ELEMENTS (evdev_rotations); i++) {
                if (evdev_rotations[i].rotation == rotation)
                        return i;
        }
        g_assert_not_reached ();
}

static gboolean
is_wacom_tablet_device (GsdXrandrManager *mgr,
                        XDeviceInfo      *device_info)
{
#ifdef HAVE_WACOM
        GsdXrandrManagerPrivate *priv = mgr->priv;
        gchar       *device_node;
        WacomDevice *wacom_device;
        gboolean     is_tablet = FALSE;

        if (priv->wacom_db == NULL)
                priv->wacom_db = libwacom_database_new ();

        device_node = xdevice_get_device_node (device_info->id);
        if (device_node == NULL)
                return FALSE;

        wacom_device = libwacom_new_from_path (priv->wacom_db, device_node, FALSE, NULL);
        g_free (device_node);
        if (wacom_device == NULL) {
                g_free (device_node);
                return FALSE;
        }
        is_tablet = libwacom_has_touch (wacom_device) &&
                    libwacom_is_builtin (wacom_device);

        libwacom_destroy (wacom_device);

        return is_tablet;
#else  /* HAVE_WACOM */
        return FALSE;
#endif /* HAVE_WACOM */
}

static void
rotate_touchscreens (GsdXrandrManager *mgr,
                     GsdRRRotation   rotation)
{
        XDeviceInfo *device_info;
        gint n_devices;
        guint i, rot_idx;
        Atom float_atom;

        if (!supports_xinput_devices ())
                return;

        g_debug ("Rotating touchscreen devices");

        device_info = XListInputDevices (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), &n_devices);
        if (device_info == NULL)
                return;

        rot_idx = get_rotation_index (rotation);

        float_atom = XInternAtom(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), "FLOAT", True);

        for (i = 0; i < n_devices; i++) {
                if (is_wacom_tablet_device  (mgr, &device_info[i])) {
                        g_debug ("Not rotating tablet device '%s'", device_info[i].name);
                        continue;
                }

                if (device_info_is_touchscreen (&device_info[i]) ||
                            device_info_is_tablet (&device_info[i])) {
                        XDevice *device;
                        gfloat *m = evdev_rotations[rot_idx].matrix;
                        PropertyHelper matrix = {
                                .name = "Coordinate Transformation Matrix",
                                .nitems = 9,
                                .format = 32,
                                .type = float_atom,
                                .data.i = (int *)m,
                        };

                        g_debug ("About to rotate '%s'", device_info[i].name);

                        gdk_error_trap_push ();
                        device = XOpenDevice (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), device_info[i].id);
                        if (gdk_error_trap_pop () || (device == NULL))
                                continue;

                        if (device_set_property (device, device_info[i].name, &matrix) != FALSE) {
                                g_debug ("Rotated '%s' to configuration '%f, %f, %f, %f, %f, %f, %f, %f, %f'\n",
                                         device_info[i].name,
                                         evdev_rotations[rot_idx].matrix[0],
                                         evdev_rotations[rot_idx].matrix[1],
                                         evdev_rotations[rot_idx].matrix[2],
                                         evdev_rotations[rot_idx].matrix[3],
                                         evdev_rotations[rot_idx].matrix[4],
                                         evdev_rotations[rot_idx].matrix[5],
                                         evdev_rotations[rot_idx].matrix[6],
                                         evdev_rotations[rot_idx].matrix[7],
                                         evdev_rotations[rot_idx].matrix[8]);
                        }

                        XCloseDevice (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), device);
                }
        }
        XFreeDeviceList (device_info);
}

/* We use this when the XF86RotateWindows key is pressed, or the
 * orientation of a tablet changes. The key is present
 * on some tablet PCs; they use it so that the user can rotate the tablet
 * easily. Some other tablet PCs will have an accelerometer instead.
 */
static void
handle_rotate_windows (GsdXrandrManager *mgr,
                       GsdRRRotation rotation,
                       guint32 timestamp)
{
        GsdXrandrManagerPrivate *priv = mgr->priv;
        GsdRRScreen *screen = priv->rw_screen;
        GsdRRConfig *current;
        GsdRROutputInfo *rotatable_output_info;
        int num_allowed_rotations;
        GsdRRRotation allowed_rotations;
        GsdRRRotation next_rotation;
        gboolean success, show_error;

        g_debug ("Handling XF86RotateWindows with rotation %d", rotation);

        /* Which output? */

        current = gsd_rr_config_new_current (screen, NULL);

        rotatable_output_info = get_laptop_output_info (screen, current);
        if (rotatable_output_info == NULL) {
                g_debug ("No laptop outputs found to rotate; XF86RotateWindows key will do nothing");
                goto out;
        }

        if (rotation <= GSD_RR_ROTATION_NEXT) {
                /* Which rotation? */

                get_allowed_rotations_for_output (current, priv->rw_screen, rotatable_output_info, &num_allowed_rotations, &allowed_rotations);
                next_rotation = get_next_rotation (allowed_rotations, gsd_rr_output_info_get_rotation (rotatable_output_info));

                if (next_rotation == gsd_rr_output_info_get_rotation (rotatable_output_info)) {
                        g_debug ("No rotations are supported other than the current one; XF86RotateWindows key will do nothing");
                        goto out;
                }
                show_error = TRUE;
        } else {
                next_rotation = rotation;
                show_error = FALSE;
        }

        /* Rotate */

        gsd_rr_output_info_set_rotation (rotatable_output_info, next_rotation);

        success = apply_configuration (mgr, current, timestamp, show_error, TRUE);
        if (success)
                rotate_touchscreens (mgr, next_rotation);

out:
        g_object_unref (current);
}

static GsdRRConfig *
make_default_setup (GsdXrandrManager *manager)
{
        GsdXrandrManagerPrivate *priv = manager->priv;
        GsdRRConfig *config;
        GsdXrandrBootBehaviour boot;

        boot = g_settings_get_enum (priv->settings, CONF_KEY_DEFAULT_MONITORS_SETUP);
        g_debug ("xrandr default monitors setup: %d\n", boot);

        switch (boot) {
        case GSD_XRANDR_BOOT_BEHAVIOUR_DO_NOTHING:
                config = make_xinerama_setup (manager, priv->rw_screen);
                break;
        case GSD_XRANDR_BOOT_BEHAVIOUR_FOLLOW_LID:
                if (laptop_lid_is_closed (manager))
                        config = make_other_setup (priv->rw_screen);
                else
                        config = make_xinerama_setup (manager, priv->rw_screen);
                break;
        case GSD_XRANDR_BOOT_BEHAVIOUR_CLONE:
                config = make_clone_setup (manager, priv->rw_screen);
                break;
        case GSD_XRANDR_BOOT_BEHAVIOUR_DOCK:
                config = make_other_setup (priv->rw_screen);
                break;
        default:
                g_assert_not_reached ();
        }

        return config;
}

static void
auto_configure_outputs (GsdXrandrManager *manager, guint32 timestamp)
{
        GsdRRConfig *config;

        g_debug ("xrandr auto-configure");
        config = make_default_setup (manager);
        if (config) {
                apply_configuration (manager, config, timestamp, TRUE, FALSE);
                g_object_unref (config);
        } else {
                g_debug ("No applicable configuration found during auto-configure");
        }
}

static void
use_stored_configuration_or_auto_configure_outputs (GsdXrandrManager *manager, guint32 timestamp)
{
        GsdXrandrManagerPrivate *priv = manager->priv;
        char *intended_filename;
        GError *error;
        gboolean success;

        intended_filename = gsd_rr_config_get_intended_filename ();

        error = NULL;
        success = apply_configuration_from_filename (manager, intended_filename, TRUE, timestamp, &error);
        g_free (intended_filename);

        if (!success) {
                /* We don't bother checking the error type.
                 *
                 * Both G_FILE_ERROR_NOENT and
                 * GSD_RR_ERROR_NO_MATCHING_CONFIG would mean, "there
                 * was no configuration to apply, or none that matched
                 * the current outputs", and in that case we need to run
                 * our fallback.
                 *
                 * Any other error means "we couldn't do the smart thing
                 * of using a previously- saved configuration, anyway,
                 * for some other reason.  In that case, we also need to
                 * run our fallback to avoid leaving the user with a
                 * bogus configuration.
                 */

                if (error)
                        g_error_free (error);

                if (timestamp != priv->last_config_timestamp || timestamp == GDK_CURRENT_TIME) {
                        priv->last_config_timestamp = timestamp;
                        auto_configure_outputs (manager, timestamp);
                        log_msg ("  Automatically configured outputs\n");
                } else
                        log_msg ("  Ignored autoconfiguration as old and new config timestamps are the same\n");
        } else
                log_msg ("Applied stored configuration\n");
}

static void
on_randr_event (GsdRRScreen *screen, gpointer data)
{
        GsdXrandrManager *manager = GSD_XRANDR_MANAGER (data);
        GsdXrandrManagerPrivate *priv = manager->priv;
        guint32 change_timestamp, config_timestamp;

        if (!priv->running)
                return;

        gsd_rr_screen_get_timestamps (screen, &change_timestamp, &config_timestamp);

        log_open ();
        log_msg ("Got RANDR event with timestamps change=%u %c config=%u\n",
                 change_timestamp,
                 timestamp_relationship (change_timestamp, config_timestamp),
                 config_timestamp);

        if (change_timestamp >= config_timestamp) {
                GsdRRConfig *rr_config;

                /* The event is due to an explicit configuration change.
                 *
                 * If the change was performed by us, then we need to do nothing.
                 *
                 * If the change was done by some other X client, we don't need
                 * to do anything, either; the screen is already configured.
                 */

                /* Check if we need to update the primary */
                rr_config = gsd_rr_config_new_current (priv->rw_screen, NULL);
                if (gsd_rr_config_ensure_primary (rr_config)) {
                        if (gsd_rr_config_applicable (rr_config, priv->rw_screen, NULL)) {
                                print_configuration (rr_config, "Updating for primary");
                                priv->last_config_timestamp = config_timestamp;
                                gsd_rr_config_apply_with_time (rr_config, priv->rw_screen, config_timestamp, NULL);
                        }
                }
                g_object_unref (rr_config);

                show_timestamps_dialog (manager, "ignoring since change > config");
                log_msg ("  Ignoring event since change >= config\n");
        } else {
                /* Here, config_timestamp > change_timestamp.  This means that
                 * the screen got reconfigured because of hotplug/unplug; the X
                 * server is just notifying us, and we need to configure the
                 * outputs in a sane way.
                 */

                show_timestamps_dialog (manager, "need to deal with reconfiguration, as config > change");
                use_stored_configuration_or_auto_configure_outputs (manager, config_timestamp);
        }

        if (priv->main_touchscreen_id != -1) {
                /* Set mapping of input devices onto displays */
                log_msg ("\nSetting touchscreen mapping on RandR event\n");
                do_touchscreen_mapping (manager);
        }

        log_close ();
}

static void
get_allowed_rotations_for_output (GsdRRConfig *config,
                                  GsdRRScreen *rr_screen,
                                  GsdRROutputInfo *output,
                                  int *out_num_rotations,
                                  GsdRRRotation *out_rotations)
{
        GsdRRRotation current_rotation;
        int i;

        *out_num_rotations = 0;
        *out_rotations = 0;

        current_rotation = gsd_rr_output_info_get_rotation (output);

        /* Yay for brute force */

        for (i = 0; i < G_N_ELEMENTS (possible_rotations); i++) {
                GsdRRRotation rotation_to_test;

                rotation_to_test = possible_rotations[i];

                gsd_rr_output_info_set_rotation (output, rotation_to_test);

                if (gsd_rr_config_applicable (config, rr_screen, NULL)) { /* NULL-GError */
                        (*out_num_rotations)++;
                        (*out_rotations) |= rotation_to_test;
                }
        }

        gsd_rr_output_info_set_rotation (output, current_rotation);

        if (*out_num_rotations == 0 || *out_rotations == 0) {
                g_warning ("Huh, output %p says it doesn't support any rotations, and yet it has a current rotation?", output);
                *out_num_rotations = 1;
                *out_rotations = gsd_rr_output_info_get_rotation (output);
        }
}

static gboolean
apply_intended_configuration (GsdXrandrManager *manager, const char *intended_filename, guint32 timestamp)
{
        GError *my_error;
        gboolean result;

        my_error = NULL;
        result = apply_configuration_from_filename (manager, intended_filename, TRUE, timestamp, &my_error);
        if (!result) {
                if (my_error) {
                        if (!g_error_matches (my_error, G_FILE_ERROR, G_FILE_ERROR_NOENT) &&
                            !g_error_matches (my_error, GSD_RR_ERROR, GSD_RR_ERROR_NO_MATCHING_CONFIG))
                                error_message (manager, _("Could not apply the stored configuration for monitors"), my_error, NULL);

                        g_error_free (my_error);
                }
        }

        return result;
}

static void
apply_default_boot_configuration (GsdXrandrManager *mgr, guint32 timestamp)
{
        GsdXrandrManagerPrivate *priv = mgr->priv;
        GsdRRConfig *config;
        GsdXrandrBootBehaviour boot;

        boot = g_settings_get_enum (priv->settings, CONF_KEY_DEFAULT_MONITORS_SETUP);

        if (boot == GSD_XRANDR_BOOT_BEHAVIOUR_DO_NOTHING)
                return;

        config = make_default_setup (mgr);
        if (config) {
                /* We don't save the configuration (the "false" parameter to the following function) because we don't want to
                 * install a user-side setting when *here* we are using a system-default setting.
                 */
                apply_configuration (mgr, config, timestamp, TRUE, FALSE);
                g_object_unref (config);
        }
}

static gboolean
apply_stored_configuration_at_startup (GsdXrandrManager *manager, guint32 timestamp)
{
        GError *my_error;
        gboolean success;
        char *backup_filename;
        char *intended_filename;
        GsdPnpIds *pnp_ids;

        /* This avoids the GsdPnpIds object being created multiple times.
         * See c9240e8b69c5833074508b46bc56307aac12ec19 */
        pnp_ids = gsd_pnp_ids_new ();
        backup_filename = gsd_rr_config_get_backup_filename ();
        intended_filename = gsd_rr_config_get_intended_filename ();

        /* 1. See if there was a "saved" configuration.  If there is one, it means
         * that the user had selected to change the display configuration, but the
         * machine crashed.  In that case, we'll apply *that* configuration and save it on top of the
         * "intended" one.
         */

        my_error = NULL;

        success = apply_configuration_from_filename (manager, backup_filename, FALSE, timestamp, &my_error);
        if (success) {
                /* The backup configuration existed, and could be applied
                 * successfully, so we must restore it on top of the
                 * failed/intended one.
                 */
                restore_backup_configuration (manager, backup_filename, intended_filename, timestamp);
                goto out;
        }

        if (!g_error_matches (my_error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
                /* Epic fail:  there (probably) was a backup configuration, but
                 * we could not apply it.  The only thing we can do is delete
                 * the backup configuration.  Let's hope that the user doesn't
                 * get left with an unusable display...
                 */

                unlink (backup_filename);
                goto out;
        }

        /* 2. There was no backup configuration!  This means we are
         * good.  Apply the intended configuration instead.
         */

        success = apply_intended_configuration (manager, intended_filename, timestamp);

out:
	g_object_unref (pnp_ids);

        if (my_error)
                g_error_free (my_error);

        g_free (backup_filename);
        g_free (intended_filename);

        return success;
}

static gboolean
apply_default_configuration_from_file (GsdXrandrManager *manager, guint32 timestamp)
{
        GsdXrandrManagerPrivate *priv = manager->priv;
        char *default_config_filename;
        gboolean result;

        default_config_filename = g_settings_get_string (priv->settings, CONF_KEY_DEFAULT_CONFIGURATION_FILE);
        if (!default_config_filename)
                return FALSE;

        result = apply_configuration_from_filename (manager, default_config_filename, TRUE, timestamp, NULL);

        g_free (default_config_filename);
        return result;
}

static void
turn_off_laptop_display (GsdXrandrManager *manager, guint32 timestamp)
{
        GsdXrandrManagerPrivate *priv = manager->priv;
        GsdRRConfig *config;
        
        config = gsd_rr_config_new_current (priv->rw_screen, NULL);

        turn_off_laptop_display_in_configuration (priv->rw_screen, config);

        /* We don't turn the laptop's display off if it is the only display present. */
        if (!config_is_all_off (config)) {
                /* We don't save the configuration (the "false" parameter to the following function) because we
                 * wouldn't want to restore a configuration with the laptop's display turned off, if at some
                 * point later the user booted his laptop with the lid open.
                 */
                apply_configuration (manager, config, timestamp, FALSE, FALSE);
        }

        g_object_unref (config);
}

static void
lid_state_changed_cb (UpClient *client, GParamSpec *pspec, gpointer data)
{
        GsdXrandrManager *manager = data;
        GsdXrandrManagerPrivate *priv = manager->priv;
        gboolean is_closed;

        is_closed = up_client_get_lid_is_closed (priv->upower_client);

        if (is_closed != priv->laptop_lid_is_closed) {
                priv->laptop_lid_is_closed = is_closed;
                if (!follow_laptop_lid(manager))
                        return;

                /* Refresh the RANDR state.  The lid just got opened/closed, so we can afford to
                 * probe the outputs right now.  It will also help the case where we can't detect
                 * hotplug/unplug, but the fact that the lid's state changed lets us know that the
                 * user probably did something interesting.
                 */

                gsd_rr_screen_refresh (priv->rw_screen, NULL); /* NULL-GError */

                if (is_closed)
                        turn_off_laptop_display (manager, GDK_CURRENT_TIME); /* sucks not to have a timestamp for the notification */
                else
                        use_stored_configuration_or_auto_configure_outputs (manager, GDK_CURRENT_TIME);
        }
}

static gboolean
matches_name (GsdRROutputInfo *output, GsdRROutput *to_match)
{
        return (g_strcmp0 (gsd_rr_output_info_get_name (output),
                       gsd_rr_output_get_name (to_match) ) == 0);
}

static gint
monitor_for_output (GsdRROutput *output)
{
        GdkScreen *screen = gdk_screen_get_default ();
        GsdRRCrtc *crtc = gsd_rr_output_get_crtc (output);
        gint x, y;

        if (!crtc)
                return -1;

        gsd_rr_crtc_get_position (crtc, &x, &y);

        return gdk_screen_get_monitor_at_point (screen, x, y);
}

static gboolean
output_get_dimensions (GsdRROutput *output,
                       guint         *width,
                       guint         *height)
{
        GdkScreen *screen = gdk_screen_get_default ();
        gint monitor_num;

        monitor_num = monitor_for_output (output);

        if (monitor_num < 0)
                return FALSE;

        *width = gdk_screen_get_monitor_width_mm (screen, monitor_num);
        *height = gdk_screen_get_monitor_height_mm (screen, monitor_num);
        return TRUE;
}

static GsdRROutput *
input_info_find_size_match (GsdXrandrManager *manager, GsdRRScreen *rr_screen)
{
        guint i, input_width, input_height, output_width, output_height;
        gdouble min_width_diff, min_height_diff;
        GsdRROutput **outputs, *match = NULL;
        GsdXrandrManagerPrivate *priv = manager->priv;

        g_return_val_if_fail (rr_screen != NULL, NULL);

        if (!xdevice_get_dimensions (priv->main_touchscreen_id,
                                     &input_width, &input_height))
                return NULL;

        /* Restrict the matches to be below a narrow percentage */
        min_width_diff = min_height_diff = 0.05;

        g_debug ("Input device '%s' has %dx%d mm",
                 priv->main_touchscreen_name, input_width, input_height);

        outputs = gsd_rr_screen_list_outputs (rr_screen);

        for (i = 0; outputs[i] != NULL; i++) {
                gdouble width_diff, height_diff;
                if (!output_get_dimensions (outputs[i], &output_width, &output_height))
                        continue;

                width_diff = ABS (1 - ((gdouble) output_width / input_width));
                height_diff = ABS (1 - ((gdouble) output_height / input_height));

                g_debug ("Output '%s' has size %dx%d mm, deviation from "
                         "input device size: %.2f width, %.2f height ",
                         gsd_rr_output_get_name (outputs[i]),
                         output_width, output_height, width_diff, height_diff);

                if (width_diff <= min_width_diff && height_diff <= min_height_diff) {
                    match = outputs[i];
                    min_width_diff = width_diff;
                    min_height_diff = height_diff;
                }
        }

        if (match) {
                g_debug ("Output '%s' is considered a best size match (%.2f / %.2f)",
                         gsd_rr_output_get_name (match),
                         min_width_diff, min_height_diff);
        } else {
                g_debug ("No input/output size match was found\n");
        }

        return match;
}

static GsdRROutputInfo *
get_mappable_output_info (GsdXrandrManager *manager, GsdRRScreen *screen, GsdRRConfig *config)
{
        int i;
        GsdRROutputInfo **outputs = gsd_rr_config_get_outputs (config);

        GsdRROutput *size_match = NULL;

        size_match = input_info_find_size_match (manager, screen);

        for (i = 0; outputs[i] != NULL; i++) {
                if (is_laptop (screen, outputs[i]) || (size_match && matches_name (outputs[i], size_match)))
                        return outputs[i];
        }

        return NULL;
}

static void
set_touchscreen_id (GsdXrandrManager *manager)
{
        GsdXrandrManagerPrivate *priv = manager->priv;
        XDeviceInfo *device_info;
        int n_devices;
        int i;

        device_info = XListInputDevices (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                         &n_devices);
        if (device_info == NULL)
                return;

        for (i = 0; i < n_devices; i++) {
                if (device_info_is_touchscreen (&device_info[i])) {
                    /* Let's deal only with the 1st touchscreen */
                    priv->main_touchscreen_id = (int)device_info[i].id;
                    priv->main_touchscreen_name = g_strdup (device_info[i].name);
                    break;
                }
        }

        XFreeDeviceList (device_info);
}

static int
map_touch_to_output (GsdRRScreen *screen, int device_id,
                     GsdRROutputInfo *output)
{
        int status = 0;
        char command[100];
        gchar *name = gsd_rr_output_info_get_name (output);

        if (!name) {
                g_debug ("Failure to map screen with missing name");
                status = 1;
                goto out;
        }

        if (gsd_rr_output_info_is_active(output)) {
                g_debug ("Mapping touchscreen %d onto output %s",
                         device_id, name);
                sprintf (command, "xinput  --map-to-output %d %s",
                         device_id, name);
                status = system (command);
        }
        else {
                g_debug ("No need to map %d onto output %s. The output is off",
                         device_id, name);
        }

out:
        return (status == 0);
}

static void
do_touchscreen_mapping (GsdXrandrManager *manager)
{
        GsdXrandrManagerPrivate *priv = manager->priv;
        GsdRRScreen *screen = priv->rw_screen;
        GsdRRConfig *current;
        GsdRROutputInfo *laptop_output;

        if (!supports_xinput_devices ())
                return;

        current = gsd_rr_config_new_current (screen, NULL);
        laptop_output = get_mappable_output_info (manager, screen, current);

        if (laptop_output == NULL) {
                g_debug ("No laptop screen detected");
                goto out;
        }

        if (priv->main_touchscreen_id != -1) {
                /* Set initial mapping */
                g_debug ("Setting initial touchscreen mapping");
                map_touch_to_output (screen,
                                     priv->main_touchscreen_id,
                                     laptop_output);
        }
        else {
                g_debug ("No main touchscreen detected");
        }

out:
        g_object_unref (current);
}

gboolean
gsd_xrandr_manager_start (GsdXrandrManager *manager,
                          GError          **error)
{
        g_debug ("Starting xrandr manager");
        gnome_settings_profile_start (NULL);

        log_open ();
        log_msg ("------------------------------------------------------------\nSTARTING XRANDR PLUGIN\n");

        manager->priv->rw_screen = gsd_rr_screen_new (gdk_screen_get_default (), error);

        if (manager->priv->rw_screen == NULL) {
                log_msg ("Could not initialize the RANDR plugin%s%s\n",
                         (error && *error) ? ": " : "",
                         (error && *error) ? (*error)->message : "");
                log_close ();
                return FALSE;
        }

        g_signal_connect (manager->priv->rw_screen, "changed", G_CALLBACK (on_randr_event), manager);

        manager->priv->upower_client = up_client_new ();
        manager->priv->laptop_lid_is_closed = up_client_get_lid_is_closed (manager->priv->upower_client);
        g_signal_connect (manager->priv->upower_client, "notify::lid-is-closed",
                          G_CALLBACK (lid_state_changed_cb), manager);

        log_msg ("State of screen at startup:\n");
        log_screen (manager->priv->rw_screen);

        manager->priv->running = TRUE;
        manager->priv->settings = g_settings_new (CONF_SCHEMA);

        show_timestamps_dialog (manager, "Startup");
        if (!apply_stored_configuration_at_startup (manager, GDK_CURRENT_TIME)) /* we don't have a real timestamp at startup anyway */
                if (!apply_default_configuration_from_file (manager, GDK_CURRENT_TIME))
                        apply_default_boot_configuration (manager, GDK_CURRENT_TIME);

        /* Initialise touchscreen mapping */
        set_touchscreen_id (manager);
        if (manager->priv->main_touchscreen_id != -1)
                do_touchscreen_mapping (manager);

        log_msg ("State of screen after initial configuration:\n");
        log_screen (manager->priv->rw_screen);

        log_close ();

        gnome_settings_profile_end (NULL);

        return TRUE;
}

void
gsd_xrandr_manager_stop (GsdXrandrManager *manager)
{
        g_debug ("Stopping xrandr manager");

        manager->priv->running = FALSE;

        if (manager->priv->bus_cancellable != NULL) {
                g_cancellable_cancel (manager->priv->bus_cancellable);
                g_object_unref (manager->priv->bus_cancellable);
                manager->priv->bus_cancellable = NULL;
        }

        if (manager->priv->settings != NULL) {
                g_object_unref (manager->priv->settings);
                manager->priv->settings = NULL;
        }

        if (manager->priv->rw_screen != NULL) {
                g_object_unref (manager->priv->rw_screen);
                manager->priv->rw_screen = NULL;
        }

        if (manager->priv->upower_client != NULL) {
                g_signal_handlers_disconnect_by_data (manager->priv->upower_client, manager);
                g_object_unref (manager->priv->upower_client);
                manager->priv->upower_client = NULL;
        }

        if (manager->priv->introspection_data) {
                g_dbus_node_info_unref (manager->priv->introspection_data);
                manager->priv->introspection_data = NULL;
        }

        if (manager->priv->connection != NULL) {
                g_object_unref (manager->priv->connection);
                manager->priv->connection = NULL;
        }

#ifdef HAVE_WACOM
        if (manager->priv->wacom_db != NULL) {
                libwacom_database_destroy (manager->priv->wacom_db);
                manager->priv->wacom_db = NULL;
        }
#endif /* HAVE_WACOM */

        g_free (manager->priv->main_touchscreen_name);

        log_open ();
        log_msg ("STOPPING XRANDR PLUGIN\n------------------------------------------------------------\n");
        log_close ();
}

static void
gsd_xrandr_manager_class_init (GsdXrandrManagerClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gsd_xrandr_manager_finalize;

        g_type_class_add_private (klass, sizeof (GsdXrandrManagerPrivate));
}

static void
gsd_xrandr_manager_init (GsdXrandrManager *manager)
{
        manager->priv = GSD_XRANDR_MANAGER_GET_PRIVATE (manager);

        manager->priv->current_fn_f7_config = -1;
        manager->priv->fn_f7_configs = NULL;

        /* For touchscreen mapping */
        manager->priv->main_touchscreen_id = -1;
        manager->priv->main_touchscreen_name = NULL;
}

static void
gsd_xrandr_manager_finalize (GObject *object)
{
        GsdXrandrManager *manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GSD_IS_XRANDR_MANAGER (object));

        manager = GSD_XRANDR_MANAGER (object);

        g_return_if_fail (manager->priv != NULL);

        if (manager->priv->name_id != 0)
                g_bus_unown_name (manager->priv->name_id);

        G_OBJECT_CLASS (gsd_xrandr_manager_parent_class)->finalize (object);
}

static guint32
clamp_timestamp (gint64 timestamp)
{
        if (timestamp < 0)
                return 0;
        if (timestamp > G_MAXUINT32)
                return G_MAXUINT32;
        return timestamp;
}

static void
handle_method_call_xrandr_2 (GsdXrandrManager *manager,
                             const gchar *method_name,
                             GVariant *parameters,
                             GDBusMethodInvocation *invocation)
{
        gint64 timestamp;
        GError *error = NULL;

        g_debug ("Calling method '%s' for org.gnome.SettingsDaemon.XRANDR_2", method_name);

        if (g_strcmp0 (method_name, "ApplyConfiguration") == 0) {
                gint64 parent_window_id;

                g_variant_get (parameters, "(xx)", &parent_window_id, &timestamp);
                if (gsd_xrandr_manager_2_apply_configuration (manager, parent_window_id,
                                                              clamp_timestamp(timestamp), &error) == FALSE) {
                        g_dbus_method_invocation_return_gerror (invocation, error);
                } else {
                        g_dbus_method_invocation_return_value (invocation, NULL);
                }
        } else if (g_strcmp0 (method_name, "VideoModeSwitch") == 0) {
                g_variant_get (parameters, "(x)", &timestamp);
                gsd_xrandr_manager_2_video_mode_switch (manager, clamp_timestamp(timestamp), NULL);
                g_dbus_method_invocation_return_value (invocation, NULL);
        } else if (g_strcmp0 (method_name, "Rotate") == 0) {
                g_variant_get (parameters, "(x)", &timestamp);
                gsd_xrandr_manager_2_rotate (manager, clamp_timestamp(timestamp), NULL);
                g_dbus_method_invocation_return_value (invocation, NULL);
        } else if (g_strcmp0 (method_name, "RotateTo") == 0) {
                GsdRRRotation rotation;
                g_variant_get (parameters, "(ix)", &rotation, &timestamp);
                gsd_xrandr_manager_2_rotate_to (manager, rotation, clamp_timestamp(timestamp), NULL);
                g_dbus_method_invocation_return_value (invocation, NULL);
        }
}

static void
handle_method_call (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *interface_name,
                    const gchar           *method_name,
                    GVariant              *parameters,
                    GDBusMethodInvocation *invocation,
                    gpointer               user_data)
{
        GsdXrandrManager *manager = (GsdXrandrManager *) user_data;

        g_debug ("Handling method call %s.%s", interface_name, method_name);

        if (g_strcmp0 (interface_name, "org.gnome.SettingsDaemon.XRANDR_2") == 0)
                handle_method_call_xrandr_2 (manager, method_name, parameters, invocation);
        else
                g_warning ("unknown interface: %s", interface_name);
}


static const GDBusInterfaceVTable interface_vtable =
{
        handle_method_call,
        NULL, /* Get Property */
        NULL, /* Set Property */
};

static void
on_bus_gotten (GObject             *source_object,
               GAsyncResult        *res,
               GsdXrandrManager    *manager)
{
        GDBusConnection *connection;
        GError *error = NULL;
        GDBusInterfaceInfo **infos;
        int i;

        if (manager->priv->bus_cancellable == NULL ||
            g_cancellable_is_cancelled (manager->priv->bus_cancellable)) {
                g_warning ("Operation has been cancelled, so not retrieving session bus");
                return;
        }

        connection = g_bus_get_finish (res, &error);
        if (connection == NULL) {
                g_warning ("Could not get session bus: %s", error->message);
                g_error_free (error);
                return;
        }
        manager->priv->connection = connection;

        infos = manager->priv->introspection_data->interfaces;
        for (i = 0; infos[i] != NULL; i++) {
                g_dbus_connection_register_object (connection,
                                                   GSD_XRANDR_DBUS_PATH,
                                                   infos[i],
                                                   &interface_vtable,
                                                   manager,
                                                   NULL,
                                                   NULL);
        }

        manager->priv->name_id = g_bus_own_name_on_connection (connection,
                                                               GSD_XRANDR_DBUS_NAME,
                                                               G_BUS_NAME_OWNER_FLAGS_NONE,
                                                               NULL,
                                                               NULL,
                                                               NULL,
                                                               NULL);
}

static void
register_manager_dbus (GsdXrandrManager *manager)
{
        manager->priv->introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);
        manager->priv->bus_cancellable = g_cancellable_new ();
        g_assert (manager->priv->introspection_data != NULL);

        g_bus_get (G_BUS_TYPE_SESSION,
                   manager->priv->bus_cancellable,
                   (GAsyncReadyCallback) on_bus_gotten,
                   manager);
}

GsdXrandrManager *
gsd_xrandr_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (GSD_TYPE_XRANDR_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);

                register_manager_dbus (manager_object);
        }

        return GSD_XRANDR_MANAGER (manager_object);
}
