/*
 * Copyright (C) 2014 Martin Lenders <mlenders@inf.fu-berlin.de>
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     examples
 * @{
 *
 * @file
 * @brief       Default application that shows a lot of functionality of RIOT
 *
 * @author      Martine Lenders <mlenders@inf.fu-berlin.de>
 *
 * @}
 */

#include <stdio.h>

#include "cpu-conf.h"
#include "thread.h"

#define THREAD1_STACKSIZE   (KERNEL_CONF_STACKSIZE_PRINTF)
#define THREAD2_STACKSIZE   (KERNEL_CONF_STACKSIZE_PRINTF)

static char thread1_stack[THREAD1_STACKSIZE];
static char thread2_stack[THREAD2_STACKSIZE];

static kernel_pid_t thread1_pid, thread2_pid;

typedef struct {
    int *a;
} test_struct_t;

static void *thread1(void *args)
{
    msg_t msg_req, msg_resp;
    test_struct_t req_struct;
    int counter = 0;

    (void)args;

    req_struct.a = &counter;
    msg_req.content.ptr = (char *)(&req_struct);

    while (1) {
        msg_send_receive(&msg_req, &msg_resp, thread2_pid);

        if (msg_resp.content.ptr == NULL) {
            printf("Response is null\n");
        }
        else {
            printf("*req_struct->a == %d; *resp_struct->a == %d\n", counter,
                   *(((test_struct_t *)msg_resp.content.ptr)->a));
        }
    }

    return NULL;
}

static void *thread2(void *args)
{
    msg_t msg_req, msg_resp;
    test_struct_t resp_struct;
    int counter = 0;

    (void)args;

    resp_struct.a = &counter;
    msg_resp.content.ptr = (char *)(&resp_struct);

    while (1) {
        msg_receive(&msg_req);

        (*(((test_struct_t *)msg_req.content.ptr)->a))++;
        (*(((test_struct_t *)msg_resp.content.ptr)->a))++;

        msg_reply(&msg_req, &msg_resp);
    }

    return NULL;
}

int main(void)
{
    thread1_pid = thread_create(thread1_stack, THREAD1_STACKSIZE, PRIORITY_MAIN - 1,
                                0, thread1, NULL, "thread1");
    thread2_pid = thread_create(thread2_stack, THREAD2_STACKSIZE, PRIORITY_MAIN - 1,
                                0, thread2, NULL, "thread2");
    return 0;
}
