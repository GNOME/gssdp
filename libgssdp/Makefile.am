AM_CFLAGS = $(LIBGSSDP_CFLAGS) -I$(top_srcdir)

libgssdpincdir = $(includedir)/gssdp-1.0/libgssdp

lib_LTLIBRARIES = libgssdp-1.0.la

libgssdpinc_HEADERS = 	gssdp-client.h		 \
			gssdp-error.h		 \
			gssdp-service-browser.h  \
			gssdp-service-group.h	 \
			gssdp.h

gssdp-marshal.c: gssdp-marshal.list
	$(GLIB_GENMARSHAL) --prefix=gssdp_marshal $(srcdir)/gssdp-marshal.list --header --body > gssdp-marshal.c

gssdp-marshal.h: gssdp-marshal.list
	$(GLIB_GENMARSHAL) --prefix=gssdp_marshal $(srcdir)/gssdp-marshal.list --header > gssdp-marshal.h

BUILT_SOURCES = gssdp-marshal.c gssdp-marshal.h

libgssdp_1_0_la_SOURCES = gssdp-client.c		\
			  gssdp-client-private.h	\
			  gssdp-error.c			\
			  gssdp-protocol.h		\
			  gssdp-service-browser.c	\
			  gssdp-service-group.c		\
			  gssdp-socket-source.c		\
			  gssdp-socket-source.h		\
			  $(BUILT_SOURCES)

libgssdp_1_0_la_LIBADD = $(LIBGSSDP_LIBS)

EXTRA_DIST = gssdp-marshal.list

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = $(BUILT_SOURCES)
MAINTAINERCLEANFILES = Makefile.in $(BUILT_SOURCES)
