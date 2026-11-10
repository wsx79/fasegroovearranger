# Codex persistent state (keep brief)

- Last verified baseline by planning assistant: v0.43.2; re-check current HEAD.
- Current phase: 01 VOCAL STUDIO
- Current substep: 01A DISCOVERY / DECISION GATE
- Status: NOT STARTED
- Product decisions: voice first; existing musical generation satisfactory; guide sounds remain guides.
- Confirmed changes: none.
- Tests executed in this plan: none.
- Open decisions: vocal engine architecture, licensed voicebanks, local vs opt-in service, supported languages, minimum target platforms.
- Next action: read `phases/01_VOCAL_STUDIO.md`, perform **01A only**, return recommendation and wait for authorization before implementing 01B.

Update after every authorized substep. Keep <=35 lines; archive lengthy evidence in targeted docs only if necessary.

## Vocal architecture clarification
- Phase 01: Vocal Studio = synthesized singing from lyrics and MIDI.
- Phase 02: Vocal Chops = edit/sequence user-supplied vocal audio; inspect existing implementation first.
- These are two distinct engines/workflows.
