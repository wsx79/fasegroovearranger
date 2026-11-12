#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Independent 32x FFmpeg reconstruction of the phase-08 QA fixtures (stdlib only)."""
import argparse
import array
import csv
import json
import math
from pathlib import Path
import subprocess
import sys

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('folder', type=Path)
parser.add_argument('--ffmpeg', default='ffmpeg')
args = parser.parse_args()
results = []
with (args.folder / 'manifest.csv').open(newline='') as manifest:
    for row in csv.DictReader(manifest):
        path = args.folder / row['file']
        sample_rate = int(path.name.split('-')[0])
        # Explicit zero padding matches the export estimator's boundary convention.
        filters = f'adelay=2048S:all=1,apad=pad_len=2048,aresample={sample_rate * 32}:filter_size=1024:phase_shift=10:cutoff=1:exact_rational=1'
        command = [args.ffmpeg, '-v', 'error', '-i', str(path), '-af', filters,
                   '-f', 'f64le', '-acodec', 'pcm_f64le', '-']
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        peak = 0.0
        while True:
            block = process.stdout.read(65536)
            if not block:
                break
            if len(block) % 8:
                raise SystemExit('Truncated f64 verification stream')
            samples = array.array('d')
            samples.frombytes(block)
            if sys.byteorder != 'little':
                samples.byteswap()
            if any(not math.isfinite(s) for s in samples):
                raise SystemExit('Non-finite reconstructed audio')
            peak = max(peak, max(map(abs, samples), default=0.0))
        if process.wait() != 0:
            raise SystemExit(f'FFmpeg failed: {path.name}')
        db = 20 * math.log10(max(1e-12, peak))
        expected = float(row['peak16'])
        if expected:
            difference = abs(db - 20 * math.log10(expected))
            if difference > .15:
                raise SystemExit(f'16x/32x disagree: {path.name}: {difference:.6f} dB')
        elif db > float(row['ceiling_db']) + .001:
            raise SystemExit(f'Normalized preview exceeds ceiling: {path.name}: {db:.6f} dBTP')
        results.append(dict(file=path.name, peak32_db=db, peak16=expected))
        print(f'PASS {path.name}: independent TP32 {db:.6f} dBTP')
if len(results) != 6:
    raise SystemExit(f'Expected 6 fixture files, found {len(results)}')
(args.folder / 'verification32.json').write_text(json.dumps(results, indent=2) + '\n')
