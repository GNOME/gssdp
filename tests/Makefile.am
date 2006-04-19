AM_CFLAGS = $(LIBGSSDP_CFLAGS) -I$(top_srcdir)

bin_PROGRAMS = gssdp-test

gssdp_test_SOURCES = gssdp-test.c
gssdp_test_LDADD   = $(top_builddir)/libgssdp/libgssdp-1.0.la

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = $(BUILT_SOURCES)
MAINTAINERCLEANFILES = Makefile.in $(BUILT_SOURCES)
