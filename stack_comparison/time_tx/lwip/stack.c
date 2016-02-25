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
 * @author Martine Lenders <mlenders@inf.fu-berlin.de>
 */

#include <inttypes.h>

#include "lwip.h"
#include "lwip/nd6.h"
#include "lwip/tcpip.h"
#include "lwip/netif/netdev2.h"
#include "lwip/netif.h"
#include "netif/lowpan6.h"
#include "xtimer.h"

#include "netdev.h"

#include "stack.h"

static struct netif netifs[NETDEV_NUMOF];

void stack_init(void)
{
    /* netdev needs to be set-up */
    assert(netdevs[0].netdev.netdev.driver);
    for (unsigned i = 0; i < NETDEV_NUMOF; i++) {
        netif_add(&netifs[i], &netdevs[i], lwip_netdev2_init,
                  tcpip_6lowpan_input);
    }
    netif_set_default(&netifs[0]);
    lwip_bootstrap();
    xtimer_sleep(3);    /* Let the auto-configuration run warm */
}

void stack_add_neighbor(int iface, const ipv6_addr_t *ipv6_addr,
                        const uint8_t *l2_addr, uint8_t l2_addr_len)
{
    for (int i = 0; i < LWIP_ND6_NUM_NEIGHBORS; i++) {
        struct nd6_neighbor_cache_entry *nc = &neighbor_cache[i];
        if (nc->state == ND6_NO_ENTRY) {
            nc->state = ND6_REACHABLE;
            memcpy(&nc->next_hop_address, ipv6_addr, sizeof(ip6_addr_t));
            memcpy(&nc->lladdr, l2_addr, l2_addr_len);
            nc->netif = &netifs[iface];
            nc->counter.reachable_time = UINT32_MAX;
            return;
        }
    }
}

/** @} */
