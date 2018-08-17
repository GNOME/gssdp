enums = gnome.mkenums('gssdp-enums', sources : 'gssdp-error.h',
                      identifier_prefix : 'GSSDP',
                      symbol_prefix : 'gssdp',
                      c_template : 'gssdp-enums.c.template',
                      h_template : 'gssdp-enums.h.template',
                      install_header : true,
                      install_dir : join_paths(get_option('includedir'), 'gssdp-1.2/libgssdp')
                      )

headers = files(
    'gssdp-client.h',
    'gssdp-resource-browser.h',
    'gssdp-resource-group.h',
    'gssdp-error.h',
    'gssdp.h',
)

install_headers(headers, subdir : 'gssdp-1.2/libgssdp')

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

libgssdp = library('gssdp-1.2', sources + enums,
    soversion : 0,
    version : '0.0.0',
    dependencies : dependencies + system_deps,
    include_directories : include_directories('..'),
    install : true)

gssdp = declare_dependency(link_with : libgssdp,
                           include_directories: include_directories('..'),
                           dependencies : dependencies
                       )

pkg.generate(libraries: [ libgssdp ],
             subdirs: 'gssdp-1.2',
             version: meson.project_version(),
             name: 'gssdp-1.2',
             filebase : 'gssdp-1.2',
             description: 'GObject-based SSDP library',
             requires_private: ['gobject-2.0', 'gio-2.0'])

if introspection_available
    gir = gnome.generate_gir(libgssdp,
            sources : headers + sources + enums,
            namespace : 'GSSDP',
            nsversion : '1.2',
            header : 'libgssdp/gssdp.h',
            symbol_prefix : 'gssdp',
            identifier_prefix : 'GSSDP',
            export_packages : 'gssdp-1.2',
            includes : ['GObject-2.0', 'Gio-2.0', 'Soup-2.4'],
            install: true)
endif
