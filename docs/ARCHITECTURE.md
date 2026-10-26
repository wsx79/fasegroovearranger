# Architecture and provenance

The application is a native JUCE GUI executable, not a wrapper around the four plugin binaries. All four original pure-C++ generation engines are vendored, separately namespaced (`sgk`, `fgd`, `fgg`, `fgl`) and linked into `fga_core`. Source snapshots are recorded in THIRD_PARTY_NOTICES.md. The guitar and lead step adapters add a shared `stepsPerChord` duration (8/16/32/64); its default of 16 preserves the plugin rhythm/harmony timing. The remaining engine integration lives in `Source/core/Project.cpp`.

`Project` holds musical settings, ordered sections and 18 tracks. A clip has section-relative note starts/lengths, original guitar lane, velocity, seed and a generation lock. Legacy track IDs 0–16 stay fixed; Lead is appended at ID 17. Seventeen-track schema version 1/2 projects upgrade by adding an empty Lead track, and ordinary saves retain the legacy formats; Studio settings opt into version 3. Guide insertion/reordering/removal leaves audible clip positions intact; legacy whole-arrangement transforms remain separate APIs. Bounds/finite-value validation and same-pitch overlap reconciliation protect scheduling and MIDI export.

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

`DawWorkspace.h/.cpp` owns the side track inspector and the eighteen mixer strips plus master. The mixer can resize vertically and reveals the selected channel. Its controls use the existing project gain/pan/mute/solo and sound assignments; `mixChanged()` updates audio without normalising all MIDI notes for every fader movement.

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

## Musical regeneration (0.7.2)

`Clip::motifSeed` is an optional persisted musical-take seed, independent of performance `seed`. Zero preserves legacy song-derived motifs. Explicit nonzero generation assigns one motif per melodic track and a shared motif for drum stems, for the requested scope only. Seed-zero APPLY renders stored takes deterministically. The Piano settings adapter accepts the clip motif so musical tools show the actual take. Its optional `bassLineOverride` develops selected bass-library figures using chord functions without changing the selected bass mode; the vendored Piano renderer and grid accept this override, with empty preserving original plugin behaviour. Song harmony and assigned sound engines remain under user control.

## Instrument catalogue (0.8.0)

`PluginCatalog` loads local XML preferences/cache before the UI opens, then scans at normal startup on a worker thread. VST3 discovery walks standard and custom roots recursively, skips directory symlink recursion and deduplicates overlapping paths. Bundles are fingerprinted by relative file paths, size and modification times; successful unchanged VST3 entries are reused. Audio Units are enumerated through the macOS registry. The instrument selector uses a sorted snapshot, grouped by manufacturer, without loading plugin code to open a menu.

Inspection launches the same executable with `--scan-plugin format identifier output`, allowed alongside the main instance. Only that child calls `findAllTypesForFile`. It returns instrument descriptions as XML; the worker checks format/identifier, enforces a 60-second timeout and persists a pending marker before launch. Failed/interrupted checks are quarantined; retry is explicit or triggered by a changed VST3 fingerprint. Local XML writes use temporary-file replacement. Main audio hosting stays in process. Tests use isolated preference directories and a real VST3 probe to validate recursive discovery, cached startup, unchanged reuse, updates, failure/retry, interrupted markers, removals and folder persistence.

## Handwritten MIDI and instrument windows (0.18.0)

The original eighteen generated routing IDs remain stable. Handwritten MIDI tracks append IDs 18–31; the project stores only tracks that exist, while audio schedules, meters and instrument slots have a bounded 32-track capacity. Their built-in sound uses the existing polyphonic guide voices, with the same sample/plugin/effect graph, editing, automation, JSON and stem exports. Generation, harmony edits and automatic form renewal preserve handwritten notes; a lane’s explicit automation dice still operates on that lane. New tracks are normalised before publication and their addition is one undoable operation.

The instrument window owns a toolbar and reparents the existing non-owned instrument viewport. The same panel, owner project, selection, undo and audio engine are used. Closing hides the window and flushes pending control changes; selection changes refresh its instrument and section. The main arrangement/editor page stays in place. Global Lead octave is a stored engine option, relative to lane offsets, and is shared with the vendored Lead 0.1.1 engine.

[Piano regeneration audit](PIANO_VARIATION.md) records attack-only comparisons and the bounded candidate selection from the original genre generator.

Drum samples use a SamplerVoice variant that ignores tail-allowed note-off but respects forced stops, matching the guide kit’s one-shot behavior. Playback and private offline copies use the same voice type; pitched samples retain ordinary gate behavior. Manual drum notes start with the original generator gate lengths, while the editor displays and resizes their stored MIDI durations. Arrangement grid selection leaves transport untouched; section-name clicks seek and play. Floating instrument/plugin editors are always-on-top and reopened at screen centre.

## AI composer (0.21.0)

`AiComposer` builds provider-specific OpenAI Chat Completions, Anthropic Messages, Gemini generateContent and Ollama chat requests. The musical context is explicitly constructed from track roles/names, form, harmony and bounded note excerpts; it never serialises a Project for upload. Requests use an in-memory WebInputStream, native networking on Windows/macOS and lazily loaded libcurl on Linux. A watchdog cancels blocking I/O and enforces a four-minute online deadline (fifteen minutes for a local model). No credentials or prompts are written to files or process arguments.

Models return compact MIDI patterns and section placements rather than executable generator commands. Compilation validates ranges, references, enabled tracks and resource bounds, fits the form to the nearest requested bar, enforces monophonic Lead, resolves percussion mappings and preserves the original track instruments/mix. Targeted editing restores locked independent clips over the newly written parts. The preview is private until APPLY; musical project comparison prevents stale results overwriting intervening edits, while plugin state captured at apply time stays current. A single checkpoint supports Undo.

Optional `Settings::freeTempo` persists an AI composition’s 30–300 BPM without applying genre-preset working ranges. Legacy projects retain their previous policy; applying a genre clears the override. All original generation engines and presets remain unchanged.

## Studio (0.22.0)

`StudioWindow` uses the DAW theme and exposes four tabs targeting an existing track.
`StudioSettings` is the bounded, serialisable model; default projects keep the old
schema, while extended sampler or routing settings opt into schema 3. Loading
supports schemas 1, 2 and 3. The canonical family module remains pinned unchanged.

`SampleSynth` shares immutable decoded audio between live and offline engines.
Voices and Signalsmith FFT state are prepared outside the audio callback. Classic
playback uses interpolation; tempo sync uses independent pitch/time processing
and participates in the existing latency compensation. Gate, one-shot and slice
modes share crop, reverse, envelope, fades and filter processing. Drum slice
triggers retain distinct pitches and use the pitched MIDI editor.

`MixerBuses` applies two optional groups and post-group sends to shared reverb and
tempo delay. Master and its automation act after the returns. Offline export uses
the same graph and extends all stems equally for sampler and shared effect tails.

`MidiCapture` has separate bounded device/UI input queues and an audio-to-UI queue.
Large queue storage is heap allocated. The audio callback timestamps notes;
message-thread assembly handles sustain, onset quantisation, loop splitting and
non-destructive overdub or range replacement. A take is applied as one undo step.
Input overflow is reported and releases held voices.

`StudioIO` stores relative paths for local media, collects content-addressed sample
copies transactionally, and relinks by hash or unique basename. Presets include a
verified neighbouring sample folder. The original sample files are never edited.

### Local model setup (0.22.1)

The composer initially selects Ollama (one-time preference migration). A cancellable worker requests `/api/tags` with an eight-second deadline when opened or refreshed, without sending musical context or credentials. The model menu lists installed non-cloud models, preserving a previous choice only if still available. Composition is disabled until discovery succeeds. Address normalization accepts bare hosts and common API suffixes. Native Ollama error bodies are bounded and displayed for model/memory failures; online error bodies are not exposed. Local generation uses a 16K context, up to 8192 output tokens and disables Qwen3 thinking. Opening/closing or changing the service invalidates discovery callbacks; generation still uses the existing validated preview and explicit Apply transaction.
