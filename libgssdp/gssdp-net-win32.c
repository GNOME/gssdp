/*
 * Copyright (C) 2016 Jens Georg <mail@jensge.org>
 *
 * Author: Jens Georg <mail@jensge.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#define _WIN32_WINNT 0x601
#include <config.h>

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
        gchar *message = NULL;

        if (WSAStartup (MAKEWORD (2,2), &wsaData) == 0) {
            return TRUE;
        }


        message = g_win32_error_message (WSAGetLastError ());
        g_set_error_literal (error,
                             GSSDP_ERROR,
                             GSSDP_ERROR_FAILED,
                             message);
        g_free (message);

        return FALSE;
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

int
gssdp_net_query_ifindex (GSSDPNetworkDevice *device)
{
        gunichar2 *wname =
                g_utf8_to_utf16 (device->iface_name, -1, NULL, NULL, NULL);
        CLSID clsid;
        HRESULT hr = CLSIDFromString (wname, &clsid);
        g_free (wname);

        if (FAILED (hr)) {
                return -1;
        }

        NET_LUID luid;
        if (!NETIO_SUCCESS (ConvertInterfaceGuidToLuid ((const GUID *) &clsid,
                                                        &luid))) {
                return -1;
        }

        NET_IFINDEX ifindex;
        if (!NETIO_SUCCESS (ConvertInterfaceLuidToIndex (&luid, &ifindex))) {
                return -1;
        }

        return ifindex;
}

char *
gssdp_net_mac_lookup (GSSDPNetworkDevice *device, const char *ip_address)
{
#ifdef HAVE_GETIPNETTABLE2
        char *result = NULL;
        GInetAddress *address = g_inet_address_new_from_string (ip_address);

        PMIB_IPNET_TABLE2 pipTable = NULL;

        unsigned long rc = 0;
        GSocketFamily family = g_inet_address_get_family (address);
        rc = GetIpNetTable2 (family, &pipTable);
        if (rc != NO_ERROR) {
                g_object_unref (address);
                g_warning (
                        "Failed to GetIpNetTable2 for %s: %s",
                        g_enum_to_string (g_socket_family_get_type (), family),
                        g_win32_error_message (WSAGetLastError ()));
                return g_strdup (ip_address);
        }

        for (guint i = 0; i < pipTable->NumEntries; i++) {
                GInetAddress *adapter_address;
                if (family == G_SOCKET_FAMILY_IPV4) {
                        adapter_address = g_inet_address_new_from_bytes (
                                (guint8 *) &pipTable->Table[i]
                                        .Address.Ipv4.sin_addr,
                                family);
                } else {
                        adapter_address = g_inet_address_new_from_bytes (
                                (guint8 *) &pipTable->Table[i]
                                        .Address.Ipv6.sin6_addr,
                                family);
                }
                char *ip = g_inet_address_to_string (adapter_address);
                if (!g_inet_address_equal (address, adapter_address)) {
                        g_object_unref (adapter_address);
                        continue;
                }
                g_free (ip);

                if (pipTable->Table[i].PhysicalAddressLength == 0) {
                        result = g_strdup (ip_address);
                        break;
                }
                gssize j;
                GString *mac_str = g_string_new ("");
                for (j = 0; j < pipTable->Table[i].PhysicalAddressLength; j++) {
                        if (j > 0) {
                                g_string_append_c (mac_str, ':');
                        }
                        g_string_append_printf (
                                mac_str,
                                "%02x",
                                pipTable->Table[i].PhysicalAddress[j]);
                }

                result = g_string_free (mac_str, FALSE);
                break;
        }
        g_clear_pointer (&pipTable, FreeMibTable);
        g_object_unref (address);

        return result;
#else
        return g_strdup (ip_address);
#endif
}

gboolean
gssdp_net_get_host_ip (GSSDPNetworkDevice *device, GError **error)
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
                PIP_ADAPTER_UNICAST_ADDRESS address;
                PIP_ADAPTER_PREFIX address_prefix;

                p = NULL;

                adapter = (PIP_ADAPTER_ADDRESSES) ifaceptr->data;

                for (address_prefix = adapter->FirstPrefix;
                     address_prefix != NULL;
                     address_prefix = address_prefix->Next)
                        if (address_prefix->Address.lpSockaddr->sa_family == AF_INET)
                                break;

                if (address_prefix == NULL)
                        continue;

                for (address = adapter->FirstUnicastAddress;
                     address != NULL;
                     address = address->Next) {
                        if (address->Address.lpSockaddr->sa_family != AF_INET)
                                continue;

                        if (extract_address_and_prefix (address,
                                                        address_prefix,
                                                        ip,
                                                        prefix)) {
                                p = ip;
                                q = prefix;
                        }

                        if (p != NULL) {
                                GInetAddress *mask_addr;

                                device->host_ip = g_strdup (p);
                                device->host_addr = g_inet_address_new_from_string (device->host_ip);
                                mask_addr = g_inet_address_new_from_string (q);
                                device->host_mask = g_inet_address_mask_new (mask_addr, 
                                                                            address_prefix->PrefixLength,
                                                                            NULL);
                                g_object_unref (mask_addr);

                                if (device->iface_name == NULL)
                                        device->iface_name = g_strdup (adapter->AdapterName);
                                if (device->network == NULL)
                                        device->network = g_strdup (q);

                                device->index = gssdp_net_query_ifindex (device);
                                break;
                        }
                }

                if (p != NULL)
                        break;

        }
        g_list_free (up_ifaces);
        g_free (adapters_addresses);

        return TRUE;
}

GList *
gssdp_net_list_devices (void)
{
        return NULL;
}
