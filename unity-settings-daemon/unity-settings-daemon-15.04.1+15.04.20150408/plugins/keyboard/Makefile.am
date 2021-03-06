NULL =

plugin_name = keyboard

plugin_LTLIBRARIES = \
	libkeyboard.la		\
	$(NULL)

themedir = $(pkgdatadir)/icons/hicolor
size = 64x64
context = devices

iconsdir = $(themedir)/$(size)/$(context)
icons_DATA = \
	  kbd-capslock-off.png kbd-numlock-off.png kbd-scrolllock-off.png \
	  kbd-capslock-on.png kbd-numlock-on.png kbd-scrolllock-on.png

if HAVE_FCITX
BUILT_SOURCES = input-method-engines.c
endif

input-method-engines.c: $(srcdir)/input-method-engines.gperf
	$(AM_V_GEN) gperf --output-file=input-method-engines.c $<

libkeyboard_la_SOURCES = 	\
	gsd-keyboard-plugin.c	\
	gsd-keyboard-manager.h	\
	gsd-keyboard-manager.c	\
	gsd-xkb-utils.h		\
	gsd-xkb-utils.c		\
	$(NULL)

libkeyboard_la_CPPFLAGS = \
	-I$(top_srcdir)/gnome-settings-daemon		\
	-I$(top_srcdir)/data				\
	-I$(top_srcdir)/plugins/common			\
	-DDATADIR=\""$(pkgdatadir)"\"			\
	-DLIBEXECDIR=\""$(libexecdir)"\"		\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	-DXKB_BASE=\""$(XKB_BASE)"\"			\
	$(AM_CPPFLAGS)

libkeyboard_la_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(KEYBOARD_CFLAGS)		\
	$(AM_CFLAGS)

libkeyboard_la_LDFLAGS = 	\
	$(GSD_PLUGIN_LDFLAGS)	\
	$(NULL)

libkeyboard_la_LIBADD  =				\
	$(top_builddir)/plugins/common/libcommon.la	\
	$(SETTINGS_PLUGIN_LIBS)				\
	$(XF86MISC_LIBS)				\
	$(KEYBOARD_LIBS)				\
	$(NULL)

libexec_PROGRAMS = usd-test-keyboard
usd_test_keyboard_SOURCES =	\
	test-keyboard.c		\
	gsd-keyboard-manager.h	\
	gsd-keyboard-manager.c	\
	gsd-xkb-utils.h		\
	gsd-xkb-utils.c		\
	$(NULL)

usd_test_keyboard_CFLAGS = $(libkeyboard_la_CFLAGS)
usd_test_keyboard_CPPFLAGS = $(libkeyboard_la_CPPFLAGS)
usd_test_keyboard_LDADD = $(libkeyboard_la_LIBADD) $(top_builddir)/gnome-settings-daemon/libgsd.la

plugin_in_files = 		\
	keyboard.gnome-settings-plugin.in	\
	$(NULL)

plugin_DATA = $(plugin_in_files:.gnome-settings-plugin.in=.gnome-settings-plugin)

if HAVE_IBUS
noinst_PROGRAMS = test-keyboard-ibus-utils
test_keyboard_ibus_utils_SOURCES = test-keyboard-ibus-utils.c
test_keyboard_ibus_utils_CFLAGS = $(libkeyboard_la_CFLAGS)
test_keyboard_ibus_utils_CPPFLAGS = $(libkeyboard_la_CPPFLAGS)
test_keyboard_ibus_utils_LDADD = $(libkeyboard_la_LIBADD) $(top_builddir)/gnome-settings-daemon/libgsd.la

check-local: test-keyboard-ibus-utils
	$(builddir)/test-keyboard-ibus-utils > /dev/null
endif

EXTRA_DIST = 			\
	$(icons_DATA)		\
	$(plugin_in_files)	\
	$(ui_DATA)		\
	$(NULL)

CLEANFILES = 			\
	$(plugin_DATA)		\
	$(NULL)

DISTCLEANFILES =		\
	$(plugin_DATA)		\
	$(NULL)

@GSD_INTLTOOL_PLUGIN_RULE@
