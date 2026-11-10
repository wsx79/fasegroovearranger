# Vocal Studio — English synthetic guide (01B/01C)

Vocal Studio creates a simple offline guide for a later recording by a real singer.
It is independent of Vocal Chops. It does not claim natural or neural voice quality.

1. **Add track → Vocal Studio (English)** or open Vocal Studio from a manual MIDI
   clip's context menu. Edit notes in the existing piano roll; the modeless panel
   uses the same clip/timeline. Generated instrument and Vocal Chops editors stay separate.
2. Write an English lyric draft; add individual syllables/words and assign selected
   roll notes to each syllable. Connected notes on one syllable form a melisma.
   Unassigned notes and gaps are silent. No automatic syllabification or rewriting.
   Pronunciation overrides remain stored drafts and are **ignored by this renderer**.
3. Set the clip-relative vocal region in quarter-note beats. DJ intro/outro are
   excluded by default, even when a note crosses a section boundary. The checkbox
   explicitly allows them. This policy affects rendered singing, not MIDI transport.
4. Select **eCantorix / eSpeak English guide**, install the separate trusted local
   [helper/dependencies](../helpers/ecantorix/README.md), and enter its absolute
   `ecantorix.pl` path. Legacy candidate voices cannot render. Nothing is downloaded.
5. **Render English guide** starts a worker with a project snapshot. The DAW stays
   editable; Cancel stops the helper process tree. Closing the panel cancels an active
   job. Stale results cannot be imported after relevant clip/tempo/position edits.
   Progress indicates preparation/synthesis/finalization stages, not measured ETA.
6. **Add guide audio take** imports through existing AudioTake/sampler tracks and
   mutes source MIDI with Undo/Redo support. Playback uses the normal mixer, sample-rate
   conversion, mute/solo and routing. Re-render after edits and add a replacement take
   explicitly; old takes are retained non-destructively.
7. **Export added guide WAV for singer** opens the existing single-track WAV export
   flow, including timeline intro silence, export encoding and mixer processing.
   Existing stem/export menus also work. A raw cached WAV starts at its AudioTake
   offset, so use stem export when the singer needs alignment to the song origin.
8. **Collect portable project** copies audio assets using the existing collect/relink
   system. `.fga` stores file references, never embedded WAV blobs. The cache is under
   application data `FaseGrooveArranger/VocalGuideCache`; do not delete referenced files
   before collecting/relinking. No automatic cache eviction removes active project audio.

Apply commits text/settings with Undo. Apply before switching syllables or closing;
unapplied form text is temporary. Stable note IDs survive edits; pasted notes require
assignment. The optional schema-1 `vocalStudio` metadata remains backward compatible
with 01B; older applications may drop unknown metadata. No musical generator changes.

Limits: mono 22,050 Hz/16-bit guide WAV; monophonic melody, 256 sung segments and
60 seconds per render, 180-second synthesis timeout. Split longer clips. Exact rests
are masked and synthesis tails are trimmed to note boundaries. MIDI timing uses
960 PPQ; WAV duration/masking uses exact beat values. Upstream's default -24-semitone
transpose is explicitly cancelled. Melisma/stretching can sound rough. No listening
quality claim is made. Use original English formant eSpeak data, not MBROLA/neural banks.

Cache identity includes MIDI/text, exact duration/rest masks, helper source, runtime
versions and English voice data; checksum validation rejects damaged entries.
Moving an otherwise identical clip reuses audio with a new timeline offset.
Native Windows/macOS execution remains **unverified**; see
[licences/platforms](VOCAL_GUIDE_LICENSES.md) and `02_STATUS.md` for actual tests.

Test: CTest `vocal_guide` (`fga_engine_tests --guide-vocal`); set `FGA_ECANTORIX` to
an installed prepared helper for real synthesis, otherwise it explicitly skips it.
`--vocal-studio-test <absolute-output-folder>` tests the real editor, and with the
same environment also its async render/import/Undo/Redo path. No audio callback
performs synthesis, process management or file operations.

Manual Linux listening/export checklist (not yet executed): [Italian procedure](VOCAL_GUIDE_MANUAL_TEST.it.md).
