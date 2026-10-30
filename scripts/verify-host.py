#!/usr/bin/env python3
"""Independently verify a real VST3 rendered MIDI and restored its project state."""
from pathlib import Path
import json
import math
import sys
import wave

folder = Path(sys.argv[1])
assert not (folder / 'host-error.txt').exists(), (folder / 'host-error.txt').read_text() if (folder / 'host-error.txt').exists() else ''
assert (folder / 'host.txt').read_text().startswith('PASS')
legacy_report = (folder / 'host-legacy.txt').read_text(encoding='utf-8').splitlines()
assert legacy_report and all(line.startswith('PASS ') for line in legacy_report), legacy_report
for version in (1, 2, 3):
    legacy = json.loads((folder / f'host-legacy-v{version}.fga').read_text(encoding='utf-8'))
    assert legacy['version'] == version
    assert all(t['instrumentState'] and not t['instrumentState'].startswith('FGAHOST1:') for t in legacy['tracks'][:3])
    lane = legacy['tracks'][1]['parameterAutomation'][0]
    assert lane['slot'] == 0 and lane['index'] == 0 and not lane.get('parameterId')
upgraded = json.loads((folder / 'host-upgraded.fga').read_text(encoding='utf-8'))
assert upgraded['version'] == 4
assert all(t['instrumentState'].startswith('FGAHOST1:') for t in upgraded['tracks'][:3])

def signal(name):
    with wave.open(str(folder / name), 'rb') as w:
        assert w.getsampwidth() == 3 and w.getnchannels() == 2
        frames, rate = w.getnframes(), w.getframerate()
        data = w.readframes(frames)
    # Skip attack/frame boundaries, measure within the held A4 note.
    values = [int.from_bytes(data[i:i+3], 'little', signed=True) / 8388608
              for i in range(3000*6, min(len(data), 30000*6), 6)]
    rms = math.sqrt(sum(v*v for v in values) / len(values))
    crossings = sum(a <= 0 < b for a, b in zip(values, values[1:]))
    frequency = crossings * rate / len(values)
    return rms, frequency, frames

before, loud, restored, offline = [signal(name) for name in ['host-before.wav', 'host-loud.wav', 'host-restored.wav', 'host-offline.wav']]
assert before[0] > .001 and abs(before[1]-440) < 3, before
assert before[2] == loud[2] == restored[2]
assert 2.95 < loud[0]/before[0] < 3.05, (before, loud)
assert abs(restored[0]/before[0]-1) < .001, (before, restored)
# Export rendered on a private copy of the engine (plugin re-created from its saved state).
assert offline[2] == restored[2] and abs(offline[0]/restored[0]-1) < .001 and abs(offline[1]-restored[1]) < 1, (restored, offline)
print(f'PASS real VST3 host: {before[1]:.1f} Hz MIDI note, gain ratio {loud[0]/before[0]:.3f}, restored ratio {restored[0]/before[0]:.6f}, private-copy export ratio {offline[0]/restored[0]:.6f}; editor reopened and project state restored')
print('PASS legacy versions 1/2/3 and upgraded version 4: reopen and Play with three multi-output VST3 instruments, saved state and indexed automation')

# A selected instrument program is distinct from the factory default, even when its
# opaque component chunk does not serialize the host-visible program number.
program_project = json.loads((folder / 'host-program.fga').read_text(encoding='utf-8'))
program_state = json.loads(program_project['tracks'][0]['instrumentState'].removeprefix('FGAHOST1:'))
assert program_state['program'] == 1 and program_state['programName'] == 'Bright', program_state
program = signal('host-program.wav')
assert program[0] > .001 and abs(program[1] - 880) < 4, program
export_report = (folder / 'export-ui' / 'export-ui.txt').read_text(encoding='utf-8').splitlines()
assert export_report and all(line.startswith('PASS ') for line in export_report), export_report
print(f'PASS reopened selected preset: {program[1]:.1f} Hz; native state restored while inactive; complete WAV encoding confirmation/file-choice/render flow')
