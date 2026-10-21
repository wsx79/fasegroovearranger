# Architecture and provenance

The application is a native JUCE GUI executable, not a wrapper around the three plugin binaries. All three original pure-C++ generation engines are vendored, separately namespaced (`sgk`, `fgd`, `fgg`) and linked into `fga_core`. Source snapshots are recorded in THIRD_PARTY_NOTICES.md. The guitar step adapter adds a shared `stepsPerChord` duration (8/16/32/64); its default of 16 preserves the plugin rhythm/harmony timing. The remaining engine integration lives in `Source/core/Project.cpp`.

`Project` holds musical settings, ordered sections and 17 tracks. A clip has section-relative note starts/lengths, original guitar lane, velocity, seed and a generation lock. Track count/order is fixed in schema version 1. Guide insertion/reordering/removal leaves audible clip positions intact; legacy whole-arrangement transforms remain separate APIs. Bounds/finite-value validation and same-pitch overlap reconciliation protect scheduling and MIDI export.

Piano, bass and strings come from the Piano engine's distinct parts. Piano generation starts with an original genre-specific preset and uses its variation/intention generators. Drums and guitar use their original genre-aware random generators and arrangement renderers. Harmony is shared by scale degrees/free chord codes; guitar uses its original diatonic/code chord builders. Rich extensions outside the guitar's supported vocabulary are simplified to its supported chord colour.

Scope generation writes only the eligible track/section clips; locks protect stored notes. A new take changes clip seeds. Rendering a saved take uses stored seeds without randomness. Reopening a project reads stored notes, rather than regenerating them.

`AudioEngine` schedules pre-rendered note events into original guide instruments, JUCE SamplerSound, or a hosted native VST3/AU instance. It exposes a host playhead to plugins. The audio device uses output only. A try-lock avoids blocking the realtime callback on project/plugin mutations. Buffer/schedule preparation and generation occur outside the realtime callback. Per-track buffers mix with gain/pan/mute/solo/master and a final hard safety ceiling. This initial implementation does not compensate plugin latency.

Offline audio export temporarily owns the processing engine and uses the same instruments/mixer. WAV is stereo PCM24 at device sample rate. MP3 renders at 44.1 kHz and uses the bundled Shine encoder at 320 kbps. Both include three seconds of tail. Temporary files are atomically committed only on success. Batch stems share the same song origin/duration; native plugin randomness can vary between passes. Audio export never substitutes a missing external instrument silently.

`ProjectIO` handles versioned JSON `.fga` projects, validation, atomic saves, external sample/plugin paths and plugin-state blobs. No executable code is contained in a project itself; restoring a plugin loads its external native binary. Project import never runs shell commands. The application has no analytics, embedded website or cloud storage. The family updater downloads GitHub release packages in the background and verifies asset SHA-256. INSTALL NOW runs the Windows setup or a transactional portable app replacement; projects and settings are never included in the replacement targets. A delayed launcher waits for process exit before reopening. Protected paths use manual installation.

`FamilyTheme.h`, fonts and FS PITCH logo are taken from the ecosystem. Arranger has an original multi-track/clip icon; the wordmark implementation and proportions follow the family Branding API. Credits present the creator and development assistance together, with contributor attribution retained. Italian/English guides and privacy/licence documents are embedded in the binary. The native guide reader renders headings, control tables, examples and screenshots with chapter navigation, full-text search and adjustable text size; it uses no browser or network. Source manuals remain Markdown as in the reference apps; there is no separate HTML guide export. Optional release checks use GitHub, without project uploads, and are documented in Privacy.

Build targets: `fga_core`, `fga_mp3`, `FaseGrooveArranger`, core/MP3/IO tests. GitHub Actions builds the native app on Ubuntu, Windows and macOS, and runs a Linux audio-render smoke test. See VALIDATION.md for observed results and unverified cases.

The UI reuses the original six-language family dictionaries, the circle of fifths and musical tool algorithms. Language/size/tooltip/update and circle-mode preferences live in an Arranger-specific settings file. Translating ComboBox labels preserves item IDs and does not change musical project data. Extended piano controls persist in optional schema-v1 fields; older projects retain their stored MIDI. The step grid and velocity editor drive the original user-pattern and velocity-order engine.

Opt-in MIDI chord learning uses a selected device callback and restores its enabled state when closed. Pedal/expression controllers are scheduled to hosted instruments and SMF export, with sustain-aware seek chase. Guide-sound pump can follow a tempo subdivision or the kick MIDI schedule; it does not expose an external audio-sidechain bus. Musical tools prepare settings; explicit APPLY preserves the editable MIDI workflow.

Track volume envelopes use optional schema-v1 `volumeAutomation` fields (`enabled`, musical-beat/value/outgoing-curve points). `VolumeAutomation::valueAt` is a bounded monotonic quadratic interpolation, held at the edges; disabled envelopes return unity. The audio graph evaluates the same gain per sample in playback and offline export, after each instrument and its guide effects, before track pan/master. No automation CC is injected into MIDI. UI lane visibility is independent of bypass and defaults to hidden. Section operations remap points in musical time; MIDI regeneration preserves them.

## DAW workspace (0.5.0)

`DawWorkspace.h/.cpp` owns the side track inspector and the seventeen mixer strips plus master. The mixer can resize vertically and reveals the selected channel. Its controls use the existing project gain/pan/mute/solo and sound assignments; `mixChanged()` updates audio without normalising all MIDI notes for every fader movement.

`ArrangementView` keeps the track header and section/bar rulers visible while scrolling. Single clicks select clips, double clicks open the full editor, and drags swap or copy section-aligned clips on the same instrument. Shift-drag sets the persisted loop range. Track IDs and the `.fga` schema are unchanged.

Section operations in `Project` transform loop locators and volume automation together with the musical structure. Whole-song loops remain whole-song loops after reordering. `AudioEngine` smooths live gain/pan targets, reports master peaks before the safety ceiling, clears pause meters and accepts disabled output-channel buffers.

## Independent timeline and arrangement guide (0.6.0)

`core/Timeline.cpp` adds absolute clip positions on individually edited tracks. Untouched tracks retain the legacy section-aligned representation; editing materialises their audible clip ranges and sets `Track::freeTimeline`. A free timeline may contain zero to 4096 clips, including gaps and clips beyond the guide. `clipStart`, `clipLength` and `clipAtBeat` are the common geometry for playback, export, drawing and hit testing.

Split clips trim crossing notes at their boundaries. Overwrite paste retains outside fragments. Manual editing ignores generation locks; regeneration respects them. Selected clip indices are independent of the selected guide section. Generation uses the guide at each clip position; section regeneration splits independent clips at guide boundaries before replacing that section.

Guide insertion, removal, movement and duration edits materialise aligned tracks without moving their audible content, automation or loop locators. Captured clip roles preserve the existing sustain/expression events when guide roles change. Legacy whole-arrangement section transforms remain separate core APIs.

Edited projects use format 2 with `freeTimeline` and per-clip `position`, explicit `span` and captured `role`, preserving existing notes, loop archives and instrument assignments. Untouched projects retain format 1; the loader accepts both. Native UI, project IO and audio-render checks exercise these paths.

## Navigation and MIDI editing (0.7.0)

`DawNavigation.cpp` centralises pointer-anchored zoom and wheel routing for both viewports. The viewport anchor uses content coordinates once; viewport clamping is the only reason an anchor can shift at a canvas boundary. Ruler wheels and Ctrl/Cmd wheels zoom; horizontal gestures and Shift wheels scroll. Manual navigation disables FOLLOW until explicitly restored. Selection zoom changes view state only; selection loops use the existing undoable locators.

Piano-roll shortcuts preserve relative time, pitch and lane spacing, clamping groups at boundaries. Paste normalises copied time origins, uses the in-clip cursor and rejects phrases that cannot fit. Duplicate retains the original notes and selects the copies. Drum pitches remain assigned to their tracks. Active edit gestures consume keyboard commands to protect their snapshots; note-selection clicks create no Undo entry. Empty-track clip creation during generation shares the generation checkpoint and uses the chosen cursor, which is restored after stopping transport.
