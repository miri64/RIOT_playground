#! /usr/bin/env python3
# -*- coding: utf-8 -*-
# vim:fenc=utf-8
#
# Copyright (C) 2022 Freie Universität Berlin
#
# Distributed under terms of the MIT license.

import argparse
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
    def initialize(self, ctrl):
        self.ctrl = ctrl
        self.shell = riotctrl.shell.ShellInteraction(ctrl)
        self.logfile = None

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
        tornado.ioloop.IOLoop.current().asyncio_loop.create_task(self.send_lines())

    def on_message(self, message):
        try:
            self.write_message(json.dumps({"shell_reply": self.shell.cmd(message)}))
        except pexpect.TIMEOUT as exc:
            self.ctrl.reset()
            self.shell = riotctrl.shell.ShellInteraction(ctrl)
            logging.error(exc)

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


def make_app(ctrl):
    return tornado.web.Application(
        [
            (r"/", MainHandler),
            (r"/riotshell-ws", RIOTShellWebsocket, {"ctrl": ctrl}),
            (r"/term", TerminalHandler),
        ],
        static_path=os.path.join(os.path.dirname(__file__), "static"),
    )


def config_node(ctrl, server_address, userctx):
    shell = riotctrl.shell.ShellInteraction(ctrl)
    ctrl.reset()
    shell.cmd(f"uri coap://[{server_address}]/dns")
    shell.cmd(f"server {server_address}")
    shell.cmd(f"userctx -i {userctx['alg_num']} {userctx['common_iv']}")
    shell.cmd(f"userctx -s {userctx['sender_id']} {userctx['sender_key']}")
    shell.cmd(f"userctx -r {userctx['recipient_id']} {userctx['recipient_key']}")
    shell.cmd("userctx -c")


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
                config_node(
                    ctrl,
                    "2001:db8:1::1",
                    {
                        "alg_num": "10",
                        "common_iv": "85694ff94440dc9bc07cd15ce3",
                        "sender_id": "11",
                        "sender_key": "4b2e72cc1fbc599eae2d8598041ef84a",
                        "recipient_id": "01",
                        "recipient_key": "e7bf5cfe6339302a7aae30caf46f6a38",
                    },
                )
                app = make_app(ctrl)
                app.settings["template_path"] = os.path.join(
                    os.path.dirname(__file__), "templates"
                )
                app.listen(8888)
                tornado.ioloop.IOLoop.current().start()
        finally:
            del ctrl
