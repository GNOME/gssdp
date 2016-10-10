# Version format current:revision:age
# If the library source code has changed at all since the last update, then
# increment revision (‘c:r:a’ becomes ‘c:r+1:a’).
# If any interfaces have been added, removed, or changed since the last update,
# increment current, and set revision to 0.
# If any interfaces have been added since the last public release, then
# increment age.
# If any interfaces have been removed since the last public release, then set
# age to 0.

LTVERSION = 0:0:0

AM_CFLAGS = $(LIBGSSDP_CFLAGS) -I$(top_srcdir) $(WARN_CFLAGS)

libgssdpincdir = $(includedir)/gssdp-1.2/libgssdp

lib_LTLIBRARIES = libgssdp-1.2.la

libgssdpinc_HEADERS = 	gssdp-client.h		 \
			gssdp-error.h		 \
			gssdp-resource-browser.h \
			gssdp-resource-group.h	 \
			gssdp.h \
			gssdp-enums.h

gssdp-marshal.c: gssdp-marshal.list
	$(AM_V_GEN) \
	$(GLIB_GENMARSHAL) --prefix=gssdp_marshal $(srcdir)/gssdp-marshal.list --header --body > gssdp-marshal.c

gssdp-marshal.h: gssdp-marshal.list
	$(AM_V_GEN) \
	$(GLIB_GENMARSHAL) --prefix=gssdp_marshal $(srcdir)/gssdp-marshal.list --header > gssdp-marshal.h

enumheaders = $(srcdir)/gssdp-error.h

BUILT_SOURCES = gssdp-marshal.c \
	gssdp-marshal.h \
	gssdp-enums.c \
	gssdp-enums.h

gssdp-enums.c: $(enumheaders) Makefile
	$(AM_V_GEN)$(GLIB_MKENUMS) \
		--identifier-prefix GSSDP \
		--symbol-prefix gssdp \
		--fhead "#include \"gssdp-enums.h\"\n\n" \
		--fprod "/* enumerations from \"@filename@\" */\n" \
		--fprod "#include \"@filename@\"\n" \
		--vhead "GType\n@enum_name@_get_type (void)\n{\n" \
		--vhead "    static GType type = 0;\n\n" \
		--vhead "    if (!type)\n    {\n" \
		--vhead "        static const G@Type@Value _@enum_name@_values[] = {" \
		--vprod "            { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
		--vtail "            { 0, NULL, NULL }\n        };\n\n" \
		--vtail "        type = g_@type@_register_static (\"@EnumName@\", _@enum_name@_values);\n    }\n\n" \
		--vtail "  return type;\n}\n\n" \
		$(enumheaders) > $@

gssdp-enums.h: $(enumheaders) Makefile
	$(AM_V_GEN)$(GLIB_MKENUMS) \
		--identifier-prefix GSSDP \
		--symbol-prefix gssdp \
		--fhead "#ifndef GSSDP_ENUMS_H\n" \
		--fhead "#define GSSDP_ENUMS_H\n\n" \
		--fhead "#include <glib-object.h>\n\n" \
		--fhead "G_BEGIN_DECLS\n\n" \
		--fprod "/* enumerations from @filename@ */\n" \
		--vhead "GType @enum_name@_get_type (void);\n" \
		--vhead "#define GSSDP_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
		--ftail "G_END_DECLS\n\n" \
		--ftail "#endif /* GSSDP_ENUMS_H */" \
		$(enumheaders) > $@

introspection_sources = $(libgssdpinc_HEADERS)	\
			gssdp-client.c		\
			gssdp-error.c		\
			gssdp-resource-browser.c\
			gssdp-resource-group.c

libgssdp_1_2_la_LDFLAGS = -version-info $(LTVERSION) $(WARN_LDFLAGS)
libgssdp_1_2_la_SOURCES = $(introspection_sources)	\
			  gssdp-client-private.h	\
			  gssdp-protocol.h		\
			  gssdp-socket-source.c		\
			  gssdp-socket-source.h		\
			  gssdp-socket-functions.c	\
			  gssdp-socket-functions.h	\
			  $(BUILT_SOURCES)

if HAVE_PKTINFO
libgssdp_1_2_la_SOURCES += gssdp-pktinfo-message.c \
						   gssdp-pktinfo-message.h
endif

libgssdp_1_2_la_LIBADD = $(LIBGSSDP_LIBS)

if OS_WIN32

libgssdp_1_2_la_LIBADD += -lws2_32 -liphlpapi
libgssdp_1_2_la_LDFLAGS += -no-undefined
endif

if TARGET_ANDROID
libgssdp_1_2_la_LIBADD += -llog
endif

EXTRA_DIST = gssdp-marshal.list

CLEANFILES = $(BUILT_SOURCES)

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = \
			     --symbol-prefix=gssdp		\
			     --identifier-prefix=GSSDP		\
			     --add-include-path=$(top_srcdir) \
			     --pkg-export=gssdp-1.2 \
			     $(WARN_SCANNERFLAGS)
INTROSPECTION_COMPILER_ARGS = --includedir=$(top_srcdir)


if HAVE_INTROSPECTION
GSSDP-1.2.gir: libgssdp-1.2.la
GSSDP_1_2_gir_INCLUDES = GObject-2.0 Gio-2.0 Soup-2.4
GSSDP_1_2_gir_CFLAGS = $(INCLUDES)
GSSDP_1_2_gir_LIBS = libgssdp-1.2.la
GSSDP_1_2_gir_FILES = $(introspection_sources)
GSSDP_1_2_gir_NAMESPACE = GSSDP
GSSDP_1_2_gir_VERSION = 1.2
INTROSPECTION_GIRS += GSSDP-1.2.gir

girdir = $(datadir)/gir-1.0
gir_DATA = GSSDP-1.2.gir

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(gir_DATA:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)
endif

DISTCLEANFILES = $(BUILT_SOURCES)
MAINTAINERCLEANFILES = Makefile.in $(BUILT_SOURCES)
