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
#include <inttypes.h>

#include "emb6.h"
#include "emb6/netdev2.h"
#ifdef STACK_RPL
#include "rpl.h"
#endif
#include "uip-ds6.h"
#include "net/ipv6/addr.h"
#include "thread.h"

#include "netdev.h"

#include "stack.h"

#define EMB6_STACKSIZE  (THREAD_STACKSIZE_MAIN)
#define EMB6_PRIO       (THREAD_PRIORITY_MAIN - 3)
#define EMB6_DELAY      (58)

static s_ns_t emb6;
static char emb6_stack[EMB6_STACKSIZE];

static void *_emb6_thread(void *args)
{
    (void)args;
    emb6_process(EMB6_DELAY);   /* never stops */
    return NULL;
}

void stack_init(void)
{
    netdev2_t *dev = (netdev2_t *)&netdevs[0];

    /* netdev needs to be set-up */
    assert(dev->driver);
    dev->driver->init(dev);
    emb6.hc = &sicslowpan_driver;
    emb6.llsec = &nullsec_driver;
    emb6.hmac = &nullmac_driver;
    emb6.lmac = &sicslowmac_driver;
    emb6.frame = &framer_802154;
    emb6.c_configured = 1;
    emb6_netdev2_setup((netdev2_t *)&netdevs[0]);
    emb6_init(&emb6);
    thread_create(emb6_stack, sizeof(emb6_stack), EMB6_PRIO,
                  THREAD_CREATE_STACKTEST, _emb6_thread, NULL, "emb6");
}

void stack_add_neighbor(int iface, const ipv6_addr_t *ipv6_addr,
                        const uint8_t *l2_addr, uint8_t l2_addr_len)
{
    (void)iface;
    (void)l2_addr_len;
    uip_ds6_nbr_add((const uip_ipaddr_t *)ipv6_addr,
                    (const uip_lladdr_t *)l2_addr, 0, NBR_REACHABLE);
}

#ifdef STACK_MULTIHOP
const ipv6_addr_t *stack_add_prefix(int iface, const ipv6_addr_t *prefix,
                                    uint8_t prefix_len)
{
    ipv6_addr_t addr = IPV6_ADDR_UNSPECIFIED;
    netdev2_t *netdev = (netdev2_t *)&netdevs[iface];

    (void)iface;
    netdev->driver->get(netdev, NETOPT_IPV6_IID, &addr.u64[1],
                        sizeof(addr.u64[1]));
    ipv6_addr_init_prefix(&addr, prefix, prefix_len);
    return (ipv6_addr_t *)&(uip_ds6_addr_add((uip_ipaddr_t *)&addr, 0,
                                             ADDR_MANUAL)->ipaddr);
}

void stack_add_route(int iface, const ipv6_addr_t *prefix, uint8_t prefix_len,
                     const ipv6_addr_t *next_hop)
{
    uip_ds6_route_t *res;

    (void)iface;
    res = uip_ds6_route_add((uip_ipaddr_t *)prefix, prefix_len,
                            (uip_ipaddr_t *)next_hop);
    if (res != NULL) {
        res->state.lifetime = UINT32_MAX;
    }
}
#endif

#ifdef STACK_RPL
void stack_init_rpl(int iface, const ipv6_addr_t *dodag_id)
{
    (void)iface;
    rpl_init();
#ifdef STACK_RPL_ROOT
    rpl_set_root(0, (uip_ipaddr_t *)dodag_id);
#else
    (void)dodag_id;
#endif
}
#endif

/** @} */
