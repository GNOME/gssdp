/*
 * Copyright (C) 2014 Jens Georg <mail@jensge.org>
 *
 * Author: Jens Georg <mail@jensge.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
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
