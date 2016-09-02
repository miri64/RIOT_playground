/*
 * Copyright (C) 2015 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     tests
 * @{
 *
 * @file
 * @brief       Test application for AT86RF2xx network device driver
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <stdint.h>
#include <stdio.h>

#include "at86rf2xx.h"
#include "at86rf2xx_internal.h"
#include "at86rf2xx_params.h"
#include "xtimer.h"

#define A_SIZE          (32)

static at86rf2xx_t dev;
static uint8_t a[A_SIZE];

int main(void)
{
    netdev2_t *netdev = (netdev2_t *)&dev;

    xtimer_init();
    at86rf2xx_setup(&dev, &at86rf2xx_params[0]);
    netdev->driver->init(netdev);

    while (1) {
        at86rf2xx_get_random(&dev, a, sizeof(a));
        printf("> ");
        for (int i = 0; i < sizeof(a); i++) {
            printf("%02x ", a[i]);
        }
        puts("");
    }

    return 0;
}
