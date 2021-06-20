/* 
 * Copyright (C) 2006, 2007 OpenedHand Ltd.
 *
 * Author: Jorn Baayen <jorn@openedhand.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */

#include "gssdp-error.h"

/**
 * gssdp_error_quark:
 *
 * Returns: a #GQuark uniquely used by GSSDP's errors.
 **/
GQuark
gssdp_error_quark (void)
{
        static GQuark quark = 0;

        if (!quark)
                quark = g_quark_from_static_string ("gssdp-error");

        return quark;
}
