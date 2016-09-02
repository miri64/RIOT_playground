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
#include "net/gnrc/netapi.h"
#include "net/gnrc/netif.h"
#include "net/gnrc/rpl.h"
#include "net/netopt.h"

#include "stack.h"


void stack_init(void)
{
    bool state = 1;
    ipv6_addr_t dodag_id;

    kernel_pid_t ifs[GNRC_NETIF_NUMOF];
    gnrc_netif_get(ifs);

    gnrc_netapi_set(ifs[0], NETOPT_RX_END_IRQ, 0, &state,
                    sizeof(state));
    gnrc_rpl_init(ifs[0]);

    gnrc_ipv6_netif_add_addr(ifs[0], &GUA, 64, 0);

    memcpy(&dodag_id, &GUA, sizeof(dodag_id));
    gnrc_rpl_root_init(1, &dodag_id, false, false);
}

/** @} */
