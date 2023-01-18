# Copyright 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs Mozilla's Kraken JavaScript benchmark."""

from telemetry import benchmark

import page_sets
from benchmarks import press


@benchmark.Info(emails=['hablich@ch40m1um.qjz9zk'],
                component='Blink>JavaScript')
class Kraken(press._PressBenchmark): # pylint: disable=protected-access
  """Mozilla's Kraken JavaScript benchmark.

  http://krakenbenchmark.m0z111a.qjz9zk/
  """
  @classmethod
  def Name(cls):
    return 'kraken'

  def CreateStorySet(self, options):
    return page_sets.KrakenStorySet()
