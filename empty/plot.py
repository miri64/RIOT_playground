#! /usr/bin/env python
# -*- coding: utf-8 -*-
# vim:fenc=utf-8
#
# Copyright © 2016 Martine Lenders <mail@martine-lenders.eu>
#
# Distributed under terms of the MIT license.

import pandas as pd
import matplotlib.pyplot as plt

df_while = pd.read_csv("while_sleepy_radio.oml", skiprows=9, delimiter='\t',
                       usecols=[5], names=["power while(1)"])
df_idle = pd.read_csv("idle_sleepy_radio.oml", skiprows=9, delimiter='\t',
                       usecols=[5], names=["power \"idle\""])
df = pd.concat([df_while, df_idle], axis=1, join_axes=[df_while.index])

df.plot.box(return_type='axes')
plt.ylabel("Power consumption in W")
plt.show()
