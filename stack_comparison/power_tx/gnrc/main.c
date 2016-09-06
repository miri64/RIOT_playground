/*
 * Copyright (C) 2016 HAW Hamburg
 * Copyright (C) 2016 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       Example application measuring energy consumption
 *
 * @author      Peter Kietzmann <peter.kietzmann@haw-hamburg.de>
 * @author      Martine Lenders <m.lenders@fu-berlin.de>
 *
 * @}
 */

#include <stdio.h>
#include <stdbool.h>

#include "led.h"
#include "msg.h"
#include "net/af.h"
#include "net/conn/udp.h"
#include "stack.h"
#include "xtimer.h"

#define PACKET_DELAY    1000000

#define MAIN_QUEUE_SIZE     (8)

static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];
static char data[21];

static void send(void)
{
    LED_OFF(0);
    LED_OFF(1);
    LED_OFF(2);

    // send one packet more (for receiver) but turn on LED after num packets
    for (unsigned int i = 0; i < NUM_PACKETS+1; i++) {
        sprintf(data, "%02d Msg buffer w/20b", i);

#ifdef MODULE_LWIP_CONN
        ipv6_addr_t unspec = IPV6_ADDR_UNSPECIFIED;

        conn_udp_sendto(data, strlen(data), &unspec, sizeof(unspec), &dst, sizeof(ipv6_addr_t),
                        AF_INET6, UDP_PORT, UDP_PORT);
#else
        conn_udp_sendto(data, strlen(data), NULL, 0, &dst, sizeof(ipv6_addr_t),
                        AF_INET6, UDP_PORT, UDP_PORT);
#endif

        xtimer_usleep(PACKET_DELAY);

        // turn on led after the before last packet
        // wait for the delay. otherwise it's not fair 
        // for the receiver
        if (i == NUM_PACKETS-1) {
            LED_ON(0);
            LED_ON(1);
            LED_ON(2);
        }
    }

    printf("Sender done");

}

int main(void)
{
    LED_ON(0);
    LED_ON(1);
    LED_ON(2);
    /* we need a message queue for the thread running the shell in order to
     * receive potentially fast incoming networking packets */
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);

    stack_init();

    xtimer_sleep(20);

    send();

    /* should be never reached */
    return 0;
}
