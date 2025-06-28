project('gssdp', 'c', version: '1.6.4', meson_version : '>= 0.54.0', default_options: ['c_std=gnu11'])
gnome = import('gnome')
pkg = import('pkgconfig')

cc = meson.get_compiler('c')

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())

GSSDP_API_VERSION='1.6'
GSSDP_API_NAME='gssdp-@0@'.format(GSSDP_API_VERSION)

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

getipnettable2_available = cc.compiles(files('build-aux/getipnettable2-test.c'), args: '-liphlpapi', name: 'GetIpNetTable2 is available')
conf.set('HAVE_GETIPNETTABLE2', getipnettable2_available)

glib_version = '2.70'
conf.set('GLIB_VERSION_MIN_REQUIRED', 'GLIB_VERSION_@0@'.format(glib_version.underscorify()))
conf.set('GLIB_VERSION_MAX_ALLOWED', 'GLIB_VERSION_@0@'.format(glib_version.underscorify()))



# Generate config.h, so all config has to be set up until here
subdir('internal')

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
    dependency('glib-2.0', version : '>= ' + glib_version),
    dependency('gobject-2.0', version : '>= ' + glib_version),
    dependency('gio-2.0', version : '>= ' + glib_version),
    dependency('libsoup-3.0', version : '>= 2.99.0')
]

subdir('libgssdp')
subdir('tests')

if get_option('sniffer')
  gtk = dependency('gtk4', version : '>= 4')
  subdir('tools')
endif

if get_option('vapi') and get_option('introspection')
  subdir('vala')
endif


gidocgen_dep = dependency('gi-docgen', version: '>= 2021.1',
                          fallback: ['gi-docgen', 'dummy_dep'],
                          native: true,
                          required: get_option('gtk_doc') and get_option('introspection')
                      )

subdir('doc')

if get_option('examples')
  subdir('examples')
endif

if not meson.is_subproject()
  meson.add_dist_script('build-aux/dist-docs.py')
endif

summary({'Version': meson.project_version(),
        'Library version': version,
        'Darwin version': darwin_versions},
        section: 'Versioning')

        
