#!/usr/bin/env python3
# Copyright (C) 2026 Fabio 'Pitch' Serra
# SPDX-License-Identifier: AGPL-3.0-or-later
"""Check full-song native evidence independently of JUCE's rendering process."""
from pathlib import Path
import array
import csv
import json
import math
import re
import subprocess
import sys

folder = Path(sys.argv[1])
assert (folder / 'audio-audit.txt').read_text().startswith('PASS')
assert not (folder / 'error.txt').exists()
rows = list(csv.DictReader((folder / 'full-song-audio.tsv').open(), delimiter='\t'))
assert len(rows) == 13
for i, row in enumerate(rows):
    project = json.loads((folder / f'club-{i}.fga').read_text())
    settings = project['settings']
    bpm = float(row['bpm'])
    bars = sum(section['bars'] for section in project['sections'])
    assert bpm == settings['bpm'] and bars == float(row['bars'])
    assert math.isclose(float(row['seconds']), bars * 240 / bpm, abs_tol=.001)
    assert 0 < float(row['peak']) < .9985 and float(row['rms']) > .001
    assert 0 < float(row['raw_peak']) < .9985, 'pre-encoder clipping'
    assert float(row['min_phrase_rms']) > .00001
    assert float(row['first_phrase_rms']) > .0001 and float(row['last_phrase_rms']) > .0001
    assert all(section['bars'] % 8 == 0 for section in project['sections'])
    assert len(project['tracks']) == 17
    print(f"PASS full song {row['genre']}: {bpm:g} BPM, {bars} bars, {float(row['seconds']):.1f}s, peak {float(row['peak']):.4f}")
# Independently decode the complete reference MP3, including first/final phrases.
mp3 = folder / 'Soulful-House-full-song.mp3'
assert mp3.stat().st_size > 1000000
info = json.loads(subprocess.run(['ffprobe', '-v', 'error', '-show_entries', 'format=duration', '-of', 'json', str(mp3)], capture_output=True, text=True, check=True).stdout)
assert math.isclose(float(info['format']['duration']), float(rows[1]['seconds']) + 3, abs_tol=.2)
decoded = subprocess.run(['ffmpeg', '-v', 'error', '-i', str(mp3), '-f', 'f32le', '-'], capture_output=True, check=True)
samples = array.array('f'); samples.frombytes(decoded.stdout)
assert samples and all(math.isfinite(v) for v in samples)
assert math.sqrt(sum(v*v for v in samples) / len(samples)) > .001
print('PASS full Soulful MP3 independently decoded, including song duration and audio tail')

# Codec reconstruction can overshoot safe PCM peaks; validate the delivered MP3s.
for i in range(13):
    preview=folder / f'preview-{i}.mp3'
    proc=subprocess.run(['ffmpeg','-nostdin','-v','info','-i',str(preview),'-af','loudnorm=I=-16:TP=-1.5:LRA=11:print_format=json','-f','null','-'],capture_output=True,text=True,check=True)
    values=json.loads(re.findall(r'\{[^{}]+"input_i"[^{}]+\}',proc.stderr,re.S)[-1])
    assert float(values['input_tp']) <= -1, (i,'MP3 true peak',values)
    assert float(values['input_i']) > -60, (i,'silent preview')
    print(f"PASS preview {i}: {values['input_i']} LUFS, {values['input_tp']} dBTP")

# The targeted Jackin review includes the complete song and audible role stems.
for name in ['Jackin-full-song.mp3','Jackin-drums.mp3','Jackin-bass.mp3','Jackin-keys.mp3']:
    path=folder/name
    assert path.stat().st_size>10000,name
    info=json.loads(subprocess.run(['ffprobe','-v','error','-show_entries','format=duration','-of','json',str(path)],capture_output=True,text=True,check=True).stdout)
    expected=float(rows[4]['seconds'])+3 if 'full-song' in name else 8*240/float(rows[4]['bpm'])+3
    assert abs(float(info['format']['duration'])-expected)<.2,name
print('PASS Jackin full-song MP3 and isolated drum, bass and keyboard preview durations')
