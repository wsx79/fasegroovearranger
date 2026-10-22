// Fase Groove Guitar - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

// The guitar: the lanes of the grid (the ways a house guitarist plays a chord: strums,
// stabs, chanks, muted scratches, arpeggios, licks, octaves...), the chords and how each lane
// voices them on the neck, and the groove format shared by the presets, the random grooves
// and the user pattern. Pure C++ (no JUCE).

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace fgg
{

constexpr double stepLength = 0.25;   // a 16th note, in quarter notes

//==============================================================================
/** One row of the grid: one way of playing the chord of the bar. */
enum class Lane { strumDown = 0, strumUp, stab, chank, scratch, slide, swell, arp, lick, octave, power, root };
constexpr int numLanes = 12;

const char* laneName (int lane) noexcept;        // "Down strum", "Muted scratch" ...
const char* laneShortName (int lane) noexcept;   // "DOWN", "SCRATCH" ... (the grid's row labels)

/** The lanes that play the whole chord (or most of it) at once: a strum, a stab... A guitar has
    six strings: a new chord hit stops the chord still ringing. */
bool isChordLane (int lane) noexcept;

//==============================================================================
/** Chord qualities, as the chord slots show them. */
enum class ChordType { maj7 = 0, m7, dom7, m9, maj9, dom9, m11, dom13, sus4, sixNine, major, minor,
                       halfDim, dim, aug };   // (new types are appended: saved projects keep theirs)
constexpr int numChordTypes = 15;
const char* chordTypeName (int type) noexcept;   // "maj7", "m7", "7" ... ("" for a major triad)
/** The intervals of a chord type, as a bit mask over the 12 semitones above the root. */
unsigned chordTypeMask (int type) noexcept;

const char* pitchClassName (int pitchClass) noexcept;   // C, C#, D, Eb ...

struct Chord
{
    int root = 9;                          // pitch class: 0 = C ... 9 = A
    int type = (int) ChordType::m7;
    bool operator== (const Chord& o) const noexcept { return root == o.root && type == o.type; }
    bool operator!= (const Chord& o) const noexcept { return ! (*this == o); }
};
std::string chordName (const Chord&);           // "Am7", "Fmaj9", "C"

/** Chord tones: a root and the intervals above it (a bit mask, bit 0 = the root). A chord from
    the slots, or the notes held on the MIDI input. */
struct ChordTones
{
    int root = 9;
    unsigned mask = 0x489;                 // root, minor 3rd, 5th, minor 7th
    bool operator== (const ChordTones& o) const noexcept { return root == o.root && mask == o.mask; }
};
ChordTones tonesOf (const Chord&) noexcept;
/** The chord a set of held notes makes: the root is the note the others make the best known chord
    over (an inversion keeps its own root), the lowest one when nothing fits. mask == 0: nothing held. */
ChordTones tonesOfNotes (const std::vector<int>& notes) noexcept;
/** The same from the 128 MIDI notes, true where held: no allocation (for the audio thread).
    `count` gets how many notes are held. */
ChordTones tonesOfHeld (const std::array<bool, 128>& held, int& count) noexcept;

/** A few MIDI notes, lowest first, without allocating (for the audio thread). */
struct NoteList
{
    std::array<int, 8> notes {};
    int count = 0;

    void add (int n) noexcept           { if (count < (int) notes.size()) notes[(size_t) count++] = n; }
    void clear() noexcept               { count = 0; }
    int size() const noexcept           { return count; }
    bool empty() const noexcept         { return count == 0; }
    int front() const noexcept          { return notes[0]; }
    int back() const noexcept           { return notes[(size_t) std::max (0, count - 1)]; }
    int operator[] (int i) const noexcept { return notes[(size_t) i]; }
    int* begin() noexcept               { return notes.data(); }
    int* end() noexcept                 { return notes.data() + count; }
    const int* begin() const noexcept   { return notes.data(); }
    const int* end() const noexcept     { return notes.data() + count; }
    bool operator== (const NoteList& o) const noexcept { return count == o.count && std::equal (begin(), end(), o.begin()); }
};

/** The open strings of a guitar in standard tuning, low to high. */
constexpr std::array<int, 6> openStrings { 40, 45, 50, 55, 59, 64 };

/** Where a chord sits on the neck: real shapes, one note per string, the fretting hand never
    stretching over more than four frets. Each one string by string, low to high. */
struct ChordVoicing
{
    NoteList full;      // the strummed chord: four to six strings, the root on the bottom
    NoteList stab;      // three strings high up: stabs and slides (a chank plays its top two)
    NoteList four;      // four strings high up: the muted scratch and the arpeggio
    NoteList stabAlt;   // the stab with another note on top (a bar of the phrase answers)
};

/** How a genre voices a chord. `classic` is the preset sound (a full shape, 3rd and 7th in).
    `shell` is a 3- or 4-note drop-2 (neo-soul, gospel). `triad` leaves the 7ths and 9ths out
    (house, dance). `funkGrip` is three strings, small and high (funk, jackin'). */
enum class GuitarColour { classic = 0, shell, triad, funkGrip };
constexpr int numGuitarColours = 4;
/** The voicing a genre (grooveCategories() index) uses for a random groove. Presets stay classic. */
GuitarColour guitarColourFor (int genre) noexcept;
/** Lick phrase of a genre: 0 is the preset contour. */
int lickContourFor (int genre) noexcept;

/** The shapes of a chord. With `previous` (the chord before), the hand moves as little as it
    can: the shapes slide along the neck and keep the notes the two chords share. With
    `roomForBand` the strum leaves the low strings to the bass and the stabs sit higher, above
    a piano's left hand. `colour` defaults to the preset voicing. */
ChordVoicing voiceChord (const ChordTones& chord, const ChordVoicing* previous = nullptr, bool roomForBand = false,
                         GuitarColour colour = GuitarColour::classic);

/** The MIDI notes a lane plays over a chord, lowest first, from its shapes (between E2 = 40 and
    E6 = 88): a down strum the full chord, an up strum its top four strings, a stab three high
    strings, a chank two, octaves two notes an octave apart... An arpeggio and a lick play one
    note, chosen by the step (0..15) in the bar. `octave` moves it all up or down (folded back
    into the guitar's range). `alternative`: the stab with its other top note. `contour` picks
    the lick phrase (0: the preset line). */
void laneNotes (Lane lane, const ChordTones& chord, const ChordVoicing& voicing, int stepInBar, int octave,
                NoteList& out, bool roomForBand = false, bool alternative = false, int contour = 0) noexcept;

/** The same, from the chord's own shapes (no chord before it). */
std::vector<int> laneNotes (Lane lane, const ChordTones& chord, int stepInBar, int octave = 0);

//==============================================================================
/** The scales of Fase Groove Piano (the same order): Major, Dorian, Minor, Mixolydian, Lydian,
    Phrygian, Harmonic minor, Melodic minor, Locrian, Harmonic major, Lydian dominant,
    Mixolydian b6, Phrygian dominant. */
constexpr int numScales = 13;
const char* scaleName (int scale) noexcept;
const std::array<int, 7>& scaleIntervals (int scale) noexcept;
bool scaleIsMinor (int scale) noexcept;

/** How rich the chords built on a scale are: triads, 7ths or 9ths (a 9th only where it is a
    whole tone above the root, as a keyboard player tidies them up). */
enum class ChordColour { triads = 0, sevenths, ninths };
constexpr int numChordColours = 3;
const char* chordColourName (int colour) noexcept;

/** The chord on a degree (0..6) of a scale in a key. */
Chord diatonicChord (int key, int scale, int degree, int colour);

/** A chord outside the scale, as Fase Groove Piano stores it: semitones above the key + 12 for a
    minor chord, + 24 for a dominant one. */
Chord codeChord (int key, int code, int colour);

/** The most chords a progression has: one per bar, round and round. */
constexpr int maxChords = 8;

/** Chord progressions, by category: degrees of a scale (as Fase Groove Piano's), chords from the
    circle of fifths outside it (codes), or chords set one by one (fixed: semitones above the key
    and a type). */
struct Progression
{
    const char* name;
    const char* category;
    int scale = 0;
    std::vector<int> degrees {};
    std::vector<int> codes {};
    std::vector<std::pair<int, ChordType>> fixed {};
};
const std::vector<Progression>& progressions();
/** The progression browser's headings, in display order. */
const std::vector<const char*>& progressionCategories();
/** The chords of a progression in a key (1 to 8 of them). */
std::vector<Chord> progressionChords (int progression, int key, int colour = (int) ChordColour::ninths);

/** A new progression that walks the circle of fifths in a scale (each chord a fifth below the one
    before, landing on the tonic or the dominant), now and then with a secondary dominant on the
    way. Same seed = same progression. */
std::vector<Chord> circleOfFifthsProgression (std::uint32_t seed, int key, int scale, int length, int colour);

/** How well chord b sounds after chord a, 0 (clashes) to 1: shared notes and the root's move
    (down a fifth is the strongest). */
float transitionScore (const Chord& a, const Chord& b) noexcept;

//==============================================================================
/** A guitar groove: one line per lane, one character per 16th step:
      'X' accent   'x' hit   'g' ghost (very soft)   'f' rake (a slower strum, every string heard)   '.' rest
    All lines have the same length (16 = one bar, 32 = two bars). */
struct GuitarPattern
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
void normalisePattern (GuitarPattern&, int minimumLength = 16);

//==============================================================================
/** The preset grooves, grouped by genre (the same 13 genres as Fase Groove Piano and Drums). */
const std::vector<GuitarPattern>& guitarPatterns();

/** Genre headings of the groove menu, in display order. */
const std::vector<const char*>& grooveCategories();
int categoryIndex (const std::string& category) noexcept;   // -1 when unknown

/** Swing that suits a genre (0 = straight, 1 = full triplet shuffle). */
float genreSwing (int category) noexcept;

/** The progression that suits a genre (an index into progressions()). */
int genreProgression (int category) noexcept;
/** The progression category whose chords suit a genre (the dice pick there). */
const char* genreProgressionCategory (int category) noexcept;

/** Index of the "Random groove" entry, right after the preset grooves. */
int randomPatternIndex() noexcept;

/** Index of the "User pattern" entry (editable grid), after "Random groove". */
int userPatternIndex() noexcept;

constexpr int userPatternLength = 64;   // the user pattern: up to four bars of 16ths
constexpr int randomBeatLength = 32;    // a random groove: two bars

/** Every lane exactly `length` steps: longer lines are cut, shorter ones repeated. */
void fitPattern (GuitarPattern&, int length);

} // namespace fgg
