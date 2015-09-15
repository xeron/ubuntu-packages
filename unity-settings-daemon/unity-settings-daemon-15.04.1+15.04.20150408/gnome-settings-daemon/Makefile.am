NULL =

INCLUDES = \
	-DDATADIR=\""$(datadir)"\"				\
	-DGNOME_SETTINGS_LOCALEDIR=\""$(datadir)/locale"\"	\
	-DLIBEXECDIR=\""$(libexecdir)"\" 			\
	-DGNOME_SETTINGS_PLUGINDIR=\""$(plugindir)"\"		\
	-I$(top_srcdir)/plugins/common/			\
	$(WARN_CFLAGS)						\
	$(DISABLE_DEPRECATED_CFLAGS)				\
	$(SETTINGS_DAEMON_CFLAGS)				\
	$(LIBNOTIFY_CFLAGS)					\
	$(GNOME_DESKTOP_CFLAGS)					\
	$(NULL)

privlibdir = $(pkglibdir)-$(GSD_API_VERSION)

lib_LTLIBRARIES = libunity-settings-daemon.la
libexec_PROGRAMS = check_gl_texture_size

libunity_settings_daemon_la_SOURCES =		\
	gsd-pnp-ids.c \
	gsd-pnp-ids.h \
	gsd-rr.c \
	gsd-rr.h \
	gsd-rr-config.c \
	gsd-rr-config.h \
	gsd-rr-output-info.c \
	gsd-rr-private.h \
	display-name.c \
	edid-parse.c \
	edid.h \
	gsd-idle-monitor.c \
	gsd-idle-monitor.h

nodist_libunity_settings_daemon_la_SOURCES =		\
	$(dbus_idle_built_sources)

libunity_settings_daemon_include_HEADERS =		\
	gsd-pnp-ids.h \
	gsd-rr.h \
	gsd-rr-config.h \
	gsd-idle-monitor.h

libunity_settings_daemon_includedir = $(includedir)/unity-settings-daemon-$(GSD_API_VERSION)/libunity-settings-daemon

libunity_settings_daemon_la_CFLAGS = 		\
	-DLIBEXECDIR=\""$(libexecdir)\""			\
	-DPNP_IDS=\""$(datadir)/hwdata/pnp.ids"\"			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	$(LIBUNITY_SETTINGS_DAEMON_CFLAGS)

libunity_settings_daemon_la_LIBADD = 		\
	-lm \
	$(LIBUNITY_SETTINGS_DAEMON_LIBS)

libunity_settings_daemon_la_LDFLAGS = 		\
	-version-info 1:0:0 \
	-export-symbols-regex \^gsd_.*

BUILT_SOURCES = $(dbus_idle_built_sources)

dbus_idle_built_sources = meta-dbus-idle-monitor.c meta-dbus-idle-monitor.h

$(dbus_idle_built_sources) : Makefile.am idle-monitor.xml
	$(AM_V_GEN)gdbus-codegen                                                        \
		--interface-prefix org.gnome.Mutter                                     \
		--c-namespace MetaDBus                                                  \
		--generate-c-code meta-dbus-idle-monitor                                \
		--c-generate-object-manager                                             \
		$(srcdir)/idle-monitor.xml

check_gl_texture_size_CPPFLAGS = \
	$(CHECK_GL_TEXTURE_SIZE_CFLAGS)

check_gl_texture_size_LDADD = \
	$(CHECK_GL_TEXTURE_SIZE_LIBS)

privlib_LTLIBRARIES =		\
	libgsd.la		\
	$(NULL)

libgsd_la_SOURCES =		\
	gnome-settings-profile.c	\
	gnome-settings-profile.h	\
	gnome-settings-session.c	\
	gnome-settings-session.h	\
	$(NULL)

libgsd_la_CPPFLAGS = 		\
	$(DISABLE_DEPRECATED_CFLAGS)	\
	$(NULL)

libgsd_la_CFLAGS =		\
	$(NULL)

libgsd_la_LIBADD =		\
	$(SETTINGS_DAEMON_LIBS)		\
	$(GIOUNIX_LIBS)		\
	$(NULL)

libgsd_la_LDFLAGS =		\
	-export-dynamic		\
	-avoid-version		\
	-no-undefined		\
	$(NULL)

gsddir = $(libexecdir)

gsd_PROGRAMS = \
	unity-settings-daemon

gsd_SCRIPTS = \
	unity-settings-daemon-localeexec

EXTRA_DIST = \
	unity-settings-daemon-localeexec.in \
	idle-monitor.xml

unity-settings-daemon-localeexec: unity-settings-daemon-localeexec.in
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" -e "s|\@prefix\@|$(prefix)|"  $< > $@.tmp && mv $@.tmp $@

apidir   = $(includedir)/unity-settings-daemon-$(GSD_API_VERSION)/unity-settings-daemon
api_DATA = 				\
	gnome-settings-plugin.h

unity_settings_daemon_SOURCES =		\
	main.c				\
	gnome-settings-manager.c	\
	gnome-settings-manager.h	\
	gnome-settings-plugin.c		\
	gnome-settings-plugin.h		\
	gnome-settings-plugin-info.c	\
	gnome-settings-plugin-info.h	\
	gnome-settings-module.c		\
	gnome-settings-module.h		\
	$(NULL)

unity_settings_daemon_CPPFLAGS = \
	$(AM_CPPFLAGS)

unity_settings_daemon_CFLAGS = 		\
	$(AM_CFLAGS)

unity_settings_daemon_LDFLAGS = 	\
	$(AM_LDFLAGS)

unity_settings_daemon_LDADD = 		\
	libgsd.la		\
	libunity-settings-daemon.la		\
	$(SETTINGS_DAEMON_LIBS)		\
	$(LIBNOTIFY_LIBS)		\
	$(GNOME_DESKTOP_LIBS)		\
	$(NULL)

CLEANFILES = $(gsd_SCRIPTS)             \
	$(dbus_idle_built_sources)

# vim: ts=8
