AC_PREREQ([2.64])
AC_INIT([gssdp],
        [0.14.7],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=gupnp&component=gssdp],
        [gssdp],
        [http://www.gupnp.org/])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR(libgssdp/gssdp-client.c)
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([1.11 tar-ustar no-dist-gzip dist-xz])
AM_MAINTAINER_MODE([enable])

GNOME_COMPILE_WARNINGS

AM_SILENT_RULES([yes])

AC_SEARCH_LIBS([strerror],[cposix])
AC_PROG_CC
AC_HEADER_STDC

LT_PREREQ([2.2])
LT_INIT

PKG_CHECK_MODULES(LIBGSSDP, glib-2.0 >= 2.32 \
                            gobject-2.0 >= 2.32 \
                            gio-2.0 >= 2.32 \
                            libsoup-2.4 >= 2.26.1)

LIBGTK_REQUIRED=3.0

AC_ARG_WITH( gtk,
  AS_HELP_STRING([--without-gtk],[compile without gtk+ support]),
  try_gtk=$withval, try_gtk=yes )

HAVE_LIBGTK=no
if test x$try_gtk = xyes ; then
    dnl check for gtk+
    PKG_CHECK_MODULES(LIBGTK, gtk+-3.0 >= $LIBGTK_REQUIRED ,
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
        CFLAGS="$CFLAGS -g -Wall"
fi

AC_ARG_ENABLE([strict-c],
              AS_HELP_STRING([--enable-strict-c],[Error out on warnings]),,
              [enable_strict_c=no])
AS_IF([test "x$[]enable_strict_c" = "xyes"],
      [CFLAGS="$CFLAGS -Werror"])

GOBJECT_INTROSPECTION_CHECK([0.6.7])

# vapigen
have_vapigen=no
AS_IF([test "x$found_introspection" = "xyes"],
      [
       dnl output of g-i >= 1.36.0 needs vapigen >= 0.20 to process
       AC_MSG_CHECKING([whether G-I is 1.36 or newer])
       VAPIGEN_MIN_VERSION=0.14
       AS_IF([pkg-config --atleast-version=1.36.0 gobject-introspection-1.0],
             [
              AC_MSG_RESULT([yes])
              VAPIGEN_MIN_VERSION=0.20
             ],[AC_MSG_RESULT([no])]
       )
       GUPNP_PROG_VAPIGEN([$VAPIGEN_MIN_VERSION])
      ]
)
AS_IF([test "x$VAPIGEN" != "x"], [have_vapigen=yes])
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

dnl Check for Android
AC_MSG_CHECKING([for Android as target OS])
target_android=no
case "$target_os" in
    linux-android*)
        target_android=yes
        ;;
    *)
        target_android=no
        ;;
esac
AC_MSG_RESULT([$target_android])
AM_CONDITIONAL(TARGET_ANDROID, [test $target_android = yes])

GTK_DOC_CHECK([1.14], [--flavour no-tmpl])

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

echo "
    ${PACKAGE} ${VERSION}
    ---------------------

    Prefix:                ${prefix}
    GObject-Introspection: ${found_introspection}
    VALA bindings:         ${have_vapigen}
"
