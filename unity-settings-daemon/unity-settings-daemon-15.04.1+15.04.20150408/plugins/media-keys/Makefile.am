icondir = $(datadir)/icons/hicolor
context = actions

plugin_name = media-keys

NULL =

SUBDIRS = gvc
plugin_LTLIBRARIES = libmedia-keys.la

BUILT_SOURCES = 			\
	gsd-marshal.h			\
	gsd-marshal.c			\
	shell-key-grabber.c		\
	shell-key-grabber.h		\
	$(NULL)

gsd-marshal.c: gsd-marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=gsd_marshal $< --header --body --internal > $@

gsd-marshal.h: gsd-marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --prefix=gsd_marshal $< --header --internal > $@

shell-key-grabber.c: shell-key-grabber.h
shell-key-grabber.h: Makefile.am org.gnome.ShellKeyGrabber.xml
	gdbus-codegen --interface-prefix org.gnome.		\
	              --generate-c-code shell-key-grabber	\
	              --c-namespace Shell			\
	              $(srcdir)/org.gnome.ShellKeyGrabber.xml

libmedia_keys_la_SOURCES = 		\
	what-did-you-plug-in/pa-backend.c \
	what-did-you-plug-in/dialog-window.c \
	gsd-media-keys-plugin.c		\
	gsd-media-keys-manager.h	\
	gsd-media-keys-manager.c	\
	gsd-screenshot-utils.h		\
	gsd-screenshot-utils.c		\
	shortcuts-list.h		\
	shell-keybinding-modes.h	\
	$(BUILT_SOURCES)		\
	$(NULL)

libmedia_keys_la_CPPFLAGS = \
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/gnome-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-I$(top_srcdir)/plugins/media-keys/gvc			\
	-I$(top_srcdir)/plugins/media-keys/what-did-you-plug-in	\
	-DBINDIR=\"$(bindir)\"					\
	-DPIXMAPDIR=\""$(pkgdatadir)"\"				\
	-DGTKBUILDERDIR=\""$(pkgdatadir)"\"			\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

libmedia_keys_la_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(MEDIA_KEYS_CFLAGS)		\
	$(AM_CFLAGS)

libmedia_keys_la_LDFLAGS = 		\
	$(GSD_PLUGIN_LDFLAGS)

libmedia_keys_la_LIBADD  = 		\
	$(top_builddir)/plugins/common/libcommon.la			\
	$(top_builddir)/plugins/media-keys/gvc/libgvc.la		\
	$(MEDIA_KEYS_LIBS)						\
	$(SETTINGS_PLUGIN_LIBS)						\
	-lm

plugin_in_files = 		\
	media-keys.gnome-settings-plugin.in

plugin_DATA = $(plugin_in_files:.gnome-settings-plugin.in=.gnome-settings-plugin)

libexec_PROGRAMS = usd-test-media-keys

usd_test_media_keys_SOURCES =			\
	gsd-media-keys-manager.c		\
	gsd-media-keys-manager.h		\
	gsd-screenshot-utils.h			\
	gsd-screenshot-utils.c			\
	test-media-keys.c			\
	what-did-you-plug-in/pa-backend.c	\
	what-did-you-plug-in/dialog-window.c	\
	$(BUILT_SOURCES)			\
	$(NULL)

usd_test_media_keys_CPPFLAGS = \
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/gnome-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-I$(top_srcdir)/plugins/media-keys/gvc			\
	-I$(top_srcdir)/plugins/media-keys/what-did-you-plug-in	\
	-DBINDIR=\"$(bindir)\"					\
	-DPIXMAPDIR=\""$(pkgdatadir)"\"				\
	-DGTKBUILDERDIR=\""$(pkgdatadir)"\"			\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

usd_test_media_keys_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(MEDIA_KEYS_CFLAGS)		\
	$(AM_CFLAGS)

usd_test_media_keys_LDADD = \
	$(top_builddir)/gnome-settings-daemon/libgsd.la		\
	$(top_builddir)/plugins/common/libcommon.la		\
	$(top_builddir)/plugins/media-keys/gvc/libgvc.la	\
	$(SETTINGS_DAEMON_LIBS)			\
	$(SETTINGS_PLUGIN_LIBS)			\
	$(MEDIA_KEYS_LIBS)			\
	-lm

EXTRA_DIST = 				\
	gsd-marshal.list		\
	README.media-keys-API		\
	org.gnome.ShellKeyGrabber.xml	\
	$(plugin_in_files)

CLEANFILES = 			\
	$(BUILT_SOURCES)	\
	$(plugin_DATA)

DISTCLEANFILES =		\
	$(plugin_DATA)

@GSD_INTLTOOL_PLUGIN_RULE@
