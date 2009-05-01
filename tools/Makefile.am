uidir = $(datadir)/gssdp
dist_ui_DATA = $(srcdir)/gssdp-device-sniffer.ui

AM_CFLAGS = $(LIBGSSDP_CFLAGS) $(LIBGTK_CFLAGS) -I$(top_srcdir) -DUI_DIR='"$(uidir)"'

bin_PROGRAMS = gssdp-device-sniffer

gssdp_device_sniffer_SOURCES = gssdp-device-sniffer.c
gssdp_device_sniffer_LDADD = $(LIBGSSDP_LIBS) $(LIBGTK_LIBS) $(top_builddir)/libgssdp/libgssdp-1.0.la
gssdp_device_sniffer_LDFLAGS = -export-dynamic

MAINTAINERCLEANFILES = Makefile.in
