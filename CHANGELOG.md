# Changelog — Fase Groove Arranger

## 0.5.1 — 2026-10-21

- Click anywhere in an arrangement clip to place playback precisely; Play/Space starts there, and playback already running jumps immediately. Works after horizontal scrolling.
- Arrangement and MIDI rulers retain fractional beat positions. Seeking beyond an enabled loop continues from the chosen position; seeking back into it resumes looping.
- Native regression checks cover paused/live seeking, scrolling, loop exit/re-entry and starting inside held MIDI notes.

## 0.5.0 — 2026-10-21

- DAW workspace with side inspector, seventeen-channel mixer and master, dB faders, pan, mute/solo and master overload indicator.
- Bar ruler, Shift-drag loop locators, clip selection, double-click full MIDI editing, drag swap/copy, fit-to-song and playback follow.
- Fix loop persistence and section edits, automation timing on section resize, stale meters on pause, grouped MIDI movement at boundaries, dropped-file targeting, failed Save As changing the active path, cross-project undo history and slider keyboard edits missing undo.
- Faders update the audio mix without reordering or regenerating the entire MIDI on every movement. Updated IT/EN guides, tooltips and six languages; original family theme, fonts and instrument panels retained.

- Smooth live volume/pan transitions over 10 ms to avoid abrupt gain steps while moving faders.

- Handle disabled audio outputs without null-pointer access; newly assigned drum samples use the track MIDI mapping as their initial root note.

## 0.4.7 — 2026-10-20

- Jackin’: original-library chopped hooks, more articulated Wurlitzer bass, strings off by default and alternating percussive guitar in the requested funky/disco direction. Other genres remain unchanged.
- Audit includes full Jackin’ song and separate drum, bass and keyboard previews; four-seed regressions and user-enabled strings checked.

## 0.4.6 — 2026-10-20

- Correct drum balance for the original engine’s existing lane calibration: snare, clap and cymbals are no longer attenuated twice.
- New-project master at 0.45 and true-peak verification of all thirteen MP3 previews alongside full-song PCM checks.

## 0.4.5 — Genre sound palettes and band balance

- Thirteen curated palettes using the original keyboard, guitar and drum guide engines: genre-specific sounds, kit, tone, voicing, gate, effects, levels and instrument roles.
- Dedicated guide bass stays centred instead of inheriting keyboard note panning; low-pass follows its tone. User samples and VSTs bypass this guide processing.
- Piano/guitar answers and phrase-based string/percussion entrances reduce constant layering. User patterns, locked material, routing and manual MIDI remain editable.
- Full-song audit plus thirteen eight-bar MP3 previews and stereo bass checks.

## 0.4.4 — Family guide alignment

- Use the Piano manual's nine-chapter structure, language link, centered logo/screenshots, separators, control tables, recipes, FAQ style and creator/music closing in both manuals.
- Preserve the full Arranger instructions for instruments, MIDI editing, genre-specific forms, routing, automation and export inside the corresponding family chapters.
- Refresh screenshots from the verified 0.4.3 app. Native offline reader supports the original Markdown's centered image markup, italics and nested headings/lists.

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
