#!/usr/bin/env python
# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

webgpu_cts_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
dawn_third_party_dir = os.path.dirname(webgpu_cts_dir)

_possible_chromium_third_party_dir = os.path.dirname(
    os.path.dirname(dawn_third_party_dir))
_possible_node_dir = os.path.join(_possible_chromium_third_party_dir, 'node')
if os.path.exists(_possible_node_dir):
    chromium_third_party_dir = _possible_chromium_third_party_dir
    node_dir = _possible_node_dir
