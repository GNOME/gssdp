project('GSSDP', 'c', version: '1.1.0')
gnome = import('gnome')

as_version = meson.project_version() # set in project() below
ver_arr = as_version.split('.')

as_major_version = ver_arr[0]
as_minor_version = ver_arr[1]
as_micro_version = ver_arr[2]

conf = configuration_data()
conf.set('VERSION', '"@0@"'.format(meson.project_version()))
configure_file(output : 'config.h', configuration : conf)

add_global_arguments('-DHAVE_CONFIG_H=1', language : 'c')

cc = meson.get_compiler('c')
system_deps = []

# Check whether we are compiling against Android libc
bionic_test = '''#include <sys/cdefs.h>
#if !defined(__BIONIC__)
  #error "Not compiling against Android libc"
#endif'''
bionic_available = cc.compiles(bionic_test, name : 'android libc')
if bionic_available
  system_deps += cc.find_library('log', required: true)
endif

# Check whether we are compiling on/against windows
if host_machine.system() == 'windows'
  system_deps += cc.find_library('ws2_32', required: true)
  system_deps += cc.find_library('iphlpapi', required: true)
endif

# Assume "other" unix or linux then
generic_unix = not bionic_available and host_machine.system() != 'windows'

glib_dep = dependency('glib-2.0', version : '>= 2.32')
gobject_dep = dependency('gobject-2.0', version : '>= 2.32')
gio_dep = dependency('gio-2.0', version : '>= 2.32')
soup_dep = dependency('libsoup-2.4', version : '>= 2.26.1')
gtk = dependency('gtk+-3.0', version : '>= 3.12', required: false)

subdir('libgssdp')
subdir('tests')
if (gtk.found())
  subdir('tools')
endif
subdir('vala')
