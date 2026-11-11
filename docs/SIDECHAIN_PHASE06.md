# Phase 06 — existing group sidechain

Baseline: `e637d84bad34b21a40e7a1ac7119057dac7edfa5`, branch Vocaltest.

## Targeted inventory and scope

`MixerBuses` already implemented group A/B depth, maximum-absolute stereo kick detection (track ID 4, scale 6, ceiling 1), instant attack, 180 ms exponential decay, group routing/solo/mute and offline reuse. It was not necessary to replace it. No preexisting sidechain DSP defect was reproduced; this phase extends missing controls and tests. The phase document's mistaken “05” heading is corrected to 06.

Each group now has an independent envelope, source and depth. The existing Bus & Send mixer adds attack 0–200 ms, release 10–2000 ms and curve 0.25–4. Curve is an envelope exponent; 1 is the historical response. Attack/release are time constants in milliseconds, not tempo divisions or hold times. Release keeps the historical decay-to-zero law, not a compressor's envelope follower toward the current detector level. Depth changes during playback ramp over 10 ms, using the mixer's existing smoothing convention. A stopped edit snaps. Changing source/timing/curve while playing can change the response immediately; no crossfade or click-free promise for these discrete edits.

Defaults remain source Kick, attack 0 ms, release 180 ms, curve 1, depth 0. The static legacy sample law is checked exactly, including float rounding of the release coefficient, at 44.1/48/96 kHz and blocks 1/31/128/511. The depth ramp is an intentional change for live edits, not a change to saved depth or steady-state response.

## Routing and compatibility

Select one of the 18 existing generated track slots, whose IDs are stable. Manual/vocal track selection, external inputs, group/master feedback routes and plugin sidechain buses are not added. The source tap remains after track inserts/EQ/PDC, track gain/pan/volume automation and track mute/solo gating, before group gain/mute/solo and sends. Group mute can therefore leave the detector active; the UI explains this inherited behavior. A soloed target excludes an unsoloed source. An absent source clears the detector buffer each block and the envelope decays naturally. A stem does not secretly render other tracks; the missing source does not trigger it. No latency, PDC or transport semantics are added.

Optional scalar fields on existing bus JSON objects: `sidechainSource`, `sidechainAttackMs`, `sidechainReleaseMs`, `sidechainCurve`. Existing `sidechainAmount` is retained. Missing fields use exact legacy defaults; malformed values are finite/bounded before source conversion. Existing schema/version and MIDI/generators are unchanged. Older applications ignore new fields and cannot reproduce nondefault source/timing/curve: use the current build to reproduce these settings. Undo/Redo uses existing project snapshots.

## Realtime and performance

Two detector buffers are prepared with the existing block buffers off the callback. Compared to one historical buffer the added PCM storage is `4 * blockSize` bytes (2,048 bytes at 512 samples); other additions are fixed-size envelope, smoothing and atomic-meter state. No new dependency. Envelope coefficients are computed on mix adoption, not per sample. Non-unit curves add up to two scalar powers per stereo frame. Reduction is stereo-linked and its block maximum is published as an atomic linear reduction; the existing 15 Hz UI timer converts it to dB (display floor 120 dB).

Processing/configuration are bounded and introduce no filesystem access, blocking locks or buffer allocation. The test-only regular C++ allocation counter covers active groups, curve and live engine mix adoption; it does not instrument every aligned/system allocation. Existing mixer delay/reverb and engine handoff are reused. No change to NativeCompressor, Transparent export, Vocal Studio/Chops or musical generators.

## Verification

Final Linux Release build PASS; focused CTest **4/4 PASS, 59.00 s** (`project_io` 4.25 s, `audio_engine` 46.85 s, `sidechain` 5.93 s, `dynamics` 1.96 s). Mixer UI under Xvfb PASS, including Undo/Redo of new controls, unchanged MIDI and minimum/default layouts; the final screenshot was visually checked. No final FAIL. The new test uses a frozen mathematical legacy reference rather than changing the expected result to match the implementation. The export fixture uses actual native sample playback, a muted source group, stems and track mute/solo. GUI verification reuses the existing mixer harness, checks new controls with Undo/Redo and MIDI preservation, and checks nonempty in-bounds controls at default/minimum window sizes.

Windows/macOS, listening, real audio hardware, dropout behavior, external plugins and ASan/TSan remain **NOT RUN**. Historical phase-05 and export results are not represented as new tests. No phase 07, push, tag or release.

### Numeric results and reproducibility

- Legacy sample-exact reference: **12/12 PASS**, 44.1/48/96 kHz × blocks 1/31/128/511, positive/negative stereo hits, no-source decay and live tempo changes 60/240 BPM.
- New controls: 20 ms attack time constant, 100 ms release with squared curve, independent sources/groups, depth ramp, zero/nonfinite/out-of-range inputs, reset, mute/solo/stems and atomic reduction values PASS.
- Optional-field roundtrip, legacy defaults and malformed/large metadata PASS. No schema/version migration.
- Native WAV/live source fixture: **ducked/dry RMS 0.52003**, expected 0.52 (constant-detector historical retrigger/decay explains the small difference). Selected track with pan automation follows the same detection path. Track mute/solo remove its signal; muting its group does not. The isolated target stem lacks the other source and is unducked.
- Active mixer/configuration and new-control adoption on the actual audio callback: **0 regular C++ allocations** in measured scopes.
- Local Release benchmark, 48 kHz/block 512, two active groups and one non-unit curve, including existing delay/reverb/add/configuration: **1,024,000 stereo frames / 128.24 ms wall time**, approximately 0.60% of the represented audio duration. This is one container observation, not CPU utilization of the DAW, a before/after comparison, hosted-plugin performance, hardware or dropout validation. Detector buffers total 4,096 bytes versus 2,048 historically; other additions are fixed-size state. Added algorithmic latency **0 samples**.

The first build of the new harness lacked its ProjectIO include; corrected. An initial ramp assertion incorrectly expected exactly zero output under a constant detector, overlooking the legacy alternating retrigger/decay; corrected to the frozen legacy coefficient, without changing DSP. Visual review caught and removed an overlapping PAN/GR label introduced by the new UI. All final checks above passed after these corrections.

Reproduce (configured Release build):

```sh
cmake --build build/native -j2
ctest --test-dir build/native -R '^(sidechain|project_io|audio_engine|dynamics)$' --output-on-failure -j1
"build/native/FaseGrooveArranger_artefacts/Release/Fase Groove Arranger" --sidechain-ui-test /tmp/fga-sidechain-ui
```

GUI command needs a display (tested with Xvfb). This container also needs the existing local sysroot/environment setup; logs: `/workspace/scratch/phase06-release.log`, `phase06-ctest.log`; detailed benchmark output in `build/native/Testing/Temporary/LastTest.log`; final UI evidence in `/workspace/scratch/sidechain-ui-verified`. Unrelated CTest targets and the old full export/vocal corpus were not rerun: no changes to those modules justify repeating their audits. The relevant audio-engine/dynamics regressions were rerun because their shared render path changed.

## Manual check (pending)

Open Bus & Send; route a sustained target to GROUP A. Keep Kick audible at track level, set group depth around 0.5, and play. Confirm ducking and GR. Compare instant/20 ms attack, 180/400 ms release and curve 1/2. Choose another generated source; verify that track mute/solo removes its trigger, while muting its group does not. Set depth to zero and check Undo/Redo. Save/reopen the project, then compare the WAV mix export with playback. Exporting only the target intentionally excludes the other source. Listen for clicks or unexpected pumping; this listening check has not been performed here.
