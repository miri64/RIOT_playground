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

#include "emb6.h"
#include "emb6/netdev2.h"
#include "uip-ds6.h"
#include "net/ipv6/addr.h"
#include "thread.h"

#include "netdev.h"

#include "stack.h"

#define EMB6_STACKSIZE  (THREAD_STACKSIZE_MAIN)
#define EMB6_PRIO       (THREAD_PRIORITY_MAIN - 3)
#define EMB6_DELAY      (130)

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

/** @} */
