marshall = gnome.genmarshal(
    'gssdp-marshal',
    sources : 'gssdp-marshal.list',
    prefix : 'gssdp_marshal'
)

headers = files(
    'gssdp-client.h',
    'gssdp-resource-browser.h',
    'gssdp-resource-group.h',
    'gssdp-error.h',
    'gssdp.h',
)

install_headers(headers, subdir : 'gssdp-1.0/libgssdp')

sources = files(
    'gssdp-client.c',
    'gssdp-error.c',
    'gssdp-resource-browser.c',
    'gssdp-resource-group.c',
    'gssdp-socket-source.c',
    'gssdp-socket-functions.c',
)

if pktinfo_available
  sources += 'gssdp-pktinfo-message.c'
  sources += 'gssdp-pktinfo6-message.c'
endif

libgssdp = library('gssdp-1.0', sources + marshall,
    version : '3.0.1',
    dependencies : dependencies + system_deps,
    include_directories : include_directories('..'),
    install : true)

gssdp = declare_dependency(link_with : libgssdp,
                           include_directories: include_directories('..'),
                           dependencies : dependencies
                       )

pkg.generate(libraries: [ libgssdp ],
             subdirs: 'gssdp-1.0',
             version: meson.project_version(),
             name: 'gssdp-1.0',
             filebase : 'gssdp-1.0',
             description: 'GObject-based SSDP library',
             requires_private: ['gobject-2.0', 'gio-2.0'])

if get_option('introspection')
    gir = gnome.generate_gir(libgssdp,
            sources : headers + sources,
            namespace : 'GSSDP',
            nsversion : '1.0',
            symbol_prefix : 'gssdp',
            identifier_prefix : 'GSSDP',
            export_packages : 'gssdp-1.0',
            includes : ['GObject-2.0', 'Gio-2.0', 'Soup-2.4'],
            install: true)
endif
