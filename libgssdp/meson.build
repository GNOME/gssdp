enums = gnome.mkenums(
    'gssdp-enums',
    sources : [
        'gssdp-error.h',
        'gssdp-client.h'
    ],
    identifier_prefix : 'GSSDP',
    symbol_prefix : 'gssdp',
    c_template : 'gssdp-enums.c.template',
    h_template : 'gssdp-enums.h.template',
    install_header : true,
    install_dir : join_paths(get_option('includedir'), GSSDP_API_NAME, 'libgssdp')
)

headers = files(
    'gssdp-client.h',
    'gssdp-resource-browser.h',
    'gssdp-resource-group.h',
    'gssdp-error.h',
    'gssdp.h',
)

install_headers(headers, subdir : GSSDP_API_NAME / 'libgssdp')

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

if host_machine.system() == 'windows'
  sources += 'gssdp-net-win32.c'
endif

if bionic_available
  sources += 'gssdp-net-bionic.c'
endif

if generic_unix
  sources += 'gssdp-net-posix.c'
endif

v = meson.project_version().split('.')
soversion = 0
library_minor = v[0].to_int() * 100 + v[1].to_int()
library_micro = v[2].to_int()

version = '@0@.@1@.@2@'.format(soversion, library_minor, library_micro)
version_arr = version.split('.')
major_version = version_arr[0].to_int()
minor_version = version_arr[1].to_int()
micro_version = version_arr[2].to_int()
current = major_version + minor_version + 1
interface_age = micro_version
darwin_versions = [current, '@0@.@1@'.format(current, interface_age)]

libgssdp = library(
    GSSDP_API_NAME,
    sources + enums,
    version : version,
    darwin_versions : darwin_versions,
    dependencies : dependencies + system_deps,
    include_directories : [include_directories('..'), config_h_inc],
    install : true)

gssdp = declare_dependency(link_with : libgssdp,
                           include_directories: include_directories('..'),
                           dependencies : dependencies
                       )

meson.override_dependency(GSSDP_API_NAME, gssdp)

pkg.generate(libgssdp,
             subdirs: GSSDP_API_NAME,
             version: meson.project_version(),
             name: GSSDP_API_NAME,
             requires: ['gio-2.0', 'glib-2.0', 'gobject-2.0'],
             filebase : GSSDP_API_NAME,
             description: 'GObject-based SSDP library',
             requires_private: ['gobject-2.0', 'gio-2.0'])

if get_option('introspection')
    gssdp_gir_dir = meson.current_build_dir()
    gir = gnome.generate_gir(libgssdp,
            sources : headers + sources + enums,
            namespace : 'GSSDP',
            nsversion : GSSDP_API_VERSION,
            header : 'libgssdp/gssdp.h',
            symbol_prefix : 'gssdp',
            identifier_prefix : 'GSSDP',
            export_packages : GSSDP_API_NAME,
            includes : ['GObject-2.0', 'Gio-2.0', 'Soup-3.0'],
            install: true)
endif
