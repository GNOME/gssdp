if HAVE_INTROSPECTION
if HAVE_VAPIGEN
gssdp-1.2.vapi: gssdp-1.2.stamp
	@true

gssdp-1.2.stamp: $(top_builddir)/libgssdp/GSSDP-1.2.gir \
		$(srcdir)/GSSDP-1.2.metadata
	$(AM_V_GEN)$(VAPIGEN) --metadatadir=$(srcdir) \
			      --library=gssdp-1.2 --pkg=gio-2.0 --pkg=libsoup-2.4 \
			      $(top_builddir)/libgssdp/GSSDP-1.2.gir \
	&& echo timestmap > $(@F)

vapidir = $(datadir)/vala/vapi
vapi_DATA = gssdp-1.2.vapi $(srcdir)/gssdp-1.2.deps
CLEANFILES = gssdp-1.2.vapi gssdp-1.2.stamp
EXTRA_DIST = gssdp-1.2.deps GSSDP-1.2.metadata gssdp-1.2.vapi gssdp-1.2.stamp
endif
endif
