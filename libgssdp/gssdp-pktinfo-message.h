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

#ifndef GSSDP_PKTINFO_MESSAGE_H
#define GSSDP_PKTINFO_MESSAGE_H

#include <gio/gio.h>

G_BEGIN_DECLS

#define GSSDP_TYPE_PKTINFO_MESSAGE (gssdp_pktinfo_message_get_type())

G_DECLARE_FINAL_TYPE (GSSDPPktinfoMessage,
                      gssdp_pktinfo_message,
                      GSSDP,
                      PKTINFO_MESSAGE,
                      GSocketControlMessage)

G_GNUC_INTERNAL GSocketControlMessage *
gssdp_pktinfo_message_new (GInetAddress *addr, GInetAddress *dst, gint ifindex);

G_GNUC_INTERNAL gint
gssdp_pktinfo_message_get_ifindex (GSSDPPktinfoMessage *message);

G_GNUC_INTERNAL GInetAddress *
gssdp_pktinfo_message_get_local_addr (GSSDPPktinfoMessage *message);

G_GNUC_INTERNAL GInetAddress *
gssdp_pktinfo_message_get_pkt_addr (GSSDPPktinfoMessage *message);

#endif /* GSSDP_PKTINFO_MESSAGE_H */
