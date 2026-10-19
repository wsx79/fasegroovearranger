// Fase Groove Drums - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

// Drum sequencer. Turns a musical-time window (in quarter notes, "ppq") into
// note on/off events. Pure C++ (no JUCE).

#include "Drums.h"
#include "Generators.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace fgd
{

struct DrumSettings
{
    DrumSettings() noexcept;

    int pattern = 0;
    int grooveBars = 2;                    // 2 (parts A-D of half a bar) or 4 (parts of one bar)
    float swing = 0.15f;                   // 0 = straight, 1 = triplet shuffle, up to 1.5 (a dotted 16th: the heaviest)
    int velocity = 100;
    float humanize = 0.2f;
    float accentGroove = 0.85f;             // hats, shaker, ride, tambourine: louder on the offbeat, softer 16ths (0 = flat)
    std::uint32_t humanizeSeed = 0;        // NEW FEEL: another, repeatable humanization

    int fillEveryBars = 0;                 // 0 = no fills
    FillStyle fillStyle = FillStyle::snareRoll;
    bool crashAfterFill = true;            // a crash on the bar after a fill (and every 8 bars of the drop)
    int variationEvery = 0;                // 0 = off, else every 4 or 8 bars the last bar varies

    Section section = Section::main;
    long long sectionStartBar = 0;         // where the section began: fills, crashes and variations count from there
    int sectionBars = 0;                   // how long the section lasts (0: it goes on, as when playing live)
    bool afterSectionFill = false;         // the section before ended with its own fill (a crash on our first bar)

    bool genreFeel = true;                 // each genre's own pocket: how far each drum sits off the grid and swings
    bool bassSpace = false;                // room for the bass: kick pick-ups only at the end of four bars, never just before the one

    std::uint32_t riffSeed = 1;            // used when pattern == randomPatternIndex()
    float density = 0.5f;
    int randomGenre = 0;                   // the genre NEW BEAT writes in (grooveCategories() index)

    std::array<int, numLanes> notes {};    // the MIDI note of each lane (General MIDI by default)
    std::array<float, numLanes> levels {}; // velocity scale of each lane (1 = as written)
    std::array<bool, numLanes> muted {};

    // The feel of each drum: how much of the swing it takes (1 = as the Swing control) and how
    // far it sits ahead (< 0) or behind (> 0) the beat, in milliseconds. The kick takes neither.
    std::array<float, numLanes> laneSwing {};
    std::array<float, numLanes> laneShift {};
    double tempo = 120.0;                  // turns the milliseconds into beats

    // A groove template (e.g. taken from an audio loop): when active, the timing and the level of
    // every 16th of the bar come from it instead of the swing and the accent groove.
    struct Template
    {
        bool active = false;
        std::array<float, 16> timing {};   // in 16ths, < 0 early, > 0 late
        std::array<float, 16> level {};    // 1 = as written
    };
    Template grooveTemplate;

    // Used when pattern == userPatternIndex(): the hand-edited grid, and the velocity set by
    // hand on each of its steps (0 = automatic, from the symbol).
    std::array<std::array<char, userPatternLength>, numLanes> user {};
    std::array<std::array<unsigned char, userPatternLength>, numLanes> userVelocity {};
    // ...and the chance each of those steps plays (0 or 100 = always) and its roll (0 or 1 = one
    // hit, 2-4 = that many quick hits in the 16th; never on the kick).
    std::array<std::array<unsigned char, userPatternLength>, numLanes> userChance {};
    std::array<std::array<unsigned char, userPatternLength>, numLanes> userRoll {};

    // The fill drawn by hand (Fill style: Custom): one bar. It takes over from its first written
    // step; the drums it doesn't use keep playing the groove.
    std::array<std::array<char, 16>, numLanes> customFill {};

    std::array<int, numSegments> segmentOrder { 0, 1, 2, 3 };   // A B C D
    std::uint32_t sectionVariation = 0;    // a song section's own version of the groove (0: as it is)

    // A song section with its own fill: when forcedFillBar >= 0 that is the only bar with a fill,
    // in forcedFillStyle (< 0: no fill at all).
    long long forcedFillBar = -1;
    int forcedFillStyle = -1;
};

struct NoteEvent
{
    double ppq;
    int note;
    int velocity;   // 0 for note-off
    int lane = 0;
    bool isNoteOn() const noexcept { return velocity > 0; }
};

//==============================================================================
class GrooveEngine
{
public:
    GrooveEngine();

    /** Appends (time-ordered) events falling in [startPpq, endPpq). */
    void render (double startPpq, double endPpq, const DrumSettings& settings, std::vector<NoteEvent>& out);

    /** Releases everything currently sounding at the given time. */
    void allNotesOff (double ppq, std::vector<NoteEvent>& out);

    bool anyNoteSounding() const noexcept;

    /** The groove before the section is applied: a preset, the random beat or the user pattern. */
    static DrumPattern basePattern (const DrumSettings& s);

    /** The groove actually played: the base pattern, in the A-D order, its section's own
        version, then rewritten for the song section. */
    static DrumPattern resolvePattern (const DrumSettings& s);
    static int patternLength (const DrumSettings& s) noexcept;

    /** Length (in 16th steps) after which groove, variations and fills all repeat. */
    static int loopLengthInSteps (const DrumSettings& s) noexcept;

    /** The groove for a set of settings, worked out ahead (on the message thread): render() uses
        it instead of building its own when it matches, so the audio thread doesn't allocate. */
    struct Prepared
    {
        DrumSettings key;
        DrumPattern pattern;
        std::array<DrumPattern, 4> variations;   // one turnaround per four-bar phrase, in turn
        DrumPattern strongVariation;             // the end of eight bars: a bigger turnaround
        int genre = -1;
        bool fourFloor = true;
    };
    static void prepare (Prepared&, const DrumSettings&);
    static bool samePattern (const DrumSettings& a, const DrumSettings& b) noexcept;
    void setPrepared (const Prepared* p) noexcept { prepared = p; }

    /** Offline render of a whole clip starting at bar 1, with every note closed. */
    static std::vector<NoteEvent> renderClip (const DrumSettings& s, double lengthInBeats);

    /** Renders a whole song (intro, main, break...) with section markers. */
    struct Marker { double ppq; std::string name; };
    static std::vector<NoteEvent> renderArrangement (const DrumSettings& s, const std::vector<ArrangementPart>& parts,
                                                     std::vector<Marker>* markers = nullptr);

    /** The fill style a bar plays ("Mix" picks one per bar). */
    static FillStyle effectiveFillStyle (FillStyle style, long long bar) noexcept;

private:
    struct PendingOn { double ppq; int note; int velocity; double duration; int lane; std::size_t order = 0; };

    const Prepared& patternFor (const DrumSettings& s);
    void collectStep (long long step, const DrumSettings& s, const Prepared& resolved);
    void flushOffsUpTo (double ppq, bool inclusive, std::vector<NoteEvent>& out);
    void emitPending (double startPpq, double endPpq, std::vector<NoteEvent>& out);

    std::array<double, 128> offTime {};   // < 0 when the note is not sounding
    std::array<int, 128> laneOf {};
    std::vector<PendingOn> pending;
    std::vector<int> scratch;

    // The resolved pattern: the one prepared ahead when it matches, else built here (and kept,
    // rebuilt only when its inputs change).
    const Prepared* prepared = nullptr;
    Prepared cache;
    bool cacheValid = false;
    // Offline renders: only the steps in [stepBegin, stepEnd) play (a clip's last pushed hit
    // doesn't come round again at its end; a song part plays only its own bars).
    long long stepBegin = 0, stepEnd = -1;   // stepEnd < stepBegin: every step
};

} // namespace fgd

