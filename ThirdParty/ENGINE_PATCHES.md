# Arranger changes to the vendored engines

`ThirdParty/fasegroove{piano,drums,guitar}/Source` holds `Source/engine/*` and `Source/Guide*.h` of each plugin, at the commit in `ThirdParty/ENGINE_VERSIONS`, plus the Arranger's own changes listed here. The exact changes are in `ThirdParty/engine-patches/<plugin>.diff` (written by the script, do not edit them).

`scripts/sync-engines.py` keeps them in step with the plugin repos (siblings of this one):

- `scripts/sync-engines.py --check`: exit 1 when a plugin has engine changes not vendored yet, a conflict with the changes below, or a vendored file was edited without recording it.
- `scripts/sync-engines.py [plugin ...]`: three-way merge (last sync, Arranger, plugin HEAD) of every engine file; the changes below are kept. A plugin with a conflict is left as it is and its files are listed: port that change by hand, then `--record --synced <plugin>`. After applying, rebuild and run `ctest` (core MIDI, `scripts/verify-midi.py`) and the smoke test before committing: plugin changes can change the Arranger's MIDI.
- After changing a vendored file on purpose: describe it below, then `scripts/sync-engines.py --record <plugin>`.

## Piano (`sgk`)

- `GrooveSettings::separateBass`: the bass plays on its own track. `GrooveEngine::trackIndex` tracks bass notes apart (`numTracked` 384), so the bass never cuts or steals a piano note.
- `GrooveSettings::bassLineOverride`: a bass figure developed by the Arranger for a regenerated take; the renderer and the resolved grid (`cachedBassLineOverride`) use it, empty keeps the selected library line.
- `Comping.cpp` (the comping under a leading guitar, shared with the plugin): its `roundToInt` rounds a half away from zero (`std::lround`), as the Arranger's own copy did; the plugin rounds a half to even (as `juce::roundToInt`). Only the answers' velocities on an exact .5 differ; this keeps the Arranger's MIDI as it was.
- Until the plugin commit with `Comping.{h,cpp}` is the recorded one, the two files count as Arranger additions in `engine-patches/fasegroovepiano.diff`; after that commit, `scripts/sync-engines.py --record --synced piano` (the merge of a file added on both sides reports a conflict until then).
- `GuideVoice`: optional centred-output constructor argument, used only by the bass track (the plugin's callers keep the keyboard spread).
- Not vendored (only the plugin's processor and editor use them; `sameSettings` would not compile here): the piano-roll edited loop (`LoopNote`, `editedLoop`, `playsEditedLoop`, `carryEdits`), `stabBassLine`, `bassGate`, `bassFeelName`, `GrooveEngine::sameSettings`.
- Comments: `riffGenre`, the genre preset in `Generators.h`.

## Drums (`fgd`)

- Jackin' house keeps a four-on-the-floor shuffle: `reinforceJackinHouse` after the dice, section variations and presets (main and drop); the swung "a" stays an accent in the humanizer; cowbell and tom from the first bar of the main part.
- Not vendored (unused here): `generalMidiDrumName`, `makeRandomLane`, `defaultArrangement`.

## Guitar (`fgg`)

- `GrooveSettings::stepsPerChord`: shared harmonic duration (8/16/32/64 steps per chord); the chord, its voicing, the push to the next chord and the loop length follow it. 16 is the plugin's one chord per bar.
- Not vendored (unused here): `makeRandomLane`, `defaultArrangement`.

## All

- Every file ends with an extra empty line.
