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

#include <assert.h>

#include "net/fib.h"
#include "net/gnrc/ipv6.h"
#include "net/gnrc/netdev2/ieee802154.h"
#include "net/gnrc/sixlowpan.h"
#include "net/gnrc/udp.h"
#include "net/gnrc.h"
#include "thread.h"

#include "netdev.h"

#include "stack.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

#define _MAC_STACKSIZE      (THREAD_STACKSIZE_DEFAULT)
#define _MAC_PRIO           (THREAD_PRIORITY_MAIN - 4)

static char _mac_stacks[_MAC_STACKSIZE][NETDEV_NUMOF];
static gnrc_netdev2_t _gnrc_adapters[NETDEV_NUMOF];
static kernel_pid_t _pids[NETDEV_NUMOF];

void stack_init(void)
{
    /* netdev needs to be set-up */
    assert(netdevs[0].netdev.netdev.driver);
    gnrc_pktbuf_init();
    gnrc_sixlowpan_init();
    gnrc_ipv6_init();
    gnrc_udp_init();
    for (uint8_t i = 0; i < NETDEV_NUMOF; i++) {
        if (gnrc_netdev2_ieee802154_init(&_gnrc_adapters[i],
                                         (netdev2_ieee802154_t *)&netdevs[i])) {
            DEBUG("Could not initialize netdev2<->IEEE802154 glue code\n");
            return;
        }
        _pids[i] = gnrc_netdev2_init(_mac_stacks[i], _MAC_STACKSIZE, _MAC_PRIO,
                                     "netdev2", &_gnrc_adapters[i]);
    }
    gnrc_ipv6_netif_init_by_dev();
}

void stack_add_neighbor(int iface, const ipv6_addr_t *ipv6_addr,
                        const uint8_t *l2_addr, uint8_t l2_addr_len)
{
    gnrc_ipv6_nc_add(_pids[iface], ipv6_addr, l2_addr, l2_addr_len, 0);
}

/** @} */
