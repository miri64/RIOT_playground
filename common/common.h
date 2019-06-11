/*
 * Copyright (C) 2019 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup
 * @ingroup
 * @brief
 * @{
 *
 * @file
 * @brief
 *
 * @author  Martine Lenders <m.lenders@fu-berlin.de>
 */
#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>

#include "net/sock/util.h"
#include "net/gcoap.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CORERD_SERVER_ADDR      "[2001:db8:f4ba:cbcd:1ac0:ffee:1ac0:ffee]"

#define LUKE_PATH_POINTS        "/luke/points"
#define LUKE_PATH_TARGET        "/luke/target"

#define LUKE_POINTS_FMT         "{\"points\":%u}"
#define LUKE_POINTS_MIN_SIZE    (sizeof("{\"points\":0}") - 1)
#define LUKE_POINTS_MAX_SIZE    (sizeof("{\"points\":255}") - 1)

#define LUKE_TARGET_PATH_LEN    (16U)
#define LUKE_TARGET_PORT        (GCOAP_PORT)
#define LUKE_TARGET_FMT         "{\"addr\":\"[%s]:%u\",\"path\":\"%s\"}"

ssize_t luke_set_target(coap_pkt_t* pdu, uint8_t *buf, size_t len,
                        void *ctx);
size_t post_points_to_target(unsigned points);

static inline int make_sock_ep(sock_udp_ep_t *ep, const char *addr)
{
    ep->port = 0;
    if (sock_udp_str2ep(ep, addr) < 0) {
        return -1;
    }
    ep->family  = AF_INET6;
    ep->netif   = SOCK_ADDR_ANY_NETIF;
    if (ep->port == 0) {
        ep->port = GCOAP_PORT;
    }
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* COMMON_H */
/** @} */
