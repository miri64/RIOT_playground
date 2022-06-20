/*
 * Copyright (C) 2021 Freie Universität Berlin
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

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "event/thread.h"
#include "fmt.h"
#include "led.h"
#include "net/af.h"
#include "net/coap.h"
#include "net/credman.h"
#include "net/dns/msg.h"
#include "net/ipv4/addr.h"
#include "net/ipv6/addr.h"
#include "net/sock/dns.h"
#include "periph/gpio.h"
#include "od.h"
#include "shell.h"

#if IS_USED(MODULE_GCOAP_DTLS)
#include "net/sock/dtls.h"
#endif
#include "net/gcoap/dns.h"

#if IS_USED(MODULE_GCOAP_DNS_OSCORE)
#include "oscore_native/crypto.h"
#endif

#define SWITCH_GPIO         GPIO_PIN(PA, 7)

#define PSK_ID_LEN          32U
#define PSK_LEN             32U

#define TEST_TAG            2599U
#define TEST_PSK_ID         "client_identity"
#define TEST_PSK            "secretPSK"

static void _check_switch_event_handler(event_t *event);

static char line_buf[512U];
static char _psk_id[PSK_ID_LEN];
static char _psk[PSK_LEN];
static credman_credential_t _credential = {
    .type = CREDMAN_TYPE_PSK,
    .params = {
        .psk = {
            .id = { .s = _psk_id, .len = 0U, },
            .key = { .s = _psk, .len = 0U, },
        }
    },
};
static uint8_t _mock_response[CONFIG_DNS_MSG_LEN];
static size_t _mock_response_len = 0U;
static uint8_t _resp_code = COAP_CODE_EMPTY;
static event_t _check_switch_event = {
    .handler = _check_switch_event_handler,
};

#if IS_USED(MODULE_GCOAP_DTLS)
static_assert(CONFIG_GCOAP_DNS_CREDS_MAX == CONFIG_DTLS_CREDENTIALS_MAX,
              "CONFIG_GCOAP_DNS_CREDS_MAX and CONFIG_DTLS_CREDENTIALS_MAX "
              "must be equal for this test");
#endif
static_assert(!IS_USED(MODULE_GCOAP_DTLS) ||
             (CONFIG_GCOAP_DNS_CREDS_MAX == CONFIG_CREDMAN_MAX_CREDENTIALS),
              "CONFIG_GCOAP_DNS_CREDS_MAX and CONFIG_CREDMAN_MAX_CREDENTIALS "
              "must be equal for this test");

#define INIT_TEST_PSK(t) \
    _credential.type = CREDMAN_TYPE_PSK; \
    _credential.tag = (t); \
    _credential.params.psk.id.len = sizeof(TEST_PSK_ID); \
    strcpy((char *)_credential.params.psk.id.s, TEST_PSK_ID); \
    _credential.params.psk.key.len = sizeof(TEST_PSK); \
    strcpy((char *)_credential.params.psk.key.s, TEST_PSK)

static void _check_switch(void) {
    if (gpio_read(SWITCH_GPIO)) {
        puts("Switch is OFF");
    }
    else {
        puts("Switch is ON");
    }
}

static void _check_switch_event_handler(event_t *event)
{
    (void)event;
    _check_switch();
}

static int _check_switch_shell_handler(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    _check_switch();
    return 0;
}

static void _uri_usage(const char *cmd)
{
    printf("usage: %s -d\n", cmd);
    printf("       %s <uri>\n", cmd);
}

static int _set_uri(int argc, char **argv)
{
    int res;

    if ((argc > 1) && (strcmp(argv[1], "-d") == 0)) {
        gcoap_dns_server_uri_set(NULL);
        puts("Successfully reset URI\n");
        return 0;
    }
    else if (argc == 1) {
        const char *uri = gcoap_dns_server_uri_get();

        if (uri) {
            puts(uri);
            return 0;
        }
        else {
            _uri_usage(argv[0]);
            return 1;
        }
    }
    /* argc > 1 can be assumed since argc == 1 returns above */
    res = gcoap_dns_server_uri_set(argv[1]);
    if (res < 0) {
        errno = -res;
        perror("Unable to set URI");
        return errno;
    }
    printf("Successfully added URI %s\n", argv[1]);
    return 0;
}

static void _dns_server_usage(char *cmd)
{
    printf("usage: %s <DNS server addr> [<DNS server port>]\n", cmd);
}

static int _dns_server(int argc, char **argv)
{
    char addrstr[INET6_ADDRSTRLEN];

    if (((argc > 1) && !inet_pton(AF_INET6, argv[1],
                                  sock_dns_server.addr.ipv6)) ||
        ((argc > 2) && ((sock_dns_server.port = atoi(argv[2])) == 0))) {
        _dns_server_usage(argv[0]);
        return 1;
    }
    if (argc < 2) {
        if (sock_dns_server.port) {
            inet_ntop(AF_INET6, sock_dns_server.addr.ipv6, addrstr,
                      sizeof(addrstr));
            printf("DNS server: [%s]:%u\n", addrstr, sock_dns_server.port);
        }
        else {
            puts("DNS server: -");
        }
        return 0;
    }
    if (argc < 3) {
        sock_dns_server.port = SOCK_DNS_PORT;
    }
    if (sock_dns_server.port > 0) {
        inet_ntop(AF_INET6, sock_dns_server.addr.ipv6, addrstr,
                  sizeof(addrstr));
        printf("DNS server: [%s]:%u\n", addrstr, sock_dns_server.port);
    }
    else {
        puts("DNS server: -");
    }
    sock_dns_server.family = AF_INET6;
    return 0;
}


static void _creds_usage(const char *cmd)
{
    printf("usage: %s -d <cred_tag>\n", cmd);
    printf("       %s <cred_tag> <psk_id> <psk>\n", cmd);
}

static int _creds(int argc, char **argv)
{
    if ((argc > 1) && (strcmp(argv[1], "-d") == 0)) {
        if (argc < 2) {
            _creds_usage(argv[0]);
            return 1;
        }
        credman_tag_t tag = atoi(argv[2]);

        if (tag == 0) {
            _creds_usage(argv[0]);
            return 1;
        }
        gcoap_dns_cred_remove(tag, CREDMAN_TYPE_PSK);
        printf("Successfully removed credentials with tag %u\n", tag);
        return 0;
    }
    if (argc < 4) {
        _creds_usage(argv[0]);
        return 1;
    }
    if ((_credential.tag = atoi(argv[1])) == 0) {
        _creds_usage(argv[0]);
        return 1;
    }
    if ((_credential.params.psk.id.len = strlen(argv[2])) > PSK_ID_LEN) {
        printf("PSK ID too long (max. %u bytes allowed)\n", PSK_ID_LEN);
        return 1;
    }
    if ((_credential.params.psk.key.len = strlen(argv[3])) > PSK_LEN) {
        printf("PSK too long (max. %u bytes allowed)\n", PSK_LEN);
        return 1;
    }
    _credential.type = CREDMAN_TYPE_PSK;
    strcpy((char *)_credential.params.psk.id.s, argv[2]);
    strcpy((char *)_credential.params.psk.key.s, argv[3]);

    int res = gcoap_dns_cred_add(&_credential);

    if (res < 0) {
        errno = -res;
        perror("Unable to add credential");
        return errno;
    }
    printf("Successfully added creds: %u, %s, %s\n", _credential.tag, argv[2], argv[3]);
    return 0;
}

static bool _parse_singlehex(char hex, uint8_t *target) {
    if ('0' <= hex && hex <= '9') {
        *target = hex - '0';
        return true;
    }
    if ('a' <= hex && hex <= 'f') {
        *target = hex - 'a' + 10;
        return true;
    }
    if ('A' <= hex && hex <= 'F') {
        *target = hex - 'A' + 10;
        return true;
    }
    return false;
}

static bool _parse_hex(const char *hex, size_t len, uint8_t *data) {
    uint8_t acc;
    while (len) {
        if (!_parse_singlehex(*hex++, &acc)) {
            return false;
        }
        *data = acc << 4;
        if (!_parse_singlehex(*hex++, &acc)) {
            return false;
        }
        *data |= acc;
        len--;
        data++;
    }
    if (len == 0 && *hex == '-') {
        hex++;
    }
    return *hex == 0;
}

#if IS_USED(MODULE_GCOAP_DNS_OSCORE)
static bool _parse_i64(char *chars, int64_t *out) {
    char *end = NULL;
    *out = strtoll(chars, &end, 10);
    if (*end != ' ' && *end != '\0' && *end != '\n')
        return false;
    return true;
}

static void _userctx_usage(const char *cmd)
{
    printf("usage: %s -i <alg> <common-iv>\n", cmd);
    printf("       %s -s <sender-id> <sender-key>\n", cmd);
    printf("       %s -r <recipient-id> <recipient-key>\n", cmd);
    printf("       %s -c\n", cmd);
    printf("(in that order)\n");
}

static int _userctx(int argc, char **argv)
{
    static int64_t alg_num;
    static uint8_t sender_key[32], recipient_key[32];
    static uint8_t common_iv[20];
    static uint8_t sender_id[OSCORE_KEYID_MAXLEN], recipient_id[OSCORE_KEYID_MAXLEN];
    static size_t sender_id_len, recipient_id_len;

    if ((argc < 2) || ((strcmp("-c", argv[1]) != 0) && (argc < 4))) {
        _userctx_usage(argv[0]);
        return 1;
    }

    if (strcmp("-i", argv[1]) == 0) {
        if (!_parse_i64(argv[2], &alg_num)) {
            printf("Algorithm number was not a number\n");
            return 1;
        }
        if (!_parse_hex(argv[3], oscore_crypto_aead_get_ivlength(alg_num), common_iv)) {
            printf("Invalid Common IV\n");
            return 1;
        }
        printf("Successfully initialized user context addition.\n");
    }
    else {
        if (alg_num == 0) {
            _userctx_usage(argv[0]);
            return 1;
        }
        if (strcmp("-s", argv[1]) == 0) {
            sender_id_len = strlen(argv[2]) / 2;
            if (sender_id_len > OSCORE_KEYID_MAXLEN) {
                printf("Sender ID too long\n");
                return 1;
            }
            if (!_parse_hex(argv[2], sender_id_len, sender_id)) {
                printf("Invalid Sender ID\n");
                return 1;
            }
            if (!_parse_hex(argv[3], oscore_crypto_aead_get_keylength(alg_num), sender_key)) {
                printf("Invalid Sender Key'\n");
                return 1;
            }
            printf("Sender key user context addition.\n");
        }
        else if (strcmp("-r", argv[1]) == 0) {
            recipient_id_len = strlen(argv[2]) / 2;
            if (recipient_id_len > OSCORE_KEYID_MAXLEN) {
                printf("Recipient ID too long\n");
                return 1;
            }
            if (!_parse_hex(argv[2], recipient_id_len, recipient_id)) {
                printf("Invalid Recipient ID\n");
                return 1;
            }
            if (!_parse_hex(argv[3], oscore_crypto_aead_get_keylength(alg_num), recipient_key)) {
                printf("Invalid Recipient Key\n");
                return 1;
            }
            printf("Recipient key user context addition.\n");
        }
        else if (strcmp("-c", argv[1]) == 0) {
            if (!sender_id_len || !recipient_id_len) {
                _userctx_usage(argv[0]);
                return 1;
            }
            if (gcoap_dns_oscore_set_secctx(alg_num, sender_id, sender_id_len,
                                            recipient_id, recipient_id_len,
                                            common_iv, sender_key, recipient_key) < 0) {
                puts("Unable to set OSCORE user context");
                return 1;
            }
            printf("Successfully added user context\n");
        }
        else {
            _userctx_usage(argv[0]); 
            return 1;
        }
    }
    return 0;
}
#endif

static int _proxy(int argc, char **argv)
{
    if (argc < 2) {
        const char *proxy = gcoap_dns_server_proxy_get();

        if (proxy) {
            puts(proxy);
        }
        else {
            printf("usage: %s [<proxy URI>|-]\n", argv[0]);
            return 1;
        }
    }
    else if (argv[1][0] == '-') {
        gcoap_dns_server_proxy_reset();
        puts("Successfully reset proxy URI\n");
    }
    else {
        int res = gcoap_dns_server_proxy_set(argv[1]);

        if (res < 0) {
            errno = -res;
            perror("Unable to set proxy URI");
            return errno;
        }
        printf("Successfully added proxy URI %s\n", argv[1]);
    }
    return 0;
}
static void _query_usage(const char *cmd)
{
    printf("usage: %s <hostname> [inet|inet6]\n", cmd);
}

int _parse_af(const char *family_name)
{
    if (strcmp("inet6", family_name) == 0) {
        return AF_INET6;
    }
    else if (strcmp("inet", family_name) == 0) {
        return AF_INET;
    }
    else {
        printf("Unexpected family %s\n", family_name);
        return AF_UNSPEC;
    }
}

int _print_addr(const char *hostname, const uint8_t *addr, int addr_len)
{
    char addr_str[IPV6_ADDR_MAX_STR_LEN];

    switch (addr_len) {
        case sizeof(ipv4_addr_t):
            printf("Hostname %s resolves to %s (IPv4)\n",
                   hostname, ipv4_addr_to_str(addr_str, (ipv4_addr_t *)addr,
                                              sizeof(addr_str)));
            break;
        case sizeof(ipv6_addr_t):
            printf("Hostname %s resolves to %s (IPv6)\n",
                   hostname, ipv6_addr_to_str(addr_str, (ipv6_addr_t *)addr,
                                              sizeof(addr_str)));
            break;
        default:
            printf("Unexpected address format resolved for hostname %s\n",
                   hostname);
            return 1;
    }
    return 0;
}

static int _query(int argc, char **argv)
{
    uint8_t addr_out[sizeof(ipv6_addr_t)];
    const char *hostname;
    int family = AF_INET6;
    int res;
    int offset = 0;

    if (argc < 2) {
        _query_usage(argv[0]);
        return 1;
    }
    if (argc < (offset + 2)) {
        _query_usage(argv[0]);
        return 1;
    }
    if (argc == (offset + 3)) {
        if ((family = _parse_af(argv[2 + offset])) == AF_UNSPEC) {
            return 1;
        }
    }
    hostname = argv[offset + 1];
    res = gcoap_dns_query(hostname, addr_out, family);
    if (res < 0) {
        errno = -res;
        perror("Unable to resolve query");
        return errno;
    }
    return _print_addr(hostname, addr_out, res);
}

static void _dns_usage(char *cmd)
{
    printf("usage: %s request <name> [inet|inet6]\n", cmd);
}

static int _dns(int argc, char **argv)
{
    uint8_t addr_out[sizeof(ipv6_addr_t)] = {0};
    int family = AF_INET6;

    if (argc < 2) {
        _dns_usage(argv[0]);
    }
    if (argc == 3) {
        if ((family = _parse_af(argv[2])) == AF_UNSPEC) {
            return 1;
        }
    }
    int res = sock_dns_query(argv[1], addr_out, family);

    return _print_addr(argv[1], addr_out, res);
}

static ssize_t _copy_mock_response(const char *str)
{
    size_t len = strlen(str) / 2; 
    size_t start = _mock_response_len;

    if ((_mock_response_len + len) >= CONFIG_DNS_MSG_LEN) {
        printf("Too many bytes added to response");
        return -ENOBUFS;
    }
    if (!_parse_hex(str, len, &_mock_response[start])) {
        printf("Unable to parse response hex\n");
        return -ENOBUFS;
    }
    _mock_response_len += len;
    return _mock_response_len - start;
}

static int _resp(int argc, char **argv)
{
    if (argc < 2) {
        od_hex_dump(_mock_response, _mock_response_len, OD_WIDTH_DEFAULT);
        return 0;
    }
    if (argc > 2 && (strcmp(argv[1], "-c") == 0)) {
        ssize_t size = _copy_mock_response(argv[2]);
        if (size >= 0) {
            printf("Successfully continued response\n");
            od_hex_dump(&_mock_response[_mock_response_len - size], size,
                        OD_WIDTH_DEFAULT);
            return 0;
        }
        else {
            return 1;
        }
    }
    _resp_code = atoi(argv[1]);
    if ((argc > 2) && ((_resp_code >> 5) == 2)) {
        _mock_response_len = 0;
        if (_copy_mock_response(argv[2]) < 0) {
            return 1;
        }
        printf("Successfully set response with code %u.%02u\n", _resp_code >> 5, _resp_code & 0x1f);
        od_hex_dump(_mock_response, _mock_response_len, OD_WIDTH_DEFAULT);
    }
    else {
        printf("Successfully set response code %u.%02u\n", _resp_code >> 5, _resp_code & 0x1f);
        _mock_response_len = 0;
    }
    return 0;
}

static const shell_command_t _shell_commands[] = {
    { "switch", "Checks if the switch is off or on", _check_switch_shell_handler },
    { "uri", "Sets URI to DoC server", _set_uri},
    { "server", "Sets DNS server", _dns_server },
    { "creds", "Adds/removes credentials for DoC server", _creds},
    { "proxy", "Sets proxy URI for DoC queries", _proxy},
    { "doc", "Sends DoC query for a hostname", _query},
    { "dns", "Sends DNS query for a hostname", _dns},
    { "resp", "Set static response for mock DoC server", _resp},
#if IS_USED(MODULE_GCOAP_DNS_OSCORE)
    { "userctx", "Adds/removes OSCORE security context for DoC server", _userctx},
#endif
    { NULL, NULL, NULL }
};

static void _gpio_irq(void *arg)
{
    event_post(EVENT_PRIO_MEDIUM, arg);
    if (gpio_read(SWITCH_GPIO)) {
        LED0_OFF;
    }
    else {
        LED0_ON;
    }
}

int main(void)
{
    gpio_init_int(SWITCH_GPIO, GPIO_IN_PU, GPIO_BOTH, _gpio_irq, &_check_switch_event);
    if (gpio_read(SWITCH_GPIO)) {
        LED0_OFF;
    }
    else {
        LED0_ON;
    }
    shell_run(_shell_commands, line_buf, sizeof(line_buf));
    return 0;
}

/** @} */
