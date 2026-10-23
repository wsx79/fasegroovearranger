# Fase Groove Arranger: DAW audit (v0.13.0, branch `daw`)

The scope is the standalone arranger: 17 fixed tracks (piano, bass, strings, guitar and 13 drum stems). They are fed by the three vendored Fase Groove engines and played by built-in guide sounds, a sampler, or a VST3/AU instrument with one insert effect.

## 1. Engine integration

- **Re-vendored** `ThirdParty/*/Source/engine` from Piano 0.31.0, Drums 0.20.0 and Guitar 0.10.0. This was a three-way merge against each file's last synced upstream version, so the arranger's own adaptations are kept:
  - piano `bassLineOverride`;
  - guitar `stepsPerChord` (shared harmonic rhythm);
  - drums Jackin' reinforcement and section rules;
  - the arranger trimmings (no edited-loop/`carryEdits`/`makeRandomLane` code).
- **Fixes now included:**
  - guitar seed hashing, so neighbouring seeds no longer play near-copies, plus the new genre role weights;
  - piano `shapeGenreRiff` (genre riffs no longer repeat every 64 seeds, and the bass dice now change the riff) and gospel walks;
  - drums: the Jackin' turnaround plays only in Main/Drop, section variations no longer remove the offbeat hat, the closed-hat jack branch can now be reached, and the larger `pending` reserve.
- **New arranger adaptation:** piano `separateBass`. Piano and bass are separate tracks, so a pitch they share no longer cuts or steals the other's note.
- **Interfaces:** the APIs are unchanged. The engines run only on the message thread (`generate`). The audio thread plays stored notes, so the per-block string building in the plugins (their known realtime issue) never reaches arranger playback.
- **Shared timing and harmony:** BPM, key, chords/progression (`chordSteps`, `chordCount`, `degrees`, `freeChords`), bar grid and section types are all taken from `Project::settings`. Seeds follow `riffSeed` > clip `motifSeed` > song seed.
- **Compatibility:** saved projects keep their stored notes and play the same. Only regenerating (a new or zero seed) uses the new engine output.

## 2. DAW functions that exist (verified in code and smoke evidence)

| Area | Implemented |
|---|---|
| Timeline | Ruler with sections (rename, duplicate, move, resize, type), zoom (wheel, +/-, slider, FIT, ZOOM SELECTION), grid/snap, FOLLOW, resizable track heights |
| Clips | Section clips and free-timeline clips: split, cut/copy/paste at cursor, duplicate, delete, multi-select on one track, drag/copy across tracks (drum remap), loop-extend, lock |
| MIDI editor | Piano roll, drum lanes, guitar lanes/notes; draw/select/erase, marquee, move/resize, velocity lane, quantise, ±12, humanize, copy/paste/cut, Ctrl+D, arrow nudge |
| Transport | Play/pause/stop/rewind, precise seek and chase, loop region (button, selection, section), playhead beyond the song end; **metronome (new)** |
| Mixer | Gain, pan, mute, solo, meters, master with clip indicator, smoothed faders, volume automation with curves |
| Instruments | Guide sounds, sampler, VST3/AU instrument plus one insert effect per track, plugin editor, out-of-process catalog scan, missing-plugin handling |
| Files | `.fga` atomic save, autosave every 60 s, recovery; MIDI import into a clip; export of MIDI (song, track, separate tracks) and WAV/MP3 (mix, track, stems) |
| Undo | 40-step project snapshots, with **plugin and effect states (new)** |

## 3. Fixes made in this pass

**A: bugs**
1. Plugin notes hung after stop. Note-offs were queued but never delivered while stopped. Every sounding note is now tracked, explicit note-offs are sent on stop, seek and loop, and they are flushed to plugins while stopped.
2. Every edit during playback hard-reset all 17 instruments, cutting and retriggering every note and tail. Now only the edited tracks resync, and only the notes whose MIDI changed are released or restruck. Regenerate no longer stops playback.
3. Loop wrap and seek during playback reset all DSP (reverb, chorus, voices), which clicked at every loop. They now release notes with their natural tail.
4. Silent blocks while editing: `publish`/`updateMix` (on every fader move) and plugin-state capture (save, autosave, undo) held the engine lock that the audio thread only try-locks. The mix and the schedules are now handed over lock-free (`Handoff`: an atomic pending/retired pair, never freed on the audio thread), and plugin states are read without the lock.
5. A mono plugin played only on the left channel. Fixed.
6. A plugin with more than 32 channels allocated on the audio thread. The scratch buffer is now resized in place.
7. Note extraction:
   - per-voice note-off pairing;
   - humanised notes a hair before a section end are dropped instead of left as 1 ms stubs;
   - intro entrance hits snap to the phrase line;
   - unisons inside a strum, or a chord plus a fill, become one attack (guitar double attacks). Drum flams are kept.
8. Recovering the autosave made it the project file, so SAVE overwrote the autosave slot. Fixed.
9. Undo after changing a plugin restored a stale plugin state. Fixed.

**B: completed**
- MP3 export is gapless: Xing/LAME Info frame with encoder delay and padding, so the decoded length equals the WAV exactly (checked with ffmpeg).
- A `.fga` given on the command line or to a second instance now opens.
- Closing asks Save / Discard / Cancel (it used to offer only Discard / Cancel).
- The playhead info sent to plugins now includes bar start, bar count, loop points and samples.

**C: added**
- Metronome (C, Audio menu; live only, never exported).
- **L** toggles the loop.

## 4. Missing functions, by priority

| P | Function | Note |
|---|---|---|
| 1 | Plugin delay compensation | Insert-effect latency is ignored. Read `getLatencySamples()` and delay the other tracks. |
| 1 | Pan / plugin-parameter automation | Only volume automation exists. Extend `VolumeAutomation` into generic lanes. |
| 2 | Sends, buses, return FX, more than one insert | Needs a small routing graph (an `AudioProcessorGraph` per track or a fixed send bus). |
| 2 | MIDI input: audition and record | No MIDI input device handling yet. |
| 2 | Multi-track clip selection, clip mute and gain | Selection is limited to one track. |
| 3 | Audio tracks and audio import | The architecture is MIDI-only. A large change: plan it separately. |
| 3 | Tempo map, time signatures other than 4/4, markers, count-in | The project model assumes 4/4 and a single BPM. |
| 3 | Recent files, project templates, freeze/bounce in place, MIDI clock out | Convenience items. |

## 5. UI components to keep for the restyling

Arrangement view (ruler, section guide, clip lanes, automation lanes, track-height handles), piano roll and drum/guitar lane editors, inspector, mixer strips and master, transport cluster, plugin browser and catalog, instrument panels and piano tools (circle of fifths, coach), guide pages, and the options and update overlays.

## 6. Obsolete or duplicate controls (candidates)

- The timeline buttons CUT, COPY, PASTE, SPLIT, DUPLICATE and DELETE repeat the shortcuts and the clip menu.
- Several loop entry points: LOOP, LOOP CLIP, LOOP SELECTION and the section menu's "Loop section".
- INSTRUMENT, OPEN VST, LOAD SAMPLE and LOAD VST3 / AU overlap with the plugin browser.
- The inspector gain/pan sliders repeat the mixer strip.
- `sectionSelector` and `panelSections` are both section pickers.
- The zoom slider repeats +/-, the wheel and FIT.
- NEW TRACK, NEW SECTION, NEW CLIP, GENERATE SONG, NEW CHORDS and NEW ARRANGEMENT could become one Generate menu.

## 7. Constraints for the restyling

- Keep the persistent IDs: track storage order (`trackAtRow`), the `.fga` JSON keys, the plugin `instrumentId`/state and the 17-track layout.
- The smoke tests drive controls by name and position: `writeUiEvidence`, `verifyTimelineEditing`, `verifyNavigation`, `verifyDawWorkspace` and `verifyPianoTools`. Update them with any layout change.
- Every edit must go through `checkpoint()` and then `changed()`/`mixChanged()`. Never call the engine lock from the UI for mix or notes.
- Every text goes through `tr()`, with entries in `ArrangerTranslations.inc` (6 languages).

## 8. Residual issues

- Instrument and effect loading, sample loading and export still take the engine lock. Playback is silent for that moment (by design).
- Changing a plugin's state through undo calls `setStateInformation` from the message thread while the plugin plays. This is the standard host contract, but a badly written plugin may glitch.
- Pausing silences at once, with no tails while stopped. Starting playback clears the old tails.
- Undo snapshots copy the whole project, plugin states included (40 levels). Very large plugin states increase memory use.
