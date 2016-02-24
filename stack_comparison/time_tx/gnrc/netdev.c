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

#include <errno.h>
#include <string.h>

#include "net/netopt.h"

#include "netdev.h"

netdev2_test_t netdevs[NETDEV_NUMOF];

static int _get_src_len(netdev2_t *dev, void *value, size_t max_len)
{
    uint16_t *v = value;

    (void)dev;
    if (max_len != sizeof(uint16_t)) {
        return -EOVERFLOW;
    }

    *v = sizeof(uint64_t);

    return sizeof(uint16_t);
}

static int _get_max_pkt_size(netdev2_t *dev, void *value, size_t max_len)
{
    uint16_t *v = value;

    (void)dev;
    if (max_len != sizeof(uint16_t)) {
        return -EOVERFLOW;
    }

    *v = 104;   /* 127 - maximum MAC overhead of IEEE 802.15.4 (with PAN compression) */

    return sizeof(uint16_t);
}

void netdev_init(void)
{
    for (uint8_t i = 0; i < NETDEV_NUMOF; i++) {
        const uint8_t addr[] = { 0x02, 0x01, 0x02, 0x03,
                                 0x04, 0x05, 0x06, i };
        netdev2_test_setup(&netdevs[i], NULL);
        netdev2_test_set_get_cb(&netdevs[i], NETOPT_SRC_LEN, _get_src_len);
        netdev2_test_set_get_cb(&netdevs[i], NETOPT_MAX_PACKET_SIZE,
                                _get_max_pkt_size);
#ifdef MODULE_GNRC
#ifdef MODULE_GNRC_SIXLOWPAN
        netdevs[i].netdev.proto = GNRC_NETTYPE_SIXLOWPAN;
#else
        netdevs[i].netdev.proto = GNRC_NETTYPE_UNDEF;
#endif
#endif
        netdevs[i].netdev.pan = 0x0708;
        memcpy(netdevs[i].netdev.short_addr, &addr[6], sizeof(uint16_t));
        memcpy(netdevs[i].netdev.long_addr, &addr[0], sizeof(uint64_t));
        netdevs[i].netdev.seq = 0;
        netdevs[i].netdev.chan = 26;
        netdevs[i].netdev.flags = (NETDEV2_IEEE802154_ACK_REQ |
                                   NETDEV2_IEEE802154_SRC_MODE_LONG |
                                   NETDEV2_IEEE802154_PAN_COMP);
    }
}

/** @} */
