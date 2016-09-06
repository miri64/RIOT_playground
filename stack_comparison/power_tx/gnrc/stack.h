/*
 * Copyright (C) 2016 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup
 * @ingroup
 * @brief
 * @{
 *
 * @file
 * @brief
 *
 * @author  Martine Lenders <mlenders@inf.fu-berlin.de>
 */
#ifndef STACK_H_
#define STACK_H_

#include "net/ipv6/addr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UDP_PORT        1234
#define NUM_PACKETS     100

extern ipv6_addr_t dst;

void stack_init(void);

#ifdef __cplusplus
}
#endif

#endif /* STACK_H_ */
/** @} */
