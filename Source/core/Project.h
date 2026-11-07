// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <atomic>
#include <algorithm>
#include <cmath>
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "VolumeAutomation.h"
#include "StudioSettings.h"

namespace sgk {struct GrooveSettings;struct GroovePattern;enum class Extension;}
namespace fgg {struct Chord;}
namespace fga {
inline constexpr const char* hostedStatePrefix="FGAHOST1:"; // native state plus queued controller parameter values
constexpr int leadTrack = 17;
constexpr int legacyTrackCount = 17;
constexpr int trackCount = 18; // generated tracks, stable IDs
constexpr int maxTracks = 32;
constexpr double maxClipBeats = 131072.;
constexpr double maxAudioRegionSeconds = maxClipBeats*2.; // timeline extent at the minimum 30 BPM; source files remain sample-memory limited
// Dedicated engine slot: never a MIDI track and never exported as a stem.
constexpr int masterEffectTrack = maxTracks, engineTrackCount = maxTracks + 1;
inline bool isManualTrack(int track) { return track >= trackCount && track < maxTracks; }
inline bool isDrumTrack(int track) { return track >= 4 && track < legacyTrackCount; }
inline bool isHarmonicTrack(int track) { return track >= 0 && (track < 4 || track == leadTrack); }
inline int instrumentFamily(int track) { return isDrumTrack(track) ? 4 : track == leadTrack ? 5 : isManualTrack(track) ? 0 : track; }
enum class Section { main, intro, breakdown, buildup, drop, outro };
enum class Scope { song, track, section, clip };
struct Note {
    double start = 0, length = .25;
    int pitch = 60, velocity = 96, lane = 0;bool reverse=false;
    bool operator==(const Note& o) const { return start == o.start && length == o.length && pitch == o.pitch && velocity == o.velocity && lane == o.lane && reverse == o.reverse; }
};
struct Clip {
    std::vector<Note> notes;
    std::uint32_t seed = 1;
    bool locked = false;
    // A zero span follows the section; explicit spans and loop periods use beats.
    // Preserve the source across shortening so extending can recover its notes.
    double span = 0, loopLength = 0, loopPhase = 0;
    // Source seconds for a recorded audio region; negative offset means ordinary MIDI.
    double audioOffset = -1, audioDuration = 0, clipFadeIn = 0, clipFadeOut = 0;
    std::array<VolumeAutomation,3> expression; // sustain, modulation, pitch bend (0..1)
    // Inherited feel frozen for this take when another scope changes the global knobs.
    std::array<double,3> feel {-1,-1,-1};
    int role = -1;
    double position = 0; // Absolute beat when the track has an independent timeline.
    std::vector<Note> loopSource;
    // Persistent musical take, separate from performance timing. Zero loads legacy motifs.
    std::uint32_t motifSeed = 0;
    // Empty shows the section name. A custom label stays with the clip through edits.
    std::string name;
    bool operator==(const Clip& o) const { return notes == o.notes && motifSeed == o.motifSeed && seed == o.seed && locked == o.locked && span == o.span && audioOffset == o.audioOffset && audioDuration == o.audioDuration && clipFadeIn == o.clipFadeIn && clipFadeOut == o.clipFadeOut && expression == o.expression && feel == o.feel && loopPhase == o.loopPhase && loopLength == o.loopLength && loopSource == o.loopSource && position == o.position && role == o.role && name == o.name; }
};
struct ClipId {
    int track = -1, index = -1;
    bool operator<(const ClipId& o) const { return track < o.track || (track == o.track && index < o.index); }
    bool operator==(const ClipId& o) const { return track == o.track && index == o.index; }
};
struct ClipPlacement { int track = -1; double beat = 0; };
// rowDelta is a track-row offset from the anchor. clip.position is relative to the group origin.
struct ClipGroupPiece { int rowDelta = 0; Clip clip; };
struct SectionHarmony {
    bool enabled=false;int key=-1,scale=-1,chordCount=4,chordSteps=16;
    std::array<int,8> degrees{0,3,4,0,0,0,0,0},freeChords{-1,-1,-1,-1,-1,-1,-1,-1};
    std::array<bool,8> manualChords{};
};
struct SongSection { Section type = Section::main; int bars = 8; std::string name = "Main"; SectionHarmony harmony;bool preDropSilence=false,snareRoll=false,reverseCrash=false; };
// Stable storage IDs preserve existing projects and plugin assignments. The UI and
// MIDI file use musical order through trackAtRow().
int trackAtRow(int row);
int rowForTrack(int track);
double defaultDrumNoteLength(int track);
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
inline std::uint64_t nextTrackSessionId(){static std::atomic<std::uint64_t> next{1};return next.fetch_add(1,std::memory_order_relaxed);}
struct EffectSlot {
    int slot=1; // stable automation target, independent of position in the chain
    std::string name,pluginName,path,pluginId,state;
    bool bypassed=false;
    const std::string& displayName() const {return name.empty()?pluginName:name;}
};
struct Track {
    std::uint64_t sessionId=nextTrackSessionId(); // transient identity; copied by Undo, never serialized or used for MIDI routing
    std::string name;
    int kind = 0; // 0 piano, 1 bass, 2 strings, 3 guitar, 4..16 individual drums, 17 lead, 18..31 optional MIDI/vocal parts
    double gain = .75, pan = 0;
    EqSettings eq;
    bool mute = false, solo = false, locked = false;
    int sound = 0, sampleRoot = 60;
    std::string instrument = "internal", instrumentPath, instrumentId, instrumentState;
    std::vector<EffectSlot> effects;
    EffectSlot* effect(int slot){for(auto& e:effects)if(e.slot==slot)return &e;return nullptr;}
    const EffectSlot* effect(int slot)const{for(const auto& e:effects)if(e.slot==slot)return &e;return nullptr;}
    int nextEffectSlot()const{for(int slot=1;slot<=maxTrackEffects;++slot)if(!effect(slot))return slot;return 0;}
    bool removeEffect(int slot){auto it=std::find_if(effects.begin(),effects.end(),[slot](const auto& e){return e.slot==slot;});if(it==effects.end())return false;effects.erase(it);parameters.erase(std::remove_if(parameters.begin(),parameters.end(),[slot](const auto& lane){return lane.slot==slot;}),parameters.end());return true;}
    bool moveEffect(int slot,int position){auto it=std::find_if(effects.begin(),effects.end(),[slot](const auto& e){return e.slot==slot;});if(it==effects.end()||position<0||position>=(int)effects.size())return false;auto saved=std::move(*it);effects.erase(it);effects.insert(effects.begin()+position,std::move(saved));return true;}
    VolumeAutomation volume;
    VolumeAutomation panAutomation;            // 0..1 (see panFromAutomation); replaces `pan` while active
    std::vector<ParameterAutomation> parameters; // hosted plugin parameters, at most maxParameterLanes
    SamplerSettings sampler;
    std::string sampleHash;
    int group=-1;
    std::array<double,2> sends{};
    EngineOptions engine;
    bool vocalChops = false; // optional generated sample part; keeps legacy routing IDs
    bool freeTimeline = false;
    std::vector<Clip> clips;
};
inline bool sameEffectPlugins(const Track& a,const Track& b){if(a.effects.size()!=b.effects.size())return false;for(const auto& fx:a.effects){auto* old=b.effect(fx.slot);if(!old||old->path!=fx.path||old->pluginId!=fx.pluginId)return false;}return true;}
inline bool hasGenerator(const Track& t){return !isManualTrack(t.kind)||t.vocalChops;}
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
    // Chromatic spelling is independent of a user-authored chord override.
    std::array<bool, 8> manualChords {};
    // Regenerating (song, track, section, clip) also writes musical automation for that scope.
    // Off in projects saved before it existed; new projects start with it on.
    bool randomAutomation = false;
    bool grooveEnabled=false;std::array<double,16> grooveOffsets{},grooveAccents{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    bool freeTempo = false; // AI/imported compositions may use 30..300 BPM beyond genre presets
    bool targetEnabled = false;
    double targetSeconds = 0; // clock target; 0 or disabled leaves the arrangement unchanged
    double targetToleranceSeconds = 5; // ± seconds; 0 is an exact target and never rewrites the song
};
struct Project {
    std::string name = "Untitled groove";
    Settings settings;
    std::vector<SongSection> sections;
    std::vector<Track> tracks;
    std::array<BusSettings,4> buses=defaultBuses();
    Track masterEffects;
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
    // Group edits keep each clip on its row, shifted by the same time and row delta.
    // A failed edit leaves the project unchanged. Locks travel with the clip.
    // Drum pitches are remapped by normalise when a clip lands on a drum track.
    bool collectClipGroup(const std::vector<ClipId>&, int anchorTrack, std::vector<ClipGroupPiece>&, double& origin, double& endBeat) const;
    bool translateClipGroup(const std::vector<ClipId>&, double timeDelta, int rowDelta, bool copy, double& appliedDelta, std::vector<ClipPlacement>& placed);
    bool removeClipGroup(const std::vector<ClipId>&);
    bool placeClipGroup(int anchorTrack, double anchorBeat, const std::vector<ClipGroupPiece>&, std::vector<ClipPlacement>& placed);
    // Section length, order and removal move clips, automation and loops with the guide.
    bool resizeGuide(int section, int bars);
    void removeGuide(int section);
    void moveGuide(int from, int to);
    // Free-timeline time edits shared by section insert, resize, move and delete.
    bool splitFreeAt(double beat);
    void shiftFreeFrom(double beat, double delta);
    bool deleteFreeRange(double start, double end);
    bool foldFreeTracks();
    void normalise();
    bool deleteMidiTrack(int track); // fixed instrument tracks cannot be removed
    int addVocalChopsTrack();
    int addMidiTrack(); // returns its stable routing ID, or -1 at capacity
    bool withinNoteBudget() const;
    bool insertSection(int index, SongSection section, int copyFrom = -1);
    bool duplicateSection(int index);
    void removeSection(int index);
    void moveSection(int from, int to);
    void resizeSection(int index, int bars, bool shiftAfter = false);
    bool resizeClipLoop(int track, int section, double beats);
    bool resizeSectionLoop(int index, int bars);
};
inline double projectSeconds(const Project& p){return p.settings.bpm>0?p.length()*60./p.settings.bpm:0;}
inline double targetBeat(const Project& p){return p.settings.targetEnabled&&p.settings.targetSeconds>0&&p.settings.bpm>0?p.settings.targetSeconds*p.settings.bpm/60.:-1;}
enum class TargetFit { off, within, below, above };
inline TargetFit targetFit(const Project& p){if(!p.settings.targetEnabled||!(p.settings.targetSeconds>0)||!(p.settings.bpm>0))return TargetFit::off;const double delta=projectSeconds(p)-p.settings.targetSeconds;const double tol=std::max(0.,p.settings.targetToleranceSeconds);if(std::abs(delta)<=tol)return TargetFit::within;return delta<0?TargetFit::below:TargetFit::above;}
const char* sectionName(Section);
enum class Genre : int { pianoHouse,soulful,deepHouse,gospel,jackinHouse,pianoJack,ukGarage,afroHouse,discoHouse,ninetiesHouse,neoSoul,chill,danceStabs,techno,electroHouse,count };
constexpr int deepHouseGenre=(int)Genre::deepHouse,technoGenre=(int)Genre::techno,electroHouseGenre=(int)Genre::electroHouse,numGenres=(int)Genre::count;
// Invalid legacy IDs retain their old Dance Stabs fallback.
// normaliseGenre -> 0..14: arranger arrays (styles, profiles, forms, extensions) have one row per style.
inline int normaliseGenre(int genre){return genre<0?0:genre>=numGenres?12:genre;}
inline bool isElectronicGenre(int genre){return genre==technoGenre||genre==electroHouseGenre;}
// engineGenre -> 0..12: vendor engines only know the original thirteen styles; Techno and
// Electro House reuse the Dance Stabs engine row but get their own arranger generators.
inline int engineGenre(int genre){return isElectronicGenre(genre)?12:normaliseGenre(genre);}
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
// Change structure by reusing stored MIDI takes; locked projects remain unchanged.
bool rearrangeSong(Project&, std::uint32_t seed);
void chooseProgression(Settings&, int preset);
void chooseProgression(Project&, int preset);
void randomiseHarmony(Settings&, std::uint32_t seed);
void randomiseHarmony(Project&, std::uint32_t seed);
std::vector<std::string> chordNames(const Settings&);
// Shared reduction of the piano harmony for generated and hand-entered guitar/lead notes.
fgg::Chord bandChord(int key,int scale,int degree,int free,sgk::Extension extension);
// A zero seed applies settings to stored takes. A nonzero seed draws new genre
// patterns, releasing fixed pattern/riff overrides on targeted tracks. Song scope
// also changes key, tempo, feel and 8-bar section lengths unless notes are locked.
// Handwritten MIDI tracks keep their notes, routing and automation.
void generate(Project&, Scope scope = Scope::song, int track = 0, int section = 0, std::uint32_t seed = 0);
// Instrument-panel edits re-render the stored takes (zero seed) of the edited
// track. Harmony edits (key, scale, chords, voicing...) also re-render the other
// harmonic parts that follow the chords: piano, bass, strings and guitar. Locks hold.
void applyInstrumentChange(Project&, int track, bool harmony);
// Header edits explicitly take precedence on unlocked generated tracks.
enum FeelParameter : unsigned { densityFeel=1, swingFeel=2, humanizeFeel=4 };
void applyGlobalFeel(Project&, unsigned parameters);
bool rebuildClipLoop(Clip&, double duration);

SamplerSettings vocalSamplerSettings();
void generateVocalChops(Project&,Scope,int track,int section,std::uint32_t seed);
void remapVocalChops(Project&,int track,const SamplerSettings& previous,bool force=false);
// A new clip variation keeps explicit riff/pattern choices and renews its
// performance seed and the visible genre-based Density, Swing and Feel.
// Returns false (and changes nothing) for a locked track or clip.
bool regenerateClip(Project&, int track, int clip, std::uint32_t seed);
// Musical automation for a scope (as generate): build-up sweeps and risers, volume and pan
// movement, plugin parameter sweeps on the lanes a track already has. No fades in intros or
// outros. Locked tracks, clips and lanes are kept; the same seed gives the same lanes.
// `index` is the section (Scope::section) or the clip (Scope::clip). onlyLane: 0 volume,
// 1 pan, 2 + k plugin lane k, -1 every lane of the track(s).
void randomiseAutomation(Project&, Scope scope, int track, int index, std::uint32_t seed, int onlyLane = -1);
inline double takeFeel(double track,double take,double global){return track>=0?track:take>=0?take:global;}
Settings resolveSectionHarmony(const Project&,int section);
inline bool sectionHarmonyEnabled(const Project& p,int section){return section>=0&&section<(int)p.sections.size()&&p.sections[(size_t)section].harmony.enabled;}
inline Settings activeHarmony(const Project& p,int section){return sectionHarmonyEnabled(p,section)?resolveSectionHarmony(p,section):p.settings;}
inline void storeHarmony(Project& p,int section,const Settings& s){
    if(sectionHarmonyEnabled(p,section)){auto& h=p.sections[(size_t)section].harmony;h.key=s.key;h.scale=s.scale;h.chordCount=s.chordCount;h.chordSteps=s.chordSteps;h.degrees=s.degrees;h.freeChords=s.freeChords;h.manualChords=s.manualChords;}
    else{p.settings.key=s.key;p.settings.scale=s.scale;p.settings.chordCount=s.chordCount;p.settings.chordSteps=s.chordSteps;p.settings.degrees=s.degrees;p.settings.freeChords=s.freeChords;p.settings.manualChords=s.manualChords;}
}
std::string chordNameAt(const Project&,double beat);
// Empty when the sounding pitches are not one major, minor or seventh chord.
// Octave duplicates collapse. The lowest note picks the root only when several roots fit.
std::string recognizeChordPitches(const std::vector<int>& midiNotes, int key, int scale);
// Notes are clip-local. Overlapping pitches in [from, to) replace `fallback` only when
// every recognized sonority is the same chord. Melody, silence and mixed chords keep `fallback`.
std::string chordHeardInClip(const std::vector<Note>&, double from, double to, int key, int scale, const std::string& fallback);
// One chord cell at `beat`, clipped to the section, using the same harmony as chordNameAt.
void chordCell(const Project&, double beat, double& start, double& end, std::string& name);
sgk::GrooveSettings pianoEngineSettings(const Project&,int track,std::uint32_t seed,std::uint32_t motifSeed=0,const Clip* clip=nullptr);
void setPianoPattern(EngineOptions&,const sgk::GroovePattern&);
void applyPianoIntention(Project&,int track,int intention,bool keepRhythm,bool keepHarmony,std::uint32_t seed);
// Section-aligned clip gestures retain the fixed instrument routing.
bool transferClip(Project&, int track, int from, int to, bool copy);
void tidyNotes(Clip&, double beats);
void transpose(Clip&, int semitones);
void quantise(Clip&, double grid, double strength = 1, double limit = 0);
void humanize(Clip&, double amount, std::uint32_t seed, double beats);
struct TimedEvent { double beat; int track, pitch, velocity, lane; int controller=-1; double audioOffset=-1,audioDuration=0,audioBeats=0,fadeIn=0,fadeOut=0;bool reverse=false; };
std::vector<TimedEvent> events(const Project&, int onlyTrack = -1);
// Standard MIDI File type 1, 960 PPQ, tempo, time/key signatures and section markers.
std::vector<std::uint8_t> midiFile(const Project&, int onlyTrack = -1);
}
