resource = gnome.compile_resources(
    'org.gupnp.GSSDP.DeviceSniffer',
    'gssdp-device-sniffer.gresource.xml'
)

sniffer_deps = [glib_dep, gobject_dep, gio_dep, gtk, soup_dep]
sniffer = executable(
    'gssdp-device-sniffer',
    [
        'gssdp-device-sniffer.c',
        resource
    ],
    dependencies : sniffer_deps,
    include_directories : include_directories('..'),
    link_with: libgssdp,
    install: true,
    export_dynamic : true
)

