// Shared guitar/lead scale table and chord spelling. Included inside each engine namespace.
// Not part of the locked family module: the two engines own Chord, ChordType and mod12.
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
        if (third == 3 && fifth == 6)
            return sevenths && seventh == 9 ? ChordType::dim7 : sevenths && seventh == 10 ? ChordType::halfDim : ChordType::dim;
        if (third == 3)
        {
            if (sevenths && seventh == 11) return ChordType::mMaj7;
            return ! sevenths || seventh != 10 ? ChordType::minor : ninths ? ChordType::m9 : ChordType::m7;
        }
        if (! sevenths) return ChordType::major;
        if (seventh == 11) return ninths ? ChordType::maj9 : ChordType::maj7;
        if (seventh == 10) return ninth == 1 ? ChordType::dom7b9 : ninths ? ChordType::dom9 : ChordType::dom7;
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

// Arranger progressions do not call this; the guitar and lead plugins do.
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
