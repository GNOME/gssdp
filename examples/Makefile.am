AM_CFLAGS = $(LIBGSSDP_CFLAGS) -I$(top_srcdir)

noinst_PROGRAMS = test-browser test-publish

test_browser_SOURCES = test-browser.c
test_browser_LDADD   = $(top_builddir)/libgssdp/libgssdp-1.0.la $(LIBGSSDP_LIBS)

test_publish_SOURCES = test-publish.c
test_publish_LDADD   = $(top_builddir)/libgssdp/libgssdp-1.0.la $(LIBGSSDP_LIBS)

CLEANFILES = $(BUILT_SOURCES)
DISTCLEANFILES = $(BUILT_SOURCES)
MAINTAINERCLEANFILES = Makefile.in $(BUILT_SOURCES)
