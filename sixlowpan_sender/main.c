/*
 * Copyright (C) 2008, 2009, 2010  Kaspar Schleiser <kaspar@schleiser.de>
 * Copyright (C) 2013 INRIA
 * Copyright (C) 2013 Ludwig Ortmann <ludwig.ortmann@fu-berlin.de>
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
 * @brief       Default application that shows a lot of functionality of RIOT
 *
 * @author      Martine Lenders <mlenders@inf.fu-berlin.de>
 *
 * @}
 */

#include <stdio.h>
#include <stdlib.h>

#include "basic_mac.h"
#include "netdev/default.h"
#include "board_uart0.h"
#include "ipv6.h"
#include "sixlowpan.h"
#include "cpu-conf.h"
#include "hwtimer.h"
#include "posix_io.h"
#include "random.h"
#include "sixlowpan.h"
#include "shell.h"
#include "shell_commands.h"

static char mac_stack[KERNEL_CONF_STACKSIZE_DEFAULT];
static char sixlowpan_stack[SIXLOWPAN_CONTROL_STACKSIZE];
static kernel_pid_t sixlowpan;
static uint8_t data_value;
static uint16_t src;
static size_t src_len = 2;

static void send(int argc, char **argv) {
    if (argc < 2) {
        puts("Send n bytes to fe80::1\n");
        puts("Usage: send <n>");
        return;
    }

    ipv6_hdr_t ipv6_hdr;
    netdev_hlist_t ulh = { NULL, NULL, &ipv6_hdr, sizeof(ipv6_hdr_t) };
    long long n = atoll(argv[1]);
    uint8_t bytes[n];
    uint16_t dest = 1;

    if (n < 0 || n >= (1 << 16)) {
        puts("n must be a 16-bit unsigned integer");
        return;
    }

    for (int i = 0; i < n; i++) {
        bytes[i] = data_value;
    }

    data_value++;

    ulh.next = &ulh;
    ulh.prev = &ulh;
    ipv6_hdr_set_version(&ipv6_hdr);
    ipv6_hdr_set_trafficclass(&ipv6_hdr, 0);
    ipv6_hdr_set_flowlabel(&ipv6_hdr, 0);
    ipv6_hdr.length = byteorder_htons((uint16_t)n);
    ipv6_hdr.nextheader = IPV6_PROTO_NUM_NONE;
    ipv6_hdr.hoplimit = 64;
    ipv6_hdr.srcaddr.u64[0] = byteorder_htonll(0xfe80000000000000);
    ipv6_hdr.srcaddr.u32[2] = byteorder_htonl(0x000000ff);
    ipv6_hdr.srcaddr.u16[6] = byteorder_htons(0xfe00);
    ipv6_hdr.srcaddr.u16[7] = byteorder_htons(src);
    ipv6_hdr.destaddr.u64[0] = byteorder_htonll(0xfe80000000000000);
    ipv6_hdr.destaddr.u32[2] = byteorder_htonl(0x000000ff);
    ipv6_hdr.destaddr.u16[6] = byteorder_htons(0xfe00);
    ipv6_hdr.destaddr.u16[7] = byteorder_htons(dest);

    netapi_send_data2(sixlowpan, &ulh, &dest, sizeof(uint16_t), bytes, n);
}

static void comp(int argc, char **argv) {
    sixlowpan_hc_status_t status;

    if (argc < 2) {
        puts("Activate/Deactivate header compression\n");
        puts("Usage: comp 0; for deactivation, comp 1; for activation");
        return;
    }

    if (argv[1][0] == '0') {
        status = SIXLOWPAN_HC_DISABLE;
        puts("Disable 6LoWPAN header compression");
    }
    else {
        status = SIXLOWPAN_HC_ENABLE;
        puts("Enable 6LoWPAN header compression");
    }

    sixlowpan_hc_set_status(sixlowpan, status);
}

static const shell_command_t shell_command[] = {
    {"send", "Send n bytes to dest", send},
    {"comp", "Configure 6LoWPAN header compression", comp},
    {NULL, NULL, NULL},
};

int main(void)
{
    shell_t shell;
    kernel_pid_t mac = basic_mac_init(mac_stack, BASIC_MAC_CONTROL_STACKSIZE,
                                      PRIORITY_MAIN - 1, CREATE_STACKTEST,
                                      "basic_mac_default", NETDEV_DEFAULT);
    sixlowpan = sixlowpan_init(mac, sixlowpan_stack, SIXLOWPAN_CONTROL_STACKSIZE,
                               PRIORITY_MAIN - 1, CREATE_STACKTEST,
                               "basic_mac_default");

    genrand_init(hwtimer_now());
    data_value = (uint8_t)(genrand_uint32() % 256);

    netapi_set_option(sixlowpan, NETAPI_CONF_SRC_LEN, &src_len, sizeof(size_t));
    netapi_get_option(sixlowpan, NETAPI_CONF_ADDRESS, &src, sizeof(uint16_t));

    (void) posix_open(uart0_handler_pid, 0);

    (void) puts("Welcome to RIOT!");

    shell_init(&shell, shell_command, UART0_BUFSIZE, uart0_readc, uart0_putc);

    shell_run(&shell);
    return 0;
}
