AM_CFLAGS = $(LIBGSSDP_CFLAGS) -I$(top_srcdir)

libgssdpincdir = $(includedir)/gssdp-1.0/libgssdp

lib_LTLIBRARIES = libgssdp-1.0.la

libgssdpinc_HEADERS = 	gssdp-device.h		\
			gssdp-discoverable.h	\
			gssdp-root-device.h	\
			gssdp-service.h		\
			gssdp.h

libgssdp_1_0_la_SOURCES =

libgssdp_1_0_la_LIBADD = $(LIBGSSDP_LIBS)

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = $(BUILT_SOURCES)
MAINTAINERCLEANFILES = Makefile.in $(BUILT_SOURCES)
