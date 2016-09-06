/*
 * Copyright (C) 2016 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @author  Martine Lenders <mlenders@inf.fu-berlin.de>
 */

#include "kernel_types.h"
#include "net/gnrc/netif.h"
#include "net/gnrc/rpl.h"

#include "stack.h"

#ifdef RPL_STACK
ipv6_addr_t dst = {{ 0x20, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }};
#else
ipv6_addr_t dst = {{ 0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                     0x34, 0x32, 0x48, 0x33, 0x46, 0xdf, 0x8e, 0x76 }};
#endif

void stack_init(void)
{
    kernel_pid_t ifs[GNRC_NETIF_NUMOF];
    gnrc_netif_get(ifs);
#ifdef RPL_STACK
    gnrc_rpl_init(ifs[0]);
#endif
}

/** @} */
