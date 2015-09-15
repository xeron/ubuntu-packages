/**
 * Create a test app for your plugin quickly.
 *
 * #define NEW gsd_media_keys_manager_new
 * #define START gsd_media_keys_manager_start
 * #define MANAGER GsdMediaKeysManager
 * #include "gsd-media-keys-manager.h"
 *
 * #include "test-plugin.h"
 */

#include "config.h"

#include <stdlib.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libnotify/notify.h>

#ifndef SCHEMA_NAME
#define SCHEMA_NAME PLUGIN_NAME
#endif

#ifndef PLUGIN_NAME
#error Include PLUGIN_CFLAGS in the test application s CFLAGS
#endif /* !PLUGIN_NAME */

static MANAGER *manager = NULL;

static gboolean
has_settings (void)
{
	const gchar * const * list;
	guint i;

	list = g_settings_list_schemas ();
	for (i = 0; list[i] != NULL; i++) {
		if (g_str_equal (list[i], "org.gnome.settings-daemon.plugins." SCHEMA_NAME))
			return TRUE;
	}
	return FALSE;
}

static void
print_enable_disable_help (void)
{
	fprintf (stderr, "To deactivate:\n");
	fprintf (stderr, "\tgsettings set org.gnome.settings-daemon.plugins." SCHEMA_NAME " active false\n");
	fprintf (stderr, "To reactivate:\n");
	fprintf (stderr, "\tgsettings set org.gnome.settings-daemon.plugins." SCHEMA_NAME " active true\n");
}

int
main (int argc, char **argv)
{
        GError  *error;
        GSettings *settings;

        bindtextdomain (GETTEXT_PACKAGE, GNOME_SETTINGS_LOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);
        notify_init ("gnome-settings-daemon");

	g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);

        error = NULL;
        if (! gtk_init_with_args (&argc, &argv, NULL, NULL, NULL, &error)) {
                fprintf (stderr, "%s\n", error->message);
                g_error_free (error);
                exit (1);
        }

	if (has_settings () == FALSE) {
		fprintf (stderr, "The schemas for plugin '%s' isn't available, check your installation.\n", SCHEMA_NAME);
	} else {
		settings = g_settings_new ("org.gnome.settings-daemon.plugins." SCHEMA_NAME);
		if (g_settings_get_boolean (settings, "active") != FALSE) {
			fprintf (stderr, "Plugin '%s' is not disabled. You need to disable it before launching the test application.\n", SCHEMA_NAME);
			print_enable_disable_help ();
			exit (1);
		}
		print_enable_disable_help();
	}

        manager = NEW ();

        error = NULL;
        START (manager, &error);

        gtk_main ();

        STOP (manager);
        g_object_unref (manager);

        return 0;
}
