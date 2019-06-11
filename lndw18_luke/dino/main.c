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

#include <stdio.h>

#include "board.h"
#include "net/cord/config.h"
#include "net/cord/ep.h"
#include "net/gcoap.h"
#include "periph/gpio.h"
#include "net/sock/util.h"
#include "timex.h"
#include "xtimer.h"

#include "common.h"

#ifndef DINO_MOVE_PIN
#define DINO_MOVE_PIN       GPIO_PIN(PB, 23)
#endif

static const char corerd_server_addr[] = CORERD_SERVER_ADDR;

static void _clear_dino(void *arg)
{
    (void)arg;
    LED0_OFF;
    gpio_clear(DINO_MOVE_PIN);
}

static xtimer_t _clear_timer = { .callback = _clear_dino };

static void _set_dino(void *arg)
{
    (void)arg;
    LED0_ON;
    gpio_set(DINO_MOVE_PIN);
    xtimer_set(&_clear_timer, 5 * US_PER_SEC);
}

static void _toggle_dino(void *arg)
{
    (void)arg;
    LED0_TOGGLE;
    gpio_toggle(DINO_MOVE_PIN);
    xtimer_set(&_clear_timer, 5 * US_PER_SEC);
}

static ssize_t _dino_move(coap_pkt_t* pdu, uint8_t *buf, size_t len,
                          void *ctx)
{
    unsigned code;
    (void)ctx;
    if (coap_method2flag(coap_get_code_detail(pdu)) == COAP_POST) {
        unsigned p = 0;
        unsigned content_type = coap_get_content_type(pdu);

        if ((pdu->payload_len < LUKE_POINTS_MIN_SIZE) ||
            (pdu->payload_len > LUKE_POINTS_MAX_SIZE) ||
            (content_type != COAP_FORMAT_JSON) ||
            (sscanf((char *)pdu->payload, LUKE_POINTS_FMT, &p) != 1)) {
            printf("(%u < %u) || (%u > %u) || (%u != %u) || "
                   "(payload unparsable)\n", pdu->payload_len,
                   LUKE_POINTS_MIN_SIZE, pdu->payload_len, LUKE_POINTS_MAX_SIZE,
                   content_type, COAP_FORMAT_JSON);
            code = COAP_CODE_BAD_REQUEST;
        }
        else {
            code = COAP_CODE_VALID;
        }
        if (p > 0) {
            _set_dino(NULL);
        }
        return gcoap_response(pdu, buf, len, code);
    }
    else {
        return 0;
    }
}

static const coap_resource_t _resources[] = {
    { LUKE_PATH_POINTS, COAP_POST, _dino_move, NULL },
};

static gcoap_listener_t _listener = {
    (coap_resource_t *)&_resources[0],
    sizeof(_resources) / sizeof(_resources[0]),
    NULL
};

/* ==== Haupt-Funktion ==== */

int main(void)
{
    sock_udp_ep_t corerd_server;

    /* Schalte On-Board-LED aus */
    LED0_OFF;
    /* Verknüpfe den On-Board-Button-Pin mit dem entsprechenden Handler */
    gpio_init_int(BTN0_PIN, BTN0_MODE, GPIO_FALLING, _toggle_dino, NULL);
    /* Initialisiere den Dino-Motor-Pin als ausgehenden Pin */
    gpio_init(DINO_MOVE_PIN, GPIO_OUT);
    /* Registriere die CoAP-Server Definition */
    gcoap_register_listener(&_listener);

    /* warte ein wenig, bis sich das Netzwerk konfiguriert hat */
    xtimer_sleep(2);

    /* Erstelle einen sock-Endpunkt für die vorkonfigurierte Addresse
     * des Ressourcen-Verzeichnisses */
    if (make_sock_ep(&corerd_server, corerd_server_addr) < 0) {
        puts("Can not parse CORERD_SERVER_ADDR");
    }
    cord_ep_register(&corerd_server, NULL);
    gpio_clear(DINO_MOVE_PIN);
    while (1) { }
    return 0;
}

/** @} */
