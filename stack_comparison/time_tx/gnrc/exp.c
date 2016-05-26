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
#include "thread.h"
#include "xtimer.h"

#include "netdev.h"
#include "stack.h"

#include "exp.h"

#define TIMER_WINDOW_SIZE   (16)

static ipv6_addr_t dst = EXP_ADDR;
static const uint8_t dst_l2[] = EXP_ADDR_L2;
static const uint8_t honeyguide[] = { 0x2d, 0x4e };

#define HONEYGUIDE_LEN  (sizeof(honeyguide))
#define TAIL_LEN        (HONEYGUIDE_LEN + sizeof(uint16_t))

static uint8_t payload_buffer[EXP_MAX_PAYLOAD];
static uint32_t timer_window[TIMER_WINDOW_SIZE];
static uint16_t payload_size;

static int _netdev2_send(netdev2_t *dev, const struct iovec *vector, int count)
{
    /* first things first */
    const uint32_t stop = xtimer_now();
    const uint8_t *payload = vector[count - 1].iov_base;
    const size_t payload_len = vector[count - 1].iov_len;
    const uint8_t *_hg = &payload[payload_len - HONEYGUIDE_LEN];
    int res = 0;
    uint32_t start;
    uint16_t exp_payload_len;
    uint8_t id;

    (void)dev;
    for (int i = 0; i < count; i++) {
        res += vector[i].iov_len;
    }

    /* filter out unwanted packets */
    if (payload_len < TAIL_LEN) {
        return res;
    }
    if (memcmp(_hg, honeyguide, sizeof(honeyguide)) != 0) {
        return res;
    }
    memcpy(&exp_payload_len, &payload[payload_len - TAIL_LEN], sizeof(uint16_t));
    id = payload[payload_len - TAIL_LEN - 1];

    for (int i = 0; i < count; i++) {
        res += vector[i].iov_len;
    }

    start = timer_window[id % TIMER_WINDOW_SIZE];
#ifndef EXP_STACKTEST
    printf("%u,%" PRIu32 "\n", (unsigned)exp_payload_len, (stop - start));
#else
    (void)stop;
    (void)start;
#endif
    return res;
}

void exp_run(void)
{
    ipv6_addr_t unspec = IPV6_ADDR_UNSPECIFIED;

    netdev2_test_set_send_cb(&netdevs[0], _netdev2_send);
    stack_add_neighbor(0, &dst, dst_l2, sizeof(dst_l2));
#ifdef STACK_MULTIHOP
    const ipv6_addr_t *gua;
    ipv6_addr_t prefix = EXP_PREFIX;
    gua = stack_add_prefix(0, &prefix, EXP_PREFIX_LEN);
    stack_add_route(0, &unspec, 0, &dst);
#ifdef STACK_RPL
    stack_init_rpl(0, gua);
#else
    (void)gua;
#endif
    dst.u64[0].u64 = 0;
    ipv6_addr_init_prefix(&dst, &prefix, EXP_PREFIX_LEN);
#endif

#ifdef EXP_STACKTEST
    puts("thread,stack_size,stack_free");
#else
    puts("payload_len,tx_traversal_time");
#endif
    for (payload_size = EXP_PAYLOAD_STEP; payload_size <= EXP_MAX_PAYLOAD;
         payload_size += EXP_PAYLOAD_STEP) {
        for (unsigned id = 0; id < EXP_RUNS; id++) {
            for (unsigned j = 0; j < (payload_size - TAIL_LEN); j++) {
                payload_buffer[j] = id & 0xff;
            }
            memcpy((uint16_t *)&payload_buffer[payload_size - TAIL_LEN],
                   &payload_size, sizeof(uint16_t));
            memcpy(&payload_buffer[payload_size - HONEYGUIDE_LEN], honeyguide,
                   sizeof(honeyguide));
            timer_window[id % TIMER_WINDOW_SIZE] = xtimer_now();
            conn_udp_sendto(payload_buffer, payload_size, &unspec, sizeof(unspec),
                            &dst, sizeof(dst), AF_INET6, EXP_SRC_PORT, EXP_DST_PORT);
#if EXP_PACKET_DELAY
            xtimer_usleep(EXP_PACKET_DELAY);
#endif
        }
    }
#ifdef EXP_STACKTEST
    for (kernel_pid_t i = 0; i <= KERNEL_PID_LAST; i++) {
        const thread_t *p = (thread_t *)sched_threads[i];
        if ((p != NULL) &&
            (strcmp(p->name, "idle") != 0) &&
            (strcmp(p->name, "main") != 0) &&
            (strcmp(p->name, "exp_receiver") != 0)) {
            printf("%s,%u,%u\n", p->name, p->stack_size,
                   thread_measure_stack_free(p->stack_start));
        }
    }
#endif
}

/** @} */
