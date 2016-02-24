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
 * @brief   netdev related defintions
 *
 * @author  Martine Lenders <mlenders@inf.fu-berlin.de>
 */
#ifndef NETDEV_H_
#define NETDEV_H_

#include "net/netdev2_test.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NETDEV_NUMOF
#define NETDEV_NUMOF    (1U)    /**< number of network devices */
#endif

/**
 * @brief   Network devices for the test
 */
extern netdev2_test_t netdevs[NETDEV_NUMOF];

void netdev_init(void);

#ifdef __cplusplus
}
#endif

#endif /* NETDEV_H_ */
/** @} */
