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
import socket
import sys
from tornado.log import app_log
import tornado.ioloop
import tornado.web
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


def incoming_observation(response):
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
        logging.error("")
        return
    for link in links.values():
        if link["addr"].startswith("[fe80::"):
            logging.error("address {} link-local".format(link["addr"]))
            continue
        path = link["path"]
        node, sense, *rest = path.strip("/").split("/")
        if node not in nodes:
            nodes[node] = {sense: link}
        else:
            nodes[node][sense] = link
    if "btn" in nodes and "target" in nodes["btn"] and \
       "dsp" in nodes and "points" in nodes["dsp"]:
        asyncio.ensure_future(register(nodes["btn"], nodes["dsp"]))
    if "dsp" in nodes and "target" in nodes["dsp"] and \
       "dino" in nodes and "points" in nodes["dino"]:
        asyncio.ensure_future(register(nodes["dsp"], nodes["dino"]))



async def observe(resource):
    coap_client = await get_coap_client()
    request = Message(code=GET, uri=resource, observe=0)

    try:
        pr = coap_client.request(request)
        observation_is_over = asyncio.Future()

        pr.observation.register_errback(observation_is_over.set_result)
        pr.observation.register_callback(incoming_observation)
        logging.info("waiting for OBSERVE response")
        resp = await pr.response
        incoming_observation(resp)
        exit_reason = await observation_is_over
        logging.info(exit_reason)
    finally:
        if not pr.observation.cancelled:
            pr.observation.cancel()
            await asyncio.sleep(50)


class CoapRequestHandler(tornado.web.RequestHandler):
    METHODS = {
        "GET": GET,
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
            self.write(resp.payload)
        else:
            app_log.error("Error {}, {}".format(resp, resp.payload))
            raise tornado.web.HTTPError(status, resp.payload,
                                        reason=resp.code.name_printable)


    async def get(self):
        return await self._common()

    async def post(self):
        return await self._common()


async def main(corerd_addr, *args, **kwargs):
    await observe('coap://[{addr}]/resource-lookup'.format(addr=corerd_addr))
    # error might have been caused by client so better shut down
    await shutdown_coap_client()
    # restart main
    event_loop = tornado.ioloop.IOLoop.current()
    event_loop.spawn_callback(main, corerd_addr, *args, **kwargs)


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("corerd_addr")
    p.add_argument("http_port", type=int, default=5656, nargs="?")
    args = p.parse_args()
    application = tornado.web.Application([
        (r"/coap", CoapRequestHandler)
    ])
    application.listen(args.http_port)
    event_loop = tornado.ioloop.IOLoop.current()
    event_loop.spawn_callback(main, **vars(args))
    event_loop.start()
