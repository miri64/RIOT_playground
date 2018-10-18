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

#include <stdbool.h>
#include <stdio.h>

#include "led.h"
#include "jsmn.h"
#include "msg.h"
#include "periph/gpio.h"
#include "net/cord/ep.h"
#include "net/gcoap.h"
#include "net/sock/util.h"
#include "thread.h"
#include "xtimer.h"

#include "shell.h"

#define BUTTON_SEND_MSG_TYPE        (0x4d41)

#ifndef BUTTON_PIN
#define BUTTON_PIN                  GPIO_PIN(PB, 2)
#endif
#define BUTTON_DEBOUNCE_INTERVAL    (500U * US_PER_MS)

#ifndef BUTTON_TARGET_PORT
#define BUTTON_TARGET_PORT          (GCOAP_PORT)
#endif

#ifndef BUTTON_TARGET_PATH_LEN
#define BUTTON_TARGET_PATH_LEN      (16U)
#endif

#ifndef BUTTON_SET_TARGET_PATH
#define BUTTON_SET_TARGET_PATH      "/button/target"
#endif

#define BUTTON_SET_TARGET_FMT       "{\"addr\":\"%s\",\"path\":\"%s\"}"
#define BUTTON_SET_TARGET_MIN_SIZE  (sizeof("{\"addr\":\"fe80::1\",\"path\":\"/\"}"))
#define BUTTON_SET_TARGET_MAX_SIZE  (sizeof("{\"addr\":\"\",\"path\":\"\"}") + \
                                     IPV6_ADDR_MAX_STR_LEN + \
                                     BUTTON_TARGET_PATH_LEN)

#ifndef CORERD_SERVER_ADDR
#define CORERD_SERVER_ADDR  "[2001:db8::3:25a:4550:a00:455a]"
#endif

static xtimer_t _debounce_timer;
static msg_t _button_send                   = { .type = BUTTON_SEND_MSG_TYPE };
static sock_udp_ep_t _button_target_remote  = { .family = AF_INET6,
                                                .port = BUTTON_TARGET_PORT,
                                                .netif = SOCK_ADDR_ANY_NETIF };
static jsmn_parser _json_parser;
static char _button_target_path[BUTTON_TARGET_PATH_LEN];
static const char corerd_server_addr[] = CORERD_SERVER_ADDR;
static uint32_t _last_sent;
static kernel_pid_t _main_pid   = KERNEL_PID_UNDEF;

static void _enable_button(void *arg)
{
    gpio_t button = (gpio_t)arg;
    gpio_irq_enable(button);
}

static void _button_press(void *arg)
{
    gpio_irq_disable((gpio_t)arg);
    _debounce_timer.callback = _enable_button;
    _debounce_timer.arg = arg;
    xtimer_set(&_debounce_timer, BUTTON_DEBOUNCE_INTERVAL);
    msg_send_int(&_button_send, _main_pid);
}

static void _post_to_target(void)
{
    uint8_t buf[GCOAP_PDU_BUF_SIZE];
    coap_pkt_t pdu;
    int res;
    size_t len;

    if ((xtimer_now_usec() - US_PER_SEC) < _last_sent) {
        puts("Last send less than a second ago");
        return;
    }
    if (ipv6_addr_is_unspecified((ipv6_addr_t *)&_button_target_remote.addr) ||
        (_button_target_path[0] == '\0')) {
        puts("No target defined");
        return;
    }
    LED0_ON;
    gcoap_req_init(&pdu, buf, GCOAP_PDU_BUF_SIZE, COAP_POST,
                   _button_target_path);
    len = gcoap_finish(&pdu, 0, COAP_FORMAT_JSON);
    _last_sent = xtimer_now_usec();
    res = gcoap_req_send2(buf, len, &_button_target_remote, NULL);
    if (res == 0) {
        puts("Unable to send");
    }
    LED0_OFF;
}

static int make_sock_ep(sock_udp_ep_t *ep, const char *addr)
{
    ep->port = 0;
    if (sock_udp_str2ep(ep, addr) < 0) {
        return -1;
    }
    ep->family  = AF_INET6;
    ep->netif   = SOCK_ADDR_ANY_NETIF;
    ep->port = GCOAP_PORT;
    return 0;
}

static inline bool jsoneq(const char *json, jsmntok_t *tok, const char *s)
{
    /* Das Objekt ist ein String */
    return (tok->type == JSMN_STRING)
        /* Der String ist so lang wie erwartet */
        && ((int) strlen(s) == tok->end - tok->start)
        /* Der String hat den erwarteten Wert */
        && (strncmp(json + tok->start, s, tok->end - tok->start) == 0);
}

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

    jsmn_init(&_json_parser);
    num = jsmn_parse(&_json_parser, payload, payload_len, toks,
                     sizeof(toks) / sizeof(toks[0]));
    if (num < 0) {
        printf("Failed to parse JSON: %s (res: %d)\n", payload, payload_len);
        return COAP_CODE_BAD_REQUEST;
    }
    if ((num < 1) || (toks[0].type != JSMN_OBJECT)) {
        puts("Object expected at top-level");
        return COAP_CODE_BAD_REQUEST;
    }
    for (int i = 1; i < num; i++) {
        if (jsoneq(payload, &toks[i], "addr")) {
            if (toks[i + 1].type != JSMN_STRING) {
                /* Wert von "addr" ist kein String */
                puts("\"addr\" is no a string");
                return COAP_CODE_BAD_REQUEST;
            }
            /* Setze Verbindungspartner zum Wert von "addr" */
            make_sock_ep(&_button_target_remote, payload + toks[i + 1].start);
            i++;
        }
        else if (jsoneq(payload, &toks[i], "path")) {
            if (toks[i + 1].type != JSMN_STRING) {
                /* Wert von "path" ist kein String */
                puts("\"path\" is no a string");
                return COAP_CODE_BAD_REQUEST;
            }
            strncpy(_button_target_path,
                    payload + toks[i + 1].start,
                    toks[i + 1].end - toks[i + 1].start);
            i++;
        }
        else {
            printf("Unexpected key: %.*s\n", toks[i].end - toks[i].start,
                   payload + toks[i].start);
            return COAP_CODE_BAD_REQUEST;
        }
    }
    puts("Button target set");
    return COAP_CODE_VALID;
}

static ssize_t _set_target(coap_pkt_t* pdu, uint8_t *buf, size_t len,
                           void *ctx)
{
    (void)ctx;
    switch (coap_method2flag(coap_get_code_detail(pdu))) {
        case COAP_POST: {
            unsigned code;

            /* Prüfe ob Nachricht das korrekte Format hat */
            if (pdu->content_type != COAP_FORMAT_JSON) {
                puts("Not a JSON object");
                code = COAP_CODE_BAD_REQUEST;
            }
            else {
                code = _set_target_from_payload((char *)pdu->payload,
                                                pdu->payload_len);
            }
            return gcoap_response(pdu, buf, len, code);
        }
        default:
            return 0;
    }
}

static const coap_resource_t _resources[] = {
    { BUTTON_SET_TARGET_PATH, COAP_POST, _set_target, NULL },
};

static gcoap_listener_t _listener = {
    (coap_resource_t *)&_resources[0],
    sizeof(_resources) / sizeof(_resources[0]),
    NULL
};

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

int main(void)
{
    LED0_OFF;
    sock_udp_ep_t corerd_server;

    _main_pid = sched_active_pid;
    gpio_init_int(BUTTON_PIN, GPIO_IN_PU, GPIO_FALLING, _button_press,
                  (void *)BUTTON_PIN);
    gcoap_register_listener(&_listener);

    xtimer_sleep(2);    /* warte ein wenig, bis sich das Netzwerk konfiguriert
                         * hat */

    if (make_sock_ep(&corerd_server, corerd_server_addr) < 0) {
        puts("Can not parse CORERD_SERVER_ADDR");
    }
    cord_ep_register(&corerd_server, NULL);
    _coap_client();
    return 0;
}

/** @} */
