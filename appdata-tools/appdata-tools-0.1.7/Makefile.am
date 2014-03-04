ACLOCAL_AMFLAGS = -I m4

SUBDIRS = 						\
	data						\
	po						\
	src

snapshot:
	$(MAKE) dist distdir=$(PACKAGE)-$(VERSION)-`date +"%Y%m%d"`

DISTCHECK_CONFIGURE_FLAGS =				\
	--enable-schemas

MAINTAINERCLEANFILES =					\
	*~						\
	aclocal.m4					\
	appdata-tools-*.tar.xz				\
	ChangeLog					\
	compile						\
	config.guess					\
	config.h.*					\
	config.sub					\
	configure					\
	depcomp						\
	gtk-doc.make					\
	INSTALL						\
	install-sh					\
	ltmain.sh					\
	Makefile.in					\
	missing						\
	mkinstalldirs					\
	test-driver

EXTRA_DIST =						\
	COPYING						\
	MAINTAINERS					\
	AUTHORS						\
	INSTALL						\
	README						\
	NEWS						\
	autogen.sh					\
	config.h

distclean-local:
	if test $(srdcir) = .; then :; else \
		rm -f ChangeLog; \
	fi

ChangeLog:
	@echo Creating $@
	@if test -d "$(srcdir)/.git"; then \
	  (GIT_DIR=$(top_srcdir)/.git ./missing --run git log 075acbe26f88ac044af50885bcce57d697e17f24.. --stat -M -C --name-status  --date=short --no-color) | fmt --split-only > $@.tmp \
	  && mv -f $@.tmp $@ \
	  || ($(RM) $@.tmp; \
	      echo Failed to generate ChangeLog, your ChangeLog may be outdated >&2; \
	      (test -f $@ || echo git-log is required to generate this file >> $@)); \
	else \
	  test -f $@ || \
	  (echo A git checkout and git-log is required to generate ChangeLog >&2 && \
	  echo A git checkout and git-log is required to generate this file >> $@); \
	fi

.PHONY: ChangeLog

uninstall-am:
	$(RM) -R $(DESTDIR)$(localstatedir)/lib/zif

-include $(top_srcdir)/git.mk
