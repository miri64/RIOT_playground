/*
 * Copyright (C) 2016 HAW Hamburg
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
    LED0_OFF;
    LED1_OFF;
    LED2_OFF;

    // send one packet more (for receiver) but turn on LED after num packets
    for (unsigned int i = 0; i < NUM_PACKETS+1; i++) {

        sprintf(data, "%02d Msg buffer w/20b", i);

        conn_udp_sendto(data, strlen(data), NULL, 0, &GUA, sizeof(ipv6_addr_t),
                        AF_INET6, PORT, PORT);

        xtimer_usleep(PACKET_DELAY);

        // turn on led after the before last packet
        // wait for the delay. otherwise it's not fair 
        // for the receiver
        if (i == NUM_PACKETS-1) {
            LED0_ON;
            LED1_ON;
            LED2_ON;
        }
    }

    printf("Sender done");

}

int main(void)
{
    LED0_ON;
    LED1_ON;
    LED2_ON;
    /* we need a message queue for the thread running the shell in order to
     * receive potentially fast incoming networking packets */
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);

    stack_init();

    send();

    /* should be never reached */
    return 0;
}
