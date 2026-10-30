#!/usr/bin/env python3
# Copyright (C) 2026 Fabio 'Pitch' Serra
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Verify native serial VST3 effects, state, automation and the inspector."""
import argparse
from native_test_support import build_root, output_folder, run_application, verify

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir')
parser.add_argument('--output')
args = parser.parse_args()
build = build_root(args.build_dir)
effect = build / 'ArrangerEffectProbe_artefacts' / 'Release' / 'VST3' / 'Arranger Effect Probe.vst3'
if not effect.exists():
    raise SystemExit(f'Build the native effect probe first: {effect}')
folder = output_folder('native-effects', args.output)
run_application(build, '--effects-smoke-test', effect, folder)
verify('verify-effects.py', folder)
