#! /usr/bin/env python3
# -*- coding: utf-8 -*-
# vim:fenc=utf-8
#
# Copyright (C) 2022 Freie Universität Berlin
#
# Distributed under terms of the MIT license.

import riotctrl.ctrl
import sys
import time


class Singleton(type):
    _instances = {}

    def __call__(cls, *args, **kwargs):
        if cls not in cls._instances:
            cls._instances[cls] = super(Singleton, cls).__call__(*args, **kwargs)
        return cls._instances[cls]


class RIOTCtrl(riotctrl.ctrl.RIOTCtrl, metaclass=Singleton):
    def __init__(self, application_directory, env=None, logfile=None):
        self._logfile = logfile
        super().__init__(application_directory, env)

    @property
    def logfile(self):
        return self._logfile

    def start_term(self, **spawnkwargs):
        if self._logfile is None:
            return super().start_term(**spawnkwargs)
        self.stop_term()

        term_cmd = self.make_command(self.TERM_TARGETS)
        cmd = f"{' '.join(term_cmd)} 2>&1 | tee -a {self._logfile}"
        self.term = self.TERM_SPAWN_CLASS(
            "bash", args=["-c", cmd], env=self.env, **spawnkwargs
        )
        self.term.logfile = sys.stdout

        # on many platforms, the termprog needs a short while to be ready
        time.sleep(self.TERM_STARTED_DELAY)
