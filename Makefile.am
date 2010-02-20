if USE_LIBGTK
TOOLS_DIR = tools
else
TOOLS_DIR =
endif

SUBDIRS = libgssdp $(TOOLS_DIR) tests doc

ACLOCAL_AMFLAGS=${ACLOCAL_FLAGS} -I m4

pkgconfig_DATA= gssdp-1.0.pc
pkgconfigdir = $(libdir)/pkgconfig

EXTRA_DIST = gssdp-1.0.pc.in gssdp-1.0-uninstalled.pc.in

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

# Extra clean files so that maintainer-clean removes *everything*
MAINTAINERCLEANFILES = aclocal.m4 compile config.guess config.sub configure depcomp install-sh ltmain.sh Makefile.in missing config.h.in
