# Version format current:revision:age
# If the library source code has changed at all since the last update, then
# increment revision (‘c:r:a’ becomes ‘c:r+1:a’).
# If any interfaces have been added, removed, or changed since the last update,
# increment current, and set revision to 0.
# If any interfaces have been added since the last public release, then
# increment age.
# If any interfaces have been removed since the last public release, then set
# age to 0.

LTVERSION = 3:0:0

AM_CFLAGS = $(LIBGSSDP_CFLAGS) -I$(top_srcdir)

libgssdpincdir = $(includedir)/gssdp-1.0/libgssdp

lib_LTLIBRARIES = libgssdp-1.0.la

libgssdpinc_HEADERS = 	gssdp-client.h		 \
			gssdp-error.h		 \
			gssdp-resource-browser.h \
			gssdp-resource-group.h	 \
			gssdp.h

gssdp-marshal.c: gssdp-marshal.list
	$(AM_V_GEN) \
	$(GLIB_GENMARSHAL) --prefix=gssdp_marshal $(srcdir)/gssdp-marshal.list --header --body > gssdp-marshal.c

gssdp-marshal.h: gssdp-marshal.list
	$(AM_V_GEN) \
	$(GLIB_GENMARSHAL) --prefix=gssdp_marshal $(srcdir)/gssdp-marshal.list --header > gssdp-marshal.h

BUILT_SOURCES = gssdp-marshal.c gssdp-marshal.h

introspection_sources = $(libgssdpinc_HEADERS)	\
			gssdp-client.c		\
			gssdp-error.c		\
			gssdp-resource-browser.c\
			gssdp-resource-group.c

libgssdp_1_0_la_LDFLAGS = -version-info $(LTVERSION)
libgssdp_1_0_la_SOURCES = $(introspection_sources)	\
			  gssdp-client-private.h	\
			  gssdp-protocol.h		\
			  gssdp-socket-source.c		\
			  gssdp-socket-source.h		\
			  gssdp-socket-functions.c	\
			  gssdp-socket-functions.h	\
			  $(BUILT_SOURCES)

libgssdp_1_0_la_LIBADD = $(LIBGSSDP_LIBS)

if OS_WIN32

libgssdp_1_0_la_LIBADD += -lws2_32 -liphlpapi
libgssdp_1_0_la_LDFLAGS += -no-undefined
endif

EXTRA_DIST = gssdp-marshal.list

CLEANFILES = $(BUILT_SOURCES)

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --warn-all				\
			     --symbol-prefix=gssdp		\
			     --identifier-prefix=GSSDP		\
			     --add-include-path=$(top_srcdir) \
			     --pkg-export=gssdp-1.0
INTROSPECTION_COMPILER_ARGS = --includedir=$(top_srcdir)


if HAVE_INTROSPECTION
GSSDP-1.0.gir: libgssdp-1.0.la
GSSDP_1_0_gir_INCLUDES = GObject-2.0 Gio-2.0
GSSDP_1_0_gir_CFLAGS = $(INCLUDES)
GSSDP_1_0_gir_LIBS = libgssdp-1.0.la
GSSDP_1_0_gir_FILES = $(introspection_sources)
GSSDP_1_0_gir_NAMESPACE = GSSDP
GSSDP_1_0_gir_VERSION = 1.0
INTROSPECTION_GIRS += GSSDP-1.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = GSSDP-1.0.gir

typelibdir = $(libdir)/girepository-1.0/
typelib_DATA = $(gir_DATA:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif

DISTCLEANFILES = $(BUILT_SOURCES)
MAINTAINERCLEANFILES = Makefile.in $(BUILT_SOURCES)
