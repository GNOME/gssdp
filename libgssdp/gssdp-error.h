/* 
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#ifndef GSSDP_ERROR_H
#define GSSDP_ERROR_H

#include <glib.h>

G_BEGIN_DECLS

GQuark
gssdp_error_quark (void) G_GNUC_CONST;

#define GSSDP_ERROR (gssdp_error_quark ())

/**
 * GSSDPError:
 * @GSSDP_ERROR_NO_IP_ADDRESS: GSSDP could not find a valid IP address of a
 * #GSSDPClient.
 * @GSSDP_ERROR_FAILED: Unknown error.
 *
 * Error used in client creation.
 *
 */
typedef enum {
        GSSDP_ERROR_NO_IP_ADDRESS,
        GSSDP_ERROR_FAILED
} GSSDPError;

G_END_DECLS

#endif /* GSSDP_ERROR_H */
