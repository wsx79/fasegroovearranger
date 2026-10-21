# Validation record

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
- Updater version comparison is checked offline; live release availability, networking and browser download depend on GitHub and the target device. Portable updates require user installation, and no project data is uploaded.

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
