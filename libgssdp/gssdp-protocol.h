/* 
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#ifndef GSSDP_PROTOCOL_H
#define GSSDP_PROTOCOL_H

G_BEGIN_DECLS

#define SSDP_ADDR "239.255.255.250"
#define SSDP_V6_LL "FF02::C"
#define SSDP_V6_SL "FF05::C"
#define SSDP_V6_GL "FF0E::C"

#define SSDP_PORT 1900
#define SSDP_PORT_STR "1900"

#define SSDP_DISCOVERY_REQUEST                      \
        "M-SEARCH * HTTP/1.1\r\n"                   \
        "Host: %s:" SSDP_PORT_STR "\r\n" \
        "Man: \"ssdp:discover\"\r\n"                \
        "ST: %s\r\n"                                \
        "MX: %d\r\n"                                \
        "User-Agent: %s\r\n"  \

#define SSDP_DISCOVERY_RESPONSE                     \
        "HTTP/1.1 200 OK\r\n"                       \
        "Location: %s\r\n"                          \
        "%s"                                        \
        "Ext:\r\n"                                  \
        "USN: %s\r\n"                               \
        "Server: %s\r\n"                            \
        "Cache-Control: max-age=%d\r\n"             \
        "ST: %s\r\n"                                \
        "Date: %s\r\n"                              \
        "Content-Length: 0\r\n"

#define SSDP_ALIVE_MESSAGE                          \
        "NOTIFY * HTTP/1.1\r\n"                     \
        "Host: %s:" SSDP_PORT_STR "\r\n" \
        "Cache-Control: max-age=%d\r\n"             \
        "Location: %s\r\n"                          \
        "%s"                                        \
        "Server: %s\r\n"                            \
        "NTS: ssdp:alive\r\n"                       \
        "NT: %s\r\n"                                \
        "USN: %s\r\n"

#define SSDP_BYEBYE_MESSAGE                         \
        "NOTIFY * HTTP/1.1\r\n"                     \
        "Host: %s:" SSDP_PORT_STR "\r\n" \
        "NTS: ssdp:byebye\r\n"                     \
        "NT: %s\r\n"                                \
        "USN: %s\r\n"

#define SSDP_UPDATE_MESSAGE                         \
        "NOTIFY * HTTP/1.1\r\n"                     \
        "Host: %s:" SSDP_PORT_STR "\r\n" \
        "Location: %s\r\n"                          \
        "NT: %s\r\n"                                \
        "NTS: ssdp:update\r\n"                      \
        "USN: %s\r\n"                               \
        "NEXTBOOTID.UPNP.ORG: %u\r\n"

#define SSDP_SEARCH_METHOD "M-SEARCH"
#define GENA_NOTIFY_METHOD "NOTIFY"

#define SSDP_ALIVE_NTS  "ssdp:alive"
#define SSDP_BYEBYE_NTS "ssdp:byebye"
#define SSDP_UPDATE_NTS "ssdp:update"

#define SSDP_DEFAULT_MAX_AGE 1800
#define SSDP_DEFAULT_MX      3

G_END_DECLS

#endif /* GSSDP_PROTOCOL_H */
