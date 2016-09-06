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

#include "lwip.h"
#include "lwip/netif/netdev2.h"
#include "lwip/netif.h"
#include "net/netdev2.h"
#include "net/netopt.h"
#include "netif/lowpan6.h"

#include "stack.h"

ipv6_addr_t dst = {{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }};

void stack_init(void)
{
    bool state = 1;
    netdev2_t *dev = (netdev2_t *)netif_default->state;

    dev->driver->set(dev, NETOPT_RX_END_IRQ, &state, sizeof(state));
}

/** @} */
