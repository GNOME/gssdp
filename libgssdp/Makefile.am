# Version format current:revision:age
# If the library source code has changed at all since the last update, then
# increment revision (‘c:r:a’ becomes ‘c:r+1:a’).
# If any interfaces have been added, removed, or changed since the last update,
# increment current, and set revision to 0.
# If any interfaces have been added since the last public release, then
# increment age.
# If any interfaces have been removed since the last public release, then set
# age to 0.

LTVERSION = 2:0:0

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

libgssdp_1_0_la_LDFLAGS = -version-info $(LTVERSION)

libgssdp_1_0_la_SOURCES = gssdp-client.c		\
			  gssdp-client-private.h	\
			  gssdp-error.c			\
			  gssdp-protocol.h		\
			  gssdp-resource-browser.c	\
			  gssdp-resource-group.c	\
			  gssdp-socket-source.c		\
			  gssdp-socket-source.h		\
			  $(BUILT_SOURCES)

libgssdp_1_0_la_LIBADD = $(LIBGSSDP_LIBS)

EXTRA_DIST = gssdp-marshal.list


if HAVE_INTROSPECTION

GSSDP-1.0.gir: libgssdp-1.0.la $(INTROSPECTION_SCANNER)
	$(AM_V_GEN) \
	$(INTROSPECTION_SCANNER) -v \
		--namespace=GSSDP --nsversion=1.0 \
                --include=GObject-2.0 \
		--library=$< \
		--libtool="$(top_builddir)/libtool" \
		--pkg gobject-2.0 \
		--output $@ \
		-I$(top_srcdir) \
		$(addprefix $(top_srcdir)/libgssdp/, $(libgssdpinc_HEADERS)) \
		$(top_srcdir)/libgssdp/gssdp-client.c \
		$(top_srcdir)/libgssdp/gssdp-error.c \
		$(top_srcdir)/libgssdp/gssdp-protocol.h \
		$(top_srcdir)/libgssdp/gssdp-resource-browser.c \
		$(top_srcdir)/libgssdp/gssdp-resource-group.c

girdir = $(datadir)/gir-1.0
gir_DATA = GSSDP-1.0.gir

typelibdir = $(libdir)/girepository-1.0/
typelib_DATA = $(gir_DATA:.gir=.typelib)

%.typelib: %.gir $(INTROSPECTION_COMPILER)
	$(AM_V_GEN) \
	$(INTROSPECTION_COMPILER) \
		--includedir=$(srcdir) \
		--includedir=. \
		$(INTROSPECTION_COMPILER_OPTS) $< -o $(@F)

endif

CLEANFILES = $(BUILT_SOURCES) $(gir_DATA) $(typelib_DATA)
DISTCLEANFILES = $(BUILT_SOURCES)
MAINTAINERCLEANFILES = Makefile.in $(BUILT_SOURCES)
