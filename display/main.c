/*
 * Copyright (C) 2018-19 Freie Universität Berlin
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
#include "color.h"
#include "led.h"
#include "mutex.h"
#include "net/cord/ep.h"
#include "net/gcoap.h"
#include "net/ipv6/addr.h"
#include "net/gnrc/netif.h"
#include "lpd8808.h"
#include "lpd8808_params.h"
#include "thread_flags.h"
#include "xtimer.h"

#include "common.h"

#define LUKE_DISPLAY_PREFIX         "/dsp"
#define LUKE_PATH_DIFFICULTY        "/dif"

#define LUKE_DIFFICULTY_FMT         "{\"difficulty\":%u}"
#define LUKE_DIFFICULTY_MIN_SIZE    (sizeof("{\"difficulty\":1}") - 1)
#define LUKE_DIFFICULTY_MAX_SIZE    (sizeof("{\"difficulty\":10}") - 1)

#define LUKE_POINTS_PER_LED         (1U)
#define LUKE_POINTS_MAX             (LPD8808_PARAM_LED_CNT * LUKE_POINTS_PER_LED)
#define LUKE_POINT_DROP_VALUE       (2U)
#define LUKE_POINT_DROP_TIMEOUT_MAX (1000U * US_PER_MS)

#define LUKE_DIFFICULTY_DEFAULT     (9U)
#define LUKE_DIFFICULTY_MAX         (10U)

#define LUKE_VICTORY_COND               (5U)
#define LUKE_VICTORY_RESET_THRESHOLD    (3U)

#define LUKE_HUE_MAX                (120.0)
#define LUKE_THREAD_FLAG_DEC        (1 << 3)

static thread_t *_main_thread = NULL;
static const char corerd_server_addr[] = CORERD_SERVER_ADDR;
static ssize_t _luke_points(coap_pkt_t* pdu, uint8_t *buf, size_t len,
                            void *ctx);
static ssize_t _luke_difficulty(coap_pkt_t* pdu, uint8_t *buf, size_t len,
                                void *ctx);

static lpd8808_t _dev;
static color_rgb_t _color_map[LPD8808_PARAM_LED_CNT];
static color_rgb_t _leds[LPD8808_PARAM_LED_CNT];
static const coap_resource_t _resources[] = {
    { LUKE_DISPLAY_PREFIX LUKE_PATH_DIFFICULTY, COAP_POST | COAP_GET, _luke_difficulty, NULL },
    { LUKE_DISPLAY_PREFIX LUKE_PATH_POINTS, COAP_POST | COAP_GET, _luke_points, NULL },
    { LUKE_DISPLAY_PREFIX LUKE_PATH_TARGET, COAP_POST | COAP_GET, luke_set_target, NULL },
    { LUKE_PATH_REBOOT, COAP_POST, luke_reboot, NULL },
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
static volatile uint8_t _difficulty = LUKE_DIFFICULTY_DEFAULT;
static volatile uint8_t _last_reported_difficulty = !LUKE_DIFFICULTY_DEFAULT;

static void _notify_points(void)
{
    uint8_t buf[GCOAP_PDU_BUF_SIZE];
    coap_pkt_t pdu;

    /* send Observe notification for /luke/points */
    switch (gcoap_obs_init(&pdu, &buf[0], GCOAP_PDU_BUF_SIZE,
            &_resources[1])) {
        case GCOAP_OBS_INIT_OK: {
            size_t payload_len = sprintf((char *)pdu.payload, LUKE_POINTS_FMT,
                                         _points);
            size_t len;

            printf("Sending '%s' (%u bytes) to observers\n",
                   (char *)pdu.payload, (unsigned)payload_len);
            len = gcoap_finish(&pdu, payload_len, COAP_FORMAT_JSON);
            printf("The CoAP message has %u byte\n", (unsigned)len);
            gcoap_obs_send(&buf[0], len, &_resources[1]);
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

    assert(_main_thread);
    thread_flags_set(_main_thread, LUKE_THREAD_FLAG_DEC);
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

static void _notify_difficulty(void)
{
    uint8_t buf[GCOAP_PDU_BUF_SIZE];
    coap_pkt_t pdu;

    /* send Observe notification for /luke/points */
    switch (gcoap_obs_init(&pdu, &buf[0], GCOAP_PDU_BUF_SIZE,
            &_resources[0])) {
        case GCOAP_OBS_INIT_OK: {
            size_t payload_len = sprintf((char *)pdu.payload,
                                         LUKE_DIFFICULTY_FMT,
                                         _difficulty);
            size_t len;

            _last_reported_difficulty = _difficulty;
            printf("Sending '%s' (%u bytes) to observers\n",
                   (char *)pdu.payload, (unsigned)payload_len);
            len = gcoap_finish(&pdu, payload_len, COAP_FORMAT_JSON);
            printf("The CoAP message has %u byte\n", (unsigned)len);
            gcoap_obs_send(&buf[0], len, &_resources[0]);
            break;
        }
        default:
            break;
    }
}

static void _incr_difficulty(void *arg)
{
    (void)arg;
    if (_difficulty++ >= LUKE_DIFFICULTY_MAX) {
        /* loop to difficulty 1 */
        _difficulty = 1;
    }
    _points = (((uint16_t)_difficulty) * LUKE_POINTS_MAX) / LUKE_DIFFICULTY_MAX;
    /* possibly need to wake-up main thread to decrement points again */
    thread_flags_set(_main_thread, LUKE_THREAD_FLAG_DEC);
}

static ssize_t _luke_difficulty(coap_pkt_t* pdu, uint8_t *buf, size_t len,
                                void *ctx)
{
    unsigned d = 0;
    size_t payload_len = 0;

    puts("luke difficulty");
    (void)ctx;
    switch (coap_method2flag(coap_get_code_detail(pdu))) {
        case COAP_GET: {
            gcoap_resp_init(pdu, buf, len, COAP_CODE_CONTENT);
            payload_len = sprintf((char *)pdu->payload, LUKE_DIFFICULTY_FMT,
                                  _difficulty);
            return gcoap_finish(pdu, payload_len, COAP_FORMAT_JSON);
        }
        case COAP_POST: {
            unsigned content_type = coap_get_content_type(pdu);
            if ((pdu->payload_len < LUKE_DIFFICULTY_MIN_SIZE) ||
                (pdu->payload_len > LUKE_DIFFICULTY_MAX_SIZE) ||
                (content_type != COAP_FORMAT_JSON) ||
                (sscanf((char *)pdu->payload, LUKE_DIFFICULTY_FMT, &d) != 1)) {
                printf("(%u < %u) || (%u > %u) || (%u != %u) || "
                       "(payload unparsable)\n", pdu->payload_len,
                       LUKE_DIFFICULTY_MIN_SIZE, pdu->payload_len,
                       LUKE_DIFFICULTY_MAX_SIZE, content_type,
                      COAP_FORMAT_JSON);
                return gcoap_response(pdu, buf, len, COAP_CODE_BAD_REQUEST);
            }
            else {
                _notify_difficulty();
                gcoap_resp_init(pdu, buf, len, COAP_CODE_VALID);
                _difficulty = (d == 0) ? 1 : (d > 10) ? 10 : d;
                payload_len = sprintf((char *)pdu->payload, LUKE_DIFFICULTY_FMT,
                                      _difficulty);
                return gcoap_finish(pdu, payload_len, COAP_FORMAT_JSON);
            }
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
    xtimer_ticks32_t now;

    _main_thread = (thread_t *)sched_active_thread;
    gpio_init_int(BTN0_PIN, BTN0_MODE, GPIO_FALLING, _incr_difficulty, NULL);
    lpd8808_init(&_dev, &lpd8808_params[0]);
    lpd8808_load_rgb(&_dev, _leds);
    puts("Sleeping for 2 seconds");
    xtimer_sleep(2);
    gcoap_register_listener(&_listener);
    if (make_sock_ep(&corerd_server, corerd_server_addr) < 0) {
        puts("Can not parse CORERD_SERVER_ADDR");
    }
    puts("Register to CoRE RD server");
    cord_ep_register(&corerd_server, NULL);
    /* signal ready to display */
    puts("Init complete");
    _init_color_map();
    now = xtimer_now();
    while (1) {
        uint32_t timeout;
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
        if (_difficulty != _last_reported_difficulty) {
            _notify_difficulty();
        }
        timeout = LUKE_POINT_DROP_TIMEOUT_MAX -
            ((LUKE_POINT_DROP_TIMEOUT_MAX * (_difficulty - 1)) /
             LUKE_DIFFICULTY_MAX);
        if (_points > 0) {
            printf("timeout: %lu ms\n", (long unsigned)timeout);
            xtimer_periodic_wakeup(&now, timeout);
        }
        else {
            thread_flags_wait_any(LUKE_THREAD_FLAG_DEC);
            now = xtimer_now();
        }
    }
    return 0;
}

/** @} */
