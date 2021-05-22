resource = gnome.compile_resources(
    'org.gupnp.GSSDP.DeviceSniffer',
    'gssdp-device-sniffer.gresource.xml'
)

sniffer = executable(
    'gssdp-device-sniffer',
    [
        'gssdp-device-sniffer.c',
        resource
    ],
    dependencies : [gssdp, gtk],
    install: true,
    export_dynamic : true,
    gui_app : true
)
