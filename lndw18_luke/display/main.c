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

#include <stdatomic.h>

#include "color.h"
#include "mutex.h"
#include "net/gcoap.h"
#include "net/gnrc/netif.h"
#include "net/gnrc/ipv6/nib/abr.h"
#include "net/gnrc/sixlowpan/ctx.h"
#include "lpd8808.h"
#include "lpd8808_params.h"
#include "xtimer.h"

#include "shell.h"

#ifndef LUKE_DISPLAY_PREFIX
#define LUKE_DISPLAY_PREFIX         { 0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, \
                                      0, 0, 0, 0, 0, 0, 0, 0 }
#endif
#ifndef LUKE_DISPLAY_PREFIX_LEN
#define LUKE_DISPLAY_PREFIX_LEN     (64U)
#endif


#define LUKE_PAYLOAD_FMT            "{\"points\":%u}"
#define LUKE_PAYLOAD_MIN_SIZE       (sizeof("{\"points\":0}") - 1)
#define LUKE_PAYLOAD_MAX_SIZE       (sizeof("{\"points\":255}") - 1)
#define LUKE_POINT_PATH             "/luke/points"

#define LUKE_START_VALUE            (0U)
#define LUKE_POINT_DROP_VALUE       (4U)
#define LUKE_POINT_DROP_TIMEOUT     (2U * US_PER_SEC)

static ssize_t _post_points(coap_pkt_t* pdu, uint8_t *buf, size_t len,
                            void *ctx);

static lpd8808_t _dev;
static color_rgb_t _leds[LPD8808_PARAM_LED_CNT];
static const coap_resource_t _resources[] = {
    { LUKE_POINT_PATH, COAP_POST, _post_points, NULL },
};
static gcoap_listener_t _listener = {
    (coap_resource_t *)&_resources[0],
    sizeof(_resources) / sizeof(_resources[0]),
    NULL
};
static mutex_t _points_mutex = MUTEX_INIT;
static const color_rgb_t _color = { 0, 255, 0 };
static uint8_t _points = 0U;

static void _display_points(void)
{
    for (int i = 0; i < LPD8808_PARAM_LED_CNT; i++) {
        if (i < _points) {
            memcpy(&_leds[i], &_color, sizeof(_leds[i]));
        }
        else {
            memset(&_leds[i], 0, sizeof(_leds[i]));
        }
    }
    lpd8808_load_rgb(&_dev, _leds);
}

static void _increment_points(int p)
{
    mutex_lock(&_points_mutex);
    printf("increment by %i\n", _points);
    _points = ((_points + p) > LPD8808_PARAM_LED_CNT) ?
              LPD8808_PARAM_LED_CNT : (uint8_t)(_points + p);
    printf("incremented to %u\n", _points);
    _display_points();
    mutex_unlock(&_points_mutex);
}

static void _decrement_points(int p)
{
    mutex_lock(&_points_mutex);
    printf("decrement by %i\n", _points);
    _points = (((unsigned)p > _points)) ? 0U : (uint8_t)(_points - p);
    printf("decremented to %u\n", _points);
    _display_points();
    mutex_unlock(&_points_mutex);
}

static ssize_t _post_points(coap_pkt_t* pdu, uint8_t *buf, size_t len,
                            void *ctx)
{
    unsigned p = 0;
    unsigned code;

    (void)ctx;
    printf("POST %s\n", (char *)pdu->url);
    if ((pdu->payload_len < LUKE_PAYLOAD_MIN_SIZE) ||
        (pdu->payload_len > LUKE_PAYLOAD_MAX_SIZE) ||
        (pdu->content_type != COAP_FORMAT_JSON) ||
        (sscanf((char *)pdu->payload, LUKE_PAYLOAD_FMT, &p) != 1)) {
        printf("(%u < %u) || (%u > %u) || (%u != %u) || "
               "(payload unparsable)\n", pdu->payload_len,
               LUKE_PAYLOAD_MIN_SIZE, pdu->payload_len, LUKE_PAYLOAD_MAX_SIZE,
               pdu->content_type, COAP_FORMAT_JSON);
        code = COAP_CODE_BAD_REQUEST;
    }
    else {
        code = COAP_CODE_VALID;
        _increment_points(p);
    }
    puts("Sending response");
    return gcoap_response(pdu, buf, len, code);
}

static void _init_iface(void)
{
    ipv6_addr_t prefix = { .u8 = LUKE_DISPLAY_PREFIX }, addr;
    gnrc_netif_t *netif = gnrc_netif_iter(NULL);
    netopt_enable_t enable = NETOPT_ENABLE;
    char addr_str[IPV6_ADDR_MAX_STR_LEN];

    gnrc_netif_ipv6_addrs_get(netif, &addr, sizeof(addr));
    ipv6_addr_init_prefix(&addr, &prefix, LUKE_DISPLAY_PREFIX_LEN);
    gnrc_netif_ipv6_addr_add(netif, &addr, LUKE_DISPLAY_PREFIX_LEN,
                             GNRC_NETIF_IPV6_ADDRS_FLAGS_STATE_VALID);
    gnrc_netapi_set(netif->pid, NETOPT_IPV6_SND_RTR_ADV, 0, &enable,
                    sizeof(enable));
    gnrc_ipv6_nib_abr_add(&addr);
    printf("Set address to %s\n", ipv6_addr_to_str(addr_str, &addr,
                                                   sizeof(addr_str)));
}

int main(void)
{
    xtimer_ticks32_t now = xtimer_now();

    lpd8808_init(&_dev, &lpd8808_params[0]);
    lpd8808_load_rgb(&_dev, _leds);
    gcoap_register_listener(&_listener);
    _init_iface();
    while (1) {
        _decrement_points(LUKE_POINT_DROP_VALUE);
        xtimer_periodic_wakeup(&now, LUKE_POINT_DROP_TIMEOUT);
    }
    return 0;
}

/** @} */
