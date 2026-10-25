#!/usr/bin/env python3
"""Independent SMF reader: validate structure, note pairs, tempo and split stems."""
from pathlib import Path
import struct
import sys

def read(path, expected_tracks, require_cc=False):
    data = Path(path).read_bytes()
    assert data[:4] == b'MThd'
    header_len, fmt, tracks, ppq = struct.unpack('>IHHH', data[4:14])
    assert (header_len, fmt, tracks, ppq) == (6, 1, expected_tracks, 960)
    pos, note_count, tempos, markers = 14, 0, 0, 0
    controllers=set(); last_cc={}
    for _ in range(tracks):
        assert data[pos:pos+4] == b'MTrk'
        size = struct.unpack('>I', data[pos+4:pos+8])[0]
        pos += 8
        end, active, tick = pos + size, {}, 0
        track_name = ""
        def vlq():
            nonlocal pos
            value = 0
            for _ in range(4):
                v = data[pos]; pos += 1
                value = (value << 7) | (v & 127)
                if not v & 128:
                    return value
            raise AssertionError('overlong MIDI delta')
        while pos < end:
            tick += vlq()
            status = data[pos]; pos += 1
            if status == 255:
                typ = data[pos]; pos += 1
                size = vlq()
                payload = data[pos:pos+size]; pos += size
                if typ == 81:
                    assert size == 3 and int.from_bytes(payload, 'big') > 0
                    tempos += 1
                if typ == 6: markers += 1
                if typ == 3: track_name = payload.decode("utf-8")
                if typ == 47: assert size == 0 and pos == end
            else:
                typ, channel = status >> 4, status & 15
                if track_name == "Lead": assert channel == 4, "Lead must use melodic channel 5, never drum channel 10"
                assert typ in (8, 9, 11, 12)
                pitch = data[pos]; pos += 1
                if typ == 12: continue
                velocity = data[pos]; pos += 1
                if typ==11:
                    controllers.add(pitch);last_cc[pitch]=velocity
                    assert 0<=pitch<=127 and 0<=velocity<=127
                    continue
                key = channel, pitch
                if typ == 9 and velocity:
                    assert key not in active, f'overlapping note {key}'
                    active[key] = tick; note_count += 1
                else:
                    assert key in active, f'orphan note-off {key}'
                    assert tick > active.pop(key), 'zero note duration'
        assert not active, f'hanging notes in track: {active}'
        assert pos == end
    assert pos == len(data) and tempos == 1 and markers >= 6 and note_count > 0
    if require_cc: assert {11,64}.issubset(controllers) and last_cc[64]==0 and last_cc[11]==127, 'pedal/expression export and reset'
    print(f'PASS {path}: {tracks} tracks, {note_count} matched notes, {markers} section markers')

folder = Path(sys.argv[1] if len(sys.argv) > 1 else 'build/core')
read(folder / 'test-song.mid', 19)
read(folder / 'test-kick.mid', 2)

read(folder / "test-piano-cc.mid",2,True)

for path in sorted(folder.glob("club-*.mid")):
    read(path,19)
