/*
 * Copyright (C) 2008, 2009, 2010  Kaspar Schleiser <kaspar@schleiser.de>
 * Copyright (C) 2013 INRIA
 * Copyright (C) 2013 Ludwig Ortmann <ludwig.ortmann@fu-berlin.de>
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
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 * @author      Oliver Hahm <oliver.hahm@inria.fr>
 * @author      Ludwig Ortmann <ludwig.ortmann@fu-berlin.de>
 *
 * @}
 */

#include <stdio.h>

#include "board_uart0.h"
#include "cpu-conf.h"
#include "inet_ntop.h"
#include "posix_io.h"
#include "shell.h"
#include "shell_commands.h"

static void send(int argc, char **argv) {
    if (argc < 2) {
        puts("Send n bytes to dest\n");
        puts("Usage: send <dest> <n>");
    }
}

static const shell_command_t shell_command[] = {
    {"send", "Send n bytes to dest", send},
    {NULL, NULL, NULL},
};

int main(void)
{
    shell_t shell;
    (void) posix_open(uart0_handler_pid, 0);

    (void) puts("Welcome to RIOT!");

    shell_init(&shell, shell_command, UART0_BUFSIZE, uart0_readc, uart0_putc);

    shell_run(&shell);
    return 0;
}
