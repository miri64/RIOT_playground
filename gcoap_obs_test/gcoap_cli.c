/*
 * Copyright (c) 2018 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @brief       Test application for CoAP OBSERVE
 *
 * @author      Martine Lenders <m.lenders@fu-berlin.de>
 *
 * @}
 */

#include <stdint.h>
#include <stdio.h>
#include "net/gcoap.h"
#include "mutex.h"

#define ENABLE_DEBUG (0)
#include "debug.h"

#define DATA_GEN_STACK_SIZE (THREAD_STACKSIZE_DEFAULT)
#define DATA_GEN_PRIO       (THREAD_PRIORITY_MAIN - 1)

static ssize_t _handle_test(coap_pkt_t *pdu, uint8_t *buf, size_t len,
                            void *ctx);

static char _data_gen_stack[DATA_GEN_STACK_SIZE];
mutex_t _data_gen_pauser = MUTEX_INIT;
static const char *_test_payload = "Test successfull";

/* CoAP resources */
static const coap_resource_t _resources[] = {
    { "/test", COAP_GET, _handle_test, NULL },
};

static gcoap_listener_t _listener = {
    (coap_resource_t *)&_resources[0],
    sizeof(_resources) / sizeof(_resources[0]),
    NULL
};

static bool locked = false;

static ssize_t _handle_test(coap_pkt_t *pdu, uint8_t *buf, size_t len,
                            void *ctx)
{
    (void)ctx;
    gcoap_resp_init(pdu, buf, len, COAP_CODE_CONTENT);
    memcpy(pdu->payload, _test_payload, strlen(_test_payload));
    if (locked) {
        locked = false;
        mutex_unlock(&_data_gen_pauser);
    }
    return gcoap_finish(pdu, strlen(_test_payload), COAP_FORMAT_TEXT);
}

static void *_data_gen(void *arg)
{
    (void)arg;
    while (1) {
        mutex_lock(&_data_gen_pauser);
        uint8_t buf[GCOAP_PDU_BUF_SIZE];
        coap_pkt_t pdu;
        size_t len;

        switch (gcoap_obs_init(&pdu, &buf[0], GCOAP_PDU_BUF_SIZE,
                &_resources[0])) {
            case GCOAP_OBS_INIT_OK:
                printf("gcoap_cli: creating /test notification\n");
                strcpy((char *)pdu.payload, _test_payload);
                len = gcoap_finish(&pdu, strlen(_test_payload),
                                   COAP_FORMAT_TEXT);
#ifdef MODULE_PKTCNT_FAST
                printf("%1u.%02u;%u-%s\n",
                       coap_get_code_class(&pdu),
                       coap_get_code_detail(&pdu),
                       coap_get_id(&pdu),
                       pktcnt_addr_str);
#endif
                gcoap_obs_send(&buf[0], len, &_resources[0]);
                break;
            case GCOAP_OBS_INIT_UNUSED:
                printf("gcoap_cli: no observer for /test\n");
                break;
            case GCOAP_OBS_INIT_ERR:
                printf("gcoap_cli: error initializing /test notification\n");
                break;
        }
        mutex_unlock(&_data_gen_pauser);
        xtimer_sleep(1U);
    }
    return NULL;
}

int pause_gen(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    if (!locked) {
        mutex_lock(&_data_gen_pauser);
    }
    else {
        mutex_unlock(&_data_gen_pauser);
    }
    locked = !locked;
    return 0;
}

void gcoap_cli_init(void)
{
    gcoap_register_listener(&_listener);
    thread_create(_data_gen_stack, DATA_GEN_STACK_SIZE, DATA_GEN_PRIO,
                  THREAD_CREATE_STACKTEST, _data_gen, NULL, "data-gen");
}
