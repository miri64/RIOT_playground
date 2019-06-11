/*
 * Copyright (C) 2018-19 Freie Universität Berlin
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
#include "net/cord/ep.h"
#include "net/gcoap.h"
#include "net/gnrc/ipv6/nib/ft.h"
#include "thread.h"
#include "xtimer.h"

#include "common.h"

#define LUKE_BUTTON_PREFIX          "/btn"

#define LUKE_MSG_TYPE_SEND_POINTS   (0x4d41)
#define LUKE_SEND_TIMEOUT           (100U * US_PER_MS)

#ifndef LUKE_BUTTON
#define LUKE_BUTTON                 GPIO_PIN(PB, 2)
#endif
#define LUKE_DEBOUNCE_INTERVAL      (10U * MS_PER_SEC)

#ifndef LUKE_DISPLAY_PORT
#define LUKE_DISPLAY_PORT           (GCOAP_PORT)
#endif

#define LUKE_START_VALUE            (0U)

static const char corerd_server_addr[] = CORERD_SERVER_ADDR;
static atomic_uint _counter = ATOMIC_VAR_INIT(LUKE_START_VALUE);
static xtimer_t _send_timer, _debounce_timer;
static msg_t _send_timer_msg = { .type = LUKE_MSG_TYPE_SEND_POINTS };
static const coap_resource_t _resources[] = {
    { LUKE_BUTTON_PREFIX LUKE_PATH_TARGET, COAP_POST | COAP_GET, luke_set_target, NULL },
};
static gcoap_listener_t _listener = {
    (coap_resource_t *)&_resources[0],
    sizeof(_resources) / sizeof(_resources[0]),
    NULL
};

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

static void _client(void)
{
    _schedule_next_send();
    while (1) {
        msg_t msg;

        msg_receive(&msg);
        if (msg.type == LUKE_MSG_TYPE_SEND_POINTS) {
            unsigned counter;

            counter = atomic_exchange(&_counter, LUKE_START_VALUE);
            _schedule_next_send();
            printf("Posting %u points\n", counter);
            if (post_points_to_target(counter) == 0) {
                puts("Unable to send CoAP message");
            }
        }
    }
}

int main(void)
{
    sock_udp_ep_t corerd_server;

    gpio_init_int(LUKE_BUTTON, GPIO_IN_PU, GPIO_FALLING, _increment_counter,
                  (void *)LUKE_BUTTON);
    gcoap_register_listener(&_listener);
    puts("Sleeping for 10 seconds");
    xtimer_sleep(10);
    if (make_sock_ep(&corerd_server, corerd_server_addr) < 0) {
        puts("Can not parse CORERD_SERVER_ADDR");
    }
    puts("Register to CoRE RD server");
    cord_ep_register(&corerd_server, NULL);
    /* signal ready to display */
    puts("Init complete");
    _client();
    return 0;
}

/** @} */
