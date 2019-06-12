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
import tornado.ioloop
import urllib.parse

from aiocoap import *
from aiocoap.numbers import media_types
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
    request.opt.content_format = 50 # json
    pr = coap_client.request(request)
    resp = await pr.response
    logging.info("Tried to register {} with {}: {}".format(
            target_node["points"]["href"],
            config_node["target"]["href"],
            resp))


def incoming_observation(response):
    links = {}
    nodes = {}

    if response.code.is_successful():
        cf = response.opt.content_format
        mime_type = media_types.get(cf, "type %s" % cf)
        mime_type, *parameters = mime_type.split(";")
        if mime_type == "application/link-format":
            ls = linkformat.parse(response.payload.decode('utf-8'))
            for link in ls.links:
                link = link.as_json_data()
                pr = urllib.parse.urlparse(link["href"])
                link["addr"] = pr.netloc
                link["path"] = pr.path
                links[link["href"]] = link
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


async def main(corerd_addr):
    await observe('coap://[{addr}]/resource-lookup'.format(addr=corerd_addr))
    # error might have been caused by client so better shut down
    await shutdown_coap_client()
    # restart main
    event_loop = tornado.ioloop.IOLoop.current()
    event_loop.spawn_callback(main, corerd_addr)


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("corerd_addr")
    args = p.parse_args()
    event_loop = tornado.ioloop.IOLoop.current()
    event_loop.spawn_callback(main, **vars(args))
    event_loop.start()
