# -*- coding: utf-8 -*-
# vim:fenc=utf-8
#
# Copyright (C) 2022 Freie Universität Berlin
#
# Distributed under terms of the MIT license.

import io
import os

import tornado.testing

from .. import main


class TestMain(tornado.testing.AsyncHTTPTestCase):
    def get_app(self):
        pktfile = io.StringIO()
        app = main.make_app(pktfile)
        app.settings["template_path"] = os.path.join(
            os.path.dirname(__file__), "..", "templates"
        )
        return app

    def test_riot_main(self):
        response = self.fetch("/")
        self.assertEqual(response.code, 200)
