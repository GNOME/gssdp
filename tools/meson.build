resource = gnome.compile_resources(
    'org.gupnp.GSSDP.DeviceSniffer',
    'gssdp-device-sniffer.gresource.xml',
    source_dir: [meson.current_source_dir(), meson.current_source_dir() / '..' / 'doc']
)

sniffer = executable(
    'gssdp-device-sniffer',
    [
        'gssdp-device-sniffer.c',
        'main-window.c',
        'main-window.h',
        resource
    ],
    dependencies : [gssdp, gtk],
    install: true,
    export_dynamic : true,
    gui_app : true
)
