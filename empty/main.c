/*
 * Copyright (C) 2016 Martine Lenders <mlenders@riot-os.org>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include "board.h"

int main(void)
{
#ifdef LED_RED_OFF
    LED_RED_OFF;
#endif
#ifdef LED_GREEN_OFF
    LED_GREEN_OFF;
#endif
#ifdef LED_ORANGE_OFF
    LED_ORANGE_OFF;
#endif
#ifdef LED_YELLOW_OFF
    LED_YELLOW_OFF;
#endif
#ifndef IDLE_TEST
    while (1);
#endif
    return 0;
}
