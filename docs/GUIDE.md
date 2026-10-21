# User guide – Fase Groove Arranger

*🇮🇹 [Versione italiana](GUIDA.md)*

<p align="center"><img src="logo.svg" alt="Fase Groove Arranger" width="560"></p>

Fase Groove Arranger **writes MIDI** arrangements for soulful, deep, jackin’ house and the other
family genres: pick a key, chords and a groove, listen with the guide sounds, then
**export MIDI or audio to your DAW**, or use your own samples and VST3/AU instruments directly.

**Contents**

1. [Installation](#1-installation)
2. [Your first groove in 60 seconds](#2-your-first-groove-in-60-seconds)
3. [The interface, section by section](#3-the-interface-section-by-section)
4. [Exporting MIDI and audio to your DAW](#4-exporting-midi-and-audio-to-your-daw)
5. [Using instruments in real time](#5-using-instruments-in-real-time)
6. [Building a track: intro, break, drop](#6-building-a-track-intro-break-drop)
7. [Soulful and jackin’ house recipes](#7-soulful-and-jackin-house-recipes)
8. [FAQ and troubleshooting](#8-faq-and-troubleshooting)
9. [Listen to my music](#9-listen-to-my-music)

---

## 1. Installation

This guide covers **v0.5.1** of Fase Groove Arranger. Download the package for your system from
the [latest release](https://github.com/wsx79/fasegroovearranger/releases/latest) and extract it.

### Manual installation (zip / tar.gz, all systems)

Download your platform package and extract it. The app works on its own: **you do not need a DAW or the three Fase Groove plugins installed**.

| System | Package | Start |
|---|---|---|
| Windows | `.zip` | Open the extracted folder and run `Fase Groove Arranger.exe`. |
| macOS | `.tar.gz` | Extract and open `Fase Groove Arranger.app`. The package is universal: Intel and Apple Silicon. |
| Linux | `.tar.gz` | Extract and run `Fase Groove Arranger`. Keep the executable permissions. |

From GitHub Actions, the artifact ZIP contains the package shown above: extract that too. These packages are portable and have no installer or distribution signature.

**First launch:** open **AUDIO**, choose your output and a buffer suited to the device. The app uses audio output; it does not open the microphone.

> Start with the guide sound. You can hear and build the whole song before choosing your instruments.

---

## 2. Your first groove in 60 seconds

1. Choose **Soulful** in the genre menu.
2. Choose the groove: song length follows genre and phrase development, without a fixed minute target.
3. Click **NEW ARRANGEMENT**: the intro, main parts, break and outro appear.
4. Press **PLAY** or the **space bar** and listen.
5. Double-click a **piano clip** in the main part. Its large piano roll opens; move a note or change its velocity.
6. Return with **< ARRANGEMENT**. Click the **Piano track name** and choose a guide sound, sample or your own VST.
7. Click **SAVE**, name the project, then **EXPORT** for WAV, MP3 or MIDI.

Want another take? **REGENERATE TRACK** changes just the selected instrument; **REGENERATE CLIP** changes only that track in that section. **UNDO** takes you back to the previous attempt.

> Found a part you like? Open its panel and enable **LOCK TRACK** or **LOCK CLIP** before trying other takes.

---

## 3. The interface, section by section

<p align="center"><img src="screenshot.png" alt="Interface" width="900"></p>

### Current interface

Every button of the interface is drawn the same way: **lit (filled) = on**, raised = off.
**Amber** marks the main actions, **cyan** transport and automation. Piano, bass, strings, guitar
and drums keep their family colours. Every button has its tooltip. Click a track name for its
parameters and double-click a clip for its notes.

The top contains project commands, transport and the controls shared by everyone: **tempo, key and scale**. Below are genre, resulting duration and chord progression. The large area is the song, with sections across and instruments down the side.

| Where | What you find |
|---|---|
| **NEW · OPEN · SAVE · SAVE AS** | Start, open and save your songs. |
| **PLAY · STOP · LOOP** | Listen, return to the beginning and repeat the selected region. The blue cursor shows your position. |
| **Genre and duration** | Genre defines the form. Length follows groove development and complete harmonic phrases; the display shows the result. |
| **PROGRESSION · EDIT CHORDS** | Ready-made progressions and individual chords. HARMONY shows the current progression. |
| **Arrangement** | Each rectangle is a clip: one instrument's notes in one section. |
| **Track name** | Opens its instrument panel, with groove, playing, sound and mixer controls. |
| **Double-click clip / E** | Opens the large MIDI editor for just that track and section. |
| **AUDIO · OPTIONS · GUIDE** | Output device; information, credits and privacy; the guide. |

Colours follow the instruments: **amber** for piano and kick, **cyan** for bass and backbeats, **green** for cymbals, **violet** for percussion and **red** for strings and tom/crash. Guitar playing lanes keep their own colours.

**M** and **S** beside a track name mute and solo it without opening the panel. Click a section header to position playback at that point. **ZOOM** changes how much of the song you see.

### DAW workspace

The side **INSPECTOR** follows the selected track and clip: volume in dB, pan, mute/solo, generation locks, sample or VST assignment and access to the original instrument panels. The **<** arrow at its top right folds it into a narrow rail to give the tracks more width; **>** reopens it. The arrow remains visible when scrolling through the controls. Drag the panel’s **right edge** to resize its width; double-click that edge to restore the default width. The chosen size is remembered. **INSPECTOR / I** also works. Scroll the inspector if vertical space is limited.

The **MIXER** shows seventeen channels in musical order with a fixed master at the right. Faders affect audio only, preserving MIDI velocities. Meters clear on pause. The master **CLIP** indicator latches overloads; click it to reset. **COLLAPSE MIXER**, at the top right of the mixer, folds the faders away to give space back to the tracks. A slim bar keeps **SHOW MIXER** available to restore the previous height. **MIXER / F3** also works. Drag the mixer header to adjust its height; double-click restores the default height. The chosen size is remembered; channels scroll horizontally when needed.

- **Click anywhere in a clip** to select it and place the cursor exactly there; press **PLAY** or **Space** to listen from that point. While playing, clicking jumps immediately. This also works after horizontal scrolling and beyond an active loop’s end; **double-click** or **E** opens the full MIDI editor.
- **Drag a clip** to another section on the same track to swap both clips; **Alt-drag** copies over the destination. Undo restores both. Notes are trimmed to the destination section when lengths differ.
- **Click the bar ruler** to seek precisely, as in the MIDI editor ruler; **Shift-drag** sets the loop range. Both locators remain visible with LOOP off.
- **FIT SONG / F** shows the complete form; **FOLLOW** scrolls during playback; **INSPECTOR / I** and **MIXER / F3** toggle the panels.
- Drop a MIDI file or personal sample on a track row to target that track.

Moving a multiple-note selection in the piano roll preserves spacing at clip and MIDI pitch boundaries. Loop locators and automation follow section insertion, removal, movement and duration changes.

### Transport & tempo (top)

| Control | What it does |
|---|---|
| **⏮ ▶/❚❚ ■** | Return to the start, play / pause, stop. The space bar toggles play / pause. |
| **TEMPO** | Sets song BPM within the selected genre’s range. Duration follows the bar count. |
| **LOOP** | Repeats the chosen region; LOOP SECTION repeats the selected editor section. |

### Progression, Harmony & Scales and Groove

**The song has one shared key and scale.** Piano, bass, strings and guitar use the same progression. There is no need to LINK the individual instruments.

| Control | What it does |
|---|---|
| **Key / Scale** | The root and mode, at the top beside tempo. |
| **PROGRESSION** | A progression from the Fase Groove Piano library. Some also choose a suitable scale. |
| **NEW CHORDS** | Prepares another degree progression in the selected scale. |
| **EDIT CHORDS** | Change each chord: a scale degree, or major, minor or dominant on a chromatic root. One to eight chords, with a shared harmonic duration. |
| **Extensions / Voicing** | Chord colour and the piano notes' arrangement. |
| **DENSITY** | From a few notes to a busier groove. |
| **SWING** | Moves alternating sixteenths, from straight towards shuffle. |
| **FEEL / HUMANIZE** | Small timing and velocity differences. |

**Tempo** changes playback speed immediately. Other choices prepare the next generation; the notes you have written stay in place until you apply them.

Inside an instrument panel, **APPLY TRACK** or **APPLY CLIP** applies its parameters while keeping the current groove take. The **NEW** buttons in the arrangement choose a different take.

> After changing the key or progression, regenerate the relevant harmonic instruments together. A locked part keeps its notes, even when the rest of the song changes harmony.

### Tracks: extended piano, bass, strings and drums

The first thirteen tracks are the drum pieces: **Kick, Snare, Clap, Rim, Closed hat, Open hat, Ride, Shaker, Tambourine, Conga, Cowbell, Tom, Crash**. Then come **Bass, Piano, Strings and Guitar**. The guitar stays together on one track.

<p align="center"><img src="images/guide_piano.png" alt="Piano" width="900"></p>

Click an instrument's **name** to open its panel. Choose how to hear it at the top; below are groove, feel, playing and fills. **EDIT MIDI** opens the notes; **< ARRANGEMENT** returns to the song.

| Instrument | Where to start |
|---|---|
| **Piano** | Groove, gate, register and strum; then movement, top line and range to refine the right hand. |
| **Bass** | A bass line and its feel: classic, sub or sub with gaps on the beat. |
| **Strings** | A strings style, from sustained support to a rhythmic part. |
| **Drums** | Just the selected piece: groove, velocity, MIDI note and pocket. |
| **Guitar** | Strums, let ring, room for band, pushes and the twelve-lane mixer. |

Guitar-part **VOLUME** faders control the guide sound; the selected part’s **LEVEL** changes MIDI dynamics on generation. Part mutes also affect playback of existing notes. MIDI export preserves the clip’s notes.

### Genre sound palettes

Genre also prepares the original guide sounds, kit, tone, voicing and band balance. The guide bass stays centred and dry; guitar and strings leave phrase space for the theme. Personal samples and VSTs retain their assignment and mix. Settings prepare the next MIDI generation; audio controls change immediately. Old projects retain their stored presets until a genre is selected again. See [genre sounds and roles](https://github.com/wsx79/fasegroovearranger/blob/main/docs/GENRE_SOUND.md).

#### Piano: extended view

Piano uses the extended editor’s adjacent panels: Harmony & Scales, Circle of Fifths, Groove and Guide Sound. Rhythm/Velocity and Performance are below. Key, scale and transport remain in the DAW toolbar. **ADVANCED** jumps to the panel sections.

It includes **per-chord inversions**, independent riff/feel seeds, **NEW FEEL**, sustain/expression CC64/CC11, A–D order and locks, a 32-step rhythm grid and individual velocity faders. Click a step to cycle rest, note, accent, ghost, push or hold. Drag a fader for velocity; double-click restores automatic dynamics. **RND PIANO** changes rhythm while respecting locked segments; **RND VELOCITY** changes dynamics only.

**DNA**, **MORPH**, **A/B**, **VARIANTS** and the **CIRCLE OF FIFTHS** use the original musical tools. Results prepare settings: APPLY TRACK / APPLY CLIP writes the notes. A and B also audition a clip with two voicings; UNDO reverses the trial. PRESETS saves and loads track settings in `.fgapreset` files. IMPORT MIDI estimates chords and rhythm; check the result before regenerating. **LEARN** accepts a chord when all keys are released, after you choose a MIDI input.

**EDIT CHORDS → Each chord** sets the shared harmonic duration: half, one, two or four bars. Guide-sound pump provides rate, release and **Tempo / Kick MIDI** source. Kick MIDI follows notes in the Kick track; it is not an external audio input.

**OPTIONS → SETTINGS → Language** selects English, Italian, Spanish, French, German or Portuguese. Control tooltips can be toggled in the same panel. Both preferences stay on this computer; complete manuals are available in Italian and English.

#### Guide sound, sample or VST

| Button | What you choose |
|---|---|
| **BUILT-IN** | Return to the guide sound. Piano and bass have seven keyboard tones; strings, four guitars and four drum kits have their own sounds. |
| **LOAD SAMPLE** | Your WAV, AIFF, FLAC or OGG, up to 60 seconds. The file stays on disk; the project remembers its path. |
| **LOAD VST3 / AU** | An instrument installed on your computer. Choose the `.vst3` bundle; macOS also lists installed Audio Units. |
| **OPEN VST** | The loaded instrument's own interface. |

**GAIN, PAN, MUTE and SOLO** belong to the track. **TONE, REVERB, CHORUS and PUMP** change only the guide sound; use your VST's controls for its sound. The groove controls still write the MIDI sent to it.

A pitched sample starts with root MIDI 60; drums use the piece's note. **Right-click the track name → sample root** changes it. This is a single sample transposed across the track's notes, not a multisample library.

### Piano roll and rhythm grid

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

### Guitar part and mixer

<p align="center"><img src="images/guide_guitar.png" alt="The guitar's twelve playing lanes" width="900"></p>

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

<p align="center"><img src="images/guide_mixer.png" alt="The guitar mixer" width="900"></p>

The twelve lanes sit side by side in the mixer: **level, octave, mute, swing share and push/pull**. Push/pull is in milliseconds: negative is earlier, positive later.

> Try **Room for band** to leave the bass its space. For a more percussive guitar, shorten strums and reduce **Let ring**. Listen with the piano, not only in solo.

### PROJECT, undo and redo

**SAVE / SAVE AS** writes a `.fga` project: sections, notes, parameters, locks, mixer and VST state. Samples and plugins stay at their paths; keep the files used in the song.

An autosave is written once a minute when work has changed. **AUDIO → restore autosave** opens it; **open autosave folder** shows its location. Save separately named copies for versions you want to keep. UNDO retains up to forty steps while the app is open.

### Shortcuts

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

### OPTIONS: settings, language and credits

**OPTIONS → SETTINGS** follows the same panel as Piano, Drums and Guitar: language, interface size, tooltips and updates. **ABOUT** contains credits and SOURCE CODE, Ko-fi, PayPal and Spotify buttons. Audio, guide, privacy and licences open from the buttons below the preferences. This Markdown document is the source guide, as in the other family apps.

<p align="center"><img src="options.png" alt="OPTIONS - SETTINGS" width="460"></p>

> This guide uses English or Italian control names; with the interface in the other language,
> the translated label is in the same position. Chord, mode and groove names stay unchanged.

---

## 4. Exporting MIDI and audio to your DAW

| Export | What you get |
|---|---|
| **MIDI** | The multitrack song, one track or separate files for all tracks. Includes tempo and section markers, not the sounds. |
| **WAV** | Mix, one track or all stems, in 24-bit stereo. Use this for further mixing. |
| **MP3** | Mix, one track or all stems, stereo 320 kbps / 44.1 kHz. The encoder is included. |

All WAV stems start at the song's beginning, have the same length and include a three-second tail. Even an empty track produces an aligned file. Mix export follows mute/solo; a stem exports even when its track is muted and retains gain, pan and master level.

> Choose WAV to bring stems into a DAW. MP3 adds codec delay: useful for listening and sharing, rather than precise track alignment.

---

## 5. Using instruments in real time

Arranger hosts instruments directly: click the **track name**, choose **BUILT-IN**,
**LOAD SAMPLE** or **LOAD VST3 / AU**, then **OPEN VST** for your instrument’s controls.
Press **▶** to hear the project notes.

Tempo and position belong to the song; internal engines do not need LINK or SYNC DAW.
To continue in another DAW, use the MIDI or stems in chapter 4.

---

## 6. Building a track: intro, break, drop

**NEW ARRANGEMENT** derives song length from the genre’s sections, complete harmonic cycles and groove development. Longer evolving motifs have room to unfold; dense grooves may answer sooner. There is no minute target.

| Part | How to use it |
|---|---|
| **Intro** | Rhythmic phrases provide room for the DJ blend; chords, bass and other instruments then enter progressively within the intro. |
| **Main** | The principal groove. Establish the song's progression and sound here. |
| **Breakdown** | Some breathing room, changing the space and the instruments' roles. |
| **Build-up** | A rise towards the return, in styles that call for it. |
| **Drop** | The full groove comes back. It need not sound exactly like the first main part. |
| **Outro** | Instruments leave progressively within the outro. The final rhythmic phrase allows the next blend; no separate mixing passage is appended outside the song. |

The form follows the style. **Deep and Chill** develop through longer passages; **Jackin'** uses short breaks and returns; **Garage & UK** has two drops and a cut; **Soulful and Gospel** use answers and reprises; **Afro & Latin** develops its percussion; **Funky & Disco** moves between verses and choruses.

Tempo is chosen within the genre range: Soulful 118–125, Deep 115–125, Garage & UK 130–135, Afro & Latin 115–125 BPM. The tempo control uses the selected style’s range. Neo Soul / Broken and Chill / Pads retain their slower warm-up character. Duration in seconds is `bars × 240 / BPM`.

The song keeps its base groove across sections, changing performance, density and roles. Generated sections follow eight-bar phrases even with three, five or seven chords; the harmonic cycle continues across section boundaries instead of always restarting at chord one.

### Change only what you need

| Button | What changes |
|---|---|
| **GENERATE SONG** | New takes on all unlocked tracks, keeping the existing sections. |
| **REGENERATE TRACK** | The selected instrument throughout the song. |
| **REGENERATE SECTION** | This section across all unlocked tracks. |
| **REGENERATE CLIP** | Just the selected track in this section. |
| **NEW ARRANGEMENT** | Rebuilds the form and notes from genre, harmonic phrases and groove development. Unlock tracks and clips first. |

**Right-click a section:** rename, duplicate with its notes, move, remove, change length or musical role. **+ SECTION** adds a Main part after the selected section. Extending a section leaves room to fill: regenerate or write the extra notes. Shortening it trims notes at its new end.

> Try this: duplicate a drop, change only the bass in the second copy, then make the piano sparser. Keep a recognisable idea and change who carries it.

### Track volume automation

In the arrangement view, **A** in each track header opens its volume lane. **AUTOMATION** shows or hides the selected track’s lane. Lanes start hidden and automation starts bypassed, leaving the instrument fader in control.

- Click in the lane to add a point and enable automation. The first edit also creates unity-gain endpoints.
- Drag points to change time and level. Timing snaps to one beat; hold **Shift** for free timing. Zoom in for precise editing.
- **Alt-drag a segment between points** to bend its rise or fall; the centre is linear.
- Right-click to delete a point, reset its outgoing curve, clear the envelope, or enable/bypass automation.
- **ON / BYPASS** at the left enables or bypasses the envelope while retaining points. Hiding a lane with **A** does not change playback. Point edits support UNDO/REDO.

Values multiply the track fader: 0% is silence, 100% leaves the level unchanged, and 200% adds about 6 dB. Gain changes continuously, including sustained notes and effect tails. Values hold before the first and after the last point. It works with built-in sounds, samples and VST/AU instruments, during playback and WAV/MP3 mix and stem export.

Envelopes are saved in `.fga` projects, independently of BPM and MIDI regeneration. Inserting, duplicating, deleting or moving sections moves their points too. Changing section length or using NEW ARRANGEMENT keeps absolute point times, clamped to the new song length: review the envelope afterwards. Old projects open with automation bypassed. Lane visibility belongs to the current editing session and is not saved in the project.

Volume automation **does not change note velocities or export MIDI CC**: it is DAW audio gain. Original engine dynamics still work; extra arranger velocity ramps previously used to imitate fades are removed. Export audio to carry the curves into another application; MIDI retains musical notes and original CC.

---

## 7. Soulful and jackin’ house recipes

| Sound | Settings |
|---|---|
| **Classic '90s deep house** | A Dorian · *Dorian Vamp i – IV* · 9ths · Close · *Classic Offbeat Stabs* · Swing 20% · Gate 40% |
| **Kerri Chandler style** | F Minor (Aeolian) · *Late Night i – VII – VI – VII* · 11ths · Wide · *Deep Chords* · Swing 30% |
| **Gospel / soulful** | Eb Major · *Gospel Lift IV – V – vi – I* · 9ths · Drop 2 · *Gospel Bounce* · Fills every 4 bars |
| **Funky disco house** | C Mixolydian · *Mixo Groove* · 7ths · Close · *Nile Chops* or *Funk Clav Chops* · Bass: Octaves |
| **Jazzy late night** | D Major · *Circle of 5ths vi – ii – V – I* · 11ths · Rootless · *Neo Soul Lazy* · Humanize 40% |
| **Modern / quartal** | G Dorian · *Deep Minor i – v* · 9ths · Quartal · *Broken Beat* · Swing 15% |
| **Soulful grand** | Eb Major · *Soulful Gospel I – iii – IV – V* · 9ths · Drop 2 · Sound: *Soulful Grand* · *Soulful Sunday* · Strum 25% · Reverb 40% |
| **'90s house piano** | C Minor (Aeolian) · *Jackin' Minor i – iv* · 7ths · Close · Top +8va · Sound: *House Piano (M1 style)* · *Classic Offbeat Stabs* |
| **Jackin' organ** | G Dorian · *Deep Dorian i – ii – IV – i* · 7ths · Close · Sound: *House Organ* · *Jack Organ Pump* · Swing 40% |
| **Jackin' house** | A Minor (Aeolian) · *Jackin' Minor i – iv* · 7ths · Close · *Jackin' Stabs* or *Jack Bounce* · Swing 40% · Gate 35% · Bass: Octaves |
| **Gospel slide** | Eb Major · *Turnaround I – vi – ii – V* · 9ths + altered V · Drop 2 · Approach chord: From above · *Gospel Bounce* · Strum 20% |
| **Floating deep sus** | F Dorian · *Dorian Vamp i – IV* · Sus (9sus4) · Wide · *Deep Chords* · Reverb 40% |
| **Lydian dominant funk** | E Lydian Dominant · *Lydian Dom Funk I7#11 – II* · 9ths · Shell · *Funky Upstrokes* · Swing 30% |
| **Dark Afro-deep** | A Phrygian Dominant · *Dark Deep I – II* · 7ths · Drop 2+4 · *Deep Sway* · Gate 70% |
| **Random inspiration** | 🎲 RANDOM + 🎲 NEW RIFF a few times, Density 30–60%, then export what you like |

Apply these settings in the track panels using **APPLY TRACK / APPLY CLIP**.
Then build your song with the examples below.

### Soulful: piano in front, guitar in the spaces

1. Choose **Soulful** and a groove, then **NEW ARRANGEMENT**.
2. Start the piano with **Rhodes (FM)** and seventh or ninth chords.
3. Listen to the main part. If it is too busy, lower piano density and click **APPLY CLIP**.
4. Keep guitar in accompaniment and mute lanes that cover the piano.
5. Lock the piano take you like, then try **REGENERATE TRACK** on bass.

### Deep: fewer notes, more room

1. Choose **Deep** and generate the form.
2. Try a compact piano register, longer gate and fewer notes.
3. Choose a **Sub** bass feel. Keep strings as support and listen to the break.
4. Duplicate a main part and change just one or two instruments in the reprise.

### Jackin': short grooves and strong returns

1. Choose **Jackin'** or **Jackin' Piano**.
2. Start with short piano gate and more swing.
3. Strip back the break to set up the return: a **REGENERATE CLIP** on piano may be enough.
4. Keep the kick recognisable and compare the return with what came before using **LOOP SECTION**.

### Funky & Disco: let guitar lead

1. Choose **Funky & Disco** and generate the song.
2. Open guitar and enable **SOLISTA**.
3. Listen to DOWN, UP, CHANK and OCTAVE in context; lower lanes that overlap too much.
4. Try strings in the chorus and leave more space in the part before it.
5. Check intro and outro: entrances and exits should follow the genre’s phrases; the first and final rhythmic phrases leave room for blending.

---

## 8. FAQ and troubleshooting

**I changed key, but still hear the old chords**

You have prepared a new key while the clips still contain their previous notes. Regenerate the relevant harmonic instruments. Check locks: a locked part keeps its notes.

**I cannot hear anything**

Check **AUDIO**, output device, PLAY, master level and MUTE/SOLO. Click **BUILT-IN** in the track panel to try the guide sound. If that works, check the assigned sample or VST.

**A VST or sample cannot be found**

The project remembers its path, but the file may have moved or may not be installed here. Relink it with **LOAD SAMPLE** or **LOAD VST3 / AU**. Check the plugin's architecture and its authorisation. Missing instruments must be relinked before audio export.

**How do I keep edited notes while trying other takes?**

Enable **LOCK CLIP** for that part, or **LOCK TRACK** for the instrument. Use NEW on other elements. You can still edit locked material manually.

**What determines the duration?**

Genre sets the musical form; the groove and harmonic cycle determine its development in complete phrases. Changing BPM changes the resulting duration without forcing a new bar count.

**Where are recording, automation and effect chains?**

This version works on MIDI arrangements. Audio/MIDI recording, automation beyond track volume, audio editing, buses/sends, external effect chains and plugin delay compensation are not present. Export MIDI and WAV to continue in your DAW. Some advanced Fase Groove plugin controls have not yet been brought into the Arranger.

**My MIDI file will not import**

Use a PPQ MIDI file; SMPTE files are not supported. Import fills the selected clip up to its section length; it does not reconstruct an entire external song automatically.

**What’s new in the latest version?**
Find version-by-version changes in the [changelog](../CHANGELOG.md) and on the
[Releases](https://github.com/wsx79/fasegroovearranger/releases) page.

**Does the app connect to the internet or collect data?**
Your music stays on your computer. Only the optional update check contacts GitHub; external
links open when you choose them. See the [privacy policy](../PRIVACY.md).

---

## 9. Listen to my music

Fase Groove Arranger is made by **Fabio 'Pitch' Serra**, DJ and producer. If you like the sound of
this app, you'll probably like my tracks too:

- 🎧 **Listen to my productions on Spotify:** [Fabio 'Pitch' Serra on Spotify](https://open.spotify.com/artist/29iQGuXML8LEw3xRp1dNRP)
- 🛒 **Download my tracks** from all the major digital music stores: just search for
  **Fabio 'Pitch' Serra**.

Thanks for listening and for supporting independent music! 🎹🔥

### Credits, support and privacy

**Fase Groove Arranger is by Fabio 'Pitch' Serra / FS Pitch, DJ & Producer.**

[Listen to Fabio 'Pitch' Serra on Spotify](https://open.spotify.com/artist/29iQGuXML8LEw3xRp1dNRP)

[Buy me a coffee on Ko-fi](https://ko-fi.com/fabios1979)

[Support the project with PayPal](https://paypal.me/fabios79)

Development assistance: **Claude (Anthropic)** and **Codex (OpenAI)**, AI coding assistants. Open credits, licences and privacy through **OPTIONS**.

[Source code and the Fase Groove family](https://github.com/wsx79/fasegroovearranger)

The app is free and open source, **GNU AGPL v3 or later**, built with JUCE and the Inter typeface. It works offline with no accounts, analytics or music uploads. External links open your browser only when you choose them. Your samples and plugins keep their own licences and terms; Privacy and Licences are included in the package.
