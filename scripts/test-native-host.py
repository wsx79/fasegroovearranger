#!/usr/bin/env python3
# Copyright (C) 2026 Fabio 'Pitch' Serra
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Exercise the same real VST3 editor/state/audio lifecycle on each native platform."""
import argparse
from native_test_support import build_root, output_folder, run_application, verify

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir')
parser.add_argument('--output')
args = parser.parse_args()
build = build_root(args.build_dir)
plugin = build / 'ArrangerHostProbe_artefacts' / 'Release' / 'VST3' / 'Arranger Host Probe.vst3'
if not plugin.exists():
    raise SystemExit(f'Build the native host probe first: {plugin}')
folder = output_folder('native-host', args.output)
run_application(build, '--host-smoke-test', plugin, folder)
verify('verify-host.py', folder)
