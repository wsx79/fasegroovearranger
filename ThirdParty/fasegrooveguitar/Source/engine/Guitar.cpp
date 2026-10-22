// Fase Groove Guitar - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "Guitar.h"

#include <algorithm>

namespace fgg
{

namespace
{
    int mod12 (int v) noexcept { return ((v % 12) + 12) % 12; }
}

const char* laneName (int lane) noexcept
{
    static const char* names[] { "Down strum", "Up strum", "Stab", "Chank", "Muted scratch", "Slide",
                                 "Swell", "Arpeggio", "Lick", "Octaves", "Power chord", "Root" };
    return lane >= 0 && lane < numLanes ? names[lane] : "";
}

const char* laneShortName (int lane) noexcept
{
    static const char* names[] { "DOWN", "UP", "STAB", "CHANK", "SCRATCH", "SLIDE",
                                 "SWELL", "ARP", "LICK", "OCTAVE", "POWER", "ROOT" };
    return lane >= 0 && lane < numLanes ? names[lane] : "";
}

bool isChordLane (int lane) noexcept
{
    switch ((Lane) lane)
    {
        case Lane::strumDown: case Lane::strumUp: case Lane::stab: case Lane::chank:
        case Lane::scratch: case Lane::slide: case Lane::swell:
            return true;
        default:
            return false;
    }
}

//==============================================================================
const char* chordTypeName (int type) noexcept
{
    static const char* names[] { "maj7", "m7", "7", "m9", "maj9", "9", "m11", "13", "7sus4", "6/9", "", "m", "m7b5", "dim", "aug" };
    return type >= 0 && type < numChordTypes ? names[type] : "";
}

unsigned chordTypeMask (int type) noexcept
{
    auto bits = [] (std::initializer_list<int> intervals)
    {
        unsigned m = 0;
        for (int i : intervals)
            m |= 1u << (unsigned) mod12 (i);
        return m;
    };
    switch ((ChordType) type)
    {
        case ChordType::maj7:    return bits ({ 0, 4, 7, 11 });
        case ChordType::dom7:    return bits ({ 0, 4, 7, 10 });
        case ChordType::m9:      return bits ({ 0, 3, 7, 10, 14 });
        case ChordType::maj9:    return bits ({ 0, 4, 7, 11, 14 });
        case ChordType::dom9:    return bits ({ 0, 4, 7, 10, 14 });
        case ChordType::m11:     return bits ({ 0, 3, 7, 10, 14, 17 });
        case ChordType::dom13:   return bits ({ 0, 4, 10, 14, 21 });
        case ChordType::sus4:    return bits ({ 0, 5, 7, 10 });
        case ChordType::sixNine: return bits ({ 0, 4, 7, 9, 14 });
        case ChordType::major:   return bits ({ 0, 4, 7 });
        case ChordType::minor:   return bits ({ 0, 3, 7 });
        case ChordType::halfDim: return bits ({ 0, 3, 6, 10 });
        case ChordType::dim:     return bits ({ 0, 3, 6 });
        case ChordType::aug:     return bits ({ 0, 4, 8 });
        case ChordType::m7:
        default:                 return bits ({ 0, 3, 7, 10 });
    }
}

const char* pitchClassName (int pitchClass) noexcept
{
    static const char* names[] { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    return names[mod12 (pitchClass)];
}

std::string chordName (const Chord& c)
{
    return std::string (pitchClassName (c.root)) + chordTypeName (c.type);
}

ChordTones tonesOf (const Chord& c) noexcept
{
    return { mod12 (c.root), chordTypeMask (c.type) };
}

namespace
{
    int popCount (unsigned v) noexcept
    {
        int n = 0;
        for (; v != 0; v &= v - 1)
            ++n;
        return n;
    }

    /** The chord a set of pitch classes makes: the root is the held note the others make the
        best known chord over (an inversion keeps its own root), the lowest one on a tie. */
    ChordTones chordOfPitchClasses (unsigned pitchClasses, int lowest) noexcept
    {
        ChordTones best { mod12 (lowest), 0 };
        int bestScore = -1000000;
        for (int r = 0; r < 12; ++r)
        {
            if (((pitchClasses >> (unsigned) r) & 1u) == 0)
                continue;
            unsigned mask = 0;
            for (int i = 0; i < 12; ++i)
                if ((pitchClasses >> (unsigned) mod12 (r + i)) & 1u)
                    mask |= 1u << (unsigned) i;
            int fit = -1000;
            for (int t = 0; t < numChordTypes; ++t)
            {
                const unsigned type = chordTypeMask (t);
                fit = std::max (fit, 3 * popCount (mask & type) - 3 * popCount (mask & ~type) - 2 * popCount (type & ~mask));
            }
            const int score = 2 * fit + (r == mod12 (lowest) ? 1 : 0);
            if (score > bestScore)
            {
                bestScore = score;
                best = { r, mask };
            }
        }
        return best;
    }
}

ChordTones tonesOfNotes (const std::vector<int>& notes) noexcept
{
    if (notes.empty())
        return { 0, 0 };
    unsigned pitchClasses = 0;
    for (int n : notes)
        pitchClasses |= 1u << (unsigned) mod12 (n);
    return chordOfPitchClasses (pitchClasses, *std::min_element (notes.begin(), notes.end()));
}

ChordTones tonesOfHeld (const std::array<bool, 128>& held, int& count) noexcept
{
    count = 0;
    int lowest = -1;
    unsigned pitchClasses = 0;
    for (int n = 0; n < 128; ++n)
    {
        if (! held[(size_t) n])
            continue;
        if (lowest < 0)
            lowest = n;
        pitchClasses |= 1u << (unsigned) mod12 (n);
        ++count;
    }
    if (lowest < 0)
        return { 0, 0 };
    return chordOfPitchClasses (pitchClasses, lowest);
}

//==============================================================================
namespace
{
    bool inChord (const ChordTones& c, int note) noexcept
    {
        return (c.mask >> (unsigned) mod12 (note - c.root)) & 1u;
    }

    bool has (const ChordTones& c, int interval) noexcept { return (c.mask >> (unsigned) interval) & 1u; }

    /** The root on the low E string: E2 (40) to Eb3 (51). */
    int bassRoot (const ChordTones& c) noexcept { return 40 + mod12 (c.root - 4); }

    /** The tones a shape can't leave out: the 3rd (the 4th of a sus chord), the 7th (or the 6th
        in its place), the flat 5th of a diminished chord, the sharp 5th of an augmented one. */
    unsigned essentialTones (const ChordTones& c) noexcept
    {
        unsigned e = 0;
        if (has (c, 3) || has (c, 4))
            e |= has (c, 4) ? 1u << 4u : 1u << 3u;
        else if (has (c, 5))
            e |= 1u << 5u;
        if (has (c, 10) || has (c, 11))
            e |= has (c, 10) ? 1u << 10u : 1u << 11u;
        else if (has (c, 9))
            e |= 1u << 9u;
        if (has (c, 6) && ! has (c, 7))
            e |= 1u << 6u;
        if (has (c, 8) && ! has (c, 7) && has (c, 4))
            e |= 1u << 8u;
        return e;
    }

    /** How much each tone tells of the chord: the 3rd and the 7th most, then the colours, the
        root, the 5th last. */
    double toneWeight (int interval) noexcept
    {
        switch (interval)
        {
            case 3: case 4:            return 10.0;
            case 10: case 11:          return 9.0;
            case 6: case 8:            return 7.0;
            case 2: case 5: case 9:    return 6.0;
            case 1:                    return 5.0;
            case 0:                    return 4.0;
            case 7:
            default:                   return 2.0;
        }
    }

    /** What a shape is looked for on: side by side strings, from `firstString`. */
    struct ShapeSpec
    {
        int firstString, strings;
        bool rootInBass;
        int minGap;        // semitones between two neighbouring strings, at least
        int lowest;        // no note below
        int highest;       // no note above
        int targetTop;     // where the top note likes to sit
        double stringBonus;
    };

    /** The best shape of a chord on some strings: every note a chord tone, low to high string by
        string, the fretting hand within four frets (a barre and three fingers), the tones that
        matter all there. Near `previous` when given. No allocation. */
    struct ShapeSearch
    {
        const ChordTones& chord;
        const ShapeSpec& spec;
        const NoteList* previous;
        int avoidTop;
        unsigned essentials;
        unsigned penalise = 0;   // chord tones a coloured voicing would rather leave out

        std::array<int, 6> fret {}, pitch {};
        int windowLow = 1;
        double bestScore = -1.0e18;
        NoteList best;

        void run()
        {
            for (windowLow = 1; windowLow <= 12; ++windowLow)
                step (0);
        }

        void step (int i)
        {
            if (i == spec.strings)
            {
                evaluate();
                return;
            }
            const int string = spec.firstString + i;
            for (int f = 0; f <= windowLow + 3; f = (f == 0 ? windowLow : f + 1))
            {
                const int n = openStrings[(size_t) string] + f;
                if (! inChord (chord, n) || n < spec.lowest || n > spec.highest)
                    continue;
                if (i > 0 && n - pitch[(size_t) i - 1] < spec.minGap)
                    continue;
                if (i == 0 && spec.rootInBass && mod12 (n - chord.root) != 0)
                    continue;
                fret[(size_t) i] = f;
                pitch[(size_t) i] = n;
                step (i + 1);
            }
        }

        void evaluate()
        {
            // The fretting hand: the lowest fret may be a barre, three fingers for the rest.
            int lowFret = 99, highFret = 0, fretted = 0;
            for (int i = 0; i < spec.strings; ++i)
                if (fret[(size_t) i] > 0)
                {
                    lowFret = std::min (lowFret, fret[(size_t) i]);
                    highFret = std::max (highFret, fret[(size_t) i]);
                    ++fretted;
                }
            if (fretted > 0)
            {
                if (highFret - lowFret > 3)
                    return;
                int fingers = 0;
                for (int i = 0; i < spec.strings; ++i)
                    fingers += fret[(size_t) i] > lowFret ? 1 : 0;
                if (fingers > 3)
                    return;
                if (lowFret != windowLow && windowLow > 1)
                    return;   // the same shape is met in its own window
            }
            const int top = pitch[(size_t) spec.strings - 1];
            if (top == avoidTop)
                return;

            unsigned covered = 0;
            int doubledThird = 0;
            for (int i = 0; i < spec.strings; ++i)
            {
                const int interval = mod12 (pitch[(size_t) i] - chord.root);
                if ((covered >> (unsigned) interval) & 1u)
                    doubledThird += (interval == 3 || interval == 4 || interval == 10 || interval == 11) ? 1 : 0;
                covered |= 1u << (unsigned) interval;
            }
            double score = spec.stringBonus * spec.strings - 30.0 * popCount (essentials & ~covered) - 3.0 * doubledThird;
            for (int i = 0; i < 12; ++i)
                if ((covered >> (unsigned) i) & 1u)
                    score += toneWeight (i);
            score -= 1.5 * (spec.strings - popCount (covered));
            score -= 0.35 * std::abs (top - spec.targetTop);
            if (penalise != 0)
                for (int i = 0; i < 12; ++i)
                    if (((penalise >> (unsigned) i) & 1u) && ((covered >> (unsigned) i) & 1u))
                        score -= 14.0;
            if (fretted > 0)
                score -= 0.3 * (double) (lowFret + highFret) * 0.5;

            if (previous != nullptr && ! previous->empty())
            {
                // The hand moves as little as it can: each note near one of the chord before.
                double move = 0.0;
                for (int i = 0; i < spec.strings; ++i)
                {
                    int nearest = 99;
                    for (int p : *previous)
                        nearest = std::min (nearest, std::abs (pitch[(size_t) i] - p));
                    move += nearest;
                }
                score -= 1.2 * move / spec.strings + 0.4 * std::abs (top - previous->back());
            }

            if (score > bestScore)
            {
                bestScore = score;
                best.clear();
                for (int i = 0; i < spec.strings; ++i)
                    best.add (pitch[(size_t) i]);
            }
        }
    };

    NoteList bestShape (const ChordTones& c, std::initializer_list<ShapeSpec> specs, const NoteList* previous, int avoidTop = -1,
                        unsigned dropEssentials = 0, unsigned penalise = 0)
    {
        NoteList best;
        double bestScore = -1.0e18;
        for (const auto& spec : specs)
        {
            ShapeSearch search { c, spec, previous, avoidTop, essentialTones (c) & ~dropEssentials, penalise };
            search.run();
            if (! search.best.empty() && search.bestScore > bestScore)
            {
                bestScore = search.bestScore;
                best = search.best;
            }
        }
        return best;
    }

    /** The pentatonic (or the five notes) that suits the chord, around the high strings. */
    NoteList licks (const ChordTones& c) noexcept
    {
        static constexpr int minorScale[] { 0, 3, 5, 7, 10 }, majorScale[] { 0, 2, 4, 7, 9 }, dominant[] { 0, 2, 4, 7, 10 },
                             halfDiminished[] { 0, 3, 5, 6, 10 }, diminished[] { 0, 2, 3, 6, 9 }, augmented[] { 0, 2, 4, 8, 10 },
                             suspended[] { 0, 2, 5, 7, 10 };
        const int* scale = majorScale;
        if (has (c, 3) && has (c, 6) && ! has (c, 7))   scale = has (c, 10) ? halfDiminished : diminished;
        else if (has (c, 4) && has (c, 8) && ! has (c, 7)) scale = augmented;
        else if (has (c, 3) && ! has (c, 4))            scale = minorScale;
        else if (has (c, 4) && has (c, 10))             scale = dominant;
        else if (! has (c, 3) && ! has (c, 4))          scale = suspended;
        NoteList notes;
        for (int p = 62; p <= 77; ++p)
            for (int i = 0; i < 5; ++i)
                if (mod12 (p - c.root) == scale[i])
                    notes.add (p);
        return notes;
    }

    /** A whole shape up or down by octaves, folded back into the guitar's range (E2 .. E6). */
    void moveByOctaves (NoteList& notes, int octave) noexcept
    {
        if (notes.empty())
            return;
        for (auto& n : notes)
            n += 12 * std::clamp (octave, -3, 3);
        while (notes.front() < 40)
            for (auto& n : notes)
                n += 12;
        while (notes.back() > 88 && notes.front() - 12 >= 40)
            for (auto& n : notes)
                n -= 12;
    }

    ChordVoicing voiceColoured (const ChordTones& c, const ChordVoicing* previous, bool roomForBand, GuitarColour colour)
    {
        ChordVoicing v;
        const int low = roomForBand ? 48 : 40;
        const unsigned extensions = (1u << 1) | (1u << 2) | (1u << 6) | (1u << 9) | (1u << 10) | (1u << 11);
        const NoteList* prevFull = previous != nullptr ? &previous->full : nullptr;
        const NoteList* prevStab = previous != nullptr ? &previous->stab : nullptr;
        const NoteList* prevFour = previous != nullptr ? &previous->four : nullptr;
        const int stabLow = roomForBand ? 60 : 55, stabTop = roomForBand ? 74 : 70;

        if (colour == GuitarColour::triad)
        {
            v.full = bestShape (c, { { 1, 3, true, 1, low, 76, 64, 3.0 }, { 2, 3, true, 1, low, 78, 67, 2.5 },
                                     { 0, 3, true, 1, low, 74, 62, 2.0 } }, prevFull, -1, extensions, extensions);
            v.stab = bestShape (c, { { 3, 3, false, 2, stabLow, 79, stabTop, 0.0 }, { 2, 3, false, 2, stabLow, 79, stabTop, 0.0 } },
                                prevStab, -1, extensions, extensions);
            v.four = bestShape (c, { { 2, 3, false, 1, roomForBand ? 55 : 50, 79, stabTop, 0.0 },
                                     { 1, 3, false, 1, roomForBand ? 55 : 50, 79, stabTop, 0.0 } },
                                prevFour, -1, extensions, extensions);
        }
        else if (colour == GuitarColour::funkGrip)
        {
            v.full = bestShape (c, { { 2, 3, true, 1, std::max (low, 48), 74, 64, 3.0 },
                                     { 3, 3, false, 1, 55, 78, 67, 2.5 },
                                     { 1, 3, true, 1, std::max (low, 45), 72, 62, 2.0 } }, prevFull);
            v.stab = bestShape (c, { { 4, 2, false, 1, 60, 84, 72, 2.0 }, { 3, 3, false, 1, 57, 80, 69, 1.5 } }, prevStab);
            v.four = bestShape (c, { { 3, 3, false, 1, 55, 80, 69, 1.0 }, { 2, 3, false, 1, 50, 76, 64, 1.0 } }, prevFour);
        }
        else
        {
            // Shell and drop-2: three or four strings, the 3rd and the 7th, the 5th left out when it can be.
            v.full = bestShape (c, { { 1, 4, true, 1, low, 76, 67, 2.5 }, { 2, 4, false, 1, std::max (low, 45), 79, 69, 2.5 },
                                     { 1, 3, true, 1, low, 74, 64, 2.0 } }, prevFull);
            v.stab = bestShape (c, { { 3, 3, false, 2, stabLow, 82, stabTop + 2, 0.0 }, { 2, 3, false, 2, stabLow, 79, stabTop, 0.0 } }, prevStab);
            v.four = bestShape (c, { { 2, 4, false, 1, roomForBand ? 55 : 50, 79, stabTop + 1, 0.0 },
                                     { 1, 3, true, 1, low, 76, 67, 1.0 } }, prevFour);
        }

        if (v.full.empty())
            v.full = bestShape (c, { { 2, 4, true, 1, low, 79, 66, 3.0 }, { 1, 3, true, 1, low, 76, 64, 2.0 } }, prevFull);
        if (v.stab.empty())
            v.stab = bestShape (c, { { 3, 3, false, 2, stabLow, 79, stabTop, 0.0 } }, prevStab);
        v.stabAlt = bestShape (c, { { 3, 3, false, 2, stabLow, 79, stabTop + 2, 0.0 }, { 2, 3, false, 2, stabLow, 79, stabTop, 0.0 } },
                               &v.stab, v.stab.empty() ? -1 : v.stab.back(),
                               colour == GuitarColour::triad ? extensions : 0, colour == GuitarColour::triad ? extensions : 0);
        if (v.stabAlt.empty())
            v.stabAlt = v.stab;
        if (v.four.empty())
            v.four = v.stab;
        return v;
    }
}

ChordVoicing voiceChord (const ChordTones& c, const ChordVoicing* previous, bool roomForBand, GuitarColour colour)
{
    if (colour != GuitarColour::classic)
        return voiceColoured (c, previous, roomForBand, colour);

    ChordVoicing v;
    const int low = roomForBand ? 48 : 40;
    // The strummed chord: six strings with the root on the low E, five from the A, four from the D.
    if (roomForBand)
        v.full = bestShape (c, { { 1, 5, true, 1, low, 79, 66, 3.0 }, { 2, 4, true, 1, low, 79, 67, 3.0 } },
                            previous != nullptr ? &previous->full : nullptr);
    else
        v.full = bestShape (c, { { 0, 6, true, 1, low, 79, 64, 3.0 }, { 1, 5, true, 1, low, 79, 65, 3.0 }, { 2, 4, true, 1, low, 79, 66, 3.0 } },
                            previous != nullptr ? &previous->full : nullptr);

    const int stabLow = roomForBand ? 60 : 55, stabTop = roomForBand ? 74 : 70;
    v.stab = bestShape (c, { { 3, 3, false, 2, stabLow, 79, stabTop, 0.0 }, { 2, 3, false, 2, stabLow, 79, stabTop, 0.0 } },
                        previous != nullptr ? &previous->stab : nullptr);
    v.stabAlt = bestShape (c, { { 3, 3, false, 2, stabLow, 79, stabTop, 0.0 }, { 2, 3, false, 2, stabLow, 79, stabTop, 0.0 } },
                           &v.stab, v.stab.empty() ? -1 : v.stab.back());
    if (v.stabAlt.empty())
        v.stabAlt = v.stab;
    v.four = bestShape (c, { { 2, 4, false, 2, roomForBand ? 55 : 50, 79, stabTop + 1, 0.0 }, { 1, 4, false, 2, roomForBand ? 55 : 50, 79, stabTop, 0.0 } },
                        previous != nullptr ? &previous->four : nullptr);
    return v;
}

void laneNotes (Lane lane, const ChordTones& chord, const ChordVoicing& v, int stepInBar, int octave,
                NoteList& out, bool roomForBand, bool alternative, int contour) noexcept
{
    out.clear();
    const int step = ((stepInBar % 16) + 16) % 16;
    const NoteList& stab = alternative ? v.stabAlt : v.stab;
    switch (lane)
    {
        case Lane::strumDown:
        case Lane::swell:
            out = v.full;
            break;
        case Lane::strumUp:   // the top four strings
            for (int i = std::max (0, v.full.size() - 4); i < v.full.size(); ++i)
                out.add (v.full[i]);
            break;
        case Lane::stab:
        case Lane::slide:
            out = stab;
            break;
        case Lane::scratch:
            out = v.four;
            break;
        case Lane::chank:     // the top two strings of the stab, short and choked
            for (int i = std::max (0, stab.size() - 2); i < stab.size(); ++i)
                out.add (stab[i]);
            break;
        case Lane::arp:       // up and down four strings
        {
            static constexpr int order[] { 0, 1, 2, 3, 2, 1 };
            if (! v.four.empty())
                out.add (v.four[std::min (v.four.size() - 1, order[step % 6])]);
            break;
        }
        case Lane::lick:      // a phrase on the pentatonic. 0 is the preset line; the others follow a style.
        {
            static constexpr int contours[][16] {
                { 0, 2, 3, 4, 3, 2, 0, 1, 2, 4, 5, 4, 3, 2, 1, 0 },   // preset
                { 0, 0, 2, 2, 4, 4, 3, 3, 5, 5, 4, 4, 2, 2, 1, 0 },   // gospel, on the 8ths
                { 0, 1, 0, 2, 1, 0, 3, 1, 0, 2, 1, 0, 2, 1, 0, 1 },   // funk, short and on the 16ths
                { 0, 0, 1, 0, 3, 2, 0, 4, 1, 0, 2, 0, 3, 1, 0, 2 },   // neo-soul, sparse
                { 0, 2, 0, 3, 1, 0, 2, 4, 0, 3, 1, 2, 0, 4, 2, 1 },   // afro
                { 4, 4, 4, 4, 2, 2, 2, 2, 4, 4, 3, 3, 2, 2, 1, 0 },   // dance, held
            };
            const int id = std::clamp (contour, 0, (int) std::size (contours) - 1);
            const auto scale = licks (chord);
            if (! scale.empty())
                out.add (scale[std::min (scale.size() - 1, contours[id][step] + 2)]);
            break;
        }
        case Lane::octave:
        {
            const int r = 52 + mod12 (chord.root - 4);
            out.add (r);
            out.add (r + 12);
            break;
        }
        case Lane::power:     // root, the chord's own 5th (flat on a diminished chord), octave
        {
            const int r = bassRoot (chord) + (roomForBand ? 12 : 0);
            const int fifth = has (chord, 7) ? 7 : has (chord, 6) ? 6 : has (chord, 8) ? 8 : 7;
            out.add (r);
            out.add (r + fifth);
            out.add (r + 12);
            break;
        }
        case Lane::root:
        default:
            out.add (v.full.empty() ? bassRoot (chord) : v.full.front());
            break;
    }
    if (out.empty())
        out.add (bassRoot (chord));
    moveByOctaves (out, octave);
}

std::vector<int> laneNotes (Lane lane, const ChordTones& chord, int stepInBar, int octave)
{
    NoteList notes;
    laneNotes (lane, chord, voiceChord (chord), stepInBar, octave, notes);
    return { notes.begin(), notes.end() };
}

//==============================================================================
//==============================================================================
const char* scaleName (int scale) noexcept
{
    static const char* names[numScales] { "Major (Ionian)", "Dorian", "Minor (Aeolian)", "Mixolydian", "Lydian", "Phrygian",
                                          "Harmonic Minor", "Melodic Minor", "Locrian", "Harmonic Major", "Lydian Dominant",
                                          "Mixolydian b6", "Phrygian Dominant" };
    return names[std::clamp (scale, 0, numScales - 1)];
}

const std::array<int, 7>& scaleIntervals (int scale) noexcept
{
    static const std::array<std::array<int, 7>, numScales> scales {{
        { 0, 2, 4, 5, 7, 9, 11 }, { 0, 2, 3, 5, 7, 9, 10 }, { 0, 2, 3, 5, 7, 8, 10 }, { 0, 2, 4, 5, 7, 9, 10 },
        { 0, 2, 4, 6, 7, 9, 11 }, { 0, 1, 3, 5, 7, 8, 10 }, { 0, 2, 3, 5, 7, 8, 11 }, { 0, 2, 3, 5, 7, 9, 11 },
        { 0, 1, 3, 5, 6, 8, 10 }, { 0, 2, 4, 5, 7, 8, 11 }, { 0, 2, 4, 6, 7, 9, 10 }, { 0, 2, 4, 5, 7, 8, 10 },
        { 0, 1, 4, 5, 7, 8, 10 } }};
    return scales[(size_t) std::clamp (scale, 0, numScales - 1)];
}

bool scaleIsMinor (int scale) noexcept { return scaleIntervals (scale)[2] == 3; }

const char* chordColourName (int colour) noexcept
{
    static const char* names[numChordColours] { "Triads", "7ths", "9ths" };
    return names[std::clamp (colour, 0, numChordColours - 1)];
}

namespace
{
    /** The type of a chord from its 3rd, 5th, 7th and 9th (semitones above the root, -1: none). */
    ChordType typeOf (int third, int fifth, int seventh, int ninth, int colour)
    {
        const bool sevenths = colour >= (int) ChordColour::sevenths, ninths = colour >= (int) ChordColour::ninths && ninth == 2;
        if (third == 4 && fifth == 8) return ChordType::aug;
        if (third == 3 && fifth == 6) return sevenths && seventh == 10 ? ChordType::halfDim : ChordType::dim;
        if (third == 3)
            return ! sevenths || seventh != 10 ? ChordType::minor : ninths ? ChordType::m9 : ChordType::m7;
        if (! sevenths) return ChordType::major;
        if (seventh == 11) return ninths ? ChordType::maj9 : ChordType::maj7;
        if (seventh == 10) return ninths ? ChordType::dom9 : ChordType::dom7;
        return ChordType::major;
    }
}

Chord diatonicChord (int key, int scale, int degree, int colour)
{
    const auto& s = scaleIntervals (scale);
    degree = ((degree % 7) + 7) % 7;
    auto above = [&] (int steps) { return mod12 (s[(size_t) ((degree + steps) % 7)] - s[(size_t) degree]); };
    return { mod12 (key + s[(size_t) degree]), (int) typeOf (above (2), above (4), above (6), above (1), colour) };
}

Chord codeChord (int key, int code, int colour)
{
    const int root = mod12 (key + code % 12), kind = std::clamp (code / 12, 0, 2);
    const bool sevenths = colour >= (int) ChordColour::sevenths, ninths = colour >= (int) ChordColour::ninths;
    ChordType t;
    if (kind == 1)      t = ! sevenths ? ChordType::minor : ninths ? ChordType::m9 : ChordType::m7;
    else if (kind == 2) t = ninths ? ChordType::dom9 : ChordType::dom7;   // a dominant keeps its 7th
    else                t = ! sevenths ? ChordType::major : ninths ? ChordType::maj9 : ChordType::maj7;
    return { root, (int) t };
}

std::vector<Chord> circleOfFifthsProgression (std::uint32_t seed, int key, int scale, int length, int colour)
{
    // Small, repeatable random numbers.
    auto next = [&seed] { seed = seed * 1664525u + 1013904223u; return (int) (seed >> 8); };
    length = std::clamp (length, 1, maxChords);
    // Each chord a fifth below the one before (three scale steps up), landing on the tonic or,
    // now and then, on the dominant (an open end that turns round).
    const int last = next() % 4 == 0 ? 4 : 0;
    const int first = ((last - 3 * (length - 1)) % 7 + 7 * length) % 7;
    std::vector<Chord> chords;
    for (int i = 0; i < length; ++i)
        chords.push_back (diatonicChord (key, scale, first + 3 * i, colour));
    // A secondary dominant: the chord before another one becomes its V7 (a fifth above it).
    if (length >= 3 && next() % 3 != 0)
    {
        const int at = next() % (length - 1);
        chords[(size_t) at] = { mod12 (chords[(size_t) at + 1].root + 7), (int) (colour >= (int) ChordColour::ninths ? ChordType::dom9 : ChordType::dom7) };
    }
    return chords;
}

float transitionScore (const Chord& a, const Chord& b) noexcept
{
    const auto ta = tonesOf (a), tb = tonesOf (b);
    int shared = 0, sizeB = 0;
    for (int pc = 0; pc < 12; ++pc)
    {
        const bool inA = (ta.mask >> (unsigned) mod12 (pc - ta.root)) & 1u, inB = (tb.mask >> (unsigned) mod12 (pc - tb.root)) & 1u;
        shared += inA && inB ? 1 : 0;
        sizeB += inB ? 1 : 0;
    }
    const int move = mod12 (b.root - a.root);
    const float root = move == 5 ? 1.0f : move == 7 ? 0.75f : move == 2 || move == 10 ? 0.6f : move == 3 || move == 4 || move == 8 || move == 9 ? 0.55f
                     : move == 0 ? 0.5f : move == 6 ? 0.1f : 0.35f;
    return std::clamp (0.55f * root + 0.45f * (float) shared / (float) std::max (1, sizeB) + 0.15f, 0.0f, 1.0f);
}

const std::vector<Progression>& progressions()
{
    // New progressions are always appended: a project stores its chords, the browser the index.
    using T = ChordType;
    static const std::vector<Progression> list {
        { "i7 - iv7 (minor vamp)", "Deep house", 1, {}, {}, { { 0, T::m7 }, { 5, T::m7 }, { 0, T::m7 }, { 5, T::m7 } } },
        { "im9 - bVImaj9 - bVII6/9 - im9", "Deep house", 2, {}, {}, { { 0, T::m9 }, { 8, T::maj9 }, { 10, T::sixNine }, { 0, T::m9 } } },
        { "ii9 - V13 - Imaj9 - vi9", "Soulful & Gospel", 0, {}, {}, { { 2, T::m9 }, { 7, T::dom13 }, { 0, T::maj9 }, { 9, T::m9 } } },
        { "IVmaj7 - V7 - iii7 - vi7", "Soulful & Gospel", 0, {}, {}, { { 5, T::maj7 }, { 7, T::dom7 }, { 4, T::m7 }, { 9, T::m7 } } },
        { "vi7 - IVmaj7 - I - V", "Soulful & Gospel", 0, {}, {}, { { 9, T::m7 }, { 5, T::maj7 }, { 0, T::major }, { 7, T::major } } },
        { "i7 - IV9 (Dorian disco)", "Jackin' & funky", 1, {}, {}, { { 0, T::m7 }, { 5, T::dom9 }, { 0, T::m7 }, { 5, T::dom9 } } },
        { "I9 (one-chord funk)", "Jackin' & funky", 3, {}, {}, { { 0, T::dom9 }, { 0, T::dom9 }, { 0, T::dom9 }, { 0, T::dom9 } } },
        { "i7 - i7 - bVII7 - bVII7", "Jackin' & funky", 2, {}, {}, { { 0, T::m7 }, { 0, T::m7 }, { 10, T::dom7 }, { 10, T::dom7 } } },
        { "im9 - bVIImaj9 - bVImaj9 - v7", "Garage & UK", 2, {}, {}, { { 0, T::m9 }, { 10, T::maj9 }, { 8, T::maj9 }, { 7, T::m7 } } },
        { "i7 - i7 - ii7 - i7 (modal)", "Afro & Latin", 1, {}, {}, { { 0, T::m7 }, { 0, T::m7 }, { 2, T::m7 }, { 0, T::m7 } } },
        { "vi - IV - I - V", "Disco & '90s", 0, {}, {}, { { 9, T::minor }, { 5, T::major }, { 0, T::major }, { 7, T::major } } },
        { "IVmaj9 - iii7 - ii9 - V13", "Neo soul & jazz", 0, {}, {}, { { 5, T::maj9 }, { 4, T::m7 }, { 2, T::m9 }, { 7, T::dom13 } } },
        { "Imaj7 - IVmaj7 - Imaj9 - IVmaj7", "Deep house", 0, {}, {}, { { 0, T::maj7 }, { 5, T::maj7 }, { 0, T::maj9 }, { 5, T::maj7 } } },
        { "i - bVI - bIII - bVII", "Disco & '90s", 2, {}, {}, { { 0, T::minor }, { 8, T::major }, { 3, T::major }, { 10, T::major } } },
        { "I7sus4 - I7 - I7sus4 - I9", "Jackin' & funky", 3, {}, {}, { { 0, T::sus4 }, { 0, T::dom7 }, { 0, T::sus4 }, { 0, T::dom9 } } },
        { "i7 - IV7 (jack bounce)", "Jackin' & funky", 1, {}, {}, { { 0, T::m7 }, { 5, T::dom7 }, { 0, T::m7 }, { 5, T::dom7 } } },
        { "im11 - im11 - bVIImaj9 - ivm9", "Afro & Latin", 1, {}, {}, { { 0, T::m11 }, { 0, T::m11 }, { 10, T::maj9 }, { 5, T::m9 } } },
        { "Imaj9 - vi9 - ii9 - V7sus4", "Soulful & Gospel", 0, {}, {}, { { 0, T::maj9 }, { 9, T::m9 }, { 2, T::m9 }, { 7, T::sus4 } } },
        // Fase Groove Piano's progressions (degrees of a scale, or codes from the circle of fifths).
        { "Dorian Vamp  i - IV", "Deep house", 1, { 0, 3 }, {  } },
        { "Deep Minor  i - v", "Deep house", 1, { 0, 4 }, {  } },
        { "Soulful Descent  IV - iii - ii - I", "Soulful & Gospel", 0, { 3, 2, 1, 0 }, {  } },
        { "Jazzy  ii - V", "Soulful & Gospel", 0, { 1, 4 }, {  } },
        { "Turnaround  I - vi - ii - V", "Soulful & Gospel", 0, { 0, 5, 1, 4 }, {  } },
        { "Royal Road  IV - V - iii - vi", "Soulful & Gospel", 0, { 3, 4, 2, 5 }, {  } },
        { "Minor Flow  i - VI - III - VII", "Deep house", 2, { 0, 5, 2, 6 }, {  } },
        { "Late Night  i - VII - VI - VII", "Deep house", 2, { 0, 6, 5, 6 }, {  } },
        { "Gospel Lift  IV - V - vi - I", "Soulful & Gospel", 0, { 3, 4, 5, 0 }, {  } },
        { "Mixo Groove  I - VII - IV - I", "Jackin' & funky", 3, { 0, 6, 3, 0 }, {  } },
        { "Lydian Float  I - II", "Modal & exotic", 4, { 0, 1 }, {  } },
        { "8-Bar Journey", "Soulful & Gospel", 0, { 0, 5, 3, 4, 2, 5, 1, 4 }, {  } },
        { "Circle of 5ths  vi - ii - V - I", "Circle of 5ths", 0, { 5, 1, 4, 0 }, {  } },
        { "Circle of 5ths  iii - vi - ii - V", "Circle of 5ths", 0, { 2, 5, 1, 4 }, {  } },
        { "Full Circle  I-IV-vii-iii-vi-ii-V-I", "Circle of 5ths", 0, { 0, 3, 6, 2, 5, 1, 4, 0 }, {  } },
        { "Minor Circle  i - iv - VII - III", "Circle of 5ths", 2, { 0, 3, 6, 2 }, {  } },
        { "Dorian Circle  i - iv - VII - III", "Circle of 5ths", 1, { 0, 3, 6, 2 }, {  } },
        { "Jackin' Minor  i - iv", "Jackin' & funky", 2, { 0, 3 }, {  } },
        { "Jackin' Groove  i - VII - VI - v", "Jackin' & funky", 2, { 0, 6, 5, 4 }, {  } },
        { "Soulful Gospel  I - iii - IV - V", "Soulful & Gospel", 0, { 0, 2, 3, 4 }, {  } },
        { "Deep Dorian  i - ii - IV - i", "Deep house", 1, { 0, 1, 3, 0 }, {  } },
        { "Funky Mixo  I - IV - VII - IV", "Jackin' & funky", 3, { 0, 3, 6, 3 }, {  } },
        { "Gospel Plagal  I - iv - V - I", "Soulful & Gospel", 9, { 0, 3, 4, 0 }, {  } },
        { "Lydian Dom Funk  I7#11 - II", "Modal & exotic", 10, { 0, 1 }, {  } },
        { "Soul Borrowed  I - iv - bVII - I", "Soulful & Gospel", 11, { 0, 3, 6, 0 }, {  } },
        { "Dark Deep  I - II (Phrygian dom.)", "Modal & exotic", 12, { 0, 1 }, {  } },
        { "Phrygian Deep  i - II - i - vii", "Modal & exotic", 5, { 0, 1, 0, 6 }, {  } },
        { "Deep House Classic  vi - IV - I - V", "Deep house", 0, { 5, 3, 0, 4 }, {  } },
        { "Jackin' Dorian  i - IV - i - v", "Jackin' & funky", 1, { 0, 3, 0, 4 }, {  } },
        { "Neo Soul  ii - iii - IV - V", "Soulful & Gospel", 0, { 1, 2, 3, 4 }, {  } },
        { "Jackin' Stab  i - III - IV", "Jackin' & funky", 1, { 0, 2, 3, 3 }, {  } },
        { "Jack Swing  i - v - VII - IV", "Jackin' & funky", 1, { 0, 4, 6, 3 }, {  } },
        { "Funky Dominant  I7 - IV7", "Jackin' & funky", 3, { 0, 3 }, {  } },
        { "Disco Jack  ii - V - I - vi", "Jackin' & funky", 0, { 1, 4, 0, 5 }, {  } },
        { "Ghetto Funk  i - iv - v - iv", "Jackin' & funky", 2, { 0, 3, 4, 3 }, {  } },
        { "Phrygian Jack  i - bII", "Jackin' & funky", 5, { 0, 1 }, {  } },
        { "Lydian Jack  I7 - bVII+", "Jackin' & funky", 10, { 0, 6, 0, 6 }, {  } },
        { "Chicago Bounce  i - VII - i - IV", "Jackin' & funky", 1, { 0, 6, 0, 3 }, {  } },
        { "Maj9 Drift  I - bIII - bVI - bVII", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 0, 3, 8, 10 } },
        { "Deep Planing  I - bVII - bVI - bVII", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 0, 10, 8, 10 } },
        { "Borrowed iv  I - IV - iv - I", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 0, 5, 17, 0 } },
        { "Minor Plagal  vi - IV - iv - I", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 21, 5, 17, 0 } },
        { "Backdoor  ii - bVII7 - I", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 14, 34, 0, 0 } },
        { "Dorian Lift  i - bIII - IV - i", "Chromatic & borrowed", 1, { 0, 0, 0, 0 }, { 12, 3, 5, 12 } },
        { "Neapolitan Deep  i - bII - i - v", "Chromatic & borrowed", 2, { 0, 0, 0, 0 }, { 12, 1, 12, 19 } },
        { "Minor Dominant  vi - III7 - vi - IV", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 21, 28, 21, 5 } },
        { "Floating Maj7  IV - V - iii - bIII", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 5, 7, 16, 3 } },
        { "Mediant Shift  I - iii - bIII - IV", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 0, 16, 3, 5 } },
        { "Lydian Turn  I - II - iv - I", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 0, 2, 17, 0 } },
        { "Picardy House  i - iv - V7 - I", "Chromatic & borrowed", 2, { 0, 0, 0, 0 }, { 12, 17, 31, 0 } },
        { "Deep Maj9 Walk  I - bVI - IV - bVII", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 0, 8, 5, 10 } },
        { "Gospel Flat VII  IV - bVII7 - I - vi", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 5, 34, 0, 21 } },
        { "Minor Glide  i - bVII - bVI - V7", "Chromatic & borrowed", 2, { 0, 0, 0, 0 }, { 12, 10, 8, 31 } },
        { "Secondary Soul  I - VI7 - ii - V7", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 0, 33, 14, 31 } },
        { "Soul Cycle  I - iii - vi - IV", "Soulful & Gospel", 0, { 0, 2, 5, 3 }, {  } },
        { "Sunday Gospel  IV - iii - vi - ii - V", "Soulful & Gospel", 0, { 3, 2, 5, 1, 4 }, {  } },
        { "Uplift  vi - V - IV - V", "Soulful & Gospel", 0, { 5, 4, 3, 4 }, {  } },
        { "Two-Five Lift  ii - V - iii - vi", "Soulful & Gospel", 0, { 1, 4, 2, 5 }, {  } },
        { "Gospel Walk-up  I - ii - iii - IV", "Soulful & Gospel", 0, { 0, 1, 2, 3 }, {  } },
        { "Classic Soul  I - IV - ii - V", "Soulful & Gospel", 0, { 0, 3, 1, 4 }, {  } },
        { "Sunrise  I - V - vi - IV", "Soulful & Gospel", 0, { 0, 4, 5, 3 }, {  } },
        { "Deep Soul  ii - V - I - IV", "Soulful & Gospel", 0, { 1, 4, 0, 3 }, {  } },
        { "Vocal Lift  IV - V - iii - vi - ii - V - I - I", "Soulful & Gospel", 0, { 3, 4, 2, 5, 1, 4, 0, 0 }, {  } },
        { "Church Turn  I - I7 - IV - iv", "Soulful & Gospel", 0, { 0, 0, 0, 0 }, { 0, 24, 5, 17 } },
        { "Gospel Secondary  I - III7 - vi - IV", "Soulful & Gospel", 0, { 0, 0, 0, 0 }, { 0, 28, 21, 5 } },
        { "Deep Minor Vamp  i - VI", "Deep house", 2, { 0, 5 }, {  } },
        { "Dorian Swing  i - IV - v - IV", "Deep house", 1, { 0, 3, 4, 3 }, {  } },
        { "Late Night Minor  i - v - VI - iv", "Deep house", 2, { 0, 4, 5, 3 }, {  } },
        { "Deep Float  ii - I", "Deep house", 0, { 1, 0 }, {  } },
        { "Deep 7ths  vi - iii - IV - I", "Deep house", 0, { 5, 2, 3, 0 }, {  } },
        { "Minor Two  i - ii", "Deep house", 1, { 0, 1 }, {  } },
        { "Midnight  iv - i - VII - i", "Deep house", 2, { 3, 0, 6, 0 }, {  } },
        { "Deep Soul Minor  i - iv - VII - VI", "Deep house", 2, { 0, 3, 6, 5 }, {  } },
        { "Dorian Night  i - III - IV - ii", "Deep house", 1, { 0, 2, 3, 1 }, {  } },
        { "Jack Vamp  I7 - IV7 - I7 - V7", "Jackin' & funky", 3, { 0, 0, 0, 0 }, { 24, 29, 24, 31 } },
        { "Funky Dorian  i - IV - VII - IV", "Jackin' & funky", 1, { 0, 3, 6, 3 }, {  } },
        { "Chicago Shuffle  I - vi - IV - V", "Jackin' & funky", 0, { 0, 5, 3, 4 }, {  } },
        { "Disco Climb  I - ii - iii - ii", "Jackin' & funky", 0, { 0, 1, 2, 1 }, {  } },
        { "Jackin' Two  i - VII", "Jackin' & funky", 2, { 0, 6 }, {  } },
        { "Ghetto Dorian  i - ii - i - IV", "Jackin' & funky", 1, { 0, 1, 0, 3 }, {  } },
        { "Funk Turn  I7 - bVII7 - IV7 - I7", "Jackin' & funky", 3, { 0, 0, 0, 0 }, { 24, 34, 29, 24 } },
        { "Long Circle  iii - vi - ii - V - I", "Circle of 5ths", 0, { 2, 5, 1, 4, 0 }, {  } },
        { "Minor Circle Down  i-iv-VII-III-VI-ii-v-i", "Circle of 5ths", 2, { 0, 3, 6, 2, 5, 1, 4, 0 }, {  } },
        { "Secondary Circle  III7 - VI7 - II7 - V7", "Circle of 5ths", 0, { 0, 0, 0, 0 }, { 28, 33, 26, 31 } },
        { "Soul Circle  vi - ii - V - I - IV", "Circle of 5ths", 0, { 5, 1, 4, 0, 3 }, {  } },
        { "Soul Minor iv  I - iii - IV - iv", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 0, 16, 5, 17 } },
        { "Flat Six Seven Lift  IV - bVI - bVII - I", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 5, 8, 10, 0 } },
        { "Tritone Soul  ii - bII7 - I", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 14, 25, 0, 0 } },
        { "Deep Chromatic  i - bVII - bVI - v", "Chromatic & borrowed", 2, { 0, 0, 0, 0 }, { 12, 10, 8, 19 } },
        { "Major Minor Swap  I - i - IV - iv", "Chromatic & borrowed", 0, { 0, 0, 0, 0 }, { 0, 12, 5, 17 } },
        { "Mixo Vamp  I - v - IV - I", "Modal & exotic", 3, { 0, 4, 3, 0 }, {  } },
        { "Lydian Glow  I - II - vii - I", "Modal & exotic", 4, { 0, 1, 6, 0 }, {  } },
        { "Phrygian Minor  i - bII - bIII - bII", "Modal & exotic", 5, { 0, 1, 2, 1 }, {  } },
        { "Minor Drama  i - iv - V7 - i", "Modal & exotic", 2, { 0, 0, 0, 0 }, { 12, 17, 31, 12 } },
        { "Dorian Climb  i - VII - ii - IV", "Deep house", 1, { 0, 6, 1, 3 }, {  } },
        { "Dorian Return  i - v - IV - i", "Deep house", 1, { 0, 4, 3, 0 }, {  } },
        { "Dorian Sway  i - VII - VII - IV", "Deep house", 1, { 0, 6, 6, 3 }, {  } },
        { "Dorian Turn  i - IV - ii - VII", "Deep house", 1, { 0, 3, 1, 6 }, {  } },
        { "Dorian Pull  i - ii - v", "Deep house", 1, { 0, 1, 4 }, {  } },
        { "Dorian Step Down  i - v - IV - III", "Deep house", 1, { 0, 4, 3, 2 }, {  } },
        { "Dorian Glide  i - VII - IV", "Deep house", 1, { 0, 6, 3 }, {  } },
        { "Dorian Lift  i - IV - ii - IV", "Deep house", 1, { 0, 3, 1, 3 }, {  } },
        { "Dorian Rise  i - ii - v - v", "Deep house", 1, { 0, 1, 4, 4 }, {  } },
        { "Dorian Walk  i - IV - ii - III", "Deep house", 1, { 0, 3, 1, 2 }, {  } },
        { "Dorian Bloom  i - VII - III - IV", "Deep house", 1, { 0, 6, 2, 3 }, {  } },
        { "Dorian Drift  i - III - VII - IV", "Deep house", 1, { 0, 2, 6, 3 }, {  } },
        { "Dorian Hold  i - VII - IV - IV", "Deep house", 1, { 0, 6, 3, 3 }, {  } },
        { "Dorian Stairs  i - ii - III - III", "Deep house", 1, { 0, 1, 2, 2 }, {  } },
        { "Dorian Arrival  III - IV - v - i", "Deep house", 1, { 2, 3, 4, 0 }, {  } },
        { "Dorian Fall  i - VII - v - IV", "Deep house", 1, { 0, 6, 4, 3 }, {  } },
        { "Dorian Echo  i - ii - IV - ii", "Deep house", 1, { 0, 1, 3, 1 }, {  } },
        { "Dorian Fold  IV - III - i", "Deep house", 1, { 3, 2, 0 }, {  } },
        // More for the guitar.
        { "Italo Anthem  vi - IV - V - iii", "Disco & '90s", 0, { 5, 3, 4, 2 }, {  } },
        { "90s Rave  i - VI - VII - i", "Disco & '90s", 2, { 0, 5, 6, 0 }, {  } },
        { "Piano Anthem  I - V - vi - iii - IV", "Disco & '90s", 0, { 0, 4, 5, 2, 3 }, {  } },
        { "Boogie Down  i - IV9", "Disco & '90s", 0, { 0, 0 }, { 12, 29 } },
        { "Disco Strut  ii - V - ii - V", "Disco & '90s", 0, { 1, 4, 1, 4 }, {  } },
        { "Mirrorball  I - bIII - IV - I", "Disco & '90s", 0, { 0, 0, 0, 0 }, { 0, 3, 5, 0 } },
        { "Funky Town  I7 - bVII - IV - I7", "Disco & '90s", 0, { 0, 0, 0, 0 }, { 24, 10, 5, 24 } },
        { "Classic 90s  I - IV - vi - V", "Disco & '90s", 0, { 0, 3, 5, 4 }, {  } },
        { "Euro Minor  i - III - VII - VI", "Disco & '90s", 2, { 0, 2, 6, 5 }, {  } },
        { "Garage Minor  i - VII - v - VI", "Garage & UK", 2, { 0, 6, 4, 5 }, {  } },
        { "Speed Garage  i - iv - i - VII", "Garage & UK", 2, { 0, 3, 0, 6 }, {  } },
        { "2-Step Soul  ii - iii - vi - V", "Garage & UK", 0, { 1, 2, 5, 4 }, {  } },
        { "UK Funky  i - v - iv - i", "Garage & UK", 2, { 0, 4, 3, 0 }, {  } },
        { "Garage Lift  IV - V - iii - vi - ii", "Garage & UK", 0, { 3, 4, 2, 5, 1 }, {  } },
        { "Afro Modal  i - VII - i - VII", "Afro & Latin", 1, { 0, 6, 0, 6 }, {  } },
        { "Afro Lift  i - ii - III - ii", "Afro & Latin", 1, { 0, 1, 2, 1 }, {  } },
        { "Latin Tumbao  i - iv - V - i", "Afro & Latin", 6, { 0, 3, 4, 0 }, {  } },
        { "Son Montuno  I - IV - V - IV", "Afro & Latin", 0, { 0, 3, 4, 3 }, {  } },
        { "Highlife  I - IV - I - V", "Afro & Latin", 0, { 0, 3, 0, 4 }, {  } },
        { "Afro Phrygian  I - bII", "Afro & Latin", 12, { 0, 1, 0, 1 }, {  } },
        { "Bossa Turn  ii - V - I - VI7", "Afro & Latin", 0, { 0, 0, 0, 0 }, { 14, 31, 0, 33 } },
        { "Neo Soul Slide  IV - iii - vi - V/vi", "Neo soul & jazz", 0, { 0, 0, 0, 0 }, { 5, 16, 21, 28 } },
        { "Rhodes Drift  I - iv - iii - bIII", "Neo soul & jazz", 0, { 0, 0, 0, 0 }, { 0, 17, 16, 3 } },
        { "Dilla Turn  ii - V - iii - VI7", "Neo soul & jazz", 0, { 0, 0, 0, 0 }, { 14, 31, 16, 33 } },
        { "Minor Two-Five  ii - V - i", "Neo soul & jazz", 6, { 1, 4, 0, 0 }, {  } },
        { "Tritone Turn  I - bII7", "Neo soul & jazz", 0, { 0, 0 }, { 0, 25 } },
        { "Soul Ballad  I - vi - IV - iv", "Neo soul & jazz", 0, { 0, 0, 0, 0 }, { 0, 21, 5, 17 } },
        { "Lydian Soul  I - II - iii - II", "Neo soul & jazz", 4, { 0, 1, 2, 1 }, {  } },
        { "Jazz Waltz  I - vi - ii - V - iii - VI7 - ii - V", "Neo soul & jazz", 0, { 0, 0, 0, 0, 0, 0, 0, 0 }, { 0, 21, 14, 31, 16, 33, 14, 31 } },
        { "Chill Sus  I7sus - IV - I7sus - bVII", "Deep house", 0, { 0, 0, 0, 0 }, { 24, 5, 24, 10 } },
        { "Deep Lydian  I - II - I - vii", "Deep house", 4, { 0, 1, 0, 6 }, {  } },
        { "Sunset  IV - I - V - vi", "Deep house", 0, { 3, 0, 4, 5 }, {  } },
        { "Moonlight  vi - iii - IV - V", "Deep house", 0, { 5, 2, 3, 4 }, {  } },
        { "Ambient Minor  i - VI - iv - v", "Deep house", 2, { 0, 5, 3, 4 }, {  } },
        { "Deep Mixo  I - v - VII - IV", "Deep house", 3, { 0, 4, 6, 3 }, {  } },
        { "Shout  IV - V - vi - V/vi", "Soulful & Gospel", 0, { 0, 0, 0, 0 }, { 5, 7, 21, 28 } },
        { "Praise  I - V/vi - vi - IV - V", "Soulful & Gospel", 0, { 0, 0, 0, 0, 0 }, { 0, 28, 21, 5, 7 } },
        { "Church Cadence  IV - iv - I - VI7 - ii - V - I", "Soulful & Gospel", 0, { 0, 0, 0, 0, 0, 0, 0 }, { 5, 17, 0, 33, 14, 31, 0 } },
        { "Gospel Descent  vi - V - IV - iii - ii - V - I", "Soulful & Gospel", 0, { 5, 4, 3, 2, 1, 4, 0 }, {  } },
        { "Circle 7 Down  I - IV - vii - iii - vi - ii - V", "Circle of 5ths", 0, { 0, 3, 6, 2, 5, 1, 4 }, {  } },
        { "Dominant Chain  I - VI7 - II7 - V7", "Circle of 5ths", 0, { 0, 0, 0, 0 }, { 0, 33, 26, 31 } },
        { "Minor Circle Five  i - iv - VII - III - VI", "Circle of 5ths", 2, { 0, 3, 6, 2, 5 }, {  } },
        { "Half Circle  ii - V - I", "Circle of 5ths", 0, { 1, 4, 0 }, {  } },
        { "Reverse Circle  I - V - ii - vi", "Circle of 5ths", 0, { 0, 4, 1, 5 }, {  } },
        // Dance stabs: minor vamps a power chord can sit on. Appended, so older indexes stay put.
        { "Power Minor  i - VII - VI - VII", "Dance stabs", 2, { 0, 6, 5, 6 }, {  } },
        { "Club Stab  i - VI - VII - i", "Dance stabs", 2, { 0, 5, 6, 0 }, {  } },
        { "One Chord Drive  i", "Dance stabs", 2, { 0, 0, 0, 0 }, {  } },
        { "Drop Lift  i - iv - VII - i", "Dance stabs", 2, { 0, 3, 6, 0 }, {  } },
        { "Minor Power  i - VII - i - VI", "Dance stabs", 2, { 0, 6, 0, 5 }, {  } },
    };
    return list;
}

const std::vector<const char*>& progressionCategories()
{
    static const std::vector<const char*> categories { "Soulful & Gospel", "Deep house", "Jackin' & funky", "Disco & '90s", "Garage & UK",
                                                       "Afro & Latin", "Neo soul & jazz", "Circle of 5ths", "Modal & exotic",
                                                       "Chromatic & borrowed", "Dance stabs" };
    return categories;
}

std::vector<Chord> progressionChords (int progression, int key, int colour)
{
    const auto& list = progressions();
    const auto& p = list[(size_t) std::clamp (progression, 0, (int) list.size() - 1)];
    std::vector<Chord> chords;
    if (! p.fixed.empty())
        for (const auto& [semitones, type] : p.fixed)
            chords.push_back ({ mod12 (key + semitones), (int) type });
    else if (! p.codes.empty())
        for (size_t i = 0; i < p.codes.size(); ++i)
        {
            // A code that is a chord of the scale is built from the scale (it takes its 7th).
            const int code = p.codes[i];
            int degree = -1;
            for (int d = 0; d < 7 && degree < 0; ++d)
            {
                const auto c = diatonicChord (0, p.scale, d, (int) ChordColour::sevenths);
                const auto t = (ChordType) c.type;
                const int kind = code / 12;
                const bool same = c.root == code % 12
                               && (kind == 1 ? (t == ChordType::m7 || t == ChordType::minor)
                                 : kind == 2 ? t == ChordType::dom7
                                             : (t == ChordType::maj7 || t == ChordType::dom7 || t == ChordType::major));
                if (same)
                    degree = d;
            }
            chords.push_back (degree >= 0 ? diatonicChord (key, p.scale, degree, colour) : codeChord (key, code, colour));
        }
    else
        for (int d : p.degrees)
            chords.push_back (diatonicChord (key, p.scale, d, colour));
    if ((int) chords.size() > maxChords)
        chords.resize ((size_t) maxChords);
    return chords;
}

//==============================================================================
void normalisePattern (GuitarPattern& p, int minimumLength)
{
    size_t length = (size_t) std::max (1, minimumLength);
    for (const auto& line : p.lanes)
        length = std::max (length, line.size());

    for (auto& line : p.lanes)
    {
        for (auto& c : line)
            if (! isHit (c))
                c = '.';

        if (line.empty())
            line.assign (length, '.');

        // A one-bar line in a two-bar groove plays twice.
        const std::string base = line;
        while (line.size() < length)
            line += base.substr (0, std::min (base.size(), length - line.size()));
    }
}

void fitPattern (GuitarPattern& p, int length)
{
    length = std::max (1, length);
    for (auto& line : p.lanes)
    {
        for (auto& c : line)
            if (! isHit (c))
                c = '.';
        if (line.empty())
            line.assign ((size_t) length, '.');
        const std::string base = line;
        while ((int) line.size() < length)
            line += base.substr (0, std::min (base.size(), (size_t) length - line.size()));
        line.resize ((size_t) length);
    }
}

namespace
{
    std::string compact (const char* s)
    {
        std::string out;
        for (; *s != 0; ++s)
            if (*s != ' ')
                out += *s;
        return out;
    }

    struct Line { Lane lane; const char* steps; };

    GuitarPattern groove (const char* name, const char* category, std::initializer_list<Line> lines)
    {
        GuitarPattern p;
        p.name = name;
        p.category = category;
        for (const auto& l : lines)
            p[l.lane] = compact (l.steps);
        normalisePattern (p);
        return p;
    }

    constexpr auto DN = Lane::strumDown, UP = Lane::strumUp, ST = Lane::stab, CK = Lane::chank, SC = Lane::scratch,
                   SL = Lane::slide, SW = Lane::swell, AR = Lane::arp, LK = Lane::lick, OC = Lane::octave,
                   PW = Lane::power, RT = Lane::root;

    // The muted scratch fills the 16ths no other chord hit plays (the engine leaves it out
    // where one does): a funk player's hand never stops.
    constexpr const char* allScratch = "gggg gggg gggg gggg";
    constexpr const char* offbeats   = "..X. ..X. ..X. ..X.";
}

const std::vector<GuitarPattern>& guitarPatterns()
{
    static const std::vector<GuitarPattern> patterns {
        // ---- Piano House: bright offbeat chords and an open strum ----
        groove ("Offbeat Chords", "Piano House",
                { { ST, offbeats }, { SC, "g... g... g... g.g." } }),
        groove ("House Strummer", "Piano House",
                { { DN, "X... ..x. X... x.x." }, { UP, "...x .x.. ...x ...x" }, { SC, ".g.. g... .g.. .g.." } }),
        groove ("Piano House Chank", "Piano House",
                { { CK, "..x. ..x. ..xx ..x." }, { ST, "X... .... .... ...." }, { RT, "X... .... X... ...." } }),
        groove ("Uplifting Chords", "Piano House",
                { { DN, "X... .... X... ...." }, { UP, "..x. ..x. ..x. .xx." }, { SC, ".... g... .... g..." } }),

        // ---- Soulful: warm strums and chanks under the vocal ----
        groove ("Soulful Sunday", "Soulful",
                { { DN, "X... ..x. .... x..." }, { UP, "...x .... .x.. ...x" }, { SC, "..g. g... g.g. ..g." } }),
        groove ("Soul Chank & Root", "Soulful",
                { { CK, "..x. ..x. ..x. ..x." }, { RT, "X... .... X..x ...." }, { SC, ".g.. .g.. .g.. .g.g" } }),
        groove ("Soulful Sixteenths", "Soulful",
                { { DN, "X.g. x.g. X.g. x.g." }, { UP, ".g.x .g.x .g.x .x.x" } }),
        groove ("Sunday Arp", "Soulful",
                { { AR, "x.xx x.xx x.xx x.xx" }, { SW, "X... .... .... ...." } }),

        // ---- Deep: dub chords that echo, swells and soft picking ----
        groove ("Deep Swell", "Deep",
                { { SW, "X... .... .... ...." }, { CK, ".... ..x. .... ..g." } }),
        groove ("Deep Chord Echo", "Deep",
                { { ST, "..X. .... ..x. ...g" }, { SC, ".... .... .... g..." } }),
        groove ("Deep Arp", "Deep",
                { { AR, "x.x. x.x. x.x. x.x." }, { SW, "X... .... .... .... .... .... .... ...." } }),
        groove ("Late Night Stab", "Deep",
                { { ST, "...X .... ..x. .... ...X .... .... ..x." }, { RT, "X... .... .... .... X... .... .... ...." } }),

        // ---- Gospel: big eighth-note strums and shouts ----
        groove ("Gospel Strum", "Gospel",
                { { DN, "X... X... X... X..." }, { UP, "..x. ..x. ..x. ..x." } }),
        groove ("Gospel Shout", "Gospel",
                { { ST, "X..x ..x. X..x ..x." }, { RT, "X... .... X... ...." } }),
        groove ("Church Arp", "Gospel",
                { { AR, "x.xx x.xx x.xx x.xx" }, { SW, "X... .... .... ...." } }),

        // ---- Jackin': raw stabs and chanks with the swing ----
        groove ("Jack Stab", "Jackin'",
                { { ST, offbeats }, { SC, allScratch } }),
        groove ("Chicago Chank", "Jackin'",
                { { CK, "x.xx .x.x x.xx .x.x" }, { ST, ".... .... .... ..X." } }),
        groove ("Jack Octaves", "Jackin'",
                { { OC, "x..x ..x. x..x ..x." }, { ST, "..X. .... ..X. ...." } }),

        // ---- Jackin' Piano: bouncy stabs ----
        groove ("Bouncy Stabs", "Jackin' Piano",
                { { ST, "X..X ..X. .X.. X.X." }, { SC, ".g.. g... g.g. .g.." } }),
        groove ("Jack Strum", "Jackin' Piano",
                { { DN, "X... X.x. X... X.x." }, { UP, "..x. ...x ..x. ...x" } }),
        groove ("Pumping Chords", "Jackin' Piano",
                { { ST, offbeats }, { PW, "X... X... X... X..." } }),

        // ---- Garage & UK: two-step chops and skanks ----
        groove ("Two-Step Chops", "Garage & UK",
                { { ST, "X... ..x. ...x ...." }, { SC, "..g. g... g... g.g." } }),
        groove ("Garage Skank", "Garage & UK",
                { { CK, "..x. ..x. .x.. ..x." }, { SL, "...X .... .... ...." } }),
        groove ("UK Swell & Stab", "Garage & UK",
                { { SW, "X... .... .... ...." }, { ST, ".... ..X. .... ...x" } }),

        // ---- Afro & Latin: highlife picking, montunos and clave chanks ----
        groove ("Afro Highlife", "Afro & Latin",
                { { AR, "x.x. xx.x x.x. xx.x" }, { RT, "X... .... X... ...." } }),
        groove ("Afro Chank", "Afro & Latin",
                { { CK, "x..x ..x. ..x. x..." }, { RT, "X... .... X... ...." }, { SC, ".g.. g..g .g.. .g.g" } }),
        groove ("Latin Montuno", "Afro & Latin",
                { { AR, "x.xx .xx. x.xx .xx." }, { OC, "X... .... .... ...." } }),

        // ---- Funky & Disco: the chicken scratch, octaves, sixteenths ----
        groove ("Nile Scratch", "Funky & Disco",
                { { DN, "X... ..x. ..x. x..." }, { UP, ".x.x ...x .x.. .x.x" }, { SC, "..g. g... g... ..g." } }),
        groove ("Disco Octaves", "Funky & Disco",
                { { OC, "X.x. x.x. X.x. x.x." }, { SC, ".g.g .g.g .g.g .g.g" } }),
        groove ("Funk Sixteenths", "Funky & Disco",
                { { DN, "X... .... X... ...." }, { UP, ".... ..x. .... ..x." }, { SC, allScratch } }),
        groove ("Disco Chank Line", "Funky & Disco",
                { { CK, "x.xx .xx. x.xx .x.x" }, { ST, ".... X... .... X..." } }),

        // ---- '90s Piano House: strummed anthems and power chords ----
        groove ("90s Anthem Strum", "'90s Piano House",
                { { DN, "X... ..x. ..x. X..." }, { UP, ".... x... .... ..x." }, { RT, "X... X... X... X..." } }),
        groove ("Rave Power", "'90s Piano House",
                { { PW, "X... ..X. ..X. X..." }, { ST, ".... .... .... ..x." } }),
        groove ("Italo Offbeat", "'90s Piano House",
                { { ST, offbeats }, { OC, "X... X... X... X..." } }),

        // ---- Neo Soul & Broken: lazy chords and licks ----
        groove ("Neo Soul Lazy", "Neo Soul & Broken",
                { { ST, "X... ...x ..x. ...." }, { LK, ".... ..x. .... .x.x" } }),
        groove ("Broken Strum", "Neo Soul & Broken",
                { { DN, "X... ..x. .... ...." }, { UP, ".... .... .x.. ...." }, { LK, ".... .... .... x.x." },
                  { SL, ".... .... .... .... X... ..x. .... ...." } }),
        groove ("Neo Arp", "Neo Soul & Broken",
                { { AR, "x.x. .x.x x.x. .x.x" }, { RT, "X... .... ..x. ...." } }),

        // ---- Chill & Pads: swells and soft picking ----
        groove ("Chill Swell", "Chill & Pads",
                { { SW, "X... .... .... .... X... .... .... ...." }, { LK, ".... .... ..x. .... .... ..x. .x.. x.x." } }),
        groove ("Chill Picking", "Chill & Pads",
                { { AR, "x.x. x.x. x.x. x.x." }, { SW, "X... .... .... ...." } }),
        groove ("Sunset Chords", "Chill & Pads",
                { { DN, "f... .... ..x. ...." }, { UP, ".... ..x. .... .x.." } }),

        // ---- Dance Stabs: big straight club hits ----
        groove ("Dance Power", "Dance Stabs",
                { { PW, "X.x. X.x. X.x. X.x." } }),
        groove ("Big Stabs", "Dance Stabs",
                { { ST, "X... ..X. ..X. X..." }, { SC, ".g.. g... g... .g.." } }),
        groove ("Offbeat Power", "Dance Stabs",
                { { PW, offbeats }, { RT, "X... X... X... X..." } }),
    };
    return patterns;
}

const std::vector<const char*>& grooveCategories()
{
    static const std::vector<const char*> categories { "Piano House", "Soulful", "Deep", "Gospel", "Jackin'", "Jackin' Piano", "Garage & UK",
                                                       "Afro & Latin", "Funky & Disco", "'90s Piano House", "Neo Soul & Broken",
                                                       "Chill & Pads", "Dance Stabs" };
    return categories;
}

int categoryIndex (const std::string& category) noexcept
{
    const auto& c = grooveCategories();
    for (size_t i = 0; i < c.size(); ++i)
        if (category == c[i])
            return (int) i;
    return -1;
}

float genreSwing (int category) noexcept
{
    // The same swing per genre as Fase Groove Drums, so the guitar sits in the drums' pocket.
    static const float swing[] { 0.45f, 0.2f, 0.5f, 0.3f, 0.8f, 0.6f, 0.75f, 0.35f, 0.3f, 0.45f, 0.9f, 0.3f, 0.1f };
    return category >= 0 && category < (int) std::size (swing) ? swing[category] : 0.2f;
}

const char* genreProgressionCategory (int category) noexcept
{
    // Funky stays in the funk list (not Italo / '90s pop). Dance stabs stay on minor power vamps.
    static const char* categories[] { "Soulful & Gospel", "Soulful & Gospel", "Deep house", "Soulful & Gospel", "Jackin' & funky",
                                      "Jackin' & funky", "Garage & UK", "Afro & Latin", "Jackin' & funky", "Disco & '90s",
                                      "Neo soul & jazz", "Deep house", "Dance stabs" };
    return categories[std::clamp (category, 0, 12)];
}

GuitarColour guitarColourFor (int genre) noexcept
{
    switch (genre)
    {
        case 3: case 10:                 return GuitarColour::shell;      // gospel, neo-soul
        case 0: case 5: case 9: case 12: return GuitarColour::triad;       // piano house, jackin' piano, '90s, dance
        case 4: case 8:                  return GuitarColour::funkGrip;   // jackin', funky
        default:                         return GuitarColour::classic;
    }
}

int lickContourFor (int genre) noexcept
{
    switch (genre)
    {
        case 3:          return 1;   // gospel
        case 4: case 8:  return 2;   // jackin', funky
        case 10: case 11: return 3;  // neo-soul, chill
        case 7:          return 4;   // afro
        case 12:         return 5;   // dance
        default:         return 0;
    }
}

int genreProgression (int category) noexcept
{
    static const int progression[] { 4, 2, 1, 3, 7, 15, 8, 9, 5, 10, 11, 12, 13 };
    return category >= 0 && category < (int) std::size (progression) ? progression[category] : 0;
}

int randomPatternIndex() noexcept
{
    return (int) guitarPatterns().size();
}

int userPatternIndex() noexcept
{
    return randomPatternIndex() + 1;
}

} // namespace fgg
