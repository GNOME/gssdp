if HAVE_INTROSPECTION
if HAVE_VAPIGEN
gssdp-1.0.vapi: gssdp-1.0.stamp
	@true

gssdp-1.0.stamp: $(top_builddir)/libgssdp/GSSDP-1.0.gir \
		$(srcdir)/GSSDP-1.0.metadata \
		$(srcdir)/gssdp-1.0-custom.vala
	$(AM_V_GEN)$(VAPIGEN) --metadatadir=$(srcdir) \
			      --library=gssdp-1.0 --pkg=gio-2.0 --pkg=libsoup-2.4 \
			      $(top_builddir)/libgssdp/GSSDP-1.0.gir \
				  $(srcdir)/gssdp-1.0-custom.vala && \
	echo timestmap > $(@F)

vapidir = $(datadir)/vala/vapi
vapi_DATA = gssdp-1.0.vapi $(srcdir)/gssdp-1.0.deps
CLEANFILES = gssdp-1.0.vapi gssdp-1.0.stamp
EXTRA_DIST = gssdp-1.0.deps GSSDP-1.0.metadata gssdp-1.0-custom.vala
endif
endif
