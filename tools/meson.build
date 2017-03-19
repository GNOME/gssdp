sniffer_uidir = join_paths(get_option('datadir'), 'gssdp')

sniffer = executable('gssdp-device-sniffer',
                     ['gssdp-device-sniffer.c'],
                     dependencies : [gssdp, gtk],
                     install: true,
                     c_args : '-DUI_DIR="@0@"'.format(sniffer_uidir)
                     )
install_data('gssdp-device-sniffer.ui', install_dir : sniffer_uidir)
