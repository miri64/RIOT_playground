/*
 * Copyright (c) 2018 Freie Universität Berlin
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
 * @brief       gcoap OBSERVE test
 *
 * @author      Martine Lenders <m.lenders@fu-berlin.de>
 *
 * @}
 */

#include <stdio.h>
#include "msg.h"

#include "net/gcoap.h"
#include "net/gnrc/netif.h"
#include "kernel_types.h"
#include "shell.h"

#define MAIN_QUEUE_SIZE (4)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

extern int pause_gen(int argc, char **argv);
extern void gcoap_cli_init(void);

static const shell_command_t shell_commands[] = {
    { "pause", "Pause data gen", pause_gen },
    { NULL, NULL, NULL }
};

int main(void)
{
    /* for the thread running the shell */
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);

    /* _init_iface(); */
    gcoap_cli_init();

    /* start shell */
    puts("gCoAP OBSERVE test");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    /* should never be reached */
    return 0;
}
