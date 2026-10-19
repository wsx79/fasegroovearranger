# Validation record

This document distinguishes executed checks from outstanding platform/plugin validation.

## Executed in the development workspace

- Compiled the original three engines plus the arranger core with g++ / C++17.
- Passed tests for all 13 genres: note bounds/validity, deterministic stored takes, isolated clip/track/section regeneration, track/clip locks, section duplication/reordering/removal, MIDI track counts and editing operations.
- Independently parsed the exported complete MIDI file and kick stem: correct SMF type 1 / 960 PPQ, tempo and six markers, no unclosed or overlapping same-channel pitches. Example full song: 5,189 matched notes, conductor + 17 instrument tracks.
- Compiled the bundled Shine library and encoded a stereo test signal. ffprobe identified MP3, 44.1 kHz, 2 channels, 320 kbps and approximately 2 seconds duration.

## Automated checks configured

- Windows, macOS and Linux standalone compilation.
- JSON project round-trip, safe save/load, schema rejection and PPQ MIDI import tests.
- Linux native-app smoke test: original internal instruments, WAV mix/stems, integrated MP3 export and screenshot.
- Independent rendered-audio inspection.

Build execution results are recorded in GitHub Actions. A configured workflow is not itself evidence that a platform build passed.

## Requires target hardware and user instruments

Real audio-device playback, latency/buffer behaviour, external VST3/AU loading/UI/state, compatibility with particular sound libraries, long-session editing, accessibility and signed distribution require testing on target machines. No claim is made that arbitrary third-party plugins have already been verified. Current missing DAW capabilities are listed in README.md and both guides.
