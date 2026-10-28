# Validation record

## 0.23.5 — auxiliary-window close and first-click actions

- All owned non-modal tool windows explicitly return activation and keyboard focus to the main DAW after hiding. Studio ends auditions and takes, then synchronously refreshes recording/import availability; its hidden timer is not responsible for restoring controls. Existing busy guards remain in place.
- Native UI coverage repeats Studio, Mixer, AI and instrument-panel closure three times each forces a missing mouse-up in each tool peer and sends one mouse-down/up pair through the main native peer to Generate Song, checking focus, enabled state, a changed generation seed and exactly one Undo checkpoint. Additional assertions keep disabled owners disabled, cancel a hidden pressed button without invoking its action and preserve a gesture in another visible window. The new input regression fails before JUCE input recovery and passes after it. Existing host coverage checks focus after VST editor closure.
- Build, local native UI/host results and official release outcomes are recorded in the release report.

## 0.23.2 — workspace controls and time display

- Inspector gain/pan and home master/density/swing/feel use rotary controls with the existing values, gestures and Undo. Labelled actions gain vector icons; compact inspector dismissal and double-click track-header opening retain transport position.
- Section-start lines match header colours. Footer zoom/fit is aligned with Harmony, and the collapsed mixer shows a stereo master peak meter with dB and peak hold instead of the mixer caption.
- Clicking the transport position switches bars and elapsed time with a remaining-time legend; arrangement labels convert beat positions using project BPM. Notes, snap, loops, project schema and musical timing remain unchanged. The preference is local.
- Native presentation verification exercises real controls, rotary Undo, footer zoom/fit, readout conversion, unchanged MIDI/project/Undo and a compact meter driven by actual live guide audio. Existing native Studio, guide, MIDI editing, navigation and workspace regressions are retained.
- Theme/font/shared-module and six-language checks pass. Italian and English manuals and screenshots are refreshed. Local build/logs/screenshots and reversible patches are in the review report; official cross-platform release outcomes are recorded in the release report.

## 0.23.1 — manual refresh

- Both manuals retain nine chapters and eleven images; local anchor checks, family resources and Windows/Linux packaging checks pass. Updated native screenshots follow the current Arranger.
- The app rebuilt successfully with embedded manuals. Native UI verification passed for Italian/English manual rendering, navigation, search, zoom and the surrounding Studio/workspace flows.
- Only documentation and release metadata change from 0.23.0. Audio code, MIDI generation, project schema and the official release workflow remain unchanged. Final cross-platform outcomes are recorded in the release report.

## 0.23.0 candidate — audit fixes (local, publication awaits approval)

- Audit peer tests drive the real callback with a hosted test instance: live float headroom, processing without the control mutex, concurrent state snapshots/restores, automation boundary subdivision, MIDI offsets and per-subblock playhead, loop capture, latency subtraction and capacity reporting.
- Queues are bounded; overflow leaves unrelated tracks sounding. Input timestamps are mapped once per physical callback, and prepared guitar/lead queues cannot grow on the audio thread.
- Native PCM tests cover snapped/long bus delay, stereo WAV capture/finalization, input capture during engine maintenance, long sample imports and non-destructive take import. Short/sparse legacy-project tests preserve existing data and reject duplicate IDs.
- The native Studio verifier exercises actual audio start/finish/import callbacks and one-step Undo with retained WAVs. Source input opens only on explicit user recording/settings; macOS microphone purpose text and privacy notices are updated.
- The project model is still 4/4; engine playhead/metronome/count-in support configured meters. Hosted automation ramps update at at most 64 frames with exact breakpoint cuts. PDC storage is bounded at 262143 frames and an exceeded limit is shown.
- Plugin state snapshots quiesce processing and may briefly silence output depending on vendor serialization time; factory creation and replacement preparation occur outside that window. Input recording retains frames during maintenance; master takes record audible output, including silence.
- Physical microphones/drivers and official Windows/macOS binaries are not tested by the local PCM fixtures. Final command logs, outcomes, patch reversibility and images are in the local audit report.

## 0.22.7 candidate — single waveform and compact PDF composition (local)

- One full-width waveform replaces the overview/detail split. Separate zoom/position sliders, the extra ADSR diagram and stacked mode buttons are removed. Native checks exercise wheel/pinch/fit without project or Undo changes, real file/browser drops, modes and parameter edits with Undo.
- Five modules follow the reference in two rows (region/envelope, tuning/filter/slice-sync), with knob/value/caption order. Source/preset controls sit above the tabs; slicing actions and chips share the row under the wave. The shorter keyboard retains mapped-key and playing-note highlights, pads, audition, input and tuner.
- Native checks verify all parameter/caption bounds and usable slice button sizes at minimum window size, visible keyboard, 440/880 Hz tuner, audition, recording and workspace regressions.
- Shared scoped StudioTheme uses the six PDF colours, Inter, 11 px labels, 12 px values and flat controls. The family and global DAW themes remain unchanged. Bus/send checks retain dB conversion, silent floor, typed Pan, M/S, Undo and identical MIDI.
- All seven CTest groups passed before these subsequent UI-only adjustments; DSP, MIDI generation and project format remain unchanged. Local build/native tests and repository/package checks are included in the review report.
- Metadata is 0.22.7 locally. No commit, push, tag, workflow or release without explicit user approval; official Windows/macOS builds await the approved pipeline.

## 0.22.6 — Mixer groups and A/B sends

- All seven Linux CTest groups pass. The audio graph, project schema and MIDI engine are unchanged; this moves the existing routing controls and adds mixer entry points.
- Native mixer tests change output group, send amount, group gain and return mute through actual callbacks, verify one-step Undo and byte-identical MIDI, then restore the original state.
- Per-channel A/reverb and B/delay buttons change the real send values and their visible on/off states. Undo restores the prior amount. Controls fit 100/140/210/320 px strip heights.
- The mixer header opens the dedicated always-on-top panel; hide/reopen restores centred placement. Real wheel callbacks preserve project JSON and Undo, and meter labels retain Silence/plain dB.
- Existing sampler audition/tuner, MIDI recording, Studio window and workspace checks pass after removing Studio's bus tab. Native images update the mixer, sampler and recording guides.
- Common theme/module/translations and package-layout checks pass locally. Native local checks used the updated source with the prior version resource; the official tagged workflow builds the final version on Windows/macOS/Linux and gates publication on all checks.

## 0.22.5 — Audio pitch measurement in Studio

- All seven local Linux CTest groups pass. Controlled PCM checks cover 55–1568 Hz, detuned tones within four cents, a decaying tonal kick, a missing fundamental, silence, white noise and stereo anti-phase signals. Retargeting discards prior sample data.
- The native Studio test measures the actual 440 Hz sample independently of its MIDI root, then measures 880 Hz after live octave transposition and verifies the displayed text. Play/Stop/repeat, project/Undo preservation, sampler controls, scrolling, MIDI recording and floating-window checks continue to pass.
- The callback only filters/decimates the selected sample stream into a bounded SPSC queue. YIN analysis and UI formatting run on the message thread. Readings are independent of track mute/gain and reflect the sampler before track effects. No automatic parameter, MIDI or project changes are made.
- The dedicated pitch label uses the same octave convention as the Studio keyboard; the previous indicator explicitly says MIDI NOTE. The last stable pitch is held for one second with a distinct label, then cleared. New sources reset measurement history. Unstable/noisy sources may have no note; this is a monophonic tuner, approximately 35–1600 Hz.
- The six-language dictionaries and common module/theme/font checks pass; IT/EN guides and a native image document the readout. Local UI/audio tests used updated source with the prior version resource; the official pipeline builds and tests the final tagged version. Physical audio hardware is outside this environment.

## 0.22.4 — Isolated Studio sample audition

- Seven local Linux CTest groups pass. New audio callbacks verify isolated audible output despite track mute/zero gain, preserved transport position, natural one-pass completion, repeated regions, live filter edits, trim/reverse/root changes and first-slice mapping.
- Stop, repeat-off, song playback, recording and instrument replacement terminate audition correctly. MIDI queued while auditioning does not play later. Missing/invalid sample targets are rejected.
- Native Studio checks exercise the actual Play/Stop/repeat buttons, sounding-note feedback, close cleanup, enabled states and tooltips. Project JSON and Undo remain unchanged by audition controls. Existing sampler, scrolling, recording and floating-window checks pass.
- Studio uses a dedicated instance of the existing sample synthesiser before track mixing. The normal sampler DSP and project schema are unchanged. Parameter snapshots remain owned by the private audition instance; synth allocation/destruction happen outside the audio callback.
- The title uses an ASCII separator and is checked by the native integration test. The guide sampler image is captured from the updated native window. Common module/theme/fonts and six-language dictionaries pass their checks.
- These checks use controlled PCM, real audio/UI callbacks and a virtual display; physical audio hardware remains outside this environment. Local native tests used the updated code with the previous version resource; the official pipeline builds and tests the final tagged version.

## 0.22.3 — Studio scrolling and meter labels

- Native UI regression sends real wheel callbacks over sampler sliders, bus sliders, track/routing/mode/device menus, including pages detached by the tab control. Project JSON, menu choices and Undo history remain unchanged.
- Zero meters use the localized Silence label; unity and half gain display plain numeric `0.0 dB` and `-6.0 dB`. The native bus image is refreshed.
- Existing sampler trim/slicing/reset/Undo, actual note indication, recording, editor and floating-window checks pass. The audio DSP and project format are unchanged.
- No user project or changed Kick parameter was supplied; the exact cause of that user's attenuation is not established. Recovery instructions distinguish sampler defaults from track/group mix settings.

## 0.22.2 — Studio sampler workflow

- Seven Linux CTest groups pass, with project round-trip checks preserving sample drum pitches outside slice mode and the original fixed MIDI mapping for built-in drum guides. The core executable is rebuilt against the updated core library and passes again.
- Native Studio checks cover the inspector entry, sample-only target IDs, retained all-track recording/bus targets, wheel/pinch pointer anchoring, horizontal pan and double-click fit without trim changes.
- Transient/equal slicing, visible mapped keys/pads, actual sounding-note display, reset preserving file/MIDI and Undo pass. A take keeps its target when changing tabs; close/reopen and recovery checks remain passing.
- Audio callbacks verify note indication during a one-shot tail after note-off, clearing at Stop, sustain and monitor retargeting. GUI note reads use atomic snapshots rather than locking synthesiser voices.
- Studio owns its tooltip window; a real tooltip is shown in the native check. Descriptions, keyboard tips and compact layout checks pass. New labels/tooltips use the six family languages; shared theme/font/module files remain unchanged.
- Refreshed native sampler, recording and bus images accompany the IT/EN guides. Hardware MIDI keyboards and physical trackpad gestures still require a device-level check; native tests invoke the actual UI and audio callbacks with controlled input.

## 0.22.1 — Local AI setup

- Seven Linux CTest checks pass. Ollama HTTP fixtures verify installed-model discovery, duplicate/cloud/remote-alias exclusion, address normalization, empty/wrong servers, connection refusal, missing models, memory errors, no-key requests, blocked redirects and cancellation.
- Native English/Italian UI checks run the actual model-list worker, choose an installed model, invoke COMPOSE and verify a six-minute 138 BPM MIDI preview. Changing duration invalidates APPLY. Existing Apply/Undo/stale-project checks and the full native UI suite pass.
- These are controlled HTTP fixtures, not model inference: local model quality, speed and hardware fit still need testing on the user's computer.
- Updated IT/EN setup manuals, privacy statements and native screenshots pass family-resource/translation checks and Linux/Windows packaging fixtures. CI additionally runs the native local-AI flow on Linux and HTTP checks on all three systems before release.

## 0.21.1 — Piano note-range feedback

- All seven Linux CTest checks pass. Native UI checks now verify the Piano range control's actual painted handles at both limits, hit targets and real mouse-down/drag/up notifications. Both handles move to their new pitches; the old positions contain no ghost handles. The resulting MIDI changes, and Undo restores each range edit and the original notes.
- The defect was in Arranger's single-value-only slider renderer. Piano's original VST/standalone uses JUCE's existing two-value renderer and needs no change.
- Fresh native Piano imagery and IT/EN manuals show and explain both handles. The embedded nine-chapter/eight-image manuals and original window, editing and AI UI checks pass. Full native smoke and independent WAV/MP3/stem verification pass, including audible regeneration variety.

## 0.21.0 — AI composition

- All seven Linux CTest checks pass. Project I/O tests now exercise composition compilation: six minutes at 138 BPM outside genre limits, MIDI pattern expansion, percussion mapping, monophonic Lead, harmony metadata, duration overrides, DJ phrase preservation and closing-fill movement.
- AI failures are transactional: malformed JSON/notes, disabled tracks, unknown references, duplicate pattern IDs and simultaneous Lead attacks cannot mutate the destination. Full-song locks, targeted-track isolation and locked independent clips are covered; assigned instruments, effect/plugin state and mix remain intact.
- Four provider request and response envelopes are checked offline. A real local HTTP fixture exercises POST context, a six-minute MIDI result, authentication/quota/invalid-response failures, redirect blocking and cancellation during a stalled response. No paid authenticated online request or local model weights were used in validation; users configure their own service/model.
- Native UI smoke passes the actual composer window, provider selection, masked session key, six-minute insertion, tempo display at 138 BPM, duration display at 6:00, immediate playback, Undo and stale-project rejection. English and Italian screenshots come from the application. The original six interface languages and icon tooltips pass.
- Embedded IT/EN manuals pass nine-chapter navigation, eight images including the new AI composer, search, zoom and table layout. Exact shared family theme/fonts, linked manuals, packaging and source/license fixtures pass.
- Native mix/stem/MP3 rendering, mute/solo and audible-regeneration checks pass. The real VST3 host probe passes MIDI pitch, parameter ratio, state restore, editor lifecycle and private-copy export.
- Release retains the Windows/macOS/Linux workflow and Windows installer, adding local AI HTTP tests on each platform and Linux libcurl build headers. macOS app metadata allows the selected local Ollama connection without relaxing HTTPS for online services.

## 0.20.0 — transport selection, drum gates and floating editors

- All seven Linux CTest checks pass, including the new handwritten drum gate checks and a real 1.2-second WAV sample played by a 0.25-beat Crash note. The sample remains audible after note-off in realtime playback and private-copy WAV export; Stop silences it.
- Native UI smoke passes section-header seek-and-play, non-seeking grid selection while stopped/playing and after scrolling, numbered-ruler seek, loop resize/removal/Undo and independent clip editing. Undo retains selection for unchanged clip ranges while the playhead is elsewhere.
- Native editing gestures pass for Kick and Crash as well as pitched instruments and Guitar: handwritten gates match the original Drums engine; move, velocity, right-edge duration editing and delete work. Drum notes display their stored lengths.
- The Piano panel's Extensions and main harmony selector are exercised through real ComboBox callbacks: triads and ninths produce different MIDI, and returning to triads restores the same notes.
- Instrument-window checks cover always-on-top, explicit close and centred reopening. A real VST3 host probe exercises the same wrapper lifecycle, including moved-window close/reopen, and verifies MIDI pitch, parameter changes, state restore and private-copy export.
- Native rendering passes aligned stems, mute/solo isolation, MP3, sustained-note chase, automation, audible regeneration variation and embedded IT/EN guide checks. Family theme/font/manual and Windows/Linux packaging fixtures pass.
- Release follows the unchanged Windows/macOS/Linux workflow, Windows installer and full thirteen-genre club audio gate.

## 0.19.0 — MIDI deletion and workspace refinements

- All seven Linux CTest checks pass, including core generation, independent MIDI reader, MP3, project I/O, updater, realtime audio and sample browser.
- Core deletion checks protect all generated instruments and retain a later manual track's notes, name, plugin state and effect state after routing compaction. Deleted capacity is reusable and newly added tracks receive distinct default names.
- Native UI smoke deletes a manual track through its inspector action, verifies the following track's notes and sound, and exercises Undo/Redo. It also fills the 32-track capacity, deletes and re-adds a track and checks that + MIDI is re-enabled after deletion and Undo.
- The collapsed inspector is fully hidden with zero retained width, restoring the arrangement's left edge to 12 px; its toolbar icon restores the original width. Existing mixer/inspector resize, collapse, MIDI and transport checks pass.
- All six section choices insert their selected role and support Undo. Explicit clip regeneration, APPLY and song regeneration reset horizontal zoom to fit; the MIDI editor also fits the generated pitch range.
- Native panel checks cover Piano, Bass, Strings, Guitar, Drums and Lead. Playing menus are constrained inside the coloured frame. Screenshots verify track names above the mixer faders, source buttons below, the compact inspector icon and a readable master CLIP indicator.
- Native MIDI-editing, automation, transport, sample browser, embedded IT/EN guides and audio export checks pass. Eighteen aligned stem exports, silent lane volume/mute tests, MP3 and audible regeneration variation pass independent verification.
- Family theme/fonts, guide links and packaging fixtures pass. Release uses the unchanged Windows/macOS/Linux workflow and full thirteen-genre club audio gate.

## 0.18.0 — handwritten tracks, separate panels and piano variety

- All seven Linux CTest checks pass: generation, independent MIDI reader, MP3, project I/O, updater, realtime audio and sample browser.
- The [piano audit](PIANO_VARIATION.md) compares 48 consecutive regenerations in each of 13 genres, ignoring transposition, velocity, holds, ghosts and humanisation. Similar consecutive pairs fall from 155/611 to 1/611, with the remaining pair in sparse Chill. Separate core regressions exercise sixteen consecutive takes per genre, saved-take reproducibility, fixed presets, locks and untargeted clips.
- Manual MIDI tracks start with canonical empty clips, preserve notes/routing/name/automation through song, section, harmony, genre and form operations, and support explicit automation-lane dice. Save/load is byte-identical after normalisation. The 32-track limit and stable visual/routing IDs are checked. Realtime audio tests confirm handwritten guide-synth output.
- Native UI smoke passes the actual detached instrument viewport without changing the main page, all six instrument layouts, pitched editing, manual-track add/undo/redo, rename and window-title refresh, generation isolation and project round-trip. All twelve complete key names fit the transport display. Lead internal and external panels have no overlapping or out-of-bounds controls; global octave is visible and captioned.
- Lead 0.1.1 (`07b33a3`) supplies the shared global-octave engine. Octave tests retain rhythmic attacks, velocities and relative lane settings, and transpose pitches by twelve semitones. `sync-engines.py --check lead` passes; only the existing shared chord-duration patch remains.
- Native audio verification passes eighteen aligned stem exports, guide volume/mute isolation, MP3, automation, transport and audible regeneration differences. The real VST3 host probe passes note pitch, plugin state/editor lifecycle, effect gain and independent export; the isolated catalogue probe passes.
- English/Italian guides retain the nine-chapter family structure and native illustrations. Family theme/fonts, guide links and packaging fixtures pass. The unchanged release workflow gates Windows, macOS and Linux packages and the thirteen-genre club-audio audit before publication.

## 0.17.0 — Lead integration and inspector

- Seven CTest checks pass on Linux: core generation, independent MIDI reader, MP3, project I/O, shared updater safeguards, audio engine and sample browser.
- Lead is vendored from the released v0.1.0 source at `a2294b9`; its only engine patch is the shared harmonic duration. `sync-engines.py --check lead` passes.
- The original MIDI tracks are byte-identical in all 16 baseline MIDI files; the new track is appended and exported on melodic channel 5. Core checks exercise all thirteen genres, monophony, take isolation, locks and shared chord durations.
- Seventeen-track JSON migration preserves existing notes, plugin/effect routing, mix and automation exactly, appending an empty Lead track. Eighteen-track projects and Lead settings round-trip.
- Native UI smoke passes Lead controls, pitched editing, internal/external panel layout, all inspector card bounds at minimum/default/expanded widths, resizing/collapse, actions and undo. Both embedded guides render nine chapters and seven images.
- Native audio verification passes all eighteen aligned stems, Lead guide-volume/mute isolation and MIDI preservation. VST3 host and isolated plugin-catalogue probes pass.
- Packaging fixtures pass English/Italian guides, Lead illustration and licence, inspector preview, platform payload allowlists and executable permissions. The existing CI workflow also runs its full thirteen-genre club audio audit and native platform builds before publishing.


## DAW workspace and regressions (0.5.0)

[Native interface validation](https://github.com/wsx79/fasegroovearranger/actions/runs/39558741208), source `4a403992e831ec5ad99577daf49741ff7e818e8f`:

- Windows, universal macOS and Linux builds passed their engine, MP3 and project IO suites. The Linux app passed actual component gestures for clip selection/swap/undo, ruler seeking, loop dragging, full MIDI editing, audio-only fader changes and undo, plus compact workspace layout.
- Native callback checks passed live fader ramping, pause silence/meters, seek/chase and loop boundaries. The final packaging refresh also checks disabled audio-output pointers and assigns newly loaded drum samples using their MIDI mapping.
- Original instrument panels, family theme and Inter font files remain byte-identical. Six-language refresh preserves the musical JSON; tooltip and extended piano checks pass. Native screenshots from this run populate both source manuals and the embedded guide images.
- All thirteen generated MIDI songs are byte-identical to the 0.4.7 baseline in local comparison. New core regressions cover whole-song and section loops, section-relative automation resizing and section-aligned clip swap/copy.
- Host-fixture state restoration and independent WAV/MP3/stem checks passed. Final distribution rebuilds repeat native checks and the complete thirteen-genre audio audit; release `SOURCE.txt` identifies the exact packaged source and successful run.

This verifies the tested processors and UI gestures. Real hardware and arbitrary commercial instruments still require testing on their target systems.

## Family alignment, 0.4.1

[Windows, universal macOS and Linux validation](https://github.com/wsx79/fasegroovearranger/actions/runs/39393262535), source `8d9d10d1cf2dd3f32e3e2c6baeeff56ec510a3c9`, passed all four jobs, including native smoke and VST3 integration. The four manual screenshots now come directly from that native build. The following update-overlay and embedded-image refresh is checked by its own subsequent workflow, linked in the pull request.

- Exact canonical family theme and Inter Regular/SemiBold Git blobs verified offline on all three systems; Windows checkout CRLF is normalized for the theme hash.
- All instrument controls fit without overlap: Piano, Bass, Strings, Kick and Guitar. The SETTINGS / ABOUT geometry, link-button positions and sizes match the copied family overlay.
- Actual native screenshots inspected: extended Piano configuration, Drums pads and settings, Guitar and twelve-part mixer, individual editors, settings, credits and six languages. Review used Linux native rendering; Windows/macOS builds and tests also passed.
- MIDI editing gestures passed for five instrument types and guitar note mode. Six-language switching preserved musical JSON and item IDs; original Piano musical tools and MIDI chord-learning simulation passed.
- Independent SMF parsing passed 18-track song export (12,344 matched notes), isolated Kick (615 notes), Piano with sustain/expression (2,830 notes), note pairing and seven section markers.
- Native audio checks passed 12 active and five intentionally empty stems, aligned at 305,809 frames / 44.1 kHz; mix RMS 0.14136. Muting and guitar part faders silenced audio while retaining stored MIDI; MP3 was independently decoded by ffmpeg.
- The real test VST3 loaded, received MIDI/playhead data, opened/closed its editor and restored its saved state. It is a fixture, not validation of arbitrary commercial instruments.
- Bilingual native manuals passed 13 chapters, tables, four screenshots, navigation, search and zoom. Source guides remain Markdown; the invented separate HTML export has been removed.
- Updater versions, platform asset selection, checksums, staging and rollback are checked offline. Live release availability/downloads depend on GitHub and the target device. Installation requires INSTALL NOW and local write/setup permissions; no project data is uploaded.

See [FAMILY_ALIGNMENT.md](FAMILY_ALIGNMENT.md) for pinned reference revisions, the original UI differences and necessary standalone adaptations. Historical records below describe earlier revisions and formats, not the current distribution.

## Family editor revision, 0.4.0

[Native builds on Windows, macOS and Linux](https://github.com/wsx79/fasegroovearranger/actions/runs/39385637071), source `56e67d16c57446acc098fe7218c2b76bf39f115c`, passed all four jobs. Native screenshots were inspected against the original Piano extended layout, Drums grid and Guitar layout. Panel bounds/overlap checks passed for Piano, Bass, Strings, Kick and Guitar; six-language switching and original musical tool checks passed. The subsequent [native revision](https://github.com/wsx79/fasegroovearranger/actions/runs/39387405449), source `f53d1b61532f9a45b3a54a862f4af22289092692`, also passed all four jobs. Actual editor gestures passed for all five instrument types and guitar note mode. Independent WAV inspection confirmed silence with guitar guide faders at zero and guitar parts muted, while MIDI bytes stayed unchanged. Further source refinements must be checked against their own final CI result.

The local core suite passes all thirteen genres with complete phrases, stored-take determinism, scope isolation and locks. New checks verify that the legacy minute property and tempo do not stretch the musical form to a clock target, longer developing motifs and sparse/busy grooves affect phrase development, and the original intro’s harmonic content is not stripped into a compulsory mixing window. Independent SMF parsing passes conductor + 17 tracks and isolated Kick/Piano stems, matched note pairs, controller resets and section markers.

Default genre forms currently range from 4:25 (Gospel) to 6:05 (Jackin’) at their default tempi. These are outcomes, not fixed durations; changing motifs, harmonic cycles and evolution can change the form.

This document distinguishes executed checks from outstanding platform/plugin validation.

## Executed in the development workspace

- Compiled the original three engines plus the arranger core with g++ / C++17.
- Passed tests for all 13 genres: note bounds/validity, deterministic stored takes, isolated clip/track/section regeneration, track/clip locks, section duplication/reordering/removal, MIDI track counts and editing operations.
- Independently parsed the exported complete MIDI file and kick stem: correct SMF type 1 / 960 PPQ, tempo and six markers, no unclosed or overlapping same-channel pitches. Example full song: 4,970 matched notes, conductor + 17 instrument tracks.
- Compiled the bundled Shine library and encoded a stereo test signal. ffprobe identified MP3, 44.1 kHz, 2 channels, 320 kbps and approximately 2 seconds duration. Independent ffmpeg decoding recovered the expected 440 Hz / 880 Hz signals on the two channels.

## Executed in GitHub Actions

[Windows, macOS and Linux validation of the bilingual guide and standalone](https://github.com/wsx79/fasegroovearranger/actions/runs/39375342152), source commit `116e933b5274eeae4ccf9c37afc18e3776c56f04`:

- All three native applications compiled; arranger, integrated MP3 and project IO tests passed. Project IO covers JSON round-trip, atomic saves, schema rejection and PPQ MIDI import.
- The native guide rendered both languages, 13 chapters, 11 tables and four embedded screenshots. Automated checks passed internal chapter links, sidebar navigation, search and text zoom; actual guide and credit screenshots were inspected.
- Offline HTML generation passed on Windows, macOS and Linux, including UTF-8 Italian accents, chapter targets and local screenshot assets. Local checks also validated stylesheet, script and language links. Browser interaction was not executed in this workspace.
- The Linux application rendered a mix, all 17 aligned 24-bit WAV stems, MP3 and a fully muted silent mix. Twelve stems were active; five were intentionally empty. All stems aligned at 305,809 frames / 44.1 kHz; mix RMS was 0.14149 and the peak remained below the clipping threshold.
- ffmpeg independently decoded the MP3 into non-silent PCM. The MPEG header confirmed 320 kbps / 44.1 kHz / stereo.
- Callback simulation passed seeking, held-note chase, looping and pause silence.
- Native screenshots and routing checks cover the arrangement, individual instrument panels, full MIDI editors, isolated drums, all guitar lanes and notes mode, the guitar mixer, and APPLY preserving the stored take.
- A real native VST3 test instrument loaded and received MIDI/playhead data. Its editor opened and closed repeatedly. Parameter changes affected rendered audio; saved plugin state restored the original result. This validates the fixture, not arbitrary commercial instruments.
- Windows, universal macOS and Linux packages were produced with guides, privacy, licences, font assets and exact corresponding-source links.

Final packaging refreshes the four documentation screenshots and credit spacing. Subsequent changes rerun the applicable checks; the current pull request exposes the latest workflow result. Automated rendering does not replace listening through a real audio device.

## Requires target hardware and user instruments

Real audio-device playback, latency/buffer behaviour, commercial VST3/AU compatibility and state restoration, long-session editing, accessibility and signed distribution require testing on target machines. No claim is made that arbitrary third-party plugins have already been verified. Current missing DAW capabilities are listed in README.md and both guides.

## Extended piano and six languages (0.3.0)

[Successful Windows, universal macOS and Linux build](https://github.com/wsx79/fasegroovearranger/actions/runs/39379795779), source `230b5d902a631f09b13a7c6d765588557e30afb9`:

- Native checks passed six live interface languages, translated control tooltips, unchanged musical JSON and preserved menu selections. Actual Italian/English interface and extended piano screenshots were inspected.
- The original circle of fifths, DNA, Morph, A/B coach, variation lab and MIDI-learning window rendered natively.
- Core checks passed user-grid generation, step velocity changes, per-chord inversion, controller scheduling and DNA rhythm/harmony locks, in addition to all thirteen genre suites. Project IO covers the additional persistent parameters.
- Independent SMF validation passed the 18-track song, isolated kick and piano/controller fixture. The latter preserves note pairs and contains CC64/CC11, with pedal and expression reset at the end.
- Guide refreshes and final packaging rerun the same platform checks. Real MIDI-device callbacks and commercial-plugin interpretation of CC64/CC11 still need testing with the user's hardware and instruments.

## DAW navigation and MIDI selection workflows (0.7.0)

The native `daw-navigation.txt` report checks pointer anchoring in scrolled timeline/MIDI views, horizontal wheel routing, selection zoom/loop and locator Undo, MIDI group boundary nudges, octave steps, cut/duplicate/cursor paste, rejection of partial phrases, drum isolation, gesture shortcut protection and one-step Undo when generating at an empty-track cursor. Navigation must preserve project JSON/MIDI and leave edit history intact. Existing native layout, persistence, transport/chase, automation, host and full-song audio checks remain required.

## Family installers and upgrades (0.7.1)

`family_updater` tests both changelog formats, platform/repository asset routing, corrupt download rejection, archive traversal rejection, staging, complete swaps and rollback at each transaction step. Windows also checks custom setup-folder detection. `scripts/test-installer.ps1` installs the actual setup, upgrades a simulated older binary, verifies the exact tested executable and bundled guides/source, then uninstalls while preserving a user-created project. `tests/test-packaging.py` verifies legacy and stable update layouts. Live UAC prompts, protected folders and user-specific security policies still depend on the target computer.

## Studio 0.22.0

New automated coverage verifies sampler/preset/routing schema round-trips, portable
folder moves, deduplicated collection, cancellation and destination preservation,
and refusal of ambiguous relinks. DSP tests exercise gated and one-shot note-offs,
trim/reverse, filtering, loop seams and independent tempo/pitch changes using
rendered frequency and duration. A long sample verifies natural export tails;
private-copy and live-engine renders are byte-identical. MIDI tests drive the
actual callback queue, count-in, velocity, duration, sustain, onset quantisation,
loop splitting and track locks. Native UI evidence covers trim/Undo, drum slice
pitches and editor, the four Studio panels and floating-window reopening.

Run the existing seven CTest groups, native `--smoke-test`, `scripts/verify-audio.py`,
family inventory/translation checks and platform release workflow. `--ui-test DIR`
runs the native UI evidence without the audio export suite. A physical MIDI device
and hardware round-trip latency require an actual audio/MIDI device; the cloud
checks use the same callback with queued input and decoded PCM fixtures.
