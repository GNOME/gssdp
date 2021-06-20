/*
 * Copyright (C) 2014 Jens Georg <mail@jensge.org>
 *
 * Author: Jens Georg <mail@jensge.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
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
