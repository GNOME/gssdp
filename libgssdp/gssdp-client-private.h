/* 
 * Copyright (C) 2006 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#ifndef GSSDP_CLIENT_PRIVATE_H
#define GSSDP_CLIENT_PRIVATE_H

#include "gssdp-client.h"

G_BEGIN_DECLS

typedef enum {
        _GSSDP_DISCOVERY_REQUEST  = 0,
        _GSSDP_DISCOVERY_RESPONSE = 1,
        _GSSDP_ANNOUNCEMENT       = 2
} _GSSDPMessageType;

G_GNUC_INTERNAL void
_gssdp_client_send_message (GSSDPClient       *client,
                            const char        *dest_ip,
                            gushort            dest_port,
                            const char        *message,
                            _GSSDPMessageType  type);

G_GNUC_INTERNAL const char *
_gssdp_client_get_mcast_group (GSSDPClient    *client);

G_END_DECLS

#endif /* GSSDP_CLIENT_PRIVATE_H */
