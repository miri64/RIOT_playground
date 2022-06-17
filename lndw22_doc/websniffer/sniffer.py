#! /usr/bin/env python3
# -*- coding: utf-8 -*-
# vim:fenc=utf-8
#
# Copyright (C) 2022 Freie Universität Berlin
#
# Distributed under terms of the MIT license.

import abc
import logging
import re
import sys
import threading
import time

import serial


class AbstractRIOTSniffer(abc.ABC):
    @abc.abstractproperty
    @property
    def channel(self):
        raise NotImplementedError()  # pragma: no cover

    @abc.abstractproperty
    @property
    def logger(self):
        raise NotImplementedError()  # pragma: no cover

    @abc.abstractproperty
    @property
    def port(self):
        raise NotImplementedError()  # pragma: no cover

    @abc.abstractproperty
    @property
    def out(self):
        raise NotImplementedError()  # pragma: no cover

    @abc.abstractmethod
    def connect(self):
        raise NotImplementedError()  # pragma: no cover

    def config(self):
        self.port.write("ifconfig\n".encode())
        while True:
            line = self.port.readline()
            if line == b"":
                raise AssertionError("Application has no network interface defined")
            match = re.search(r"^Iface +(\d+)", line.decode(errors="ignore"))
            if match is not None:
                iface = int(match.group(1))
                break

        # set channel, raw mode, and promiscuous mode
        for cmd in [
            f"ifconfig {iface} set chan {self.channel}",
            f"ifconfig {iface} raw",
            f"ifconfig {iface} promisc",
        ]:
            self.logger.info(cmd)
            self.port.write(f"{cmd}\n".encode())
            line = ""
            while not line.startswith("> "):
                line = self.port.readline().decode()
                self.logger.info(line)

    def generate_outfile(self):
        # count incoming packets
        count = 0
        print(f"RX: {count}", file=sys.stderr, end="\r")
        length = 0
        while True:
            line = self.port.readline().rstrip()

            pkt_header = re.match(
                r">? *rftest-rx --- len (\w+).*", line.decode(errors="ignore")
            )
            if pkt_header:
                now = time.time()
                length = int(pkt_header.group(1), 16)
                self.out.write(f"{now}:{length}:")
                count += 1
                print(f"RX: {count}", file=sys.stderr, end="\r")
                continue
            pkt_data = re.match(r"(\w\w )+", line.decode(errors="ignore"))
            if pkt_data:
                for part in line.decode(errors="ignore").split(" "):
                    byte = re.match(r"(\w\w)", part)
                    if byte:
                        self.out.write(byte.group(1))
                        length -= 1
                if length <= 0:
                    self.out.write("\n")
                    length = 0
            self.out.flush()


class TTYRIOTSniffer(threading.Thread, AbstractRIOTSniffer):
    def __init__(self, device, channel=26, baudrate=500000, outfile=sys.stdout):
        super().__init__()
        self._device = device
        self._channel = channel
        self._baudrate = baudrate
        self._outfile = outfile
        self._port = None
        self._time_offset = None
        self._logger = logging.getLogger(type(self).__name__)
        self._init_logger()

    @property
    def baudrate(self):
        return self._baudrate

    @property
    def channel(self):
        return self._channel

    @property
    def port(self):
        if self._port is None:
            raise ConnectionError("{self._device} is not connected")
        return self._port

    @property
    def logger(self):
        return self._logger

    @property
    def out(self):
        return self._outfile

    def _init_logger(self):
        self._logger.setLevel(logging.INFO)

    def connect(self):
        self._port = serial.Serial(
            self._device, self._baudrate, dsrdtr=0, rtscts=0, timeout=1
        )

    def run(self, *args, **kwargs):
        while True:
            try:
                self.connect()
                self.config()
                self.generate_outfile()
                return
            except serial.SerialException as exc:
                self.port.close()
                self.logger.error(exc)
                time.sleep(5)
