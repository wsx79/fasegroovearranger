# Changelog — Fase Groove Arranger

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
