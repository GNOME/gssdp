AC_PREREQ(2.53)
AC_INIT(gssdp, 0.8.2, http://www.gupnp.org/)
AM_INIT_AUTOMAKE()
AC_CONFIG_SRCDIR(libgssdp/gssdp-client.c)
AM_CONFIG_HEADER(config.h)

m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])],)

AC_ISC_POSIX
AC_PROG_CC
AC_STDC_HEADERS
AC_PROG_LIBTOOL
AC_CONFIG_MACRO_DIR([m4])

PKG_CHECK_MODULES(LIBGSSDP, glib-2.0 >= 2.18 \
                            gobject-2.0 >= 2.18 \
                            libsoup-2.4)

LIBGTK_REQUIRED=2.12.0

AC_ARG_WITH( gtk,
  AC_HELP_STRING([--without-gtk],[compile without gtk+ support]),
  try_gtk=$withval, try_gtk=yes )

HAVE_LIBGTK=no
if test x$try_gtk = xyes ; then
    dnl check for gtk+
    PKG_CHECK_MODULES(LIBGTK, gtk+-2.0 >= $LIBGTK_REQUIRED ,
      [
        HAVE_LIBGTK=yes
        AC_SUBST(LIBGTK_CFLAGS)
        AC_SUBST(LIBGTK_LIBS)
      ],
      [ 
        HAVE_LIBGTK=no
        AC_MSG_WARN([gtk+ $LIBGTK_REQUIRED or greater not found, some tools will not build.])
      ])
else
    AC_MSG_NOTICE([gtk+ usage disabled, some tools may not build.])
fi

if test x$HAVE_LIBGTK = xno; then
  USE_LIBGTK=no
else
  USE_LIBGTK=yes
  AC_DEFINE(HAVE_LIBGTK, , [Use gtk+])
fi

AC_SUBST(HAVE_LIBGTK)
AM_CONDITIONAL(USE_LIBGTK, test x$USE_LIBGTK = xyes)

# glib-genmarshal
GLIB_GENMARSHAL=`pkg-config --variable=glib_genmarshal glib-2.0`
AC_SUBST(GLIB_GENMARSHAL)

# Debugging
AC_ARG_ENABLE(debug,
	[  --enable-debug          enable debugging],,
        enable_debug=no)
if test "x$enable_debug" = "xyes"; then
        CFLAGS="$CFLAGS -g -Wall -Werror"
fi

GOBJECT_INTROSPECTION_CHECK([0.6.4])

GTK_DOC_CHECK([1.0])

AC_OUTPUT([
Makefile
libgssdp/Makefile
tools/Makefile
tests/Makefile
doc/Makefile
doc/version.xml
gssdp-1.0.pc
gssdp-1.0-uninstalled.pc
])
