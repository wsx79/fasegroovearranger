// Fase Groove Drums - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

// The drum kit: the lanes of the grid, their names and their General MIDI notes,
// and the groove format shared by the presets, the random beats and the user pattern.
// Pure C++ (no JUCE).

#include <array>
#include <string>
#include <vector>

namespace fgd
{

constexpr double stepLength = 0.25;   // a 16th note, in quarter notes

//==============================================================================
/** One row of the grid: one piece of the kit. */
enum class Lane { kick = 0, snare, clap, rim, closedHat, openHat, ride, shaker, tambourine, conga, cowbell, tom, crash };
constexpr int numLanes = 13;

const char* laneName (int lane) noexcept;        // "Kick", "Closed hat" ...
const char* laneShortName (int lane) noexcept;   // "KICK", "C.HAT" ... (the grid's row labels)

/** General MIDI drum note of a lane (kick 36, snare 38, clap 39 ...). */
int defaultNote (int lane) noexcept;

/** The lane a MIDI note plays in the default (General MIDI) mapping, or -1. Covers the GM
    neighbours too (e.g. 35 = kick, 40 = snare, 44 = closed hat, 41..50 = toms). */
int laneForGeneralMidiNote (int note) noexcept;

//==============================================================================
/** A drum groove: one line per lane, one character per 16th step:
      'X' accent   'x' hit   'g' ghost note (very soft)   'f' flam (a soft grace note just before)   '.' rest
    All lines have the same length (16 = one bar, 32 = two bars). */
struct DrumPattern
{
    std::string name;
    std::string category {};   // genre heading in the groove menu
    std::array<std::string, numLanes> lanes {};

    int length() const noexcept { return (int) lanes[0].size(); }
    std::string& operator[] (Lane l) noexcept             { return lanes[(size_t) l]; }
    const std::string& operator[] (Lane l) const noexcept { return lanes[(size_t) l]; }
};

inline bool isHit (char c) noexcept { return c == 'X' || c == 'x' || c == 'g' || c == 'f'; }

/** Every lane the same length (the longest one, shorter lines repeated), unknown symbols
    as rests: what every pattern goes through before it plays. */
void normalisePattern (DrumPattern&, int minimumLength = 16);

//==============================================================================
/** The preset grooves, grouped by genre (the same 13 genres as Fase Groove Piano). */
const std::vector<DrumPattern>& drumPatterns();

/** Genre headings of the groove menu, in display order. */
const std::vector<const char*>& grooveCategories();
int categoryIndex (const std::string& category) noexcept;   // -1 when unknown

/** Swing that suits a genre (0 = straight, 1 = full triplet shuffle). */
float genreSwing (int category) noexcept;

/** Index of the "Random beat" entry, right after the preset grooves. */
int randomPatternIndex() noexcept;

/** Index of the "User pattern" entry (editable grid), after "Random beat". */
int userPatternIndex() noexcept;

constexpr int userPatternLength = 64;   // the user pattern: up to four bars of 16ths
constexpr int randomBeatLength = 32;    // a random beat: two bars

/** Every lane exactly `length` steps: longer lines are cut, shorter ones repeated. */
void fitPattern (DrumPattern&, int length);

} // namespace fgd

