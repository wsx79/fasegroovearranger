// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Project.h"
#include "../../ThirdParty/fasegroovepiano/Source/engine/GrooveEngine.h"
#include "../../ThirdParty/fasegroovedrums/Source/engine/GrooveEngine.h"
#include "../../ThirdParty/fasegrooveguitar/Source/engine/GrooveEngine.h"
#include <cmath>
#include <limits>
#include <random>
#include <stdexcept>

namespace fga {
namespace {
std::uint32_t mix(std::uint32_t x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; return x ^ (x >> 16); }
double finite(double x, double fallback, double lo, double hi) { return std::isfinite(x) ? std::clamp(x, lo, hi) : fallback; }
sgk::GrooveSettings pianoSettings(const Settings& s, std::uint32_t seed) {
    sgk::GrooveSettings p;
    p.keyRoot=s.key; p.scale=s.scale; p.extension=(sgk::Extension)s.extension; p.voicing=(sgk::Voicing)s.voicing;
    p.progressionLength=s.chordCount; p.progression=s.degrees; p.freeChords=s.freeChords;
    p.bpm=s.bpm; p.swing=(float)s.swing; p.riffDensity=(float)s.density; p.humanize=(float)s.humanize;
    p.bass=(sgk::BassMode)s.bassMode; p.strings=(sgk::StringsMode)s.stringsMode;
    p.realism=true; p.variationEvery=4; p.fillEveryBars=4; p.fillStyle=sgk::FillStyle::mix;
    p.humanizeSeed=seed; p.riffSeed=seed; p.strum=.15f;
    // The piano's random riff is genre-neutral. Start from its actual genre library,
    // then apply the original variation and intention engines to that vocabulary.
    std::vector<int> candidates;
    const auto& patterns=sgk::groovePatterns();
    const auto category=sgk::grooveCategories()[(size_t)s.genre];
    for(size_t i=0;i<patterns.size();++i) if(patterns[i].category==category) candidates.push_back((int)i);
    p.pattern=candidates.empty()?0:candidates[seed%candidates.size()];
    auto pattern=sgk::makeVariation(patterns[(size_t)p.pattern],seed);
    if(s.density<.35) pattern=sgk::applyIntention(pattern,sgk::Intention::minimal,seed);
    if(s.density>.72) pattern=sgk::addGhostNotes(pattern,seed);
    p.pattern=sgk::userPatternIndex();
    for(int i=0;i<sgk::userPatternLength;++i) {
        p.userRight[(size_t)i]=pattern.rightHand[(size_t)i%pattern.rightHand.size()];
        p.userLeft[(size_t)i]=pattern.leftHand[(size_t)i%pattern.leftHand.size()];
    }
    return p;
}
template<class E, class Filter> std::vector<Note> notesFrom(const std::vector<E>& stream, double beats, Filter filter) {
    std::array<int,128> active; active.fill(-1);
    std::vector<Note> notes;
    for(const auto& e:stream) {
        if(!filter(e)||e.note<0||e.note>127) continue;
        auto& index=active[(size_t)e.note];
        if(e.isNoteOn()) {
            if(index>=0) notes[(size_t)index].length=std::max(.001,e.ppq-notes[(size_t)index].start);
            const double start=std::clamp(e.ppq,0.0,beats);
            if(start>=beats) {index=-1;continue;}
            notes.push_back({start,.25,e.note,std::clamp(e.velocity,1,127),0}); index=(int)notes.size()-1;
        } else if(index>=0) {
            notes[(size_t)index].length=std::max(.001,std::min(beats,e.ppq)-notes[(size_t)index].start); index=-1;
        }
    }
    for(int i:active) if(i>=0) notes[(size_t)i].length=std::max(.001,beats-notes[(size_t)i].start);
    return notes;
}
}
const char* sectionName(Section s) {
    static const char* names[]{"Main","Intro","Breakdown","Build-up","Drop","Outro"};
    return names[std::clamp((int)s,0,5)];
}
std::vector<std::string> genres() { std::vector<std::string> out; for(auto s:sgk::grooveCategories()) out.emplace_back(s); return out; }
double Project::sectionStart(int index) const { double p=0; for(int i=0;i<std::clamp(index,0,(int)sections.size());++i) p+=sections[(size_t)i].bars*4.0; return p; }
void Project::normalise() {
    auto& s=settings;
    s.genre=std::clamp(s.genre,0,12); s.key=std::clamp(s.key,0,11); s.scale=std::clamp(s.scale,0,12);
    s.extension=std::clamp(s.extension,0,10); s.voicing=std::clamp(s.voicing,0,10);
    s.bpm=finite(s.bpm,122,30,300); s.swing=finite(s.swing,.2,0,1.5); s.density=finite(s.density,.55,0,1); s.humanize=finite(s.humanize,.2,0,1);
    s.chordCount=std::clamp(s.chordCount,1,8); s.bassMode=std::clamp(s.bassMode,0,sgk::numBassModes-1); s.stringsMode=std::clamp(s.stringsMode,0,10);
    for(auto& d:s.degrees) d=std::clamp(d,0,6);
    for(auto& c:s.freeChords) c=std::clamp(c,-1,35);
    if(sections.empty()) sections.push_back({Section::main,8,"Main"});
    if(sections.size()>64) sections.resize(64);
    for(auto& p:sections) {p.bars=std::clamp(p.bars,1,128); p.type=(Section)std::clamp((int)p.type,0,5);}
    if(tracks.size()>trackCount) tracks.resize(trackCount);
    while(tracks.size()<trackCount) { Track t; t.kind=(int)tracks.size(); t.name=t.kind<4?std::array<const char*,4>{"Piano","Bass","Strings","Guitar"}[(size_t)t.kind]:fgd::laneName(t.kind-4); tracks.push_back(t); }
    for(size_t i=0;i<tracks.size();++i) {
        auto& t=tracks[i]; t.kind=(int)i; t.clips.resize(sections.size());
        t.gain=finite(t.gain,.75,0,2); t.pan=finite(t.pan,0,-1,1); t.sampleRoot=std::clamp(t.sampleRoot,0,127); t.sound=std::clamp(t.sound,0,6);
        for(size_t k=0;k<sections.size();++k) tidyNotes(t.clips[k],sections[k].bars*4.0);
    }
    masterGain=finite(masterGain,.8,0,2); loopStart=finite(loopStart,0,0,length()); loopEnd=finite(loopEnd,length(),loopStart,length());
    if(loopEnd-loopStart<1./960.)loop=false;
}
void Project::insertSection(int index,SongSection section,int copyFrom) {
    if(sections.size()>=64) return;
    index=std::clamp(index,0,(int)sections.size());
    std::vector<Clip> copies;
    for(auto& t:tracks) copies.push_back(copyFrom>=0&&copyFrom<(int)t.clips.size()?t.clips[(size_t)copyFrom]:Clip{});
    sections.insert(sections.begin()+index,std::move(section));
    for(size_t i=0;i<tracks.size();++i) tracks[i].clips.insert(tracks[i].clips.begin()+index,copies[i]);
    normalise();
}
void Project::removeSection(int index) {
    if(sections.size()<2||index<0||index>=(int)sections.size()) return;
    sections.erase(sections.begin()+index); for(auto& t:tracks) t.clips.erase(t.clips.begin()+index); normalise();
}
void Project::moveSection(int from,int to) {
    if(from<0||to<0||from>=(int)sections.size()||to>=(int)sections.size()||from==to) return;
    auto s=sections[(size_t)from]; sections.erase(sections.begin()+from); sections.insert(sections.begin()+to,s);
    for(auto& t:tracks) {auto c=std::move(t.clips[(size_t)from]); t.clips.erase(t.clips.begin()+from); t.clips.insert(t.clips.begin()+to,std::move(c));}
}
void chooseProgression(Project& p,int preset) {
    const auto& all=sgk::progressionPresets(); const auto& pr=all[(size_t)std::clamp(preset,0,(int)all.size()-1)];
    p.settings.scale=pr.scale; auto slots=sgk::presetSlots(pr,p.settings.extension);
    p.settings.chordCount=std::min(8,(int)slots.size());
    for(int i=0;i<p.settings.chordCount;++i) {p.settings.degrees[(size_t)i]=std::max(0,slots[(size_t)i].degree);p.settings.freeChords[(size_t)i]=slots[(size_t)i].freeCode;}
}
void randomiseHarmony(Project& p,std::uint32_t seed) {
    auto degrees=sgk::randomProgression(seed,4,p.settings.scale); p.settings.chordCount=4; p.settings.freeChords.fill(-1);
    std::copy(degrees.begin(),degrees.end(),p.settings.degrees.begin());
}
void applyGenre(Project& p,int genre) {
    p.settings.genre=std::clamp(genre,0,12);
    static const int tempos[]{124,122,120,123,126,126,132,122,120,126,96,100,128};
    static const int presets[]{4,2,0,8,17,28,9,0,33,27,29,6,4};
    p.settings.bpm=tempos[p.settings.genre]; p.settings.swing=fgd::genreSwing(p.settings.genre);
    chooseProgression(p,presets[p.settings.genre]);
    // Match each style to an actual named bass line in the original library.
    static const int bassLines[]{8,23,16,11,14,13,18,15,20,12,10,16,12};
    p.settings.bassMode=sgk::bassModeForLine(bassLines[p.settings.genre]);
    p.settings.stringsMode=p.settings.genre==8?(int)sgk::StringsMode::discoOctaves:(int)sgk::StringsMode::pad;
}
Project newProject(int genre) {
    Project p;
    p.sections={{Section::intro,8,"Intro"},{Section::main,16,"Main"},{Section::breakdown,8,"Breakdown"},{Section::buildup,4,"Build-up"},{Section::drop,16,"Drop"},{Section::outro,8,"Outro"}};
    if(genre==10||genre==11) {p.sections[0].bars=4;p.sections[5].bars=4;}
    p.normalise(); applyGenre(p,genre); p.loopEnd=p.length(); generate(p); return p;
}
std::vector<std::string> chordNames(const Settings& s) {
    auto settings=pianoSettings(s,s.seed); std::vector<std::string> out;
    for(int i=0;i<s.chordCount;++i) out.push_back(sgk::chordName(sgk::GrooveEngine::chordForSlot(settings,i)));
    return out;
}
void generate(Project& p,Scope scope,int track,int section,std::uint32_t seed) {
    p.normalise();
    if(track<0||track>=trackCount||section<0||section>=(int)p.sections.size()) throw std::out_of_range("Generation selection");
    const auto& s=p.settings;
    for(int ti=0;ti<trackCount;++ti) {
        auto& t=p.tracks[(size_t)ti];
        if(t.locked||((scope==Scope::track||scope==Scope::clip)&&ti!=track)) continue;
        for(size_t si=0;si<p.sections.size();++si) {
            if((scope==Scope::section||scope==Scope::clip)&&(int)si!=section) continue;
            auto& clip=t.clips[si]; if(clip.locked) continue;
            if(seed) clip.seed=ti>=4?mix(seed+313U):mix(seed+(std::uint32_t)ti*131U+(std::uint32_t)si*977U);
            else if(clip.seed==1) clip.seed=ti>=4?mix(s.seed+313U):mix(s.seed+(std::uint32_t)ti*131U+(std::uint32_t)si*977U);
            const double beats=p.sections[si].bars*4.0; const auto type=p.sections[si].type;
            auto ps=pianoSettings(s,clip.seed);
            if(ti<3) {
                ps.section=(sgk::Section)type; ps.sectionVariation=clip.seed;
                auto all=sgk::GrooveEngine::renderArrangement(ps,{{(sgk::Section)type,p.sections[si].bars,clip.seed,-1}});
                clip.notes=notesFrom(all,beats,[ti](const sgk::NoteEvent& e){return ti==0?(e.part==sgk::Part::chords||e.part==sgk::Part::fill):ti==1?e.part==sgk::Part::bass:e.part==sgk::Part::strings;});
            } else if(ti==3) {
                fgg::GuitarSettings gs; gs.pattern=fgg::randomPatternIndex(); gs.randomGenre=s.genre; gs.riffSeed=clip.seed;
                gs.density=(float)s.density; gs.swing=(float)s.swing; gs.humanize=(float)s.humanize; gs.humanizeSeed=clip.seed;
                gs.tempo=s.bpm; gs.roomForBand=true; gs.numChords=s.chordCount; gs.fillEveryBars=4; gs.variationEvery=4; gs.fillStyle=fgg::FillStyle::mix;
                for(int i=0;i<s.chordCount;++i) gs.chords[(size_t)i]=s.freeChords[(size_t)i]>=0?fgg::codeChord(s.key,s.freeChords[(size_t)i],std::min(s.extension,2)):fgg::diatonicChord(s.key,s.scale,s.degrees[(size_t)i],std::min(s.extension,2));
                auto all=fgg::GrooveEngine::renderArrangement(gs,{{(fgg::Section)type,p.sections[si].bars,clip.seed,-1}});
                clip.notes=notesFrom(all,beats,[](const fgg::NoteEvent& e){return !e.isController()&&e.lane<fgg::numLanes;});
                // Keep each note's articulation lane for the original guide guitar.
                for(auto& n:clip.notes) for(const auto& e:all) if(e.isNoteOn()&&e.note==n.pitch&&std::abs(e.ppq-n.start)<.00001){n.lane=e.lane;break;}
            } else {
                fgd::DrumSettings ds; ds.pattern=fgd::randomPatternIndex(); ds.randomGenre=s.genre;
                // One kit seed for all drum stems keeps kick, backbeat and hats coherent.
                ds.riffSeed=clip.seed; ds.sectionVariation=mix(clip.seed+(std::uint32_t)si*977U);
                ds.density=(float)s.density; ds.swing=(float)s.swing; ds.humanize=(float)s.humanize; ds.humanizeSeed=clip.seed;
                ds.tempo=s.bpm; ds.bassSpace=true; ds.fillEveryBars=4; ds.variationEvery=4; ds.fillStyle=fgd::FillStyle::mix;
                auto all=fgd::GrooveEngine::renderArrangement(ds,{{(fgd::Section)type,p.sections[si].bars,ds.sectionVariation,-1}});
                clip.notes=notesFrom(all,beats,[ti](const fgd::NoteEvent& e){return e.lane==ti-4;});
            }
            tidyNotes(clip,beats);
        }
    }
}
void tidyNotes(Clip& c,double beats) {
    constexpr double tick=1./960.;
    c.notes.erase(std::remove_if(c.notes.begin(),c.notes.end(),[beats,tick](const Note& n){return !std::isfinite(n.start)||!std::isfinite(n.length)||n.start>beats-tick||n.length<=0||n.pitch<0||n.pitch>127||n.velocity<1||n.velocity>127;}),c.notes.end());
    for(auto& n:c.notes) {n.start=std::max(0.0,n.start);n.length=std::min(std::max(tick,n.length),beats-n.start);n.lane=std::clamp(n.lane,0,12);}
    std::stable_sort(c.notes.begin(),c.notes.end(),[](const Note& a,const Note& b){return a.start==b.start?a.pitch<b.pitch:a.start<b.start;});
    // One MIDI channel per instrument: coalesce simultaneous guitar-string hits
    // and close retriggered pitches so a later note-off cannot cut the next note.
    std::vector<Note> clean;clean.reserve(c.notes.size());std::array<int,128> last;last.fill(-1);
    for(auto n:c.notes){int index=last[(size_t)n.pitch];if(index>=0){auto& prev=clean[(size_t)index];if(n.start-prev.start<tick){prev.length=std::max(prev.length,n.start+n.length-prev.start);prev.velocity=std::max(prev.velocity,n.velocity);continue;}if(prev.start+prev.length>n.start)prev.length=n.start-prev.start;}
        last[(size_t)n.pitch]=(int)clean.size();clean.push_back(n);}
    c.notes=std::move(clean);
}
void transpose(Clip& c,int semitones) {for(auto& n:c.notes)n.pitch=std::clamp(n.pitch+semitones,0,127);}
void quantise(Clip& c,double grid,double strength) {if(!std::isfinite(grid)||grid<=0)return;strength=std::clamp(strength,0.0,1.0);for(auto& n:c.notes)n.start+=(std::round(n.start/grid)*grid-n.start)*strength;}
std::vector<TimedEvent> events(const Project& p,int onlyTrack) {
    std::vector<TimedEvent> out;
    for(size_t ti=0;ti<p.tracks.size();++ti) {
        if(onlyTrack>=0&&(int)ti!=onlyTrack)continue;
        const auto& t=p.tracks[ti];
        for(size_t si=0;si<t.clips.size()&&si<p.sections.size();++si) {
            const double begin=p.sectionStart((int)si),end=p.sections[si].bars*4.;
            for(const auto& n:t.clips[si].notes) {
                out.push_back({begin+n.start,(int)ti,n.pitch,n.velocity,n.lane});
                out.push_back({begin+std::min(end,n.start+n.length),(int)ti,n.pitch,0,n.lane});
            }
        }
    }
    std::stable_sort(out.begin(),out.end(),[](const TimedEvent& a,const TimedEvent& b){if(a.beat!=b.beat)return a.beat<b.beat;return a.velocity<b.velocity;});return out;
}
}
