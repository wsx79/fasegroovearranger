#!/usr/bin/env python3
# Copyright (C) 2026 Fabio 'Pitch' Serra
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Render and independently decode full arrangements with no audio device required."""
import argparse
import shutil
import platform
import subprocess
from native_test_support import build_root, output_folder, verify

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir')
parser.add_argument('--output')
args = parser.parse_args()
if not shutil.which('ffmpeg'):
    raise SystemExit('ffmpeg is required for the independent MP3 verification.')
build = build_root(args.build_dir)
folder = output_folder('club-audit', args.output)
engine = build / 'fga_engine_tests_artefacts' / 'Release' / ('fga_engine_tests.exe' if platform.system() == 'Windows' else 'fga_engine_tests')
if not engine.is_file():
    raise SystemExit(f'Build the native engine tests first: {engine}')
subprocess.run([str(engine), '--club-audit', str(folder)], check=True, timeout=5400)
verify('verify-club-audio.py', folder)
