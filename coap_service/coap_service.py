#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# vim:fenc=utf-8
#
# Copyright (C) 2019 Freie Universitaet Berlin
# This file is subject to the terms and conditions of the GNU Lesser
# General Public License v2.1. See the file LICENSE in the top level
# directory for more details.

import argparse
import asyncio
import json
import logging
import os
import socket
import sys
from tornado.log import app_log
import tornado.ioloop
import tornado.web
import tornado.websocket
import urllib.parse

from aiocoap import *
from aiocoap.message import coap_schemes
from aiocoap.numbers import media_types, media_types_rev
from aiocoap.util import linkformat


logging.basicConfig(level=logging.INFO)

coap_client = None

async def get_coap_client():
    global coap_client

    if coap_client is None:
        coap_client = await Context.create_client_context()
    return coap_client


async def shutdown_coap_client():
    global coap_client

    try:
        await coap_client.shutdown()
    except:
        pass
    finally:
        coap_client = None


def link_to_json(link):
    return json.dumps(
            {k: v for k,v in link.items() if k in ["addr", "path"]}
        ).encode("utf-8")


async def register(config_node, target_node):
    coap_client = await get_coap_client()
    request = Message(code=POST, mtype=CON)
    request.set_request_uri(config_node["target"]["href"])
    request.payload = link_to_json(target_node["points"])
    request.opt.content_format = media_types_rev["application/json"]
    pr = coap_client.request(request)
    resp = await pr.response
    logging.info("Tried to register {} with {}: {}".format(
            target_node["points"]["href"],
            config_node["target"]["href"],
            resp))


def incoming_observation(response, websocket):
    links = {}
    nodes = {}

    logging.info("Received response {}".format(response))
    if response.code.is_successful():
        cf = response.opt.content_format
        mime_type = media_types.get(cf, "type %s" % cf)
        mime_type, *parameters = mime_type.split(";")
        logging.info("Content format: {}".format(mime_type))
        if mime_type == "application/link-format":
            ls = linkformat.parse(response.payload.decode('utf-8'))
            for link in ls.links:
                link = link.as_json_data()
                pr = urllib.parse.urlparse(link["href"])
                link["addr"] = pr.netloc
                link["path"] = pr.path
                links[link["href"]] = link
    else:
        logging.error("Error: {}: {}".format(response, response.payload))
        return
    if websocket is not None and not websocket.is_closed():
        websocket.write_message(response.payload)
    elif websocket is None:
        for link in links.values():
            if link["addr"].startswith("[fe80::"):
                logging.error("address {} link-local".format(link["addr"]))
                continue
            path = link["path"]
            node, *rest = path.strip("/").split("/")
            if (len(rest) > 0) and (node not in nodes):
                nodes[node] = {rest[0]: link}
            elif (len(rest) > 0):
                nodes[node][rest[0]] = link
        if "btn" in nodes and "target" in nodes["btn"] and \
           "dsp" in nodes and "points" in nodes["dsp"]:
            asyncio.ensure_future(register(nodes["btn"], nodes["dsp"]))
        if "dsp" in nodes and "target" in nodes["dsp"] and \
           "dino" in nodes and "points" in nodes["dino"]:
            asyncio.ensure_future(register(nodes["dsp"], nodes["dino"]))



async def observe(resource, websocket=None):
    coap_client = await get_coap_client()
    if (websocket is not None) and websocket.is_closed():
        # if websocket was closed during awaiting client
        return
    request = Message(code=GET, uri=resource, observe=0)

    try:
        pr = coap_client.request(request)
        observation_is_over = asyncio.Future()

        pr.observation.register_errback(observation_is_over.set_result)
        pr.observation.register_callback(
            lambda data, ws=websocket: incoming_observation(data, ws))
        logging.info("waiting for OBSERVE response")
        if websocket is not None:
            websocket.observation = pr.observation
        resp = await pr.response
        incoming_observation(resp, websocket)
        exit_reason = await observation_is_over
        logging.info("reason to exit observation: '{}'".format(exit_reason))
    finally:
        if not pr.observation.cancelled:
            pr.observation.cancel()
            await asyncio.sleep(50)
        if websocket is not None:
            websocket.close()


class CoapRequestHandler(tornado.web.RequestHandler):
    METHODS = {
        "GET": GET,
        "HEAD": GET,
        "POST": POST,
    }

    async def _common(self):
        coap_client = await get_coap_client()
        target = self.get_query_argument("target")
        p = urllib.parse.urlparse(target)
        if p.scheme not in coap_schemes:
            raise tornado.web.HTTPError(400)
        assert self.request.method in CoapRequestHandler.METHODS
        request = Message(code=CoapRequestHandler.METHODS[self.request.method],
                          uri=target)
        try:
            if "Content-Type" in self.request.headers:
                request.opt.content_format = \
                        media_types_rev[self.request.headers["Content-Type"]]
        except KeyError:
            raise tornado.web.HTTPError(400)
        request.payload = self.request.body
        pr = coap_client.request(request)
        app_log.info("waiting for {} response".format(self.request.method))
        try:
            resp = await pr.response
        except socket.gaierror as e:
            self.log_exception(*sys.exc_info())
            raise tornado.web.HTTPError(400, str(e))
        cf = resp.opt.content_format
        mime_type = media_types.get(cf, "type %s" % cf)
        mime_type, *parameters = mime_type.split(";")
        self.content_type = mime_type
        status_class, status_detail = divmod(resp.code, 0x20)
        status = ((status_class * 100) + status_detail)
        if resp.code.is_successful():
            self.set_status(status, resp.code.name_printable)
            app_log.info("Success {}, {}".format(resp, resp.payload))
            if not self.request.method in ["HEAD"]:
                self.write(resp.payload)
        else:
            app_log.error("Error {}, {}".format(resp, resp.payload))
            raise tornado.web.HTTPError(status, resp.payload,
                                        reason=resp.code.name_printable)


    async def get(self):
        return await self._common()

    async def head(self):
        return await self._common()

    async def post(self):
        return await self._common()


class CoapObserveHandler(tornado.websocket.WebSocketHandler):
    websockets = set()
    keeper = None

    @staticmethod
    def keep_alive():
        for websocket in CoapObserveHandler.websockets:
            websocket.ping()

    def is_closed(self):
        return self not in CoapObserveHandler.websockets

    def check_origin(self, origin):
        app_log.info("origin: {}".format(origin))
        return origin.startswith("file://") or \
               origin.startswith("http://localhost")

    async def get(self):
        self.target = self.get_query_argument("target")
        p = urllib.parse.urlparse(self.target)
        if p.scheme not in coap_schemes:
            raise tornado.web.HTTPError(400)
        return await super(CoapObserveHandler, self).get()

    async def open(self):
        if not CoapObserveHandler.websockets:
            CoapObserveHandler.keeper = tornado.ioloop.PeriodicCallback(
                    CoapObserveHandler.keep_alive, 60 * 1000
                )
            CoapObserveHandler.keeper.start()
        CoapObserveHandler.websockets.add(self)
        self.observation = None
        event_loop = tornado.ioloop.IOLoop.current()
        event_loop.spawn_callback(observe, self.target, self)
        app_log.info("Websocket to {} opened".format(self.target))

    async def on_message(self):
        pass

    async def cancel_observation(self):
        if (self.observation is not None) and (not self.observation.cancelled):
            self.observation.cancel()

    def on_close(self):
        app_log.info("Websocket to {} closed".format(self.target))
        CoapObserveHandler.websockets.discard(self)
        if not CoapObserveHandler.websockets:
            CoapObserveHandler.keeper.stop()
        event_loop = tornado.ioloop.IOLoop.current()
        event_loop.spawn_callback(self.cancel_observation)


async def main(corerd_addr, www_dir, http_port=5656, *args, **kwargs):
    corerd_anchor = 'coap://[{addr}]'.format(addr=corerd_addr)
    corerd = '{anchor}/resource-lookup'.format(anchor=corerd_anchor)
    with open(os.path.join(www_dir, "coap_service.json"), "w") as json_file:
        json.dump({
            "corerd": {"url": corerd, "anchor": corerd_anchor},
            "coap_service": "localhost:{}".format(http_port),
        }, json_file)
    await observe(corerd)
    # error might have been caused by client so better shut down
    await shutdown_coap_client()
    # restart main
    event_loop = tornado.ioloop.IOLoop.current()
    event_loop.spawn_callback(main, corerd_addr, *args, **kwargs)


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("corerd_addr")
    p.add_argument("www_dir")
    p.add_argument("http_port", type=int, default=5656, nargs="?")
    args = p.parse_args()
    application = tornado.web.Application([
        (r"/coap", CoapRequestHandler),
        (r"/coap_observe", CoapObserveHandler),
    ])
    application.listen(args.http_port)
    event_loop = tornado.ioloop.IOLoop.current()
    event_loop.spawn_callback(main, **vars(args))
    event_loop.start()
