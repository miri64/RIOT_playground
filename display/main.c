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

#include "color.h"
#include "jsmn.h"
#include "led.h"
#include "mutex.h"
#include "net/cord/ep.h"
#include "net/gcoap.h"
#include "net/ipv6/addr.h"
#include "net/gnrc/netif.h"
#include "net/sock/util.h"
#include "lpd8808.h"
#include "lpd8808_params.h"
#include "xtimer.h"

#include "shell.h"

#define CORERD_SERVER_ADDR          "[2001:db8:f4ba:cbcd:1ac0:ffee:1ac0:ffee]"

#define LUKE_PAYLOAD_FMT            "{\"points\":%u}"
#define LUKE_PAYLOAD_MIN_SIZE       (sizeof("{\"points\":0}") - 1)
#define LUKE_PAYLOAD_MAX_SIZE       (sizeof("{\"points\":255}") - 1)

#define LUKE_PATH_POINTS            "/luke/points"
#define LUKE_PATH_VICTORY           "/luke/vic"

#define LUKE_POINTS_PER_LED         (1U)
#define LUKE_POINTS_MAX             (LPD8808_PARAM_LED_CNT * LUKE_POINTS_PER_LED)
#define LUKE_POINT_DROP_VALUE       (2U)
#define LUKE_POINT_DROP_TIMEOUT     (200U * US_PER_MS)

#define LUKE_TARGET_FMT             "{\"addr\":\"[%s]:%u\",\"path\":\"%s\"}"

#define LUKE_VICTORY_COND               (5U)
#define LUKE_VICTORY_RESET_THRESHOLD    (3U)
#define LUKE_VICTORY_TARGET_PATH_LEN    (16U)
#define LUKE_VICTORY_TARGET_PORT        (GCOAP_PORT)

#define LUKE_HUE_MAX                (120.0)

static const char corerd_server_addr[] = CORERD_SERVER_ADDR;
static ssize_t _luke_points(coap_pkt_t* pdu, uint8_t *buf, size_t len,
                            void *ctx);
static ssize_t _luke_victory(coap_pkt_t* pdu, uint8_t *buf, size_t len,
                             void *ctx);

static jsmn_parser _json_parser;
static lpd8808_t _dev;
static color_rgb_t _color_map[LPD8808_PARAM_LED_CNT];
static color_rgb_t _leds[LPD8808_PARAM_LED_CNT];
static const coap_resource_t _resources[] = {
    { LUKE_PATH_POINTS, COAP_POST | COAP_GET, _luke_points, NULL },
    { LUKE_PATH_VICTORY, COAP_POST | COAP_GET, _luke_victory, NULL },
};
static sock_udp_ep_t _victory_target_remote  = {
    .family = AF_INET6,
    .port = LUKE_VICTORY_TARGET_PORT,
    .netif = SOCK_ADDR_ANY_NETIF
};
static char _victory_target_path[LUKE_VICTORY_TARGET_PATH_LEN];
static gcoap_listener_t _listener = {
    (coap_resource_t *)&_resources[0],
    sizeof(_resources) / sizeof(_resources[0]),
    NULL
};
static mutex_t _points_mutex = MUTEX_INIT;
static uint32_t _victory_last_sent;
static uint16_t _points = 64U;
static uint16_t _last_notified;
static uint8_t _in_victory_cond = 0U;

static void _notify_points(void)
{
    uint8_t buf[GCOAP_PDU_BUF_SIZE];
    coap_pkt_t pdu;

    /* send Observe notification for /luke/points */
    switch (gcoap_obs_init(&pdu, &buf[0], GCOAP_PDU_BUF_SIZE,
            &_resources[0])) {
        case GCOAP_OBS_INIT_OK: {
            size_t payload_len = sprintf((char *)pdu.payload, LUKE_PAYLOAD_FMT,
                                         _points);
            size_t len;

            len = gcoap_finish(&pdu, payload_len, COAP_FORMAT_JSON);
            gcoap_obs_send(&buf[0], len, &_resources[0]);
            break;
        }
        default:
            break;
    }
}

static void _display_points(void)
{
    memset(&_leds[0], 0, sizeof(_leds));
    for (unsigned i = 0; i < (_points / LUKE_POINTS_PER_LED); i++) {
        memcpy(&_leds[i], &_color_map[i], sizeof(_leds[i]));
    }
    lpd8808_load_rgb(&_dev, _leds);
    if (_last_notified != _points) {
        _notify_points();
        _last_notified = _points;
    }
}

static void _increment_points(int p)
{
    uint16_t new_points;

    mutex_lock(&_points_mutex);
    new_points = _points + p;
    printf("increment by %i\n", p);
    if (new_points > LUKE_POINTS_MAX) {
        _points = LUKE_POINTS_MAX;
        _in_victory_cond++;
    }
    else {
        _points = new_points;
    }
    printf("incremented to %u\n", _points);
    _display_points();
    mutex_unlock(&_points_mutex);
}

static void _decrement_points(int p)
{
    mutex_lock(&_points_mutex);
    printf("decrement by %i\n", p);
    _points = (((unsigned)p > _points)) ? 0U : (uint8_t)(_points - p);
    if (_points < (LUKE_POINTS_MAX - LUKE_VICTORY_RESET_THRESHOLD)) {
        _in_victory_cond = 0;
    }
    printf("decremented to %u\n", _points);
    _display_points();
    mutex_unlock(&_points_mutex);
}

static ssize_t _luke_points(coap_pkt_t* pdu, uint8_t *buf, size_t len,
                            void *ctx)
{
    unsigned p = 0;
    unsigned code;

    (void)ctx;
    switch (coap_method2flag(coap_get_code_detail(pdu))) {
        case COAP_GET: {
            size_t payload_len;

            gcoap_resp_init(pdu, buf, len, COAP_CODE_CONTENT);
            mutex_lock(&_points_mutex);
            payload_len = sprintf((char *)pdu->payload, LUKE_PAYLOAD_FMT,
                                  _points);
            mutex_unlock(&_points_mutex);
            return gcoap_finish(pdu, payload_len, COAP_FORMAT_JSON);
        }
        case COAP_POST: {
            unsigned content_type = coap_get_content_type(pdu);
            if ((pdu->payload_len < LUKE_PAYLOAD_MIN_SIZE) ||
                (pdu->payload_len > LUKE_PAYLOAD_MAX_SIZE) ||
                (content_type != COAP_FORMAT_JSON) ||
                (sscanf((char *)pdu->payload, LUKE_PAYLOAD_FMT, &p) != 1)) {
                printf("(%u < %u) || (%u > %u) || (%u != %u) || "
                       "(payload unparsable)\n", pdu->payload_len,
                       LUKE_PAYLOAD_MIN_SIZE, pdu->payload_len, LUKE_PAYLOAD_MAX_SIZE,
                       content_type, COAP_FORMAT_JSON);
                code = COAP_CODE_BAD_REQUEST;
            }
            else {
                code = COAP_CODE_VALID;
                _increment_points(p);
            }
            puts("Sending response");
            return gcoap_response(pdu, buf, len, code);
        }
        default:
            return 0;
    }
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

static unsigned _set_target_from_payload(char *payload, size_t payload_len)
{
    int num;
    /* 5 expected tokens:
     *  1. top-level object
     *  2. key of addr
     *  3. value of addr
     *  4. key of path
     *  5. value of path */
    jsmntok_t toks[5];
    char *remote_tok = NULL, *path_tok = NULL;
    size_t path_tok_len = 0;

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
            i++;
            if (toks[i].type != JSMN_STRING) {
                puts("\"addr\" is no a string");
                return COAP_CODE_BAD_REQUEST;
            }
            remote_tok = payload + toks[i].start;
            payload[toks[i].end] = '\0';
        }
        else if (jsoneq(payload, &toks[i], "path")) {
            i++;

            if (toks[i].type != JSMN_STRING) {
                /* Wert von "path" ist kein String */
                puts("\"path\" is no a string");
                return COAP_CODE_BAD_REQUEST;
            }
            path_tok_len = toks[i].end - toks[i].start;
            path_tok = payload + toks[i].start;
        }
        /* sonst haben wir einen unerwarteten Schlüssel; das ist nicht erlaubt */
        else {
            printf("Unexpected key: %.*s\n", toks[i].end - toks[i].start,
                   payload + toks[i].start);
            return COAP_CODE_BAD_REQUEST;
        }
    }
    if (!remote_tok || !path_tok) {
        puts("path or addr unset");
        return COAP_CODE_BAD_REQUEST;
    }
    if (make_sock_ep(&_victory_target_remote, remote_tok) < 0) {
        printf("Unable to parse address '%s'\n", remote_tok);
        return COAP_CODE_BAD_REQUEST;
    }
    if (path_tok_len) {
        strncpy(_victory_target_path, path_tok, path_tok_len);
    }
    else {
        _victory_target_path[0] = '\0';
    }
    printf("Victory target set: {'addr': '%s', 'path': '%s'}\n", remote_tok,
           _victory_target_path);
    return COAP_CODE_VALID;
}

static ssize_t _luke_victory(coap_pkt_t* pdu, uint8_t *buf, size_t len,
                             void *ctx)
{
    char addr_str[48];  /* max address length in LUKE_TARGET_READ_FMT + 1 */
    unsigned code = COAP_CODE_BAD_REQUEST;

    (void)ctx;
    switch (coap_method2flag(coap_get_code_detail(pdu))) {
        case COAP_GET: {
            size_t payload_len;
            uint16_t port;

            gcoap_resp_init(pdu, buf, len, COAP_CODE_CONTENT);
            sock_udp_ep_fmt(&_victory_target_remote, addr_str, &port);
            payload_len = sprintf((char *)pdu->payload, LUKE_TARGET_FMT,
                                  addr_str, port, _victory_target_path);
            return gcoap_finish(pdu, payload_len, COAP_FORMAT_JSON);
        }
        case COAP_POST: {
            unsigned content_type = coap_get_content_type(pdu);

            if (content_type == COAP_FORMAT_JSON) {
                code = _set_target_from_payload((char *)pdu->payload,
                                                pdu->payload_len);
            }
            else {
                puts("Content-type not application/json");
            }
            puts("Sending response");
            return gcoap_response(pdu, buf, len, code);
        }
        default:
            return 0;
    }
}

static void _init_color_map(void)
{
    for (unsigned i = 0; i < LPD8808_PARAM_LED_CNT; i++) {
        color_hsv_t color_hsv = {
            .h = i * (LUKE_HUE_MAX / LUKE_POINTS_MAX),
            .s = 1.0,
            .v = 1.0,
        };
        uint8_t tmp;

        color_hsv2rgb(&color_hsv, &_color_map[i]);
        /* green and blue are switched */
        tmp = _color_map[i].g;
        _color_map[i].g = _color_map[i].b;
        _color_map[i].b = tmp;
    }
}

static void _post_to_victory_target(void)
{
    uint8_t buf[GCOAP_PDU_BUF_SIZE];
    coap_pkt_t pdu;
    int res;
    size_t len;
    size_t payload_len;
    uint32_t now = xtimer_now_usec();

    if (ipv6_addr_is_unspecified((ipv6_addr_t *)&_victory_target_remote.addr) ||
        (_victory_target_path[0] == '\0')) {
        puts("No target defined");
        return;
    }
    if ((_victory_last_sent - now) < US_PER_SEC) {
        puts("Last send less than a second ago");
        return;
    }
    gcoap_req_init(&pdu, buf, GCOAP_PDU_BUF_SIZE, COAP_POST,
                   _victory_target_path);
    payload_len = sprintf((char *)pdu.payload, LUKE_PAYLOAD_FMT,
                          _points);
    len = gcoap_finish(&pdu, payload_len, COAP_FORMAT_JSON);
    _victory_last_sent = xtimer_now_usec();
    res = gcoap_req_send2(buf, len, &_victory_target_remote, NULL);
    if (res == 0) {
        puts("Unable to send");
    }
}

int main(void)
{
    sock_udp_ep_t corerd_server;
    xtimer_ticks32_t now = xtimer_now();

    lpd8808_init(&_dev, &lpd8808_params[0]);
    lpd8808_load_rgb(&_dev, _leds);
    gcoap_register_listener(&_listener);
    puts("Sleeping for 2 seconds");
    xtimer_sleep(2);
    if (make_sock_ep(&corerd_server, corerd_server_addr) < 0) {
        puts("Can not parse CORERD_SERVER_ADDR");
    }
    puts("Register to CoRE RD server");
    cord_ep_register(&corerd_server, NULL);
    /* signal ready to display */
    puts("Init complete");
    _init_color_map();
    while (1) {
        if (_in_victory_cond < LUKE_VICTORY_COND) {
            LED0_ON;
            _post_to_victory_target();
        }
        else {
            LED0_OFF;
        }
        _decrement_points(LUKE_POINT_DROP_VALUE);
        xtimer_periodic_wakeup(&now, LUKE_POINT_DROP_TIMEOUT);
    }
    return 0;
}

/** @} */
