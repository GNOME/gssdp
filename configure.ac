AC_PREREQ(2.53)
AC_INIT(gssdp, 0.6.5, http://www.gupnp.org/)
AM_INIT_AUTOMAKE()
AC_CONFIG_SRCDIR(libgssdp/gssdp-client.c)
AM_CONFIG_HEADER(config.h)
AM_MAINTAINER_MODE

AC_ISC_POSIX
AC_PROG_CC
AC_STDC_HEADERS
AC_PROG_LIBTOOL

PKG_CHECK_MODULES(LIBGSSDP, glib-2.0 >= 2.18 \
                            gobject-2.0 >= 2.18 \
                            libsoup-2.4)

LIBGLADE_REQUIRED=2.6.0

AC_ARG_WITH( libglade,
  AC_HELP_STRING([--without-libglade],[compile without libglade support]),
  try_libglade=$withval, try_libglade=yes )

HAVE_LIBGLADE=no
if test x$try_libglade = xyes ; then
    dnl check for libglade
    PKG_CHECK_MODULES(LIBGLADE, libglade-2.0 >= $LIBGLADE_REQUIRED ,
      [
        HAVE_LIBGLADE=yes
        AC_SUBST(LIBGLADE_CFLAGS)
        AC_SUBST(LIBGLADE_LIBS)
      ],
      [ 
        HAVE_LIBGLADE=no
        AC_MSG_WARN([libglade $LIBGLADE_REQUIRED or greater not found, some tools will not build.])
      ])
else
    AC_MSG_NOTICE([libglade usage disabled, some tools may not build.])
fi

if test x$HAVE_LIBGLADE = xno; then
  USE_LIBGLADE=no
else
  USE_LIBGLADE=yes
  AC_DEFINE(HAVE_LIBGLADE, , [Use libglade])
fi

AC_SUBST(HAVE_LIBGLADE)
AM_CONDITIONAL(USE_LIBGLADE, test x$USE_LIBGLADE = xyes)

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
