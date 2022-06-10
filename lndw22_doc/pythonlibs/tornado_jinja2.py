#! /usr/bin/env python3
# -*- coding: utf-8 -*-
# vim:fenc=utf-8
#
# Copyright (C) 2022 Freie Universität Berlin
#
# Distributed under terms of the MIT license.

import jinja2
import tornado


class TemplateRendering:
    """
    A simple class to hold methods for rendering templates.
    """

    def render_template(self, template_name, **kwargs):
        template_dirs = []
        if self.settings.get("template_path", ""):
            template_dirs.append(self.settings["template_path"])

        env = jinja2.Environment(loader=jinja2.FileSystemLoader(template_dirs))

        try:
            template = env.get_template(template_name)
        except jinja2.TemplateNotFound:
            raise jinja2.TemplateNotFound(template_name)
        content = template.render(kwargs)
        return content


class BaseTemplateHandler(tornado.web.RequestHandler, TemplateRendering):
    """
    RequestHandler already has a `render()` method. I'm writing another
    method `render2()` and keeping the API almost same.
    """

    def render2(self, template_name, **kwargs):
        """
        This is for making some extra context variables available to
        the template
        """
        kwargs.update(
            {
                "settings": self.settings,
                "STATIC_URL": self.settings.get("static_url_prefix", "/static/"),
                "static_url": self.static_url,
                "request": self.request,
                "xsrf_token": self.xsrf_token,
                "xsrf_form_html": self.xsrf_form_html,
            }
        )
        content = self.render_template(template_name, **kwargs)
        self.write(content)


