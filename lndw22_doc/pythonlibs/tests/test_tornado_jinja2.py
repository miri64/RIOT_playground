# -*- coding: utf-8 -*-
# vim:fenc=utf-8
#
# Copyright (C) 2022 Freie Universität Berlin
#
# Distributed under terms of the MIT license.

import os

import jinja2
import pytest
import tornado.web
import tornado.testing

from .. import tornado_jinja2


def test_render_template():
    rendering = tornado_jinja2.TemplateRendering()
    rendering.settings = {
        "template_path": os.path.join(os.path.dirname(__file__), "tmpl")
    }
    res = rendering.render_template("base.html")
    assert res == "This is the base template"
    with pytest.raises(jinja2.TemplateNotFound):
        res = rendering.render_template("foobar.html")
    rendering.settings["locale_path"] = "foobar"
    res = rendering.render_template("base.html")
    del rendering.settings["template_path"]
    with pytest.raises(jinja2.TemplateNotFound):
        res = rendering.render_template("base.html")


class TestHandler(tornado_jinja2.BaseTemplateHandler):
    def get(self):
        return self.render2("base.html")


class TestBaseTemplateHandler(tornado.testing.AsyncHTTPTestCase):
    def get_app(self):
        app = tornado.web.Application(
            [
                (r"/", TestHandler),
            ]
        )
        app.settings["template_path"] = os.path.join(os.path.dirname(__file__), "tmpl")
        return app

    def test_render2(self):
        assert self.fetch("/").body == b"This is the base template"
