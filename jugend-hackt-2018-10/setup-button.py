#! /usr/bin/env python3
# -*- coding: utf-8 -*-
# vim:fenc=utf-8
#
# Copyright © 2018 Martine Lenders <m.lenders@fu-berlin.de>
#
# Distributed under terms of the MIT license.

import json
import re
import subprocess
import sys

RD_SERVER_ADDRESS = "[2001:db8:0:1::1]"

if __name__ == "__main__":
    resources = subprocess.run("coap-client coap://%s/resource-lookup" %
                               RD_SERVER_ADDRESS, shell=True,
                               capture_output=True, text=True).stdout
    resources = resources.strip()
    resources = resources.split(",")
    dino_move_target = {}
    button_target_url = None
    for resource in resources:
        if "/dino/move" in resource:
            m = re.search("<coap://([^/]+)(/.*)>", resource)
            if m is None:
                print("Error: /dino/move resource can't be parsed")
                break
            dino_move_target["addr"] = m.group(1)
            dino_move_target["path"] = m.group(2)
        elif "/button/target" in resource:
            m = re.search("<(coap://.+)>", resource)
            if m is None:
                print("Error: /dino/move resource can't be parsed")
                break
            button_target_url = m.group(1)
    if "addr" not in dino_move_target or button_target_url is None:
        print("Error: not all resources were found")
        sys.exit(1)
    print("coap-client %s -m post -t application/json -e '%s'" %
          (button_target_url, json.dumps(dino_move_target)))
    subprocess.run("coap-client %s -m post -t application/json -e '%s'" %
                   (button_target_url, json.dumps(dino_move_target)),
                   shell=True)
