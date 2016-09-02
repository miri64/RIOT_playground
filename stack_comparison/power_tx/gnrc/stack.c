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

void stack_init(void)
{
    kernel_pid_t ifs[GNRC_NETIF_NUMOF];
    gnrc_netif_get(ifs);
    gnrc_rpl_init(ifs[0]);
}

/** @} */
