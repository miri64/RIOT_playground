/*
 * Copyright (C) 2018 Freie Universität Berlin
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
 * @brief       Test for lwIP
 *
 * @author      Martine Lenders <m.lenders@fu-berlin.de>
 *
 * This test application tests the lwIP package.
 * @}
 */

#include "lwip/api.h"
#include "xtimer.h"

#ifndef TCP_SERVER_ADDR
#define TCP_SERVER_ADDR     "fe80::1"
#endif

#ifndef TCP_SERVER_PORT
#define TCP_SERVER_PORT     (1337)
#endif

int main(void)
{
    ip6_addr_t addr;
    struct netconn *conn;
    struct netbuf *buf;

    xtimer_sleep(5U);   /* wait 5 sec to bootstrap network */

    ip6addr_aton(TCP_SERVER_ADDR, &addr);
    conn = netconn_new(NETCONN_TYPE_IPV6 | NETCONN_TCP);
    netconn_connect(conn, &addr, TCP_SERVER_PORT);
    puts("Kill TCP server now");
    netconn_recv(conn, &buf);

    return 0;
}
