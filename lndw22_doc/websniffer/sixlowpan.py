#! /usr/bin/env python3
# -*- coding: utf-8 -*-
# vim:fenc=utf-8
#
# Copyright (C) 2022 Freie Universität Berlin
#
# Distributed under terms of the MIT license.


import asyncio
import copy

from scapy.all import (
    Dot15d4,
    Dot15d4Data,
    IPv6,
    LoWPANFragmentationFirst,
    LoWPANFragmentationSubsequent,
    raw,
)


class SixlowpanReassembly:
    MAX_AGE = 10

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._fragments = dict()
        self._gcs = dict()

    def _remove(self, src, tag):
        try:
            del self._fragments[src, tag]
        except KeyError:
            pass
        try:
            del self._gcs[src, tag]
        except KeyError:
            pass

    async def _timeout(self, src, tag):
        await asyncio.sleep(self.MAX_AGE)
        self._remove(src, tag)

    def __getitem__(self, src_tag_tuple):
        return [(time, pkg) for typ, time, pkg in self._fragments[src_tag_tuple]]

    def add(self, time, fragment):
        cls = None
        if LoWPANFragmentationFirst in fragment:
            cls = LoWPANFragmentationFirst
            offset = 0
        elif LoWPANFragmentationSubsequent in fragment:
            cls = LoWPANFragmentationSubsequent
            offset = fragment[cls].datagramOffset
        if cls is None:
            return None, None, None
        src = fragment[Dot15d4Data].src_addr
        tag = fragment[cls].datagramTag
        size = fragment[cls].datagramSize
        if (src, tag) in self._fragments:
            self._fragments[src, tag].append((cls, time, fragment))
        else:
            self._fragments[src, tag] = [(cls, time, fragment)]
        self._gcs[src, tag] = asyncio.ensure_future(self._timeout(src, tag))
        fragments = []
        offsets = set()
        for typ, _, frag in self._fragments[src, tag]:
            if typ is LoWPANFragmentationFirst:
                offset = 0
            else:
                offset = frag[typ].datagramOffset
            if offset not in offsets:
                fragments.append((offset, typ, frag))
            offsets.add(offset)

        fragments.sort(key=lambda v: v[0])
        pkt = b""
        # sixlowpan_defragment of scapy is somewhat broken so let's do it ourself
        for _, typ, frag in fragments:
            pkt += raw(frag[typ].payload)
        if pkt:
            hdrs = None
            for layer in fragment.layers():
                if fragment[layer].name.startswith("802.15.4"):
                    hdr = copy.deepcopy(fragment[layer])
                    hdr.remove_payload()
                    if hdrs is None:
                        hdrs = hdr
                    else:
                        hdrs /= hdr
            pkt = hdrs / pkt
            pkt = Dot15d4(raw(pkt))
            if IPv6 in pkt and (pkt[IPv6].plen + len(IPv6())) == size:
                return src, tag, pkt
        return src, tag, None

    def remove(self, src, tag):
        try:
            self._gcs[src, tag].cancel()
        except KeyError:
            pass
        self._remove(src, tag)

    def cancel(self):
        for gc in self._gcs.values():
            gc.cancel()
        self._fragments = dict()
