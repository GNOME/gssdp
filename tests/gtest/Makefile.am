TESTS_ENVIRONMENT = G_SLICE=debug-blocks \
		    LD_LIBRARY_PATH=$(top_builddir)/libgssdp/.libs

TESTS=$(check_PROGRAMS)

check_PROGRAMS = test-regression

test_regression_SOURCES = test-regression.c

LDADD = \
	$(top_builddir)/libgssdp/libgssdp-1.0.la \
	$(LIBGSSDP_LIBS)

AM_CFLAGS = \
	    $(LIBGSSDP_CFLAGS) \
	    -I $(top_srcdir) \
	    -DDATA_PATH="\"$(srcdir)\""

