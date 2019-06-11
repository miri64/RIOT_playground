/*
 * Copyright (C) 2019 Freie Universität Berlin
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

#include "jsmn.h"
#include "common.h"

static jsmn_parser _json_parser;
static sock_udp_ep_t _target_remote  = {
    .family = AF_INET6,
    .port = LUKE_TARGET_PORT,
    .netif = SOCK_ADDR_ANY_NETIF
};
static char _target_path[LUKE_TARGET_PATH_LEN];
static uint32_t _victory_last_sent;

static unsigned _set_target_from_payload(char *payload, size_t payload_len);

ssize_t luke_set_target(coap_pkt_t* pdu, uint8_t *buf, size_t len,
                        void *ctx)
{
    char addr_str[48];
    unsigned code = COAP_CODE_BAD_REQUEST;

    (void)ctx;
    switch (coap_method2flag(coap_get_code_detail(pdu))) {
        case COAP_GET: {
            size_t payload_len;
            uint16_t port;

            gcoap_resp_init(pdu, buf, len, COAP_CODE_CONTENT);
            sock_udp_ep_fmt(&_target_remote, addr_str, &port);
            payload_len = sprintf((char *)pdu->payload, LUKE_TARGET_FMT,
                                  addr_str, port, _target_path);
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

size_t post_points_to_target(unsigned points)
{
    uint8_t buf[GCOAP_PDU_BUF_SIZE];
    coap_pkt_t pdu;
    size_t len;
    size_t payload_len;
    uint32_t now = xtimer_now_usec();

    if (ipv6_addr_is_unspecified((ipv6_addr_t *)&_target_remote.addr) ||
        (_target_path[0] == '\0')) {
        puts("No target defined");
        return 0;
    }
    if ((_victory_last_sent - now) < US_PER_SEC) {
        puts("Last send less than a second ago");
        return 0;
    }
    gcoap_req_init(&pdu, buf, GCOAP_PDU_BUF_SIZE, COAP_POST,
                   _target_path);
    payload_len = sprintf((char *)pdu.payload, LUKE_POINTS_FMT,
                          points);
    len = gcoap_finish(&pdu, payload_len, COAP_FORMAT_JSON);
    _victory_last_sent = xtimer_now_usec();
    return gcoap_req_send2(buf, len, &_target_remote, NULL);
}

static inline bool _jsoneq(const char *json, jsmntok_t *tok, const char *s)
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
        if (_jsoneq(payload, &toks[i], "addr")) {
            i++;
            if (toks[i].type != JSMN_STRING) {
                puts("\"addr\" is no a string");
                return COAP_CODE_BAD_REQUEST;
            }
            remote_tok = payload + toks[i].start;
            payload[toks[i].end] = '\0';
        }
        else if (_jsoneq(payload, &toks[i], "path")) {
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
    if (make_sock_ep(&_target_remote, remote_tok) < 0) {
        printf("Unable to parse address '%s'\n", remote_tok);
        return COAP_CODE_BAD_REQUEST;
    }
    if (path_tok_len) {
        strncpy(_target_path, path_tok, path_tok_len);
    }
    else {
        _target_path[0] = '\0';
    }
    printf("Victory target set: {'addr': '%s', 'path': '%s'}\n", remote_tok,
           _target_path);
    return COAP_CODE_VALID;
}


/** @} */
