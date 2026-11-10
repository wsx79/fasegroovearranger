# Engineering contract — low-token, clean-code Codex workflow

## Token discipline
- Search symbols/paths first (`rg -n`, `rg --files`); open only relevant ranges, never dump entire large files.
- Avoid repeating README, changelog, full code, full diffs, or test logs in conversation.
- Do not re-read unchanged files; rely on focused diff and status. Read `02_STATUS.md` on session resume.
- Report concise findings with exact `path:line` references, no speculative feature inventories.
- Use existing tests/build scripts rather than creating redundant harnesses.
- Do not scan unrelated FaseGroove repositories unless integration actually requires it.

## Clean code and audio safety
- C++17/JUCE conventions already in repo; prefer minimal coherent changes and separation DSP / UI / persistence.
- Reuse existing modules; avoid duplicated EQ, mixer, audio player, piano roll, project format, and generic frameworks.
- No allocations, blocking locks, filesystem/network access, or model inference on realtime audio callback.
- Prepare buffers off audio thread; deterministic, bounded processing; explicit cancellation and lifecycle handling.
- Treat NaN/Inf, clipping, sample-rate conversion, latency/PDC, tails, silence, plugin failure, device changes and memory usage.
- Keep old projects and existing MIDI playback unaffected; optional new fields must have safe defaults.
- No new third-party dependency/model without inspecting license, maintenance, platform availability and packaging impact.
- Protect privacy: do not upload user vocals, lyrics, project audio or API keys without informed opt-in.
- No unauthorized voice cloning or impersonation workflows; use properly licensed voicebanks and explicit consent for user-supplied voice data.

## Workflow per substep
1. **Inventory:** exact existing components and behavior, with evidence.
2. **Reproduce/specify:** failing regression or acceptance test; if feature new, acceptance criteria first.
3. **Implement:** smallest safe change, no unrelated refactors.
4. **Verify:** focused tests then available broader suite; record PASS/FAIL/NOT RUN accurately.
5. **Document:** short status and changelog entry; commit after successful verification.

## Stop conditions
- Unclear voice-engine license, incompatible model distribution, mandatory external service not approved, major format migration, destructive changes, or irreproducible serious regression: stop and ask for decision, offering 2-3 concrete options.
- If test environment cannot validate a critical platform, mark it unverified, not passed.

## Final response format (<=200 words)
Current phase/substep; confirmed findings; files changed; tests PASS/FAIL/NOT RUN; known risks; commit SHA (or none); next decision needed.

## Vocal scope
Do not confuse sample-based Vocal Chops with lyrics-to-singing Vocal Studio. Both must coexist; do not merge or rewrite existing chops when implementing singing.
