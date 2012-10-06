AC_PREREQ([2.64])
AC_INIT([gssdp],
        [0.13.0],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=gupnp&component=gssdp],
        [gssdp],
        [http://www.gupnp.org/])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR(libgssdp/gssdp-client.c)
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AM_INIT_AUTOMAKE([1.11 tar-ustar no-dist-gzip dist-xz])
AM_MAINTAINER_MODE([enable])

AM_SILENT_RULES([yes])

AC_SEARCH_LIBS([strerror],[cposix])
AC_PROG_CC
AC_HEADER_STDC

LT_PREREQ([2.2])
LT_INIT

PKG_CHECK_MODULES(LIBGSSDP, glib-2.0 >= 2.22 \
                            gobject-2.0 >= 2.22 \
                            gio-2.0 >= 2.22 \
                            libsoup-2.4 >= 2.26.1)

LIBGTK_REQUIRED=2.12.0

AC_ARG_WITH( gtk,
  AS_HELP_STRING([--without-gtk],[compile without gtk+ support]),
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

GOBJECT_INTROSPECTION_CHECK([0.6.7])

# vapigen
AS_IF([test "x$found_introspection" = "xyes"],
      [
       VALA_PROG_VAPIGEN([0.14])
      ]
)
AM_CONDITIONAL([HAVE_VAPIGEN], [test "x$VAPIGEN" != "x"])


dnl Check for Win32
AC_MSG_CHECKING([for Win32])
case "$host" in
    *-*-mingw*)
        os_win32=yes
        ;;
    *)
        os_win32=no
        ;;
esac
AC_MSG_RESULT([$os_win32])
AM_CONDITIONAL(OS_WIN32, [test $os_win32 = yes])

GTK_DOC_CHECK([1.0])

AC_CONFIG_FILES([
Makefile
libgssdp/Makefile
tools/Makefile
tests/Makefile
tests/gtest/Makefile
vala/Makefile
doc/Makefile
doc/version.xml
gssdp-1.0.pc
gssdp-1.0-uninstalled.pc
])
AC_OUTPUT
