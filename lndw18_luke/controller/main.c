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
#include "net/gnrc/ipv6/nib/ft.h"
#include "thread.h"
#include "xtimer.h"

#include "shell.h"

#define LUKE_MSG_TYPE_SEND_POINTS   (0x4d41)
#define LUKE_SEND_TIMEOUT           (1U * US_PER_SEC)

#ifndef LUKE_BUTTON
#define LUKE_BUTTON                 GPIO_PIN(PA, 6)
#endif
#define LUKE_DEBOUNCE_INTERVAL      (10U * MS_PER_SEC)

#ifndef LUKE_DISPLAY_PREFIX
#define LUKE_DISPLAY_PREFIX         { 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, \
                                      0, 0, 0, 0, 0, 0, 0, 1 }
#endif
#ifndef LUKE_DISPLAY_PREFIX_LEN
#define LUKE_DISPLAY_PREFIX_LEN     (64U)
#endif

#ifndef LUKE_DISPLAY_PORT
#define LUKE_DISPLAY_PORT           (GCOAP_PORT)
#endif

#define LUKE_PAYLOAD_FMT            "{\"points\":%u}"
#define LUKE_PAYLOAD_SIZE           (sizeof("{\"points\":255}"))
#define LUKE_POINT_PATH             "/luke/points"

#define LUKE_START_VALUE            (0U)

static atomic_uint _counter = ATOMIC_VAR_INIT(LUKE_START_VALUE);
static xtimer_t _send_timer, _debounce_timer;
static msg_t _send_timer_msg = { .type = LUKE_MSG_TYPE_SEND_POINTS };
static sock_udp_ep_t remote = { .family = AF_INET6,
                                .port = LUKE_DISPLAY_PORT,
                                .netif = SOCK_ADDR_ANY_NETIF };

static void _enable_button(void *arg)
{
    gpio_t button = (gpio_t)arg;
    gpio_irq_enable(button);
}

static void _increment_counter(void *arg)
{
    gpio_irq_disable((gpio_t)arg);
    _debounce_timer.callback = _enable_button;
    _debounce_timer.arg = arg;
    xtimer_set(&_debounce_timer, LUKE_DEBOUNCE_INTERVAL);
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
    if (ipv6_addr_is_unspecified((ipv6_addr_t *)&remote.addr)) {
        return;
    }
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
                           counter / 5);
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

static void _init_remote(void)
{
    static const ipv6_addr_t prefix = { .u8 = LUKE_DISPLAY_PREFIX };
    gnrc_ipv6_nib_ft_t fte;
    void *state = NULL;

    xtimer_sleep(5U);
    while (gnrc_ipv6_nib_ft_iter(NULL, 0, &state, &fte)) {
        if (ipv6_addr_is_unspecified(&fte.dst)) {
            break;
        }
    }
    if (state == NULL) {
        puts("Can't set remote");
    }
    else {
        char addr_str[IPV6_ADDR_MAX_STR_LEN];

        ipv6_addr_set_aiid((ipv6_addr_t *)&remote.addr, &fte.next_hop.u8[8]);
        ipv6_addr_init_prefix((ipv6_addr_t *)&remote.addr,
                              &prefix, LUKE_DISPLAY_PREFIX_LEN);
        printf("Set remote to [%s]\n",
               ipv6_addr_to_str(addr_str, (ipv6_addr_t *)&remote.addr,
                                sizeof(addr_str)));
    }
}

int main(void)
{
    gpio_init_int(LUKE_BUTTON, GPIO_IN, GPIO_RISING, _increment_counter,
                  (void *)LUKE_BUTTON);
    _init_remote();
    _client();
    return 0;
}

/** @} */
