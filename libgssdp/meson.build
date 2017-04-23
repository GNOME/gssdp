enums = gnome.mkenums('gssdp-enums', sources : 'gssdp-error.h',
                      identifier_prefix : 'GSSDP',
                      symbol_prefix : 'gssdp',
                      c_template : 'gssdp-enums.c.template',
                      h_template : 'gssdp-enums.h.template'
                      )

headers = [
    'gssdp-client.h',
    'gssdp-resource-browser.h',
    'gssdp-resource-group.h',
    'gssdp.h'
    ]
install_headers(headers, subdir : 'gssdp-1.0/libgssdp')

sources = [
    'gssdp-client.c',
    'gssdp-error.c',
    'gssdp-resource-browser.c',
    'gssdp-resource-group.c',
    'gssdp-client-private.h',
    'gssdp-protocol.h',
    'gssdp-socket-source.c',
    'gssdp-socket-source.h',
    'gssdp-socket-functions.c',
    'gssdp-socket-functions.h'
    ] + enums

if host_machine.system() == 'windows'
  sources += 'gssdp-net-win32.c'
endif

if bionic_available
  sources += 'gssdp-net-bionic.c'
endif

if generic_unix
  sources += 'gssdp-net-posix.c'
endif

deps = [glib_dep, gobject_dep, gio_dep, soup_dep] + system_deps

libgssdp = shared_library('gssdp-1.2', sources,
    soversion : 0,
    version : '0.0.0',
    dependencies : deps,
    include_directories : include_directories('..'),
    install : true)
