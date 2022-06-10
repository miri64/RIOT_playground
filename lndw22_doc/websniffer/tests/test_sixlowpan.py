#! /usr/bin/env python3
# -*- coding: utf-8 -*-
# vim:fenc=utf-8
#
# Copyright (C) 2022 Freie Universität Berlin
#
# Distributed under terms of the MIT license.

# pylint: disable=redefine-outer-scope

import asyncio

import pytest

from scapy.all import Dot15d4, IPv6, UDP
from scapy.contrib.coap import CoAP

from .. import sixlowpan


TEST_DATA = [
    Dot15d4(
        b"q\xdc\x08#\x00\xd8\xbc\x01\x18\x19%\x04\x00\x82\xae\x01\x18\x19%\x04\x00\xc0~"
        b"\x00\x0fl\x07\t>\xbe? \x01\r\xb8\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        b"\x01\xf0\x163\x163g\xb1bD\xbe\xaf\xd3C\x90\xff:1\xbe\x0f\x0b\xc4\xf6e\xdd\xa4"
        b'.\xa2c \xd4-\xa56z"#\x91\x83M\xf6\xef:\xcb\xb5\xf5\x0b\xe3\xd7tp\xb7W\xb5\xee'
        b'P"\xd3\x1e\xe8\xb9\xd2j\xf0\xbdJ\xd3\x1f\x1f\x96\xf2h'
    ),
    Dot15d4(
        b"a\xdc\t#\x00\xd8\xbc\x01\x18\x19%\x04\x00\x82\xae\x01\x18\x19%\x04\x00\xe0~"
        b"\x00\x0f\x0e\xdfw\x1e>\xce\x93R\x1a+2\xfdo\xb8\x9b"
    ),
    Dot15d4(
        b"a\xdc\n#\x00\xd8\xbc\x01\x18\x19%\x04\x00\x82\xae\x01\x18\x19%\x04\x00{w:\x87"
        b"\x00\x1d@\x00\x00\x00\x00 \x01\r\xb8\x00\x00\x00\x00\x02\x04%\x19\x18\x01\xbc"
        b"\xd8\x01\x02\x00\x04%\x19\x18\x01\xae\x82\x00\x00\x00\x00\x00\x00"
    ),
    Dot15d4(
        b"q\xdc\x08#\x00\xd8\xbc\x01\x18\x19%\x04\x00\x82\xae\x01\x18\x19%\x04\x00\xc0~"
        b"\x00\x0e"
    ),
]


@pytest.fixture()
def reass_buf():
    buf = sixlowpan.SixlowpanReassembly()
    orig_max_age = sixlowpan.SixlowpanReassembly.MAX_AGE
    yield buf
    buf.cancel()
    sixlowpan.SixlowpanReassembly.MAX_AGE = orig_max_age


@pytest.mark.asyncio
async def test_sixlowpan_reassembly_add(reass_buf):
    src1, tag1, res1 = reass_buf.add(1654856246.497744, TEST_DATA[0])
    assert tag1 == 15
    assert src1 == 1166689614016130
    assert res1 is None
    src2, tag2, res2 = reass_buf.add(1654856246.502744, TEST_DATA[0])
    assert tag2 == tag1
    assert src2 == src1
    assert res2 is None
    src3, tag3, res3 = reass_buf.add(1654856256.8323612, TEST_DATA[1])
    assert tag3 == tag1
    assert src3 == src1
    assert IPv6 in res3
    assert UDP in res3
    assert CoAP in res3
    src, tag, res = reass_buf.add(1654859418.691214, TEST_DATA[2])
    assert tag is None
    assert src is None
    assert res is None
    src, tag, res = reass_buf.add(1654859418.691214, TEST_DATA[3])
    print(repr(TEST_DATA[3]))
    assert tag == 14
    assert src == 1166689614016130
    assert res is None


@pytest.mark.asyncio
async def test_sixlowpan_reassembly_remove(reass_buf):
    exp_tag = 15
    exp_src = 1166689614016130
    with pytest.raises(KeyError):
        reass_buf[exp_src, exp_tag]  # pylint: disable=pointless-statement
    src, tag, _ = reass_buf.add(1654856246.497744, TEST_DATA[0])
    assert src == exp_src
    assert tag == exp_tag
    assert len(reass_buf[src, tag]) == 1
    src, tag, res = reass_buf.add(1654856256.8323612, TEST_DATA[1])
    assert len(reass_buf[src, tag]) == 2
    reass_buf.remove(src, tag)
    with pytest.raises(KeyError):
        reass_buf[src, tag]  # pylint: disable=pointless-statement
    reass_buf.remove(src, tag)


@pytest.mark.asyncio
async def test_sixlowpan_reassembly_timeout(reass_buf):
    reass_buf.MAX_AGE = 0.01
    exp_tag = 15
    exp_src = 1166689614016130
    with pytest.raises(KeyError):
        reass_buf[exp_src, exp_tag]  # pylint: disable=pointless-statement
    src, tag, _ = reass_buf.add(1654856246.497744, TEST_DATA[0])
    assert src == exp_src
    assert tag == exp_tag
    assert len(reass_buf[src, tag]) == 1
    await asyncio.sleep(0.015)
    with pytest.raises(KeyError):
        reass_buf[exp_src, exp_tag]  # pylint: disable=pointless-statement
