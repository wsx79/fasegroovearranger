#!/usr/bin/env python3
"""Independently verify a real VST3 rendered MIDI and restored its project state."""
from pathlib import Path
import math
import sys
import wave

folder = Path(sys.argv[1])
assert not (folder / 'host-error.txt').exists(), (folder / 'host-error.txt').read_text() if (folder / 'host-error.txt').exists() else ''
assert (folder / 'host.txt').read_text().startswith('PASS')

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
