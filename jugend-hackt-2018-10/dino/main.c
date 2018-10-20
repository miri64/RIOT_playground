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

/* Ziehe benötigte Header-Definitionen der C-Standardbibliothek in die main.c */
#include <stdio.h>              /* Ein- und Ausgabesupport (für puts() und printf()) */

#include "board.h"              /* Board-Konfiguration für den On-Board-Button
                                 * und die On-Board-LED (zum Debuggen) */
#include "net/cord/config.h"    /* Ressourcen-Verzeichnis-Konfiguration */
#include "net/cord/ep.h"        /* Endpunkt-Unterstützung für das Ressourcen-Verzeichnis */
#include "net/gcoap.h"          /* CoAP-Bibliothek */
#include "periph/gpio.h"        /* RIOTs GPIO-Unterstützung */
#include "net/sock/util.h"      /* Hilfsfunktionen für sock-Endpunkte */
#include "xtimer.h"             /* Timer-Unterstützung */

/* Der Pin mit dem der Motor des Dinos angesteuert wird */
#ifndef DINO_MOVE_PIN
#define DINO_MOVE_PIN       GPIO_PIN(PB, 23)
#endif

/* Der CoAP-Pfad um den Dino zu begegen */
#define DINO_MOVE_PATH      "/dino/move"

/* Die Adresse des Ressourcen-Verzeichnisses */
#ifndef CORERD_SERVER_ADDR
#define CORERD_SERVER_ADDR  "[2001:db8::3:25a:4550:a00:455a]"
#endif

/* Die Adresse des Ressourcen */
static const char corerd_server_addr[] = CORERD_SERVER_ADDR;

/* ==== Hardware-Teil ==== */

/* GPIO-IRQ-Callback der aufgeruffen wird, wenn der On-Board-Button (zum
 * Debuggen) gedrückt wird */
static void _toggle_dino(void *arg)
{
    (void)arg;
    LED0_TOGGLE;                /* Toggle die LED (zum sichtbaren Debuggen) */
    gpio_toggle(DINO_MOVE_PIN); /* Toggle den Pin zur Ansteuerung des Motors des Dinos */
}

/* ==== CoAP-Server-Teil ==== */

/* Handler für den "/dino/move"-Pfad */
static ssize_t _dino_move(coap_pkt_t* pdu, uint8_t *buf, size_t len,
                          void *ctx)
{
    (void)ctx;
    /* Wenn die Anfrage eine POST-Anfrage war */
    if (coap_method2flag(coap_get_code_detail(pdu)) == COAP_POST) {
        /* Wiederverwende den GPIO-IRQ-Callback um den Dino zu bewegen */
        _toggle_dino(NULL);
        /* Sende eine Antwort, dass die Anfrage erfolgreich bearbeitet wurde */
        return gcoap_response(pdu, buf, len, COAP_CODE_VALID);
    }
    /* sonst */
    else {
        /* lass die Bibliothek das die Antwort erstellen und senden */
        return 0;
    }
}

/* Definition der verfügbaren Ressourcen */
static const coap_resource_t _resources[] = {
    { DINO_MOVE_PATH, COAP_POST, _dino_move, NULL },
};

/* Definition des CoAP-Servers */
static gcoap_listener_t _listener = {
    (coap_resource_t *)&_resources[0],
    sizeof(_resources) / sizeof(_resources[0]),
    NULL
};

/* Erstellt einen sock-Endpunkt `ep` von einem Adress-String `addr` */
static int make_sock_ep(sock_udp_ep_t *ep, const char *addr)
{
    ep->port = 0;
    if (sock_udp_str2ep(ep, addr) < 0) {
        return -1;
    }
    ep->family  = AF_INET6;
    ep->netif   = SOCK_ADDR_ANY_NETIF;
    if (ep->port == 0) {
        ep->port = CORD_SERVER_PORT;
    }
    return 0;
}

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
    /* Registriere den CoAP-Server im Ressourcen-Verzeichnis */
    cord_ep_register(&corerd_server, NULL);
    /* wir müchten nicht terminieren */
    while (1) { /* Endlos-Schleife */ }
    return 0;
}

/** @} */
