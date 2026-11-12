# PHASE 03 — Audio engine reliability


Inspect AudioEngine.cpp/.h, Handoff, plugin lifecycle, PDC and tests. Stress rapid updates, play/stop/seek, loop, mute/solo, audio take playback, voice render integration, buffer sizes 32/64/128/512/1024, 44.1/48/96 kHz, plugin swaps and export while playing. Reproduce actual defects before patching. Keep callback realtime-safe, preserve `.fga` compatibility, add focused regression tests. Do not touch genre generation or guide sounds. Commit only after tests; update status. Do not anticipate subsequent phases or push/tag/release without authorization.
