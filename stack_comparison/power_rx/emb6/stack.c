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

#include "at86rf2xx.h"
#include "at86rf2xx_params.h"
#include "thread.h"

#include "emb6.h"
#include "emb6/netdev2.h"
#include "rpl.h"
#include "uip-ds6.h"
#include "thread.h"

#include "stack.h"

#define EMB6_STACKSIZE  (THREAD_STACKSIZE_DEFAULT)
#define EMB6_PRIO       (THREAD_PRIORITY_MAIN - 3)
#define EMB6_DELAY      (58)

#ifdef RPL_STACK
ipv6_addr_t dst = {{ 0x20, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 }};
#else
ipv6_addr_t dst = {{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }};
#endif

static s_ns_t emb6 = {
    .hc = &sicslowpan_driver,
    .llsec = &nullsec_driver,
    .hmac = &nullmac_driver,
    .lmac = &sicslowmac_driver,
    .frame = &framer_802154,
    .c_configured = 1,
};
static char emb6_stack[EMB6_STACKSIZE];
static at86rf2xx_t dev;

static void *_emb6_thread(void *args)
{
    (void)args;
    emb6_process(EMB6_DELAY);   /* never stops */
    return NULL;
}

void stack_init(void)
{
    netdev2_t *netdev = (netdev2_t *)&dev;

    at86rf2xx_setup(&dev, &at86rf2xx_params[0]);
    netdev->driver->init(netdev);
    emb6_netdev2_setup(netdev);
    emb6_init(&emb6);
#ifdef RPL_STACK
    rpl_init();
    uip_ds6_addr_add((uip_ipaddr_t *)&dst, 0, ADDR_MANUAL);
    rpl_set_root(0, (uip_ipaddr_t *)&dst);
#endif
    thread_create(emb6_stack, sizeof(emb6_stack), EMB6_PRIO,
                  THREAD_CREATE_STACKTEST, _emb6_thread, NULL, "emb6");
}

/** @} */
