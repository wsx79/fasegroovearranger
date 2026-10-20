# Changelog — Fase Groove Arranger

## 0.4.3 — Track volume automation

- Optional volume lanes on all 17 tracks: draggable points, beat snap/free timing, curved segments, bypass, clear and undo/redo. Hidden and bypassed by default; hiding never mutes automation.
- Sample-accurate post-instrument gain for internal engines, user samples and VST/AU; shared playback and mix/stem WAV/MP3 export graph. MIDI notes/CC stay unchanged.
- Optional project fields preserve envelopes; old projects default to bypass. Section insertion, duplication, deletion and reordering move points with the musical material.
- Remove arranger-added note-velocity fades in intro/outro and organic lifts. Preserve original engine articulation and expression.
- Six-language controls/tooltips and adapted Markdown manuals. Core, persistence, native editing gestures and audio-envelope checks.

[Italiano](CHANGELOG.it.md)

## 0.4.2 — 2026-10-20

- Prevent guide-mix clipping in new songs with 6 dB more master headroom; audit raw peaks before encoding and reject non-finite audio.

- Club arrangements: progressive mix-in/out inside the song, working BPM ranges by genre, and groove-preserving soulful/organic lifts.
- Keep each song’s motif across sections; preserve the DJ phrase grid and harmonic continuity with odd chord counts.
- Correct sub-tick outro spill, factory grooves left over after a genre change, and inconsistent normalisation in MIDI/audio exports.
- Full-song MIDI/audio audit for all thirteen genres, reference Soulful MP3 and source-based arrangement notes.

## [0.4.1] — Unreleased

- Restore the Guitar family OPTIONS overlay: SETTINGS / ABOUT geometry, controls, links and credits.
- Add interface-size preference and optional GitHub release checks; preserve audio, guide, privacy and licence access.
- Reuse original IconButton transport, undo/redo, arrows and dice; align instrument grouping and control proportions with reference editors.
- Separate Branding as in the other apps and introduce an original track/clip symbol for Arranger.
- Add Italian README and Markdown changelogs; remove the separately invented HTML guide export.

## [0.4.0] — Previous development build

- Standalone Piano, Drums and Guitar engines, multitrack arrangement, MIDI editors and MIDI/WAV/MP3 export.
- Extended instrument panels, Piano musical tools, VST3/AU hosting and `.fga` projects.
