uidir = $(datadir)/gssdp
ui_DATA = gssdp-device-sniffer.glade

AM_CFLAGS = $(LIBGUPNP_CFLAGS) $(LIBGLADE_CFLAGS) -I$(top_srcdir) -DUI_DIR='"$(uidir)"'

bin_PROGRAMS = gssdp-device-sniffer

gssdp_device_sniffer_SOURCES = gssdp-device-sniffer.c
gssdp_device_sniffer_LDADD = $(LIBGLADE_LIBS) $(LIBGUPNP_LIBS) $(top_builddir)/libgssdp/libgssdp-1.0.la
gssdp_device_sniffer_LDFLAGS = -export-dynamic

MAINTAINERCLEANFILES = Makefile.in
