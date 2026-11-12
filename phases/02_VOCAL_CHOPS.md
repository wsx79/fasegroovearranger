# PHASE 02 — Vocal Chops (sample-based vocal instrument)

## Goal

## 02A — Audit first, no rewrite
- Locate existing chop editor, slicing, transient detection, sample handling, pitch/time processing, MIDI mapping, project persistence, preview, mixer routing and export.
- Document actual implemented features with file references; do not rebuild them.
- Identify only confirmed bugs, usability gaps and DSP quality limitations, reproducing each where possible.
- Check licensing and privacy of any supplied samples; never fetch unauthorized copyrighted vocal samples.
- STOP and present a concise proposed improvement list for approval before major changes.

## 02B — Targeted improvements after approval
- Preserve original audio and non-destructive slice markers.
- Precise beat/bar snapping, 125 BPM and tempo-change behavior; one-shots and loop mode.
- Stable pitch-shift/time-stretch quality, with CPU budget and offline high-quality render when available.
- MIDI/pad triggering and reproducible chop patterns where missing; avoid duplicate sequencers.
- Fade/crossfade at cut points to avoid clicks; silence, zero-length slice, reverse, loop boundaries.
- Existing track, effects, automation, mute/solo, undo, project save/reopen, stems/export.
- Ensure audio callback remains realtime-safe; avoid excessive sample copies.

## Acceptance
- Original source is unmodified; project reopen reproduces edits.
- 8-bar 125 BPM chopped pattern stays on grid, including seek/loop and tempo change.
- No clicks at cut points in tested fixtures; no crashes for invalid/empty files.
- Compare CPU, memory, output quality with baseline; document test coverage and listening checks.

## Delivery
Small focused commits with tests, updated `02_STATUS.md`, concise report. No push/tag/release without approval. Proceed to audio engine only after user approval.
