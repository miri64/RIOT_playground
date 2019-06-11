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
#include "led.h"
#include "mutex.h"
#include "net/cord/ep.h"
#include "net/gcoap.h"
#include "net/ipv6/addr.h"
#include "net/gnrc/netif.h"
#include "lpd8808.h"
#include "lpd8808_params.h"
#include "xtimer.h"

#include "common.h"

#define LUKE_PATH_VICTORY           "/luke/vic"

#define LUKE_POINTS_PER_LED         (1U)
#define LUKE_POINTS_MAX             (LPD8808_PARAM_LED_CNT * LUKE_POINTS_PER_LED)
#define LUKE_POINT_DROP_VALUE       (2U)
#define LUKE_POINT_DROP_TIMEOUT     (200U * US_PER_MS)

#define LUKE_VICTORY_COND               (5U)
#define LUKE_VICTORY_RESET_THRESHOLD    (3U)

#define LUKE_HUE_MAX                (120.0)

static const char corerd_server_addr[] = CORERD_SERVER_ADDR;
static ssize_t _luke_points(coap_pkt_t* pdu, uint8_t *buf, size_t len,
                            void *ctx);

static lpd8808_t _dev;
static color_rgb_t _color_map[LPD8808_PARAM_LED_CNT];
static color_rgb_t _leds[LPD8808_PARAM_LED_CNT];
static const coap_resource_t _resources[] = {
    { LUKE_PATH_POINTS, COAP_POST | COAP_GET, _luke_points, NULL },
    { LUKE_PATH_VICTORY, COAP_POST | COAP_GET, luke_set_target, NULL },
};
static gcoap_listener_t _listener = {
    (coap_resource_t *)&_resources[0],
    sizeof(_resources) / sizeof(_resources[0]),
    NULL
};
static mutex_t _points_mutex = MUTEX_INIT;
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
            size_t payload_len = sprintf((char *)pdu.payload, LUKE_POINTS_FMT,
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
            payload_len = sprintf((char *)pdu->payload, LUKE_POINTS_FMT,
                                  _points);
            mutex_unlock(&_points_mutex);
            return gcoap_finish(pdu, payload_len, COAP_FORMAT_JSON);
        }
        case COAP_POST: {
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
                _increment_points(p);
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
        if (_in_victory_cond >= LUKE_VICTORY_COND) {
            LED0_ON;
            if (post_points_to_target(_points) == 0) {
                puts("Unable to send CoAP message");
            }
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
