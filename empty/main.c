/*
 * Copyright (C) 2016 Martine Lenders <mlenders@riot-os.org>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#ifdef MODULE_AT86RF2XX
#include "at86rf2xx.h"
#include "at86rf2xx_params.h"
#endif
#include "led.h"

#define AT86RF2XX_NUM (sizeof(at86rf2xx_params) / sizeof(at86rf2xx_params[0]))

int main(void)
{
#ifdef MODULE_AT86RF2XX
    for (int i = 0; i < AT86RF2XX_NUM; i++) {
        netopt_state_t sleep = NETOPT_STATE_SLEEP;
        at86rf2xx_t dev;
        netdev2_t *netdev = (netdev2_t *)&dev;

        at86rf2xx_setup(&dev, &at86rf2xx_params[i]);
        netdev->driver->init(netdev);
        netdev->driver->set(netdev, NETOPT_STATE, &sleep, sizeof(netopt_state_t));
    }

#endif
    LED0_OFF;
    LED1_OFF;
    LED2_OFF;
    LED3_OFF;
#ifndef IDLE_TEST
    while (1);
#endif
    return 0;
}
