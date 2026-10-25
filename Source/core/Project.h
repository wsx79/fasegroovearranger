// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "VolumeAutomation.h"

namespace sgk {struct GrooveSettings;struct GroovePattern;}
namespace fga {
constexpr int leadTrack = 17;
constexpr int legacyTrackCount = 17;
constexpr int trackCount = 18; // generated tracks, stable IDs
constexpr int maxTracks = 32;
inline bool isManualTrack(int track) { return track >= trackCount && track < maxTracks; }
inline bool isDrumTrack(int track) { return track >= 4 && track < legacyTrackCount; }
inline bool isHarmonicTrack(int track) { return track >= 0 && (track < 4 || track == leadTrack); }
inline int instrumentFamily(int track) { return isDrumTrack(track) ? 4 : track == leadTrack ? 5 : isManualTrack(track) ? 0 : track; }
enum class Section { main, intro, breakdown, buildup, drop, outro };
enum class Scope { song, track, section, clip };
struct Note {
    double start = 0, length = .25;
    int pitch = 60, velocity = 96, lane = 0;
    bool operator==(const Note& o) const { return start == o.start && length == o.length && pitch == o.pitch && velocity == o.velocity && lane == o.lane; }
};
struct Clip {
    std::vector<Note> notes;
    std::uint32_t seed = 1;
    bool locked = false;
    // A zero span follows the section; explicit spans and loop periods use beats.
    // Preserve the source across shortening so extending can recover its notes.
    double span = 0, loopLength = 0;
    int role = -1;
    double position = 0; // Absolute beat when the track has an independent timeline.
    std::vector<Note> loopSource;
    // Persistent musical take, separate from performance timing. Zero loads legacy motifs.
    std::uint32_t motifSeed = 0;
    bool operator==(const Clip& o) const { return notes == o.notes && motifSeed == o.motifSeed && seed == o.seed && locked == o.locked && span == o.span && loopLength == o.loopLength && loopSource == o.loopSource && position == o.position && role == o.role; }
};
struct SongSection { Section type = Section::main; int bars = 8; std::string name = "Main"; };
// Stable storage IDs preserve existing projects and plugin assignments. The UI and
// MIDI file use musical order through trackAtRow().
int trackAtRow(int row);
int rowForTrack(int track);
struct EngineOptions {
    std::uint32_t riffSeed=0,humanizeSeed=0;
    int pumpRate=0,pumpSource=0;
    bool pedalCC=false;
    std::array<int,8> inversions {};
    std::array<int,4> velocityOrder {0,1,2,3};
    std::array<bool,4> segmentLocked {};
    std::array<char,32> userRight {},userLeft {};
    std::array<unsigned char,32> userVelRight {},userVelLeft {};
    int pattern = -1, velocity = 100, grooveBars = 2, fillEvery = 8, fillStyle = -1, variationEvery = 8;
    double swing = -1, humanize = -1, density = -1, gate = .55, strum = .15, accent = .85, tone = .5;
    double reverb = .12, chorus = 0, pump = 0, pumpRelease = .2;
    int centre = 64, movement = 1, strumDirection = 0, topLine = 0, approach = 0, evolveBars = 0, humanizeStyle = 0, bassFeel = 0;
    int rangeLow = 24, rangeHigh = 108, midiNote = -1, globalOctave = 0;
    bool octaveTop = false, realism = true, letRing = false, roomForBand = true, pushChanges = true, afterFill = true, genreFeel = true, bassSpace = true;
    std::array<int,4> segments {0,1,2,3};
    std::array<double,12> laneVolume {1,1,1,1,1,1,1,1,1,1,1,1};
    std::array<double,12> laneLevel {1,1,1,1,1,1,1,1,1,1,1,1};
    std::array<double,12> laneSwing {1,1,1,1,1,1,1,1,1,1,1,1};
    std::array<double,12> laneShift {};
    std::array<int,12> laneOctave {};
    std::array<bool,12> laneMute {};
};
struct Track {
    std::string name;
    int kind = 0; // 0 piano, 1 bass, 2 strings, 3 guitar, 4..16 individual drums, 17 lead, 18..31 handwritten MIDI
    double gain = .75, pan = 0;
    bool mute = false, solo = false, locked = false;
    int sound = 0, sampleRoot = 60;
    std::string instrument = "internal", instrumentPath, instrumentId, instrumentState;
    std::string effectPath, effectId, effectState;
    VolumeAutomation volume;
    VolumeAutomation panAutomation;            // 0..1 (see panFromAutomation); replaces `pan` while active
    std::vector<ParameterAutomation> parameters; // hosted plugin parameters, at most maxParameterLanes
    EngineOptions engine;
    bool freeTimeline = false;
    std::vector<Clip> clips;
};
struct Settings {
    int genre = 1, key = 9, scale = 1, extension = 2, voicing = 0;
    double bpm = 122, swing = .2, density = .55, humanize = .2;
    int bassMode = 3, stringsMode = 1;
    std::uint32_t seed = 1979;
    bool guitarLead = false;
    int chordSteps = 16;
    int chordCount = 4;
    std::array<int, 8> degrees {0, 3, 5, 4, 0, 3, 5, 4};
    std::array<int, 8> freeChords {-1,-1,-1,-1,-1,-1,-1,-1};
    // Regenerating (song, track, section, clip) also writes musical automation for that scope.
    // Off in projects saved before it existed; new projects start with it on.
    bool randomAutomation = false;
};
struct Project {
    std::string name = "Untitled groove";
    Settings settings;
    std::vector<SongSection> sections;
    std::vector<Track> tracks;
    double masterGain = .8;
    VolumeAutomation masterAutomation; // gain multiplier on the master, as a track's volume
    // Every automation envelope with its value ceiling (for edits that move or cut time).
    std::vector<std::pair<VolumeAutomation*, double>> envelopes();
    bool loop = false;
    double loopStart = 0, loopEnd = 0;
    // Loop locators. An empty region (start == end) means no loop region at all.
    bool hasLoopRegion() const { return loopEnd - loopStart >= 1. / 960.; }
    bool loopMatches(double from, double to) const;
    void setLoopRegion(double from, double to);   // sets and enables the loop
    bool toggleLoopRegion(double from, double to); // same active region: clears it; returns the new state
    bool toggleLoop();                             // no region: loops the whole song
    void clearLoop();
    double sectionStart(int index) const;
    double length() const;
    double clipLength(int track, int section) const;
    double clipStart(int track, int clip) const;
    int sectionAtBeat(double beat) const;
    int clipAtBeat(int track, double beat) const;
    int detachTrack(int track, int selected = -1);
    int splitClip(int track, int clip, double beat);
    int placeClip(int track, Clip clip, double beat);
    int moveClip(int track, int clip, int targetTrack, double beat, bool copy);
    bool deleteClip(int track, int clip);
    void resizeGuide(int section, int bars);
    void insertGuide(int section, SongSection);
    void removeGuide(int section);
    void moveGuide(int from, int to);
    void normalise();
    bool deleteMidiTrack(int track); // generated instrument tracks cannot be removed
    int addMidiTrack(); // returns its stable routing ID, or -1 at capacity
    void insertSection(int index, SongSection section, int copyFrom = -1);
    bool duplicateSection(int index);
    void removeSection(int index);
    void moveSection(int from, int to);
    void resizeSection(int index, int bars);
    bool resizeClipLoop(int track, int section, double beats);
    bool resizeSectionLoop(int index, int bars);
};
const char* sectionName(Section);
std::vector<std::string> genres();
// Automatic club-arrangement policy. These are curated working ranges, not
// universal genre definitions; see docs/CLUB_ARRANGEMENT.md for the sources.
struct GenreProfile {
    int minBpm, maxBpm, preferredBpm, mixingBars;
    bool rollingBuild;
};
const GenreProfile& genreProfile(int genre);
Project newProject(int genre = 1);
void applyGenre(Project&, int genre);
void arrangeSong(Project&);
void chooseProgression(Project&, int preset);
void randomiseHarmony(Project&, std::uint32_t seed);
std::vector<std::string> chordNames(const Settings&);
// A zero seed applies settings to stored takes. A nonzero seed draws new genre
// patterns, releasing fixed pattern/riff overrides on targeted tracks. Song scope
// also changes key, tempo, feel and 8-bar section lengths unless notes are locked.
// Handwritten MIDI tracks keep their notes, routing and automation.
void generate(Project&, Scope scope = Scope::song, int track = 0, int section = 0, std::uint32_t seed = 0);
// Instrument-panel edits re-render the stored takes (zero seed) of the edited
// track. Harmony edits (key, scale, chords, voicing...) also re-render the other
// harmonic parts that follow the chords: piano, bass, strings and guitar. Locks hold.
void applyInstrumentChange(Project&, int track, bool harmony);
// A new performance take of one clip from the current instrument settings: groove,
// riff and pattern choices are kept, only the clip's variation seed changes.
// Returns false (and changes nothing) for a locked track or clip.
bool regenerateClip(Project&, int track, int clip, std::uint32_t seed);
// Musical automation for a scope (as generate): build-up sweeps and risers, volume and pan
// movement, plugin parameter sweeps on the lanes a track already has. No fades in intros or
// outros. Locked tracks, clips and lanes are kept; the same seed gives the same lanes.
// `index` is the section (Scope::section) or the clip (Scope::clip). onlyLane: 0 volume,
// 1 pan, 2 + k plugin lane k, -1 every lane of the track(s).
void randomiseAutomation(Project&, Scope scope, int track, int index, std::uint32_t seed, int onlyLane = -1);
sgk::GrooveSettings pianoEngineSettings(const Project&,int track,std::uint32_t seed,std::uint32_t motifSeed=0);
void setPianoPattern(EngineOptions&,const sgk::GroovePattern&);
void applyPianoIntention(Project&,int track,int intention,bool keepRhythm,bool keepHarmony,std::uint32_t seed);
// Section-aligned clip gestures retain the fixed instrument routing.
bool transferClip(Project&, int track, int from, int to, bool copy);
void tidyNotes(Clip&, double beats);
void transpose(Clip&, int semitones);
void quantise(Clip&, double grid, double strength = 1, double limit = 0);
void humanize(Clip&, double amount, std::uint32_t seed, double beats);
struct TimedEvent { double beat; int track, pitch, velocity, lane; int controller=-1; };
std::vector<TimedEvent> events(const Project&, int onlyTrack = -1);
// Standard MIDI File type 1, 960 PPQ, tempo, time/key signatures and section markers.
std::vector<std::uint8_t> midiFile(const Project&, int onlyTrack = -1);
}
