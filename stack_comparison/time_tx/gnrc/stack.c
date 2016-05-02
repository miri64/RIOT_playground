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

#ifdef STACK_MULTIHOP
#include "net/fib.h"
#endif
#include "net/gnrc/ipv6.h"
#include "net/gnrc/netdev2/ieee802154.h"
#ifdef STACK_RPL
#include "net/gnrc/rpl.h"
#endif
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

#ifdef STACK_MULTIHOP
const ipv6_addr_t *stack_add_prefix(int iface, const ipv6_addr_t *prefix,
                                    uint8_t prefix_len)
{
    netdev2_t *netdev = (netdev2_t *)&netdevs[iface];
    ipv6_addr_t addr = IPV6_ADDR_UNSPECIFIED;

    netdev->driver->get(netdev, NETOPT_IPV6_IID, &addr.u64[1],
                        sizeof(addr.u64[1]));
    ipv6_addr_init_prefix(&addr, prefix, prefix_len);
    return gnrc_ipv6_netif_add_addr(_pids[iface], prefix, prefix_len, 0);
}

void stack_add_route(int iface, const ipv6_addr_t *prefix, uint8_t prefix_len,
                     const ipv6_addr_t *next_hop)
{
    uint32_t prefix_flags = 0;
    (void)prefix_len;
    if (ipv6_addr_is_unspecified(prefix)) {
        prefix_flags |= (((uint32_t)prefix_len) << FIB_FLAG_NET_PREFIX_SHIFT);
    }
    fib_add_entry(&gnrc_ipv6_fib_table, _pids[iface],
                  (uint8_t *)prefix, sizeof(ipv6_addr_t), prefix_flags,
                  (uint8_t *)next_hop, sizeof(ipv6_addr_t), 0,
                  (uint32_t)FIB_LIFETIME_NO_EXPIRE);
}
#endif

#ifdef STACK_RPL
void stack_init_rpl(int iface, const ipv6_addr_t *dodag_id)
{
    gnrc_rpl_init(_pids[iface]);
#ifdef STACK_RPL_ROOT
    gnrc_rpl_root_init(0, (ipv6_addr_t *)dodag_id, true, false);
#else
    (void)dodag_id;
#endif
}
#endif

/** @} */
