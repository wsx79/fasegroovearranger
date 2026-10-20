# Credits, licences and third-party notices

## Fase Groove ecosystem

Copyright (C) 2026 Fabio 'Pitch' Serra / FS Pitch. Application and original generation engines: **GNU Affero General Public License version 3 or later**, full text in `LICENSE` and in the original source folders.

Creator: **Fabio 'Pitch' Serra — DJ & Producer**. Development assistance: **Claude (Anthropic)** and **Codex (OpenAI) — AI coding assistants**. The original FS PITCH artist logo, Inter theme and artist/donation links are retained. Source provenance and licences for each component are listed below.

Original sources, copied with copyright and licence notices:

- https://github.com/wsx79/fasegroovepiano at `026f0451d683f25520409f8dd273ff5a4496cad0`
- https://github.com/wsx79/fasegroovedrums at `7f1def6a4e4cb72ed887f7754171affec12af001`
- https://github.com/wsx79/fasegrooveguitar at `e571f8d02621c65adeb35207214771571499fc65`

See `ThirdParty/*/LICENSE`, original notices and `docs/ARCHITECTURE.md`. The original third-party documentation is retained for provenance; this app's privacy and guide are the documents in the repository root and `docs/`.

## JUCE

JUCE 8.0.9 — https://github.com/juce-framework/JUCE. Copyright JUCE contributors / Raw Material Software. JUCE is available under AGPLv3 and commercial terms. This source distribution uses the open-source AGPL option. JUCE includes its own platform/component notices, available in the fetched/local JUCE source. Keep the corresponding source and notices when distributing binaries.

## Inter

Inter Regular and SemiBold — Copyright The Inter Project Authors. **SIL Open Font License 1.1**, full text in `Resources/Inter-OFL.txt`. Fonts are embedded without modification.

## Shine MP3 encoder

Shine — https://github.com/toots/shine at `ab5e3526b64af1a2eaa43aa6f441a7312e013519`. Original work/history credits include Gabriel Bouvigne, Pete Everett, Patrick Roberts and the Shine contributors; see `ThirdParty/shine/README.md`, `README.old` and source files. The library's full licence is the **GNU Library General Public License version 2**, in `ThirdParty/shine/COPYING`. Its source is included; it is linked into the AGPL application. The complete application source and build instructions allow rebuilding/relinking with a modified library. Do not remove these notices or restrict the user's applicable library modification/relinking rights.

Shine is a simple fixed-point encoder without a psychoacoustic model. The app uses 320 kbps stereo for sharing; WAV is the lossless production format.

## Trademarks and user content

VST is a registered trademark of Steinberg Media Technologies GmbH. Audio Units, macOS and Windows belong to their respective owners. Naming a format does not imply affiliation or endorsement.

No proprietary plugin binary, commercial sample library or third-party musical recording is bundled. Users provide and license their own instruments and samples. Generated MIDI/audio is not required to be AGPL merely because it was produced by this application; rights in any user-supplied content continue to apply. The software is provided without warranty as set out in its licences.

The six-language UI dictionaries and CircleOfFifths component are adapted from those same family snapshots, under their original AGPL notices. The guitar step engine has an Arranger-specific shared harmonic-duration adapter; the 16-step default retains the plugin behaviour.
