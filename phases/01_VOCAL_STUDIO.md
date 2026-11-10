# PHASE 01 — Sung Vocal Studio (highest priority)

**Separate system from Vocal Chops:** this phase creates original singing from lyrics and MIDI. It must NOT replace, merge, or reimplement the existing sample-based Vocal Chops workflow. Vocal Chops has its own Phase 02. Both share existing DAW tracks, transport, mixer and export only where appropriate.


## Goal
Enable a user to create an **original sung vocal performance** from lyrics + MIDI melody within FaseGroove Arranger, edit it musically and render to an editable audio track/take. This is **not** merely vocal chops, generic text-to-speech, or a replacement of existing instrument sounds. Voice must follow the authored notes and timing; user controls where singing starts/stops, including DJ intro/outro silence.

## Scope and boundaries
- Inspect existing piano roll, MIDI editing, vocal chops, AudioTake/audio recording, track system, project serialization, automation, sample-rate conversion, bounce/export, transport and UI before proposing code.
- Preserve current MIDI, arranger, harmony and genre behavior. Vocal Studio is additive and opt-in.
- Support original lyrics and licensed synthetic voices; no unauthorized celebrity/artist imitation or cloning.
- Avoid imposing subscriptions, cloud keys or proprietary redistribution terms without user approval.
- Audio rendering/inference **must never run on realtime callback**.

## 01A — Discovery and decision gate (DO THIS FIRST, NO IMPLEMENTATION)
1. Inventory existing capabilities and reusable classes (`path:line`). Identify missing primitives only.
2. Evaluate at least 2 realistic singing-synthesis approaches: e.g. local licensed phoneme/voicebank engine, optional remote service with consent, or compatible model/runtime integration. **Verify actual current availability, licenses, supported languages, model redistribution rights, CPU/RAM/GPU needs, Windows/macOS/Linux portability, latency, offline rendering, phoneme/timing controls and voice quality.** Do not assume a named model is usable merely because it exists.
3. Propose recommended architecture with adapter interface only if two concrete implementations justify it; otherwise prefer simple direct integration.
4. Specify a feasible MVP, dependencies, license and packaging risks, language coverage, benchmark plan, estimated complexity.
5. Provide 2-3 options with tradeoffs and ask for user approval of engine/voicebank approach. **STOP after 01A.**

## 01B — Project data and UI MVP (after approval)
- Vocal clip linked to notes and lyric syllables/phonemes, with stable IDs and timeline position.
- Editor for lyrics, syllable-to-note assignment, melisma (one syllable over multiple notes), rests, pronunciation override where supported.
- Use existing piano roll for pitch, onset, duration, velocity; no second incompatible piano roll.
- Explicit vocal regions: **no voice in DJ intro/outro by default**, but user can override; preserve manually authored notes and edits.
- Voice selection and model capability display; clear errors for missing voicebank/model.
- Non-destructive project persistence and backward compatibility; Undo/Redo and save/reopen tests.

## 01C — Render pipeline
- Asynchronous cancellable offline render with progress and bounded resources.
- Correct tempo/beat alignment, sample-rate conversion, rendered start offset, clip tails, and waveform caching.
- Preview, re-render only affected vocal regions, cache invalidation on lyrics/notes/voice/settings change.
- Bounce result to existing audio-track/take system; route through existing mixer, insert FX, automation and export/stems.
- Never embed generated audio blobs directly into project JSON; use existing asset/collect/relink strategy.
- Avoid leaking user lyrics/audio to services; opt-in if remote.

## 01D — Expression and musical controls
- Pitch bend/portamento, vibrato, dynamics, articulation and breath/phrase boundaries **only where supported by selected engine**.
- Multiple vocal takes and harmonies as independent editable tracks if feasible without architecture duplication.
- English and Italian lyrics prioritized, but do not claim either supported without verifying engine/voicebank.
- No automatic rewriting of lyrics or harmony without user action.

## Acceptance tests
- 8-bar 125 BPM melody with lyrics renders aligned to MIDI, including rests and held notes.
- 16-bar DJ intro and outro remain silent vocally by default; explicit overrides work.
- Edit one note/word and re-render: correct invalidation, unchanged unrelated clips.
- Save/reopen old and new `.fga` projects; missing model/asset fails gracefully.
- Undo/Redo, mute/solo, transport seek/loop, offline WAV/stem export and routing remain correct.
- Repeated cancel/retry, device/sample-rate changes, long lyrics and invalid input do not crash or leak.
- No audio callback inference, allocations or blocking work introduced.
- Audio listening tests required before claiming natural voice quality. Separate technical PASS from subjective quality.

## Deliverables
- 01A: short architecture decision memo, no code.
- 01B–D: small testable commits, concise changelog, UI/user documentation, test fixtures and updated `02_STATUS.md`.
- Do not proceed to Phase 02 until Vocal Studio MVP is demonstrated and approved.
