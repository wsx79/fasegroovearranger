// Fase Groove Drums - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "GrooveEngine.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace fgd
{

namespace
{
    long long floorDiv (long long a, long long b) noexcept
    {
        long long q = a / b;
        if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
        return q;
    }

    int mod (long long a, long long m) noexcept { return (int) (((a % m) + m) % m); }

    /** Deterministic 0..1 noise, so re-rendering a step always gives the same feel. */
    double noise (long long step, int salt) noexcept
    {
        auto x = (std::uint64_t) (step * 0x9E3779B97F4A7C15LL) ^ ((std::uint64_t) salt * 0xBF58476D1CE4E5B9ULL);
        x ^= x >> 31; x *= 0x94D049BB133111EBULL; x ^= x >> 29;
        return (double) (x >> 11) * (1.0 / 9007199254740992.0);
    }

    double symbolLevel (char symbol) noexcept
    {
        switch (symbol)
        {
            case 'X': return 1.0;
            case 'f': return 0.9;
            case 'g': return 0.42;
            case 'x':
            default:  return 0.8;
        }
    }

    /** How much a lane drifts with Humanize: the kick stays tight, hands drift more. */
    double laneLooseness (int lane) noexcept
    {
        switch ((Lane) lane)
        {
            case Lane::kick:       return 0.0;   // never moves (see timeFor)
            case Lane::snare:
            case Lane::clap:       return 0.6;
            case Lane::shaker:
            case Lane::tambourine:
            case Lane::conga:      return 1.2;
            default:               return 0.9;
        }
    }

    /** How long a hit is held: drums and percussion a beat (samplers in gate mode play the whole
        sound; the next hit of the same note closes it first), hats short, cymbals long. */
    double noteLength (int lane) noexcept
    {
        switch ((Lane) lane)
        {
            case Lane::closedHat:
            case Lane::shaker:
            case Lane::tambourine: return 0.9 * stepLength;
            case Lane::openHat:    return 2.0 * stepLength;
            case Lane::crash:      return 8.0 * stepLength;
            default:               return 4.0 * stepLength;
        }
    }

    bool isHatLike (int lane) noexcept
    {
        const auto l = (Lane) lane;
        return l == Lane::closedHat || l == Lane::openHat || l == Lane::shaker || l == Lane::tambourine || l == Lane::ride;
    }

    /** How soft a ghost note is: hats and shakers barely there, the snare's a little more. */
    double ghostLevel (int lane) noexcept
    {
        if (isHatLike (lane))
            return 0.3;
        const auto l = (Lane) lane;
        return l == Lane::snare || l == Lane::clap || l == Lane::rim ? 0.35 : 0.42;
    }

    /** Each genre's pocket (grooveCategories() order): how far a drum sits behind (> 0, in ms) or
        ahead of the beat, and how much of the swing it takes. */
    void genrePocket (int genre, int lane, double& pushMs, double& swingShare) noexcept
    {
        pushMs = 0.0;
        swingShare = 1.0;
        const auto l = (Lane) lane;
        const bool backbeat = l == Lane::snare || l == Lane::clap || l == Lane::rim;
        const bool hats = l == Lane::closedHat || l == Lane::openHat;
        const bool shakers = l == Lane::shaker || l == Lane::tambourine;
        const bool hands = l == Lane::conga || l == Lane::tom || l == Lane::cowbell;
        switch (genre)
        {
            case 1: case 3:   // soulful, gospel: the shaker and the tambourine drive, a hair ahead
                if (shakers) pushMs = -3.0;
                break;
            case 2:           // deep: a lazy backbeat
                if (backbeat) pushMs = 6.0;
                break;
            case 5:           // jackin' piano
                if (backbeat) pushMs = 4.0;
                break;
            case 6:           // garage: the hats and the rims shuffle harder than the rest
                if (hats || shakers || l == Lane::rim) swingShare = 1.15;
                break;
            case 7:           // afro: straight hands, the congas just behind
                if (hands || shakers || l == Lane::rim) swingShare = 0.5;
                if (l == Lane::conga) pushMs = 4.0;
                break;
            case 10:          // neo soul & broken: the snare well behind, the hats a little
                if (backbeat) pushMs = 10.0;
                if (hats) pushMs = 3.0;
                break;
            case 11:          // chill
                if (backbeat) pushMs = 5.0;
                break;
            default:
                break;
        }
    }

    int lcm (int a, int b) noexcept { return a / std::gcd (a, b) * b; }

    /** The level a hat (or shaker, tambourine, ride) has at each 16th of the beat.
        House: the offbeat 8th sings. Jackin' and garage: the swung 16th. Afro: almost even.
        Neo soul: the downbeat, with a softer hat. Chill: a gentle curve. An unknown genre
        (a hand-written grid) stays on the house curve. Percussion is gentler, and flatter in afro. */
    double accentProfile (int lane, int stepInBeat, int genre) noexcept
    {
        const int s = stepInBeat & 3;
        const bool perc = lane == (int) Lane::conga || lane == (int) Lane::cowbell;
        if (perc)
        {
            if (genre == 7)
            {
                static constexpr double flat[] { 0.9, 0.82, 0.95, 0.88 };
                return flat[s];
            }
            static constexpr double percussion[] { 0.82, 0.55, 1.0, 0.8 };
            return percussion[s];
        }
        if (genre == 4 || genre == 6)
        {
            static constexpr double swung[] { 0.55, 0.3, 0.62, 1.0 };
            return swung[s];
        }
        if (genre == 10)
        {
            static constexpr double back[] { 0.85, 0.35, 0.5, 0.4 };
            return back[s];
        }
        if (genre == 11)
        {
            static constexpr double gentle[] { 0.7, 0.55, 0.85, 0.6 };
            return gentle[s];
        }
        if (genre == 7)
        {
            static constexpr double even[] { 0.75, 0.7, 0.82, 0.72 };
            return even[s];
        }
        static constexpr double hats[] { 0.45, 0.28, 1.0, 0.4 };
        return hats[s];
    }

    /** Genre of a settings block: the random genre, the preset's category, or -1 for a user grid. */
    int settingsGenre (const DrumSettings& s) noexcept
    {
        if (s.pattern == userPatternIndex())
            return -1;
        if (s.pattern == randomPatternIndex())
            return s.randomGenre;
        const auto& presets = drumPatterns();
        if (s.pattern < 0 || s.pattern >= (int) presets.size())
            return -1;
        return categoryIndex (presets[(size_t) s.pattern].category);
    }

    /** Whether a drum keeps playing the groove under a fill. */
    bool fillKeeps (FillStyle style, int lane, const std::array<bool, numLanes>& customLanes) noexcept
    {
        const auto l = (Lane) lane;
        const bool hats = l == Lane::closedHat || l == Lane::openHat;
        const bool backbeat = l == Lane::snare || l == Lane::clap;
        switch (style)
        {
            case FillStyle::dropOut:     return false;
            case FillStyle::hatLift:     return ! hats && ! backbeat;
            case FillStyle::kickBuild:   return l != Lane::kick;
            case FillStyle::rimShuffle:  return l == Lane::kick || hats || l == Lane::shaker || l == Lane::ride;
            case FillStyle::openHatRun:  return ! hats;
            case FillStyle::clapStutter: return ! backbeat;
            case FillStyle::percBreak:   return l == Lane::closedHat || l == Lane::shaker || l == Lane::tambourine;
            case FillStyle::custom:      return ! customLanes[(size_t) lane];
            default:                     return l == Lane::kick;   // the kick keeps the floor
        }
    }

    bool followsAccentGroove (int lane) noexcept
    {
        switch ((Lane) lane)
        {
            case Lane::closedHat: case Lane::ride: case Lane::shaker: case Lane::tambourine:
            case Lane::conga: case Lane::cowbell: return true;   // (an open hat is an accent wherever it is)
            default: return false;
        }
    }
}

DrumSettings::DrumSettings() noexcept
{
    for (int l = 0; l < numLanes; ++l)
    {
        notes[(size_t) l] = defaultNote (l);
        levels[(size_t) l] = 1.0f;
        laneSwing[(size_t) l] = 1.0f;
        user[(size_t) l].fill ('.');
        userVelocity[(size_t) l].fill (0);
        userChance[(size_t) l].fill (0);
        userRoll[(size_t) l].fill (0);
        customFill[(size_t) l].fill ('.');
    }
    grooveTemplate.level.fill (1.0f);
}

//==============================================================================
GrooveEngine::GrooveEngine()
{
    offTime.fill (-1.0);
    laneOf.fill (0);
    pending.reserve (512);
    scratch.reserve (128);
}

DrumPattern GrooveEngine::basePattern (const DrumSettings& s)
{
    const auto& presets = drumPatterns();

    const int length = patternLength (s);

    if (s.pattern == randomPatternIndex())
    {
        auto p = makeRandomPattern (s.riffSeed, s.density, s.randomGenre);
        fitPattern (p, length);
        if (length > randomBeatLength)   // four bars: the last one turns around
            p = makeVariation (p, s.riffSeed + 31u);
        return p;
    }

    if (s.pattern == userPatternIndex())
    {
        DrumPattern p;
        p.name = "User pattern";
        for (int l = 0; l < numLanes; ++l)
            p.lanes[(size_t) l].assign (s.user[(size_t) l].begin(), s.user[(size_t) l].end());
        fitPattern (p, length);
        return p;
    }

    // Every groove plays as two (or four) bars, a one-bar groove repeated: A-D are always a
    // quarter of it.
    auto p = presets[(size_t) std::clamp (s.pattern, 0, (int) presets.size() - 1)];
    fitPattern (p, length);
    return p;
}

DrumPattern GrooveEngine::resolvePattern (const DrumSettings& s)
{
    auto p = applySegmentOrder (basePattern (s), s.segmentOrder);
    if (s.sectionVariation != 0)
        p = makeSectionVariation (p, s.sectionVariation);
    return applySection (p, s.section);
}

int GrooveEngine::patternLength (const DrumSettings& s) noexcept
{
    return s.grooveBars >= 4 ? 64 : 32;
}

int GrooveEngine::loopLengthInSteps (const DrumSettings& s) noexcept
{
    int steps = patternLength (s);
    if (s.variationEvery > 0)
        steps = lcm (steps, s.variationEvery * 16);
    if (s.fillEveryBars > 0)
        steps = lcm (steps, s.fillEveryBars * 16);
    return steps;
}

int GrooveEngine::previewStepVelocity (const DrumSettings& s, int lane, int step, char symbol) noexcept
{
    if (lane < 0 || lane >= numLanes || ! isHit (symbol))
        return 0;

    const int stepInBar = ((step % 16) + 16) % 16;
    double level = symbol == 'g' ? ghostLevel (lane) : symbolLevel (symbol);
    const auto& groove = s.grooveTemplate;
    if (groove.active && lane != (int) Lane::kick)
        level *= std::clamp ((double) groove.level[(size_t) stepInBar], 0.2, 1.6);
    else if (followsAccentGroove (lane) && symbol != 'g' && ! groove.active)
    {
        const double a = std::clamp ((double) s.accentGroove, 0.0, 1.0);
        const double shape = 1.0 - a * (1.0 - accentProfile (lane, stepInBar % 4, settingsGenre (s)));
        level *= symbol == 'X' ? std::max (shape, 0.9) : shape * (1.0 + 0.25 * a);
    }
    else if (lane == (int) Lane::openHat && symbol == 'x' && ! groove.active)
        level *= stepInBar % 4 == 2 ? 1.0 : 1.0 - 0.15 * std::clamp ((double) s.accentGroove, 0.0, 1.0);

    level *= (double) s.levels[(size_t) lane] * sectionVelocityScale (s.section);
    return std::clamp ((int) std::lround ((double) s.velocity * level), 1, 127);
}

FillStyle GrooveEngine::effectiveFillStyle (FillStyle style, long long bar, int genre) noexcept
{
    if (style != FillStyle::mix)
        return style;

    // Mix: a different fill now and then, always the same one on the same bar.
    // House keeps the original list. Garage, afro, neo soul and chill pick from their own.
    auto pick = [bar] (const FillStyle* choices, size_t n)
    {
        return choices[(size_t) (noise (bar, 4242) * (double) n) % n];
    };
    if (genre == 6 || genre == 10)
    {
        static constexpr FillStyle choices[] { FillStyle::rimShuffle, FillStyle::snareRoll, FillStyle::clapBuild, FillStyle::hatLift };
        return pick (choices, std::size (choices));
    }
    if (genre == 7)
    {
        static constexpr FillStyle choices[] { FillStyle::congaRun, FillStyle::percBreak, FillStyle::rimShuffle, FillStyle::clapBuild };
        return pick (choices, std::size (choices));
    }
    if (genre == 11)
    {
        static constexpr FillStyle choices[] { FillStyle::hatLift, FillStyle::dropOut, FillStyle::rimShuffle, FillStyle::percBreak };
        return pick (choices, std::size (choices));
    }
    static constexpr FillStyle choices[] { FillStyle::snareRoll, FillStyle::tomRun, FillStyle::clapBuild, FillStyle::hatLift,
                                           FillStyle::snareRoll, FillStyle::congaRun, FillStyle::kickBuild, FillStyle::rimShuffle };
    return choices[(size_t) (noise (bar, 4242) * (double) std::size (choices)) % std::size (choices)];
}

bool GrooveEngine::samePattern (const DrumSettings& a, const DrumSettings& b) noexcept
{
    const bool usesSeed = a.pattern == randomPatternIndex();
    const bool usesUser = a.pattern == userPatternIndex();
    // riffSeed feeds every turnaround (prepare), not only the random beat. Leaving it out
    // kept a stale variation on the audio thread after the seed changed, so playback and export differed.
    return a.pattern == b.pattern && a.section == b.section && a.segmentOrder == b.segmentOrder
        && a.sectionVariation == b.sectionVariation && a.grooveBars == b.grooveBars
        && a.riffSeed == b.riffSeed
        && (! usesSeed || (a.density == b.density && a.randomGenre == b.randomGenre))
        && (! usesUser || a.user == b.user);
}

void GrooveEngine::prepare (Prepared& r, const DrumSettings& s)
{
    r.key = s;
    r.pattern = resolvePattern (s);
    // Another turnaround each phrase (the first as it always was), and a bigger one at the end of
    // eight bars, so a long loop never plays the same four bars twice in a row.
    const auto seed = (std::uint32_t) std::max (0, s.pattern) * 7919u + s.riffSeed;
    for (size_t k = 0; k < r.variations.size(); ++k)
        r.variations[k] = makeVariation (r.pattern, seed + (std::uint32_t) k * 104729u);
    r.strongVariation = makeVariation (makeVariation (r.pattern, seed + 7u), seed + 13u);
    r.genre = categoryIndex (r.pattern.category);
    const auto& kick = r.pattern[Lane::kick];
    r.fourFloor = kick.size() >= 16 && isHit (kick[0]) && isHit (kick[4]) && isHit (kick[8]) && isHit (kick[12]);
}

const GrooveEngine::Prepared& GrooveEngine::patternFor (const DrumSettings& s)
{
    if (prepared != nullptr && samePattern (prepared->key, s))
        return *prepared;
    if (! cacheValid || ! samePattern (cache.key, s))
    {
        prepare (cache, s);
        cacheValid = true;
    }
    return cache;
}

//==============================================================================
void GrooveEngine::collectStep (long long step, const DrumSettings& s, const Prepared& resolved)
{
    if (stepEnd >= stepBegin && (step < stepBegin || step >= stepEnd))
        return;

    const DrumPattern& normal = resolved.pattern;
    const long long bar = floorDiv (step, 16);
    const long long sectionBar = bar - s.sectionStartBar;   // fills, crashes and phrases count from the section's start
    const int stepInBar = mod (step, 16);
    const long long sectionStep = step - s.sectionStartBar * 16;

    // Every N bars the last bar plays a turnaround: another one each phrase, a bigger one when
    // two phrases end.
    const bool varied = s.variationEvery > 0 && mod (sectionBar, s.variationEvery) == s.variationEvery - 1;
    const DrumPattern* chosen = &normal;
    if (varied)
    {
        const bool strong = mod (sectionBar + 1, s.variationEvery * 2) == 0;
        const auto& v = strong ? resolved.strongVariation
                               : resolved.variations[(size_t) mod (floorDiv (sectionBar, s.variationEvery), (long long) resolved.variations.size())];
        if (v.length() == normal.length())
            chosen = &v;
    }
    const DrumPattern& pattern = *chosen;
    const DrumPattern& variation = pattern;
    const int len = std::max (1, pattern.length());
    const int pos = mod (sectionStep, len);

    const auto& groove = s.grooveTemplate;
    const double swingDelay = groove.active ? (double) groove.timing[(size_t) stepInBar] * stepLength
                            : (step % 2 != 0) ? std::clamp (s.swing, 0.0f, 1.5f) * stepLength / 3.0 : 0.0;
    const double human = std::clamp (s.humanize, 0.0f, 1.0f);
    const int seedSalt = (int) (s.humanizeSeed % 100003u) * 7919;
    const double tempo = std::clamp (s.tempo, 20.0, 400.0);

    // Fills: the end of every N bars of the section (or a fill asked for on one bar).
    auto isFillBar = [&s] (long long b)
    {
        if (s.forcedFillBar >= 0)
            return b == s.forcedFillBar && s.forcedFillStyle >= 0;
        return s.fillEveryBars > 0 && sectionAllowsFills (s.section) && mod (b - s.sectionStartBar, s.fillEveryBars) == s.fillEveryBars - 1;
    };
    const bool fillBar = isFillBar (bar);
    const FillStyle style = (s.forcedFillBar >= 0 && bar == s.forcedFillBar && s.forcedFillStyle >= 0)
                                ? effectiveFillStyle ((FillStyle) std::clamp (s.forcedFillStyle, 0, numFillStyles - 1), bar, resolved.genre)
                                : effectiveFillStyle (s.fillStyle, bar, resolved.genre);
    // The custom fill takes over from its first written step.
    int fillStart = fillStartStep (style);
    std::array<bool, numLanes> customLanes {};
    if (style == FillStyle::custom)
    {
        fillStart = 16;
        for (int l = 0; l < numLanes; ++l)
            for (int i = 0; i < 16; ++i)
                if (isHit (s.customFill[(size_t) l][(size_t) i]))
                {
                    customLanes[(size_t) l] = true;
                    fillStart = std::min (fillStart, i);
                }
    }
    const bool inFill = fillBar && stepInBar >= fillStart;

    // The build-up: how far into it we are (the whole section in a song, the loop when live).
    const double buildSteps = s.sectionBars > 0 ? s.sectionBars * 16.0 : (double) len;
    const double buildProgress = std::clamp ((s.sectionBars > 0 ? (double) sectionStep : (double) pos) / std::max (1.0, buildSteps - 1.0), 0.0, 1.0);

    // A velocity set by hand on this step of the user pattern (stored per step as written,
    // before the A-D order). It stays on the variation bar and under a fill, except on the hits
    // they change.
    // The same goes for the chance and the roll set on a step.
    auto userIndex = [&] (int lane) -> int
    {
        if (s.pattern != userPatternIndex() || len != patternLength (s))
            return -1;
        if (varied && variation.lanes[(size_t) lane][(size_t) pos] != normal.lanes[(size_t) lane][(size_t) pos])
            return -1;
        const int segment = len / numSegments;
        return std::clamp (s.segmentOrder[(size_t) (pos / segment)], 0, numSegments - 1) * segment + pos % segment;
    };
    auto handVelocity = [&] (int lane) -> int
    {
        const int index = userIndex (lane);
        return index < 0 ? 0 : s.userVelocity[(size_t) lane][(size_t) index];
    };

    auto velocityFor = [&] (int lane, double level, int salt)
    {
        double v = s.velocity * level * (double) s.levels[(size_t) lane] * sectionVelocityScale (s.section);
        if (s.section == Section::buildup)   // crescendo through the build-up, the snare from almost nothing
            v *= lane == (int) Lane::snare ? 0.35 + 0.8 * buildProgress : 0.65 + 0.45 * buildProgress;
        // Humanize: the kick and the backbeat stay steady, the rest breathes.
        const bool steadyBackbeat = (lane == (int) Lane::snare || lane == (int) Lane::clap) && stepInBar % 4 == 0;
        const double spread = lane == (int) Lane::kick ? 3.0 : steadyBackbeat ? 5.0 : 12.0;
        v += human * spread * (noise (step, seedSalt + salt + 7) - 0.5) * 2.0;
        return std::clamp ((int) std::lround (v), 1, 127);
    };

    // Exact genre: "Jackin' Piano" also begins with "Jackin" and must not grow a 909 turnaround.
    const bool jackin = pattern.category == "Jackin'";
    const bool hasSnare = std::any_of (pattern.lanes[(size_t) Lane::snare].begin(), pattern.lanes[(size_t) Lane::snare].end(), isHit);
    // Jackin': the clap lands a hair before the 909 snare under it, so the two flam.
    const double jackFlam = jackin && hasSnare ? -0.006 * tempo / 60.0 : 0.0;

    auto timeFor = [&] (int lane, int salt)
    {
        // The kick: no push or pull, no humanize. On the beat and the 8ths it is exactly on the
        // grid; a kick on an off 16th (two-step, broken beats) swings with the hats.
        if (lane == (int) Lane::kick)
            return (double) step * stepLength
                 + (step % 2 != 0 ? swingDelay * std::clamp ((double) s.laneSwing[(size_t) lane], 0.0, 2.0) : 0.0);
        // Each drum takes its share of the swing and sits ahead of or behind the beat (with the
        // genre's own pocket on top); humanize moves a hit around a point a little behind the
        // grid, the hats around the grid itself.
        double pocketMs = 0.0, pocketSwing = 1.0;
        if (s.genreFeel)
            genrePocket (resolved.genre, lane, pocketMs, pocketSwing);
        const double shiftMs = std::clamp ((double) s.laneShift[(size_t) lane] + pocketMs, -40.0, 40.0);
        const double shift = shiftMs / 1000.0 * tempo / 60.0;
        const double centre = isHatLike (lane) ? 0.5 : 0.3;
        const double t = (double) step * stepLength
                       + swingDelay * std::clamp ((double) s.laneSwing[(size_t) lane] * pocketSwing, 0.0, 2.0) + shift
                       + (lane == (int) Lane::clap ? jackFlam : 0.0)
                       + human * laneLooseness (lane) * 0.025 * (noise (step, seedSalt + salt) - centre);
        return step >= 0 ? std::max (t, 0.0) : t;
    };

    // Life (with Humanize): over a four-bar phrase the soft notes come and go, as in a played or
    // programmed loop: ghosts, the hats' and shakers' 16ths between the offbeats, the clap's
    // pick-ups. Each eight bars it comes out a little differently (NEW FEEL makes another).
    // The kick and the accents never change.
    const int phraseBar = mod (sectionBar, 4);
    const int phrase = mod (floorDiv (sectionBar, 8), 4);
    auto isSoft = [&] (int lane, char symbol)
    {
        if (symbol == 'X' || lane == (int) Lane::kick)
            return false;
        if (symbol == 'g')
            return true;
        // Ghosts on the "e" come and go. The swung "a" is the accent (jackin'
        // and garage) and stays put, the same way a written 'X' does.
        if (followsAccentGroove (lane) && stepInBar % 2 == 1)
            return stepInBar % 4 != 3;
        return (lane == (int) Lane::clap || lane == (int) Lane::snare || lane == (int) Lane::rim) && stepInBar % 4 != 0;
    };
    auto lifeNoise = [&] (int lane, int salt) { return noise (phraseBar * 16 + stepInBar, seedSalt + lane * 101 + salt + phrase * 977); };

    auto add = [&] (int lane, char symbol, int note, double levelScale = 1.0, bool fromFill = false, int roll = 1)
    {
        if (s.muted[(size_t) lane] || ! isHit (symbol))
            return;
        if (! fromFill && human > 0.0 && isSoft (lane, symbol) && lifeNoise (lane, 77) < 0.4 * human)
            return;   // not in this bar of the phrase
        const int salt = lane * 13 + 1;
        if (! fromFill)
            if (const int index = userIndex (lane); index >= 0)
            {
                // A step that only plays now and then (the same way every time it comes round).
                const int chance = s.userChance[(size_t) lane][(size_t) index];
                if (chance > 0 && chance < 100 && noise (step, seedSalt + salt + 31) * 100.0 >= chance)
                    return;
                roll = std::max (roll, (int) s.userRoll[(size_t) lane][(size_t) index]);
            }
        const double t = timeFor (lane, salt);
        double level = (symbol == 'g' ? ghostLevel (lane) : symbolLevel (symbol)) * levelScale;
        if (groove.active && lane != (int) Lane::kick)
            level *= std::clamp ((double) groove.level[(size_t) stepInBar], 0.2, 1.6);
        if (const int set = fromFill ? 0 : handVelocity (lane); set > 0)
            level = set / 100.0;   // 100 = as the Velocity control
        else if (followsAccentGroove (lane) && symbol != 'g' && ! groove.active)
        {
            // The accent groove: written accents stay on top, the rest follows the beat.
            const double a = std::clamp ((double) s.accentGroove, 0.0, 1.0);
            const double shape = 1.0 - a * (1.0 - accentProfile (lane, stepInBar % 4, resolved.genre));
            level *= symbol == 'X' ? std::max (shape, 0.9) : shape * (1.0 + 0.25 * a);
        }
        else if (lane == (int) Lane::openHat && symbol == 'x' && ! groove.active)
            level *= stepInBar % 4 == 2 ? 1.0 : 1.0 - 0.15 * std::clamp ((double) s.accentGroove, 0.0, 1.0);   // the "and" sings
        roll = lane == (int) Lane::kick ? 1 : std::clamp (roll, 1, 4);   // a roll would take the kick off the grid
        if (roll > 1)
        {
            // A roll: that many quick hits in the 16th, the first one the loudest.
            const double gap = stepLength / roll;
            for (int k = 0; k < roll; ++k)
                pending.push_back ({ t + gap * k, note, velocityFor (lane, level * (k == 0 ? 1.0 : 0.82), salt + k), gap * 0.9, lane });
            return;
        }
        pending.push_back ({ t, note, velocityFor (lane, level, salt), noteLength (lane), lane });
        if (symbol == 'f' && lane != (int) Lane::kick)   // the flam's grace note, just before (never on the kick)
            pending.push_back ({ t - 0.035, note, velocityFor (lane, 0.4 * levelScale, salt + 3), 0.03, lane });
    };

    // A fill's tom or conga a few notes from its drum's own, unless that note plays another drum
    // (a drum rack or a sampler with its own map): then the drum's own note.
    auto nearNote = [&s] (int lane, int offset)
    {
        const int n = s.notes[(size_t) lane] + offset;
        if (offset == 0 || n < 0 || n > 127)
            return s.notes[(size_t) lane];
        for (int l = 0; l < numLanes; ++l)
            if (l != lane && s.notes[(size_t) l] == n)
                return s.notes[(size_t) lane];
        return n;
    };

    // The song: the main part starts lighter (no hand percussion for its first eight bars).
    // Jackin' keeps the cowbell and the tom from bar one. Other genres still
    // start the main part without hand percussion.
    const bool lightStart = ! jackin && s.section == Section::main && s.sectionBars >= 16 && sectionBar >= 0 && sectionBar < 8;
    // The build-up's last two beats: no kick, so the drop hits harder.
    const bool dropGap = s.section == Section::buildup && s.sectionBars > 0 && sectionStep >= s.sectionBars * 16 - 8;

    // ---- The groove as written (what a fill leaves of it) ----
    for (int lane = 0; lane < numLanes; ++lane)
    {
        char symbol = pattern.lanes[(size_t) lane][(size_t) pos];
        if (inFill && ! fillKeeps (style, lane, customLanes))
            continue;
        if (lightStart && (lane == (int) Lane::conga || lane == (int) Lane::cowbell || lane == (int) Lane::tom))
            continue;
        if (lane == (int) Lane::kick)
        {
            if (dropGap)
                continue;
            // Room for the bass: on a four-on-the-floor groove the kick's pick-ups only come at the end
            // of four bars, and never on the last 16th before the one.
            if (s.bassSpace && resolved.fourFloor && stepInBar % 4 != 0 && (stepInBar == 15 || phraseBar != 3))
                continue;
        }
        // The build-up's snare roll: quarters, 8ths, 16ths, then 32nds in its last bar.
        int roll = 1;
        if (s.section == Section::buildup && lane == (int) Lane::snare)
        {
            const int every = buildSteps > 16.0 ? (buildProgress < 0.25 ? 4 : buildProgress < 0.5 ? 2 : 1)
                                                : (buildProgress < 0.5 ? 2 : 1);
            const long long at = s.sectionBars > 0 ? sectionStep : pos;
            symbol = mod (at, every) == 0 ? (stepInBar % 4 == 0 ? 'X' : 'x') : '.';
            if (buildSteps >= 32.0 && (s.sectionBars > 0 ? sectionStep : pos) >= (long long) buildSteps - 16)
                roll = 2;
        }
        // The open hat and the closed hat on the same step: the open hat (one hi-hat can't play
        // both, and the closed one would choke it in a drum rack).
        if (lane == (int) Lane::closedHat && isHit (symbol) && isHit (pattern.lanes[(size_t) Lane::openHat][(size_t) pos])
            && ! s.muted[(size_t) Lane::openHat] && ! (inFill && ! fillKeeps (style, (int) Lane::openHat, customLanes)))
            continue;
        // ...and now and then a soft note that isn't written: a hat or shaker 16th, or the clap's
        // ghost, just before the next bar.
        if (symbol == '.' && human > 0.0 && ! inFill && stepInBar >= 13 && stepInBar % 2 == 1 && phraseBar % 2 == 1)
        {
            const bool plays = std::any_of (pattern.lanes[(size_t) lane].begin(), pattern.lanes[(size_t) lane].end(), isHit);
            const bool suits = (followsAccentGroove (lane) && lane != (int) Lane::openHat && lane != (int) Lane::ride)
                            || (lane == (int) Lane::clap && stepInBar == 15);
            if (plays && suits && lifeNoise (lane, 91) < 0.3 * human)
                symbol = 'g';
        }
        // Jackin': at the end of the phrase the 909 snare (or the clap, with no snare) turns the
        // beat round with loud swung 16ths into the next bar.
        if (symbol == '.' && human > 0.0 && ! inFill && phraseBar == 3 && stepInBar >= 13 && jackin
            && lane == (hasSnare ? (int) Lane::snare : (int) Lane::clap) && lifeNoise (lane, 57) < 0.55)
            symbol = stepInBar == 14 ? 'g' : 'x';
        add (lane, symbol, s.notes[(size_t) lane], 1.0, false, roll);
    }

    // ---- The fill ----
    if (inFill)
    {
        const int start = fillStart;
        const double through = (double) (stepInBar - start) / (double) std::max (1, 15 - start);
        const double ramp = 0.62 + 0.45 * through;
        const int snare = (int) Lane::snare, clap = (int) Lane::clap, tom = (int) Lane::tom, conga = (int) Lane::conga;

        switch (style)
        {
            case FillStyle::snareRoll:
            case FillStyle::clapBuild:
            {
                // From soft to loud, 32nds on the last beat.
                const int lane = style == FillStyle::snareRoll ? snare : clap;
                if (stepInBar >= 12 || stepInBar % 2 == 0)
                    add (lane, stepInBar >= 14 ? 'X' : 'x', s.notes[(size_t) lane], 0.35 + 0.75 * through, true, stepInBar >= 12 ? 2 : 1);
                break;
            }
            case FillStyle::tomRun:
            {
                static constexpr int down[] { 5, 3, 0, -2 };   // high tom to floor tom
                add (tom, stepInBar == 12 ? 'X' : 'x', nearNote (tom, down[std::clamp (stepInBar - 12, 0, 3)]), ramp, true);
                break;
            }
            case FillStyle::hatLift:
                if (stepInBar == 12 || stepInBar == 14)
                    add ((int) Lane::openHat, 'X', s.notes[(size_t) Lane::openHat], 1.0, true);
                if (stepInBar == 15)
                    add (snare, 'g', s.notes[(size_t) snare], 1.0, true);
                break;
            case FillStyle::dropOut:
                if (stepInBar == 15)
                    add (snare, 'X', s.notes[(size_t) snare], 1.0, true);
                break;
            case FillStyle::congaRun:
                add (conga, stepInBar == 12 || stepInBar == 15 ? 'X' : 'x', nearNote (conga, stepInBar >= 14 ? 1 : 0), ramp, true);
                break;
            case FillStyle::kickBuild:   // 16th kicks into the next bar (on the grid)
                if (stepInBar >= 12)
                    add ((int) Lane::kick, stepInBar == 15 ? 'X' : 'x', s.notes[(size_t) Lane::kick], ramp, true);
                break;
            case FillStyle::rimShuffle:
            {
                static constexpr char line[] = "x.xxx.xX";
                add ((int) Lane::rim, line[stepInBar - 8], s.notes[(size_t) Lane::rim], ramp, true);
                break;
            }
            case FillStyle::openHatRun:
                if (stepInBar >= 12 || stepInBar % 2 == 0)
                    add ((int) Lane::openHat, stepInBar >= 14 ? 'X' : 'x', s.notes[(size_t) Lane::openHat], ramp, true);
                break;
            case FillStyle::clapStutter:
                if (stepInBar != 13)
                    add (clap, stepInBar == 15 ? 'X' : 'x', s.notes[(size_t) clap], ramp, true, stepInBar >= 14 ? 2 : 1);
                break;
            case FillStyle::percBreak:
            {
                static constexpr char congas[] = "x.x.xx.xx.xxxxxX";
                add (conga, congas[stepInBar], nearNote (conga, stepInBar % 3 == 1 ? 1 : 0), ramp, true);
                if (stepInBar == 8 || stepInBar == 11 || stepInBar == 14)
                    add ((int) Lane::cowbell, 'x', s.notes[(size_t) Lane::cowbell], 1.0, true);
                break;
            }
            case FillStyle::custom:
                for (int l = 0; l < numLanes; ++l)
                    add (l, s.customFill[(size_t) l][(size_t) stepInBar], s.notes[(size_t) l], 1.0, true);
                break;
            case FillStyle::mix:
            default:
                break;
        }
    }

    // ---- Crash: on the one after a fill that closes a phrase, at the start of a song section,
    //      and every 8 bars of the drop ----
    if (stepInBar == 0 && s.crashAfterFill && pattern.lanes[(size_t) Lane::crash][(size_t) pos] == '.')
    {
        bool crash = false;
        if (s.sectionBars > 0 && sectionBar == 0)
        {
            const bool opens = s.section == Section::main || s.section == Section::breakdown
                            || s.section == Section::drop || s.section == Section::outro;
            crash = s.afterSectionFill || (s.sectionStartBar > 0 && opens);
        }
        else
        {
            // A fill every bar or two would crash all the time: only when it ends four bars.
            const bool afterFill = isFillBar (bar - 1) && (s.forcedFillBar >= 0 || mod (sectionBar, 4) == 0);
            const bool dropPhrase = s.section == Section::drop && mod (sectionBar, 8) == 0;
            crash = afterFill || dropPhrase;
        }
        if (crash)
            add ((int) Lane::crash, 'X', s.notes[(size_t) Lane::crash], 1.0, true);
    }
}

//==============================================================================
void GrooveEngine::flushOffsUpTo (double ppq, bool inclusive, std::vector<NoteEvent>& out)
{
    scratch.clear();
    for (int n = 0; n < 128; ++n)
    {
        const double off = offTime[(size_t) n];
        if (off >= 0.0 && (off < ppq || (inclusive && off <= ppq)))
            scratch.push_back (n);
    }

    std::sort (scratch.begin(), scratch.end(),
               [this] (int a, int b) { return offTime[(size_t) a] < offTime[(size_t) b]; });

    for (int n : scratch)
    {
        out.push_back ({ offTime[(size_t) n], n, 0, laneOf[(size_t) n] });
        offTime[(size_t) n] = -1.0;
    }
}


void GrooveEngine::emitPending (double startPpq, double endPpq, std::vector<NoteEvent>& out)
{
    pending.erase (std::remove_if (pending.begin(), pending.end(),
                                   [&] (const PendingOn& p) { return p.ppq < startPpq || p.ppq >= endPpq || p.note < 0 || p.note > 127; }),
                   pending.end());
    for (size_t i = 0; i < pending.size(); ++i)
        pending[i].order = i;
    std::sort (pending.begin(), pending.end(), [] (const PendingOn& a, const PendingOn& b)
    { return a.ppq < b.ppq || (a.ppq == b.ppq && a.order < b.order); });

    for (const auto& p : pending)
    {
        flushOffsUpTo (p.ppq, true, out);

        // The closed hat chokes the open hat, in MIDI too (for samplers without choke groups);
        // not one that has only just started.
        if (p.lane == (int) Lane::closedHat)
            for (int n = 0; n < 128; ++n)
                if (offTime[(size_t) n] > p.ppq && laneOf[(size_t) n] == (int) Lane::openHat && n != p.note
                    && p.ppq - (offTime[(size_t) n] - noteLength ((int) Lane::openHat)) > 0.05)
                {
                    out.push_back ({ p.ppq, n, 0, (int) Lane::openHat });
                    offTime[(size_t) n] = -1.0;
                }

        auto& off = offTime[(size_t) p.note];
        if (off >= 0.0)
            out.push_back ({ p.ppq, p.note, 0, laneOf[(size_t) p.note] });   // retrigger

        out.push_back ({ p.ppq, p.note, p.velocity, p.lane });
        off = p.ppq + p.duration;
        laneOf[(size_t) p.note] = p.lane;
    }

    flushOffsUpTo (endPpq, false, out);
}

void GrooveEngine::render (double startPpq, double endPpq, const DrumSettings& s, std::vector<NoteEvent>& out)
{
    if (endPpq <= startPpq)
        return;

    const auto& resolved = patternFor (s);
    pending.clear();

    // Swing, a template, a drum pulled behind and humanize delay a hit (by < 3 steps), so look back
    // a little; a drum pushed ahead and a flam's grace note come before their step, so look ahead too.
    const auto firstStep = (long long) std::floor (startPpq / stepLength) - 3;
    // A negative template plus a 40 ms push and flam can precede three steps at high tempos.
    const auto lastStep  = (long long) std::floor (endPpq / stepLength) + 3;
    for (auto step = firstStep; step <= lastStep; ++step)
        collectStep (step, s, resolved);

    emitPending (startPpq, endPpq, out);
}

void GrooveEngine::allNotesOff (double ppq, std::vector<NoteEvent>& out)
{
    for (int n = 0; n < 128; ++n)
        if (offTime[(size_t) n] >= 0.0)
        {
            out.push_back ({ ppq, n, 0, laneOf[(size_t) n] });
            offTime[(size_t) n] = -1.0;
        }
}

bool GrooveEngine::anyNoteSounding() const noexcept
{
    return std::any_of (offTime.begin(), offTime.end(), [] (double t) { return t >= 0.0; });
}

//==============================================================================
std::vector<NoteEvent> GrooveEngine::renderClip (const DrumSettings& s, double lengthInBeats)
{
    // Only the clip's own steps: the first hit of the next time round (a drum pushed ahead) would
    // sound at the very end, and again at the start when the clip loops.
    GrooveEngine engine;
    engine.stepBegin = 0;
    engine.stepEnd = (long long) std::ceil (lengthInBeats / stepLength - 1.0e-9);
    std::vector<NoteEvent> events;
    engine.render (0.0, lengthInBeats, s, events);
    engine.allNotesOff (lengthInBeats, events);
    return events;
}

std::vector<NoteEvent> GrooveEngine::renderArrangement (const DrumSettings& s, const std::vector<ArrangementPart>& parts,
                                                        std::vector<Marker>* markers)
{
    // One pass over the whole song: each part plays its own bars with its own settings, and a hit
    // pushed ahead of a part's first beat (or held over its end) still belongs to that part.
    GrooveEngine engine;
    std::vector<NoteEvent> events;
    long long bar = 0;
    bool previousEndedWithFill = false;

    for (const auto& part : parts)
    {
        if (part.bars <= 0)
            continue;

        auto settings = s;
        settings.section = part.section;
        settings.sectionStartBar = bar;
        settings.sectionBars = part.bars;
        settings.afterSectionFill = previousEndedWithFill;
        settings.sectionVariation = part.variation;
        settings.forcedFillBar = -1;
        if (part.endFill != -1)
        {
            // Its own fill on its last bar (or none at all).
            settings.forcedFillBar = bar + part.bars - 1;
            settings.forcedFillStyle = part.endFill;
        }
        previousEndedWithFill = part.endFill >= 0
                             || (part.endFill == -1 && s.fillEveryBars > 0 && sectionAllowsFills (part.section)
                                 && part.bars % s.fillEveryBars == 0);

        if (markers != nullptr)
            markers->push_back ({ (double) bar * 4.0, sectionName ((int) part.section) });

        const auto& resolved = engine.patternFor (settings);
        engine.stepBegin = 0;
        engine.stepEnd = -1;
        for (auto step = bar * 16; step < (bar + part.bars) * 16; ++step)
            engine.collectStep (step, settings, resolved);
        bar += part.bars;
    }

    const double end = (double) bar * 4.0;
    engine.emitPending (0.0, end, events);
    engine.allNotesOff (end, events);
    return events;
}

} // namespace fgd

