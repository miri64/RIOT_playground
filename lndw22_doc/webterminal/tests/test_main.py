# -*- coding: utf-8 -*-
# vim:fenc=utf-8
#
# Copyright (C) 2022 Freie Universität Berlin
#
# Distributed under terms of the MIT license.

import os

import tornado.testing
import tornado.websocket

from .. import main


class MockCtrl:  # pytest: disable=too-few-public-methods
    pass


class TestMain(tornado.testing.AsyncHTTPTestCase):
    def get_app(self):
        app = main.make_app(
            ctrl=MockCtrl,
            userctx={
                "alg_num": "b5",
                "common_iv": "221c386f56a8cef818ba3a5bea",
                "sender_id": "ef",
                "sender_key": "ae88a3f5e7ebb37aca3b5e4244afe4ea",
                "recipient_id": "9b",
                "recipient_key": "125d2575f0495dc66bb3d93057cf67fa",
            },
            server_addr="2001:db8:2b47:4ddc::1",
        )
        app.settings["template_path"] = os.path.join(
            os.path.dirname(__file__), "..", "templates"
        )
        return app

    def test_riot_main(self):
        response = self.fetch("/")
        self.assertEqual(response.code, 200)

    def test_riot_term(self):
        response = self.fetch("/term")
        self.assertEqual(response.code, 200)
