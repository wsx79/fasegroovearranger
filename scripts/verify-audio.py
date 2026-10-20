#!/usr/bin/env python3
"""Independent PCM24 / MPEG frame checks for native standalone smoke renders."""
from pathlib import Path
import math
import struct
import sys
import wave
import array
import subprocess

folder = Path(sys.argv[1])
assert not (folder / 'error.txt').exists(), (folder / 'error.txt').read_text() if (folder / 'error.txt').exists() else ''

def inspect(path):
    with wave.open(str(path), 'rb') as w:
        assert (w.getnchannels(), w.getsampwidth()) == (2, 3)
        rate, frames = w.getframerate(), w.getnframes()
        raw = w.readframes(frames)
    squared, peak, count = 0, 0, 0
    for i in range(0, len(raw), 3):
        sample = int.from_bytes(raw[i:i+3], 'little', signed=True) / 8388608
        assert math.isfinite(sample)
        squared += sample * sample; peak = max(peak, abs(sample)); count += 1
    return rate, frames, math.sqrt(squared / max(count, 1)), peak

mix = inspect(folder / 'mix.wav')
assert mix[2] > .001 and 0 < mix[3] < .9985, ("default mix must retain headroom", mix)
active = 0
for i in range(17):
    stem = inspect(folder / f'stem-{i}.wav')
    assert stem[:2] == mix[:2], f'stem {i} alignment'
    if stem[2] > .00001: active += 1
    if i < 5: assert stem[2] > .00001, f'silent primary instrument {i}'
assert active >= 8, f'only {active} active stems'
silence = inspect(folder / 'silence.wav')
assert silence[3] == 0, silence
mp3 = (folder / 'mix.mp3').read_bytes()
assert len(mp3) > 50000
# MPEG-1 Layer III, 320 kbps, 44.1 kHz, stereo.
header = int.from_bytes(mp3[:4], 'big')
assert header >> 21 == 0x7ff and (header >> 19) & 3 == 3 and (header >> 17) & 3 == 1
assert (header >> 12) & 15 == 14 and (header >> 10) & 3 == 0 and (header >> 6) & 3 != 3
assert (folder / 'interface.png').stat().st_size > 10000
decoded = subprocess.run(['ffmpeg', '-v', 'error', '-i', str(folder / 'mix.mp3'), '-f', 'f32le', '-'], capture_output=True, check=True)
samples = array.array('f'); samples.frombytes(decoded.stdout)
assert samples and math.sqrt(sum(s*s for s in samples) / len(samples)) > .001
assert (folder / 'transport.txt').read_text().startswith('PASS')
print(f'PASS native rendering: {active} active stems, aligned {mix[1]} frames at {mix[0]} Hz, RMS {mix[2]:.5f}, silent mute mix, bundled MP3 and screenshot')

assert (folder / "ui.txt").read_text().startswith("PASS"), "instrument/editor interface routing"

assert (folder / "guide.txt").read_text().startswith("PASS"), "native guide navigation, search and layout"
for name in ["guide-welcome-it.png", "guide-chapter-it.png", "guide-search-en.png", "credits.png"]:
    assert (folder / name).stat().st_size > 10000, name
