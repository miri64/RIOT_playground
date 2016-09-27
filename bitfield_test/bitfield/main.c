/*
 * Copyright (C) 2016 Martine Lenders <mlenders@riot-os.org>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "net/ipv6/addr.h"

/**
 * @brief   States for neighbor unreachability detection
 *
 * @see [RFC 4861, section 7.3.2](https://tools.ietf.org/html/rfc4861#section-7.3.2)
 * @see [RFC 7048](https://tools.ietf.org/html/rfc7048)
 */
typedef enum {
    GNRC_IPV6_NIB_NUD_STATE_UNMANAGED = 0,  /**< not managed by NUD */
    GNRC_IPV6_NIB_NUD_STATE_UNREACHABLE,    /**< entry is not reachable */
    GNRC_IPV6_NIB_NUD_STATE_INCOMPLETE,     /**< address resolution is currently performed */
    GNRC_IPV6_NIB_NUD_STATE_STALE,          /**< address might not be reachable */
    GNRC_IPV6_NIB_NUD_STATE_DELAY,          /**< NUD will be performed in a moment */
    GNRC_IPV6_NIB_NUD_STATE_PROBE,          /**< NUD is performed */
    GNRC_IPV6_NIB_NUD_STATE_REACHABLE,      /**< entry is reachable */
} gnrc_ipv6_nib_nud_state_t;

/**
 * @brief States for 6LoWPAN address registration (6Lo-AR)
 */
typedef enum {
    GNRC_IPV6_NIB_AR_STATE_NONE = 0,        /**< not managed by 6Lo-AR */
    GNRC_IPV6_NIB_AR_STATE_GC,              /**< address can be removed when low on memory */
    GNRC_IPV6_NIB_AR_STATE_TENTATIVE,       /**< address registration still pending */
    GNRC_IPV6_NIB_AR_STATE_REGISTERED,      /**< address is registered */
} gnrc_ipv6_nib_ar_state_t;

/**
 * @brief   A public NIB entry
 */
typedef struct {
    ipv6_addr_t dst;            /**< A destination or prefix to destination */
    ipv6_addr_t next_hop;       /**< Next hop to gnrc_ipv6_nib_t::dst */
#if defined(MODULE_GNRC_SIXLOWPAN_ND_ROUTER) || defined(DOXYGEN)
    /**
     * @brief   The neighbors EUI-64 (used for DAD)
     *
     * 
     */
    eui64_t eui64;
#endif
    uint8_t l2addr[8];          /**< L2 address of next hop */
    unsigned pfx_len : 8;       /**< prefix-length of gnrc_ipv6_nib_t::dst */
    unsigned l2addr_len : 4;    /**< length of gnrc_ipv6_nib_t::l2addr */
    gnrc_ipv6_nib_nud_state_t nud_state : 3;
                                /**< NUD state of gnrc_ipv6_nib_t::next_hop */
    bool is_router : 1;         /**< gnrc_ipv6_nib_t::next_hop is router */
    unsigned iface : 5;         /**< interface to gnrc_ipv6_nib_t::dst */
    gnrc_ipv6_nib_ar_state_t ar_state : 2;
                                /**< 6Lo-AR state of gnrc_ipv6_nib_t::next_hop */
    /**
     * @brief   Use gnrc_ipv6_nib_t::cid for compression of gnrc_ipv6_nib_t::dst
     *          when using @ref net_gnrc_sixlowpan_iphc
     */
    bool use_for_comp : 1;

    /**
     * @brief   Context identifier for gnrc_ipv6_nib_t::dst for stateful header
     *          compression using @ref net_gnrc_sixlowpan_iphc
     */
    unsigned cid : 4;
} gnrc_ipv6_nib_t;

static gnrc_ipv6_nib_t nib_entry;

int main(void)
{
    printf("sizeof(nib_entry) = %u\n", sizeof(nib_entry));
    nib_entry.pfx_len = 45;
    nib_entry.l2addr_len = 3;
    nib_entry.nud_state = GNRC_IPV6_NIB_NUD_STATE_STALE;
    nib_entry.is_router = true;
    nib_entry.iface = 5;
    nib_entry.ar_state = GNRC_IPV6_NIB_AR_STATE_TENTATIVE;
    nib_entry.use_for_comp = true;
    nib_entry.cid = 0x6;
    printf("pfx_len: %u\n", nib_entry.pfx_len);
    printf("l2addr_len: %u\n", nib_entry.l2addr_len);
    printf("nud_state: %u\n", nib_entry.nud_state);
    printf("is_router: %u\n", nib_entry.is_router);
    printf("iface: %u\n", nib_entry.iface);
    printf("ar_state: %u\n", nib_entry.ar_state);
    printf("use_for_comp: %u\n", nib_entry.use_for_comp);
    printf("cid: %u\n", nib_entry.cid);

    return 0;
}
