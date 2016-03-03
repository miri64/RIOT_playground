/*
 * Copyright (C) 2016 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @author Martine Lenders <mlenders@inf.fu-berlin.de>
 */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "byteorder.h"
#include "msg.h"
#include "mutex.h"
#include "net/af.h"
#include "net/conn/udp.h"
#include "net/ipv6.h"
#include "net/inet_csum.h"
#include "net/protnum.h"
#include "net/sixlowpan.h"
#include "net/udp.h"
#include "thread.h"
#include "xtimer.h"

#include "netdev.h"
#include "stack.h"

#include "exp.h"

#define IEEE802154_MAX_FRAME_SIZE   (125U)
#define MHR_LEN                     (23U)
#if (((EXP_DST_PORT & 0xfff0) == 0xf0b0) && ((EXP_SRC_PORT & 0xfff0) == 0xf0b0))
#define IPHC_LEN                    (6U)
#elif ((EXP_DST_PORT & 0xff00) == 0xf000) || ((EXP_SRC_PORT & 0xff00) == 0xf000)
#define IPHC_LEN                    (8U)
#else
#define IPHC_LEN                    (9U)
#endif
#define IPUDP_LEN                   (sizeof(ipv6_hdr_t) + sizeof(udp_hdr_t))
#define MAX_FRAGMENTS               (18)
#define TIMER_WINDOW_SIZE           (16)
#define THREAD_PRIO                 (THREAD_PRIORITY_MAIN - 1)
#define THREAD_STACK_SIZE           (THREAD_STACKSIZE_DEFAULT + \
                                     THREAD_EXTRA_STACKSIZE_PRINTF)
#define THREAD_MSG_QUEUE_SIZE       (8)

static netdev2_t const *netdev = (netdev2_t *)&netdevs[0];
static const ipv6_addr_t src = EXP_ADDR;
static ipv6_addr_t dst;
static const uint8_t src_l2[] = EXP_ADDR_L2;
static uint8_t dst_l2[] = NETDEV_ADDR_PREFIX;
static uint32_t timer_window[TIMER_WINDOW_SIZE];
static uint8_t recv_buffer[EXP_MAX_PAYLOAD];
static uint8_t uncomp_buffer[EXP_MAX_PAYLOAD + IPUDP_LEN];
static uint8_t frag_buf[MAX_FRAGMENTS][IEEE802154_MAX_FRAME_SIZE];
static uint8_t frag_buf_len[MAX_FRAGMENTS];
static char thread_stack[THREAD_STACK_SIZE];
static msg_t thread_msg_queue[THREAD_MSG_QUEUE_SIZE];
static mutex_t sync = MUTEX_INIT;

static uint16_t payload_size;
static uint8_t _frag = 0;
static uint8_t _id = 0;

static inline uint8_t *_sixlowpan_buf(uint8_t *buf);
static inline void _init_frag1(uint8_t *buf, unsigned size, unsigned tag);
static inline void _init_fragn(uint8_t *buf, unsigned size, unsigned tag,
                               unsigned offset);
static inline void _init_iphc(uint8_t *buf);
static inline void _init_ipv6(ipv6_hdr_t *buf);
static inline void _set_chksum(uint8_t *slo_buf, const udp_hdr_t *udp_buf);
static inline udp_hdr_t *_udp_buf(void);
static inline void _init_udp(uint16_t src_port, uint16_t dst_port,
                             uint16_t length);
static void prepare_mhrs(void);
static bool prepare_sixlowpan(void);
static void reset_frag_buf(void);
static inline int min(const int a, const int b);

static void _netdev_isr(netdev2_t *dev)
{
    netdev2_test_t *netdev = (netdev2_test_t *)dev;
    mutex_unlock(&netdev->mutex);
    dev->event_callback(dev, NETDEV2_EVENT_RX_COMPLETE,
                        dev->isr_arg);
    mutex_lock(&netdev->mutex);
}

static int _netdev_recv(netdev2_t *dev, char *buf, int len, void *info)
{
    (void)dev;
    if (buf != NULL) {
        uint8_t frag;
        if (len < frag_buf_len[_frag]) {
            mutex_unlock(&sync);
            return -ENOBUFS;
        }
        frag = _frag++;
        if (frag == 0) {
            timer_window[_id % TIMER_WINDOW_SIZE] = xtimer_now();
        }
        if (info != NULL) {
            netdev2_ieee802154_rx_info_t *radio_info = info;
            radio_info->rssi = 255;
            radio_info->lqi = 35;
        }
        memcpy(buf, frag_buf[frag], frag_buf_len[frag]);
        mutex_unlock(&sync);
        return frag_buf_len[frag];
    }
    return frag_buf_len[_frag];
}

void *_thread(void *arg)
{
    conn_udp_t conn;
    ipv6_addr_t addr = IPV6_ADDR_UNSPECIFIED;
    size_t addr_len;
    uint16_t port;

    (void)arg;
    memset(&conn, 0, sizeof(conn));
    msg_init_queue(thread_msg_queue, THREAD_MSG_QUEUE_SIZE);
    conn_udp_create(&conn, &addr, sizeof(addr), AF_INET6, EXP_DST_PORT);
    conn_udp_getlocaladdr(&conn, &addr, &port);

    while (1) {
        int res;
        if ((res = conn_udp_recvfrom(&conn, recv_buffer, sizeof(recv_buffer),
                                     &addr, &addr_len, &port)) > 0) {
            uint32_t stop = xtimer_now();
            uint8_t id = recv_buffer[0];
            printf("%d,%" PRIu32 "\n", res,
                   stop - timer_window[id % TIMER_WINDOW_SIZE]);
        }
        else {
            /* find a reason to stop to not have conn_udp_close() optimized out
             */
            break;
        }
    }
    /* for size comparison include remaining conn_udp functions */
    conn_udp_close(&conn);
    return NULL;
}

void exp_run(void)
{
    dst_l2[7] = 0;

    ipv6_addr_set_link_local_prefix(&dst);
    ipv6_addr_set_aiid(&dst, dst_l2);
    dst.u8[8] ^= 0x02;
#ifdef STACK_MULTIHOP
    const ipv6_addr_t *gua;
    ipv6_addr_t prefix = EXP_PREFIX, unspec = IPV6_ADDR_UNSPECIFIED;
    gua = stack_add_prefix(0, &prefix, EXP_PREFIX_LEN);
    stack_add_route(0, &unspec, 0, &dst);
#ifdef STACK_RPL
    stack_init_rpl(0, gua);
#else
    (void)gua;
#endif
#endif
    prepare_mhrs();
    _init_ipv6((ipv6_hdr_t *)uncomp_buffer);
    if (thread_create(thread_stack, sizeof(thread_stack), THREAD_PRIO,
                      THREAD_CREATE_STACKTEST, _thread, NULL, "receiver") < 0) {
        return;
    }
    netdev2_test_set_isr_cb(&netdevs[0], _netdev_isr);
    netdev2_test_set_recv_cb(&netdevs[0], _netdev_recv);
    puts("payload_len,rx_traversal_time");
    for (payload_size = EXP_PAYLOAD_STEP; payload_size <= EXP_MAX_PAYLOAD;
         payload_size += EXP_PAYLOAD_STEP) {
        bool fragmented = prepare_sixlowpan();
        for (unsigned id = 0; id < EXP_RUNS; id++) {
            mutex_lock(&sync);
            _id = id;
            size_t offset = IPUDP_LEN;
            unsigned fragments = 0;

            memset(&uncomp_buffer[offset], id & 0xff, payload_size);
            _init_udp(EXP_SRC_PORT, EXP_DST_PORT, payload_size);
            if (fragmented) {
                _set_chksum(&(frag_buf[0][frag_buf_len[0] - IPHC_LEN]),
                            _udp_buf());
                for (unsigned i = 0; i < MAX_FRAGMENTS; i++) {
                    sixlowpan_frag_t *f = (sixlowpan_frag_t *)_sixlowpan_buf(frag_buf[i]);
                    unsigned max_len;

                    f->tag = byteorder_htons(id);
                    if (offset >= (IPUDP_LEN + payload_size)) {
                        frag_buf_len[i] = 0;
                        continue;
                    }
#if defined(MODULE_LWIP)
                    if (i == 0) {
                        max_len = min((((IEEE802154_MAX_FRAME_SIZE - frag_buf_len[i]) >> 3) << 3) - IPHC_LEN,
                                      IPUDP_LEN + payload_size - offset);
                    }
                    else {
                        max_len = min((((IEEE802154_MAX_FRAME_SIZE - frag_buf_len[i]) >> 3) << 3),
                                      IPUDP_LEN + payload_size - offset);
                    }
#else
                    max_len = min(((IEEE802154_MAX_FRAME_SIZE - frag_buf_len[i]) >> 3) << 3,
                                  IPUDP_LEN + payload_size - offset);
#endif
                    for (unsigned j = 0; j < max_len; j++) {
                        frag_buf[i][frag_buf_len[i]++] = uncomp_buffer[offset++];
                    }
                    fragments++;
                }
            }
            else {
                _set_chksum(&(frag_buf[0][frag_buf_len[0] - IPHC_LEN]),
                            _udp_buf());
                while (offset < (IPUDP_LEN + payload_size)) {
                    frag_buf[0][frag_buf_len[0]++] = uncomp_buffer[offset++];
                }
                fragments = 1;
            }
            for (unsigned i = 0; i < fragments; i++) {
                netdev->event_callback((netdev2_t *)netdev, NETDEV2_EVENT_ISR,
                                       netdev->isr_arg);
                mutex_lock(&sync);  /* sync with netdev2 */
#if EXP_FRAGMENT_DELAY
                xtimer_usleep(EXP_FRAGMENT_DELAY);
#endif
            }
            _frag = 0;
#if EXP_PACKET_DELAY
            xtimer_usleep(EXP_PACKET_DELAY);
#endif
            reset_frag_buf();
            mutex_unlock(&sync);
        }
    }
    /* for size comparison include remaining conn_udp functions */
    conn_udp_sendto(recv_buffer, 2, &dst, sizeof(dst), &src, sizeof(src), AF_INET6,
                    EXP_SRC_PORT, EXP_DST_PORT);
}

static inline uint8_t *_sixlowpan_buf(uint8_t *buf)
{
    return buf + MHR_LEN;
}

static inline void _init_frag1(uint8_t *buf, unsigned size, unsigned tag)
{
    sixlowpan_frag_t *frag1 = (sixlowpan_frag_t *)_sixlowpan_buf(buf);

    frag1->disp_size = byteorder_htons(size);
    frag1->disp_size.u8[0] &= ~SIXLOWPAN_FRAG_1_DISP;
    frag1->disp_size.u8[0] |= SIXLOWPAN_FRAG_1_DISP;
    frag1->tag = byteorder_htons(tag);
}

static inline void _init_fragn(uint8_t *buf, unsigned size, unsigned tag,
                               unsigned offset)
{
    sixlowpan_frag_n_t *fragn = (sixlowpan_frag_n_t *)_sixlowpan_buf(buf);

    fragn->disp_size = byteorder_htons(size);
    fragn->disp_size.u8[0] &= ~SIXLOWPAN_FRAG_N_DISP;
    fragn->disp_size.u8[0] |= SIXLOWPAN_FRAG_N_DISP;
    fragn->tag = byteorder_htons(tag);
    fragn->offset = offset >> 3;
}

static inline void _init_iphc(uint8_t *buf)
{
    buf[0] = SIXLOWPAN_IPHC1_DISP;
    buf[0] |= SIXLOWPAN_IPHC1_TF | SIXLOWPAN_IPHC1_NH;
    buf[0] |= 0x02;     /* Hop-limit: 64 */
    buf[1] = 0;
    buf[1] |= SIXLOWPAN_IPHC2_SAM;
    buf[1] |= SIXLOWPAN_IPHC2_DAM;
    buf[2] = 0xf0;      /* UDP LOWPAN_NHC ID */
#if (((EXP_DST_PORT & 0xfff0) == 0xf0b0) && ((EXP_SRC_PORT & 0xfff0) == 0xf0b0))
    buf[2] |= 0x03;
    buf[3] = (((EXP_SRC_PORT) & 0xf) << 4) | ((EXP_DST_PORT) & 0xf);
#elif ((EXP_DST_PORT & 0xff00) == 0xf000)
    buf[2] |= 0x01;
    buf[3] = (EXP_SRC_PORT >> 8);
    buf[4] = (EXP_SRC_PORT & 0xff);
    buf[5] = (EXP_DST_PORT & 0xff);
#elif ((EXP_SRC_PORT & 0xff00) == 0xf000)
    buf[2] |= 0x02;
    buf[3] = (EXP_SRC_PORT & 0xff);
    buf[4] = (EXP_DST_PORT >> 8);
    buf[5] = (EXP_DST_PORT & 0xff);
#else
    buf[3] = (EXP_SRC_PORT >> 8);
    buf[4] = (EXP_SRC_PORT & 0xff);
    buf[5] = (EXP_DST_PORT >> 8);
    buf[6] = (EXP_DST_PORT & 0xff);
#endif
}

static inline void _set_chksum(uint8_t *slo_buf, const udp_hdr_t *udp_buf)
{
    slo_buf[IPHC_LEN - 2] = udp_buf->checksum.u8[0];
    slo_buf[IPHC_LEN - 1] = udp_buf->checksum.u8[1];
}

static inline void _init_ipv6(ipv6_hdr_t *buf)
{
    memcpy(&buf->src, &src, sizeof(src));
    memcpy(&buf->dst, &dst, sizeof(dst));
}

static inline udp_hdr_t *_udp_buf(void)
{
    return (udp_hdr_t *)(&uncomp_buffer[sizeof(ipv6_hdr_t)]);
}

static inline void _init_udp(uint16_t src_port, uint16_t dst_port,
                             uint16_t length)
{
    udp_hdr_t *buf = _udp_buf();
    uint16_t chksum = 0;

    length += sizeof(udp_hdr_t);
    buf->src_port = byteorder_htons(src_port);
    buf->dst_port = byteorder_htons(dst_port);
    buf->checksum.u16 = 0;
    buf->length = byteorder_htons(length);
    chksum = inet_csum(chksum, (uint8_t *)buf, length);
    chksum = ipv6_hdr_inet_csum(chksum,
                                (ipv6_hdr_t *)(((uint8_t *)buf) - sizeof(ipv6_hdr_t)),
                                PROTNUM_UDP, length);
    if (chksum == 0) {
        chksum = 0xffff;
    }
    buf->checksum = byteorder_htons(~chksum);
}

static void prepare_mhrs(void)
{
    for (unsigned i = 0; i < MAX_FRAGMENTS; i++) {
        const le_uint16_t pan_id = byteorder_btols(byteorder_htons(NETDEV_PAN_ID));
        ieee802154_set_frame_hdr(frag_buf[i], src_l2, sizeof(src_l2),
                                 dst_l2, sizeof(dst_l2), pan_id, pan_id,
                                 IEEE802154_FCF_TYPE_DATA | IEEE802154_FCF_ACK_REQ,
                                 i);
    }
}

static bool prepare_sixlowpan(void)
{
    if (payload_size > (IEEE802154_MAX_FRAME_SIZE - MHR_LEN - IPHC_LEN)) {
#if defined(MODULE_LWIP)    /* lwIP uses datagram size *after* compression */
        int fragn_offset = ((IEEE802154_MAX_FRAME_SIZE - MHR_LEN -
                             sizeof(sixlowpan_frag_t) - IPHC_LEN) >> 3) << 3;
#else
        int fragn_offset = ((IEEE802154_MAX_FRAME_SIZE - MHR_LEN -
                             sizeof(sixlowpan_frag_t) - IPHC_LEN +
                             sizeof(ipv6_hdr_t) + sizeof(udp_hdr_t)) >> 3) << 3;
#endif
        _init_iphc(_sixlowpan_buf(frag_buf[0]) + sizeof(sixlowpan_frag_t));
#if defined(MODULE_LWIP)    /* lwIP uses datagram size *after* compression */
        _init_frag1(frag_buf[0], payload_size + IPHC_LEN, 0);
#else
        _init_frag1(frag_buf[0], payload_size + IPUDP_LEN, 0);
#endif
        frag_buf_len[0] = MHR_LEN + sizeof(sixlowpan_frag_t) + IPHC_LEN;
        for (unsigned frag = 1; frag < MAX_FRAGMENTS; frag++) {
#if defined(MODULE_LWIP)    /* lwIP uses datagram size *after* compression */
            _init_fragn(frag_buf[frag], payload_size + IPHC_LEN, 0,
                        fragn_offset);
#else
            _init_fragn(frag_buf[frag], payload_size + IPUDP_LEN, 0,
                        fragn_offset);
#endif
            _init_iphc(_sixlowpan_buf(frag_buf[frag]) + sizeof(sixlowpan_frag_n_t));
            fragn_offset += ((IEEE802154_MAX_FRAME_SIZE - MHR_LEN -
                             sizeof(sixlowpan_frag_n_t)) >> 3) << 3;
            frag_buf_len[frag] = MHR_LEN + sizeof(sixlowpan_frag_n_t);
        }
        return true;
    }
    else {
        _init_iphc(_sixlowpan_buf(frag_buf[0]));
        frag_buf_len[0] = MHR_LEN + IPHC_LEN;
        return false;
    }
}

static void reset_frag_buf(void)
{
    if (payload_size > (IEEE802154_MAX_FRAME_SIZE - MHR_LEN - IPHC_LEN)) {
        frag_buf_len[0] = MHR_LEN + IPHC_LEN + sizeof(sixlowpan_frag_t);
        for (unsigned frag = 1; frag < MAX_FRAGMENTS; frag++) {
            frag_buf_len[frag] = MHR_LEN + sizeof(sixlowpan_frag_n_t);
        }
    }
    else {
        frag_buf_len[0] = MHR_LEN + IPHC_LEN;
    }
}

static inline int min(const int a, const int b)
{
    return (a < b) ? a : b;
}

/** @} */
