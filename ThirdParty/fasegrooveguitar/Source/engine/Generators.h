// Fase Groove Guitar - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

// On-the-fly material: random grooves by genre, turnaround variations and the
// song-section versions of a groove (intro / break / build-up / drop ...).

#include "Guitar.h"

#include <array>
#include <cstdint>
#include <vector>

namespace fgg
{

//==============================================================================
enum class Section { main = 0, intro, breakdown, buildup, drop, outro };
constexpr int numSections = 6;
const char* sectionName (int section) noexcept;

/** Rewrites a groove for a song section: chanks and scratches in the intro / outro, a held
    swell in the break, a scratch building up, the full groove in the main part and the drop. */
GuitarPattern applySection (const GuitarPattern& source, Section section);

/** Loudness of a section. */
double sectionVelocityScale (Section section) noexcept;
bool sectionAllowsFills (Section section) noexcept;

//==============================================================================
/** A brand new two-bar guitar groove in the style of a genre (grooveCategories() index), from a
    seed. density 0..1 goes from a few chords to busy 16th strumming. Same seed = same groove. */
GuitarPattern makeRandomPattern (std::uint32_t seed, float density, int genre);

/** A variation of a groove for the turnaround bar: a slide into the next bar, a scratch run,
    an up-strum pick-up, a lick. Same seed = same variation. */
GuitarPattern makeVariation (const GuitarPattern& source, std::uint32_t seed);

/** A song section's own version of a groove: the same main chords, with the scratches, the
    up-strums and the ghosts moved around a little. Same seed = same version. */
GuitarPattern makeSectionVariation (const GuitarPattern& source, std::uint32_t seed);

//==============================================================================
/** A groove is split into four equal parts, A B C D (half a bar each in a two-bar groove). */
constexpr int numSegments = 4;

/** Rebuilds the groove playing the parts in the given order (e.g. A A B C). */
GuitarPattern applySegmentOrder (const GuitarPattern& source, const std::array<int, numSegments>& order);

/** Copies part `segment` of `from` into `into` (patterns of the same length). */
void copySegment (GuitarPattern& into, const GuitarPattern& from, int segment);

//==============================================================================
/** What the end of a fill bar plays. */
enum class FillStyle { strumBuild = 0, octaveRun, scratchBuild, chankLift, dropOut, slideUp, mix,
                       stabStutter, lickRun, arpRun, bassWalk, swellOut, custom };
constexpr int numFillStyles = 13;
const char* fillStyleName (int style) noexcept;
/** First 16th (in the bar) a fill style takes over. */
int fillStartStep (FillStyle style) noexcept;

//==============================================================================
struct ArrangementPart
{
    Section section;
    int bars;
    std::uint32_t variation = 0;   // its own version of the groove (0: the groove as it is)
    int endFill = -1;              // fill on its last bar: -1 as set in the Song panel, -2 none, else a fill style
};

} // namespace fgg

