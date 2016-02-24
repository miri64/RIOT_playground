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

#include "xtimer.h"

#include "netdev.h"
#include "stack.h"
#include "exp.h"

int main(void) {
    xtimer_init();
    netdev_init();
    stack_init();
    exp_run();
    return 0;
}

/** @} */
