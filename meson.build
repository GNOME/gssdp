project('GSSDP', 'c', version: '1.1.0')
gnome = import('gnome')
pkg = import('pkgconfig')

as_version = meson.project_version() # set in project() below
ver_arr = as_version.split('.')

as_major_version = ver_arr[0]
as_minor_version = ver_arr[1]
as_micro_version = ver_arr[2]

cc = meson.get_compiler('c')

conf = configuration_data()
conf.set('VERSION', '"@0@"'.format(meson.project_version()))

# Check for struct in_pktinfo
pktinfo_test = '''#include <netinet/ip.h>
struct in_pktinfo pktinfo;
'''

pktinfo_available = cc.compiles(pktinfo_test,
                                name : 'struct in_pktinfo is available')
if pktinfo_available
  conf.set('HAVE_PKTINFO', '1')
endif

# Check for if_nametoindex
if_nametoindex_test = '''#include <net/if.h>
int main(int argc, char *argv[]) {
  (void) argc;
  (void) argv;
  if_nametoindex("lo");
  return 0;
}
'''

ifnametoindex_available = cc.compiles(if_nametoindex_test,
                                      name : 'if_nametoindex is available')
if ifnametoindex_available
  conf.set('HAVE_IFNAMETOINDEX', '1')
endif

# Check for SIOCGIFINDEX
siocgifindex_test = '''#include <sys/ioctl.h>
static const int foo = SIOCGIFINDEX;
'''
siocgifindex_available = cc.compiles(siocgifindex_test,
                                     name : 'SIOCGIFINDEX is available')
if siocgifindex_available
  conf.set('HAVE_SIOCGIFINDEX', '1')
endif

configure_file(output : 'config.h', configuration : conf)

add_global_arguments('-DHAVE_CONFIG_H=1', language : 'c')

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

if get_option('ui')
  gtk = dependency('gtk+-3.0', version : '>= 3.12', required: false)
endif

gir = find_program('g-ir-scanner', required: false)
vapigen = find_program('vapigen', required: false)

introspection_available = gir.found() and get_option('introspection')
vapi_available = introspection_available and vapigen.found() and get_option('vapi')

subdir('libgssdp')
subdir('tests')
if get_option('ui') and gtk.found()
  subdir('tools')
endif

if vapi_available
  subdir('vala')
endif

if get_option('docs')
  subdir('doc')
endif
