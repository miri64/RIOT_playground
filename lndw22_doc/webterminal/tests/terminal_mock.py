#! /usr/bin/env python3
# -*- coding: utf-8 -*-
# vim:fenc=utf-8
#
# Copyright (C) 2022 Freie Universität Berlin
#
# Distributed under terms of the MIT license.

import sys


if __name__ == "__main__":
    print("This goes to stdout")
    print("This goes to stderr", file=sys.stderr)
