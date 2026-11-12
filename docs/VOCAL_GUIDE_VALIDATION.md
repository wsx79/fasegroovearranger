> **Documento storico del backend rimosso.** Non descrive dipendenze o funzioni
> attive di Vocaltest. eCantorix/eSpeak e i relativi helper non sono più eseguiti
> né distribuiti. [Stato corrente di Vocal Studio](VOCAL_STUDIO.md).

# 01C validation notes

Linux, 2026-11-10; branch Vocaltest; baseline ac8744a, v0.43.2.
No Windows/macOS machine was available. No live DAW listening session was performed.
Build/test artifacts and third-party tools are outside tracked source.
Final review results: Linux full app/test build PASS; native CTest 13/13 PASS
(87.26 s), including core, IO, updater, audio engine, real vocal guide and browser;
real editor 22/22 PASS; piano roll 12/12 PASS,
including its existing asynchronous single-track WAV export. No final FAIL.
Earlier GUI attempts had an inactive virtual display (killed); restored using a
live foreground Xvfb session. The first export assertion checked an unsuffixed
filename; the flow had correctly written the BPM/key-suffixed WAV. The test now
uses exportNamedFile and passes; the export implementation was not changed.
Full host/UI-audit and sanitizer suites were not run. Logs/artifacts for the final
review are in /workspace/scratch/01c-final-review.

## Cache investigation

The previously archived prototype's `cached guide reused` FAIL was rerun with
separate result/path/timestamp diagnostics. It passed with the restored source;
its original assertion did not retain the component that failed. Therefore the
historical failure's exact cause is **not proven**, and is not labelled resolved.

A distinct reproducible defect was established: e.g. changing a note end by
0.0001 beat leaves the 960-PPQ MIDI identical while changing the required WAV
frames and exact silence mask. The original MIDI-only cache identity produced
one key for both plans; duration validation then forced alternations to overwrite
that shared file. Regression coverage verifies identical MIDI, distinct exact
durations/keys, and successful reuse of the original WAV after the edit.

The new key includes exact duration and audible intervals, helper/Util hashes,
runtime versions, and default English voice data. Final WAVs have checksum
sidecars; malformed/damaged entries are regenerated. A hit is observed explicitly,
and its file is backdated before ten repeat checks: an accidental rewrite cannot
pass by happening within the same filesystem timestamp tick. Do not infer that
this distinct fix proves the historical failure's cause.

## Targeted acceptance coverage

- Real prepared eCantorix process, not a mock; English formant eSpeak and SoX.
- 125 BPM, eight-bar render duration; separate fixture checks note onset/take offset,
  held note, audible syllables, C4 pitch correlation and exact rest silence.
- Default 16-bar intro/outro exclusion and explicit override in MIDI plan; actual
  single-track WAV stem has silent intro and outro.
- Text edit invalidation, ten cache hits, sub-tick collision, checksum corruption repair.
- Immediate cancellation, repeated active renderer cancellation with partial-directory
  cleanup, supervised child cancel/retry, and POSIX descendant termination marker.
- Existing AudioTake import, sample load, project save/reopen, separate WAV export.
- Real modeless editor start/finish/import, stale result refusal, Undo/Redo and export.
- Source preparation rejects an unrecognised upstream before writing output;
  generated helper syntax and four binary pipes checked on Linux.

`FGA_ECANTORIX` must name the prepared helper to run actual synthesis tests;
otherwise they print NOT RUN. `FGA_ESPEAK_DATA` can point to relocated voice data.
`FGA_GUIDE_TEST_OUTPUT` optionally copies the short generated fixture WAV to an
absolute path. Core and engine suites remain available without these tools.

## Remaining validation

Native Windows/macOS compilation, packaging, Perl XS setup, process-tree cancel,
Unicode paths, device/DAW interaction and live listening are NOT RUN. Windows
uses Perl >=5.22, direct list-form pipes and a Job Object; macOS uses POSIX spawn
with its native environment accessor. Source inspection is not a platform PASS.
Licence/package obligations are in VOCAL_GUIDE_LICENSES.md. No binaries may be
bundled without authorization and review of the exact platform package contents.

## Final 01C code review

Reviewed render planning/cache, subprocess lifecycle, worker/editor ownership,
AudioTake import, persistence, build targets and GPL helper preparation. No musical
engine or Vocal Chops source changed. Process management, files, hashing and synthesis
run on the worker/control threads; progress is atomic and results are read only after
worker completion. Closing/replacing the editor retires a cooperatively cancelled
worker; the worker owns its project snapshot and does not access UI/audio callbacks.
Temporary audio buffers are limited by the 60-second/256-segment render limits.

Review fixes: check cancellation/timeout/log limits again after process exit (fast
exit previously bypassed the last check); read at most 1024 diagnostic bytes on failure;
use JUCE's owning writer API so failed creation retains/closes the output stream;
apply pending form text before importing, then reject stale results. Regressions cover
fast excessive output, explicit failure/diagnostic preservation, timeout and a lyric
edited in the form after rendering. The historical cache issue remains unproven as
explained above. No ASan/TSan or physical-device listening validation was performed.

Before review, all 18 pending 01C files were inventoried, hashed and backed up outside
the repository. The manual test procedure adds one documentation file. The reviewed
manifest contains only text/source/license files; no eCantorix, eSpeak, SoX or Perl
library/helper appears in the JUCE application link command. GPL source preparation
and COPYING remain separate; this does not waive distribution obligations.

Manual DAW/listening validation is pending: [Italian procedure](VOCAL_GUIDE_MANUAL_TEST.it.md).
