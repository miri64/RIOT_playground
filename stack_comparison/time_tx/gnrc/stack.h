/*
 * Copyright (C) 2016 Martine Lenders <mlenders@inf.fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @brief   stack related defintitions
 *
 * @author  Martine Lenders <mlenders@inf.fu-berlin.de>
 */
#ifndef STACK_H_
#define STACK_H_

#include "net/ipv6/addr.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief   Initialize stack.
 */
void stack_init(void);

void stack_add_neighbor(int iface, const ipv6_addr_t *ipv6_addr,
                        const uint8_t *l2_addr, uint8_t l2_addr_len);
#ifdef STACK_MULTIHOP
const ipv6_addr_t *stack_add_prefix(int iface, const ipv6_addr_t *prefix,
                                    uint8_t prefix_len);
void stack_add_route(int iface, const ipv6_addr_t *prefix, uint8_t prefix_len,
                     const ipv6_addr_t *next_hop);
#endif
#ifdef STACK_RPL
void stack_init_rpl(int iface, const ipv6_addr_t *dodag_id);
#endif

#ifdef __cplusplus
}
#endif

#endif /* STACK_H_ */
/** @} */
