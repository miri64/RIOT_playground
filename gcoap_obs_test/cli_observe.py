#!env/bin/python3
# -*- coding: utf-8 -*-

# Copyright (C) 2017 Freie Universität Berlin
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# Small tool that parses pre-defined events printed by RIOT nodes to STDIO and
# forwards them to a central data sink for logging
#
# Author:   Hauke Petersen <hauke.petersen@fu-berlin.de>

import argparse
import asyncio
from aiocoap import Context, Message, GET, NON
import random


TIMEOUT = 2


async def subscribe(addr, timeout):
    global successful_observations
    uri = "coap://{}/test".format(addr)
    await asyncio.sleep(random.randint(0, 1000) / 1000)
    protocol = await Context.create_client_context()
    while True:
        try:
            print("OBSERVE request to '{}'".format(uri))
            request = Message(mtype=NON, code=GET, uri=uri, observe=0)
            pr = protocol.request(request)
            r = await asyncio.wait_for(pr.response, timeout=TIMEOUT)
            print("First result for %s: %s\n\t%r" % (uri, r.code, r.payload))
            # don't use for loop to iteratie over observation, to be able to
            # timeout notifications
            iterator = pr.observation.__aiter__()
            while True:
                r = await asyncio.wait_for(iterator.__anext__(),
                                           timeout=TIMEOUT)
                print("Next result for %s: %s\n\t%r" %
                      (uri, r.code, r.payload))
        except asyncio.TimeoutError:
            continue
        except asyncio.CancelledError:
            continue


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("addrs", default="[::1]:5683", nargs="+",
                   help="IPv6 address of target node")
    p.add_argument("interval", default="1", nargs="?",
                   help="Request interval")
    p.add_argument("-c", "--confirmable", action="store_true",
                   help="Send CoAP message as confirmable")
    p.add_argument("-t", "--timeout", default=1,
                   help="Timeout for follow-up response")
    args = p.parse_args()
    tasks = map(lambda addr: subscribe(addr, args.timeout), args.addrs)
    try:
        super_task = asyncio.gather(*tasks)
        asyncio.get_event_loop().run_until_complete(super_task)
    except KeyboardInterrupt:
        super_task.cancel()
