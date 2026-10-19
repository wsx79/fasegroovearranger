# Fase Groove Arranger — English guide

Fase Groove Arranger combines the original Piano, Drums and Guitar generation engines in a native standalone MIDI workstation. Its 17 instrument tracks share a song arrangement and harmony. The three plugins do not need to be installed.

## Run the desktop packages

Download your platform artifact from GitHub Actions. Extract the artifact ZIP, then the distribution archive inside it: ZIP on Windows, tar.gz on macOS/Linux. Launch the .exe, .app bundle or Linux executable. Unix archives preserve executable permissions. An external DAW and microphone access are not needed. Development packages have no installer or distribution signing.

## Compose and arrange

Choose one of the original 13 genres, a key, scale, chord colour, voicing and progression. Click GENERATE SONG. The initial form is Intro 8 bars, Main 16, Breakdown 8, Build-up 4, Drop 16 and Outro 8; Neo Soul and Chill have shorter intro/outro sections. The engines apply their original section behaviours, variations and fills.

NEW TRACK regenerates the selected instrument throughout the song. NEW SECTION regenerates that section across all unlocked instruments. NEW CLIP changes only the selected instrument/section cell. LOCK TRACK and LOCK CLIP protect material from generation while allowing manual editing. UNDO/REDO keeps up to 40 in-memory steps.

Generation settings apply to the next generation; existing notes stay editable and keep their current pitches. Tempo changes playback immediately. After changing harmony or key, regenerate the relevant instruments together to keep them harmonically aligned. Locked material keeps its existing notes. EDIT CHORDS edits each slot as a scale degree or a chromatic major/minor/dominant chord and sets the progression length from 1 to 8 chords. NEW CHORDS prepares another progression in the selected scale; the progression menu uses the original piano library.

Right-click a section header to rename, duplicate its MIDI, remove, reorder, change its musical function or change its length. A longer section needs regeneration or editing to fill the extra time. Shortening trims notes at the boundary. + SECTION inserts a generated Main section. Projects support 1–64 sections, each 1–128 bars, in 4/4.

## Edit MIDI

Select an arrangement cell to open its piano roll. Click empty space to add a note, drag a note to move it, drag its right edge to resize, or Alt-drag vertically to change velocity. Shift-click selects multiple notes. Right-click, Delete or Backspace deletes. Ctrl/Cmd+A selects the clip's notes while the piano roll is focused.

QUANTISE uses the selected grid (quarter through 32nd notes). -12/+12 transpose by an octave. VEL changes selected notes or sets the velocity of new notes. With no selection, quantise/transpose apply to the complete clip. COPY copies the selection, or the entire clip when no notes are selected. PASTE preserves section-relative timing. Notes are trimmed to section bounds and same-pitch overlaps are reconciled for reliable note-offs.

IMPORT MIDI or drag a PPQ .mid file into the application to replace the selected clip. All imported note tracks are merged into that clip and cropped to the section. Project tempo stays unchanged. SMPTE MIDI is not supported.

## Assign instruments

The built-in sounds are the synthesized guide instruments from the original applications, including electric/acoustic-style keyboards, organs, strings, a plucked guitar and four drum kits. Guitar articulations remain together on one track; each drum is a separate track.

LOAD SAMPLE assigns a personal WAV/AIFF/FLAC/OGG sample of up to 60 seconds. Pitched instruments default to root MIDI 60; drums default to their GM note. Right-click the track to change the sample root. The sampler transposes a single sample; it does not implement multisample libraries.

LOAD VST3 / AU assigns one native instrument plugin to a track. Select the actual .vst3 bundle. On macOS the installed Audio Units menu lists system-registered instruments. Windows/Linux support VST3; macOS additionally supports Audio Units. OPEN VST opens the plugin's editor or generic parameter editor. Plugins must match the application's architecture and have their own installation/licence. No third-party plugin binaries or sample libraries are bundled. Plugins run inside the application process; save before trying a new instrument.

The generation engine always remains internal Fase Groove code; your instrument receives MIDI and supplies its sound. Plugin state is saved in the project. External samples/plugins are referenced by path, not embedded. Relink missing instruments with the load buttons or explicitly choose the original guide sound from the track menu. Missing instruments must be resolved before audio export.

## Playback and mixing

PLAY/Space toggles playback; STOP returns to the beginning. |< rewinds. Click a section header to seek. Held notes are chased when playback starts in the middle of a chord. LOOP toggles the loop region; LOOP SECTION sets that region to the selected section.

Each track has gain, pan, mute, solo and a meter. The master affects all outputs. Keep headroom: the final safety ceiling can distort an overloaded mix. AUDIO selects the output device, sample rate and buffer. The application opens no microphone input.

## Save and export

SAVE/SAVE AS writes `.fga` projects containing the notes, arrangement, generation settings, locks, mixer and instrument state. Saves use a temporary file and atomic replacement. Unsaved work is copied once per minute to `FS Pitch/Fase Groove Arranger/autosave.fga` in the user application-data directory. AUDIO opens that folder or restores the autosave. Keep separate saved versions for long-term history.

EXPORT writes a complete multitrack SMF MIDI file, one instrument MIDI file or separate MIDI stems. MIDI includes tempo, 4/4 signature, key/scale metadata, section markers and track names at 960 PPQ. Drums use GM channel 10. MIDI contains notes, not plugin sound.

Audio export supports a stereo 24-bit WAV at the current device sample rate, or stereo 320 kbps / 44.1 kHz MP3 with the bundled Shine encoder. Export the mix, one instrument or all 17 stems. Every stem starts at the song beginning and includes three seconds of tail. Empty tracks produce matching silent files. Mix export respects mute/solo; individual stems ignore mute/solo and retain track gain/pan/master. Batch exports create a dated subfolder. Playback stops during rendering; the progress dialog supports cancellation.

WAV is the lossless format for further production and exact stem alignment. MP3 adds codec delay/padding and is intended for sharing. Plugins with internal randomisation or round-robin samples may vary between renders. Plugin latency compensation is not implemented.

## Shortcuts, credits and privacy

Space: play/pause. Ctrl/Cmd+S: save. Ctrl/Cmd+Shift+S: save as. Ctrl/Cmd+Z: undo. Ctrl/Cmd+Shift+Z or Ctrl/Cmd+Y: redo. Piano roll shortcuts apply when the editor is focused.

ECOSYSTEM opens credits, privacy and third-party notices. Creator: Fabio 'Pitch' Serra / FS Pitch, DJ & Producer. Original ecosystem code: Claude (Anthropic), AI coding assistant. Arranger integration: Codex (OpenAI), AI coding assistant. Original artist logo, Spotify and donation links are retained. The application is AGPL-3.0-or-later software.

The app is offline with no accounts, analytics, telemetry or music uploads. It reads files you select and writes local projects, autosaves and exports. External links open your browser only when clicked. Third-party plugins have their own privacy/permission terms. See PRIVACY.md and THIRD_PARTY_NOTICES.md.

Version 0.1.0 does not include audio/MIDI recording, audio editing, automation lanes, time stretching, effects chains, buses/sends, controller recording, signatures other than 4/4, plugin delay compensation or sandboxed plugin scanning. It is an initial MIDI arranging workstation, with compatibility requiring validation against your actual plugins. Development packages are unsigned and have no installer.
