/*
 * Copyright (C) 2010 Jens Georg <mail@jensge.org>
 *
 * Author: Jens Georg <mail@jensge.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <errno.h>
#include <string.h>

#include <glib.h>

#ifdef G_OS_WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
#endif

#include "gssdp-error.h"
#include "gssdp-socket-functions.h"

static char*
gssdp_socket_error_message (int error) {
#ifdef G_OS_WIN32
        return g_win32_error_message (error);
#else
        return g_strdup (g_strerror (error));
#endif
}

static int
gssdp_socket_errno () {
#ifdef G_OS_WIN32
        return WSAGetLastError ();
#else
        return errno;
#endif
}


static gboolean
gssdp_socket_option_set (GSocket    *socket,
                         int         level,
                         int         option,
                         const void *optval,
                         socklen_t   optlen,
                         GError    **error) {
        int res;

        res = setsockopt (g_socket_get_fd (socket),
                          level,
                          option,
                          optval,
                          optlen);

        if (res == -1) {
                char *message;
                int error_code;

                error_code = gssdp_socket_errno ();
                message = gssdp_socket_error_message (error_code);
                g_set_error_literal (error,
                                     GSSDP_ERROR,
                                     GSSDP_ERROR_FAILED,
                                     message);
                g_free (message);
        }

        return res != -1;
}

gboolean
gssdp_socket_enable_loop (GSocket *socket,
                          gboolean _enable,
                          GError **error) {
#if defined(__OpenBSD__)
        guint8 enable = (guint8) _enable;
#else
        gboolean enable = _enable;
#endif
        return gssdp_socket_option_set (socket,
                                        IPPROTO_IP,
                                        IP_MULTICAST_LOOP,
                                        (char *) &enable,
                                        sizeof (enable),
                                        error);
}

gboolean
gssdp_socket_set_ttl (GSocket *socket,
                      int      _ttl,
                      GError **error) {
#if defined(__OpenBSD__)
        guint8 ttl = (guint8) _ttl;
#else
        int ttl = _ttl;
#endif
        return gssdp_socket_option_set (socket,
                                        IPPROTO_IP,
                                        IP_MULTICAST_TTL,
                                        (char *) &ttl,
                                        sizeof (ttl),
                                        error);
}

gboolean
gssdp_socket_enable_broadcast (GSocket *socket,
                               gboolean enable,
                               GError **error) {
        return gssdp_socket_option_set (socket,
                                        SOL_SOCKET,
                                        SO_BROADCAST,
                                        (char *) &enable,
                                        sizeof (enable),
                                        error);
}

gboolean
gssdp_socket_mcast_interface_set (GSocket      *socket,
                                  GInetAddress *iface_address,
                                  GError      **error) {

        const guint8 *address;
        gsize native_size;

        address = g_inet_address_to_bytes (iface_address);
        native_size = g_inet_address_get_native_size (iface_address);

        return gssdp_socket_option_set (socket,
                                        IPPROTO_IP,
                                        IP_MULTICAST_IF,
                                        (char *) address,
                                        native_size,
                                        error);
}

gboolean
gssdp_socket_reuse_address (GSocket *socket,
                            gboolean enable,
                            GError **error) {
#if defined(G_OS_WIN32) || defined(__OpenBSD__)
        return gssdp_socket_option_set (socket,
                                        SOL_SOCKET,
#if defined(__OpenBSD__)
                                        SO_REUSEPORT,
#else
                                        SO_REUSEADDR,
#endif

                                        (char *) &enable,
                                        sizeof (enable),
                                        error);
#endif
        return TRUE;
}


/*
 * Iface may be NULL if no special interface is wanted
 */
gboolean
gssdp_socket_mcast_group_join (GSocket       *socket,
                               GInetAddress  *group,
                               GInetAddress  *iface,
                               GError       **error) {
        struct ip_mreq mreq;
        GError *inner_error = NULL;
        gboolean result;
#ifdef G_OS_WIN32
        GSocketAddress *local_address;
#endif
        if (group == NULL || ! G_IS_INET_ADDRESS (group)) {
                g_set_error_literal (error,
                                     GSSDP_ERROR,
                                     GSSDP_ERROR_NO_IP_ADDRESS,
                                     "Address is not a valid address");

                return FALSE;
        }

        if (!g_inet_address_get_is_multicast (group)) {
                char *address;

                address = g_inet_address_to_string (group);
                g_set_error (error,
                             GSSDP_ERROR,
                             GSSDP_ERROR_FAILED,
                             "Address '%s' is not a multicast address",
                             address);
                g_free (address);

                return FALSE;
        }

        if (g_inet_address_get_family (group) != G_SOCKET_FAMILY_IPV4) {
                g_set_error_literal (error,
                                     GSSDP_ERROR,
                                     GSSDP_ERROR_FAILED,
                                     "IPv6 not supported");

                return FALSE;
        }
#ifdef G_OS_WIN32
        /* On Window, it is only possible to join multicast groups on a bound
         * socket
         * Note: This test is valid on Windows only. On linux, local_addres
         * will be the ANY address (0.0.0.0 for IPv4)
         */
        local_address = g_socket_get_local_address (socket, &inner_error);
        if (local_address == NULL) {
                g_set_error_literal (error,
                                     GSSDP_ERROR,
                                     GSSDP_ERROR_FAILED,
                                     "Cannot join multicast group;"
                                     "socket is not bound");

                return FALSE;
        }
        g_object_unref (local_address);
#endif

        memset (&mreq, 0, sizeof (struct ip_mreq));
        memcpy (&(mreq.imr_multiaddr),
                g_inet_address_to_bytes (group),
                g_inet_address_get_native_size (group));

        /* if omitted, join will fail if there isn't an explicit multicast
         * route or a default route
         */
        if (iface != NULL)
                memcpy (&(mreq.imr_interface),
                        g_inet_address_to_bytes (iface),
                        g_inet_address_get_native_size (iface));

        result = gssdp_socket_option_set (socket,
                                          IPPROTO_IP,
                                          IP_ADD_MEMBERSHIP,
                                          (char *) &mreq,
                                          sizeof (mreq),
                                          &inner_error);
        if (!result)
                g_propagate_error (error, inner_error);

        return result;
}


