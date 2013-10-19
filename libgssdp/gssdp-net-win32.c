#define _WIN32_WINNT 0x502
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
typedef int socklen_t;
/* from the return value of inet_addr */

#include "gssdp-error.h"
#include "gssdp-net.h"

gboolean
gssdp_net_init (GError **error)
{
        WSADATA wsaData = {0};
        if (WSAStartup (MAKEWORD (2,2), &wsaData) != 0) {
                gchar *message;

                message = g_win32_error_message (WSAGetLastError ());
                g_set_error_literal (error,
                                     GSSDP_ERROR,
                                     GSSDP_ERROR_FAILED,
                                     message);
                g_free (message);

                return FALSE;
        }
}

void
gssdp_net_shutdown (void)
{
        WSACleanup ();
}

static gboolean
is_primary_adapter (PIP_ADAPTER_ADDRESSES adapter)
{
        int family =
                adapter->FirstUnicastAddress->Address.lpSockaddr->sa_family;

        return !(adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK ||
                 family == AF_INET6);
}

static gboolean
extract_address_and_prefix (PIP_ADAPTER_UNICAST_ADDRESS  adapter,
                            PIP_ADAPTER_PREFIX           prefix,
                            char                        *iface,
                            char                        *network) {
        DWORD ret = 0;
        DWORD len = INET6_ADDRSTRLEN;

        ret = WSAAddressToStringA (adapter->Address.lpSockaddr,
                                   adapter->Address.iSockaddrLength,
                                   NULL,
                                   iface,
                                   &len);
        if (ret != 0)
                return FALSE;

        if (prefix) {
                ret = WSAAddressToStringA (prefix->Address.lpSockaddr,
                                           prefix->Address.iSockaddrLength,
                                           NULL,
                                           network,
                                           &len);
                if (ret != 0)
                        return FALSE;
        } else if (strcmp (iface, "127.0.0.1"))
                strcpy (network, "127.0.0.0");
        else
                return FALSE;

        return TRUE;
}

gboolean
gssdp_net_get_host_ip (GSSDPNetworkDevice *device)
{
        GList *up_ifaces = NULL, *ifaceptr = NULL;
        ULONG flags = GAA_FLAG_INCLUDE_PREFIX |
                      GAA_FLAG_SKIP_DNS_SERVER |
                      GAA_FLAG_SKIP_MULTICAST;
        DWORD size = 15360; /* Use 15k buffer initially as documented in MSDN */
        DWORD ret;
        PIP_ADAPTER_ADDRESSES adapters_addresses;
        PIP_ADAPTER_ADDRESSES adapter;

        do {
                adapters_addresses = (PIP_ADAPTER_ADDRESSES) g_malloc0 (size);
                ret = GetAdaptersAddresses (AF_UNSPEC,
                                            flags,
                                            NULL,
                                            adapters_addresses,
                                            &size);
                if (ret == ERROR_BUFFER_OVERFLOW)
                        g_free (adapters_addresses);
        } while (ret == ERROR_BUFFER_OVERFLOW);

        if (ret == ERROR_SUCCESS)
                for (adapter = adapters_addresses;
                     adapter != NULL;
                     adapter = adapter->Next) {
                        if (adapter->FirstUnicastAddress == NULL)
                                continue;
                        if (adapter->OperStatus != IfOperStatusUp)
                                continue;
                        /* skip Point-to-Point devices */
                        if (adapter->IfType == IF_TYPE_PPP)
                                continue;

                        if (device->iface_name != NULL &&
                            strcmp (device->iface_name, adapter->AdapterName) != 0)
                                continue;

                        /* I think that IPv6 is done via pseudo-adapters, so
                         * that there are either IPv4 or IPv6 addresses defined
                         * on the adapter.
                         * Loopback-Devices and IPv6 go to the end of the list,
                         * IPv4 to the front
                         */
                        if (is_primary_adapter (adapter))
                                up_ifaces = g_list_prepend (up_ifaces, adapter);
                        else
                                up_ifaces = g_list_append (up_ifaces, adapter);
                }

        for (ifaceptr = up_ifaces;
             ifaceptr != NULL;
             ifaceptr = ifaceptr->next) {
                char ip[INET6_ADDRSTRLEN];
                char prefix[INET6_ADDRSTRLEN];
                const char *p, *q;
                PIP_ADAPTER_ADDRESSES adapter;
                PIP_ADAPTER_UNICAST_ADDRESS address;

                p = NULL;

                adapter = (PIP_ADAPTER_ADDRESSES) ifaceptr->data;
                address = adapter->FirstUnicastAddress;

                if (address->Address.lpSockaddr->sa_family != AF_INET)
                        continue;

                if (extract_address_and_prefix (address,
                                                adapter->FirstPrefix,
                                                ip,
                                                prefix)) {
                                                p = ip;
                                                q = prefix;
                }

                if (p != NULL) {
                        device->host_ip = g_strdup (p);
                        /* This relies on the compiler doing an arithmetic
                         * shift here!
                         */
                        gint32 mask = 0;
                        if (adapter->FirstPrefix->PrefixLength > 0) {
                                mask = (gint32) 0x80000000;
                                mask >>= adapter->FirstPrefix->PrefixLength - 1;
                        }
                        device->mask.sin_family = AF_INET;
                        device->mask.sin_port = 0;
                        device->mask.sin_addr.s_addr = htonl ((guint32) mask);

                        if (device->iface_name == NULL)
                                device->iface_name = g_strdup (adapter->AdapterName);
                        if (device->network == NULL)
                                device->network = g_strdup (q);
                        break;
                }

        }
        g_list_free (up_ifaces);
        g_free (adapters_addresses);

        return TRUE;
}
