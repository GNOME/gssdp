TESTS_ENVIRONMENT = G_SLICE=debug-blocks \
		    LD_LIBRARY_PATH=$(top_builddir)/libgssdp/.libs:$(LD_LIBRARY_PATH)

TESTS=$(check_PROGRAMS)

check_PROGRAMS = test-regression test-functional

noinst_LIBRARIES = libtestutil.a

libtestutil_a_SOURCES = test-util.h test-util.c

test_regression_SOURCES = test-regression.c
test_functional_SOURCES = test-functional.c

LDADD = \
	$(top_builddir)/libgssdp/libgssdp-1.0.la \
	libtestutil.a \
	$(LIBGSSDP_LIBS)

AM_CFLAGS = \
	    $(LIBGSSDP_CFLAGS) \
	    -I $(top_srcdir) \
	    -DDATA_PATH="\"$(srcdir)\""

