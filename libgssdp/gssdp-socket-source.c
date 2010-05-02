/* 
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 * Copyright (C) 2009 Nokia Corporation, all rights reserved.
 * Copyright (C) 2010 Jens Georg <mail@jensge.org>
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *         Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *                               <zeeshan.ali@nokia.com>
 *         Jens Georg <mail@jensge.org>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <glib.h>

#include "gssdp-socket-functions.h"
#include "gssdp-socket-source.h"
#include "gssdp-protocol.h"
#include "gssdp-error.h"

/**
 * gssdp_socket_source_new
 *
 * Return value: A new #GSSDPSocketSource
 **/
GSSDPSocketSource *
gssdp_socket_source_new (GSSDPSocketSourceType type,
                         const char           *host_ip,
                         GError              **error)
{
        GSSDPSocketSource *socket_source = NULL;
        GInetAddress *iface_address = NULL;
        GSocketAddress *bind_address = NULL;
        GInetAddress *group = NULL;
        GError *inner_error = NULL;
        GSocketFamily family;

        iface_address = g_inet_address_new_from_string (host_ip);
        if (iface_address == NULL) {
                if (error != NULL) {
                        *error = g_error_new (GSSDP_ERROR,
                                              GSSDP_ERROR_FAILED,
                                              "Invalid host ip: %s",
                                              host_ip);
                        inner_error = *error;
                }

                goto error;
        }

        family = g_inet_address_get_family (iface_address);

        if (family == G_SOCKET_FAMILY_IPV4)
                group = g_inet_address_new_from_string (SSDP_ADDR);
        else {
                if (error != NULL) {
                        *error = g_error_new_literal (GSSDP_ERROR,
                                                      GSSDP_ERROR_FAILED,
                                                      "IPv6 address");
                        inner_error = *error;
                }

                goto error;
        }


        /* Create source */
        socket_source = g_slice_new0 (GSSDPSocketSource);

        /* Create socket */
        socket_source->socket = g_socket_new (G_SOCKET_FAMILY_IPV4,
                                              G_SOCKET_TYPE_DATAGRAM,
                                              G_SOCKET_PROTOCOL_UDP,
                                              &inner_error);

        if (!socket_source->socket) {
                g_propagate_prefixed_error (error,
                                            inner_error,
                                            "Could not create socket");

                goto error;
        }

        /* Enable broadcasting */
        if (!gssdp_socket_enable_broadcast (socket_source->socket,
                                            TRUE,
                                            &inner_error)) {
                g_propagate_prefixed_error (error,
                                            inner_error,
                                            "Failed to enable broadcast");
                goto error;
        }

        /* TTL */
        if (!gssdp_socket_set_ttl (socket_source->socket,
                                   4,
                                   &inner_error)) {
                g_propagate_prefixed_error (error,
                                            inner_error,
                                            "Failed to set TTL");

        }
        /* Set up additional things according to the type of socket desired */
        if (type == GSSDP_SOCKET_SOURCE_TYPE_MULTICAST) {
                /* Enable multicast loopback */
                if (!gssdp_socket_enable_loop (socket_source->socket,
                                               TRUE,
                                               &inner_error)) {
                        g_propagate_prefixed_error (
                                        error,
                                        inner_error,
                                        "Failed to enable loop-back");

                        goto error;
                }

                if (!gssdp_socket_mcast_interface_set (socket_source->socket,
                                                       iface_address,
                                                       &inner_error)) {
                        g_propagate_prefixed_error (
                                        error,
                                        inner_error,
                                        "Failed to set multicast interface");

                        goto error;
                }

#ifdef G_OS_WIN32
                bind_address = g_inet_socket_address_new (iface_address,
                                                          SSDP_PORT);
#else
                bind_address = g_inet_socket_address_new (group,
                                                          SSDP_PORT);
#endif
        } else {
                bind_address = g_inet_socket_address_new (iface_address,
                                                          SSDP_PORT);
        }

        /* Bind to requested port and address */
        if (!g_socket_bind (socket_source->socket,
                            bind_address,
                            TRUE,
                            &inner_error)) {
                g_propagate_prefixed_error (error,
                                            inner_error,
                                            "Failed to bind socket");

                goto error;
        }

        if (type == GSSDP_SOCKET_SOURCE_TYPE_MULTICAST) {

                 /* Subscribe to multicast channel */
                if (!gssdp_socket_mcast_group_join (socket_source->socket,
                                                    group,
                                                    iface_address,
                                                    &inner_error)) {
                        char *address = g_inet_address_to_string (group);
                        g_propagate_prefixed_error (error,
                                                    inner_error,
                                                    "Failed to join group %s",
                                                    address);
                        g_free (address);

                        goto error;
                }
        }

        socket_source->source = g_socket_create_source (socket_source->socket,
                                                        G_IO_IN | G_IO_ERR,
                                                        NULL);
error:
        if (iface_address != NULL)
                g_object_unref (iface_address);
        if (bind_address != NULL)
                g_object_unref (bind_address);
        if (group != NULL)
                g_object_unref (group);
        if (inner_error != NULL) {
                if (error == NULL) {
                        g_warning ("Failed to create socket source: %s",
                                   inner_error->message);
                        g_error_free (inner_error);
                }

                return NULL;
        }

        return socket_source;
}

GSocket *
gssdp_socket_source_get_socket (GSSDPSocketSource *socket_source)
{
        g_return_val_if_fail (socket_source != NULL, NULL);

        return socket_source->socket;
}

void
gssdp_socket_source_destroy (GSSDPSocketSource *socket_source)
{
        g_return_if_fail (socket_source != NULL);
        g_source_destroy (socket_source->source);
        g_socket_close (socket_source->socket, NULL);
        g_object_unref (socket_source->socket);
        g_slice_free (GSSDPSocketSource, socket_source);
}
