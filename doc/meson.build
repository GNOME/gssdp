entities = configuration_data()
entities.set('VERSION', meson.project_version())
configure_file(input: 'version.xml.in',
               output: 'version.xml', configuration:
               entities)

gnome.gtkdoc('gssdp',
             main_sgml : 'gssdp-docs.sgml',
             src_dir : [join_paths(meson.source_root(), 'libgssdp'),
                        join_paths(meson.build_root(), 'libgssdp')],
             ignore_headers : [
                 'gssdp-socket-source.h',
                 'gssdp-client-private.h',
                 'gssdp-protocol.h',
                 'gssdp-pktinfo-message.h',
                 'gssdp-net.h'
             ],
             dependencies : libgssdp,
             install : true)
