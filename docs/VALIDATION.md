# Validation record

This document distinguishes executed checks from outstanding platform/plugin validation.

## Executed in the development workspace

- Compiled the original three engines plus the arranger core with g++ / C++17.
- Passed tests for all 13 genres: note bounds/validity, deterministic stored takes, isolated clip/track/section regeneration, track/clip locks, section duplication/reordering/removal, MIDI track counts and editing operations.
- Independently parsed the exported complete MIDI file and kick stem: correct SMF type 1 / 960 PPQ, tempo and six markers, no unclosed or overlapping same-channel pitches. Example full song: 4,970 matched notes, conductor + 17 instrument tracks.
- Compiled the bundled Shine library and encoded a stereo test signal. ffprobe identified MP3, 44.1 kHz, 2 channels, 320 kbps and approximately 2 seconds duration. Independent ffmpeg decoding recovered the expected 440 Hz / 880 Hz signals on the two channels.

## Executed in GitHub Actions

[Successful Windows, macOS and Linux validation run](https://github.com/wsx79/fasegroovearranger/actions/runs/39338384737), source commit `68fec8f6d35a733cec4791609ce1a2e19d0ae898`:

- All three native standalone applications compiled and passed the core, integrated MP3 and project IO tests.
- Project IO checks cover JSON round-trip, atomic file save/load, invalid schema rejection and PPQ MIDI import.
- Linux standalone rendered a complete mix, 17 WAV stems, MP3 and a fully muted silent mix. The test arrangement had 12 active stems and five intentionally empty ones. All 17 WAV stems aligned at 305,809 frames / 44.1 kHz; the mix RMS was 0.14934.
- ffmpeg independently decoded the MP3 and confirmed a non-silent signal. The MPEG header confirmed 320 kbps / 44.1 kHz / stereo.
- Manual callback simulation passed seeking, held-note chase, crossing a loop boundary and pause silence.
- Inspected the actual native screenshot from the smoke-test artifact; adjusted control widths to keep the loop/mute/solo/lock labels readable.
- Windows/macOS/Linux executable packages, full notices and exact corresponding-source links were produced as workflow artifacts.

Further changes rerun the relevant checks. The native smoke test is automated; it does not substitute for listening on a real audio device or testing the user's external plugins.

## Requires target hardware and user instruments

Real audio-device playback, latency/buffer behaviour, external VST3/AU loading/UI/state, compatibility with particular sound libraries, long-session editing, accessibility and signed distribution require testing on target machines. No claim is made that arbitrary third-party plugins have already been verified. Current missing DAW capabilities are listed in README.md and both guides.
