# Changelog — Fase Groove Arranger

## 0.22.5

- Studio tuner measures the selected sample audio and displays detected note, cents and Hz during audition, MIDI input and song playback.
- Transpose and Fine edits are reflected in the reading; the MIDI trigger indication remains separate. Silence/noise have no fabricated note.
- Pitch analysis runs outside the audio callback; six-language tooltips and IT/EN guides explain tuning and source limits.

## 0.22.4

- Isolated Studio sample audition with Play, Stop and optional repeat; current sampler edits are heard live without changing song position or MIDI.
- Audition note feedback, lifecycle cleanup and localized tooltips. Sampler playback, track mixer and song transport remain separate.
- Use a plain ASCII separator in the Studio title to avoid a garbled character.

## 0.22.3

- Inactive Studio bus meters show localized Silence instead of special minus/infinity glyphs; active readings use numeric dB.
- Mouse-wheel and trackpad scrolling no longer change Studio parameters, bus levels, routing or menu choices accidentally. Waveform zoom/pan, scrollbar navigation and explicit drag/text editing remain available.
- Native regression verifies that scrolling leaves project data and Undo unchanged, including controls on inactive tabs. Documented recovery of a quiet kick without replacing its MIDI or sample.

## 0.22.2

- Open the sampler from the track inspector; sample-only targets in the sampler page preserve all-track MIDI recording and bus routing.
- Reorganize Studio into clear control groups with a persistent performance keyboard, note names, active-key/pad feedback and bus meters.
- Pointer-anchored wheel/pinch zoom, horizontal scrolling and double-click waveform fit.
- One-action transient/equal slicing with playable MIDI mappings, and an undoable sampler reset preserving file, notes and mix.
- Descriptive localized tooltips in Studio’s own window, respecting the global preference; updated IT/EN guides and regression checks.

## 0.22.1

- Local Ollama is the initial AI composer service; no API key is requested for local models.
- Automatically detect installed models with cancellable connection checks, normalize server addresses and explain missing models/server setup in the composer.
- Show Ollama memory/server errors, exclude cloud models from the offline list and lower local context memory requirements.
- Refresh IT/EN setup guides and privacy notes; add HTTP regressions for discovery, no-key requests and failures.

## 0.22.0

- Studio sampler: waveform trim, loops, envelope, filter, portable presets, slicing and independent tempo/pitch synchronization.
- MIDI recording: input device and virtual keyboard, count-in, overdub/replacement, quantization and undo.
- Two groups and two shared reverb/delay returns, using the same graph for playback and export.
- Verified sample collection and relinking, with backward-compatible project loading.


## 0.21.2 — 2026-10-26

- Pinned shared-module SHA and complete inventory checked offline and against its canonical source in CI; Lead included in default sync/check.
- Translation catalogs checked for duplicate keys, including multiline entries; intentional common/product overrides remain supported.
- Removed two shadowed translation rows, preserving intentional case-insensitive common overrides; restored complete Spanish, French, German and Portuguese loading messages.

## 0.21.1 — 2026-10-26

- Fix Piano's note-range slider in Harmony & Scales: both limit handles now appear at their actual positions and follow dragging, with the selected interval highlighted. The previous theme drew a stationary single-value handle although the MIDI limits changed.
- Native UI regression checks visible handles, hit targets, dragging both limits and the resulting MIDI. IT/EN guides explain the two handles.

## 0.21.0 — 2026-10-25

- Amber AI COMPOSER button with a new family sparkle icon and a separate composer window.
- OpenAI, Anthropic/Claude, Google Gemini and Ollama adapters, editable model selection, session-only API keys and cancellable background requests. Credentials and prompts remain in memory during HTTPS requests.
- The model writes actual MIDI motifs, transitions and arrangement sections; bounded validation, monophonic Lead, track mappings and nearest-bar duration fitting precede a reviewable preview.
- New-song and selected-track composition retain assigned instruments and mix. Targeted edits retain other tracks and locked clips, including independent timelines. APPLY supports optional playback, Undo and stale-project protection.
- AI projects retain their chosen 30–300 BPM outside factory genre limits, including save/load; choosing a factory genre restores its original tempo policy.
- IT/EN guides and privacy documents describe setup, API billing, Ollama, transmitted musical context and key handling. Local HTTP fixtures exercise real requests, failures and cancellation without API keys.

## 0.20.0 — 2026-10-25

- Click the section-name header to start playback from that exact position. Grid/clip clicks select and edit without seeking or interrupting playback; the numbered ruler retains cursor and loop controls.
- Undo keeps the selected clip when its timeline range is unchanged, independently of the playhead.
- Drum notes show their actual MIDI lengths and support right-edge resizing. Handwritten hits use the original generator gates, including two beats for Crash.
- Drum samples play their natural one-shot tails, in playback and private-copy audio export, matching the built-in kit. Forced transport stops still silence them.
- Instrument panels and VST editors stay on top until manually closed and reopen centred on screen.
- The main Piano Extensions selector immediately applies harmony changes, matching the original panel; both controls are checked against the resulting MIDI.
- Italian and English family guides document the updated interactions.

## 0.19.0 — 2026-10-25

- Handwritten MIDI tracks can be deleted from the inspector or track menu. Undo restores notes, routing, instrument/effect state and following tracks; generated instrument tracks remain protected.
- The inspector uses a compact panel icon. Hiding it returns its entire width to the arrangement; reopen it from the toolbar or with I.
- + SECTION opens a menu for Main, Intro, Breakdown, Build-up, Drop and Outro, inserting the chosen role as one undoable action.
- Mixer strips show the track name at the top and the built-in/sample/plugin selector at the foot, including compact strips.
- Piano/Bass/Strings controls sit inside the coloured playing frame with consistent padding.
- Regeneration automatically fits the song in the arrangement, or the clip length and generated pitch range in the MIDI editor.

## 0.18.0 — 2026-10-25

- Instrument panels open in a separate resizable window; the arrangement or MIDI editor stays in place, with playback and edits shared in real time.
- Add handwritten MIDI tracks with + MIDI (up to 32 tracks in total). They support piano-roll editing, built-in sound, sample/VST/AU, effects, automation, project persistence and stems; automatic generation preserves their notes.
- Rename tracks directly by double-clicking their inspector/mixer name, or through the existing track context menu. The inspector uses OPEN VST for the plugin editor.
- Lead global octave in the instrument panel, relative to the twelve lane offsets, using the same engine setting as Lead VST3/Standalone.
- Piano regeneration chooses a rhythm more distinct from the previous take, drawn from the original genre generator. Holds, accents and humanisation do not count as different attacks; saved takes and locked segments remain reproducible.
- Wider key display in the transport, including complete flat names such as Db.

## 0.17.0 — 2026-10-25

- Original Fase Groove Lead engine integrated as the eighteenth track: monophonic generation, shared harmony and chord duration, twelve phrase lanes, four guide synths, sample/plugin routing, automation and MIDI/audio stems.
- Existing seventeen-track projects load unchanged, with an empty Lead track appended. Original track IDs and MIDI remain stable.
- Inspector redesigned as compact track, sound, mix and clip cards, with a single-row source selector, clear labels and a contextual plugin editor. Existing actions, undo, collapse and resize remain functional.

## 0.16.2 — 2026-10-25

- The save / discard dialog on close, the messages and the rename box follow the Arranger's theme instead of the system default.

## 0.16.1 — 2026-10-25

- Loop in the ruler: drag its edges to resize it (snapped to the grid, at least one beat), drag the middle to move it, drag on an empty ruler to draw a new one; one undo per gesture, a click still moves the playhead.

## 0.16.0 — 2026-10-25

- New professional DAW look: centred transport with an LCD (position, tempo, metre, key), icon toolbar with tooltips, ruler with coloured section markers, clips with note previews, track headers with instrument icon and stereo meters, mixer with dB scale, stereo peak-hold meters and pan knobs, sectioned inspector, redrawn piano-roll keyboard. Nothing redraws while stopped.
- Updater and translations from the shared FaseGroove module (same checks and behaviour); two translation fixes (pt "OPÇÕES", it "Blocca A").

## 0.15.3 — 2026-10-24

- The piano comping now compiles from the Piano engine itself (one shared file); generated MIDI unchanged.

## 0.15.2 — 2026-10-24

- More varied piano: every new song (GENERATE SONG, a new genre) takes one of the genre's voicings and colours, and the random riffs answer the first bar with an early hit or a pick-up (Piano 0.31.3 engine).
- Vendored engines kept in step with the plugins by scripts/sync-engines.py; the Arranger's own engine changes are recorded in ThirdParty/ENGINE_PATCHES.md.

## 0.15.1 — 2026-10-24

- Every song already has automation: a new or opened project writes random automation on GENERATE SONG and when a track, section or clip is regenerated (switch it off in the lane menu).
- Automated lanes read in blue: lane title, lane menu, and the volume and pan of the inspector.

## 0.15.0 — 2026-10-24

- Automation beyond volume: track pan, master volume and hosted plugin parameters, drawn and edited like the volume lanes, undoable, saved and rendered in exports.
- Random automation (dice): build-up sweeps, risers, volume, pan and plugin-parameter moves following the sections; rolled again when the song, a track, a section or a clip is regenerated, respecting locks. On by default in new projects only.
- LOAD SAMPLE in the inspector opens and closes the sample browser; the browser can be resized and keeps its size.
- Drums and Guitar engines updated (Afro snare parts, audible random guitar grooves).
- Shared FaseGroove theme, circle of fifths and logo now come from the common module.

## 0.14.4 — 2026-10-24

- Lighter: the mixer, the instrument panel, the timeline and the piano roll redraw only what changes; instrument-panel knobs re-render the part once the gesture settles instead of on every step; translations are applied only when the language changes; fewer project copies and recalculations on every edit; the plugin settings stay idle when hidden.
- Drums and Guitar engines updated to 0.20.1 and 0.10.1. Generated music is unchanged.
- Unused leftovers removed (duplicate translations, controls that were never created, the old song-minutes field; old projects still open).

## 0.14.3 — 2026-10-24

- Loop: the loop button, L, section/clip loop and Ctrl+L switch the loop off again; a double-click on the ruler or the context menu clears the region. Undoable and saved with the project.
- LOAD SAMPLE in the inspector opens the built-in sample browser for that track.
- Track headers show the instrument in use: the plugin, the sample or the built-in sound.
- The inspector is rebuilt as one stable column.
- GENERATE SONG follows the current genre's form every time; timeline edits no longer freeze it; locked tracks and clips stay.
- Instrument panel changes re-render the part; harmony changes also re-render bass, strings and guitar (locks respected).
- MIDI editor: REGENERATE button (undoable). Pencil, select and eraser are icons.

## 0.14.2 — 2026-10-23

- Sample browser treats dot-prefixed files and folders as hidden on every platform, including Windows.

## 0.14.1 — 2026-10-23

- Fix Windows build of sample browser.

## 0.14.0 — 2026-10-23

- Sample browser (BROWSER, key B): your sample folders and favourites, saved; it reopens at the last folder. Breadcrumb, back/forward/up, search, type filter, sort by name, date, size or length, file info, preview with optional auto-preview, load by double-click, Enter, LOAD or by dragging onto a track. Folders scan in the background.
- Interface: one toolbar (editing tools on the left; GUITAR LEADS, INSPECTOR, BROWSER and MIXER on the right); the transport shows the position, LOOP and CLICK (metronome). The timeline has alternating track shading, bar and section lines, clearer A/M/S switches and a playhead marker; it no longer repaints while idle. The duplicate edit buttons are gone (the same commands are in the clip menu and on the keyboard).
- Engines updated to Piano 0.31, Drums 0.20 and Guitar 0.10; piano and bass, on separate tracks, no longer cut each other's notes.
- Playback: plugin delay compensation; loading a plugin or sample affects only its track; exports render on a private copy, so playback keeps going; edits while playing restart only the notes that changed; no clicks at loop wraps and seeks; plugin notes are released on stop, seek and loop; mono plugins play on both sides.
- Gapless MP3 export; a .fga opens from the command line or a second launch; closing asks to save; recovering the autosave no longer lets SAVE overwrite it; undo restores the right plugin states. Metronome on C, loop on L.

## 0.13.0 — 2026-10-22

- Drum grooves follow the song genre. Open hats, garage kicks, the deep rim and jackin' hats come from that style. Jackin' shuffle is 62%. The jack pocket stays: four-on-the-floor, clap or snare on 2 and 4, offbeat open hat, swung closed-hat accent and shaker.
- Guitar grooves draw both bars from the genre's own rhythm: offbeat stabs, short dance stabs, gospel eighths, laid-back space, and funk or jackin' sixteenths. A speaking accent remains in every bar.
- Piano riffs follow the song genre again. There is no new genre menu. House, '90s piano house and dance stabs keep the classic figures. The other styles each have their own riffs, and a new seed still writes a different one. Gospel chord dice walk by step. NEW RIFF keeps the genre's voicing and colour. String stabs are rewritten with the new riff. Guitar LEAD still uses the four-bar piano roll. Locked grid segments stay.

## 0.12.1 — 2026-10-22

- Jackin' House drums stay a jack groove on Generate Song, regenerate, random, variations and sections: four-on-the-floor kick, clap or snare on 2 and 4, open hat on the offbeats, swung 16ths around 56–62%, and a syncopated shaker. Tempo stays in 120–127. The beat does not turn into two-step, broken beat or garage.
- Song regenerate no longer pushes Jackin' up to 130 BPM or a triplet shuffle. Cowbell and tom keep playing through the phrase.
- Piano figures are the classic house riffs again, as in FaseGroove Piano 0.29.0. Genre no longer picks the piano rhythm, variation, fill, accent or progression. Regenerating piano or string stabs writes a different figure each time. A saved genre still opens and is ignored by the piano. Guitar LEAD keeps the four-bar piano roll. Guitar and drum genres are unchanged.

## 0.12.0 — 2026-10-22

- Strings play a harmony part in every genre that uses them. Pads and chord lines follow the progression through the verse, chorus and break, and they enter during the intro and outro instead of being removed from the arrangement.
- Regenerating the strings track, a section or a clip always writes notes. Jackin' still starts without a string bed; regenerating the strings track adds a pad. A locked track or clip is left as it is, and the part is saved with the song.

## 0.11.0 — 2026-10-22

- Plugin browser in the same style as the FaseGroove groove browser: manufacturers on the left, plugins on the right, search, favourites and recents. FaseGroove engines are the first group. A click loads the plugin; double-click or Enter closes the browser.
- Startup scan covers the default VST3 folders and one custom folder, including subfolders. The known-plugin list is saved as JUCE KnownPluginList XML. Unchanged files are not inspected again. Failed scans stay quarantined. RESCAN and RETRY FAILED stay in plugin settings.
- Each track can load one VST3/AU instrument and one insert effect. The project stores both plugins and their state.
- Random guitar grooves stay audible. Every bar keeps a speaking accent, plain 16ths cannot fall through the accent curve, and humanize cannot silence a hit. Ghosts stay quieter. Funky, jackin' and jackin' piano intros are no longer only a ghost scratch.

## 0.10.0 — 2026-10-22

- Piano random riffs, variations, breaks and build-ups stay in the song genre. Mix fills, accents and random progressions follow gospel, jackin', garage, afro, funky, neo soul and chill. House-family grooves keep their figures. The song genre drives this; the panel does not add a second genre menu.
- Engine panels no longer repeat the section menu or the track volume fader, and the piano panel no longer saves standalone `.fgapreset` files. Tempo, key, transport and export stay on the arranger. Stored parameters are unchanged.
- Guitar random grooves, turnarounds, section templates and mix fills stay inside the genre. Random patterns use that genre's voicing and lick; presets stay on the classic shapes. A slide on the downbeat is no longer dropped, and humanize no longer revoices every hit of the fourth bar.
- Drum fills, turnarounds and hat accents follow the genre. Garage, neo soul and chill keep their kick and hats in sections, and chill no longer gains a 16th shaker. A riff-seed change refreshes preset turnarounds, and Jackin' Piano no longer takes the Jackin' snare flam.
- Piano fills no longer index an empty scale run, and guitar-lead comping closes notes still ringing at the clip end.

## 0.9.0 — 2026-10-22

- GENERATE SONG also draws a new tempo inside the genre, density and swing within the style, and new 8-bar section lengths. Section order stays. Locks freeze the form; locked harmonic notes keep the key and progression.
- NEW CHORDS / RANDOM CHORDS write the new harmony into unlocked tracks. Quantise keeps note starts and ends inside the clip. Swing, density and humanize update the arrangement immediately. Split and paste no longer detach a timeline when the edit cannot fit.
- Duplicate a guide section with its notes. Humanize selected notes from the clip menu or the H key. Remove unused engine helpers that the arranger never called.
- GENERATE SONG chooses a different key, genre progression and bass figure. Locked harmonic notes retain the current harmony; partial regeneration stays in the song's harmony.
- Regeneration releases fixed groove/riff overrides, combines genre piano phrases, develops bass rhythm and draws fresh Jackin’ guitar patterns. APPLY preserves explicit settings and stored takes; locks, assignments, mix and untargeted notes remain.
- Regression checks compare piano/bass/guitar rhythms with feel disabled across all thirteen genres, including fixed presets and seeds; update generation feedback and IT/EN guides.

- Align distribution with Piano/Guitar/Drums: one platform ZIP plus Windows setup, a clean explicit runtime payload, linked guide resources and required licences. Consolidate CI diagnostics into one separate validation artifact.
- Track header clicks select without opening the engine; an amber gear beside Solo opens configuration. Add GUITAR LEADS to the main arrangement toolbar, preserving locks and Undo.
- Hide piano editing controls ignored by independent bass/string modes, guide controls ignored by samples/plugins, and duplicate routing/drum controls. Mode and sound changes refresh the available panel controls.

## 0.8.0 — 2026-10-21

- Persistent instrument catalogue grouped by manufacturer, ready immediately from the plugin selector. VST3 system/user defaults plus configurable personal folders; recursive discovery and path deduplication.
- Automatic first-launch and startup scans in the background. Cache unchanged VST3 bundles, detect updates/removals, and retain project assignments. Registered Audio Units remain available on macOS.
- Inspect plugin binaries in a child process with timeout and persistent failed/interrupted-scan quarantine. Settings include folder management, scan, cancel and retry controls in all six interface languages.
- Native integration checks for nested/overlapping paths, saved catalogue, unchanged-plugin reuse, updates, quarantine/retry and removal/rediscovery. Preserve the family settings layout, theme and existing manual loading.

## 0.7.2 — 2026-10-21

- Regeneration creates a persistent musical take for the requested song, track, section or clip, instead of mostly changing humanisation over the same riff. Track/song takes share a motif across sections and drum stems share a kit seed.
- Develop the selected bass figure with chord-safe root/fifth/octave answers and pickups, preserving its anchors and held notes. Save/reload, duplicate and deterministic APPLY retain the take.
- Fix RND LANE reusing the take on instrument pages. APPLY TRACK/CLIP still apply settings without rerolling; fixed presets, hand-edited grids and explicit riff seeds remain intentional constraints. Assigned instruments, mix, automation, locks and untargeted clips are preserved.
- Add musical diversity tests across all thirteen genres, scope isolation, repeatability and project persistence checks.

## 0.7.1 — 2026-10-21

- Family Windows setup: English/Italian wizard, Arranger branding, Start menu/optional desktop shortcuts and uninstall, adapted for the standalone application.
- Family update flow: download the matching package, INSTALL NOW / LATER / SKIP, silent setup upgrades, transactional portable replacement and automatic reopen.
- Stable Windows/macOS/Linux update ZIPs alongside versioned archives; preserve projects, samples and preferences. Verify GitHub asset SHA-256 before installation.
- Native updater rollback/integrity checks, real Windows install/upgrade/uninstall tests and updated IT/EN installation/privacy documentation.

## 0.7.0 — 2026-10-21

- Pointer-anchored wheel zoom in the arrangement and MIDI editors, keyboard zoom, horizontal/vertical scrolling and arrangement middle-button panning.
- Selection zoom and loop commands; manual navigation suspends playback following.
- MIDI note nudging, octave steps, cut, adjacent duplication and cursor-based paste with phrase boundary checks and Undo.
- Protect clip gestures from conflicting shortcuts; avoid empty Undo steps for note selection and duplicate history entries when generating into an empty track; retain the cursor for generation.
- Keep pasted and duplicated timeline groups fully selected for subsequent edits.
- Updated six-language tooltips, native checks and IT/EN Markdown guides.

## 0.6.0 — 2026-10-21

- Independent timeline clips: move/copy between tracks, split at cursor, cut/copy/paste, duplicate, delete and multi-selection on a track, with a snap grid and Undo.
- The Intro/Main/Break/Build-up/Drop/Outro lane is now an editable guide. Dragging guide edges or inserting/removing/reordering guide sections preserves clip positions and automation.
- Independent clips retain their full instrument MIDI editors, loop resizing, generation controls and WAV/MP3/MIDI export. Edited projects use .fga format 2; format 1 remains readable.

## 0.5.2 — 2026-10-21

- Resize each track vertically from the lower edge of its header. Independent saved heights, default reset by double-click, Escape cancellation and aligned automation lanes. MIDI and playback position are unchanged.

## 0.5.1 — 2026-10-21

- Drag clip edges to resize individual MIDI loops or section-header edges to resize every track together. Loop source is retained when shortening, repeated without time stretching when extending, saved in .fga and editable in the full MIDI editor. Overlaps are masked on the same track; Undo and Escape are supported.
- Resize the inspector by dragging its right edge and the mixer by dragging its header; dimensions are remembered. Double-click either resize area to restore its default size.
- Track Inspector header arrow collapses the side panel to a narrow restore rail, recovering horizontal track space; the arrow stays accessible while scrolling the controls.
- Mixer header button collapses the faders into a slim bar, recovering track space; SHOW MIXER restores the previous height. Existing MIXER/F3 toggles remain available.
- Click anywhere in an arrangement clip to place playback precisely; Play/Space starts there, and playback already running jumps immediately. Works after horizontal scrolling.
- Arrangement and MIDI rulers retain fractional beat positions. Seeking beyond an enabled loop continues from the chosen position; seeking back into it resumes looping.
- Native regression checks cover paused/live seeking, scrolling, loop exit/re-entry and starting inside held MIDI notes.

## 0.5.0 — 2026-10-21

- DAW workspace with side inspector, seventeen-channel mixer and master, dB faders, pan, mute/solo and master overload indicator.
- Bar ruler, Shift-drag loop locators, clip selection, double-click full MIDI editing, drag swap/copy, fit-to-song and playback follow.
- Fix loop persistence and section edits, automation timing on section resize, stale meters on pause, grouped MIDI movement at boundaries, dropped-file targeting, failed Save As changing the active path, cross-project undo history and slider keyboard edits missing undo.
- Faders update the audio mix without reordering or regenerating the entire MIDI on every movement. Updated IT/EN guides, tooltips and six languages; original family theme, fonts and instrument panels retained.

- Smooth live volume/pan transitions over 10 ms to avoid abrupt gain steps while moving faders.

- Handle disabled audio outputs without null-pointer access; newly assigned drum samples use the track MIDI mapping as their initial root note.

## 0.4.7 — 2026-10-20

- Jackin’: original-library chopped hooks, more articulated Wurlitzer bass, strings off by default and alternating percussive guitar in the requested funky/disco direction. Other genres remain unchanged.
- Audit includes full Jackin’ song and separate drum, bass and keyboard previews; four-seed regressions and user-enabled strings checked.

## 0.4.6 — 2026-10-20

- Correct drum balance for the original engine’s existing lane calibration: snare, clap and cymbals are no longer attenuated twice.
- New-project master at 0.45 and true-peak verification of all thirteen MP3 previews alongside full-song PCM checks.

## 0.4.5 — Genre sound palettes and band balance

- Thirteen curated palettes using the original keyboard, guitar and drum guide engines: genre-specific sounds, kit, tone, voicing, gate, effects, levels and instrument roles.
- Dedicated guide bass stays centred instead of inheriting keyboard note panning; low-pass follows its tone. User samples and VSTs bypass this guide processing.
- Piano/guitar answers and phrase-based string/percussion entrances reduce constant layering. User patterns, locked material, routing and manual MIDI remain editable.
- Full-song audit plus thirteen eight-bar MP3 previews and stereo bass checks.

## 0.4.4 — Family guide alignment

- Use the Piano manual's nine-chapter structure, language link, centered logo/screenshots, separators, control tables, recipes, FAQ style and creator/music closing in both manuals.
- Preserve the full Arranger instructions for instruments, MIDI editing, genre-specific forms, routing, automation and export inside the corresponding family chapters.
- Refresh screenshots from the verified 0.4.3 app. Native offline reader supports the original Markdown's centered image markup, italics and nested headings/lists.

## 0.4.3 — Track volume automation

- Optional volume lanes on all 17 tracks: draggable points, beat snap/free timing, curved segments, bypass, clear and undo/redo. Hidden and bypassed by default; hiding never mutes automation.
- Sample-accurate post-instrument gain for internal engines, user samples and VST/AU; shared playback and mix/stem WAV/MP3 export graph. MIDI notes/CC stay unchanged.
- Optional project fields preserve envelopes; old projects default to bypass. Section insertion, duplication, deletion and reordering move points with the musical material.
- Remove arranger-added note-velocity fades in intro/outro and organic lifts. Preserve original engine articulation and expression.
- Six-language controls/tooltips and adapted Markdown manuals. Core, persistence, native editing gestures and audio-envelope checks.

[Italiano](CHANGELOG.it.md)

## 0.4.2 — 2026-10-20

- Prevent guide-mix clipping in new songs with 6 dB more master headroom; audit raw peaks before encoding and reject non-finite audio.

- Club arrangements: progressive mix-in/out inside the song, working BPM ranges by genre, and groove-preserving soulful/organic lifts.
- Keep each song’s motif across sections; preserve the DJ phrase grid and harmonic continuity with odd chord counts.
- Correct sub-tick outro spill, factory grooves left over after a genre change, and inconsistent normalisation in MIDI/audio exports.
- Full-song MIDI/audio audit for all thirteen genres, reference Soulful MP3 and source-based arrangement notes.

## [0.4.1] — Unreleased

- Restore the Guitar family OPTIONS overlay: SETTINGS / ABOUT geometry, controls, links and credits.
- Add interface-size preference and optional GitHub release checks; preserve audio, guide, privacy and licence access.
- Reuse original IconButton transport, undo/redo, arrows and dice; align instrument grouping and control proportions with reference editors.
- Separate Branding as in the other apps and introduce an original track/clip symbol for Arranger.
- Add Italian README and Markdown changelogs; remove the separately invented HTML guide export.

## [0.4.0] — Previous development build

- Standalone Piano, Drums and Guitar engines, multitrack arrangement, MIDI editors and MIDI/WAV/MP3 export.
- Extended instrument panels, Piano musical tools, VST3/AU hosting and `.fga` projects.
