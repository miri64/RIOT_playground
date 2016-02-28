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

#include <stdio.h>

#include "msg.h"
#include "xtimer.h"

#include "netdev.h"
#include "stack.h"
#include "exp.h"

#define MAIN_MSG_QUEUE_SIZE (8)

static msg_t main_msg_queue[MAIN_MSG_QUEUE_SIZE];

int main(void) {
    printf("%s\n", APPLICATION_NAME);
    xtimer_init();
    msg_init_queue(main_msg_queue, MAIN_MSG_QUEUE_SIZE);
    netdev_init();
    stack_init();
    exp_run();
    return 0;
}

/** @} */
