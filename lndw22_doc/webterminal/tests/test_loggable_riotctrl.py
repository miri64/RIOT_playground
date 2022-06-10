#! /usr/bin/env python3
# -*- coding: utf-8 -*-
# vim:fenc=utf-8
#
# Copyright (C) 2022 Freie Universität Berlin
#
# Distributed under terms of the MIT license.


from .. import loggable_riotctrl


def test_singleton():
    class TestClass(metaclass=loggable_riotctrl.Singleton):
        pass

    assert TestClass() is TestClass()
