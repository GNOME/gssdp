AM_CFLAGS = $(LIBGSSDP_CFLAGS) -I$(top_srcdir)

libgssdpincdir = $(includedir)/gssdp-1.0/libgssdp

lib_LTLIBRARIES = libgssdp-1.0.la

libgssdpinc_HEADERS = 	gssdp-device.h		 \
			gssdp-discoverable.h	 \
			gssdp-root-device.h	 \
			gssdp-service.h		 \
			gssdp.h

gssdp-marshal.c: gssdp-marshal.list
	$(GLIB_GENMARSHAL) --prefix=gssdp_marshal $(srcdir)/gssdp-marshal.list --header --body > gssdp-marshal.c

gssdp-marshal.h: gssdp-marshal.list
	$(GLIB_GENMARSHAL) --prefix=gssdp_marshal $(srcdir)/gssdp-marshal.list --header > gssdp-marshal.h

BUILT_SOURCES = gssdp-marshal.c gssdp-marshal.h

libgssdp_1_0_la_SOURCES = gssdp-device.c		\
			  gssdp-device-private.h	\
			  gssdp-discoverable.c		\
			  gssdp-root-device.c		\
			  gssdp-root-device-private.h	\
			  gssdp-service.c		\
			  $(BUILT_SOURCES)

libgssdp_1_0_la_LIBADD = $(LIBGSSDP_LIBS)

EXTRA_DIST = gssdp-marshal.list

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = $(BUILT_SOURCES)
MAINTAINERCLEANFILES = Makefile.in $(BUILT_SOURCES)
