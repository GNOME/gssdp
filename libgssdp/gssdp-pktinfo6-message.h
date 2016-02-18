/*
 * Copyright (C) 2014 Jens Georg <mail@jensge.org>
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

#ifndef GSSDP_PKTINFO6_MESSAGE_H
#define GSSDP_PKTINFO6_MESSAGE_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define GSSDP_TYPE_PKTINFO6_MESSAGE (gssdp_pktinfo6_message_get_type())
G_DECLARE_FINAL_TYPE (GSSDPPktinfo6Message,
                      gssdp_pktinfo6_message,
                      GSSDP,
                      PKTINFO6_MESSAGE,
                      GSocketControlMessage)

G_GNUC_INTERNAL GSocketControlMessage *
gssdp_pktinfo6_message_new (GInetAddress *addr, gint ifindex);

G_GNUC_INTERNAL gint
gssdp_pktinfo6_message_get_ifindex (GSSDPPktinfo6Message *message);

G_GNUC_INTERNAL GInetAddress *
gssdp_pktinfo6_message_get_local_addr (GSSDPPktinfo6Message *message);

#endif /* GSSDP_PKTINFO6_MESSAGE_H */
