# Family editor adaptation

Arranger retains the family Inter typefaces, palette, control rendering and artist credits. The DAW owns transport, key/scale, track routing, generation scope and export.

Reference source revisions:

- Piano `026f0451d683f25520409f8dd273ff5a4496cad0`: `Source/PluginEditor.cpp`, `layoutFull`, `PatternView::paint`, `PianoRollEditor::paint` and `resized`.
- Drums `7f1def6a4e4cb72ed887f7754171affec12af001`: `layout`, `DrumGrid::paint`, `cellBounds` and `VelocityView`.
- Guitar `e571f8d02621c65adeb35207214771571499fc65`: `layout`, `GuitarGrid`, `ChordPanel` and `MixerView`.

`InstrumentPanel` adapts those panel compositions to the shared Project model. Piano keeps the extended harmony/circle/groove/sound arrangement, inversion controls, two-thumb range, movement, top line, strum, feel, seeds and musical tools. Drums/Guitar retain the wide groove/ABCD row and compact part/chords/groove/sound cards. The guitar has one selected-part strip plus its full twelve-part mixer. As in the source editor, the selected-part LEVEL changes generated MIDI velocity while mixer VOLUME controls the guide sound. Lane mutes affect playback of stored notes without rewriting the clip; MIDI export retains those stored notes.

The separate MIDI editor edits only the chosen instrument. Pitched tracks show the family chord strip, fitted keyboard and velocity handles; Drums uses the family hit/accent/ghost cell rendering on the chosen drum only; Guitar shows all twelve parts with an optional pitched-note view. The original plug-in processors are not embedded: controls bind to the bundled original generation engines through the standalone Project model.

Arrangement length is derived from genre form, harmonic-cycle alignment, groove motif period and development. The old `songMinutes` project property remains readable for compatibility but no longer constrains newly generated forms. Loading a saved project preserves its stored sections and MIDI. There is no universal post-render sixteen-bar harmonic exclusion.

Native smoke evidence includes panel bounds/overlap checks, actual add/move/resize/velocity/delete editor gestures, guide-volume/mute audio renders and screenshots. These checks verify layout and routing; visual comparison against the original editors remains part of review.
