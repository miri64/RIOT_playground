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
 * @{
 */
#define GNRC_IPV6_NIB_NUD_STATE_UNMANAGED   (0LU)   /**< not managed by NUD */
#define GNRC_IPV6_NIB_NUD_STATE_UNREACHABLE (1LU)   /**< entry is not reachable */
#define GNRC_IPV6_NIB_NUD_STATE_INCOMPLETE  (2LU)   /**< address resolution is currently performed */
#define GNRC_IPV6_NIB_NUD_STATE_STALE       (3LU)   /**< address might not be reachable */
#define GNRC_IPV6_NIB_NUD_STATE_DELAY       (4LU)   /**< NUD will be performed in a moment */
#define GNRC_IPV6_NIB_NUD_STATE_PROBE       (5LU)   /**< NUD is performed */
#define GNRC_IPV6_NIB_NUD_STATE_REACHABLE   (6LU)   /**< entry is reachable */
/** @} */

/**
 * @brief States for 6LoWPAN address registration (6Lo-AR)
 * @{
 */
#define GNRC_IPV6_NIB_AR_STATE_NONE         (0LU)   /**< not managed by 6Lo-AR */
#define GNRC_IPV6_NIB_AR_STATE_GC           (1LU)   /**< address can be removed when low on memory */
#define GNRC_IPV6_NIB_AR_STATE_TENTATIVE    (2LU)   /**< address registration still pending */
#define GNRC_IPV6_NIB_AR_STATE_REGISTERED   (3LU)   /**< address is registered */
/** @} */

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
    uint32_t flags;             /**< flags */
} gnrc_ipv6_nib_t;

#define GNRC_IPV6_NIB_FLAGS_PFX_LEN_MASK        (0x000000ff)
#define GNRC_IPV6_NIB_FLAGS_L2ADDR_LEN_MASK     (0x00000f00)
#define GNRC_IPV6_NIB_FLAGS_L2ADDR_LEN_POS      (8U)
#define GNRC_IPV6_NIB_FLAGS_NUD_STATE_MASK      (0x00007000)
#define GNRC_IPV6_NIB_FLAGS_NUD_STATE_POS       (12U)
#define GNRC_IPV6_NIB_FLAGS_IS_ROUTER           (0x00008000)
#define GNRC_IPV6_NIB_FLAGS_IFACE_MASK          (0x001f0000)
#define GNRC_IPV6_NIB_FLAGS_IFACE_POS           (16U)
#define GNRC_IPV6_NIB_FLAGS_AR_STATE_MASK       (0x00600000)
#define GNRC_IPV6_NIB_FLAGS_AR_STATE_POS        (21U)
#define GNRC_IPV6_NIB_FLAGS_USE_FOR_COMP        (0x00800000)
#define GNRC_IPV6_NIB_FLAGS_CID_MASK            (0x0f000000)
#define GNRC_IPV6_NIB_FLAGS_CID_POS             (24U)

static gnrc_ipv6_nib_t nib_entry;

int main(void)
{
    printf("sizeof(nib_entry) = %u\n", sizeof(nib_entry));
    nib_entry.flags &= ~GNRC_IPV6_NIB_FLAGS_PFX_LEN_MASK;
    nib_entry.flags |= (45LU & GNRC_IPV6_NIB_FLAGS_PFX_LEN_MASK);
    nib_entry.flags &= ~GNRC_IPV6_NIB_FLAGS_L2ADDR_LEN_MASK;
    nib_entry.flags |= (3LU << GNRC_IPV6_NIB_FLAGS_L2ADDR_LEN_POS) & GNRC_IPV6_NIB_FLAGS_L2ADDR_LEN_MASK;
    nib_entry.flags &= ~GNRC_IPV6_NIB_FLAGS_NUD_STATE_MASK;
    nib_entry.flags |= (GNRC_IPV6_NIB_NUD_STATE_STALE << GNRC_IPV6_NIB_FLAGS_NUD_STATE_POS) & GNRC_IPV6_NIB_FLAGS_NUD_STATE_MASK;
    nib_entry.flags |= GNRC_IPV6_NIB_FLAGS_IS_ROUTER;
    nib_entry.flags &= ~GNRC_IPV6_NIB_FLAGS_IFACE_MASK;
    nib_entry.flags |= (5LU << GNRC_IPV6_NIB_FLAGS_IFACE_POS) & GNRC_IPV6_NIB_FLAGS_IFACE_MASK;
    nib_entry.flags &= ~GNRC_IPV6_NIB_FLAGS_AR_STATE_MASK;
    nib_entry.flags |= (GNRC_IPV6_NIB_AR_STATE_TENTATIVE << GNRC_IPV6_NIB_FLAGS_AR_STATE_POS) & GNRC_IPV6_NIB_FLAGS_AR_STATE_MASK;
    nib_entry.flags |= GNRC_IPV6_NIB_FLAGS_USE_FOR_COMP;
    nib_entry.flags &= ~GNRC_IPV6_NIB_FLAGS_CID_MASK;
    nib_entry.flags |= (0x6LU << GNRC_IPV6_NIB_FLAGS_CID_POS) & GNRC_IPV6_NIB_FLAGS_CID_MASK;
    printf("pfx_len: %u\n", (unsigned)(nib_entry.flags & GNRC_IPV6_NIB_FLAGS_PFX_LEN_MASK));
    printf("l2addr_len: %u\n", (unsigned)((nib_entry.flags & GNRC_IPV6_NIB_FLAGS_L2ADDR_LEN_MASK) >> GNRC_IPV6_NIB_FLAGS_L2ADDR_LEN_POS));
    printf("nud_state: %u\n", (unsigned)((nib_entry.flags & GNRC_IPV6_NIB_FLAGS_NUD_STATE_MASK) >> GNRC_IPV6_NIB_FLAGS_NUD_STATE_POS));
    printf("is_router: %u\n", ((bool)(nib_entry.flags & GNRC_IPV6_NIB_FLAGS_IS_ROUTER)));
    printf("iface: %u\n", (unsigned)((nib_entry.flags & GNRC_IPV6_NIB_FLAGS_IFACE_MASK) >> GNRC_IPV6_NIB_FLAGS_IFACE_POS));
    printf("ar_state: %u\n", (unsigned)((nib_entry.flags & GNRC_IPV6_NIB_FLAGS_AR_STATE_MASK) >> GNRC_IPV6_NIB_FLAGS_AR_STATE_POS));
    printf("use_for_comp: %u\n", ((bool)(nib_entry.flags & GNRC_IPV6_NIB_FLAGS_USE_FOR_COMP)));
    printf("cid: %u\n", (unsigned)((nib_entry.flags & GNRC_IPV6_NIB_FLAGS_CID_MASK) >> GNRC_IPV6_NIB_FLAGS_CID_POS));

    return 0;
}
