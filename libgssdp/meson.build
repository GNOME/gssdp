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

introspection_sources = headers + [
    'gssdp-client.c',
    'gssdp-error.c',
    'gssdp-error.h',
    'gssdp-resource-browser.c',
    'gssdp-resource-group.c'
    ] + enums
sources = introspection_sources + [
    'gssdp-client-private.h',
    'gssdp-protocol.h',
    'gssdp-socket-source.c',
    'gssdp-socket-source.h',
    'gssdp-socket-functions.c',
    'gssdp-socket-functions.h'
    ]

if pktinfo_available
  sources += [
    'gssdp-pktinfo-message.c',
    'gssdp-pktinfo-message.h'
    ]
endif

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

libgssdp = library(
    'gssdp-1.2',
    sources,
    soversion : 0,
    version : '0.0.0',
    dependencies : deps,
    include_directories : include_directories('..'),
    install : true)

pkg.generate(libraries: [ libgssdp ],
             subdirs: 'gssdp-1.2',
             version: meson.project_version(),
             name: 'gssdp-1.2',
             filebase : 'gssdp-1.2',
             description: 'GObject-based SSDP library',
             requires_private: ['gobject-2.0', 'gio-2.0'])

if get_option('introspection')
    gir = gnome.generate_gir(libgssdp,
            sources : introspection_sources,
            namespace : 'GSSDP',
            nsversion : '1.2',
            symbol_prefix : 'gssdp',
            identifier_prefix : 'GSSDP',
            export_packages : 'gssdp-1.2',
            includes : ['GObject-2.0', 'Gio-2.0', 'Soup-2.4'],
            install: true)
endif
