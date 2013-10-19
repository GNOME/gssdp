#ifndef __GSSDP_HOST_IP_H__
#define __GSSDP_HOST_IP_H__

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

#include <glib.h>

#ifdef G_OS_WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

struct _GSSDPNetworkDevice {
        char *iface_name;
        char *host_ip;
        char *network;
        struct sockaddr_in mask;
};
typedef struct _GSSDPNetworkDevice GSSDPNetworkDevice;

G_GNUC_INTERNAL gboolean gssdp_net_init (GError **error);
G_GNUC_INTERNAL void     gssdp_net_shutdown (void);
G_GNUC_INTERNAL gboolean gssdp_net_get_host_ip (GSSDPNetworkDevice *device);

#endif /* __GSSDP_HOST_IP_H__ */
