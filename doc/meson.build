if get_option('gtk_doc')
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
      output: GSSDP_API_NAME,
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
      depend_files : [gssdp_toml, files('gssdp-device-sniffer.md')],
      build_by_default: true,
      install: true,
      install_dir : docs_dir,
  )
endif

if get_option('manpages')
  pandoc = find_program('pandoc', required: true)
  custom_target(
    'manpages',
    input: files('gssdp-device-sniffer.md'),
    output: 'gssdp-device-sniffer.1',
    command: [
      pandoc,
      '@INPUT@',
      meson.current_source_dir() / 'metadata.yml',
      '-s',
      '-t', 'man',
      '-o', '@OUTPUT@'
    ],
    depend_files : files('gssdp-device-sniffer.md', 'metadata.yml'),
    build_by_default: true,
    install: true,
    install_dir: join_paths(get_option('prefix'), get_option('mandir'), 'man1'),
  )
endif
