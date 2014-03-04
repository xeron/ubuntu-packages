AM_CPPFLAGS =						\
	$(GLIB_CFLAGS)					\
	$(SOUP_CFLAGS)					\
	$(GDKPIXBUF_CFLAGS)				\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"		\
	-DG_LOG_DOMAIN=\"AppDataTools\"

bin_PROGRAMS =						\
	appdata-validate

appdata_validate_SOURCES =				\
	appdata-common.c				\
	appdata-common.h				\
	appdata-problem.c				\
	appdata-problem.h				\
	appdata-validate.c

appdata_validate_LDADD =				\
	$(GDKPIXBUF_LIBS)				\
	$(SOUP_LIBS)					\
	$(GLIB_LIBS)

appdata_validate_CFLAGS =				\
	$(WARNINGFLAGS_C)

check_PROGRAMS =					\
	appdata-self-test

appdata_self_test_SOURCES =				\
	appdata-common.c				\
	appdata-common.h				\
	appdata-problem.c				\
	appdata-problem.h				\
	appdata-self-test.c

appdata_self_test_LDADD =				\
	$(GDKPIXBUF_LIBS)				\
	$(SOUP_LIBS)					\
	$(GLIB_LIBS)

appdata_self_test_CFLAGS = $(AM_CFLAGS) $(WARNINGFLAGS_C)

TESTS = appdata-self-test

clean-local:
	rm -f *~

MAINTAINERCLEANFILES =					\
	*~						\
	*.trs						\
	*.log						\
	Makefile.in

-include $(top_srcdir)/git.mk
