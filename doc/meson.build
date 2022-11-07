entities = configuration_data()
entities.set('VERSION', meson.project_version())

gidocgen = find_program('gi-docgen', required: true)

gssdp_toml = configure_file (
    input: 'gssdp.toml.in',
    output: 'gssdp.toml',
    configuration: entities
)

docs_dir = join_paths(get_option('prefix'), get_option('datadir'), 'doc')

custom_target(
    'gssdp-doc',
    input: [ gssdp_toml, gir[0] ],
    output: 'gssdp-1.2',
    command : [
        gidocgen,
        'generate',
        '--quiet',
        '--add-include-path=@0@'.format(meson.current_build_dir() / '../libgssdp'),
        '--config', gssdp_toml,
        '--output-dir=@OUTPUT@',
        '--no-namespace-dir',
        '--content-dir=@0@'.format(meson.current_source_dir()),
        '@INPUT1@',
    ],
    depend_files : gssdp_toml,
    build_by_default: true,
    install: true,
    install_dir : docs_dir,
)

