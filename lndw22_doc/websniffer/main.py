#! /usr/bin/env python3
# -*- coding: utf-8 -*-
# vim:fenc=utf-8
#
# Copyright (C) 2022 Freie Universität Berlin
#
# Distributed under terms of the MIT license.

import argparse
import collections
import json
import logging
import os
import re
import sys
import tempfile

import aiofiles
import tornado.ioloop
import tornado.web
import tornado.websocket

from scapy.all import (
    Dot15d4,
    IPv6,
    DNS,
)
from scapy.all import load_contrib as scapy_load_contrib
from scapy.all import conf as scapy_conf
from scapy.contrib.coap import CoAP

try:
    from .sixlowpan import SixlowpanReassembly
    from .sniffer import TTYRIOTSniffer
except ImportError:
    from sixlowpan import SixlowpanReassembly
    from sniffer import TTYRIOTSniffer

sys.path.append(os.path.join(os.path.dirname(__file__), "..", "pythonlibs"))
import tornado_jinja2  # noqa: E402


class PktWebsocket(tornado.websocket.WebSocketHandler):
    def initialize(self, pktfile_name):
        self.pktfile_name = pktfile_name
        self.pktfile = None
        self.rb = None
        self.logger = logging.getLogger(type(self).__name__)
        self.logger.setLevel(logging.DEBUG)

    def _value_to_json_parsable(self, value):
        if value is not None and isinstance(value, (list, tuple)):
            res = []
            for member in value:
                res.append(self._value_to_json_parsable(member))
            return res
        elif value is not None and isinstance(value, bytes):
            return value.hex()
        elif value is not None and not isinstance(value, (str, int, float, dict)):
            return self._pkg_to_dict(value)
        else:
            return value

    def _pkg_to_dict(self, pkg):
        # Source: https://gist.github.com/cr0hn/1b0c2e672cd0721d3a07
        results = collections.defaultdict(dict)

        try:
            for lay in pkg.layers():
                layer = pkg[lay]

                # Get layer name
                layer_name = layer.name
                layer_name = re.sub("[^A-Za-z0-9_]+", "_", layer_name).lower()
                if layer_name.startswith("802_15_4"):
                    layer_name = layer_name.replace("802_15_4", "wpan")

                # Get the layer info
                tmp_t = {}
                for field in layer.fields_desc:
                    value = getattr(layer, field.name)
                    tmp_t[field.name] = self._value_to_json_parsable(value)
                results[layer_name] = tmp_t

                try:
                    tmp_t = {}
                    for x, y in layer.__dict__["fields"].items():
                        if y and not isinstance(y, (str, int, float, list, dict)):
                            tmp_t[x].update(self._pkg_to_dict(y))
                        else:
                            tmp_t[x] = y
                    results[layer_name] = tmp_t

                except KeyError:
                    # No custom fields
                    pass

        except IndexError:
            # Package finish -> do nothing
            pass

        return results

    def _try_send(self, time, pkg, fragments=None):
        if IPv6 in pkg and (DNS in pkg or CoAP in pkg):
            obj = {"time": time, "pkg": self._pkg_to_dict(pkg)}
            if fragments is not None:
                obj["fragments"] = [
                    {"time": time, "pkg": self._pkg_to_dict(fragment)}
                    for time, fragment in fragments
                ]
            self.write_message(json.dumps(obj))
            return True
        else:
            self.logger.debug("Not sending %f %r", time, pkg)
        return False

    async def send_pkts(self):
        while self.pktfile is not None:
            try:
                async for line in self.pktfile:
                    m = re.match(
                        r"^(?P<time>\d+\.\d+):(?P<length>\d+):(?P<data>\w+)$", line
                    )
                    if not m:
                        continue
                    pkg = Dot15d4(bytes.fromhex(m["data"])[: int(m["length"])])
                    if not self._try_send(float(m["time"]), pkg):
                        src, tag, res = self.rb.add(m["time"], pkg)
                        if res:
                            self._try_send(float(m["time"]), res, self.rb[src, tag])
                            self.rb.remove(src, tag)
            except (ValueError, tornado.websocket.WebSocketClosedError) as exc:
                self.logger.error(exc)

    async def close_pktfile(self):
        if self.pktfile is not None:
            await self.pktfile.close()
            self.pktfile = None

    async def open(self):
        if self.pktfile is None:
            self.pktfile = await aiofiles.open(self.pktfile_name)
        if self.rb is None:
            self.rb = SixlowpanReassembly()
        tornado.ioloop.IOLoop.current().asyncio_loop.create_task(self.send_pkts())

    def on_close(self):
        if self.rb is not None:
            self.rb.cancel()
            self.rb = None
        tornado.ioloop.IOLoop.current().asyncio_loop.create_task(self.close_pktfile())


class MainHandler(tornado_jinja2.BaseTemplateHandler):
    def get(self):
        data = {"host": self.request.host, "sniffer_ws": "/sniffer-ws"}
        return self.render2("base.html", **data)


def make_app(pktfile):
    return tornado.web.Application(
        [
            (r"/", MainHandler),
            (r"/sniffer-ws", PktWebsocket, {"pktfile_name": pktfile}),
        ],
        static_path=os.path.join(os.path.dirname(__file__), "static"),
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("device")
    parser.add_argument("-c", "--channel", type=int, nargs="?", default=26)
    parser.add_argument("-b", "--baudrate", type=int, nargs="?", default=500000)

    args = parser.parse_args()
    scapy_conf.dot15d4_protocol = "sixlowpan"
    scapy_load_contrib("coap")
    with tempfile.NamedTemporaryFile("w") as out:
        sniffer = TTYRIOTSniffer(args.device, args.channel, args.baudrate, outfile=out)
        sniffer.start()
        app = make_app(out.name)
        app.settings["template_path"] = "templates"
        app.settings["locale_path"] = "locales"
        app.listen(8889)
        tornado.ioloop.IOLoop.current().start()
