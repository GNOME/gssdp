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
	$(GLIB_GENMARSHAL) --prefix=gssdp_marshal $(srcdir)/gssdp-marshal.list --header --body > gssdp-marshal.c

gssdp-marshal.h: gssdp-marshal.list
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

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = $(BUILT_SOURCES)
MAINTAINERCLEANFILES = Makefile.in $(BUILT_SOURCES)
