NULL =

apidir = $(includedir)/unity-settings-daemon-$(GSD_API_VERSION)/unity-settings-daemon
api_DATA = gsd-enums.h

@INTLTOOL_XML_NOMERGE_RULE@

@GSETTINGS_RULES@

@INTLTOOL_DESKTOP_RULE@
desktopdir = $(sysconfdir)/xdg/autostart
desktop_in_files = unity-settings-daemon.desktop.in.in
desktop_DATA = $(desktop_in_files:.desktop.in.in=.desktop)

unity-settings-daemon.desktop.in: unity-settings-daemon.desktop.in.in
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@.tmp && mv $@.tmp $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = unity-settings-daemon.pc libunity-settings-daemon.pc

@INTLTOOL_XML_NOMERGE_RULE@

EXTRA_DIST = 					\
	$(convert_DATA)				\
	$(desktop_in_files)			\
	$(gsettings_ENUM_FILES)			\
	unity-settings-daemon.pc.in		\
	libunity-settings-daemon.pc.in		\
	$(api_DATA)				\
	$(NULL)

DISTCLEANFILES = 			\
	$(desktop_DATA)			\
	unity-settings-daemon.desktop.in \
	$(NULL)

MAINTAINERCLEANFILES =			\
	*~				\
	Makefile.in
