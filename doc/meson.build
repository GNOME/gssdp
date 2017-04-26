entities = configuration_data()
entities.set('VERSION', meson.project_version())
configure_file(input: 'version.xml.in',
               output: 'version.xml', configuration:
               entities)

gnome.gtkdoc('gssdp',
             main_xml : 'gssdp-docs.xml',
             src_dir : [join_paths(meson.source_root(), 'libgssdp'),
                        join_paths(meson.build_root(), 'libgssdp')],
             dependencies : libgssdp,
             install : true)
