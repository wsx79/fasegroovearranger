#!/usr/bin/env python3
# Copyright (C) 2026 Fabio 'Pitch' Serra
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Run native UI and short PCM/MP3 routing checks on every supported platform."""
import argparse
import os
import platform
from native_test_support import build_root, output_folder, run_application, verify

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir')
parser.add_argument('--output')
args = parser.parse_args()
build = build_root(args.build_dir)
folder = output_folder('arranger-smoke', args.output)
# The interactive evidence harness (synthetic input and pixel-layout assertions)
# runs on Linux and Windows; macOS still renders the interface and verifies the
# exported PCM/MP3 audio below.
if platform.system() == 'Darwin':
    os.environ['FGA_SKIP_UI_EVIDENCE'] = '1'
run_application(build, '--smoke-test', folder, timeout=600)
verify('verify-audio.py', folder)
