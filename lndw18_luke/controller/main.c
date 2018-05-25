/*
 * Copyright (C) 2018 Freie Universität Berlin
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

#include <stdatomic.h>
#include <stdio.h>

#include "led.h"
#include "msg.h"
#include "periph/gpio.h"
#include "net/gcoap.h"
#include "thread.h"
#include "xtimer.h"

#define LUKE_MSG_TYPE_SEND_POINTS   (0x4d41)
#define LUKE_SEND_TIMEOUT           (1U * US_PER_SEC)

#ifndef LUKE_BUTTON
#define LUKE_BUTTON     GPIO_PIN(PA, 7)
#endif

#ifndef LUKE_SERVER_ADDR
#define LUKE_SERVER_ADDR        { 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, \
                                  0, 0, 0, 0, 0, 0, 0, 1 }
#endif

#ifndef LUKE_SERVER_PORT
#define LUKE_SERVER_PORT        (GCOAP_PORT)
#endif

#define LUKE_PAYLOAD_FMT        "{\"points\":%u}"
#define LUKE_PAYLOAD_SIZE       (sizeof("{\"points\":255}"))
#define LUKE_POINT_PATH         "/luke/points"

#define LUKE_START_VALUE        (0U)

static atomic_uint _counter = ATOMIC_VAR_INIT(LUKE_START_VALUE);
static xtimer_t _send_timer;
static msg_t _send_timer_msg = { .type = LUKE_MSG_TYPE_SEND_POINTS };
static const sock_udp_ep_t remote = { .family = AF_INET6,
                                      .addr = { .ipv6 = LUKE_SERVER_ADDR },
                                      .port = LUKE_SERVER_PORT,
                                      .netif = SOCK_ADDR_ANY_NETIF };

static void _increment_counter(void *arg)
{
    (void)arg;
    atomic_fetch_add(&_counter, 1);
    LED0_TOGGLE;
}

static inline void _schedule_next_send(void)
{
    xtimer_set_msg(&_send_timer, LUKE_SEND_TIMEOUT, &_send_timer_msg,
                   sched_active_pid);
}

static void _post(char *payload, size_t payload_len)
{
    uint8_t buf[GCOAP_PDU_BUF_SIZE];
    coap_pkt_t pdu;
    int res;
    size_t len;

    gcoap_req_init(&pdu, buf, GCOAP_PDU_BUF_SIZE, COAP_POST, LUKE_POINT_PATH);
    memcpy(pdu.payload, payload, payload_len);
    len = gcoap_finish(&pdu, payload_len, COAP_FORMAT_JSON);
    res = gcoap_req_send2(buf, len, &remote, NULL);
    if (res == 0) {
        puts("Unable to send");
    }
}

static void _client(void)
{
    _schedule_next_send();
    while (1) {
        msg_t msg;

        msg_receive(&msg);
        if (msg.type == LUKE_MSG_TYPE_SEND_POINTS) {
            static char payload[LUKE_PAYLOAD_SIZE];
            unsigned counter;
            int res;

            counter = atomic_exchange(&_counter, LUKE_START_VALUE);
            _schedule_next_send();
            res = snprintf(payload, sizeof(payload), LUKE_PAYLOAD_FMT,
                           counter / 10);
            if (res > 0) {
                printf("Posting payload %s\n", payload);
                _post(payload, (size_t)res);
            }
            else {
                puts("Error setting payload");
            }
        }
    }
}

int main(void)
{
    gpio_init_int(LUKE_BUTTON, GPIO_IN, GPIO_RISING, _increment_counter, NULL);
    _client();
    return 0;
}

/** @} */
