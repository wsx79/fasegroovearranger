# Validation record

This document distinguishes executed checks from outstanding platform/plugin validation.

## Executed in the development workspace

- Compiled the original three engines plus the arranger core with g++ / C++17.
- Passed tests for all 13 genres: note bounds/validity, deterministic stored takes, isolated clip/track/section regeneration, track/clip locks, section duplication/reordering/removal, MIDI track counts and editing operations.
- Independently parsed the exported complete MIDI file and kick stem: correct SMF type 1 / 960 PPQ, tempo and six markers, no unclosed or overlapping same-channel pitches. Example full song: 4,970 matched notes, conductor + 17 instrument tracks.
- Compiled the bundled Shine library and encoded a stereo test signal. ffprobe identified MP3, 44.1 kHz, 2 channels, 320 kbps and approximately 2 seconds duration. Independent ffmpeg decoding recovered the expected 440 Hz / 880 Hz signals on the two channels.

## Executed in GitHub Actions

[Windows, macOS and Linux validation of the bilingual guide and standalone](https://github.com/wsx79/fasegroovearranger/actions/runs/39375342152), source commit `116e933b5274eeae4ccf9c37afc18e3776c56f04`:

- All three native applications compiled; arranger, integrated MP3 and project IO tests passed. Project IO covers JSON round-trip, atomic saves, schema rejection and PPQ MIDI import.
- The native guide rendered both languages, 13 chapters, 11 tables and four embedded screenshots. Automated checks passed internal chapter links, sidebar navigation, search and text zoom; actual guide and credit screenshots were inspected.
- Offline HTML generation passed on Windows, macOS and Linux, including UTF-8 Italian accents, chapter targets and local screenshot assets. Local checks also validated stylesheet, script and language links. Browser interaction was not executed in this workspace.
- The Linux application rendered a mix, all 17 aligned 24-bit WAV stems, MP3 and a fully muted silent mix. Twelve stems were active; five were intentionally empty. All stems aligned at 305,809 frames / 44.1 kHz; mix RMS was 0.14149 and the peak remained below the clipping threshold.
- ffmpeg independently decoded the MP3 into non-silent PCM. The MPEG header confirmed 320 kbps / 44.1 kHz / stereo.
- Callback simulation passed seeking, held-note chase, looping and pause silence.
- Native screenshots and routing checks cover the arrangement, individual instrument panels, full MIDI editors, isolated drums, all guitar lanes and notes mode, the guitar mixer, and APPLY preserving the stored take.
- A real native VST3 test instrument loaded and received MIDI/playhead data. Its editor opened and closed repeatedly. Parameter changes affected rendered audio; saved plugin state restored the original result. This validates the fixture, not arbitrary commercial instruments.
- Windows, universal macOS and Linux packages were produced with guides, privacy, licences, font assets and exact corresponding-source links.

Final packaging refreshes the four documentation screenshots and credit spacing. Subsequent changes rerun the applicable checks; the current pull request exposes the latest workflow result. Automated rendering does not replace listening through a real audio device.

## Requires target hardware and user instruments

Real audio-device playback, latency/buffer behaviour, commercial VST3/AU compatibility and state restoration, long-session editing, accessibility and signed distribution require testing on target machines. No claim is made that arbitrary third-party plugins have already been verified. Current missing DAW capabilities are listed in README.md and both guides.

## Extended piano and six languages (0.3.0)

[Successful Windows, universal macOS and Linux build](https://github.com/wsx79/fasegroovearranger/actions/runs/39379795779), source `230b5d902a631f09b13a7c6d765588557e30afb9`:

- Native checks passed six live interface languages, translated control tooltips, unchanged musical JSON and preserved menu selections. Actual Italian/English interface and extended piano screenshots were inspected.
- The original circle of fifths, DNA, Morph, A/B coach, variation lab and MIDI-learning window rendered natively.
- Core checks passed user-grid generation, step velocity changes, per-chord inversion, controller scheduling and DNA rhythm/harmony locks, in addition to all thirteen genre suites. Project IO covers the additional persistent parameters.
- Independent SMF validation passed the 18-track song, isolated kick and piano/controller fixture. The latter preserves note pairs and contains CC64/CC11, with pedal and expression reset at the end.
- Guide refreshes and final packaging rerun the same platform checks. Real MIDI-device callbacks and commercial-plugin interpretation of CC64/CC11 still need testing with the user's hardware and instruments.
