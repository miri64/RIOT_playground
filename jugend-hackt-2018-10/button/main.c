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
#include <stdbool.h>        /* bool-Typ-Unterstützung */
#include <stdio.h>          /* Ein- und Ausgabesupport (für puts() und printf()) */

/* Ziehe Header-Definitionen der Module und externen Pakete in die main.c */
#include "jsmn.h"           /* JSON-Parsing-Bibliothek */
#include "msg.h"            /* IPC-Nachrichten-Unterstützung des RIOT-Kernels */
#include "net/cord/ep.h"    /* Endpunkt-Unterstützung für das Ressourcen-Verzeichnis */
#include "net/gcoap.h"      /* CoAP-Bibliothek */
#include "net/sock/util.h"  /* Hilfsfunktionen für sock-Endpunkte */
#include "periph/gpio.h"    /* RIOTs GPIO-Unterstützung */
#include "thread.h"         /* Thread-Unterstützung des RIOT-Kernels */
#include "xtimer.h"         /* Timer-Unterstützung (zum Debouncing des Buttons) */


/* Typ-Definition der IPC-Nachrichten, die das Senden von CoAP-Nachrichten
 * auslösen */
#define BUTTON_SEND_MSG_TYPE        (0x4d41)

/* Der Pin auf den wir den Button gesetzt haben */
#ifndef BUTTON_PIN
#define BUTTON_PIN                  GPIO_PIN(PB, 2)
#endif

/* Der Zeitraum in dem der Button kurzzeitig deaktiviert wird */
#define BUTTON_DEBOUNCE_INTERVAL    (500U * US_PER_MS)

/* Der Port unter dem das Target für den Button verfügbar ist */
#ifndef BUTTON_TARGET_PORT
#define BUTTON_TARGET_PORT          (GCOAP_PORT)
#endif

/* Der CoAP-Pfad um das Target des Button's zu setzen */
#ifndef BUTTON_SET_TARGET_PATH
#define BUTTON_SET_TARGET_PATH      "/button/target"
#endif

/* Die maximale Länge des Pfads für ein gesetztes Target */
#ifndef BUTTON_TARGET_PATH_LEN
#define BUTTON_TARGET_PATH_LEN      (16U)
#endif

/* Die Adresse des Ressourcen-Verzeichnisses */
#ifndef CORERD_SERVER_ADDR
#define CORERD_SERVER_ADDR          "[2001:db8::3:25a:4550:a00:455a]"
#endif

/* Timer zur kurzzeitigen Deaktivierung (Debouncing) des Buttons */
static xtimer_t _debounce_timer;
/* IPC-Nachricht, die das Senden von CoAP-Nachrichten auslöst */
static msg_t _button_send                   = { .type = BUTTON_SEND_MSG_TYPE };
/* Die Process ID des Haupt-Threads */
static kernel_pid_t _main_pid   = KERNEL_PID_UNDEF;

/* JSON-Parser */
static jsmn_parser _json_parser;

/* Die zu setzende Target-Adresse für den Button */
static sock_udp_ep_t _button_target_remote  = { .family = AF_INET6,
                                                .port = BUTTON_TARGET_PORT,
                                                .netif = SOCK_ADDR_ANY_NETIF };
/* Der zu setzende CoAP-Pfad für den Button */
static char _button_target_path[BUTTON_TARGET_PATH_LEN];

/* Zeitpunkt wann die letzte CoAP-Nachricht gesendet wurde */
static uint32_t _last_sent;

/* Die Adresse des Ressourcen */
static const char corerd_server_addr[] = CORERD_SERVER_ADDR;

/* ==== Hardware-Teil ==== */

/* Timer-IRQ-Callback zur Aktivierung des Buttons */
static void _enable_button(void *arg)
{
    gpio_t button = (gpio_t)arg;
    gpio_irq_enable(button);
}

/* GPIO-IRQ-Callback der aufgerufen wird, wenn der Button gedrückt wird */
static void _button_press(void *arg)
{
    /* Deaktiviere Button kurzzeitig (Debouncing) */
    gpio_irq_disable((gpio_t)arg);
    /* Setze Debouncing-Timer */
    _debounce_timer.callback = _enable_button;
    _debounce_timer.arg = arg;
    xtimer_set(&_debounce_timer, BUTTON_DEBOUNCE_INTERVAL);
    /* Teile Haupt-Thread mit, dass er eine CoAP-Nachricht senden soll,
     * wenn möglich */
    msg_send_int(&_button_send, _main_pid);
}

/* ==== CoAP-Server-Teil ==== */

/* Prüft ob ein Token `tok` im JSON-String `json` ein Schlüssel ist, dessen
 * Wert `s` ist */
static inline bool jsoneq(const char *json, jsmntok_t *tok, const char *s)
{
    /* Das Objekt ist ein String */
    return (tok->type == JSMN_STRING)
        /* Der String ist so lang wie erwartet */
        && ((int) strlen(s) == tok->end - tok->start)
        /* Der String hat den erwarteten Wert */
        && (strncmp(json + tok->start, s, tok->end - tok->start) == 0);
}

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
        ep->port = GCOAP_PORT;
    }
    return 0;
}

/* Setze das Target für den Button von einer JSON-Payload */
static unsigned _set_target_from_payload(char *payload, size_t payload_len)
{
    int num;
    jsmntok_t toks[5];
    /* Wir erwarten nur 5 tokens:
     *  1. top-level object
     *  2. key von addr
     *  3. value von addr
     *  4. key von path
     *  5. value von path */

    /* initialisiere JSON-Parser */
    jsmn_init(&_json_parser);
    /* Parse die Payload */
    num = jsmn_parse(&_json_parser, payload, payload_len, toks,
                     sizeof(toks) / sizeof(toks[0]));
    if (num < 0) {
        printf("Failed to parse JSON: %s (res: %d)\n", payload, payload_len);
        return COAP_CODE_BAD_REQUEST;
    }
    /* Der erste Token sollte das beschreibende Objekt sein */
    if ((num < 1) || (toks[0].type != JSMN_OBJECT)) {
        puts("Object expected at top-level");
        return COAP_CODE_BAD_REQUEST;
    }
    /* Gehe über alle verbleibenden Tokens */
    for (int i = 1; i < num; i++) {
        /* wenn der Token der "addr"-Schlüssel ist */
        if (jsoneq(payload, &toks[i], "addr")) {
            i++;    /* Der Wert ist im nächsten Token */
            if (toks[i].type != JSMN_STRING) {
                /* Wert von "addr" ist kein String */
                puts("\"addr\" is no a string");
                return COAP_CODE_BAD_REQUEST;
            }
            /* Setze Verbindungspartner zum Wert von "addr" */
            make_sock_ep(&_button_target_remote, payload + toks[i].start);
        }
        /* sonst, wenn der Token der "path"-Schlüssel ist */
        else if (jsoneq(payload, &toks[i], "path")) {
            i++;    /* Der Wert ist im nächsten Token */
            size_t tok_len = toks[i].end - toks[i].start;

            if (toks[i].type != JSMN_STRING) {
                /* Wert von "path" ist kein String */
                puts("\"path\" is no a string");
                return COAP_CODE_BAD_REQUEST;
            }
            /* wenn der Wert von "path" ein leerer String ist */
            if (tok_len == 0) {
                /* mache _button_target_path auch zum leeren String */
                _button_target_path[0] = '\0';
            }
            /* sonst */
            else {
                /* Kopiere den Pfad aus der Payload */
                strncpy(_button_target_path, payload + toks[i].start,
                        tok_len);
            }
        }
        /* sonst haben wir einen unerwarteten Schlüssel; das ist nicht erlaubt */
        else {
            printf("Unexpected key: %.*s\n", toks[i].end - toks[i].start,
                   payload + toks[i].start);
            return COAP_CODE_BAD_REQUEST;
        }
    }
    puts("Button target set");
    return COAP_CODE_VALID;
}

/* Handler für den "/button/target"-Pfad */
static ssize_t _set_target(coap_pkt_t* pdu, uint8_t *buf, size_t len,
                           void *ctx)
{
    (void)ctx;
    /* Wenn die Anfrage eine POST-Anfrage war */
    if (coap_method2flag(coap_get_code_detail(pdu)) == COAP_POST) {
        unsigned code;

        /* Prüfe ob Nachricht das korrekte Format (JSON) hat */
        if (pdu->content_type != COAP_FORMAT_JSON) {
            puts("Not a JSON object");
            /* wenn nicht, melde dass die Anfrage falsch war */
            code = COAP_CODE_BAD_REQUEST;
        }
        else {
            /* sonst setze Button-Target von der Nachrichten-Payload */
            code = _set_target_from_payload((char *)pdu->payload,
                                            pdu->payload_len);
        }
        /* Erstelle und versende die entsprechende Antwort */
        return gcoap_response(pdu, buf, len, code);
    }
    /* sonst */
    else {
        /* lass die Bibliothek das die Antwort erstellen und senden */
        return 0;
    }
}

/* Definition der verfügbaren Ressourcen */
static const coap_resource_t _resources[] = {
    { BUTTON_SET_TARGET_PATH, COAP_POST, _set_target, NULL },
};

/* Definition des CoAP-Servers */
static gcoap_listener_t _listener = {
    (coap_resource_t *)&_resources[0],
    sizeof(_resources) / sizeof(_resources[0]),
    NULL
};

/* ==== CoAP-Client-Teil ==== */

/* Sende eine CoAP-POST-Nachrich an das Target des Buttons */
static void _post_to_target(void)
{
    uint8_t buf[GCOAP_PDU_BUF_SIZE];
    coap_pkt_t pdu;
    int res;
    size_t len;

    /* Prüfe, ob wir vor weniger als einer Sekunde gesendet haben. Wir wollen
     * ja nicht übertreiben ;-) */
    if ((xtimer_now_usec() - US_PER_SEC) < _last_sent) {
        puts("Last send less than a second ago");
        return;
    }
    /* Prüfe, ob das Target für den Button gesetzt ist */
    if (ipv6_addr_is_unspecified((ipv6_addr_t *)&_button_target_remote.addr) ||
        (_button_target_path[0] == '\0')) {
        puts("No target defined");
        return;
    }
    /* Bereite die CoAP-POST-Nachricht vor */
    gcoap_req_init(&pdu, buf, GCOAP_PDU_BUF_SIZE, COAP_POST,
                   _button_target_path);
    /* Schließe die CoAP-Nachricht mit leerer Payload ab */
    len = gcoap_finish(&pdu, 0, COAP_FORMAT_NONE);
    /* Ermittle die aktuelle Zeit */
    _last_sent = xtimer_now_usec();
    /* Sende die CoAP-Nachricht */
    res = gcoap_req_send2(buf, len, &_button_target_remote, NULL);
    if (res == 0) {
        puts("Unable to send");
    }
}

static void _coap_client(void)
{
    while (1) {
        msg_t msg;

        msg_receive(&msg);
        if (msg.type == BUTTON_SEND_MSG_TYPE) {
            _post_to_target();
        }
    }
}

/* ==== Haupt-Funktion ==== */

int main(void)
{
    sock_udp_ep_t corerd_server;

    /* Setze aktuelle Process-ID als die Process-ID des Haupt-Threads */
    _main_pid = sched_active_pid;
    /* Verknüpfe den Button-Pin mit dem entsprechenden Handler */
    gpio_init_int(BUTTON_PIN, GPIO_IN_PU, GPIO_FALLING, _button_press,
                  (void *)BUTTON_PIN);
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
    /* Starte den CoAP-Client */
    _coap_client();
    return 0;
}

/** @} */
