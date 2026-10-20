// Fase Groove Drums - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

// On-the-fly material: random beats by genre, turnaround variations and the
// song-section versions of a groove (intro / break / build-up / drop ...).

#include "Drums.h"

#include <array>
#include <cstdint>
#include <vector>

namespace fgd
{

//==============================================================================
enum class Section { main = 0, intro, breakdown, buildup, drop, outro };
constexpr int numSections = 6;
const char* sectionName (int section) noexcept;

/** Rewrites a groove for a song section: kick and hats in the intro / outro, no kick in the
    break, a snare roll building up, the full groove in the main part and the drop. */
DrumPattern applySection (const DrumPattern& source, Section section);

/** Loudness of a section. */
double sectionVelocityScale (Section section) noexcept;
bool sectionAllowsFills (Section section) noexcept;

//==============================================================================
/** A brand new two-bar beat in the style of a genre (grooveCategories() index), from a seed.
    density 0..1 goes from a bare kick-clap-hat to busy 16ths and percussion. Same seed = same beat. */
DrumPattern makeRandomPattern (std::uint32_t seed, float density, int genre);

/** A new two-bar line for one drum only, in the style of a genre (density as above). Never empty:
    a drum the genre rarely uses gets a light, fitting part. Same seed = same line. */
std::string makeRandomLane (Lane lane, std::uint32_t seed, float density, int genre);

/** A variation of a groove for the turnaround bar: an open hat lift, a kick pick-up, a snare
    or clap ghost before the next bar. Same seed = same variation. */
DrumPattern makeVariation (const DrumPattern& source, std::uint32_t seed);

/** A song section's own version of a groove: the same kick and backbeat, with the hats, the
    percussion and the ghost notes moved around a little. Same seed = same version. */
DrumPattern makeSectionVariation (const DrumPattern& source, std::uint32_t seed);

//==============================================================================
/** A groove is split into four equal parts, A B C D (half a bar each in a two-bar groove). */
constexpr int numSegments = 4;

/** Rebuilds the groove playing the parts in the given order (e.g. A A B C). */
DrumPattern applySegmentOrder (const DrumPattern& source, const std::array<int, numSegments>& order);

/** Copies part `segment` of `from` into `into` (patterns of the same length). */
void copySegment (DrumPattern& into, const DrumPattern& from, int segment);

//==============================================================================
/** What the end of a fill bar plays. */
enum class FillStyle { snareRoll = 0, tomRun, clapBuild, hatLift, dropOut, congaRun, mix,
                       kickBuild, rimShuffle, openHatRun, clapStutter, percBreak, custom };
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

/** Intro 8 | Main 16 | Break 8 | Build-up 4 | Drop 16 | Outro 8 */
const std::vector<ArrangementPart>& defaultArrangement();

} // namespace fgd

