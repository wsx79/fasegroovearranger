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
if platform.system() == 'Linux':
    run_application(build, '--event-loop-audit', folder / 'event-loop', timeout=60)
startup = folder / 'startup'
run_application(build, '--startup-splash-test', startup, timeout=60)
if not (startup / 'startup.txt').read_text().startswith('PASS '):
    raise RuntimeError('Startup splash did not transition to the visible application')
eq_folder = folder / 'equalizer'
run_application(build, '--eq-ui-test', eq_folder, timeout=90)
if 'FAIL ' in (eq_folder / 'eq-checks.txt').read_text():
    raise RuntimeError('Parametric EQ UI or DSP regression')
piano_folder = folder / 'piano-roll'
run_application(build, '--piano-roll-test', piano_folder, timeout=90)
if 'FAIL ' in (piano_folder / 'piano-roll.txt').read_text():
    raise RuntimeError('Piano roll chords, preferences or reference-note view regression')
mastering_folder = folder / 'mastering'
run_application(build, '--mastering-ui-test', mastering_folder, timeout=180)
mastering = (mastering_folder / 'mastering-ui.txt').read_text(encoding='utf-8').splitlines()
if not mastering or any(not line.startswith('PASS ') for line in mastering):
    raise RuntimeError('Mastering Assistant native UI report missing, failed or incomplete')
run_application(build, '--smoke-test', folder, timeout=600)
verify('verify-audio.py', folder)
