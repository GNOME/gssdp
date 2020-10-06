entities = configuration_data()
entities.set('VERSION', meson.project_version())
configure_file(input: 'version.xml.in',
               output: 'version.xml', configuration:
               entities)

gnome.gtkdoc('gssdp',
             main_xml : 'gssdp-docs.xml',
             gobject_typesfile : 'gssdp.types',
             dependencies : [gssdp, dependencies],
             src_dir : ['libgssdp'],
             ignore_headers : [
                 'gssdp-socket-source.h',
                 'gssdp-client-private.h',
                 'gssdp-protocol.h',
                 'gssdp-pktinfo-message.h',
                 'gssdp-net.h'
             ],
             install : true)
