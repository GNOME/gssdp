/* 
 * Copyright (C) 2006, 2007, 2008 OpenedHand Ltd.
 * Copyright (C) 2009 Nokia Corporation, all rights reserved.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *         Zeeshan Ali (Khattak) <zeeshanak@gnome.org>
 *                               <zeeshan.ali@nokia.com>
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
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include "gssdp-socket-source.h"
#include "gssdp-protocol.h"

struct _GSSDPSocketSource {
        GSource source;

        GPollFD poll_fd;
};

static gboolean
gssdp_socket_source_prepare  (GSource    *source,
                              int        *timeout);
static gboolean
gssdp_socket_source_check    (GSource    *source);
static gboolean
gssdp_socket_source_dispatch (GSource    *source,
                              GSourceFunc callback,
                              gpointer    user_data);
static void
gssdp_socket_source_finalize (GSource    *source);

static const GSourceFuncs gssdp_socket_source_funcs = {
        gssdp_socket_source_prepare,
        gssdp_socket_source_check,
        gssdp_socket_source_dispatch,
        gssdp_socket_source_finalize
};

/**
 * gssdp_socket_source_new
 *
 * Return value: A new #GSSDPSocketSource
 **/
GSSDPSocketSource *
gssdp_socket_source_new (GSSDPSocketSourceType type,
                         const char           *host_ip)
{
        GSource *source;
        GSSDPSocketSource *socket_source;
        struct sockaddr_in bind_addr;
        struct in_addr iface_addr;
        struct ip_mreq mreq;
        gboolean boolean = TRUE;
        guchar ttl = 4;
        int res;

        /* Create source */
        source = g_source_new ((GSourceFuncs*)&gssdp_socket_source_funcs,
                               sizeof (GSSDPSocketSource));

        socket_source = (GSSDPSocketSource *) source;

        /* Create socket */
        socket_source->poll_fd.fd = socket (AF_INET,
                                            SOCK_DGRAM,
                                            IPPROTO_UDP);
        if (socket_source->poll_fd.fd == -1)
                goto error;
        
        socket_source->poll_fd.events = G_IO_IN | G_IO_ERR;

        g_source_add_poll (source, &socket_source->poll_fd);

        /* Enable broadcasting */
        res = setsockopt (socket_source->poll_fd.fd, 
                          SOL_SOCKET,
                          SO_BROADCAST,
                          &boolean,
                          sizeof (boolean));
        if (res == -1)
                goto error;

        /* TTL */
        res = setsockopt (socket_source->poll_fd.fd,
                          IPPROTO_IP,
                          IP_MULTICAST_TTL,
                          &ttl,
                          sizeof (ttl));
        if (res == -1)
                goto error;

        memset (&bind_addr, 0, sizeof (bind_addr));
        bind_addr.sin_family = AF_INET;

        res = inet_aton (host_ip, &iface_addr);
        if (res == 0)
                goto error;

        /* Set up additional things according to the type of socket desired */
        if (type == GSSDP_SOCKET_SOURCE_TYPE_MULTICAST) {
                /* Allow multiple sockets to use the same PORT number */
                res = setsockopt (socket_source->poll_fd.fd,
                                  SOL_SOCKET,
#ifdef SO_REUSEPORT 
                                  SO_REUSEPORT,
#else
                                  SO_REUSEADDR,
#endif
                                  &boolean,
                                  sizeof (boolean));
                if (res == -1)
                        goto error;

                /* Enable multicast loopback */
                res = setsockopt (socket_source->poll_fd.fd,
                                  IPPROTO_IP,
                                  IP_MULTICAST_LOOP,
                                  &boolean,
                                  sizeof (boolean));
                if (res == -1)
                       goto error;

                /* Set the interface */
                res = setsockopt (socket_source->poll_fd.fd,
                                  IPPROTO_IP,
                                  IP_MULTICAST_IF,
                                  &iface_addr,
                                  sizeof (struct in_addr));
                if (res == -1)
                        goto error;

                /* Subscribe to multicast channel */
                res = inet_aton (SSDP_ADDR, &(mreq.imr_multiaddr));
                if (res == 0)
                        goto error;

                memcpy (&(mreq.imr_interface),
                        &iface_addr,
                        sizeof (struct in_addr));

                res = setsockopt (socket_source->poll_fd.fd,
                                  IPPROTO_IP,
                                  IP_ADD_MEMBERSHIP,
                                  &mreq,
                                  sizeof (mreq));
                if (res == -1)
                        goto error;

                bind_addr.sin_port = htons (SSDP_PORT);
                res = inet_aton (SSDP_ADDR, &(bind_addr.sin_addr));
                if (res == 0)
                        goto error;
        } else {
                bind_addr.sin_port = 0;
                memcpy (&(bind_addr.sin_addr),
                        &iface_addr,
                        sizeof (struct in_addr));
        }

        /* Bind to requested port and address */
        res = bind (socket_source->poll_fd.fd,
                    (struct sockaddr *) &bind_addr,
                    sizeof (bind_addr));
        if (res == -1)
                goto error;

        return socket_source;

error:
        g_source_destroy (source);
        
        return NULL;
}

static gboolean
gssdp_socket_source_prepare (GSource *source,
                             int     *timeout)
{
        return FALSE;
}

static gboolean
gssdp_socket_source_check (GSource *source)
{
        GSSDPSocketSource *socket_source;

        socket_source = (GSSDPSocketSource *) source;

        return socket_source->poll_fd.revents & (G_IO_IN | G_IO_ERR);
}

static gboolean
gssdp_socket_source_dispatch (GSource    *source,
                              GSourceFunc callback,
                              gpointer    user_data)
{
        GSSDPSocketSource *socket_source;

        socket_source = (GSSDPSocketSource *) source;

        if (socket_source->poll_fd.revents & G_IO_IN) {
                /* Ready to read */
                if (callback)
                        callback (user_data);
        } else if (socket_source->poll_fd.revents & G_IO_ERR) {
                /* Error */
                int value;
                socklen_t size_int;

                value = EINVAL;
                size_int = sizeof (int);
                
                /* Get errno from socket */
                getsockopt (socket_source->poll_fd.fd,
                            SOL_SOCKET,
                            SO_ERROR,
                            &value,
                            &size_int);

                g_warning ("Socket error %d received: %s",
                           value,
                           strerror (value));
        }

        return TRUE;
}

static void
gssdp_socket_source_finalize (GSource *source)
{
        GSSDPSocketSource *socket_source;

        socket_source = (GSSDPSocketSource *) source;
        
        /* Close the socket */
        close (socket_source->poll_fd.fd);
}

/**
 * gssdp_socket_source_get_fd
 *
 * Return value: The socket's FD.
 **/
int
gssdp_socket_source_get_fd (GSSDPSocketSource *socket_source)
{
        g_return_val_if_fail (socket_source != NULL, -1);
        
        return socket_source->poll_fd.fd;
}
