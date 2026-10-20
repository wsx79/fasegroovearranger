# User guide – Fase Groove Arranger

[Versione italiana](GUIDA.md)

Fase Groove Arranger **creates the MIDI for a whole song**: choose a style and a chord progression, listen to piano, bass, strings, drums and guitar, change what you like, then **export the song or its separate tracks** with your own sounds.

The grooves come from Fase Groove Piano, Drums and Guitar. Here they play in one song, sharing its key, tempo and arrangement.

**Contents**

1. [Installation](#1-installation)
2. [Your first song in 60 seconds](#2-your-first-song-in-60-seconds)
3. [The window, section by section](#3-the-window-section-by-section)
4. [Building a song: intro, break, drop](#4-building-a-song-intro-break-drop)
5. [Chords, key and feel](#5-chords-key-and-feel)
6. [Tracks and your own sounds](#6-tracks-and-your-own-sounds)
7. [Editing the notes](#7-editing-the-notes)
8. [Guitar soloist and piano accompaniment](#8-guitar-soloist-and-piano-accompaniment)
9. [Recipes to get started](#9-recipes-to-get-started)
10. [Saving and exporting](#10-saving-and-exporting)
11. [Shortcuts](#11-shortcuts)
12. [Questions and troubleshooting](#12-questions-and-troubleshooting)
13. [Listen to my music and credits](#13-listen-to-my-music-and-credits)

---

## 1. Installation

Download your platform package and extract it. The app works on its own: **you do not need a DAW or the three Fase Groove plugins installed**.

| System | Package | Start |
|---|---|---|
| Windows | `.zip` | Open the extracted folder and run `Fase Groove Arranger.exe`. |
| macOS | `.tar.gz` | Extract and open `Fase Groove Arranger.app`. The package is universal: Intel and Apple Silicon. |
| Linux | `.tar.gz` | Extract and run `Fase Groove Arranger`. Keep the executable permissions. |

From GitHub Actions, the artifact ZIP contains the package shown above: extract that too. These packages are portable and have no installer or distribution signature.

**First launch:** open **AUDIO**, choose your output and a buffer suited to the device. The app uses audio output; it does not open the microphone.

> Start with the guide sound. You can hear and build the whole song before choosing your instruments.

## 2. Your first song in 60 seconds

1. Choose **Soulful** in the genre menu.
2. Keep **5.0 min** or set the duration you need.
3. Click **NEW ARRANGEMENT**: the intro, main parts, break and outro appear.
4. Press **PLAY** or the **space bar** and listen.
5. Click a **piano clip** in the main part. Its large piano roll opens; move a note or change its velocity.
6. Return with **< ARRANGEMENT**. Click the **Piano track name** and choose a guide sound, sample or your own VST.
7. Click **SAVE**, name the project, then **EXPORT** for WAV, MP3 or MIDI.

Want another take? **NEW TRACK** changes just the selected instrument; **NEW CLIP** changes only that track in that section. **UNDO** takes you back to the previous attempt.

> Found a part you like? Open its panel and enable **LOCK TRACK** or **LOCK CLIP** before trying other takes.

## 3. The window, section by section

![The Fase Groove Arranger window](images/guide_arrangement.png)

The top contains project commands, transport and the controls shared by everyone: **tempo, key and scale**. Below are genre, duration and chord progression. The large area is the song, with sections across and instruments down the side.

| Where | What you find |
|---|---|
| **NEW · OPEN · SAVE · SAVE AS** | Start, open and save your songs. |
| **PLAY · STOP · LOOP** | Listen, return to the beginning and repeat the selected region. The blue cursor shows your position. |
| **Genre and duration** | The song's character and target length, from 4 to 12 minutes. |
| **PROGRESSION · EDIT CHORDS** | Ready-made progressions and individual chords. HARMONY shows the current progression. |
| **Arrangement** | Each rectangle is a clip: one instrument's notes in one section. |
| **Track name** | Opens its instrument panel, with groove, playing, sound and mixer controls. |
| **Clip** | Opens the large MIDI editor for just that track and section. |
| **AUDIO · OPTIONS · GUIDA** | Output device; information, credits and privacy; the guide. |

Colours follow the instruments: **amber** for piano and kick, **cyan** for bass and backbeats, **green** for cymbals, **violet** for percussion and **red** for strings and tom/crash. Guitar playing lanes keep their own colours.

**M** and **S** beside a track name mute and solo it without opening the panel. Click a section header to start from there. **ZOOM** changes how much of the song you see.

## 4. Building a song: intro, break, drop

**NEW ARRANGEMENT** writes the song form and MIDI for the chosen genre. Its length rounds up to a musical phrase: a five-minute target can become, for example, 5:14.

| Part | How to use it |
|---|---|
| **Intro / Mix in** | The song's entrance, 16 or 32 bars. Its first 16 bars leave out piano, bass, strings and guitar for DJ mixing. |
| **Main** | The principal groove. Establish the song's progression and sound here. |
| **Breakdown** | Some breathing room, changing the space and the instruments' roles. |
| **Build-up** | A rise towards the return, in styles that call for it. |
| **Drop** | The full groove comes back. It need not sound exactly like the first main part. |
| **Outro / Mix out** | The exit, 16 or 32 bars. Its last 16 bars again leave harmonic space for mixing. |

The form follows the style. **Deep and Chill** develop through longer passages; **Jackin'** uses short breaks and returns; **Garage & UK** has two drops and a cut; **Soulful and Gospel** use answers and reprises; **Afro & Latin** develops its percussion; **Funky & Disco** moves between verses and choruses.

### Change only what you need

| Button | What changes |
|---|---|
| **GENERATE SONG** | New takes on all unlocked tracks, keeping the existing sections. |
| **NEW TRACK** | The selected instrument throughout the song. |
| **NEW SECTION** | This section across all unlocked tracks. |
| **NEW CLIP** | Just the selected track in this section. |
| **NEW ARRANGEMENT** | Rebuilds the form and notes for the genre and duration. Unlock tracks and clips first. |

**Right-click a section:** rename, duplicate with its notes, move, remove, change length or musical role. **+ SECTION** adds a Main part after the selected section. Extending a section leaves room to fill: regenerate or write the extra notes. Shortening it trims notes at its new end.

> Try this: duplicate a drop, change only the bass in the second copy, then make the piano sparser. Keep a recognisable idea and change who carries it.

## 5. Chords, key and feel

**The song has one shared key and scale.** Piano, bass, strings and guitar use the same progression. There is no need to LINK the individual instruments.

| Control | What it does |
|---|---|
| **Key / Scale** | The root and mode, at the top beside tempo. |
| **PROGRESSION** | A progression from the Fase Groove Piano library. Some also choose a suitable scale. |
| **NEW CHORDS** | Prepares another degree progression in the selected scale. |
| **EDIT CHORDS** | Change each chord: a scale degree, or major, minor or dominant on a chromatic root. One to eight chords, one per bar. |
| **Extensions / Voicing** | Chord colour and the piano notes' arrangement. |
| **DENSITY** | From a few notes to a busier groove. |
| **SWING** | Moves alternating sixteenths, from straight towards shuffle. |
| **FEEL / HUMANIZE** | Small timing and velocity differences. |

**Tempo** changes playback speed immediately. Other choices prepare the next generation; the notes you have written stay in place until you apply them.

Inside an instrument panel, **APPLY TRACK** or **APPLY CLIP** applies its parameters while keeping the current groove take. The **NEW** buttons in the arrangement choose a different take.

> After changing the key or progression, regenerate the relevant harmonic instruments together. A locked part keeps its notes, even when the rest of the song changes harmony.

## 6. Tracks and your own sounds

The first thirteen tracks are the drum pieces: **Kick, Snare, Clap, Rim, Closed hat, Open hat, Ride, Shaker, Tambourine, Conga, Cowbell, Tom, Crash**. Then come **Bass, Piano, Strings and Guitar**. The guitar stays together on one track.

![The piano instrument panel](images/guide_piano.png)

Click an instrument's **name** to open its panel. Choose how to hear it at the top; below are groove, feel, playing and fills. **EDIT MIDI** opens the notes; **< ARRANGEMENT** returns to the song.

| Instrument | Where to start |
|---|---|
| **Piano** | Groove, gate, register and strum; then movement, top line and range to refine the right hand. |
| **Bass** | A bass line and its feel: classic, sub or sub with gaps on the beat. |
| **Strings** | A strings style, from sustained support to a rhythmic part. |
| **Drums** | Just the selected piece: groove, velocity, MIDI note and pocket. |
| **Guitar** | Strums, let ring, room for band, pushes and the twelve-lane mixer. |

### Guide sound, sample or VST

| Button | What you choose |
|---|---|
| **BUILT-IN** | Return to the guide sound. Piano and bass have seven keyboard tones; strings, four guitars and four drum kits have their own sounds. |
| **LOAD SAMPLE** | Your WAV, AIFF, FLAC or OGG, up to 60 seconds. The file stays on disk; the project remembers its path. |
| **LOAD VST3 / AU** | An instrument installed on your computer. Choose the `.vst3` bundle; macOS also lists installed Audio Units. |
| **OPEN VST** | The loaded instrument's own interface. |

**GAIN, PAN, MUTE and SOLO** belong to the track. **TONE, REVERB, CHORUS and PUMP** change only the guide sound; use your VST's controls for its sound. The groove controls still write the MIDI sent to it.

A pitched sample starts with root MIDI 60; drums use the piece's note. **Right-click the track name → sample root** changes it. This is a single sample transposed across the track's notes, not a multisample library.

## 7. Editing the notes

Click a **clip** to open the full editor. Choose which part to edit in the section menu; **LOOP SECTION** repeats it while you work. Each editor shows only its own track. A drum piece has one row.

| Action | Result |
|---|---|
| **Click empty space** | Add a note on the grid. |
| **Drag a note** | Move it in time; in the pitched piano roll, also change its pitch. |
| **Drag its right edge** | Change the length of pitched notes. |
| **Alt + drag vertically** | Change velocity without moving the note. |
| **Shift + click** | Add or remove a note from the selection. |
| **Right-click / DELETE / Backspace** | Delete a note. In the guitar lane view, right-click removes the strum gesture. |
| **Ctrl/Cmd + A** | Select all notes in the clip. |
| **QUANTISE** | Align to the selected grid: 1/4, 1/8, 1/16 or 1/32. |
| **−12 / +12** | Transpose pitched notes by an octave. |
| **VEL** | Change selected-note velocity; with no selection, set it for new notes. |
| **COPY / PASTE** | Copy selected notes, or the whole clip, and paste at the same section-relative positions. |

Quantise and transpose affect the selection, or the whole clip when nothing is selected. **ZOOM** opens up timing detail. **Escape** returns to the arrangement.

**IMPORT MIDI**, or dropping a `.mid` onto the window, replaces the selected clip's notes. Project tempo stays as set in the Arranger. In a drum track, imported notes are mapped to just that piece. **UNDO** restores the previous clip.

## 8. Guitar soloist and piano accompaniment

![The guitar's twelve playing lanes](images/guide_guitar.png)

Edit the guitar as in Fase Groove Guitar: **twelve ways of playing the same chord, all on one track**.

| Lane | What it plays |
|---|---|
| **DOWN / UP** | Down-strums of the chord and up-strums on the high strings. |
| **STAB / CHANK** | Short chords and very short funk chops. |
| **SCRATCH** | A muted percussive hit to fill rhythmic spaces. |
| **SLIDE / SWELL** | Slides into the chord and gradually swelling entries. |
| **ARP / LICK** | One string at a time and phrases over the chord. |
| **OCTAVE / POWER / ROOT** | Octaves, power chords and low-string roots. |

Click a lane to add a gesture on that bar's chord. Its notes select and move together. **NOTES / LANES** switches to the piano roll when you want to edit individual notes.

### Who carries the progression?

In the guitar panel or editor, **SOLISTA** changes the roles. With **SOLISTA** enabled, guitar carries the groove and piano leaves space, playing accompaniment and answers. Piano and guitar update while respecting generation locks. **UNDO** restores the previous roles.

![The guitar mixer](images/guide_mixer.png)

The twelve lanes sit side by side in the mixer: **level, octave, mute, swing share and push/pull**. Push/pull is in milliseconds: negative is earlier, positive later.

> Try **Room for band** to leave the bass its space. For a more percussive guitar, shorten strums and reduce **Let ring**. Listen with the piano, not only in solo.

## 9. Recipes to get started

### Soulful: piano in front, guitar in the spaces

1. Choose **Soulful**, five or six minutes, then **NEW ARRANGEMENT**.
2. Start the piano with **Rhodes (FM)** and seventh or ninth chords.
3. Listen to the main part. If it is too busy, lower piano density and click **APPLY CLIP**.
4. Keep guitar in accompaniment and mute lanes that cover the piano.
5. Lock the piano take you like, then try **NEW TRACK** on bass.

### Deep: fewer notes, more room

1. Choose **Deep** and generate the form.
2. Try a compact piano register, longer gate and fewer notes.
3. Choose a **Sub** bass feel. Keep strings as support and listen to the break.
4. Duplicate a main part and change just one or two instruments in the reprise.

### Jackin': short grooves and strong returns

1. Choose **Jackin'** or **Jackin' Piano**.
2. Start with short piano gate and more swing.
3. Strip back the break to set up the return: a **NEW CLIP** on piano may be enough.
4. Keep the kick recognisable and compare the return with what came before using **LOOP SECTION**.

### Funky & Disco: let guitar lead

1. Choose **Funky & Disco** and generate the song.
2. Open guitar and enable **SOLISTA**.
3. Listen to DOWN, UP, CHANK and OCTAVE in context; lower lanes that overlap too much.
4. Try strings in the chorus and leave more space in the part before it.
5. Check the outro: the final sixteen bars should stay clear of harmonic instruments for mixing.

## 10. Saving and exporting

**SAVE / SAVE AS** writes a `.fga` project: sections, notes, parameters, locks, mixer and VST state. Samples and plugins stay at their paths; keep the files used in the song.

An autosave is written once a minute when work has changed. **AUDIO → restore autosave** opens it; **open autosave folder** shows its location. Save separately named copies for versions you want to keep. UNDO retains up to forty steps while the app is open.

| In EXPORT | What you get |
|---|---|
| **MIDI** | The multitrack song, one track or separate files for all tracks. Includes tempo and section markers, not the sounds. |
| **WAV** | Mix, one track or all stems, in 24-bit stereo. Use this for further mixing. |
| **MP3** | Mix, one track or all stems, stereo 320 kbps / 44.1 kHz. The encoder is included. |

All WAV stems start at the song's beginning, have the same length and include a three-second tail. Even an empty track produces an aligned file. Mix export follows mute/solo; a stem exports even when its track is muted and retains gain, pan and master level.

> Choose WAV to bring stems into a DAW. MP3 adds codec delay: useful for listening and sharing, rather than precise track alignment.

## 11. Shortcuts

| Keys | Action |
|---|---|
| **Space** | Play / pause. |
| **Escape** | Return from the editor to the arrangement. |
| **Ctrl/Cmd + S** | Save. |
| **Ctrl/Cmd + Shift + S** | Save as. |
| **Ctrl/Cmd + Z** | Undo. |
| **Ctrl/Cmd + Shift + Z / Ctrl/Cmd + Y** | Redo. |
| **Ctrl/Cmd + A**, in the editor | Select the clip's notes. |
| **Delete / Backspace**, in the editor | Delete the selection. |

In this guide, **Ctrl/Cmd + F** focuses search. The arrows beside search move between results; **A− / A+** changes text size. Click a screenshot to open it larger.

## 12. Questions and troubleshooting

### I changed key, but still hear the old chords

You have prepared a new key while the clips still contain their previous notes. Regenerate the relevant harmonic instruments. Check locks: a locked part keeps its notes.

### I cannot hear anything

Check **AUDIO**, output device, PLAY, master level and MUTE/SOLO. Click **BUILT-IN** in the track panel to try the guide sound. If that works, check the assigned sample or VST.

### A VST or sample cannot be found

The project remembers its path, but the file may have moved or may not be installed here. Relink it with **LOAD SAMPLE** or **LOAD VST3 / AU**. Check the plugin's architecture and its authorisation. Missing instruments must be relinked before audio export.

### How do I keep edited notes while trying other takes?

Enable **LOCK CLIP** for that part, or **LOCK TRACK** for the instrument. Use NEW on other elements. You can still edit locked material manually.

### Why is the duration slightly different?

The song closes a musical phrase rather than cutting it short. Changing BPM afterwards also changes the length in minutes. Use **NEW ARRANGEMENT** to rebuild the form at the new tempo.

### Where are recording, automation and effect chains?

This version works on MIDI arrangements. Audio/MIDI recording, automation, audio editing, buses/sends, external effect chains and plugin delay compensation are not present. Export MIDI and WAV to continue in your DAW. Some advanced Fase Groove plugin controls have not yet been brought into the Arranger.

### My MIDI file will not import

Use a PPQ MIDI file; SMPTE files are not supported. Import fills the selected clip up to its section length; it does not reconstruct an entire external song automatically.

## 13. Listen to my music and credits

**Fase Groove Arranger is by Fabio 'Pitch' Serra / FS Pitch, DJ & Producer.**

[Listen to Fabio 'Pitch' Serra on Spotify](https://open.spotify.com/artist/29iQGuXML8LEw3xRp1dNRP)

[Buy me a coffee on Ko-fi](https://ko-fi.com/fabios1979)

[Support the project with PayPal](https://paypal.me/fabios79)

Development assistance: **Claude (Anthropic)** and **Codex (OpenAI)**, AI coding assistants. Open credits, licences and privacy through **OPTIONS**.

[Source code and the Fase Groove family](https://github.com/wsx79/fasegroovearranger)

The app is free and open source, **GNU AGPL v3 or later**, built with JUCE and the Inter typeface. It works offline with no accounts, analytics or music uploads. External links open your browser only when you choose them. Your samples and plugins keep their own licences and terms; Privacy and Licences are included in the package.
