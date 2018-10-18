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

#include "board.h"
#include "net/gcoap.h"
#include "periph/gpio.h"
#include "net/cord/config.h"
#include "net/cord/ep.h"
#include "net/sock/util.h"
#include "xtimer.h"

#include "shell.h"

#define DINO_MOVE_PATH      "/dino/move"
#ifndef DINO_MOVE_PIN
#define DINO_MOVE_PIN       GPIO_PIN(PB, 23)
#endif
#ifndef CORERD_SERVER_ADDR
#define CORERD_SERVER_ADDR  "[2001:db8::3:25a:4550:a00:455a]"
#endif

static const char corerd_server_addr[] = CORERD_SERVER_ADDR;

static void _toggle_dino(void *arg)
{
    (void)arg;
    LED0_TOGGLE;
    gpio_toggle(DINO_MOVE_PIN);
}

static ssize_t _dino_move(coap_pkt_t* pdu, uint8_t *buf, size_t len,
                          void *ctx)
{

    (void)ctx;
    switch (coap_method2flag(coap_get_code_detail(pdu))) {
        case COAP_POST:
            _toggle_dino(NULL);
            return gcoap_response(pdu, buf, len, COAP_CODE_VALID);
        default:
            return 0;
    }
}

static const coap_resource_t _resources[] = {
    { DINO_MOVE_PATH, COAP_POST, _dino_move, NULL },
};

static gcoap_listener_t _listener = {
    (coap_resource_t *)&_resources[0],
    sizeof(_resources) / sizeof(_resources[0]),
    NULL
};

static int make_sock_ep(sock_udp_ep_t *ep, const char *addr)
{
    ep->port = 0;
    if (sock_udp_str2ep(ep, addr) < 0) {
        return -1;
    }
    ep->family  = AF_INET6;
    ep->netif   = SOCK_ADDR_ANY_NETIF;
    ep->port = CORD_SERVER_PORT;
    return 0;
}

int main(void)
{
    sock_udp_ep_t corerd_server;

    LED0_OFF;
    gpio_init_int(BTN0_PIN, BTN0_MODE, GPIO_FALLING, _toggle_dino, NULL);
    gpio_init(DINO_MOVE_PIN, GPIO_OUT);
    gcoap_register_listener(&_listener);

    xtimer_sleep(2);    /* warte ein wenig, bis sich das Netzwerk konfiguriert
                         * hat */

    if (make_sock_ep(&corerd_server, corerd_server_addr) < 0) {
        puts("Can not parse CORERD_SERVER_ADDR");
    }
    cord_ep_register(&corerd_server, NULL);
    while (1) { /* Endlos-Schleife */ }
    return 0;
}

/** @} */
