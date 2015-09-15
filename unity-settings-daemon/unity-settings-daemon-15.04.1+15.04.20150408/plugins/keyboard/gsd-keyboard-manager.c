/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright © 2001 Ximian, Inc.
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 * Written by Sergey V. Oudaltsov <svu@users.sourceforge.net>
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
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <X11/keysym.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-languages.h>
#include <libgnome-desktop/gnome-xkb-info.h>

#ifdef HAVE_IBUS
#include <ibus.h>
#endif

#ifdef HAVE_FCITX
#include <fcitx-config/fcitx-config.h>
#include <fcitx-gclient/fcitxinputmethod.h>
#endif

#include <act/act.h>

#include "gnome-settings-session.h"
#include "gnome-settings-profile.h"
#include "gsd-keyboard-manager.h"
#include "gsd-input-helper.h"
#include "gsd-enums.h"
#include "gsd-xkb-utils.h"

#ifdef HAVE_FCITX
#include "input-method-engines.c"
#endif

#define GSD_KEYBOARD_MANAGER_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GSD_TYPE_KEYBOARD_MANAGER, GsdKeyboardManagerPrivate))

#define GSD_KEYBOARD_DIR "org.gnome.settings-daemon.peripherals.keyboard"

#define KEY_REPEAT         "repeat"
#define KEY_CLICK          "click"
#define KEY_INTERVAL       "repeat-interval"
#define KEY_DELAY          "delay"
#define KEY_CLICK_VOLUME   "click-volume"
#define KEY_REMEMBER_NUMLOCK_STATE "remember-numlock-state"
#define KEY_NUMLOCK_STATE  "numlock-state"

#define KEY_BELL_VOLUME    "bell-volume"
#define KEY_BELL_PITCH     "bell-pitch"
#define KEY_BELL_DURATION  "bell-duration"
#define KEY_BELL_MODE      "bell-mode"

#define GNOME_DESKTOP_INTERFACE_DIR "org.gnome.desktop.interface"

#define ENV_GTK_IM_MODULE    "GTK_IM_MODULE"
#define KEY_GTK_IM_MODULE    "gtk-im-module"
#define GTK_IM_MODULE_SIMPLE "gtk-im-context-simple"
#define GTK_IM_MODULE_IBUS   "ibus"
#define GTK_IM_MODULE_FCITX  "fcitx"

#define GNOME_DESKTOP_INPUT_SOURCES_DIR "org.gnome.desktop.input-sources"

#define KEY_CURRENT_INPUT_SOURCE "current"
#define KEY_INPUT_SOURCES        "sources"
#define KEY_KEYBOARD_OPTIONS     "xkb-options"

#define INPUT_SOURCE_TYPE_XKB   "xkb"
#define INPUT_SOURCE_TYPE_IBUS  "ibus"
#define INPUT_SOURCE_TYPE_FCITX "fcitx"

#define FCITX_XKB_PREFIX "fcitx-keyboard-"

#define DEFAULT_LANGUAGE "en_US"
#define DEFAULT_LAYOUT "us"

#define GSD_KEYBOARD_DBUS_NAME  "org.gnome.SettingsDaemon.Keyboard"
#define GSD_KEYBOARD_DBUS_PATH  "/org/gnome/SettingsDaemon/Keyboard"

struct GsdKeyboardManagerPrivate
{
	guint      start_idle_id;
        GSettings *settings;
        GSettings *input_sources_settings;
        GSettings *interface_settings;
        GnomeXkbInfo *xkb_info;
        GDBusProxy *localed;
        GCancellable *cancellable;
#ifdef HAVE_IBUS
        IBusBus   *ibus;
        GHashTable *ibus_engines;
        GCancellable *ibus_cancellable;
        gboolean is_ibus_active;
#endif
#ifdef HAVE_FCITX
        FcitxInputMethod *fcitx;
        GCancellable *fcitx_cancellable;
        gulong fcitx_signal_id;
        gboolean is_fcitx_active;
#endif
        gint       xkb_event_base;
        GsdNumLockState old_state;
        GdkDeviceManager *device_manager;
        guint device_added_id;
        guint device_removed_id;

        GDBusConnection *dbus_connection;
        GDBusNodeInfo *dbus_introspection;
        guint dbus_own_name_id;
        GSList *dbus_register_object_ids;

        GDBusMethodInvocation *invocation;
        guint pending_ops;

        gint active_input_source;
};

static void     gsd_keyboard_manager_class_init  (GsdKeyboardManagerClass *klass);
static void     gsd_keyboard_manager_init        (GsdKeyboardManager      *keyboard_manager);
static void     gsd_keyboard_manager_finalize    (GObject                 *object);
static gboolean apply_input_sources_settings     (GSettings               *settings,
                                                  gpointer                 keys,
                                                  gint                     n_keys,
                                                  GsdKeyboardManager      *manager);
static void     set_gtk_im_module                (GsdKeyboardManager      *manager,
                                                  GVariant                *sources);
static void     maybe_return_from_set_input_source (GsdKeyboardManager    *manager);
static void     increment_set_input_source_ops   (GsdKeyboardManager      *manager);

G_DEFINE_TYPE (GsdKeyboardManager, gsd_keyboard_manager, G_TYPE_OBJECT)

static const gchar introspection_xml[] =
        "<node>"
        "  <interface name='org.gnome.SettingsDaemon.Keyboard'>"
        "    <method name='SetInputSource'>"
        "      <arg type='u' name='idx' direction='in'/>"
        "    </method>"
        "  </interface>"
        /* Ubuntu-specific */
        "  <interface name='com.canonical.SettingsDaemon.Keyboard.Private'>"
        "    <method name='ActivateInputSource'>"
        "      <arg type='u' name='idx' direction='in'/>"
        "    </method>"
        "  </interface>"
        "</node>";

static gpointer manager_object = NULL;

static guint group_position = 0;

static void
init_builder_with_sources (GVariantBuilder *builder,
                           GSettings       *settings)
{
        const gchar *type;
        const gchar *id;
        GVariantIter iter;
        GVariant *sources;

        sources = g_settings_get_value (settings, KEY_INPUT_SOURCES);

        g_variant_builder_init (builder, G_VARIANT_TYPE ("a(ss)"));

        g_variant_iter_init (&iter, sources);
        while (g_variant_iter_next (&iter, "(&s&s)", &type, &id))
                g_variant_builder_add (builder, "(ss)", type, id);

        g_variant_unref (sources);
}

static gboolean
schema_is_installed (const gchar *name)
{
        const gchar * const *schemas;
        const gchar * const *s;

        schemas = g_settings_list_schemas ();
        for (s = schemas; *s; ++s)
                if (g_str_equal (*s, name))
                        return TRUE;

        return FALSE;
}

#ifdef HAVE_IBUS
static void
clear_ibus (GsdKeyboardManager *manager)
{
        GsdKeyboardManagerPrivate *priv = manager->priv;

        g_cancellable_cancel (priv->ibus_cancellable);
        g_clear_object (&priv->ibus_cancellable);
        g_clear_pointer (&priv->ibus_engines, g_hash_table_destroy);
        g_clear_object (&priv->ibus);
}

static void
fetch_ibus_engines_result (GObject            *object,
                           GAsyncResult       *result,
                           GsdKeyboardManager *manager)
{
        GsdKeyboardManagerPrivate *priv = manager->priv;
        GList *list, *l;
        GError *error = NULL;

        /* engines shouldn't be there yet */
        g_return_if_fail (priv->ibus_engines == NULL);

        g_clear_object (&priv->ibus_cancellable);

        list = ibus_bus_list_engines_async_finish (priv->ibus,
                                                   result,
                                                   &error);
        if (!list && error) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Couldn't finish IBus request: %s", error->message);
                g_error_free (error);

                clear_ibus (manager);
                return;
        }

        /* Maps IBus engine ids to engine description objects */
        priv->ibus_engines = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_object_unref);

        for (l = list; l; l = l->next) {
                IBusEngineDesc *engine = l->data;
                const gchar *engine_id = ibus_engine_desc_get_name (engine);

                g_hash_table_replace (priv->ibus_engines, (gpointer)engine_id, engine);
        }
        g_list_free (list);

        apply_input_sources_settings (priv->input_sources_settings, NULL, 0, manager);
}

static void
fetch_ibus_engines (GsdKeyboardManager *manager)
{
        GsdKeyboardManagerPrivate *priv = manager->priv;

        /* engines shouldn't be there yet */
        g_return_if_fail (priv->ibus_engines == NULL);
        g_return_if_fail (priv->ibus_cancellable == NULL);

        priv->ibus_cancellable = g_cancellable_new ();

        ibus_bus_list_engines_async (priv->ibus,
                                     -1,
                                     priv->ibus_cancellable,
                                     (GAsyncReadyCallback)fetch_ibus_engines_result,
                                     manager);
}

static void
maybe_start_ibus (GsdKeyboardManager *manager)
{
        if (!manager->priv->ibus) {
                ibus_init ();
                manager->priv->ibus = ibus_bus_new_async ();
                g_signal_connect_swapped (manager->priv->ibus, "connected",
                                          G_CALLBACK (fetch_ibus_engines), manager);
                g_signal_connect_swapped (manager->priv->ibus, "disconnected",
                                          G_CALLBACK (clear_ibus), manager);
        }
        /* IBus doesn't export API in the session bus. The only thing
         * we have there is a well known name which we can use as a
         * sure-fire way to activate it. */
        g_bus_unwatch_name (g_bus_watch_name (G_BUS_TYPE_SESSION,
                                              IBUS_SERVICE_IBUS,
                                              G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
                                              NULL,
                                              NULL,
                                              NULL,
                                              NULL));
}

static void
set_ibus_engine_finish (GObject            *object,
                        GAsyncResult       *res,
                        GsdKeyboardManager *manager)
{
        gboolean result;
        IBusBus *ibus = IBUS_BUS (object);
        GsdKeyboardManagerPrivate *priv = manager->priv;
        GError *error = NULL;

        g_clear_object (&priv->ibus_cancellable);

        result = ibus_bus_set_global_engine_async_finish (ibus, res, &error);
        if (!result) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Couldn't set IBus engine: %s", error->message);
                g_error_free (error);
                return;
        }

        maybe_return_from_set_input_source (manager);
}

static void
set_ibus_engine (GsdKeyboardManager *manager,
                 const gchar        *engine_id)
{
        GsdKeyboardManagerPrivate *priv = manager->priv;

        g_return_if_fail (priv->ibus != NULL);
        g_return_if_fail (priv->ibus_engines != NULL);

        g_cancellable_cancel (priv->ibus_cancellable);
        g_clear_object (&priv->ibus_cancellable);
        priv->ibus_cancellable = g_cancellable_new ();

        increment_set_input_source_ops (manager);

        ibus_bus_set_global_engine_async (priv->ibus,
                                          engine_id,
                                          -1,
                                          priv->ibus_cancellable,
                                          (GAsyncReadyCallback)set_ibus_engine_finish,
                                          manager);
}

static void
set_ibus_xkb_engine (GsdKeyboardManager *manager)
{
        IBusEngineDesc *engine;
        GsdKeyboardManagerPrivate *priv = manager->priv;

        if (!priv->ibus_engines)
                return;

        /* All the "xkb:..." IBus engines simply "echo" back symbols,
           despite their naming implying differently, so we always set
           one in order for XIM applications to work given that we set
           XMODIFIERS=@im=ibus in the first place so that they can
           work without restarting when/if the user adds an IBus
           input source. */
        engine = g_hash_table_lookup (priv->ibus_engines, "xkb:us::eng");
        if (!engine)
                return;

        set_ibus_engine (manager, ibus_engine_desc_get_name (engine));
}

static gboolean
need_ibus (GVariant *sources)
{
        GVariantIter iter;
        const gchar *type;

        g_variant_iter_init (&iter, sources);
        while (g_variant_iter_next (&iter, "(&s&s)", &type, NULL))
                if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS))
                        return TRUE;

        return FALSE;
}


static void
set_gtk_im_module (GsdKeyboardManager *manager,
                   GVariant           *sources)
{
        GsdKeyboardManagerPrivate *priv = manager->priv;
        const gchar *new_module;
        gchar *current_module;

        if (!sources || need_ibus (sources))
                new_module = GTK_IM_MODULE_IBUS;
        else
                new_module = GTK_IM_MODULE_SIMPLE;

        current_module = g_settings_get_string (priv->interface_settings,
                                                KEY_GTK_IM_MODULE);
        if (!g_str_equal (current_module, new_module))
                g_settings_set_string (priv->interface_settings,
                                       KEY_GTK_IM_MODULE,
                                       new_module);
        g_free (current_module);
}

/* XXX: See upstream bug:
 * https://codereview.appspot.com/6586075/ */
static gchar *
layout_from_ibus_layout (const gchar *ibus_layout)
{
        const gchar *p;

        /* we get something like "layout(variant)[option1,option2]" */

        p = ibus_layout;
        while (*p) {
                if (*p == '(' || *p == '[')
                        break;
                p += 1;
        }

        return g_strndup (ibus_layout, p - ibus_layout);
}

static gchar *
variant_from_ibus_layout (const gchar *ibus_layout)
{
        const gchar *a, *b;

        /* we get something like "layout(variant)[option1,option2]" */

        a = ibus_layout;
        while (*a) {
                if (*a == '(')
                        break;
                a += 1;
        }
        if (!*a)
                return NULL;

        a += 1;
        b = a;
        while (*b) {
                if (*b == ')')
                        break;
                b += 1;
        }
        if (!*b)
                return NULL;

        return g_strndup (a, b - a);
}

static gchar **
options_from_ibus_layout (const gchar *ibus_layout)
{
        const gchar *a, *b;
        GPtrArray *opt_array;

        /* we get something like "layout(variant)[option1,option2]" */

        a = ibus_layout;
        while (*a) {
                if (*a == '[')
                        break;
                a += 1;
        }
        if (!*a)
                return NULL;

        opt_array = g_ptr_array_new ();

        do {
                a += 1;
                b = a;
                while (*b) {
                        if (*b == ',' || *b == ']')
                                break;
                        b += 1;
                }
                if (!*b)
                        goto out;

                g_ptr_array_add (opt_array, g_strndup (a, b - a));

                a = b;
        } while (*a && *a == ',');

out:
        g_ptr_array_add (opt_array, NULL);
        return (gchar **) g_ptr_array_free (opt_array, FALSE);
}

static const gchar *
engine_from_locale (void)
{
        const gchar *locale;
        const gchar *locale_engine[][2] = {
                { "as_IN", "m17n:as:phonetic" },
                { "bn_IN", "m17n:bn:inscript" },
                { "gu_IN", "m17n:gu:inscript" },
                { "hi_IN", "m17n:hi:inscript" },
                { "ja_JP", "anthy" },
                { "kn_IN", "m17n:kn:kgp" },
                { "ko_KR", "hangul" },
                { "mai_IN", "m17n:mai:inscript" },
                { "ml_IN", "m17n:ml:inscript" },
                { "mr_IN", "m17n:mr:inscript" },
                { "or_IN", "m17n:or:inscript" },
                { "pa_IN", "m17n:pa:inscript" },
                { "sd_IN", "m17n:sd:inscript" },
                { "ta_IN", "m17n:ta:tamil99" },
                { "te_IN", "m17n:te:inscript" },
                { "zh_CN", "pinyin" },
                { "zh_HK", "cangjie3" },
                { "zh_TW", "chewing" },
        };
        gint i;

        locale = setlocale (LC_CTYPE, NULL);
        if (!locale)
                return NULL;

        for (i = 0; i < G_N_ELEMENTS (locale_engine); ++i)
                if (g_str_has_prefix (locale, locale_engine[i][0]))
                        return locale_engine[i][1];

        return NULL;
}

static void
add_ibus_sources_from_locale (GSettings *settings)
{
        const gchar *locale_engine;
        GVariantBuilder builder;

        locale_engine = engine_from_locale ();
        if (!locale_engine)
                return;

        init_builder_with_sources (&builder, settings);
        g_variant_builder_add (&builder, "(ss)", INPUT_SOURCE_TYPE_IBUS, locale_engine);
        g_settings_set_value (settings, KEY_INPUT_SOURCES, g_variant_builder_end (&builder));
}

static void
convert_ibus (GSettings *settings)
{
        GVariantBuilder builder;
        GSettings *ibus_settings;
        gchar **engines, **e;

        if (!schema_is_installed ("org.freedesktop.ibus.general"))
                return;

        init_builder_with_sources (&builder, settings);

        ibus_settings = g_settings_new ("org.freedesktop.ibus.general");
        engines = g_settings_get_strv (ibus_settings, "preload-engines");
        for (e = engines; *e; ++e) {
                if (g_str_has_prefix (*e, "xkb:"))
                        continue;
                g_variant_builder_add (&builder, "(ss)", INPUT_SOURCE_TYPE_IBUS, *e);
        }

        g_settings_set_value (settings, KEY_INPUT_SOURCES, g_variant_builder_end (&builder));

        g_strfreev (engines);
        g_object_unref (ibus_settings);
}
#endif  /* HAVE_IBUS */

static gboolean
xkb_set_keyboard_autorepeat_rate (guint delay, guint interval)
{
        return XkbSetAutoRepeatRate (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                     XkbUseCoreKbd,
                                     delay,
                                     interval);
}

static gboolean
check_xkb_extension (GsdKeyboardManager *manager)
{
        Display *dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
        int opcode, error_base, major, minor;
        gboolean have_xkb;

        have_xkb = XkbQueryExtension (dpy,
                                      &opcode,
                                      &manager->priv->xkb_event_base,
                                      &error_base,
                                      &major,
                                      &minor);
        return have_xkb;
}

static void
xkb_init (GsdKeyboardManager *manager)
{
        Display *dpy;

        dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
        XkbSelectEventDetails (dpy,
                               XkbUseCoreKbd,
                               XkbStateNotify,
                               XkbModifierLockMask,
                               XkbModifierLockMask);
}

static unsigned
numlock_NumLock_modifier_mask (void)
{
        Display *dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
        return XkbKeysymToModifiers (dpy, XK_Num_Lock);
}

static void
numlock_set_xkb_state (GsdNumLockState new_state)
{
        unsigned int num_mask;
        Display *dpy = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
        if (new_state != GSD_NUM_LOCK_STATE_ON && new_state != GSD_NUM_LOCK_STATE_OFF)
                return;
        num_mask = numlock_NumLock_modifier_mask ();
        XkbLockModifiers (dpy, XkbUseCoreKbd, num_mask, new_state == GSD_NUM_LOCK_STATE_ON ? num_mask : 0);
}

static const char *
num_lock_state_to_string (GsdNumLockState numlock_state)
{
	switch (numlock_state) {
	case GSD_NUM_LOCK_STATE_UNKNOWN:
		return "GSD_NUM_LOCK_STATE_UNKNOWN";
	case GSD_NUM_LOCK_STATE_ON:
		return "GSD_NUM_LOCK_STATE_ON";
	case GSD_NUM_LOCK_STATE_OFF:
		return "GSD_NUM_LOCK_STATE_OFF";
	default:
		return "UNKNOWN";
	}
}

static GdkFilterReturn
xkb_events_filter (GdkXEvent *xev_,
		   GdkEvent  *gdkev_,
		   gpointer   user_data)
{
        XEvent *xev = (XEvent *) xev_;
	XkbEvent *xkbev = (XkbEvent *) xev;
        GsdKeyboardManager *manager = (GsdKeyboardManager *) user_data;

        if (xev->type != manager->priv->xkb_event_base ||
            xkbev->any.xkb_type != XkbStateNotify)
		return GDK_FILTER_CONTINUE;

	if (xkbev->state.changed & XkbModifierLockMask) {
		unsigned num_mask = numlock_NumLock_modifier_mask ();
		unsigned locked_mods = xkbev->state.locked_mods;
		GsdNumLockState numlock_state;

		numlock_state = (num_mask & locked_mods) ? GSD_NUM_LOCK_STATE_ON : GSD_NUM_LOCK_STATE_OFF;

		if (numlock_state != manager->priv->old_state) {
			g_debug ("New num-lock state '%s' != Old num-lock state '%s'",
				 num_lock_state_to_string (numlock_state),
				 num_lock_state_to_string (manager->priv->old_state));
			g_settings_set_enum (manager->priv->settings,
					     KEY_NUMLOCK_STATE,
					     numlock_state);
			manager->priv->old_state = numlock_state;
		}
	}

        return GDK_FILTER_CONTINUE;
}

static void
install_xkb_filter (GsdKeyboardManager *manager)
{
        gdk_window_add_filter (NULL,
                               xkb_events_filter,
                               manager);
}

static void
remove_xkb_filter (GsdKeyboardManager *manager)
{
        gdk_window_remove_filter (NULL,
                                  xkb_events_filter,
                                  manager);
}

static void
free_xkb_component_names (XkbComponentNamesRec *p)
{
        g_return_if_fail (p != NULL);

        free (p->keymap);
        free (p->keycodes);
        free (p->types);
        free (p->compat);
        free (p->symbols);
        free (p->geometry);

        g_free (p);
}

static void
upload_xkb_description (const gchar          *rules_file_path,
                        XkbRF_VarDefsRec     *var_defs,
                        XkbComponentNamesRec *comp_names)
{
        Display *display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
        XkbDescRec *xkb_desc;
        gchar *rules_file;

        /* The index of layout we want is saved in the group_position
         * variable. */
        XkbLockGroup (display, XkbUseCoreKbd, group_position);

        /* Upload it to the X server using the same method as setxkbmap */
        xkb_desc = XkbGetKeyboardByName (display,
                                         XkbUseCoreKbd,
                                         comp_names,
                                         XkbGBN_AllComponentsMask,
                                         XkbGBN_AllComponentsMask &
                                         (~XkbGBN_GeometryMask), True);
        if (!xkb_desc) {
                g_warning ("Couldn't upload new XKB keyboard description");
                return;
        }

        XkbFreeKeyboard (xkb_desc, 0, True);

        rules_file = g_path_get_basename (rules_file_path);

        if (!XkbRF_SetNamesProp (display, rules_file, var_defs))
                g_warning ("Couldn't update the XKB root window property");

        g_free (rules_file);
}

static gchar *
build_xkb_group_string (const gchar *user,
                        const gchar *locale,
                        const gchar *latin)
{
        gchar *string;
        gsize length = 0;
        guint commas = 2;

        if (latin) {
                length += strlen (latin);
                group_position = 1;
        } else {
                commas -= 1;
                group_position = 0;
        }

        if (locale)
                length += strlen (locale);
        else
                commas -= 1;

        length += strlen (user) + commas + 1;

        string = malloc (length);

        if (locale && latin)
                sprintf (string, "%s,%s,%s", latin, user, locale);
        else if (locale)
                sprintf (string, "%s,%s", user, locale);
        else if (latin)
                sprintf (string, "%s,%s", latin, user);
        else
                sprintf (string, "%s", user);

        return string;
}

static gboolean
layout_equal (const gchar *layout_a,
              const gchar *variant_a,
              const gchar *layout_b,
              const gchar *variant_b)
{
        return !g_strcmp0 (layout_a, layout_b) && !g_strcmp0 (variant_a, variant_b);
}

static void
get_locale_layout (GsdKeyboardManager  *manager,
                   const gchar        **layout,
                   const gchar        **variant)
{
        const gchar *locale;
        const gchar *type;
        const gchar *id;
        gboolean got_info;

        *layout = NULL;
        *variant = NULL;

        locale = setlocale (LC_MESSAGES, NULL);
        /* If LANG is empty, default to en_US */
        if (!locale)
                locale = DEFAULT_LANGUAGE;

        got_info = gnome_get_input_source_from_locale (locale, &type, &id);
        if (!got_info)
                if (!gnome_get_input_source_from_locale (DEFAULT_LANGUAGE, &type, &id))
                        return;

        if (!g_str_equal (type, INPUT_SOURCE_TYPE_XKB))
                return;

        gnome_xkb_info_get_layout_info (manager->priv->xkb_info,
                                        id,
                                        NULL,
                                        NULL,
                                        layout,
                                        variant);
}

static void
replace_layout_and_variant (GsdKeyboardManager *manager,
                            XkbRF_VarDefsRec   *xkb_var_defs,
                            const gchar        *layout,
                            const gchar        *variant)
{
        /* Toolkits need to know about both a latin layout to handle
         * accelerators which are usually defined like Ctrl+C and a
         * layout with the symbols for the language used in UI strings
         * to handle mnemonics like Alt+Ф, so we try to find and add
         * them in XKB group slots after the layout which the user
         * actually intends to type with. */
        const gchar *latin_layout = "us";
        const gchar *latin_variant = "";
        const gchar *locale_layout = NULL;
        const gchar *locale_variant = NULL;

        if (!layout)
                return;

        if (!variant)
                variant = "";

        get_locale_layout (manager, &locale_layout, &locale_variant);

        /* We want to minimize the number of XKB groups if we have
         * duplicated layout+variant pairs.
         *
         * Also, if a layout doesn't have a variant we still have to
         * include it in the variants string because the number of
         * variants must agree with the number of layouts. For
         * instance:
         *
         * layouts:  "us,ru,us"
         * variants: "dvorak,,"
         */
        if (layout_equal (latin_layout, latin_variant, locale_layout, locale_variant) ||
            layout_equal (latin_layout, latin_variant, layout, variant)) {
                latin_layout = NULL;
                latin_variant = NULL;
        }

        if (layout_equal (locale_layout, locale_variant, layout, variant)) {
                locale_layout = NULL;
                locale_variant = NULL;
        }

        free (xkb_var_defs->layout);
        xkb_var_defs->layout = build_xkb_group_string (layout, locale_layout, latin_layout);

        free (xkb_var_defs->variant);
        xkb_var_defs->variant = build_xkb_group_string (variant, locale_variant, latin_variant);
}

static gchar *
build_xkb_options_string (gchar **options)
{
        gchar *string;

        if (*options) {
                gint i;
                gsize len;
                gchar *ptr;

                /* First part, getting length */
                len = 1 + strlen (options[0]);
                for (i = 1; options[i] != NULL; i++)
                        len += strlen (options[i]);
                len += (i - 1); /* commas */

                /* Second part, building string */
                string = malloc (len);
                ptr = g_stpcpy (string, *options);
                for (i = 1; options[i] != NULL; i++) {
                        ptr = g_stpcpy (ptr, ",");
                        ptr = g_stpcpy (ptr, options[i]);
                }
        } else {
                string = malloc (1);
                *string = '\0';
        }

        return string;
}

static gchar **
append_options (gchar **a,
                gchar **b)
{
        gchar **c, **p;

        if (!a && !b)
                return NULL;
        else if (!a)
                return g_strdupv (b);
        else if (!b)
                return g_strdupv (a);

        c = g_new0 (gchar *, g_strv_length (a) + g_strv_length (b) + 1);
        p = c;

        while (*a) {
                *p = g_strdup (*a);
                p += 1;
                a += 1;
        }
        while (*b) {
                *p = g_strdup (*b);
                p += 1;
                b += 1;
        }

        return c;
}

static void
strip_xkb_option (gchar       **options,
                  const gchar  *prefix)
{
        guint last;
        gchar **p = options;

        if (!p)
                return;

        while (*p) {
                if (g_str_has_prefix (*p, prefix)) {
                        last = g_strv_length (options) - 1;
                        g_free (*p);
                        *p = options[last];
                        options[last] = NULL;
                } else {
                        p += 1;
                }
        }
}

static gchar *
prepare_xkb_options (GsdKeyboardManager *manager,
                     guint               n_sources,
                     gchar             **extra_options)
{
        gchar **options;
        gchar **settings_options;
        gchar  *options_str;

        settings_options = g_settings_get_strv (manager->priv->input_sources_settings,
                                                KEY_KEYBOARD_OPTIONS);
        options = append_options (settings_options, extra_options);
        g_strfreev (settings_options);

        /* We might set up different layouts in different groups - see
         * replace_layout_and_variant(). But we don't want the X
         * server group switching feature to actually switch
         * them. Regularly, if we have at least two input sources,
         * gnome-shell will tell us to switch input source at that
         * point so we fix that automatically. But when there's only
         * one source, gnome-shell short circuits as an optimization
         * and doesn't call us so we can't set the group switching XKB
         * option in the first place otherwise the X server's switch
         * will take effect and we get a broken configuration. */
        if (n_sources < 2 || g_strcmp0 (g_getenv ("XDG_CURRENT_DESKTOP"), "Unity") == 0)
                strip_xkb_option (options, "grp:");

        options_str = build_xkb_options_string (options);
        g_strfreev (options);

        return options_str;
}

static void
apply_xkb_settings (GsdKeyboardManager *manager,
                    const gchar        *layout,
                    const gchar        *variant,
                    gchar              *options)
{
        XkbRF_RulesRec *xkb_rules;
        XkbRF_VarDefsRec *xkb_var_defs;
        gchar *rules_file_path;

        gsd_xkb_get_var_defs (&rules_file_path, &xkb_var_defs);

        free (xkb_var_defs->options);
        xkb_var_defs->options = options;

        replace_layout_and_variant (manager, xkb_var_defs, layout, variant);

        gdk_error_trap_push ();

        xkb_rules = XkbRF_Load (rules_file_path, NULL, True, True);
        if (xkb_rules) {
                XkbComponentNamesRec *xkb_comp_names;
                xkb_comp_names = g_new0 (XkbComponentNamesRec, 1);

                XkbRF_GetComponents (xkb_rules, xkb_var_defs, xkb_comp_names);
                upload_xkb_description (rules_file_path, xkb_var_defs, xkb_comp_names);

                free_xkb_component_names (xkb_comp_names);
                XkbRF_Free (xkb_rules, True);
        } else {
                g_warning ("Couldn't load XKB rules");
        }

        if (gdk_error_trap_pop ())
                g_warning ("Error loading XKB rules");

        gsd_xkb_free_var_defs (xkb_var_defs);
        g_free (rules_file_path);

        XkbLockModifiers (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), XkbUseCoreKbd, LockMask, 0);
}

static void
user_notify_is_loaded_cb (GObject    *object,
                          GParamSpec *pspec,
                          gpointer    user_data)
{
        ActUser *user = ACT_USER (object);
        GSettings *settings = user_data;

        if (act_user_is_loaded (user)) {
                GVariant *sources;
                GVariantIter iter;
                const gchar *type;
                const gchar *name;
                GVariantBuilder builder;

                g_signal_handlers_disconnect_by_data (user, user_data);

                sources = g_settings_get_value (settings, KEY_INPUT_SOURCES);

                g_variant_builder_init (&builder, G_VARIANT_TYPE ("aa{ss}"));

                g_variant_iter_init (&iter, sources);
                while (g_variant_iter_next (&iter, "(&s&s)", &type, &name)) {
                        g_variant_builder_open (&builder, G_VARIANT_TYPE ("a{ss}"));
                        g_variant_builder_add (&builder, "{ss}", type, name);
                        g_variant_builder_close (&builder);
                }

                g_variant_unref (sources);

                sources = g_variant_ref_sink (g_variant_builder_end (&builder));
                act_user_set_input_sources (user, sources);
                g_variant_unref (sources);
        }
}

static void
manager_notify_is_loaded_cb (GObject    *object,
                             GParamSpec *pspec,
                             gpointer    user_data)
{
        ActUserManager *manager = ACT_USER_MANAGER (object);

        gboolean loaded;
        g_object_get (manager, "is-loaded", &loaded, NULL);

        if (loaded) {
                ActUser *user;

                g_signal_handlers_disconnect_by_data (manager, user_data);

                user = act_user_manager_get_user (manager, g_get_user_name ());

                if (act_user_is_loaded (user))
                        user_notify_is_loaded_cb (G_OBJECT (user), NULL, user_data);
                else
                        g_signal_connect (user, "notify::is-loaded",
                                          user_notify_is_loaded_cb, user_data);
        }
}

#ifdef HAVE_FCITX
static gchar *
get_xkb_name (const gchar *name)
{
        gchar *xkb_name;
        gchar *separator;

        if (g_str_has_prefix (name, FCITX_XKB_PREFIX))
                name += strlen (FCITX_XKB_PREFIX);

        xkb_name = g_strdup (name);
        separator = strchr (xkb_name, '-');

        if (separator)
                *separator = '+';

        return xkb_name;
}

static gchar *
get_fcitx_name (const gchar *name)
{
        gchar *fcitx_name = g_strdup (name);
        gchar *separator = strchr (fcitx_name, '+');

        if (separator)
                *separator = '-';

        return fcitx_name;
}

static gboolean
input_source_is_fcitx_engine (const gchar *type,
                              const gchar *name,
                              const gchar *engine)
{
        if (g_str_equal (type, INPUT_SOURCE_TYPE_XKB)) {
                if (g_str_has_prefix (engine, FCITX_XKB_PREFIX)) {
                        gboolean equal;
                        gchar *fcitx_name = get_fcitx_name (name);
                        equal = g_str_equal (fcitx_name, engine + strlen (FCITX_XKB_PREFIX));
                        g_free (fcitx_name);
                        return equal;
                }
        } else if (g_str_equal (type, INPUT_SOURCE_TYPE_FCITX)) {
                return g_str_equal (name, engine);
        }

        return FALSE;
}

static void
fcitx_engine_changed (GsdKeyboardManager *manager,
                      GParamSpec         *pspec,
                      FcitxInputMethod   *fcitx)
{
        GSettings *settings = manager->priv->input_sources_settings;
        gchar *engine = fcitx_input_method_get_current_im (manager->priv->fcitx);

        if (engine) {
                GVariant *sources = g_settings_get_value (settings, KEY_INPUT_SOURCES);
                guint current = g_settings_get_uint (settings, KEY_CURRENT_INPUT_SOURCE);
                gboolean update = TRUE;

                if (current >= 0 && current < g_variant_n_children (sources)) {
                        const gchar *type;
                        const gchar *name;

                        g_variant_get_child (sources, current, "(&s&s)", &type, &name);
                        update = !input_source_is_fcitx_engine (type, name, engine);
                }

                if (update) {
                        gsize i;

                        for (i = 0; i < g_variant_n_children (sources); i++) {
                                const gchar *type;
                                const gchar *name;

                                if (i == current)
                                        continue;

                                g_variant_get_child (sources, i, "(&s&s)", &type, &name);

                                if (input_source_is_fcitx_engine (type, name, engine)) {
                                        g_settings_set_uint (settings, KEY_CURRENT_INPUT_SOURCE, i);
                                        break;
                                }
                        }
                }

                g_variant_unref (sources);
                g_free (engine);
        }
}

static gboolean
should_update_input_sources (GVariant  *sources,
                             GPtrArray *engines)
{
        GVariantIter iter;
        const gchar *type;
        const gchar *name;
        guint i = 0;

        g_variant_iter_init (&iter, sources);
        while (g_variant_iter_next (&iter, "(&s&s)", &type, &name)) {
                if (g_str_equal (type, INPUT_SOURCE_TYPE_XKB) ||
                    g_str_equal (type, INPUT_SOURCE_TYPE_FCITX)) {
                        FcitxIMItem *engine;

                        /* get the next enabled fcitx engine */
                        for (; i < engines->len; i++) {
                                engine = g_ptr_array_index (engines, i);

                                if (engine->enable)
                                        break;
                        }

                        /* there should be an enabled engine and it should match, otherwise we need to update */
                        if (i++ >= engines->len || !input_source_is_fcitx_engine (type, name, engine->unique_name))
                                return TRUE;
                }
        }

        /* if there are more enabled engines, we need to update */
        for (; i < engines->len; i++) {
                FcitxIMItem *engine = g_ptr_array_index (engines, i);

                if (engine->enable)
                        return TRUE;
        }

        return FALSE;
}

static void
update_input_sources_from_fcitx_engines (GsdKeyboardManager *manager,
                                         FcitxInputMethod   *fcitx)
{
        GPtrArray *engines = fcitx_input_method_get_imlist (manager->priv->fcitx);

        if (engines) {
                GVariant *sources = g_settings_get_value (manager->priv->input_sources_settings, KEY_INPUT_SOURCES);

                if (should_update_input_sources (sources, engines)) {
                        GVariantBuilder builder;
                        gboolean update = FALSE;
                        guint i;

                        g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ss)"));

                        for (i = 0; i < engines->len; i++) {
                                FcitxIMItem *engine = g_ptr_array_index (engines, i);

                                if (engine->enable) {
                                        if (g_str_has_prefix (engine->unique_name, FCITX_XKB_PREFIX)) {
                                                gchar *name = get_xkb_name (engine->unique_name);
                                                g_variant_builder_add (&builder, "(ss)", INPUT_SOURCE_TYPE_XKB, name);
                                                g_free (name);
                                        } else {
                                                g_variant_builder_add (&builder, "(ss)", INPUT_SOURCE_TYPE_FCITX, engine->unique_name);
                                        }

                                        update = TRUE;
                                }
                        }

                        if (update)
                                g_settings_set_value (manager->priv->input_sources_settings, KEY_INPUT_SOURCES, g_variant_builder_end (&builder));
                        else
                                g_variant_builder_clear (&builder);
                }

                g_variant_unref (sources);
                g_ptr_array_unref (engines);
        }
}
#endif

static gboolean
apply_input_source (GsdKeyboardManager *manager,
                    guint               current)
{
        GsdKeyboardManagerPrivate *priv = manager->priv;
        GVariant *sources;
        guint n_sources;
        const gchar *type = NULL;
        const gchar *id = NULL;
        gchar *layout = NULL;
        gchar *variant = NULL;
        gchar **options = NULL;

        sources = g_settings_get_value (priv->input_sources_settings, KEY_INPUT_SOURCES);
        n_sources = g_variant_n_children (sources);

        if (n_sources < 1)
                goto exit;
        else if (current >= n_sources)
                current = n_sources - 1;

        priv->active_input_source = current;

#ifdef HAVE_IBUS
        if (priv->is_ibus_active) {
                maybe_start_ibus (manager);
        }
#endif

        g_variant_get_child (sources, current, "(&s&s)", &type, &id);

        if (g_str_equal (type, INPUT_SOURCE_TYPE_XKB)) {
                const gchar *l, *v;
                gnome_xkb_info_get_layout_info (priv->xkb_info, id, NULL, NULL, &l, &v);

                layout = g_strdup (l);
                variant = g_strdup (v);

                if (!layout || !layout[0]) {
                        g_warning ("Couldn't find XKB input source '%s'", id);
                        goto exit;
                }

#ifdef HAVE_FCITX
                if (priv->is_fcitx_active && priv->fcitx) {
                        gchar *name = g_strdup_printf (FCITX_XKB_PREFIX "%s", id);
                        gchar *fcitx_name = get_fcitx_name (name);
                        fcitx_input_method_activate (priv->fcitx);
                        fcitx_input_method_set_current_im (priv->fcitx, fcitx_name);
                        g_free (fcitx_name);
                        g_free (name);
                }
#endif

#ifdef HAVE_IBUS
                if (priv->is_ibus_active) {
                        set_gtk_im_module (manager, sources);
                        set_ibus_xkb_engine (manager);
                }
#endif
        } else if (g_str_equal (type, INPUT_SOURCE_TYPE_IBUS)) {
#ifdef HAVE_IBUS
                if (priv->is_ibus_active) {
                        IBusEngineDesc *engine_desc = NULL;

                        if (priv->ibus_engines)
                                engine_desc = g_hash_table_lookup (priv->ibus_engines, id);
                        else
                                goto exit; /* we'll be called again when ibus is up and running */

                        if (engine_desc) {
                                const gchar *ibus_layout;
                                ibus_layout = ibus_engine_desc_get_layout (engine_desc);

                                if (ibus_layout) {
                                        layout = layout_from_ibus_layout (ibus_layout);
                                        variant = variant_from_ibus_layout (ibus_layout);
                                        options = options_from_ibus_layout (ibus_layout);
                                }
                        } else {
                                g_warning ("Couldn't find IBus input source '%s'", id);
                                goto exit;
                        }

                        /* NULL here is a shortcut for "I already know I
                           need the IBus module". */
                        set_gtk_im_module (manager, NULL);
                        set_ibus_engine (manager, id);
                } else {
                        g_warning ("IBus input source type specified but IBus is not active");
                }
#else
                g_warning ("IBus input source type specified but IBus support was not compiled");
#endif
        } else if (g_str_equal (type, INPUT_SOURCE_TYPE_FCITX)) {
#ifdef HAVE_FCITX
                if (priv->is_fcitx_active) {
                        if (priv->fcitx) {
                                gchar *name = g_strdup (id);
                                fcitx_input_method_activate (priv->fcitx);
                                fcitx_input_method_set_current_im (priv->fcitx, name);
                                g_free (name);
                        } else {
                                g_warning ("Fcitx input method framework unavailable");
                        }
                } else {
                        g_warning ("Fcitx input source type specified but Fcitx is not active");
                }
#else
                g_warning ("Fcitx input source type specified but Fcitx support was not compiled");
#endif
        } else {
                g_warning ("Unknown input source type '%s'", type);
        }

#ifdef HAVE_FCITX
        if (priv->is_fcitx_active && priv->fcitx && !priv->fcitx_signal_id) {
                priv->fcitx_signal_id = g_signal_connect_swapped (manager->priv->fcitx, "notify::current-im", G_CALLBACK (fcitx_engine_changed), manager);
                g_signal_connect_swapped (manager->priv->fcitx, "imlist-changed", G_CALLBACK (update_input_sources_from_fcitx_engines), manager);
        }
#endif

 exit:
        apply_xkb_settings (manager, layout, variant,
                            prepare_xkb_options (manager, n_sources, options));
        maybe_return_from_set_input_source (manager);
        g_variant_unref (sources);
        g_free (layout);
        g_free (variant);
        g_strfreev (options);
        return TRUE;
}

#ifdef HAVE_FCITX
static const gchar *
get_fcitx_engine_for_ibus_engine (const gchar *ibus_engine)
{
        const struct Engine *engine;

        if (!ibus_engine)
                return NULL;

        engine = get_engine_for_ibus_engine (ibus_engine, strlen (ibus_engine));

        return engine ? engine->fcitx_engine : NULL;
}

static void
enable_fcitx_engines (GsdKeyboardManager *manager,
                      gboolean            migrate)
{
        GsdKeyboardManagerPrivate *priv = manager->priv;
        GPtrArray *engines;
        GVariant *sources;
        GVariantIter iter;
        const gchar *type;
        const gchar *name;
        gboolean changed;
        guint i;
        guint j;

        engines = fcitx_input_method_get_imlist (priv->fcitx);

        if (!engines) {
                g_warning ("Cannot update Fcitx engine list");
                return;
        }

        sources = g_settings_get_value (priv->input_sources_settings, KEY_INPUT_SOURCES);
        changed = FALSE;
        i = 0;

        g_variant_iter_init (&iter, sources);
        while (g_variant_iter_next (&iter, "(&s&s)", &type, &name)) {
                if (g_str_equal (type, INPUT_SOURCE_TYPE_XKB)) {
                        gchar *fcitx_name = get_fcitx_name (name);

                        for (j = i; j < engines->len; j++) {
                                FcitxIMItem *engine = g_ptr_array_index (engines, j);

                                if (g_str_has_prefix (engine->unique_name, FCITX_XKB_PREFIX) &&
                                    g_str_equal (engine->unique_name + strlen (FCITX_XKB_PREFIX), fcitx_name)) {
                                        if (!engine->enable) {
                                                engine->enable = TRUE;
                                                changed = TRUE;
                                        }

                                        break;
                                }
                        }

                        g_free (fcitx_name);

                        /* j is either the index of the engine "fcitx-keyboard-<name>"
                         * or engines->len, meaning it wasn't found. */
                } else if (migrate && g_str_equal (type, INPUT_SOURCE_TYPE_IBUS)) {
                        const gchar *fcitx_name = get_fcitx_engine_for_ibus_engine (name);

                        if (!fcitx_name)
                                continue;

                        for (j = i; j < engines->len; j++) {
                                FcitxIMItem *engine = g_ptr_array_index (engines, j);

                                if (g_str_equal (engine->unique_name, fcitx_name)) {
                                        if (!engine->enable) {
                                                engine->enable = TRUE;
                                                changed = TRUE;
                                        }

                                        break;
                                }
                        }

                        /* j is either the index of the engine "<name>"
                         * or engines->len, meaning it wasn't found. */
                } else if (g_str_equal (type, INPUT_SOURCE_TYPE_FCITX)) {
                        for (j = i; j < engines->len; j++) {
                                FcitxIMItem *engine = g_ptr_array_index (engines, j);

                                if (g_str_equal (engine->unique_name, name)) {
                                        if (!engine->enable) {
                                                engine->enable = TRUE;
                                                changed = TRUE;
                                        }

                                        break;
                                }
                        }

                        /* j is either the index of the engine "<name>"
                         * or engines->len, meaning it wasn't found. */
                } else {
                        continue;
                }

                if (j < engines->len) {
                        if (j != i) {
                                gpointer ptr = engines->pdata[i];
                                engines->pdata[i] = engines->pdata[j];
                                engines->pdata[j] = ptr;
                                changed = TRUE;
                        }

                        i++;
                } else {
                        g_warning ("Fcitx engine not found for %s", name);
                }
        }

        /* we should have i enabled fcitx engines: disable everything else. */

        for (; i < engines->len; i++) {
                FcitxIMItem *engine = g_ptr_array_index (engines, i);

                if (engine->enable) {
                        engine->enable = FALSE;
                        changed = TRUE;
                }
        }

        if (changed) {
                fcitx_input_method_set_imlist (priv->fcitx, engines);
        }

        g_variant_unref (sources);
        g_ptr_array_unref (engines);
}

struct _FcitxShareStateConfig
{
        FcitxGenericConfig config;
        gint share_state;
};

typedef struct _FcitxShareStateConfig FcitxShareStateConfig;

static CONFIG_BINDING_BEGIN (FcitxShareStateConfig)
CONFIG_BINDING_REGISTER ("Program", "ShareStateAmongWindow", share_state)
CONFIG_BINDING_END ()

static CONFIG_DESC_DEFINE (get_fcitx_config_desc, "config.desc")

static void
update_share_state_from_per_window (GsdKeyboardManager *manager)
{
        FcitxConfigFileDesc *config_file_desc = get_fcitx_config_desc ();

        if (config_file_desc) {
                /* Set Fcitx' share state setting based on the GSettings per-window option. */
                GSettings *settings = g_settings_new ("org.gnome.libgnomekbd.desktop");
                gboolean per_window = g_settings_get_boolean (settings, "group-per-window");

                FcitxShareStateConfig config = { { NULL } };

                /* Load the user's Fcitx configuration. */
                FILE *file = FcitxXDGGetFileUserWithPrefix (NULL, "config", "r", NULL);
                FcitxConfigFile *config_file = FcitxConfigParseConfigFileFp (file, config_file_desc);
                FcitxShareStateConfigConfigBind (&config, config_file, config_file_desc);

                if (file)
                        fclose (file);

                config.share_state = per_window ? 0 : 1;

                /* Save the user's Fcitx configuration. */
                file = FcitxXDGGetFileUserWithPrefix (NULL, "config", "w", NULL);
                FcitxConfigSaveConfigFileFp (file, &config.config, config_file_desc);

                if (file)
                        fclose (file);

                fcitx_input_method_reload_config (manager->priv->fcitx);

                g_object_unref (settings);
        }
}

static void
migrate_fcitx_engines (GsdKeyboardManager *manager)
{
        GPtrArray *engines = fcitx_input_method_get_imlist (manager->priv->fcitx);

        if (engines) {
                gboolean migrate = FALSE;
                gboolean second = FALSE;
                guint i;

                /* Migrate if there are at least two enabled fcitx engines or
                 * there is one fcitx engine which is not a keyboard layout.
                 * These are our criteria for determining which direction to
                 * sync fcitx engines with input sources. */

                for (i = 0; i < engines->len; i++) {
                        FcitxIMItem *engine = g_ptr_array_index (engines, i);

                        if (engine->enable) {
                                if (second || !g_str_has_prefix (engine->unique_name, FCITX_XKB_PREFIX)) {
                                        migrate = TRUE;
                                        break;
                                }

                                second = TRUE;
                        }
                }

                if (migrate) {
                        /* Migrate Fcitx to GSettings. */
                        update_input_sources_from_fcitx_engines (manager, manager->priv->fcitx);
                } else {
                        /* Migrate GSettings to Fcitx. */
                        enable_fcitx_engines (manager, TRUE);
                        update_share_state_from_per_window (manager);
                }

                g_ptr_array_unref (engines);
        }
}
#endif

static gboolean
apply_input_sources_settings (GSettings          *settings,
                              gpointer            keys,
                              gint                n_keys,
                              GsdKeyboardManager *manager)
{
        GsdKeyboardManagerPrivate *priv = manager->priv;
        GVariant *sources;
        guint n_sources;
        guint current;
        ActUserManager *user_manager;
        gboolean user_manager_loaded;

#ifdef HAVE_FCITX
        if (priv->is_fcitx_active) {
                gboolean sources_changed = FALSE;

                if (keys) {
                        GQuark *quarks = keys;
                        gint i;

                        for (i = 0; i < n_keys; i++) {
                                if (quarks[i] == g_quark_try_string (KEY_INPUT_SOURCES)) {
                                        sources_changed = TRUE;
                                        break;
                                }
                        }
                } else {
                        sources_changed = TRUE;
                }

                if (sources_changed) {
                        enable_fcitx_engines (manager, FALSE);
                }
        }
#endif

        sources = g_settings_get_value (priv->input_sources_settings, KEY_INPUT_SOURCES);
        n_sources = g_variant_n_children (sources);

        if (n_sources < 1) {
                apply_xkb_settings (manager, NULL, NULL, prepare_xkb_options (manager, 0, NULL));
                maybe_return_from_set_input_source (manager);
                goto exit;
        }

        current = g_settings_get_uint (priv->input_sources_settings, KEY_CURRENT_INPUT_SOURCE);

        if (current >= n_sources) {
                g_settings_set_uint (priv->input_sources_settings, KEY_CURRENT_INPUT_SOURCE, n_sources - 1);
                goto exit;
        }

        user_manager = act_user_manager_get_default ();
        g_object_get (user_manager, "is-loaded", &user_manager_loaded, NULL);
        if (user_manager_loaded)
                manager_notify_is_loaded_cb (G_OBJECT (user_manager), NULL, priv->input_sources_settings);
        else
                g_signal_connect (user_manager, "notify::is-loaded", G_CALLBACK (manager_notify_is_loaded_cb), priv->input_sources_settings);

        apply_input_source (manager, current);

exit:
        g_variant_unref (sources);
        /* Prevent individual "changed" signal invocations since we
           don't need them. */
        return TRUE;
}

static void
apply_bell (GsdKeyboardManager *manager)
{
	GSettings       *settings;
        XKeyboardControl kbdcontrol;
        gboolean         click;
        int              bell_volume;
        int              bell_pitch;
        int              bell_duration;
        GsdBellMode      bell_mode;
        int              click_volume;

        g_debug ("Applying the bell settings");
        settings      = manager->priv->settings;
        click         = g_settings_get_boolean  (settings, KEY_CLICK);
        click_volume  = g_settings_get_int   (settings, KEY_CLICK_VOLUME);

        bell_pitch    = g_settings_get_int   (settings, KEY_BELL_PITCH);
        bell_duration = g_settings_get_int   (settings, KEY_BELL_DURATION);

        bell_mode = g_settings_get_enum (settings, KEY_BELL_MODE);
        bell_volume   = (bell_mode == GSD_BELL_MODE_ON) ? 50 : 0;

        /* as percentage from 0..100 inclusive */
        if (click_volume < 0) {
                click_volume = 0;
        } else if (click_volume > 100) {
                click_volume = 100;
        }
        kbdcontrol.key_click_percent = click ? click_volume : 0;
        kbdcontrol.bell_percent = bell_volume;
        kbdcontrol.bell_pitch = bell_pitch;
        kbdcontrol.bell_duration = bell_duration;

        gdk_error_trap_push ();
        XChangeKeyboardControl (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                KBKeyClickPercent | KBBellPercent | KBBellPitch | KBBellDuration,
                                &kbdcontrol);

        XSync (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), FALSE);
        gdk_error_trap_pop_ignored ();
}

static void
apply_numlock (GsdKeyboardManager *manager)
{
	GSettings *settings;
        gboolean rnumlock;

        g_debug ("Applying the num-lock settings");
        settings = manager->priv->settings;
        rnumlock = g_settings_get_boolean  (settings, KEY_REMEMBER_NUMLOCK_STATE);
        manager->priv->old_state = g_settings_get_enum (manager->priv->settings, KEY_NUMLOCK_STATE);

        gdk_error_trap_push ();
        if (rnumlock) {
                g_debug ("Remember num-lock is set, so applying setting '%s'",
                         num_lock_state_to_string (manager->priv->old_state));
                numlock_set_xkb_state (manager->priv->old_state);
        }

        XSync (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), FALSE);
        gdk_error_trap_pop_ignored ();
}

static void
apply_repeat (GsdKeyboardManager *manager)
{
	GSettings       *settings;
        gboolean         repeat;
        guint            interval;
        guint            delay;

        g_debug ("Applying the repeat settings");
        settings      = manager->priv->settings;
        repeat        = g_settings_get_boolean  (settings, KEY_REPEAT);
        interval      = g_settings_get_uint  (settings, KEY_INTERVAL);
        delay         = g_settings_get_uint  (settings, KEY_DELAY);

        gdk_error_trap_push ();
        if (repeat) {
                gboolean rate_set = FALSE;

                XAutoRepeatOn (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));
                /* Use XKB in preference */
                rate_set = xkb_set_keyboard_autorepeat_rate (delay, interval);

                if (!rate_set)
                        g_warning ("Neither XKeyboard not Xfree86's keyboard extensions are available,\n"
                                   "no way to support keyboard autorepeat rate settings");
        } else {
                XAutoRepeatOff (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()));
        }

        XSync (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()), FALSE);
        gdk_error_trap_pop_ignored ();
}

static void
apply_all_settings (GsdKeyboardManager *manager)
{
	apply_repeat (manager);
	apply_bell (manager);
	apply_numlock (manager);
}

static void
settings_changed (GSettings          *settings,
                  const char         *key,
                  GsdKeyboardManager *manager)
{
	if (g_strcmp0 (key, KEY_CLICK) == 0||
	    g_strcmp0 (key, KEY_CLICK_VOLUME) == 0 ||
	    g_strcmp0 (key, KEY_BELL_PITCH) == 0 ||
	    g_strcmp0 (key, KEY_BELL_DURATION) == 0 ||
	    g_strcmp0 (key, KEY_BELL_MODE) == 0) {
		g_debug ("Bell setting '%s' changed, applying bell settings", key);
		apply_bell (manager);
	} else if (g_strcmp0 (key, KEY_REMEMBER_NUMLOCK_STATE) == 0) {
		g_debug ("Remember Num-Lock state '%s' changed, applying num-lock settings", key);
		apply_numlock (manager);
	} else if (g_strcmp0 (key, KEY_NUMLOCK_STATE) == 0) {
		g_debug ("Num-Lock state '%s' changed, will apply at next startup", key);
	} else if (g_strcmp0 (key, KEY_REPEAT) == 0 ||
		 g_strcmp0 (key, KEY_INTERVAL) == 0 ||
		 g_strcmp0 (key, KEY_DELAY) == 0) {
		g_debug ("Key repeat setting '%s' changed, applying key repeat settings", key);
		apply_repeat (manager);
	} else {
		g_warning ("Unhandled settings change, key '%s'", key);
	}

}

static void
device_added_cb (GdkDeviceManager   *device_manager,
                 GdkDevice          *device,
                 GsdKeyboardManager *manager)
{
        GdkInputSource source;

        source = gdk_device_get_source (device);
        if (source == GDK_SOURCE_KEYBOARD) {
                g_debug ("New keyboard plugged in, applying all settings");
                apply_numlock (manager);
                apply_input_sources_settings (manager->priv->input_sources_settings, NULL, 0, manager);
                run_custom_command (device, COMMAND_DEVICE_ADDED);
        }
}

static void
device_removed_cb (GdkDeviceManager   *device_manager,
                   GdkDevice          *device,
                   GsdKeyboardManager *manager)
{
        GdkInputSource source;

        source = gdk_device_get_source (device);
        if (source == GDK_SOURCE_KEYBOARD) {
                run_custom_command (device, COMMAND_DEVICE_REMOVED);
        }
}

static void
set_devicepresence_handler (GsdKeyboardManager *manager)
{
        GdkDeviceManager *device_manager;

        device_manager = gdk_display_get_device_manager (gdk_display_get_default ());

        manager->priv->device_added_id = g_signal_connect (G_OBJECT (device_manager), "device-added",
                                                           G_CALLBACK (device_added_cb), manager);
        manager->priv->device_removed_id = g_signal_connect (G_OBJECT (device_manager), "device-removed",
                                                             G_CALLBACK (device_removed_cb), manager);
        manager->priv->device_manager = device_manager;
}

static void
get_sources_from_xkb_config (GsdKeyboardManager *manager)
{
        GsdKeyboardManagerPrivate *priv = manager->priv;
        GVariantBuilder builder;
        GVariant *v;
        gint i, n;
        gchar **layouts = NULL;
        gchar **variants = NULL;
        gboolean have_default_layout = FALSE;

        v = g_dbus_proxy_get_cached_property (priv->localed, "X11Layout");
        if (v) {
                const gchar *s = g_variant_get_string (v, NULL);
                if (*s)
                        layouts = g_strsplit (s, ",", -1);
                g_variant_unref (v);
        }

        if (!layouts)
                return;

        v = g_dbus_proxy_get_cached_property (priv->localed, "X11Variant");
        if (v) {
                const gchar *s = g_variant_get_string (v, NULL);
                if (*s)
                        variants = g_strsplit (s, ",", -1);
                g_variant_unref (v);
        }

        if (variants && variants[0])
                n = MIN (g_strv_length (layouts), g_strv_length (variants));
        else
                n = g_strv_length (layouts);

        init_builder_with_sources (&builder, priv->input_sources_settings);

        for (i = 0; i < n && layouts[i][0]; ++i) {
                gchar *id;

                if (variants && variants[i] && variants[i][0])
                        id = g_strdup_printf ("%s+%s", layouts[i], variants[i]);
                else
                        id = g_strdup (layouts[i]);

                if (g_str_equal (id, DEFAULT_LAYOUT))
                        have_default_layout = TRUE;

                g_variant_builder_add (&builder, "(ss)", INPUT_SOURCE_TYPE_XKB, id);
                g_free (id);
        }

        if (!have_default_layout)
                g_variant_builder_add (&builder, "(ss)", INPUT_SOURCE_TYPE_XKB, DEFAULT_LAYOUT);

        g_settings_set_value (priv->input_sources_settings, KEY_INPUT_SOURCES, g_variant_builder_end (&builder));

        g_strfreev (layouts);
        g_strfreev (variants);
}

static void
get_options_from_xkb_config (GsdKeyboardManager *manager)
{
        GsdKeyboardManagerPrivate *priv = manager->priv;
        GVariant *v;
        gchar **options = NULL;

        v = g_dbus_proxy_get_cached_property (priv->localed, "X11Options");
        if (v) {
                const gchar *s = g_variant_get_string (v, NULL);
                if (*s)
                        options = g_strsplit (s, ",", -1);
                g_variant_unref (v);
        }

        if (!options)
                return;

        g_settings_set_strv (priv->input_sources_settings, KEY_KEYBOARD_OPTIONS, (const gchar * const*) options);

        g_strfreev (options);
}

static void
convert_libgnomekbd_options (GSettings *settings)
{
        GPtrArray *opt_array;
        GSettings *libgnomekbd_settings;
        gchar **options, **o;

        if (!schema_is_installed ("org.gnome.libgnomekbd.keyboard"))
                return;

        opt_array = g_ptr_array_new_with_free_func (g_free);

        libgnomekbd_settings = g_settings_new ("org.gnome.libgnomekbd.keyboard");
        options = g_settings_get_strv (libgnomekbd_settings, "options");

        for (o = options; *o; ++o) {
                gchar **strv;

                strv = g_strsplit (*o, "\t", 2);
                if (strv[0] && strv[1])
                        g_ptr_array_add (opt_array, g_strdup (strv[1]));
                g_strfreev (strv);
        }
        g_ptr_array_add (opt_array, NULL);

        g_settings_set_strv (settings, KEY_KEYBOARD_OPTIONS, (const gchar * const*) opt_array->pdata);

        g_strfreev (options);
        g_object_unref (libgnomekbd_settings);
        g_ptr_array_free (opt_array, TRUE);
}

static void
convert_libgnomekbd_layouts (GSettings *settings)
{
        GVariantBuilder builder;
        GSettings *libgnomekbd_settings;
        gchar **layouts, **l;

        if (!schema_is_installed ("org.gnome.libgnomekbd.keyboard"))
                return;

        init_builder_with_sources (&builder, settings);

        libgnomekbd_settings = g_settings_new ("org.gnome.libgnomekbd.keyboard");
        layouts = g_settings_get_strv (libgnomekbd_settings, "layouts");

        for (l = layouts; *l; ++l) {
                gchar *id;
                gchar **strv;

                strv = g_strsplit (*l, "\t", 2);
                if (strv[0] && !strv[1])
                        id = g_strdup (strv[0]);
                else if (strv[0] && strv[1])
                        id = g_strdup_printf ("%s+%s", strv[0], strv[1]);
                else
                        id = NULL;

                if (id)
                        g_variant_builder_add (&builder, "(ss)", INPUT_SOURCE_TYPE_XKB, id);

                g_free (id);
                g_strfreev (strv);
        }

        g_settings_set_value (settings, KEY_INPUT_SOURCES, g_variant_builder_end (&builder));

        g_strfreev (layouts);
        g_object_unref (libgnomekbd_settings);
}

static gboolean
should_migrate (const gchar *id)
{
        gboolean migrate = FALSE;
        gchar *stamp_dir_path = NULL;
        gchar *stamp_file_path = NULL;
        GError *error = NULL;

        stamp_dir_path = g_build_filename (g_get_user_data_dir (), PACKAGE_NAME, NULL);
        if (g_mkdir_with_parents (stamp_dir_path, 0755)) {
                g_warning ("Failed to create directory %s: %s", stamp_dir_path, g_strerror (errno));
                goto out;
        }

        stamp_file_path = g_build_filename (stamp_dir_path, id, NULL);
        if (g_file_test (stamp_file_path, G_FILE_TEST_EXISTS))
                goto out;

        migrate = TRUE;

        if (!g_file_set_contents (stamp_file_path, "", 0, &error)) {
                g_warning ("%s", error->message);
                g_error_free (error);
        }
out:
        g_free (stamp_file_path);
        g_free (stamp_dir_path);
        return migrate;
}

static void
maybe_convert_old_settings (GSettings *settings)
{
        if (should_migrate ("input-sources-converted")) {
                GVariant *sources;
                gchar **options;

                sources = g_settings_get_value (settings, KEY_INPUT_SOURCES);
                if (g_variant_n_children (sources) < 1) {
                        convert_libgnomekbd_layouts (settings);
#ifdef HAVE_IBUS
                        convert_ibus (settings);
#endif
                }
                g_variant_unref (sources);

                options = g_settings_get_strv (settings, KEY_KEYBOARD_OPTIONS);
                if (g_strv_length (options) < 1)
                        convert_libgnomekbd_options (settings);
                g_strfreev (options);
        }
}

static void
maybe_create_initial_settings (GsdKeyboardManager *manager)
{
        GSettings *settings;
        GVariant *sources;
        gchar **options;

        settings = manager->priv->input_sources_settings;

        if (g_getenv ("RUNNING_UNDER_GDM")) {
                GVariantBuilder builder;
                /* clean the settings and get them from the "system" */
                g_variant_builder_init (&builder, G_VARIANT_TYPE ("a(ss)"));
                g_settings_set_value (settings, KEY_INPUT_SOURCES, g_variant_builder_end (&builder));
                get_sources_from_xkb_config (manager);

                g_settings_set_strv (settings, KEY_KEYBOARD_OPTIONS, NULL);
                get_options_from_xkb_config (manager);
                return;
        }

        maybe_convert_old_settings (settings);

        /* if we still don't have anything do some educated guesses */
        sources = g_settings_get_value (settings, KEY_INPUT_SOURCES);
        if (g_variant_n_children (sources) < 1) {
                get_sources_from_xkb_config (manager);
#ifdef HAVE_IBUS
                add_ibus_sources_from_locale (settings);
#endif
        }
        g_variant_unref (sources);

        options = g_settings_get_strv (settings, KEY_KEYBOARD_OPTIONS);
        if (g_strv_length (options) < 1)
                get_options_from_xkb_config (manager);
        g_strfreev (options);
}

static void
set_input_source_return (GDBusMethodInvocation *invocation)
{
        g_dbus_method_invocation_return_value (invocation, NULL);
}

static void
maybe_return_from_set_input_source (GsdKeyboardManager *manager)
{
        GsdKeyboardManagerPrivate *priv = manager->priv;

        if (!priv->invocation)
                return;

        if (priv->pending_ops > 0) {
                priv->pending_ops -= 1;
                return;
        }

        g_clear_pointer (&priv->invocation, set_input_source_return);
}

static void
increment_set_input_source_ops (GsdKeyboardManager *manager)
{
        GsdKeyboardManagerPrivate *priv = manager->priv;

        if (!priv->invocation)
                return;

        priv->pending_ops += 1;
}

static void
set_input_source (GsdKeyboardManager *manager,
                  gboolean            persist)
{
        GsdKeyboardManagerPrivate *priv = manager->priv;
        guint idx;

        g_variant_get (g_dbus_method_invocation_get_parameters (priv->invocation), "(u)", &idx);

        if (persist) {
                if (idx == priv->active_input_source) {
                        if (idx == g_settings_get_uint (priv->input_sources_settings, KEY_CURRENT_INPUT_SOURCE)) {
                                maybe_return_from_set_input_source (manager);
                                return;
                        }
                }

                g_settings_set_uint (priv->input_sources_settings, KEY_CURRENT_INPUT_SOURCE, idx);
        } else {
                if (idx == priv->active_input_source) {
                        maybe_return_from_set_input_source (manager);
                        return;
                }

                apply_input_source (manager, idx);
        }
}

static void
handle_dbus_method_call (GDBusConnection       *connection,
                         const gchar           *sender,
                         const gchar           *object_path,
                         const gchar           *interface_name,
                         const gchar           *method_name,
                         GVariant              *parameters,
                         GDBusMethodInvocation *invocation,
                         GsdKeyboardManager    *manager)
{
        GsdKeyboardManagerPrivate *priv = manager->priv;
        gboolean is_set_input_source;
        gboolean is_activate_input_source;

        is_set_input_source = g_str_equal (method_name, "SetInputSource");
        is_activate_input_source = g_str_equal (method_name, "ActivateInputSource");

        if (is_set_input_source || is_activate_input_source) {
                if (priv->invocation) {
#ifdef HAVE_IBUS
                        if (priv->is_ibus_active) {
                                /* This can only happen if there's an
                                 * ibus_bus_set_global_engine_async() call
                                 * going on. */
                                g_cancellable_cancel (priv->ibus_cancellable);
                        }
#endif
                        g_clear_pointer (&priv->invocation, set_input_source_return);
                        priv->pending_ops = 0;
                }
                priv->invocation = invocation;
                set_input_source (manager, is_set_input_source);
        }
}

static void
on_bus_name_lost (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         data)
{
        g_warning ("DBus name %s lost", name);
}

static void
got_session_bus (GObject            *source,
                 GAsyncResult       *res,
                 GsdKeyboardManager *manager)
{
        GsdKeyboardManagerPrivate *priv;
        GDBusConnection *connection;
        GDBusInterfaceInfo **interfaces;
        GError *error = NULL;
        GDBusInterfaceVTable vtable = {
                (GDBusInterfaceMethodCallFunc) handle_dbus_method_call,
                NULL,
                NULL,
        };

        connection = g_bus_get_finish (res, &error);
        if (!connection) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Couldn't get session bus: %s", error->message);
                g_error_free (error);
                return;
        }

        priv = manager->priv;
        priv->dbus_connection = connection;
        interfaces = priv->dbus_introspection->interfaces;

        if (interfaces) {
                for (; *interfaces; interfaces++) {
                        guint id = g_dbus_connection_register_object (priv->dbus_connection,
                                                                      GSD_KEYBOARD_DBUS_PATH,
                                                                      *interfaces,
                                                                      &vtable,
                                                                      manager,
                                                                      NULL,
                                                                      &error);

                        if (!id) {
                                GSList *ids;

                                for (ids = priv->dbus_register_object_ids; ids; ids = g_slist_next (ids))
                                        g_dbus_connection_unregister_object (priv->dbus_connection,
                                                                             GPOINTER_TO_UINT (ids->data));

                                g_slist_free (priv->dbus_register_object_ids);
                                priv->dbus_register_object_ids = NULL;

                                g_warning ("Error registering object: %s", error->message);
                                g_error_free (error);
                                return;
                        }

                        priv->dbus_register_object_ids = g_slist_prepend (priv->dbus_register_object_ids,
                                                                          GUINT_TO_POINTER (id));
                }
        }

        priv->dbus_own_name_id = g_bus_own_name_on_connection (priv->dbus_connection,
                                                               GSD_KEYBOARD_DBUS_NAME,
                                                               G_BUS_NAME_OWNER_FLAGS_NONE,
                                                               NULL,
                                                               on_bus_name_lost,
                                                               NULL,
                                                               NULL);
}

static void
register_manager_dbus (GsdKeyboardManager *manager)
{
        GError *error = NULL;

        manager->priv->dbus_introspection = g_dbus_node_info_new_for_xml (introspection_xml, &error);
        if (error) {
                g_warning ("Error creating introspection data: %s", error->message);
                g_error_free (error);
                return;
        }

        g_bus_get (G_BUS_TYPE_SESSION,
                   manager->priv->cancellable,
                   (GAsyncReadyCallback) got_session_bus,
                   manager);
}

static void
localed_proxy_ready (GObject      *source,
                     GAsyncResult *res,
                     gpointer      data)
{
        GsdKeyboardManager *manager = data;
        GDBusProxy *proxy;
        GError *error = NULL;

        proxy = g_dbus_proxy_new_finish (res, &error);
        if (!proxy) {
                if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
                        g_error_free (error);
                        return;
                }
                g_warning ("Failed to contact localed: %s", error->message);
                g_error_free (error);
                goto out;
        }

        manager->priv->localed = proxy;
        maybe_create_initial_settings (manager);
out:
        apply_input_sources_settings (manager->priv->input_sources_settings, NULL, 0, manager);
        register_manager_dbus (manager);
}

#ifdef HAVE_FCITX
static void
fcitx_appeared (GDBusConnection *connection,
                const gchar     *name,
                const gchar     *name_owner,
                gpointer         user_data)
{
        GsdKeyboardManager *manager = user_data;
        apply_input_sources_settings (manager->priv->input_sources_settings, NULL, 0, manager);
}

static void
fcitx_vanished (GDBusConnection *connection,
                const gchar     *name,
                gpointer         user_data)
{
        GsdKeyboardManager *manager = user_data;
        g_signal_handlers_disconnect_by_data (manager->priv->fcitx, manager);
        manager->priv->fcitx_signal_id = 0;
}
#endif

static gboolean
start_keyboard_idle_cb (GsdKeyboardManager *manager)
{
        const gchar *module;
        GError *error = NULL;

        gnome_settings_profile_start (NULL);

        g_debug ("Starting keyboard manager");

        module = g_getenv (ENV_GTK_IM_MODULE);
#ifdef HAVE_IBUS
        manager->priv->is_ibus_active = g_strcmp0 (module, GTK_IM_MODULE_IBUS) == 0;
#endif
#ifdef HAVE_FCITX
        manager->priv->is_fcitx_active = g_strcmp0 (module, GTK_IM_MODULE_FCITX) == 0;
#endif

        manager->priv->settings = g_settings_new (GSD_KEYBOARD_DIR);

	xkb_init (manager);

	set_devicepresence_handler (manager);

        manager->priv->input_sources_settings = g_settings_new (GNOME_DESKTOP_INPUT_SOURCES_DIR);
        manager->priv->interface_settings = g_settings_new (GNOME_DESKTOP_INTERFACE_DIR);
        manager->priv->xkb_info = gnome_xkb_info_new ();

#ifdef HAVE_FCITX
        if (manager->priv->is_fcitx_active) {
                manager->priv->fcitx_cancellable = g_cancellable_new ();
                manager->priv->fcitx = fcitx_input_method_new (G_BUS_TYPE_SESSION,
                                                               G_DBUS_PROXY_FLAGS_GET_INVALIDATED_PROPERTIES,
                                                               0,
                                                               manager->priv->fcitx_cancellable,
                                                               &error);
                g_clear_object (&manager->priv->fcitx_cancellable);

                if (manager->priv->fcitx) {
                        manager->priv->fcitx_signal_id = 0;

                        g_bus_watch_name (G_BUS_TYPE_SESSION,
                                          "org.fcitx.Fcitx",
                                          G_BUS_NAME_WATCHER_FLAGS_NONE,
                                          fcitx_appeared,
                                          fcitx_vanished,
                                          manager,
                                          NULL);

                        if (should_migrate ("fcitx-engines-migrated"))
                                migrate_fcitx_engines (manager);
                        else
                                enable_fcitx_engines (manager, FALSE);
                } else {
                        g_warning ("Fcitx input method framework unavailable: %s", error->message);
                        g_error_free (error);
                }
        }
#endif

        manager->priv->cancellable = g_cancellable_new ();

        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                  G_DBUS_PROXY_FLAGS_NONE,
                                  NULL,
                                  "org.freedesktop.locale1",
                                  "/org/freedesktop/locale1",
                                  "org.freedesktop.locale1",
                                  manager->priv->cancellable,
                                  localed_proxy_ready,
                                  manager);

        /* apply current settings before we install the callback */
        g_debug ("Started the keyboard plugin, applying all settings");
        apply_all_settings (manager);

        g_signal_connect (G_OBJECT (manager->priv->settings), "changed",
                          G_CALLBACK (settings_changed), manager);
        g_signal_connect (G_OBJECT (manager->priv->input_sources_settings), "change-event",
                          G_CALLBACK (apply_input_sources_settings), manager);

	install_xkb_filter (manager);

        gnome_settings_profile_end (NULL);

        manager->priv->start_idle_id = 0;

        return FALSE;
}

gboolean
gsd_keyboard_manager_start (GsdKeyboardManager *manager,
                            GError            **error)
{
        gnome_settings_profile_start (NULL);

	if (check_xkb_extension (manager) == FALSE) {
		g_debug ("XKB is not supported, not applying any settings");
		return TRUE;
	}

        manager->priv->start_idle_id = g_idle_add ((GSourceFunc) start_keyboard_idle_cb, manager);

        gnome_settings_profile_end (NULL);

        return TRUE;
}

void
gsd_keyboard_manager_stop (GsdKeyboardManager *manager)
{
        GsdKeyboardManagerPrivate *p = manager->priv;
        GSList *ids;

        g_debug ("Stopping keyboard manager");

        if (p->dbus_own_name_id) {
                g_bus_unown_name (p->dbus_own_name_id);
                p->dbus_own_name_id = 0;
        }

        for (ids = p->dbus_register_object_ids; ids; ids = g_slist_next (ids))
                g_dbus_connection_unregister_object (p->dbus_connection, GPOINTER_TO_UINT (ids->data));

        g_slist_free (p->dbus_register_object_ids);
        p->dbus_register_object_ids = NULL;

        g_cancellable_cancel (p->cancellable);
        g_clear_object (&p->cancellable);

        g_clear_object (&p->settings);
        g_clear_object (&p->input_sources_settings);
        g_clear_object (&p->interface_settings);
        g_clear_object (&p->xkb_info);
        g_clear_object (&p->localed);

#ifdef HAVE_FCITX
        if (p->is_fcitx_active) {
                if (p->fcitx_cancellable) {
                        g_cancellable_cancel (p->fcitx_cancellable);
                }

                g_clear_object (&p->fcitx_cancellable);
                g_clear_object (&p->fcitx);
        }
#endif

#ifdef HAVE_IBUS
        if (p->is_ibus_active) {
                clear_ibus (manager);
        }
#endif

        if (p->device_manager != NULL) {
                g_signal_handler_disconnect (p->device_manager, p->device_added_id);
                g_signal_handler_disconnect (p->device_manager, p->device_removed_id);
                p->device_manager = NULL;
        }

	remove_xkb_filter (manager);

        g_clear_pointer (&p->invocation, set_input_source_return);
        g_clear_pointer (&p->dbus_introspection, g_dbus_node_info_unref);
        g_clear_object (&p->dbus_connection);
}

static void
gsd_keyboard_manager_class_init (GsdKeyboardManagerClass *klass)
{
        GObjectClass   *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gsd_keyboard_manager_finalize;

        g_type_class_add_private (klass, sizeof (GsdKeyboardManagerPrivate));
}

static void
gsd_keyboard_manager_init (GsdKeyboardManager *manager)
{
        manager->priv = GSD_KEYBOARD_MANAGER_GET_PRIVATE (manager);
        manager->priv->active_input_source = -1;
}

static void
gsd_keyboard_manager_finalize (GObject *object)
{
        GsdKeyboardManager *keyboard_manager;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GSD_IS_KEYBOARD_MANAGER (object));

        keyboard_manager = GSD_KEYBOARD_MANAGER (object);

        g_return_if_fail (keyboard_manager->priv != NULL);

        if (keyboard_manager->priv->start_idle_id != 0)
                g_source_remove (keyboard_manager->priv->start_idle_id);

        G_OBJECT_CLASS (gsd_keyboard_manager_parent_class)->finalize (object);
}

GsdKeyboardManager *
gsd_keyboard_manager_new (void)
{
        if (manager_object != NULL) {
                g_object_ref (manager_object);
        } else {
                manager_object = g_object_new (GSD_TYPE_KEYBOARD_MANAGER, NULL);
                g_object_add_weak_pointer (manager_object,
                                           (gpointer *) &manager_object);
        }

        return GSD_KEYBOARD_MANAGER (manager_object);
}
