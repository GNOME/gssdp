if USE_LIBGLADE
TOOLS_DIR = tools
else
TOOLS_DIR =
endif

SUBDIRS = libgssdp $(TOOLS_DIR) tests po doc

pkgconfig_DATA= gssdp-1.0.pc gssdp-1.0-uninstalled.pc
pkgconfigdir = $(libdir)/pkgconfig

EXTRA_DIST = gssdp-1.0.pc.in gssdp-1.0-uninstalled.pc.in

# Extra clean files so that maintainer-clean removes *everything*
MAINTAINERCLEANFILES = aclocal.m4 compile config.guess config.sub configure depcomp install-sh ltmain.sh Makefile.in missing config.h.in
