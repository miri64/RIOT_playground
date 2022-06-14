#! /usr/bin/env python3
# -*- coding: utf-8 -*-
# vim:fenc=utf-8
#
# Copyright (C) 2022 Freie Universität Berlin
#
# Distributed under terms of the MIT license.

import argparse
import asyncio
import json
import logging
import os
import sys
import tempfile

import aiofiles
import pexpect
import riotctrl.shell
import tornado.ioloop
import tornado.web
import tornado.websocket

sys.path.append(os.path.join(os.path.dirname(__file__), "..", "pythonlibs"))
import tornado_jinja2  # noqa: E402

try:
    from .loggable_riotctrl import RIOTCtrl
except ImportError:
    from loggable_riotctrl import RIOTCtrl


class RIOTShellWebsocket(tornado.websocket.WebSocketHandler):
    def initialize(self, ctrl, server_addr, userctx):
        self.ctrl = ctrl
        self.server_addr = server_addr
        self.userctx = userctx
        self.shell = None
        self.logfile = None

    async def config_node(self):
        while True:
            try:
                self.shell = riotctrl.shell.ShellInteraction(self.ctrl)
                self.ctrl.reset()
                self.shell.cmd(f"uri coap://[{self.server_addr}]/dns")
                self.shell.cmd(f"server {self.server_addr}")
                self.shell.cmd(
                    f"userctx -i {self.userctx['alg_num']} {self.userctx['common_iv']}"
                )
                self.shell.cmd(
                    f"userctx -s {self.userctx['sender_id']} "
                    f"{self.userctx['sender_key']}"
                )
                self.shell.cmd(
                    f"userctx -r {self.userctx['recipient_id']} "
                    f"{self.userctx['recipient_key']}"
                )
                self.shell.cmd("userctx -c")
                return
            except (pexpect.TIMEOUT, pexpect.EOF) as exc:
                logging.error(exc)
                await asyncio.sleep(3)
                self.ctrl.start_term()

    async def send_lines(self):
        while self.logfile is not None:
            try:
                async for line in self.logfile:
                    self.write_message(json.dumps({"line": line}))
            except (ValueError, tornado.websocket.WebSocketClosedError) as exc:
                logging.info(exc)

    async def close_logfile(self):
        if self.logfile is not None:
            await self.logfile.close()
            self.logfile = None

    async def open(self):
        if self.logfile is None:
            self.logfile = await aiofiles.open(self.ctrl.logfile)
        if self.shell is None:
            await self.config_node()
        self.shell.cmd("switch")
        tornado.ioloop.IOLoop.current().asyncio_loop.create_task(self.send_lines())

    async def on_message(self, message):
        try:
            await self.write_message(
                json.dumps({"shell_reply": self.shell.cmd(message)})
            )
        except (pexpect.TIMEOUT, pexpect.EOF) as exc:
            logging.error(exc)
            await self.config_node()
            self.shell.cmd("switch")

    def on_close(self):
        tornado.ioloop.IOLoop.current().asyncio_loop.create_task(self.close_logfile())


class MainHandler(tornado_jinja2.BaseTemplateHandler):
    def get(self):
        data = {"host": self.request.host, "riotctrl_ws": "/riotshell-ws"}
        return self.render2("base.html", **data)


class TerminalHandler(tornado_jinja2.BaseTemplateHandler):
    def get(self):
        data = {"host": self.request.host, "riotctrl_ws": "/riotshell-ws"}
        return self.render2("terminal.html", **data)


def make_app(ctrl, server_addr, userctx):
    return tornado.web.Application(
        [
            (r"/", MainHandler),
            (
                r"/riotshell-ws",
                RIOTShellWebsocket,
                {"ctrl": ctrl, "server_addr": server_addr, "userctx": userctx},
            ),
            (r"/term", TerminalHandler),
        ],
        static_path=os.path.join(os.path.dirname(__file__), "static"),
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("application")
    parser.add_argument("-b", "--board", type=str, nargs="?")
    parser.add_argument("-s", "--serial", type=str, nargs="?")
    parser.add_argument("-p", "--port", type=str, nargs="?")

    args = parser.parse_args()
    with tempfile.NamedTemporaryFile("w") as logfile:
        try:
            env = {}
            if args.board:
                env["BOARD"] = args.board
            if args.serial:
                env["DEBUG_ADAPTER_ID"] = args.serial
            if args.port:
                env["PORT"] = args.port
            ctrl = RIOTCtrl(args.application, logfile=logfile.name, env=env)
            with ctrl.run_term():
                server_addr = "2001:db8:1::1"
                userctx = {
                    "alg_num": "10",
                    "common_iv": "85694ff94440dc9bc07cd15ce3",
                    "sender_id": "11",
                    "sender_key": "4b2e72cc1fbc599eae2d8598041ef84a",
                    "recipient_id": "01",
                    "recipient_key": "e7bf5cfe6339302a7aae30caf46f6a38",
                }
                app = make_app(ctrl, server_addr, userctx)
                app.settings["template_path"] = os.path.join(
                    os.path.dirname(__file__), "templates"
                )
                app.listen(8888)
                tornado.ioloop.IOLoop.current().start()
        finally:
            del ctrl
