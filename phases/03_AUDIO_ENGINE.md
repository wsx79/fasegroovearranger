# PHASE 03 — Audio engine reliability

User decision, 2026-11-11: phase 03 is authorized while functional/quality approval of phases 01/02 remains pending. Further work and final review of Vocal Studio/Chops are deferred; preserve their code. This explicit decision supersedes the former phase-01-approval prerequisite, without approving either vocal function.

Inspect AudioEngine.cpp/.h, Handoff, plugin lifecycle, PDC and tests. Stress rapid updates, play/stop/seek, loop, mute/solo, audio take playback, voice render integration, buffer sizes 32/64/128/512/1024, 44.1/48/96 kHz, plugin swaps and export while playing. Reproduce actual defects before patching. Keep callback realtime-safe, preserve `.fga` compatibility, add focused regression tests. Do not touch genre generation or guide sounds. Commit only after tests; update status. Do not anticipate subsequent phases or push/tag/release without authorization.
