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

#include "net/af.h"
#include "net/conn/udp.h"
#include "led.h"
#include "xtimer.h"

#include "stack.h"

#ifndef NUM_PACKETS
#define NUM_PACKETS 100
#endif

#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];
static conn_udp_t conn;
static char data[21];

int main(void)
{
    ipv6_addr_t any = IPV6_ADDR_UNSPECIFIED;
    ipv6_addr_t addr;
    size_t addr_len;
    uint16_t port;

    LED0_ON;
    LED1_ON;
    LED2_ON;

    /* we need a message queue for the thread running the shell in order to
     * receive potentially fast incoming networking packets */
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);

    stack_init();

    conn_udp_create(&conn, &any, sizeof(ipv6_addr_t), AF_INET6, PORT);

    while (1) {
        int packet_nr;
        int size = conn_udp_recvfrom(&conn, data, sizeof(data), &addr,
                                     &addr_len, &port);
        if (size < 0) {
            continue;
        }
        packet_nr = atoi((char *)data);
        if (packet_nr == 0) {
            LED0_OFF;
            LED1_OFF;
            LED2_OFF;
        }
        // counter starts at '0' but sender sends NUM_PACKETS+1 packets,
        // so the last number will be NUM_PACKETS
        if (packet_nr == (NUM_PACKETS)) { 
            LED0_ON;
            LED1_ON;
            LED2_ON;
            printf("last pkt_no received %i\n", packet_nr);
        }
    }

    return 0;
}
