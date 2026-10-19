# Fase Groove Arranger

**A standalone MIDI arranging workstation for the Fase Groove ecosystem.**

Create a complete song with the original Piano, Drums and Guitar engines: Intro → Main → Breakdown → Build-up → Drop → Outro. Choose one of the same 13 genres, edit the generated notes, assign sounds per track, and export the mix or individual stems.

Created by **Fabio 'Pitch' Serra / FS Pitch**. Original ecosystem code credit: **Claude (Anthropic)**. Arranger integration: **Codex (OpenAI)**. Free software under **AGPL-3.0-or-later**.

## What is implemented

- The original, vendored C++ generation engines run inside the application; no Fase Groove plugin installation is required.
- 17 tracks: Piano, Bass, Strings, Guitar, Kick, Snare, Clap, Rim, Closed Hat, Open Hat, Ride, Shaker, Tambourine, Conga, Cowbell, Tom and Crash.
- The guitar stays together as one instrument, preserving its playing lanes internally.
- Full-song, single-track, single-section and single-clip generation, deterministic stored takes, track/clip generation locks, and 40-step undo/redo.
- Six musical section types, duplication, ordering, renaming and adjustable bar lengths.
- Key, 13 scales, chord progression library, chord colour, voicing, bass/strings patterns, tempo, density, swing and humanisation.
- Editable piano roll: draw, move, resize, delete, change velocity, select multiple notes, quantise, transpose and copy/paste. Import standard PPQ MIDI into a selected clip.
- Native audio playback, seeking and song/section loops; per-track gain, pan, mute, solo and meters; master level.
- Original synthesized guide sounds, user audio samples with adjustable root note, and one native VST3 instrument per track (Audio Units also on macOS). Plugin editor and state persistence.
- Project files (`.fga`), atomic saves, periodic local autosave and missing-instrument relinking.
- Multitrack MIDI and individual MIDI files; stereo 24-bit WAV and 320 kbps stereo MP3, for a mix, one track or all 17 stems. **The MP3 encoder is bundled.**
- Italian/English guides, in-app privacy/permissions and the original ecosystem's credits, artist logo and links.

## Build and run

Requires CMake 3.22+, a C++17 compiler, and JUCE **8.0.9** (automatically fetched at build time, or supplied locally with `-DJUCE_DIR=/path/to/JUCE`). The runtime works offline.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j2
ctest --test-dir build -C Release --output-on-failure
```

The standalone application is in `build/FaseGrooveArranger_artefacts/Release/`. Open the application bundle on macOS or the executable on Windows/Linux. This is a **desktop application**, not a website or an embedded web view.

On Ubuntu/Debian install the native development dependencies first:

```sh
sudo apt-get install build-essential cmake libasound2-dev libx11-dev libxext-dev libxinerama-dev libxrandr-dev libxcursor-dev libfreetype6-dev libfontconfig1-dev libgl1-mesa-dev
```

GitHub Actions builds Windows, macOS and Linux packages and runs the tests. Download successful builds from the workflow's **Artifacts** panel. Development artifacts are unsigned; release signing/notarisation and installers are not configured.

For the portable arranging and MP3 engines alone:

```sh
cmake -S . -B build-core -DFGA_BUILD_APP=OFF
cmake --build build-core -j2
ctest --test-dir build-core --output-on-failure
```

Without CMake, `bash scripts/test-core.sh` runs the arranging tests with g++.

## First song

1. Choose a genre, key and progression. Click **GENERATE SONG**.
2. Select an instrument/section cell. Edit its notes in the piano roll.
3. Use **NEW TRACK**, **NEW SECTION** or **NEW CLIP** to try another take. Lock parts you want to keep.
4. Assign an internal sound, **LOAD SAMPLE**, or **LOAD VST3 / AU**. Use **OPEN VST** for the plugin's own controls.
5. Save a `.fga` project, then use **EXPORT** for MIDI, WAV or MP3.

Generation settings affect the **next generation**. Existing editable MIDI stays as written when key, harmony or genre controls change. Tempo updates playback immediately. Apply a new harmony to all unlocked tracks together to keep them in tune.

## Guides, privacy and scope

- [Guida italiana](docs/GUIDA.md) · [English guide](docs/GUIDE.md)
- [Privacy e permessi](PRIVACY.it.md) · [Privacy](PRIVACY.md)
- [Architecture and source provenance](docs/ARCHITECTURE.md)
- [Third-party notices](THIRD_PARTY_NOTICES.md) · [License](LICENSE)
- [Validation record](docs/VALIDATION.md)

Version **0.1.0** is an initial MIDI arranging workstation. It does not yet replace every function of a general-purpose DAW: audio recording/editing, automation lanes, audio warping, effect chains, buses/sends, MIDI controller recording, plugin delay compensation and sandboxed plugin scanning are not implemented. Native plugins run in the application process and must match the application's architecture. Arbitrary third-party plugin compatibility needs testing with the actual plugins.

The original guide instruments are synthesized audition sounds; they are not sampled commercial libraries. Use your own instruments for the final production. WAV is the lossless export; MP3 is intended for sharing and adds codec delay/padding.

## Ecosystem

[Fase Groove Piano](https://github.com/wsx79/fasegroovepiano) · [Fase Groove Drums](https://github.com/wsx79/fasegroovedrums) · [Fase Groove Guitar](https://github.com/wsx79/fasegrooveguitar)

[Ko-fi](https://ko-fi.com/fabios1979) · [PayPal](https://paypal.me/fabios79) · [Spotify](https://open.spotify.com/artist/29iQGuXML8LEw3xRp1dNRP)
