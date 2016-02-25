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

#include <inttypes.h>

#include "net/af.h"
#include "net/conn/udp.h"
#include "net/ipv6/addr.h"
#include "net/netdev2_test.h"
#include "xtimer.h"

#include "netdev.h"
#include "stack.h"

#include "exp.h"

#define TIMER_WINDOW_SIZE   (16)

static const ipv6_addr_t dst = EXP_DST;
static const uint8_t honeyguide[] = { 0x2d, 0x4e, 0x70 };

static uint8_t payload_buffer[EXP_MAX_PAYLOAD];
static uint32_t timer_window[TIMER_WINDOW_SIZE];
static uint16_t payload_size;

static int _netdev2_send(netdev2_t *dev, const struct iovec *vector, int count)
{
    /* first things first */
    const uint32_t stop = xtimer_now();
    const uint8_t *payload = vector[count - 1].iov_base;
    const size_t payload_len = vector[count - 1].iov_len;
    const uint8_t *_hg = &payload[payload_len - 3];
    int res = 0;
    uint32_t start;
    uint16_t exp_payload_len;
    uint8_t id;

    (void)dev;
    for (int i = 0; i < count; i++) {
        res += vector[i].iov_len;
    }

    /* filter out unwanted packets */
    if (payload_len < 5) {
        return res;
    }
    if (memcmp(_hg, honeyguide, sizeof(honeyguide)) != 0) {
        return res;
    }
    memcpy(&exp_payload_len, &payload[payload_len - 5], sizeof(uint16_t));
    id = payload[payload_len - 6];

    for (int i = 0; i < count; i++) {
        res += vector[i].iov_len;
    }

    start = timer_window[id % TIMER_WINDOW_SIZE];
    printf("%u,%" PRIu32 "\n", (unsigned)exp_payload_len, (stop - start));
    return res;
}

void exp_run(void)
{
    static const ipv6_addr_t unspec = IPV6_ADDR_UNSPECIFIED;
    netdev2_test_set_send_cb(&netdevs[0], _netdev2_send);
    memset(timer_window, 0, sizeof(timer_window));

    puts("payload_len,tx_traversal_time");
    /* start with at least 1 id byte */
    for (payload_size = 6; payload_size <= EXP_MAX_PAYLOAD; payload_size += EXP_PAYLOAD_STEP) {
        for (unsigned id = 0; id < EXP_RUNS; id++) {
            for (int j = 0; j < (payload_size - 5); j++) {
                payload_buffer[j] = id & 0xff;
            }
            memcpy((uint16_t *)&payload_buffer[payload_size - 5],
                   &payload_size, sizeof(uint16_t));
            memcpy(&payload_buffer[payload_size - 3], honeyguide, sizeof(honeyguide));
            timer_window[id % TIMER_WINDOW_SIZE] = xtimer_now();
            conn_udp_sendto(payload_buffer, payload_size, &unspec, sizeof(unspec),
                            &dst, sizeof(dst), AF_INET6, EXP_SRC_PORT, EXP_DST_PORT);
        }
    }
}

/** @} */
