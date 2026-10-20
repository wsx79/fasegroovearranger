// Fase Groove Guitar - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "Generators.h"

#include <algorithm>
#include <cmath>

namespace fgg
{

namespace
{
    /** Small, fast and repeatable (same seed = same beat on every computer). */
    struct Random
    {
        explicit Random (std::uint32_t seed) noexcept : state (seed * 0x9E3779B97F4A7C15ULL + 0x632BE59BD9B4E019ULL) {}

        std::uint32_t next() noexcept
        {
            state += 0x9E3779B97F4A7C15ULL;
            auto z = state;
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            return (std::uint32_t) ((z ^ (z >> 31)) >> 32);
        }

        int nextInt (int n) noexcept           { return n > 0 ? (int) (next() % (std::uint32_t) n) : 0; }
        float nextFloat() noexcept             { return (float) (next() >> 8) * (1.0f / 16777216.0f); }
        bool chance (float probability) noexcept { return nextFloat() < probability; }

        template <typename T, size_t N>
        T pick (const T (&items)[N]) noexcept { return items[nextInt ((int) N)]; }

        std::uint64_t state;
    };

    std::string compact (const char* s)
    {
        std::string out;
        for (; *s != 0; ++s)
            if (*s != ' ')
                out += *s;
        return out;
    }

    void soften (std::string& line)
    {
        for (auto& c : line)
            if (c == 'X')
                c = 'x';
    }

    // The genres, in grooveCategories() order.
    enum Genre { pianoHouse = 0, soulful, deep, gospel, jackin, jackinPiano, garage, afro, funky, nineties, neoSoul, chill, dance };

    void clearLine (std::string& line) { std::fill (line.begin(), line.end(), '.'); }
    bool hits (const std::string& line) { return std::any_of (line.begin(), line.end(), isHit); }
}

//==============================================================================
const char* sectionName (int section) noexcept
{
    static const char* names[] { "Main", "Intro", "Break", "Build-up", "Drop", "Outro" };
    return section >= 0 && section < numSections ? names[section] : "";
}

double sectionVelocityScale (Section section) noexcept
{
    switch (section)
    {
        case Section::intro:     return 0.85;
        case Section::breakdown: return 0.75;
        case Section::buildup:   return 0.95;
        case Section::drop:      return 1.08;
        case Section::outro:     return 0.85;
        case Section::main:
        default:                 return 1.0;
    }
}

bool sectionAllowsFills (Section section) noexcept
{
    return section == Section::main || section == Section::drop;
}

GuitarPattern applySection (const GuitarPattern& source, Section section)
{
    GuitarPattern p = source;
    const int len = p.length();

    switch (section)
    {
        case Section::intro:
        case Section::outro:
        {
            // Only the light parts (chanks, scratches, up-strums, picking), a little softer.
            for (int l = 0; l < numLanes; ++l)
            {
                const auto lane = (Lane) l;
                if (lane == Lane::chank || lane == Lane::scratch || lane == Lane::strumUp || lane == Lane::arp)
                    soften (p.lanes[(size_t) l]);
                else
                    clearLine (p.lanes[(size_t) l]);
            }
            if (std::none_of (p.lanes.begin(), p.lanes.end(), hits))
                for (int i = 2; i < len; i += 4)
                    p[Lane::chank][(size_t) i] = 'x';
            break;
        }
        case Section::breakdown:
        {
            // A held chord on every bar; only the picking goes on under it, softly.
            for (int l = 0; l < numLanes; ++l)
                if ((Lane) l == Lane::arp)
                    soften (p.lanes[(size_t) l]);
                else
                    clearLine (p.lanes[(size_t) l]);
            for (int i = 0; i < len; i += 16)
                p[Lane::swell][(size_t) i] = 'X';
            break;
        }
        case Section::buildup:
        {
            // A muted scratch that doubles up (quarters, 8ths, 16ths), a strum on every bar.
            for (auto& line : p.lanes)
                clearLine (line);
            for (int i = 0; i < len; ++i)
            {
                const double at = (double) i / (double) len;
                const int every = at < 0.5 && len > 16 ? 4 : at < 0.75 ? 2 : 1;
                if (i % 16 == 0)
                    p[Lane::strumDown][(size_t) i] = 'X';
                else if (i % every == 0)
                    p[Lane::scratch][(size_t) i] = i % 4 == 0 ? 'X' : 'x';
            }
            break;
        }
        case Section::drop:   // the full groove (the engine adds a big strum every 8 bars)
        case Section::main:
        default:
            break;
    }

    if (section != Section::main)
        p.name = source.name + " / " + sectionName ((int) section);
    return p;
}

//==============================================================================
namespace
{
    // One-bar cells and how common each is in house guitar (the weight).
    struct Cell { const char* steps; int weight; };

    const Cell stabCells[] {
        { "..X. ..X. ..X. ..X.", 30 }, { "..X. ..X. ..X. .xX.", 10 }, { "..X. ..x. ..X. ..x.", 10 }, { "...X ..X. ...X ..X.", 6 },
        { "..X. ...X ..X. ....", 6 }, { "X... ..X. ..X. X...", 5 }, { "X..X ..X. .X.. X.X.", 4 } };
    const Cell twoStepCells[] {
        { "X... ..x. ...x ....", 10 }, { "X... ...x ..x. ....", 8 }, { "...X .... ..x. ...x", 6 }, { "X..x .... ..x. ....", 5 } };
    const Cell chankCells[] {
        { "..x. ..x. ..x. ..x.", 20 }, { "x.xx .x.x x.xx .x.x", 8 }, { "..xx ..x. ..xx ..x.", 8 }, { "x..x ..x. ..x. x...", 6 },
        { ".x.x .x.x .x.x .x.x", 5 }, { "..x. .xx. ..x. .x.x", 5 }, { "x.xx .xx. x.xx .x.x", 4 } };
    const Cell arpCells[] {
        { "x.x. x.x. x.x. x.x.", 18 }, { "x.xx x.xx x.xx x.xx", 10 }, { "xxxx xxxx xxxx xxxx", 5 }, { "x.x. xx.x x.x. xx.x", 8 },
        { "x.xx .xx. x.xx .xx.", 6 }, { "x.x. .x.x x.x. .x.x", 5 } };
    // Funk sixteenths: the hand goes down on the even 16ths and up on the odd ones.
    const Cell downCells[] {
        { "X... ..x. ..x. x...", 10 }, { "X... ..x. X... x.x.", 8 }, { "X.x. ..x. X... ..x.", 6 }, { "X... x... X... x...", 6 },
        { "X... .... X... ....", 5 } };
    const Cell upCells[] {
        { ".x.x ...x .x.. .x.x", 8 }, { "...x .x.. ...x ...x", 8 }, { "...x ...x ...x ...x", 6 }, { ".x.. .x.. .x.. .x.x", 5 },
        { ".... ..x. .... ..x.", 4 } };
    const Cell eighthDownCells[] { { "X... X... X... X...", 8 }, { "X... x... X... x...", 6 }, { "X... .... X... x...", 4 } };
    const Cell eighthUpCells[]   { { "..x. ..x. ..x. ..x.", 8 }, { ".... ..x. .... ..x.", 5 }, { "..x. ..x. ..x. .xx.", 4 } };
    const Cell powerCells[] {
        { "X.x. X.x. X.x. X.x.", 8 }, { "X... ..X. ..X. X...", 8 }, { "..X. ..X. ..X. ..X.", 6 }, { "X..X ..X. X..X ..X.", 5 } };
    const Cell octaveCells[] { { "X.x. x.x. X.x. x.x.", 8 }, { "x..x ..x. x..x ..x.", 6 }, { "X.xx x.xx X.xx x.xx", 4 } };
    const Cell rootCells[]   { { "X... .... X... ....", 10 }, { "X... .... X..x ....", 6 }, { "X... X... X... X...", 5 }, { "X... .... ..x. ....", 4 } };
    const Cell lickCells[]   { { ".... .... .... x.x.", 6 }, { ".... ..x. .... .x.x", 5 }, { "..x. .x.. .... ....", 4 }, { ".... .... .xxx x...", 3 } };

    template <size_t N>
    const char* pickCell (Random& r, const Cell (&cells)[N])
    {
        int total = 0;
        for (const auto& c : cells) total += c.weight;
        int x = r.nextInt (total);
        for (const auto& c : cells)
            if ((x -= c.weight) < 0)
                return c.steps;
        return cells[0].steps;
    }

    /** The main part a genre's guitarist plays, and how likely each one is. */
    enum Role { funk16 = 0, offStab, chankLine, arpPick, swellPad, powerDrive, octaveDisco, strum8, twoStep, numRoles };

    struct GenreStyle
    {
        int weights[numRoles];
        float scratch;   // muted scratches filling the 16ths
        float root;      // a root note on the low string
        float lick;      // a lick answering at the end of bar 2
    };

    GenreStyle styleOf (int genre)
    {
        //                        funk stab chank arp swell power oct strum8 2step
        switch (genre)
        {
            case pianoHouse:  return { { 3, 6, 3, 1, 1, 0, 1, 4, 0 }, 0.35f, 0.3f, 0.2f };
            case soulful:     return { { 5, 2, 4, 3, 1, 0, 0, 3, 0 }, 0.45f, 0.4f, 0.3f };
            case deep:        return { { 0, 5, 3, 4, 5, 0, 0, 0, 1 }, 0.2f,  0.3f, 0.2f };
            case gospel:      return { { 2, 4, 1, 3, 1, 0, 0, 6, 0 }, 0.2f,  0.5f, 0.2f };
            case jackin:      return { { 4, 6, 6, 0, 0, 1, 3, 0, 0 }, 0.6f,  0.2f, 0.2f };
            case jackinPiano: return { { 4, 7, 3, 0, 0, 2, 1, 3, 0 }, 0.4f,  0.3f, 0.1f };
            case garage:      return { { 1, 3, 4, 1, 2, 0, 0, 0, 6 }, 0.4f,  0.2f, 0.2f };
            case afro:        return { { 1, 1, 5, 7, 0, 0, 1, 0, 0 }, 0.3f,  0.5f, 0.3f };
            case funky:       return { { 8, 2, 4, 0, 0, 0, 5, 1, 0 }, 0.7f,  0.3f, 0.3f };
            case nineties:    return { { 1, 5, 1, 1, 0, 4, 2, 5, 0 }, 0.2f,  0.5f, 0.1f };
            case neoSoul:     return { { 2, 3, 1, 4, 2, 0, 0, 1, 3 }, 0.2f,  0.3f, 0.6f };
            case chill:       return { { 0, 1, 1, 6, 6, 0, 0, 2, 0 }, 0.1f,  0.2f, 0.3f };
            case dance:       return { { 0, 5, 0, 0, 0, 7, 2, 2, 0 }, 0.1f,  0.4f, 0.0f };
            default:          return { { 2, 4, 3, 2, 1, 1, 1, 2, 1 }, 0.3f,  0.3f, 0.2f };
        }
    }

    /** At low density some plain hits (never the accents, never the first of the bar) rest. */
    void thin (std::string& line, Random& r, float density)
    {
        const float drop = std::max (0.0f, 0.45f - density) * 0.9f;
        for (size_t i = 1; i < line.size(); ++i)
            if ((line[i] == 'x' || line[i] == 'g') && i % 16 != 0 && r.chance (drop))
                line[i] = '.';
    }
}

GuitarPattern makeRandomPattern (std::uint32_t seed, float density, int genre)
{
    // A groove is built as house guitarists play: ONE main part (a funk strum, offbeat stabs,
    // a chank line, picking, a swell...), and maybe a root on the low string, the muted scratch
    // between the hits and a lick that answers at the end. Bar 2 repeats bar 1 with its answer.
    Random r (seed);
    density = std::clamp (density, 0.0f, 1.0f);
    genre = std::clamp (genre, 0, (int) grooveCategories().size() - 1);
    const auto style = styleOf (genre);
    const float more = 0.35f + 1.1f * density;

    GuitarPattern p;
    p.name = "Random groove";
    p.category = grooveCategories()[(size_t) genre];
    for (auto& line : p.lanes)
        line.assign ((size_t) randomBeatLength, '.');

    auto setBar = [&p] (Lane lane, const char* cell)   // one bar, written into both bars
    {
        const auto c = compact (cell);
        for (int i = 0; i < 16 && i < (int) c.size(); ++i)
            p[lane][(size_t) i] = p[lane][(size_t) (i + 16)] = c[(size_t) i];
    };

    int total = 0;
    for (int w : style.weights) total += w;
    int x = r.nextInt (std::max (1, total)), role = 0;
    for (; role < numRoles - 1; ++role)
        if ((x -= style.weights[role]) < 0)
            break;

    bool scratchSuits = true;
    switch ((Role) role)
    {
        case funk16:
            setBar (Lane::strumDown, pickCell (r, downCells));
            setBar (Lane::strumUp, pickCell (r, upCells));
            break;
        case offStab:
            setBar (Lane::stab, pickCell (r, stabCells));
            break;
        case chankLine:
            setBar (Lane::chank, pickCell (r, chankCells));
            if (r.chance (0.3f * more))
                setBar (Lane::stab, "X... .... .... ....");
            break;
        case arpPick:
            setBar (Lane::arp, pickCell (r, arpCells));
            if (r.chance (0.5f))
                setBar (Lane::swell, "X... .... .... ....");
            scratchSuits = false;
            break;
        case swellPad:
            setBar (Lane::swell, "X... .... .... ....");
            setBar (r.chance (0.5f) ? Lane::chank : Lane::arp, r.chance (0.5f) ? ".... ..x. .... ..x." : ".... .... x.x. x.x.");
            scratchSuits = false;
            break;
        case powerDrive:
            setBar (Lane::power, pickCell (r, powerCells));
            scratchSuits = false;
            break;
        case octaveDisco:
            setBar (Lane::octave, pickCell (r, octaveCells));
            break;
        case strum8:
            setBar (Lane::strumDown, pickCell (r, eighthDownCells));
            setBar (Lane::strumUp, pickCell (r, eighthUpCells));
            break;
        case twoStep:
        default:
            setBar (Lane::stab, pickCell (r, twoStepCells));
            break;
    }
    for (auto& line : p.lanes)
        thin (line, r, density);

    // ---- The muted scratch between the hits, the root, a lick ----
    if (scratchSuits && r.chance (style.scratch * more))
        setBar (Lane::scratch, density > 0.6f ? "gggg gggg gggg gggg" : r.chance (0.5f) ? ".g.g .g.g .g.g .g.g" : "g... g... g... g.g.");
    if (role != powerDrive && role != octaveDisco && r.chance (style.root * more))
        setBar (Lane::root, pickCell (r, rootCells));
    if (r.chance (style.lick * more))
    {
        const auto lick = compact (pickCell (r, lickCells));
        for (int i = 0; i < 16; ++i)
            if (lick[(size_t) i] != '.')
                p[Lane::lick][(size_t) (16 + i)] = lick[(size_t) i];
    }

    // ---- Bar 2 answers at its very end ----
    if (r.chance (0.3f + 0.4f * density))
        switch (r.nextInt (3))
        {
            case 0:  p[Lane::slide][30] = 'x'; break;
            case 1:  if (p[Lane::strumUp][31] == '.') p[Lane::strumUp][31] = 'x'; break;
            default: p[Lane::scratch][28] = p[Lane::scratch][29] = 'x'; break;
        }

    normalisePattern (p, randomBeatLength);
    return p;
}

std::string makeRandomLane (Lane lane, std::uint32_t seed, float density, int genre)
{
    // The genre's own idea of this lane first, from a few grooves...
    for (std::uint32_t attempt = 0; attempt < 12; ++attempt)
    {
        const auto groove = makeRandomPattern (seed * 31u + attempt * 7919u + (std::uint32_t) lane, density, genre);
        const auto& line = groove[lane];
        if (hits (line))
            return line;
    }

    // ...otherwise a light part that fits any house groove.
    Random r (seed + 99u);
    static const char* cells[numLanes][3] {
        { "X... .... X... ....", "X... ..x. ..x. x...", "X... ..x. X... x.x." },        // down strum
        { ".... ..x. .... ..x.", "..x. ..x. ..x. ..x.", ".x.x ...x .x.. .x.x" },        // up strum
        { "..X. .... ..X. ....", "..X. ..X. ..X. ..X.", "X..X ..X. .X.. X.X." },        // stab
        { "..x. ..x. ..x. ..x.", "..xx ..x. ..xx ..x.", "x.xx .x.x x.xx .x.x" },        // chank
        { "g... g... g... g...", ".g.g .g.g .g.g .g.g", "gggg gggg gggg gggg" },        // scratch
        { ".... .... .... ..X.", "...X .... .... ..x.", "...X .... ..x. ..X." },        // slide
        { "X... .... .... ....", "X... .... .... ....", "X... .... X... ...." },        // swell
        { "x... x... x... x...", "x.x. x.x. x.x. x.x.", "x.xx x.xx x.xx x.xx" },        // arpeggio
        { ".... .... .... x.x.", ".... ..x. .... .x.x", "..x. .x.. .xxx x..." },        // lick
        { "x... .... x... ....", "x..x ..x. x..x ..x.", "X.x. x.x. X.x. x.x." },        // octaves
        { "X... .... X... ....", "X... ..X. ..X. X...", "X.x. X.x. X.x. X.x." },        // power chord
        { "X... .... .... ....", "X... .... X... ....", "X... .... X..x ...." },        // root
    };
    const int choice = std::clamp ((int) (density * 2.99f) + (r.chance (0.3f) ? (r.chance (0.5f) ? -1 : 1) : 0), 0, 2);
    auto line = compact (cells[(int) lane][choice]);
    line += line;
    if (density > 0.5f && lane != Lane::swell && lane != Lane::root)
        line[(size_t) (16 + r.pick ({ 11, 13, 14, 15 }))] = 'g';
    return line;
}

GuitarPattern makeVariation (const GuitarPattern& source, std::uint32_t seed)
{
    Random r (seed * 2654435761u + 17u);
    GuitarPattern p = source;
    const int len = p.length();
    if (len < 16)
        return p;

    const int bar = len - 16;   // the last bar plays the turnaround
    auto at = [&p, bar] (Lane lane, int step) -> char& { return p[lane][(size_t) (bar + step)]; };
    auto clearChordsFrom = [&] (int step)
    {
        for (int l = 0; l < numLanes; ++l)
            if (isChordLane (l))
                for (int i = step; i < 16; ++i)
                    at ((Lane) l, i) = '.';
    };

    const int moves = 1 + r.nextInt (2);
    for (int m = 0; m < moves; ++m)
    {
        switch (r.nextInt (5))
        {
            case 0:   // a slide into the next bar
                clearChordsFrom (14);
                at (Lane::slide, 14) = 'X';
                break;
            case 1:   // a muted scratch run
                for (int i = 12; i < 16; ++i)
                    if (at (Lane::strumDown, i) == '.' && at (Lane::stab, i) == '.')
                        at (Lane::scratch, i) = i == 15 ? 'X' : 'x';
                break;
            case 2:   // an up-strum pick-up
                at (Lane::strumUp, 15) = 'x';
                break;
            case 3:   // a lick into the bar
                at (Lane::lick, 12) = 'x';
                at (Lane::lick, 14) = 'x';
                at (Lane::lick, 15) = 'x';
                break;
            default:  // a stop and a stab on the last 16th
                clearChordsFrom (12);
                at (Lane::stab, 15) = 'X';
                break;
        }
    }
    return p;
}

GuitarPattern makeSectionVariation (const GuitarPattern& source, std::uint32_t seed)
{
    Random r (seed * 747796405u + 2891336453u);
    GuitarPattern p = source;
    const int len = p.length();

    // The light parts move around a little (never off the beats): scratches, up-strums, chanks.
    bool moved = false;
    for (auto lane : { Lane::scratch, Lane::strumUp, Lane::chank, Lane::arp })
        if (hits (p[lane]) && r.chance (0.6f))
        {
            auto& line = p[lane];
            for (int bar = 0; bar < len / 16; ++bar)
            {
                const int from = bar * 16 + r.nextInt (16), to = bar * 16 + r.nextInt (16);
                if (from % 4 != 0 && to % 4 != 0)
                    std::swap (line[(size_t) from], line[(size_t) to]);
            }
            moved = true;
        }
    if (! moved)
        for (int bar = 0; bar < len / 16; ++bar)
            p[Lane::scratch][(size_t) (bar * 16 + r.pick ({ 3, 7, 11, 14 }))] = 'g';

    // A lick now and then.
    if (r.chance (0.4f))
        p[Lane::lick][(size_t) (r.nextInt (len / 16) * 16 + r.pick ({ 7, 10, 13 }))] = 'x';

    // And the turnaround of the last bar.
    return makeVariation (p, seed + 7);
}

GuitarPattern applySegmentOrder (const GuitarPattern& source, const std::array<int, numSegments>& order)
{
    const int len = source.length();
    if (len % numSegments != 0 || order == std::array<int, numSegments> { 0, 1, 2, 3 })
        return source;

    GuitarPattern p = source;
    const int segment = len / numSegments;
    for (int k = 0; k < numSegments; ++k)
    {
        const int from = std::clamp (order[(size_t) k], 0, numSegments - 1);
        for (int l = 0; l < numLanes; ++l)
            for (int i = 0; i < segment; ++i)
                p.lanes[(size_t) l][(size_t) (k * segment + i)] = source.lanes[(size_t) l][(size_t) (from * segment + i)];
    }
    return p;
}

void copySegment (GuitarPattern& into, const GuitarPattern& from, int segment)
{
    const int len = into.length();
    if (from.length() != len || len % numSegments != 0)
        return;
    const int size = len / numSegments;
    segment = std::clamp (segment, 0, numSegments - 1);
    for (int l = 0; l < numLanes; ++l)
        for (int i = segment * size; i < (segment + 1) * size; ++i)
            into.lanes[(size_t) l][(size_t) i] = from.lanes[(size_t) l][(size_t) i];
}

//==============================================================================
const char* fillStyleName (int style) noexcept
{
    static const char* names[] { "Strum build", "Octave run", "Scratch build", "Chank lift", "Drop out", "Slide up", "Mix",
                                 "Stab stutter", "Lick run", "Arpeggio run", "Bass walk", "Swell", "Custom" };
    return style >= 0 && style < numFillStyles ? names[style] : "";
}

int fillStartStep (FillStyle style) noexcept
{
    switch (style)
    {
        case FillStyle::strumBuild:
        case FillStyle::scratchBuild:
        case FillStyle::lickRun:
        case FillStyle::arpRun:
        case FillStyle::swellOut:  return 8;
        case FillStyle::custom:    return 0;   // from its first written step (see the engine)
        case FillStyle::octaveRun:
        case FillStyle::chankLift:
        case FillStyle::dropOut:
        case FillStyle::slideUp:
        case FillStyle::stabStutter:
        case FillStyle::bassWalk:
        case FillStyle::mix:
        default:                   return 12;
    }
}

const std::vector<ArrangementPart>& defaultArrangement()
{
    static const std::vector<ArrangementPart> parts {
        { Section::intro, 8 }, { Section::main, 16 }, { Section::breakdown, 8 },
        { Section::buildup, 4 }, { Section::drop, 16 }, { Section::outro, 8 },
    };
    return parts;
}

} // namespace fgg

