/*
 * Copyright (C) 2019 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @author  Martine Lenders <m.lenders@fu-berlin.de>
 */

#include "fmt.h"
#include "net/gnrc/pkt.h"

static gnrc_pktsnip_t pktsnip;

int main(void)
{
    print_str("sizeof(pktsnip): ");
    print_u32_dec(sizeof(pktsnip));
    print_str("\n");
    return 0;
}

/** @} */
