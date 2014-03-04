AC_PREREQ(2.63)

AC_INIT([appdata-tools],[0.1.7])
AC_CONFIG_SRCDIR(src)
AM_INIT_AUTOMAKE([1.9 no-dist-gzip dist-xz tar-ustar])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_SYS_LARGEFILE

# enable nice build output on automake1.11
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AS_ALL_LINGUAS
AC_PROG_CC
AC_PROG_INSTALL
AC_HEADER_STDC
LT_INIT
AM_PROG_CC_C_O

# Internationalisation
IT_PROG_INTLTOOL([0.35.0])
GETTEXT_PACKAGE=appdata-tools
AC_SUBST([GETTEXT_PACKAGE])
AM_GLIB_GNU_GETTEXT
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["$GETTEXT_PACKAGE"],[gettext domain])

dnl ---------------------------------------------------------------------------
dnl - Extra verbose warning switches
dnl ---------------------------------------------------------------------------
if test "$GCC" = "yes"; then
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wall"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wcast-align -Wno-uninitialized"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-declarations"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wpointer-arith"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wcast-align"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wwrite-strings"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Winit-self"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wreturn-type"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wformat-nonliteral"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wformat-security"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-include-dirs"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-format-attribute"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wclobbered"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wempty-body"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wignored-qualifiers"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wsign-compare"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wtype-limits"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wuninitialized"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Waggregate-return"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wdeclaration-after-statement"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wshadow"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wno-strict-aliasing"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Winline"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-parameter-type"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Woverride-init"
else
	WARNINGFLAGS_C=""
fi
AC_SUBST(WARNINGFLAGS_C)

PKG_CHECK_MODULES(GLIB, glib-2.0 >= 2.14.0 gobject-2.0 gthread-2.0)
PKG_CHECK_MODULES(SOUP, libsoup-2.4)
PKG_CHECK_MODULES(GDKPIXBUF, gdk-pixbuf-2.0)

AC_ARG_ENABLE(man,
              [AS_HELP_STRING([--disable-man],
                              [Disable building man page])],,
              enable_man=maybe)
AS_IF([test "x$enable_man" != "xno"],
  [AC_CHECK_PROG([have_xsltproc], [xsltproc], [yes], [no])
   JH_CHECK_XML_CATALOG([-//OASIS//DTD DocBook XML V4.3//EN],
     [DocBook XML DTD V4.3], [have_docbookdtd=yes], [have_docbookdtd=no])
   JH_CHECK_XML_CATALOG(
     [http://docbook.sourceforge.net/release/xsl/current/html/refentry.xsl],
     [DocBook XSL Stylesheets], [have_docbookxsl=yes], [have_docbookxsl=no])
   AS_IF([test "x$have_xsltproc" = "xyes" -a "x$have_docbookdtd" = "xyes" \
     -a "x$have_docbookxsl" = "xyes"],
     [have_manutils=yes],
     [AS_IF([test "x$enable_man" = "xyes"],
       [AC_MSG_ERROR(
         [manpage generation requested but required utilities were not found])])
      have_manutils=no])],
  [have_manutils=no])
AM_CONDITIONAL([ENABLE_MAN], [test "x$have_manutils" = "xyes"])

AC_ARG_ENABLE(schemas,
              [AS_HELP_STRING([--enable-schemas],
                              [generate schemas [default=auto]])],,
              enable_schemas=maybe)
AS_IF([test "$enable_schemas" != no], [
  AC_PATH_PROG([TRANG], [trang])
  AS_IF([test -z "$TRANG"], [
    AS_IF([test "$enable_schemas" = yes], [
      AC_MSG_ERROR([trang is required for --enable-schemas])
    ])
    enable_schemas=no
  ])
  AM_PATH_PYTHON(,[
    AX_PYTHON_MODULE([lxml])
    AS_IF([test "$HAVE_PYMOD_LXML" != yes], [
      AS_IF([test "$enable_schemas" = yes], [
        AC_MSG_ERROR([lxml $pythondir is required for --enable-schemas])
      ])
      enable_schemas=no
    ])
  ], [
    AS_IF([test "$enable_schemas" = yes], [
      AC_MSG_ERROR([python is required for --enable-schemas])
      enable_schemas=no
    ])
  ])
])
AM_CONDITIONAL(ENABLE_SCHEMAS, test "$enable_schemas" != no)

dnl ---------------------------------------------------------------------------
dnl - Makefiles, etc.
dnl ---------------------------------------------------------------------------
AC_CONFIG_FILES([
Makefile
po/Makefile.in
src/Makefile
data/Makefile
])
AC_OUTPUT

dnl ==========================================================================
echo "
                    appdata-tools $VERSION
                  =======================

        prefix:                    ${prefix}
        datadir:                   ${datadir}
        compiler:                  ${CC}
        cflags:                    ${CFLAGS}
        cppflags:                  ${CPPFLAGS}
"

