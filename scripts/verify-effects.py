#!/usr/bin/env python3
# Copyright (C) 2026 Fabio 'Pitch' Serra
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Independently measure the affine-effect exports, including every silent sample."""
import json
from pathlib import Path
import sys
import wave

folder = Path(sys.argv[1])
expected = {'effects-order-ab.wav': .1, 'effects-order-ba.wav': .05,
            'effects-automated-ba.wav': .08, 'effects-automated-ab.wav': .04,
            'effects-private-export.wav': .04, 'effects-bypass.wav': 0,
            'effects-undo.wav': .04, 'effects-relink.wav': .04, 'effects-editor-restored.wav': .03, 'effects-parameter-bypass.wav': .08}
for name, target in expected.items():
    with wave.open(str(folder / name)) as stream:
        assert stream.getsampwidth() == 3, (name, stream.getsampwidth())
        assert stream.getnchannels() == 2
        raw = stream.readframes(stream.getnframes())
    total = 0
    maximum_error = 0
    count = len(raw) // 3
    for offset in range(0, len(raw), 3):
        sample = int.from_bytes(raw[offset:offset + 3], 'little', signed=True) / 8388608
        total += sample
        maximum_error = max(maximum_error, abs(sample - target))
    assert count > 0 and maximum_error < .0001, (name, maximum_error, target)
    print(f'PASS {name}: {count} samples, mean {total / count:.6f}, max error {maximum_error:.8f}')
project = json.loads((folder / 'effects-project.fga').read_text(encoding='utf-8'))
track = project['tracks'][0]
assert project['version'] == 4
assert len(track['effects']) == 2 and len(track['parameterAutomation']) == 2
assert {item['slot'] for item in track['effects']} == {1, 2}
assert all(item['state'] for item in track['effects'])
assert track['effects'][0]['name'] == 'Voce brillante'
assert all(item['parameterId'] for item in track['parameterAutomation'])
assert 'FAIL' not in (folder / 'effects.txt').read_text(encoding='utf-8')
print('PASS persisted instance identity, alias, states and parameter IDs')
