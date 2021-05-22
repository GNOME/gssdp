project('gssdp', 'c', version: '1.2.4', meson_version : '>= 0.54.0')
gnome = import('gnome')
pkg = import('pkgconfig')

cc = meson.get_compiler('c')

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())

# Check for struct in_pktinfo
pktinfo_test = '''#define _GNU_SOURCE
#ifdef __APPLE__
#define __APPLE_USE_RFC_3542
#endif
#include <netinet/ip.h>
struct in_pktinfo pktinfo;
struct in6_pktinfo pktinfo6;
'''

pktinfo_available = cc.compiles(pktinfo_test,
                                name : 'struct in_pktinfo is available')
conf.set('HAVE_PKTINFO', pktinfo_available)

# Check for if_nametoindex
ifnametoindex_available = cc.has_function(
    'if_nametoindex',
    prefix : '#include <net/if.h>'
)
conf.set('HAVE_IFNAMETOINDEX', ifnametoindex_available)

# Check for SIOCGIFINDEX
siocgifindex_test = '''#include <sys/ioctl.h>
static const int foo = SIOCGIFINDEX;
'''
siocgifindex_available = cc.compiles(siocgifindex_test,
                                     name : 'SIOCGIFINDEX is available')
conf.set('HAVE_SIOCGIFINDEX', siocgifindex_available)

configure_file(output : 'config.h', configuration : conf)

add_project_arguments('-DHAVE_CONFIG_H=1', language : 'c')

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

dependencies = [
    dependency('glib-2.0', version : '>= 2.54'),
    dependency('gobject-2.0', version : '>= 2.54'),
    dependency('gio-2.0', version : '>= 2.54'),
    dependency('libsoup-2.4', version : '>= 2.26.1')
]

subdir('libgssdp')
subdir('tests')

if get_option('sniffer')
  gtk = dependency('gtk+-3.0', version : '>= 3.12')
  subdir('tools')
endif

if get_option('vapi') and get_option('introspection')
  subdir('vala')
endif

if get_option('gtk_doc')
  subdir('doc')
endif

if get_option('examples')
  subdir('examples')
endif
