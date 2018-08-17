sniffer_uidir = join_paths(get_option('datadir'), 'gssdp-1.2')

sniffer = executable('gssdp-device-sniffer',
                     ['gssdp-device-sniffer.c'],
                     dependencies : dependencies + [gtk],
                     include_directories : include_directories('..'),
                     link_with: libgssdp,
                     install: true,
                     c_args : '-DUI_DIR="@0@"'.format(sniffer_uidir)
                     )
install_data('gssdp-device-sniffer.ui', install_dir : sniffer_uidir)
