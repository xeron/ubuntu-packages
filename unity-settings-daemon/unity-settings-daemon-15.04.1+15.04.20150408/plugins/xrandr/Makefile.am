plugin_name = xrandr

icondir = $(datadir)/icons/hicolor
context = apps

ICON_FILES = 			\
	usd-xrandr-16.png	\
	usd-xrandr-22.png	\
	usd-xrandr-24.png	\
	usd-xrandr-32.png	\
	usd-xrandr.svg

#$(mkinstalldirs) $(DESTDIR)$(sysconfdir)/gnome-settings-daemon/xrandr # FIXME: Conflicts with g-s-d, but we might want to use the same config file
install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(icondir)/16x16/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/22x22/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/24x24/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/32x32/$(context)
	$(mkinstalldirs) $(DESTDIR)$(icondir)/scalable/$(context)
	$(INSTALL_DATA) $(srcdir)/usd-xrandr-16.png $(DESTDIR)$(icondir)/16x16/$(context)/usd-xrandr.png
	$(INSTALL_DATA) $(srcdir)/usd-xrandr-22.png $(DESTDIR)$(icondir)/22x22/$(context)/usd-xrandr.png
	$(INSTALL_DATA) $(srcdir)/usd-xrandr-24.png $(DESTDIR)$(icondir)/24x24/$(context)/usd-xrandr.png
	$(INSTALL_DATA) $(srcdir)/usd-xrandr-32.png $(DESTDIR)$(icondir)/32x32/$(context)/usd-xrandr.png
	$(INSTALL_DATA) $(srcdir)/usd-xrandr.svg $(DESTDIR)$(icondir)/scalable/$(context)/usd-xrandr.svg

uninstall-local:
	rm -f $(DESTDIR)$(icondir)/16x16/$(context)/usd-xrandr.png
	rm -f $(DESTDIR)$(icondir)/22x22/$(context)/usd-xrandr.png
	rm -f $(DESTDIR)$(icondir)/24x24/$(context)/usd-xrandr.png
	rm -f $(DESTDIR)$(icondir)/32x32/$(context)/usd-xrandr.png
	rm -f $(DESTDIR)$(icondir)/scalable/$(context)/usd-xrandr.svg

libexec_PROGRAMS = usd-test-xrandr

usd_test_xrandr_SOURCES =		\
	test-xrandr.c			\
	gsd-xrandr-manager.c		\
	gsd-xrandr-manager.h

usd_test_xrandr_CPPFLAGS =					\
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/gnome-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DLIBEXECDIR=\""$(libexecdir)"\"			\
	$(AM_CPPFLAGS)

usd_test_xrandr_CFLAGS =			\
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(XRANDR_CFLAGS)		\
	$(AM_CFLAGS)

usd_test_xrandr_LDADD =						\
	$(top_builddir)/gnome-settings-daemon/libgsd.la		\
	$(top_builddir)/gnome-settings-daemon/libunity-settings-daemon.la		\
	$(top_builddir)/plugins/common/libcommon.la		\
	$(SETTINGS_DAEMON_LIBS)			\
	$(SETTINGS_PLUGIN_LIBS)			\
	$(XRANDR_LIBS)

plugin_LTLIBRARIES = \
	libxrandr.la

libxrandr_la_SOURCES = 		\
	gsd-xrandr-plugin.c	\
	gsd-xrandr-manager.h	\
	gsd-xrandr-manager.c

libxrandr_la_CPPFLAGS =						\
	-I$(top_srcdir)/gnome-settings-daemon			\
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/plugins/common/				\
	-DBINDIR=\"$(bindir)\"					\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

libxrandr_la_CFLAGS =			\
	$(PLUGIN_CFLAGS)		\
	$(XRANDR_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(WACOM_CFLAGS)			\
	$(AM_CFLAGS)

libxrandr_la_LDFLAGS = 			\
	$(GSD_PLUGIN_LDFLAGS)

libxrandr_la_LIBADD  =					\
	$(top_builddir)/plugins/common/libcommon.la	\
	$(XRANDR_LIBS)					\
	$(WACOM_LIBS)					\
	$(SETTINGS_PLUGIN_LIBS)

plugin_in_files =			\
	xrandr.gnome-settings-plugin.in

plugin_DATA = $(plugin_in_files:.gnome-settings-plugin.in=.gnome-settings-plugin)

EXTRA_DIST = $(plugin_in_files) $(ICON_FILES)
CLEANFILES = $(plugin_DATA)
DISTCLEANFILES = $(plugin_DATA)

@GSD_INTLTOOL_PLUGIN_RULE@
