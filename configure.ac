AC_PREREQ([2.64])
AC_INIT([gssdp],
        [1.0.2],
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

AM_SILENT_RULES([yes])

AX_REQUIRE_DEFINED([GTK_DOC_CHECK])
AX_REQUIRE_DEFINED([GOBJECT_INTROSPECTION_CHECK])
AX_CHECK_ENABLE_DEBUG([yes])

AC_SEARCH_LIBS([strerror],[cposix])
AC_PROG_CC
AC_HEADER_STDC

AX_IS_RELEASE([git-directory])
AX_COMPILER_FLAGS([WARN_CFLAGS])

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

dnl Check whether in_pktinfo is available
AC_CHECK_TYPE(struct in_pktinfo,
              [
               HAVE_PKTINFO=yes
               AC_DEFINE([HAVE_PKTINFO],[1],[Whether we have IP_PKTINFO available])
              ],
              [
               HAVE_PKTINFO=no
              ], [#include <netinet/ip.h>])
AM_CONDITIONAL([HAVE_PKTINFO], [test $HAVE_PKTINFO = yes], [])

dnl Check for if_nametoindex
AC_MSG_CHECKING([for if_nametoindex])
AC_TRY_COMPILE([#include <net/if.h>],
               [if_nametoindex("lo")],
               [HAVE_IFNAMETOINDEX=yes
                AC_DEFINE([HAVE_IFNAMETOINDEX], [1], [Whether we have if_nametoindex])
               ],
               [
                HAVE_IFNAMETOINDEX=no
               ])
AC_MSG_RESULT([$HAVE_IFNAMETOINDEX])

dnl Check for SIOCGIFINDEX
AC_MSG_CHECKING([for SIOCGIFINDEX])
AC_TRY_COMPILE([#include <sys/ioctl.h>],
               [int i = SIOCGIFINDEX],
               [
                HAVE_SIOCGIFINDEX=yes
                AC_DEFINE([HAVE_SIOCGIFINDEX], [1], [Whether we have SIOCGIFINDEX])
               ],
               [
                HAVE_SIOCGIFINDEX=no
               ])
AC_MSG_RESULT([$HAVE_SIOCGIFINDEX])

GTK_DOC_CHECK([1.14], [--flavour no-tmpl])

AC_CONFIG_FILES([
Makefile
libgssdp/Makefile
tools/Makefile
tests/Makefile
examples/Makefile
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
