# Fase Groove family alignment

Source comparison, 20 October 2026. Reference branches are `main` at:

| Product | Revision | Reference code |
|---|---|---|
| Piano | `026f0451d683f25520409f8dd273ff5a4496cad0` | `Source/PluginEditor.cpp`: `layoutFull`, `OptionsOverlay`; `Branding.*`, `FamilyTheme.h`, `Localisation.*` |
| Drums | `7f1def6a4e4cb72ed887f7754171affec12af001` | `Source/PluginEditor.cpp`: `layout`, `IconButton`, `Knob`, `OptionsOverlay`; `Updater.*`, README and docs |
| Guitar | `e571f8d02621c65adeb35207214771571499fc65` | `Source/PluginEditor.cpp`: `layout`, `OptionsOverlay`, `PartCard`, `ChordPanel`, `MixerView`; `Branding.*`, README and docs |

## Differences found and corrections

| Area | Arranger before this change | Family reference and correction |
|---|---|---|
| Theme | Shared theme with an additional trailing newline; rendering already used the family palette | Synchronize the exact common Git blob `d61097f`; keep all three vendored copies identical. Inter Regular and SemiBold also match the reference Git blobs. No new palette or LookAndFeel. |
| OPTIONS | Popup language/tips/about menu plus a separate custom credits dialog | Copy Guitar's `OptionsOverlay` into `FamilyOptions.*`: same centered 620 × 620 card, SETTINGS / ABOUT tabs, CLOSE, language and size fields, tips and update row. |
| Credits | Different text order, button sizes, spacing and colours | Original creator/artwork, CODE, licence, JUCE footer and centered 126px source/Ko-fi/PayPal/Spotify buttons. Preserve existing Claude and Codex attributions. |
| Preferences | Interface-size and update controls missing | Saved interface size and optional GitHub release checker, in the original positions. Reuse the original 640 × 560 UpdateOverlay and three-button layout, adapting INSTALL NOW to DOWNLOAD for portable packages. Standalone audio, guide, privacy and licence actions extend the settings below those rows. |
| Transport | Text PLAY/STOP/UNDO/REDO in a flat row | Copy the original `IconButton` implementation. Same transport sizes, connected edges and cyan play action; same two-row utility grouping beside the logo. File commands occupy the positions needed by a project-based standalone app. |
| Piano | Narrower circle, missing groove arrows, custom caption placement | Restore the extended 300/262/252px harmony/circle/sound panels, groove arrows beside NEW RIFF, section/length/variation grouping, instrument tabs and sound row. Piano knob captions above (16px), as in Piano::Knob; Drums/Guitar captions below (14px), as in their Knob. Piano uses the original 10px caption font with 0.08 kerning; Drums/Guitar use 11px. Parameter-menu captions remain above. All extended controls remain accessible. |
| Drums/Guitar | Reordered section/fill fields and NEW FEEL; selectors/sound routing outside their original cards | Restore groove navigation, framed A–D part previews, the original 4×4 Drums pad geometry, selected drum/part controls, the original section/fills then variation/fill-style rows, NEW FEEL and fill toggle in the heading, and guide-sound selector above tone/reverb/volume. Drums' selected-lane dice applies to its selected clip. |
| Branding | Custom wordmark painter mixed into InstrumentPanel | Copy the family `Branding.h/.cpp` API and wordmark proportions. New SVG/PNG mark shows staggered track phrases and a shared ruler/playhead; no piano-key silhouette. Native window icon and BinaryData use this resource. |
| Documentation | Separate invented HTML export, no Italian README or changelogs | Source manuals are GUIDA.md / GUIDE.md as in all three references. Remove duplicate HTML generation; add the family README header/navigation and IT/EN changelogs. Preserve the existing offline native reader. |
| Privacy | Stated that no release checking existed | Document the optional GitHub request, local preferences, explicit browser download and absence of music/project uploads. |

## Necessary standalone adaptations

- There is one transport, tempo, key and scale for the song. Plugin LINK/host-sync and per-plugin MIDI-output selectors are not duplicated in every instrument panel.
- File commands, arrangement generation scopes, track gain/pan/mute/solo and VST3/AU assignment remain available. The dedicated routing strip is above the instrument cards.
- Drums are separate tracks, so the selected drum's sound/sample/mute and MIDI note refer to that track. The selector and pads switch between those tracks; right-clicking a pad toggles track mute. Pads show the track's MIDI note/sample and meter. Unlike the single-plugin audition pads, they navigate the standalone track configuration; listening uses the common transport. The large editor remains isolated to the selected drum.
- Guitar remains a single track with twelve internal parts and SOLISTA; Piano/Bass/Strings settings select their respective standalone tracks. Existing musical tools, generation seeds and editing functions are retained.
- The shared song sections are selected from the panel. Creating/reordering sections and exporting the complete song remain in the arranger.
- Release checks use the original family preference and version-comparison conventions. Arranger distributes portable ZIP/tar archives, not the plugin family's installer; the user opens Releases to download and install. A running project host is not replaced automatically.
- The existing native manual/reference reader remains functional. There is no new website, PDF format or second HTML manual.

## Verification

`scripts/verify-family.py` checks the exact shared theme/font Git blobs and both Markdown manuals with their local links. The native smoke test checks every instrument control's bounds/overlaps, SETTINGS / ABOUT geometry, MIDI editing, six languages and audio/host behaviour. Native screenshots are emitted for direct visual review; numeric layout checks alone do not establish visual equivalence.

## Follow-up comparison

A second source comparison found that the previous pass had incorrectly applied
Drums/Guitar's bottom knob captions to Piano too. Restore Piano's own top-caption
geometry and typography, including Bass/Strings panels from that editor. Restore
the missing random-chord dice beside Guitar's progression button, using the
existing shared harmony operation; the shared key selector stays in the DAW header.
These corrections do not establish complete visual parity. Continue comparing
individual panels against their corresponding plugin, including missing actions
and editor layouts, rather than treating a successful build as visual approval.

The native instrument viewport now centers the reference editors' 1180px design
width instead of filling the 1440px workstation. Previously the fixed-width
Harmony/Circle/Sound or Part/Chords/Sound cards left all extra space in Groove,
which changed the reference proportions. Global standalone routing remains in
the DAW toolbar; no musical controls are removed.

Also restore Piano::drawPanel's 6px corners, 11px letter-spaced titles and smaller
rounded accent tick, rather than applying Drums/Guitar's 8px cards and title
geometry to every instrument. The missing coloured frame below Piano/Bass/Strings
tabs is copied from Piano's paint routine. Guitar's chord cards now use the
original small-row insets and long-name font sizing.
