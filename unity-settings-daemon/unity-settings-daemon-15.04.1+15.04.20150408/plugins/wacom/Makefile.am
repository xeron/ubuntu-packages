plugin_name = wacom

plugin_LTLIBRARIES = libgsdwacom.la

libgsdwacom_la_SOURCES =	\
	gsd-wacom-plugin.c	\
	gsd-wacom-manager.h	\
	gsd-wacom-manager.c	\
	gsd-wacom-osd-window.h	\
	gsd-wacom-osd-window.c	\
	gsd-wacom-device.c	\
	gsd-wacom-device.h	\
	gsd-wacom-resources.c

libgsdwacom_la_CPPFLAGS = \
	-I$(top_srcdir)/gnome-settings-daemon		\
	-I$(top_srcdir)/plugins/common/			\
	-I$(top_srcdir)/data/				\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\" \
	-DLIBEXECDIR=\""$(libexecdir)"\" 		\
	$(AM_CPPFLAGS)

libgsdwacom_la_CFLAGS = \
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(WACOM_CFLAGS)			\
	$(AM_CFLAGS)

libgsdwacom_la_LDFLAGS =		\
	$(GSD_PLUGIN_LDFLAGS)

libgsdwacom_la_LIBADD  =						\
	$(top_builddir)/plugins/common/libcommon.la			\
	$(top_builddir)/gnome-settings-daemon/libunity-settings-daemon.la			\
	$(SETTINGS_PLUGIN_LIBS)						\
	$(WACOM_LIBS)

com.ubuntu.unity-settings-daemon.plugins.wacom.policy.in: com.ubuntu.unity-settings-daemon.plugins.wacom.policy.in.in Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

gsd-wacom-resources.c: wacom.gresource.xml tablet-layout.css
	glib-compile-resources 				\
		--target=$@ 				\
		--sourcedir=$(srcdir) 			\
		--generate-source 			\
		--c-name gsd_wacom 			\
		$(srcdir)/wacom.gresource.xml

@INTLTOOL_POLICY_RULE@
polkit_policydir = $(datadir)/polkit-1/actions
polkit_policy_in_files = com.ubuntu.unity-settings-daemon.plugins.wacom.policy.in
polkit_policy_DATA = $(polkit_policy_in_files:.policy.in=.policy)

# so it always gets included in the tarball
usd_wacom_led_helper_SOURCES =	gsd-wacom-led-helper.c

EXTRA_DIST = $(usd_wacom_led_helper_SOURCES) wacom.gresource.xml tablet-layout.css

if HAVE_GUDEV
libexec_PROGRAMS = usd-wacom-led-helper

usd_wacom_led_helper_LDFLAGS =				\
	$(BACKLIGHT_HELPER_LIBS)			\
	-lm

usd_wacom_led_helper_CFLAGS =				\
	$(BACKLIGHT_HELPER_CFLAGS)
else
libexec_PROGRAMS =
endif

EXTRA_DIST += com.ubuntu.unity-settings-daemon.plugins.wacom.policy.in.in

libexec_PROGRAMS += usd-test-wacom usd-list-wacom usd-test-wacom-osd

usd_test_wacom_SOURCES =	\
	test-wacom.c		\
	gsd-wacom-manager.c	\
	gsd-wacom-manager.h	\
	gsd-wacom-osd-window.h	\
	gsd-wacom-osd-window.c	\
	gsd-wacom-device.c	\
	gsd-wacom-device.h	\
	gsd-wacom-resources.c

usd_test_wacom_CPPFLAGS = \
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/gnome-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DBINDIR=\"$(bindir)\"					\
	-DPIXMAPDIR=\""$(pkgdatadir)"\"				\
	-DGTKBUILDERDIR=\""$(pkgdatadir)"\"			\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DLIBEXECDIR=\""$(libexecdir)"\"			\
	-DSCHEMA_NAME=\""gsdwacom"\"				\
	$(AM_CPPFLAGS)

usd_test_wacom_CFLAGS =			\
	$(PLUGIN_CFLAGS)		\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(WACOM_CFLAGS)			\
	$(AM_CFLAGS)

usd_test_wacom_LDADD =						\
	$(top_builddir)/gnome-settings-daemon/libgsd.la		\
	$(top_builddir)/plugins/common/libcommon.la		\
	$(top_builddir)/gnome-settings-daemon/libunity-settings-daemon.la		\
	$(SETTINGS_DAEMON_LIBS)					\
	$(SETTINGS_PLUGIN_LIBS)					\
	$(WACOM_LIBS)						\
	-lm

usd_list_wacom_SOURCES =	\
	list-wacom.c		\
	gsd-wacom-device.c	\
	gsd-wacom-device.h

usd_list_wacom_CPPFLAGS =					\
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/gnome-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DBINDIR=\"$(bindir)\"					\
	-DPIXMAPDIR=\""$(pkgdatadir)"\"				\
	-DGTKBUILDERDIR=\""$(pkgdatadir)"\"			\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	$(AM_CPPFLAGS)

usd_list_wacom_CFLAGS =			\
	$(SETTINGS_PLUGIN_CFLAGS)	\
	$(WACOM_CFLAGS)			\
	$(AM_CFLAGS)

usd_list_wacom_LDADD =						\
	$(top_builddir)/gnome-settings-daemon/libgsd.la		\
	$(top_builddir)/plugins/common/libcommon.la		\
	$(top_builddir)/gnome-settings-daemon/libunity-settings-daemon.la		\
	$(SETTINGS_DAEMON_LIBS)					\
	$(SETTINGS_PLUGIN_LIBS)					\
	$(WACOM_LIBS)						\
	-lm

usd_test_wacom_osd_SOURCES =					\
	test-osd-window.c					\
	gsd-wacom-osd-window.h					\
	gsd-wacom-osd-window.c					\
	gsd-wacom-device.c					\
	gsd-wacom-device.h					\
	gsd-wacom-resources.c

usd_test_wacom_osd_CPPFLAGS = \
	-I$(top_srcdir)/data/					\
	-I$(top_srcdir)/gnome-settings-daemon			\
	-I$(top_srcdir)/plugins/common				\
	-DBINDIR=\"$(bindir)\"					\
	-DPIXMAPDIR=\""$(pkgdatadir)"\"				\
	-DGTKBUILDERDIR=\""$(pkgdatadir)"\"			\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DLIBEXECDIR=\""$(libexecdir)"\"			\
	$(AM_CPPFLAGS)

usd_test_wacom_osd_CFLAGS =					\
	$(SETTINGS_PLUGIN_CFLAGS)				\
	$(WACOM_CFLAGS)						\
	$(AM_CFLAGS)

usd_test_wacom_osd_LDADD = \
	$(top_builddir)/gnome-settings-daemon/libgsd.la		\
	$(top_builddir)/plugins/common/libcommon.la		\
	$(top_builddir)/gnome-settings-daemon/libunity-settings-daemon.la		\
	$(SETTINGS_DAEMON_LIBS)					\
	$(SETTINGS_PLUGIN_LIBS)					\
	$(WACOM_LIBS)						\
	-lm

plugin_in_files = wacom.gnome-settings-plugin.in

plugin_DATA = $(plugin_in_files:.gnome-settings-plugin.in=.gnome-settings-plugin)

EXTRA_DIST += $(plugin_in_files) README.config-storage
CLEANFILES =						\
	$(plugin_DATA)					\
	gsd-wacom-resources.c				\
	com.ubuntu.unity-settings-daemon.plugins.wacom.policy	\
	com.ubuntu.unity-settings-daemon.plugins.wacom.policy.in

@GSD_INTLTOOL_PLUGIN_RULE@
