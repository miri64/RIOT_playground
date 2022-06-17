#! /usr/bin/env python3
# -*- coding: utf-8 -*-
# vim:fenc=utf-8
#
# Copyright (C) 2022 Freie Universität Berlin
#
# Distributed under terms of the MIT license.

import io

import pytest
import serial

from .. import sniffer


def test_tty_sniffer_init():
    outfile = io.StringIO()
    obj = sniffer.TTYRIOTSniffer(
        "/dev/ttyUSB1", channel=13, baudrate=12345, outfile=outfile
    )
    assert obj.baudrate == 12345
    with pytest.raises(ConnectionError):
        obj.port
    assert obj.out is outfile


def test_tty_sniffer_connect(mocker):
    outfile = io.StringIO()
    port = mocker.patch("serial.Serial")
    obj = sniffer.TTYRIOTSniffer(
        "/dev/ttyUSB1", channel=13, baudrate=12345, outfile=outfile
    )
    obj.connect()
    port.assert_called_once()
    assert obj.port == port()


def test_tty_sniffer_config(mocker):
    outfile = io.StringIO()
    port = mocker.patch("serial.Serial")
    handle = mocker.MagicMock()
    handle.readline.side_effect = [b"foobar", b"Iface   38774"] + (3 * [b"> "])
    port.return_value = handle
    obj = sniffer.TTYRIOTSniffer(
        "/dev/ttyUSB1", channel=13, baudrate=12345, outfile=outfile
    )
    obj.connect()
    obj.config()
    print(port.write.args)
    handle.readline.side_effect = None
    handle.readline.return_value = b""
    with pytest.raises(AssertionError):
        obj.config()


def test_tty_sniffer_generate_outfile(mocker):
    outfile = io.StringIO()
    port = mocker.patch("serial.Serial")
    mocker.patch("time.time", return_value=12345.6789)
    handle = mocker.MagicMock()
    handle.readline.side_effect = [
        b"rftest-rx --- len 04\n",
        b"",
        b"01 .. 02 03 04\n",
        b"rftest-rx --- len 03\n",
        b"05 06\n",
        b"07 08\n",
        serial.SerialException,
    ]
    port.return_value = handle
    obj = sniffer.TTYRIOTSniffer(
        "/dev/ttyUSB1", channel=13, baudrate=12345, outfile=outfile
    )
    obj.connect()
    try:
        obj.generate_outfile()
    except serial.SerialException:
        pass
    assert outfile.getvalue() == "12345.6789:4:01020304\n12345.6789:3:05060708\n"


def test_tty_sniffer_run1(mocker):
    outfile = io.StringIO()
    mocker.patch.object(sniffer.TTYRIOTSniffer, "connect")
    mocker.patch.object(sniffer.TTYRIOTSniffer, "config")
    generate_outfile = mocker.patch.object(sniffer.TTYRIOTSniffer, "generate_outfile")
    obj = sniffer.TTYRIOTSniffer(
        "/dev/ttyUSB1", channel=13, baudrate=12345, outfile=outfile
    )
    obj.run()
    generate_outfile.assert_called_once()


def test_tty_sniffer_run2(mocker):
    outfile = io.StringIO()
    mocker.patch("serial.Serial")
    mocker.patch("time.sleep")
    mocker.patch.object(
        sniffer.TTYRIOTSniffer, "config", side_effect=[serial.SerialException, None]
    )
    generate_outfile = mocker.patch.object(sniffer.TTYRIOTSniffer, "generate_outfile")
    obj = sniffer.TTYRIOTSniffer(
        "/dev/ttyUSB1", channel=13, baudrate=12345, outfile=outfile
    )
    obj.run()
    generate_outfile.assert_called_once()
