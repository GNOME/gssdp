gssdp_device_sniffer_sources = [
  'main.c',
  'gssdp-device-sniffer-window.c',
]

gnome = import('gnome')

gssdp_device_sniffer_sources += gnome.compile_resources('gssdp-device-sniffer-resources',
  'gssdp-device-sniffer.gresource.xml',
  c_name: 'gssdp_device_sniffer'
)

executable('gssdp-device-sniffer', gssdp_device_sniffer_sources,
    dependencies : [gssdp, gtk],
    install: true,
    export_dynamic : true,
    gui_app : true
)
