config_h = configure_file(output : 'config.h', configuration : conf)
config_header = declare_dependency(
    compile_args : '-DHAVE_CONFIG_H=1',
    include_directories: include_directories('.')
)
