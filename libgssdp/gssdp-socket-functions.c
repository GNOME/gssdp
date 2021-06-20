/*
 * Copyright (C) 2010 Jens Georg <mail@jensge.org>
 *
 * Author: Jens Georg <mail@jensge.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#include <config.h>

#ifdef __APPLE__
#define __APPLE_USE_RFC_3542
#endif

#include "gssdp-error.h"
#include "gssdp-socket-functions.h"
#include "gssdp-pktinfo-message.h"

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

#include "gssdp-pktinfo6-message.h"
static char*
gssdp_socket_error_message (int error) {
#ifdef G_OS_WIN32
        return g_win32_error_message (error);
#else
        return g_strdup (g_strerror (error));
#endif
}

static int
gssdp_socket_errno (void) {
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
gssdp_socket_mcast_interface_set (GSocket      *socket,
                                  GInetAddress *iface_address,
                                  gint          index,
                                  GError      **error) {

        const guint8 *address;
        gsize native_size;

        if (g_inet_address_get_family (iface_address) == G_SOCKET_FAMILY_IPV6) {
                return gssdp_socket_option_set (socket,
                                                IPPROTO_IPV6,
                                                IPV6_MULTICAST_IF,
                                                (char *)&index,
                                                sizeof (index),
                                                error);
        } else {
                address = g_inet_address_to_bytes (iface_address);
                native_size = g_inet_address_get_native_size (iface_address);

                return gssdp_socket_option_set (socket,
                                                IPPROTO_IP,
                                                IP_MULTICAST_IF,
                                                (char *) address,
                                                native_size,
                                                error);
        }
}

#define __GSSDP_UNUSED(x) (void)(x)

gboolean
gssdp_socket_reuse_address (GSocket *socket,
                            gboolean enable,
                            GError **error) {
#if defined(G_OS_WIN32) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
        return gssdp_socket_option_set (socket,
                                        SOL_SOCKET,
#if defined(__OpenBSD__) || defined (__NetBSD__) || defined (__FreeBSD__) || defined(__FreeBSD_kernel__)
                                        SO_REUSEPORT,
#else
                                        SO_REUSEADDR,
#endif

                                        (char *) &enable,
                                        sizeof (enable),
                                        error);
#else
        __GSSDP_UNUSED(socket);
        __GSSDP_UNUSED(enable);
        __GSSDP_UNUSED(error);
#endif
        return TRUE;
}

gboolean
gssdp_socket_enable_info         (GSocket *socket,
                                  GSocketFamily family,
                                  gboolean enable,
                                  GError **error)
{
#ifdef HAVE_PKTINFO
        /* Register the type so g_socket_control_message_deserialize() will
         * find it */
        g_type_ensure (GSSDP_TYPE_PKTINFO_MESSAGE);
        g_type_ensure (GSSDP_TYPE_PKTINFO6_MESSAGE);

        if (family == G_SOCKET_FAMILY_IPV6) {
                return gssdp_socket_option_set (socket,
                                                IPPROTO_IPV6,
                                                IPV6_RECVPKTINFO,
                                                (char *)&enable,
                                                sizeof (enable),
                                                error);
        } else if (family == G_SOCKET_FAMILY_IPV4) {
                return gssdp_socket_option_set (socket,
                                                IPPROTO_IP,
                                                IP_PKTINFO,
                                                (char *) &enable,
                                                sizeof (enable),
                                                error);
        } else {
                g_warning ("Invalid socket family: %d", family);

                return FALSE;
        }
#else
    __GSSDP_UNUSED (socket);
    __GSSDP_UNUSED (enable);
    __GSSDP_UNUSED (error);

    return TRUE;
#endif
}
