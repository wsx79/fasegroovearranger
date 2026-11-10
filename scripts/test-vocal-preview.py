#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Reuse vocal UI regressions without installing unreviewed external voice tools."""
import argparse
import json
import os
from native_test_support import build_root, output_folder, run_application

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir')
parser.add_argument('--output')
args = parser.parse_args()
build = build_root(args.build_dir)
folder = output_folder('vocal-preview', args.output)
# Deliberately exercise the missing-helper UI; do not claim synthesis PASS.
os.environ.pop('FGA_ECANTORIX', None)
results = {}
for name in ('sample-import', 'vocal-studio'):
    target = folder / name
    run_application(build, '--' + name + '-test', target)
    lines = (target / (name + '.txt')).read_text(encoding='utf-8').splitlines()
    if not lines or any(line.startswith('FAIL') for line in lines):
        raise RuntimeError('Vocal preview regression: ' + name)
    results[name] = sum(line.startswith('PASS ') for line in lines)
results['external_guide_render'] = 'NOT RUN: eCantorix/Perl/eSpeak/SoX not installed by this workflow'
(folder / 'preview-tests.json').write_text(json.dumps(results, indent=2), encoding='utf-8')
print(json.dumps(results, indent=2))
if os.environ.get('GITHUB_STEP_SUMMARY'):
    with open(os.environ['GITHUB_STEP_SUMMARY'], 'a', encoding='utf-8') as report:
        report.write('## Vocaltest Preview\n\n' + '\n'.join(f'- {k}: {v}' for k, v in results.items()) + '\n')
