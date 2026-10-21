// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Project.h"
#include "PianoComping.h"
#include "GenreSound.h"
#include "../../ThirdParty/fasegroovepiano/Source/engine/GrooveEngine.h"
#include "../../ThirdParty/fasegroovedrums/Source/engine/GrooveEngine.h"
#include "../../ThirdParty/fasegrooveguitar/Source/engine/GrooveEngine.h"
#include <cmath>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <type_traits>

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
    for(size_t i=0;i<patterns.size();++i) if(patterns[i].category==category) {
        // Jackin' is built around a chopped two-bar hook. Keep automatic
        // selection out of held organ/warehouse phrases; explicit presets and
        // hand-edited patterns below still take precedence.
        if(s.genre==4 && patterns[i].name!="Disco Jack Chops" && patterns[i].name!="Jack Chops"
                     && patterns[i].name!="Chicago Stab Jack" && patterns[i].name!="Jack Stab Skips")continue;
        candidates.push_back((int)i);
    }
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
    std::vector<Note> notes;notes.reserve(stream.size()/2);
    for(const auto& e:stream) {
        if(!filter(e)||e.note<0||e.note>127) continue;
        auto& index=active[(size_t)e.note];
        if(e.isNoteOn()) {
            if(index>=0) notes[(size_t)index].length=std::max(.001,e.ppq-notes[(size_t)index].start);
            const double start=std::clamp(e.ppq,0.0,beats);
            if(start>=beats) {index=-1;continue;}
            int lane=0;if constexpr(std::is_same_v<E,fgg::NoteEvent>||std::is_same_v<E,fgd::NoteEvent>)lane=e.lane;
            notes.push_back({start,.25,e.note,std::clamp(e.velocity,1,127),lane}); index=(int)notes.size()-1;
        } else if(index>=0) {
            notes[(size_t)index].length=std::max(.001,std::min(beats,e.ppq)-notes[(size_t)index].start); index=-1;
        }
    }
    for(int i:active) if(i>=0) notes[(size_t)i].length=std::max(.001,beats-notes[(size_t)i].start);
    return notes;
}
}
sgk::GrooveSettings pianoEngineSettings(const Project& p,int track,std::uint32_t seed){
    const auto& s=p.settings;const auto& e=p.tracks[(size_t)track].engine;auto trackSettings=s;if(e.density>=0)trackSettings.density=e.density;
    // The song keeps its motif; clip seeds change its performance/variation,
    // not the library preset on every section boundary.
    const auto motifSeed=mix(s.seed+(std::uint32_t)track*131U);
    auto ps=pianoSettings(trackSettings,e.riffSeed?e.riffSeed:motifSeed);
    ps.humanizeSeed=e.humanizeSeed?e.humanizeSeed:seed;
    ps.swing=(float)(e.swing<0?s.swing:e.swing);ps.humanize=(float)(e.humanize<0?s.humanize:e.humanize);ps.riffDensity=(float)(e.density<0?s.density:e.density);
    ps.velocity=e.velocity;ps.gate=(float)e.gate;ps.strum=(float)e.strum;ps.centre=e.centre;ps.movement=e.movement;ps.octaveTop=e.octaveTop;
    ps.strumDirection=e.strumDirection;ps.topLine=(sgk::TopLine)e.topLine;ps.approach=(sgk::ApproachStyle)e.approach;ps.evolveBars=e.evolveBars;ps.humanizeStyle=e.humanizeStyle;ps.bassFeel=e.bassFeel;
    ps.rangeLow=e.rangeLow;ps.rangeHigh=e.rangeHigh;ps.realism=e.realism;ps.grooveBars=e.grooveBars;ps.segmentOrder=e.segments;
    ps.fillEveryBars=e.fillEvery;ps.fillStyle=(sgk::FillStyle)(e.fillStyle<0?(int)sgk::FillStyle::mix:std::min(e.fillStyle,sgk::numFillStyles-1));ps.variationEvery=e.variationEvery;
    ps.stepsPerChord=s.chordSteps;ps.inversions=e.inversions;ps.velocityOrder=e.velocityOrder;
    if(e.humanizeSeed)ps.humanizeSeed=e.humanizeSeed;
    if(e.riffSeed)ps.riffSeed=e.riffSeed;
    if(e.pattern>=0&&e.pattern<=sgk::userPatternIndex())ps.pattern=e.pattern;
    if(e.pattern==sgk::userPatternIndex()){ps.userRight=e.userRight;ps.userLeft=e.userLeft;ps.userVelRight=e.userVelRight;ps.userVelLeft=e.userVelLeft;}
    return ps;
}
void setPianoPattern(EngineOptions& e,const sgk::GroovePattern& pattern){
    e.pattern=sgk::userPatternIndex();
    for(size_t i=0;i<32;++i){e.userRight[i]=pattern.rightHand.empty()?'.':pattern.rightHand[i%pattern.rightHand.size()];e.userLeft[i]=pattern.leftHand.empty()?'.':pattern.leftHand[i%pattern.leftHand.size()];}
}
void applyPianoIntention(Project& p,int track,int intention,bool keepRhythm,bool keepHarmony,std::uint32_t seed)
{
    const auto which = (sgk::Intention) std::clamp (intention,0,sgk::numIntentions-1);
    const auto s = pianoEngineSettings(p,track,seed);auto& e=p.tracks[(size_t)track].engine;
    if(e.swing<0)e.swing=p.settings.swing;
    if(e.humanize<0)e.humanize=p.settings.humanize;
    auto nudge=[](auto& value,double delta,double lo,double hi){value=(std::decay_t<decltype(value)>)std::clamp((double)value+delta,lo,hi);};

    if (! keepRhythm)
    {
        const auto source = sgk::applySegmentOrder (sgk::GrooveEngine::basePattern (s), s.segmentOrder);
        if (source.length() == sgk::userPatternLength)
        {
            auto result = sgk::applyIntention (source, which, seed);
            for (int seg = 0; seg < sgk::numSegments; ++seg)
                if (e.segmentLocked[(size_t) seg])
                    sgk::copySegment (result, source, seg);   // locked parts stay
            sgk::repairHolds (result.rightHand);
            sgk::repairHolds (result.leftHand);
            e.segments={0,1,2,3};
            setPianoPattern(e,result);
        }

        // The feel goes with the rhythm.
        switch (which)
        {
            case sgk::Intention::soulful:   nudge (e.swing, 0.05f, 0.0f, 0.5f); nudge (e.gate, 0.08f, 0.1f, 1.0f); nudge (e.humanize, 0.05f, 0.0f, 0.6f); break;
            case sgk::Intention::funky:     nudge (e.swing, 0.08f, 0.0f, 0.55f); nudge (e.gate, -0.12f, 0.2f, 1.0f); nudge (e.velocity, 4.0f, 1.0f, 127.0f); break;
            case sgk::Intention::minimal:   nudge (e.gate, 0.08f, 0.1f, 1.0f); e.fillEvery = (std::decay_t<decltype(e.fillEvery)>)(0.0f); break;
            case sgk::Intention::energetic: nudge (e.velocity, 6.0f, 1.0f, 127.0f); nudge (e.gate, -0.05f, 0.2f, 1.0f);
                                            if ((int)e.fillEvery == 0) e.fillEvery = (std::decay_t<decltype(e.fillEvery)>)(2.0f);
                                            e.octaveTop = (std::decay_t<decltype(e.octaveTop)>)(1.0f); break;
            case sgk::Intention::sophisticated: nudge (e.strum, 0.08f, 0.0f, 1.0f); break;
            case sgk::Intention::straighter: nudge (e.swing, -0.1f, 0.0f, 0.55f); nudge (e.humanize, -0.05f, 0.0f, 0.6f); break;
        }
    }

    if (! keepHarmony)
    {
        // Only colours and voicings: the chords of the progression never change.
        const int ext = (int)p.settings.extension;
        const int voicing = (int)p.settings.voicing;
        switch (which)
        {
            case sgk::Intention::soulful:
                if (ext < 2) p.settings.extension = (std::decay_t<decltype(p.settings.extension)>)(2.0f);                     // 9ths
                if (voicing == (int) sgk::Voicing::close) p.settings.voicing = (std::decay_t<decltype(p.settings.voicing)>)((float) sgk::Voicing::rootless);
                if ((int)e.approach == 0) e.approach = (std::decay_t<decltype(e.approach)>)(1.0f);
                break;
            case sgk::Intention::funky:
                if (ext == 0) p.settings.extension = (std::decay_t<decltype(p.settings.extension)>)(1.0f);                    // 7ths
                if (voicing == (int) sgk::Voicing::wide) p.settings.voicing = (std::decay_t<decltype(p.settings.voicing)>)((float) sgk::Voicing::close);
                break;
            case sgk::Intention::minimal:
                if (ext > 2) p.settings.extension = (std::decay_t<decltype(p.settings.extension)>)(1.0f);
                p.settings.voicing = (std::decay_t<decltype(p.settings.voicing)>)((float) sgk::Voicing::shell);
                break;
            case sgk::Intention::energetic:
                if (voicing == (int) sgk::Voicing::shell) p.settings.voicing = (std::decay_t<decltype(p.settings.voicing)>)((float) sgk::Voicing::close);
                break;
            case sgk::Intention::sophisticated:
                p.settings.extension = (std::decay_t<decltype(p.settings.extension)>)(ext == 3 ? 4.0f : 3.0f);                // 11ths / 13ths, or 6/9
                p.settings.voicing = (std::decay_t<decltype(p.settings.voicing)>)((float) (voicing == (int) sgk::Voicing::drop2 ? sgk::Voicing::drop24 : sgk::Voicing::drop2));
                if ((int)e.approach == 0) e.approach = (std::decay_t<decltype(e.approach)>)(2.0f);
                break;
            case sgk::Intention::straighter:
                break;   // a rhythm move only
        }
    }
}

int trackAtRow(int row) { static constexpr int order[]{4,5,6,7,8,9,10,11,12,13,14,15,16,1,0,2,3}; return order[std::clamp(row,0,16)]; }
int rowForTrack(int track) { for(int i=0;i<trackCount;++i)if(trackAtRow(i)==track)return i;return 0; }
const char* sectionName(Section s) {
    static const char* names[]{"Main","Intro","Breakdown","Build-up","Drop","Outro"};
    return names[std::clamp((int)s,0,5)];
}
const GenreProfile& genreProfile(int genre) {
    static constexpr GenreProfile profiles[]{
        {120,128,124,32,true},  // Piano House
        {118,125,122,32,false}, // Soulful: a musical lift, not a rave roll
        {115,125,120,32,false}, // Deep: gradual development
        {118,125,123,16,false}, // Gospel: soulful-house working range
        {124,130,126,32,false}, // Jackin': short percussive turnarounds
        {124,130,126,32,true},  // Jackin' Piano
        {130,135,132,32,true},  // Garage / UK
        {115,125,122,32,false}, // Afro / Latin: percussion-led lift
        {112,124,120,32,false}, // Funky / Disco
        {120,130,126,32,true},  // 90s Piano
        { 92,112, 98,16,false}, // Neo Soul / Broken: warm-up/broken-groove policy
        { 90,115,100,16,false}, // Chill / Pads: downtempo warm-up policy
        {124,130,128,32,true}   // Dance Stabs
    };
    return profiles[std::clamp(genre,0,12)];
}
std::vector<std::string> genres() { std::vector<std::string> out; for(auto s:sgk::grooveCategories()) out.emplace_back(s); return out; }
double Project::sectionStart(int index) const { double p=0; for(int i=0;i<std::clamp(index,0,(int)sections.size());++i) p+=sections[(size_t)i].bars*4.0; return p; }
double Project::clipLength(int track,int section) const {
    if(section<0||section>=(int)sections.size())return 0;
    if(track>=0&&track<(int)tracks.size()&&section<(int)tracks[(size_t)track].clips.size()){
        const double span=tracks[(size_t)track].clips[(size_t)section].span;if(std::isfinite(span)&&span>0)return std::clamp(span,.25,512.);
    }
    return sections[(size_t)section].bars*4.;
}
double Project::length() const {
    double end=sectionStart((int)sections.size());
    for(size_t ti=0;ti<tracks.size();++ti)for(size_t si=0;si<tracks[ti].clips.size()&&si<sections.size();++si)
        if(tracks[ti].clips[si].span>0)end=std::max(end,sectionStart((int)si)+clipLength((int)ti,(int)si));
    return end;
}
namespace {
bool repeatLoop(const std::vector<Note>& source,double period,double duration,std::vector<Note>& notes){
    if(!std::isfinite(period)||period<=0||!std::isfinite(duration)||duration<=0)return false;
    if(source.empty()){notes.clear();return true;}
    const auto repeats=(size_t)std::ceil(duration/period);
    if(!source.empty()&&repeats>2000000/source.size())return false;
    Clip rendered;
    for(size_t repeat=0;repeat<repeats;++repeat)for(auto n:source){n.start+=repeat*period;if(n.start<duration){n.length=std::min(n.length,duration-n.start);rendered.notes.push_back(n);}}
    tidyNotes(rendered,duration);notes=std::move(rendered.notes);return true;
}
bool resizedLoop(Clip& clip,double previous,double duration){
    std::vector<Note> expected;
    const bool unchanged=clip.loopLength>0&&repeatLoop(clip.loopSource,clip.loopLength,previous,expected)&&expected==clip.notes;
    if(!unchanged){
        if(clip.loopLength>previous){
            // Edits in a shortened view must retain the hidden part of the source.
            std::vector<Note> source;for(auto n:clip.loopSource)if(n.start>=previous)source.push_back(n);
            for(auto n:clip.notes){auto it=std::lower_bound(clip.loopSource.begin(),clip.loopSource.end(),n,[](const Note& a,const Note& b){return a.start==b.start?a.pitch<b.pitch:a.start<b.start;});
                if(it!=clip.loopSource.end()&&it->start==n.start&&it->pitch==n.pitch&&it->lane==n.lane&&n.length==std::min(it->length,previous-n.start))n.length=it->length;
                source.push_back(n);}
            Clip clean;clean.notes=std::move(source);tidyNotes(clean,clip.loopLength);clip.loopSource=std::move(clean.notes);
        }else{clip.loopSource=clip.notes;clip.loopLength=previous;}
    }
    return repeatLoop(clip.loopSource,clip.loopLength,duration,clip.notes);
}
}
bool Project::resizeClipLoop(int track,int section,double beats){
    if(track<0||track>=(int)tracks.size()||section<0||section>=(int)sections.size()||section>=(int)tracks[(size_t)track].clips.size()||!std::isfinite(beats))return false;
    beats=std::clamp(std::round(beats*960)/960.,.25,512.);auto candidate=tracks[(size_t)track].clips[(size_t)section];
    if(!resizedLoop(candidate,clipLength(track,section),beats))return false;
    size_t count=candidate.notes.size()+candidate.loopSource.size();if(count>2000000)return false;
    for(size_t ti=0;ti<tracks.size();++ti)for(size_t si=0;si<tracks[ti].clips.size();++si)if((int)ti!=track||(int)si!=section){const auto& c=tracks[ti].clips[si];count+=c.notes.size()+c.loopSource.size();if(count>2000000)return false;}
    const double before=length();const bool wholeLoop=loopStart==0&&loopEnd==before;
    candidate.span=std::abs(beats-sections[(size_t)section].bars*4.)<1./960.?0:beats;
    tracks[(size_t)track].clips[(size_t)section]=std::move(candidate);if(wholeLoop)loopEnd=length();normalise();return true;
}
bool Project::resizeSectionLoop(int index,int bars){
    if(index<0||index>=(int)sections.size())return false;bars=std::clamp(bars,1,128);
    std::vector<Clip> clips;for(size_t ti=0;ti<tracks.size();++ti){if(index>=(int)tracks[ti].clips.size())return false;auto candidate=tracks[ti].clips[(size_t)index];
        if(!resizedLoop(candidate,clipLength((int)ti,index),bars*4.))return false;candidate.span=0;clips.push_back(std::move(candidate));}
    size_t count=0;for(size_t ti=0;ti<tracks.size();++ti)for(size_t si=0;si<tracks[ti].clips.size();++si){const auto& c=(int)si==index?clips[ti]:tracks[ti].clips[si];count+=c.notes.size()+c.loopSource.size();if(count>2000000)return false;}
    resizeSection(index,bars);for(size_t ti=0;ti<tracks.size();++ti)tracks[ti].clips[(size_t)index]=std::move(clips[ti]);normalise();return true;
}
void Project::normalise() {
    auto& s=settings;
    s.genre=std::clamp(s.genre,0,12); s.key=std::clamp(s.key,0,11); s.scale=std::clamp(s.scale,0,12);
    if(s.chordSteps!=8&&s.chordSteps!=16&&s.chordSteps!=32&&s.chordSteps!=64)s.chordSteps=16;
    s.extension=std::clamp(s.extension,0,10); s.voicing=std::clamp(s.voicing,0,10);
    const auto& tempo=genreProfile(s.genre);
    s.bpm=finite(s.bpm,tempo.preferredBpm,tempo.minBpm,tempo.maxBpm); s.swing=finite(s.swing,.2,0,1.5); s.density=finite(s.density,.55,0,1); s.humanize=finite(s.humanize,.2,0,1);
    s.songMinutes=finite(s.songMinutes,5,4,20);
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
        t.volume.normalise(length());
        auto& e=t.engine;
        e.pattern=std::clamp(e.pattern,-1,10000);e.velocity=std::clamp(e.velocity,1,127);e.grooveBars=std::clamp(e.grooveBars,1,4);
        e.swing=finite(e.swing,-1,-1,1.5);e.humanize=finite(e.humanize,-1,-1,1);e.density=finite(e.density,-1,-1,1);
        e.gate=finite(e.gate,.55,.02,2);e.strum=finite(e.strum,.15,0,1);e.accent=finite(e.accent,.85,0,1);e.tone=finite(e.tone,.5,0,1);
        e.reverb=finite(e.reverb,.12,0,1);e.chorus=finite(e.chorus,0,0,1);e.pump=finite(e.pump,0,0,1);e.pumpRelease=finite(e.pumpRelease,.2,.05,1);
        e.centre=std::clamp(e.centre,24,108);e.rangeLow=std::clamp(e.rangeLow,0,126);e.rangeHigh=std::clamp(e.rangeHigh,e.rangeLow+1,127);
        e.fillEvery=std::clamp(e.fillEvery,0,32);e.fillStyle=std::clamp(e.fillStyle,-1,20);e.variationEvery=std::clamp(e.variationEvery,0,32);e.midiNote=std::clamp(e.midiNote,-1,127);
        e.topLine=std::clamp(e.topLine,0,5);e.approach=std::clamp(e.approach,0,2);e.movement=std::clamp(e.movement,0,2);e.strumDirection=std::clamp(e.strumDirection,0,2);e.evolveBars=std::clamp(e.evolveBars,0,32);e.humanizeStyle=std::clamp(e.humanizeStyle,0,3);e.bassFeel=std::clamp(e.bassFeel,0,2);
        e.pumpRate=std::clamp(e.pumpRate,0,3);e.pumpSource=std::clamp(e.pumpSource,0,1);
        for(auto& v:e.inversions)v=std::clamp(v,-3,3);
        for(auto& v:e.velocityOrder)v=std::clamp(v,0,3);
        for(auto* row:{&e.userRight,&e.userLeft})for(auto& v:*row)if(std::string(".xXg>=ud").find(v)==std::string::npos)v='.';
        for(auto* row:{&e.userVelRight,&e.userVelLeft})for(auto& v:*row)v=std::min((unsigned char)127,v);
        for(auto& v:e.segments)v=std::clamp(v,0,3);
        for(int lane=0;lane<12;++lane){auto i=(size_t)lane;e.laneVolume[i]=finite(e.laneVolume[i],1,0,2);e.laneLevel[i]=finite(e.laneLevel[i],1,0,2);e.laneSwing[i]=finite(e.laneSwing[i],1,0,1.5);e.laneShift[i]=finite(e.laneShift[i],0,-100,100);e.laneOctave[i]=std::clamp(e.laneOctave[i],-2,2);}
        for(size_t k=0;k<sections.size();++k){auto& c=t.clips[k];c.span=finite(c.span,0,0,512);if(c.span>0)c.span=std::max(.25,c.span);c.loopLength=finite(c.loopLength,0,0,512);if(c.loopLength>0)c.loopLength=std::max(.25,c.loopLength);
            if(i>=4)for(auto* notes:{&c.notes,&c.loopSource})for(auto& n:*notes){n.pitch=e.midiNote>=0?e.midiNote:fgd::defaultNote((int)i-4);n.lane=(int)i-4;}
            tidyNotes(c,clipLength((int)i,(int)k));if(c.loopLength>0){Clip source;source.notes=std::move(c.loopSource);tidyNotes(source,c.loopLength);c.loopSource=std::move(source.notes);}else c.loopSource.clear();}
    }
    masterGain=finite(masterGain,.8,0,2); loopStart=finite(loopStart,0,0,length()); loopEnd=finite(loopEnd,length(),loopStart,length());
    if(loopEnd-loopStart<1./960.)loop=false;
}
void Project::insertSection(int index,SongSection section,int copyFrom) {
    if(sections.size()>=64) return;
    index=std::clamp(index,0,(int)sections.size());
    const bool wholeSong=loopStart==0&&loopEnd==length();
    std::vector<Clip> copies;
    for(auto& t:tracks) copies.push_back(copyFrom>=0&&copyFrom<(int)t.clips.size()?t.clips[(size_t)copyFrom]:Clip{});
    const double start=sectionStart(index),duration=std::clamp(section.bars,1,128)*4.;
    for(auto& t:tracks){auto original=t.volume.points;for(auto& point:t.volume.points)if(point.beat>=start)point.beat+=duration;
        if(copyFrom>=0&&copyFrom<(int)sections.size()){const double from=sectionStart(copyFrom),end=sectionStart(copyFrom+1);for(auto point:original)if(point.beat>=from&&point.beat<end&&point.beat-from<duration){point.beat=start+point.beat-from;t.volume.points.push_back(point);}}
    }
    if(loopStart>=start)loopStart+=duration;
    if(loopEnd>start)loopEnd+=duration;
    sections.insert(sections.begin()+index,std::move(section));
    for(size_t i=0;i<tracks.size();++i) tracks[i].clips.insert(tracks[i].clips.begin()+index,copies[i]);
    if(wholeSong){loopStart=0;loopEnd=length();}
    normalise();
}
void Project::removeSection(int index) {
    if(sections.size()<2||index<0||index>=(int)sections.size()) return;
    const double start=sectionStart(index),end=sectionStart(index+1);
    for(auto& t:tracks){auto& points=t.volume.points;points.erase(std::remove_if(points.begin(),points.end(),[&](const VolumePoint& p){return p.beat>=start&&p.beat<end;}),points.end());for(auto& point:points)if(point.beat>=end)point.beat-=end-start;}
    auto shift=[&](double b){return b<=start?b:b>=end?b-(end-start):start;};
    loopStart=shift(loopStart);loopEnd=shift(loopEnd);
    sections.erase(sections.begin()+index); for(auto& t:tracks) t.clips.erase(t.clips.begin()+index); normalise();
}
void Project::moveSection(int from,int to) {
    if(from<0||to<0||from>=(int)sections.size()||to>=(int)sections.size()||from==to) return;
    const bool wholeSong=loopStart==0&&loopEnd==length();
    const double oldStart=sectionStart(from),oldEnd=sectionStart(from+1),destination=from<to?sectionStart(to+1)-(oldEnd-oldStart):sectionStart(to);
    for(auto& t:tracks){for(auto& point:t.volume.points){if(point.beat>=oldStart&&point.beat<oldEnd)point.beat+=destination-oldStart;else if(from<to&&point.beat>=oldEnd&&point.beat<destination+oldEnd-oldStart)point.beat-=oldEnd-oldStart;else if(from>to&&point.beat>=destination&&point.beat<oldStart)point.beat+=oldEnd-oldStart;}t.volume.normalise(length());}
    auto moveMarker=[&](double beat,bool ending){
        const double probe=ending?std::nextafter(beat,-INFINITY):beat;
        if(probe>=oldStart&&probe<oldEnd)return beat+destination-oldStart;
        if(from<to&&probe>=oldEnd&&probe<destination+oldEnd-oldStart)return beat-(oldEnd-oldStart);
        if(from>to&&probe>=destination&&probe<oldStart)return beat+oldEnd-oldStart;
        return beat;
    };
    loopStart=moveMarker(loopStart,false);loopEnd=moveMarker(loopEnd,true);
    auto s=sections[(size_t)from]; sections.erase(sections.begin()+from); sections.insert(sections.begin()+to,s);
    for(auto& t:tracks) {auto c=std::move(t.clips[(size_t)from]); t.clips.erase(t.clips.begin()+from); t.clips.insert(t.clips.begin()+to,std::move(c));}
    if(wholeSong){loopStart=0;loopEnd=length();}
    normalise();
}
void Project::resizeSection(int index,int bars){
    if(index<0||index>=(int)sections.size())return;
    bars=std::clamp(bars,1,128);const double start=sectionStart(index),end=sectionStart(index+1);
    const double duration=bars*4.,delta=duration-(end-start);
    auto map=[&](double beat){return beat>=end?beat+delta:beat>start?start+(beat-start)*duration/(end-start):beat;};
    for(auto& t:tracks)for(auto& point:t.volume.points)point.beat=map(point.beat);
    loopStart=map(loopStart);loopEnd=map(loopEnd);sections[(size_t)index].bars=bars;normalise();
}
bool transferClip(Project& p,int track,int from,int to,bool copy){
    if(track<0||track>=(int)p.tracks.size()||from<0||to<0||from>=(int)p.sections.size()||to>=(int)p.sections.size()||from==to)return false;
    auto& clips=p.tracks[(size_t)track].clips;
    if(from>=(int)clips.size()||to>=(int)clips.size())return false;
    if(copy)clips[(size_t)to]=clips[(size_t)from];else std::swap(clips[(size_t)from],clips[(size_t)to]);
    tidyNotes(clips[(size_t)to],p.clipLength(track,to));
    if(!copy)tidyNotes(clips[(size_t)from],p.clipLength(track,from));
    return true;
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
    p.normalise();
    p.settings.genre=std::clamp(genre,0,12);
    static const int presets[]{4,2,0,8,17,28,9,0,33,27,29,6,4};
    const auto& profile=genreProfile(p.settings.genre);
    const int feelOffset=(int)std::lround((finite(p.settings.density,.55,0,1)-.55)*4);
    const int takeOffset=(int)(mix(p.settings.seed+(std::uint32_t)p.settings.genre)%3)-1;
    p.settings.bpm=std::clamp(profile.preferredBpm+feelOffset+takeOffset,profile.minBpm,profile.maxBpm);
    p.settings.swing=fgd::genreSwing(p.settings.genre);
    // A named factory groove from the previous genre must not override the new
    // style. Preserve explicitly authored user patterns and all other controls.
    for(size_t ti=0;ti<p.tracks.size();++ti){
        auto& pattern=p.tracks[ti].engine.pattern;
        const int factoryCount=ti<3?(int)sgk::groovePatterns().size():ti==3?(int)fgg::guitarPatterns().size():(int)fgd::drumPatterns().size();
        if(pattern>=0&&pattern<factoryCount)pattern=-1;
    }
    chooseProgression(p,presets[p.settings.genre]);
    // Match each style to an actual named bass line in the original library.
    static const int bassLines[]{8,23,16,11,22,13,18,15,20,12,10,16,12};
    p.settings.bassMode=sgk::bassModeForLine(bassLines[p.settings.genre]);
    applyGenreSound(p);
}
void arrangeSong(Project& p) {
    p.normalise();
    // Phrase lengths and dramatic arcs are specific to the original thirteen styles.
    // Transitions belong to the musical form; the instrument engines shape intro/outro.
    // Mix-in/out development is part of these sections, never appended elsewhere.
    const int genre=std::clamp(p.settings.genre,0,12);
    static const std::vector<SongSection> forms[]{
        {{Section::intro,32,"Intro"},{Section::main,32,"Piano theme"},{Section::breakdown,16,"Piano break"},{Section::buildup,8,"Lift"},{Section::drop,32,"Piano drop"},{Section::main,16,"Reprise"},{Section::outro,32,"Outro"}},
        {{Section::intro,32,"Intro"},{Section::main,32,"Soulful verse"},{Section::main,16,"Answer"},{Section::breakdown,16,"Soul break"},{Section::buildup,8,"Lift"},{Section::drop,32,"Soulful chorus"},{Section::outro,32,"Outro"}},
        {{Section::intro,32,"Intro"},{Section::main,32,"Deep groove"},{Section::breakdown,16,"Atmosphere"},{Section::main,32,"Evolution"},{Section::drop,32,"Deep return"},{Section::outro,32,"Outro"}},
        {{Section::intro,16,"Intro"},{Section::main,32,"Gospel verse"},{Section::drop,16,"Response"},{Section::breakdown,16,"Gospel break"},{Section::buildup,8,"Choir lift"},{Section::drop,32,"Final response"},{Section::outro,16,"Outro"}},
        {{Section::intro,32,"Intro"},{Section::main,32,"Jackin groove"},{Section::breakdown,8,"Quick break"},{Section::drop,32,"Jackin drop"},{Section::main,16,"Drum workout"},{Section::buildup,8,"Lift"},{Section::drop,32,"Final jack"},{Section::outro,32,"Outro"}},
        {{Section::intro,32,"Intro"},{Section::main,32,"Piano jack"},{Section::breakdown,8,"Piano cut"},{Section::buildup,8,"Build"},{Section::drop,32,"Piano drop"},{Section::main,16,"Variation"},{Section::outro,32,"Outro"}},
        {{Section::intro,32,"Intro"},{Section::main,32,"UK shuffle"},{Section::breakdown,16,"Bass break"},{Section::buildup,8,"Reload"},{Section::drop,32,"Garage drop"},{Section::breakdown,8,"Cut"},{Section::drop,32,"Second drop"},{Section::outro,32,"Outro"}},
        {{Section::intro,32,"Percussion intro"},{Section::main,32,"Afro groove"},{Section::main,16,"Call and response"},{Section::breakdown,16,"Percussion break"},{Section::buildup,8,"Rise"},{Section::drop,32,"Tribal return"},{Section::outro,32,"Percussion outro"}},
        {{Section::intro,32,"Disco intro"},{Section::main,32,"Funk verse"},{Section::drop,16,"Disco chorus"},{Section::breakdown,16,"Strings break"},{Section::main,32,"Funk reprise"},{Section::drop,16,"Final chorus"},{Section::outro,32,"Disco outro"}},
        {{Section::intro,32,"Intro"},{Section::main,32,"90s theme"},{Section::breakdown,16,"Piano breakdown"},{Section::buildup,16,"Rave build"},{Section::drop,32,"Anthem drop"},{Section::outro,32,"Outro"}},
        {{Section::intro,16,"Broken intro"},{Section::main,24,"Neo soul verse"},{Section::drop,16,"Chorus"},{Section::breakdown,8,"Bridge"},{Section::main,24,"Second verse"},{Section::drop,16,"Final chorus"},{Section::outro,16,"Broken outro"}},
        {{Section::intro,16,"Chill intro"},{Section::main,24,"First landscape"},{Section::breakdown,16,"Space"},{Section::main,24,"Second landscape"},{Section::drop,16,"Warm return"},{Section::outro,16,"Chill outro"}},
        {{Section::intro,32,"Intro"},{Section::main,32,"Stab theme"},{Section::breakdown,16,"Breakdown"},{Section::buildup,16,"Tension"},{Section::drop,32,"Stab drop"},{Section::main,16,"Reprise"},{Section::outro,32,"Outro"}}
    };
    auto sections=forms[genre];
    // A phrase resolves both the harmonic cycle and the selected groove. Longer
    // evolving motifs need more room; dense material answers sooner. This changes
    // musical repeats, never stretches a form to an arbitrary clock duration.
    const auto& piano=p.tracks[0].engine;
    const auto& kit=p.tracks[4].engine;
    const auto& guitar=p.tracks[3].engine;
    int motif=std::max({piano.grooveBars,kit.grooveBars,guitar.grooveBars});
    double activity=piano.density>=0?piano.density:p.settings.density;
    // Resolve automatic, named and user grooves through the same original
    // pattern used for rendering, including custom step edits and groove length.
    const auto pattern=sgk::GrooveEngine::basePattern(pianoEngineSettings(p,0,p.settings.seed));
    motif=std::max(motif,(pattern.length()+15)/16);
    int hits=0;for(char c:pattern.rightHand)if(c!='.'&&c!='=')++hits;
    activity=.5*activity+.5*hits/std::max(1,pattern.length());
    if(kit.pattern>=0&&kit.pattern<(int)fgd::drumPatterns().size())
        motif=std::max(motif,(fgd::drumPatterns()[(size_t)kit.pattern].length()+15)/16);
    if(guitar.pattern>=0&&guitar.pattern<(int)fgg::guitarPatterns().size())
        motif=std::max(motif,(fgg::guitarPatterns()[(size_t)guitar.pattern].length()+15)/16);
    // Bass/strings and the individual drum stems can also carry a longer motif;
    // the form must not listen only to Piano, Guitar and Kick.
    int evolution=piano.evolveBars;
    for(size_t ti=0;ti<p.tracks.size();++ti){
        const auto& engine=p.tracks[ti].engine;
        motif=std::max(motif,engine.grooveBars);
        if(ti<3){
            evolution=std::max(evolution,engine.evolveBars);
            if(engine.pattern>=0&&engine.pattern<(int)sgk::groovePatterns().size())
                motif=std::max(motif,(sgk::groovePatterns()[(size_t)engine.pattern].length()+15)/16);
        }else if(ti>=4&&engine.pattern>=0&&engine.pattern<(int)fgd::drumPatterns().size())
            motif=std::max(motif,(fgd::drumPatterns()[(size_t)engine.pattern].length()+15)/16);
    }
    const int harmonicBars=std::max(1,(p.settings.chordCount*p.settings.chordSteps+15)/16);
    // A DJ phrase must remain a multiple of eight bars, even with five/seven
    // chords or a three-bar groove. The old fallback used harmonicBars alone
    // and produced e.g. 35-bar sections. Large common cycles continue across
    // section boundaries instead of turning every intro into a huge section.
    const int common=std::lcm(std::lcm(8,harmonicBars),motif);
    const int phrase=common<=64?common:8;
    const int development=(motif>=4?1:0)+(evolution>=16?1:0)+(activity<.35?1:0);
    const auto& profile=genreProfile(genre);
    for(auto& section:sections){
        if(section.type==Section::main||section.type==Section::drop){
            const int repeats=(section.bars+phrase-1)/phrase+development;
            section.bars=std::clamp(repeats*phrase,phrase,(128/phrase)*phrase);
        }else if(section.type==Section::intro||section.type==Section::outro){
            // Enough rhythmic runway, sized in phrases, not to a clock target.
            section.bars=profile.mixingBars==16&&motif>=4?32:profile.mixingBars;
        }
    }
    std::vector<SongSection> bounded;
    for(auto section:sections){int part=1;while(section.bars>128){bounded.push_back({section.type,128,section.name+" "+std::to_string(part++)});section.bars-=128;}if(part>1)section.name+=" "+std::to_string(part);bounded.push_back(std::move(section));}
    p.sections=std::move(bounded);for(auto& t:p.tracks)t.clips.assign(p.sections.size(),{});
    p.normalise();p.loopStart=0;p.loopEnd=p.length();
}
Project newProject(int genre) {
    Project p;
    // Balanced genre-specific guide levels leave summing headroom. Saved
    // projects keep their own master and mix; new songs use the curated balance.
    p.masterGain=.45;
    applyGenre(p,genre);arrangeSong(p);generate(p);return p;
}
std::vector<std::string> chordNames(const Settings& s) {
    auto settings=pianoSettings(s,s.seed); std::vector<std::string> out;
    for(int i=0;i<s.chordCount;++i) out.push_back(sgk::chordName(sgk::GrooveEngine::chordForSlot(settings,i)));
    return out;
}
namespace {
// Entrances measured on a 32-bar club intro. The outro reverses these layers.
// Shorter intros scale in complete eight-bar phrases. Rhythmic identities still
// come exclusively from the three original genre engines.
int mixEntry(int genre,int track,int bars) {
    static constexpr int harmony[13][4]{
        {16,24,24,24},{16,24,24,24},{24,16,16,24},{16,32,24,24},
        {24,16,32,24},{16,24,32,24},{24,24,16,24},{24,24,16,24},
        {24,16,24,16},{16,24,24,24},{16,32,24,24},{16,32,16,24},
        {24,24,32,24}
    };
    int entry=0;
    if(track<4)entry=harmony[genre][track];
    else{
        const int lane=track-4;
        // Kick, closed hats and shaker provide a countable pulse from bar one.
        static constexpr int drums[]{0,16,8,16,0,8,16,0,8,8,16,24,32};
        entry=drums[lane];
        if(genre==7&&(lane==7||lane==9))entry=0; // Afro shaker/congas lead
    }
    return std::clamp(((entry*bars/32+7)/8)*8,0,bars);
}
void shapeBandRoles(Clip& clip,const Project& p,int track,int section) {
    const auto& sec=p.sections[(size_t)section];const int genre=p.settings.genre;
    if(sec.bars<8||(sec.type!=Section::main&&sec.type!=Section::drop))return;
    const auto& e=p.tracks[(size_t)track].engine;
    const int user=track<3?sgk::userPatternIndex():track==3?fgg::userPatternIndex():fgd::userPatternIndex();
    if(e.pattern==user)return;
    auto active=[&](double start){const int bar=(int)(start/4),phrase=bar%8;
        if(track==2&&sec.type==Section::main)return genre==8?phrase>=4:genre==11?phrase<6:phrase>=6;
        if(track==3&&!p.settings.guitarLead){
            if(genre==4)return phrase%2==1; // percussive guitar answers the chopped keys
            if(genre==7)return phrase%4>=2; // percussion/guitar answers
            if(genre==2||genre==9||genre==12)return phrase>=6;
            return phrase%4>=2; // leave the first half to the piano
        }
        // Different percussion layers announce phrases; never gate the kick,
        // backbeat or closed hat. These are compositional rests, not audio mute.
        if(track==10||track==12||track==14||track==15)return genre==7?phrase>=2:phrase>=4;
        return true;
    };
    auto& notes=clip.notes;notes.erase(std::remove_if(notes.begin(),notes.end(),[&](const Note& n){return !active(n.start);}),notes.end());
    for(auto& n:notes){const double next=(std::floor(n.start/4)+1)*4;for(double at=next;at<n.start+n.length;at+=4)if(!active(at)){n.length=at-n.start;break;}}
}
void shapeMixSection(Clip& clip,int genre,int track,Section type,int bars) {
    if(type!=Section::intro&&type!=Section::outro)return;
    const double entrance=mixEntry(genre,track,bars)*4.;
    const double cutoff=bars*4.-entrance;
    auto& notes=clip.notes;
    notes.erase(std::remove_if(notes.begin(),notes.end(),[&](const Note& n){
        return type==Section::intro?n.start<entrance:n.start>cutoff-1./960.;
    }),notes.end());
    for(auto& n:notes){
        if(type==Section::outro)n.length=std::min(n.length,cutoff-n.start);
    }
}
}
void generate(Project& p,Scope scope,int track,int section,std::uint32_t seed) {
    p.normalise();
    if(track<0||track>=trackCount||section<0||section>=(int)p.sections.size()) throw std::out_of_range("Generation selection");
    if(seed&&scope==Scope::song)p.settings.seed=seed;
    const auto& s=p.settings;
    const auto& profile=genreProfile(s.genre);
    for(int ti=0;ti<trackCount;++ti) {
        auto& t=p.tracks[(size_t)ti];
        if(t.locked||((scope==Scope::track||scope==Scope::clip)&&ti!=track)) continue;
        for(size_t si=0;si<p.sections.size();++si) {
            if((scope==Scope::section||scope==Scope::clip)&&(int)si!=section) continue;
            auto& clip=t.clips[si]; if(clip.locked) continue;
            if(seed) clip.seed=ti>=4?mix(seed+313U):mix(seed+(std::uint32_t)ti*131U+(std::uint32_t)si*977U);
            else if(clip.seed==1) clip.seed=ti>=4?mix(s.seed+313U):mix(s.seed+(std::uint32_t)ti*131U+(std::uint32_t)si*977U);
            clip.loopSource.clear();clip.loopLength=0;
            const double beats=p.clipLength(ti,(int)si);const int renderBars=(int)std::ceil(beats/4.); const auto type=p.sections[si].type;
            const auto& e=t.engine;
            auto ps=pianoEngineSettings(p,ti,clip.seed);
            const bool mixing=type==Section::intro||type==Section::outro;
            // Soulful/gospel/afro lifts keep the groove rather than switching to
            // the plugin's generic accelerating snare/piano build pattern.
            const auto engineType=type==Section::buildup&&!profile.rollingBuild?Section::main:type;
            const auto rhythmType=mixing?Section::main:engineType;
            const int chordOffset=(int)(p.sectionStart((int)si)/(s.chordSteps*.25))%s.chordCount;
            for(auto* row:{&ps.progression,&ps.freeChords,&ps.inversions})
                std::rotate(row->begin(),row->begin()+chordOffset,row->begin()+s.chordCount);
            if(mixing){ps.fillEveryBars=0;ps.variationEvery=0;}
            if(ti<3) {
                ps.section=(sgk::Section)(ti==1?rhythmType:engineType); ps.sectionVariation=clip.seed;
                auto all=sgk::GrooveEngine::renderArrangement(ps,{{ps.section,renderBars,clip.seed,-1}});
                if(ti==0&&s.guitarLead)all=PianoComping::comping(all,clip.seed*7919u+1u,ps,beats);
                clip.notes=notesFrom(all,beats,[ti](const sgk::NoteEvent& e){return ti==0?(e.part==sgk::Part::chords||e.part==sgk::Part::fill):ti==1?e.part==sgk::Part::bass:e.part==sgk::Part::strings;});
            } else if(ti==3) {
                fgg::GuitarSettings gs; gs.pattern=e.pattern>=0&&e.pattern<(int)fgg::guitarPatterns().size()?e.pattern:fgg::randomPatternIndex(); gs.randomGenre=s.genre;
                if(s.genre==4&&e.pattern<0){
                    const auto& patterns=fgg::guitarPatterns();
                    for(size_t i=0;i<patterns.size();++i)if(patterns[i].name=="Chicago Chank"){gs.pattern=(int)i;break;}
                }
                gs.riffSeed=e.riffSeed?e.riffSeed:mix(s.seed+393U);gs.stepsPerChord=s.chordSteps;
                gs.density=ps.riffDensity; gs.swing=ps.swing; gs.humanize=ps.humanize; gs.humanizeSeed=e.humanizeSeed?e.humanizeSeed:clip.seed;
                gs.tempo=s.bpm; gs.roomForBand=true; gs.numChords=s.chordCount; gs.fillEveryBars=e.fillEvery; gs.variationEvery=e.variationEvery; gs.fillStyle=(fgg::FillStyle)(e.fillStyle<0?(int)fgg::FillStyle::mix:std::min(e.fillStyle,fgg::numFillStyles-1));
                gs.velocity=e.velocity;gs.accentGroove=(float)e.accent;gs.strumMs=(float)(e.strum*80);gs.grooveBars=e.grooveBars;gs.segmentOrder=e.segments;gs.letRing=e.letRing;gs.roomForBand=e.roomForBand;gs.pushChanges=e.pushChanges;gs.strumAfterFill=e.afterFill;
                for(size_t l=0;l<12;++l){gs.levels[l]=(float)e.laneLevel[l];gs.laneSwing[l]=(float)e.laneSwing[l];gs.laneShift[l]=(float)e.laneShift[l];gs.octaves[l]=e.laneOctave[l];gs.muted[l]=e.laneMute[l];}
                for(int i=0;i<s.chordCount;++i) gs.chords[(size_t)i]=s.freeChords[(size_t)i]>=0?fgg::codeChord(s.key,s.freeChords[(size_t)i],std::min(s.extension,2)):fgg::diatonicChord(s.key,s.scale,s.degrees[(size_t)i],std::min(s.extension,2));
                std::rotate(gs.chords.begin(),gs.chords.begin()+chordOffset,gs.chords.begin()+s.chordCount);
                if(mixing){gs.fillEveryBars=0;gs.variationEvery=0;}
                auto all=fgg::GrooveEngine::renderArrangement(gs,{{(fgg::Section)engineType,renderBars,clip.seed,-1}});
                clip.notes=notesFrom(all,beats,[](const fgg::NoteEvent& e){return !e.isController()&&e.lane<fgg::numLanes;});
                // Articulation lanes are carried directly by notesFrom, in linear time.
            } else {
                fgd::DrumSettings ds; ds.pattern=e.pattern>=0&&e.pattern<(int)fgd::drumPatterns().size()?e.pattern:fgd::randomPatternIndex(); ds.randomGenre=s.genre;
                // One kit seed for all drum stems keeps kick, backbeat and hats coherent.
                ds.riffSeed=e.riffSeed?e.riffSeed:mix(s.seed+313U); ds.sectionVariation=mix(clip.seed+(std::uint32_t)si*977U);
                ds.density=ps.riffDensity; ds.swing=ps.swing; ds.humanize=ps.humanize; ds.humanizeSeed=e.humanizeSeed?e.humanizeSeed:clip.seed;
                ds.tempo=s.bpm; ds.bassSpace=e.bassSpace; ds.fillEveryBars=e.fillEvery; ds.variationEvery=e.variationEvery; ds.fillStyle=(fgd::FillStyle)(e.fillStyle<0?(int)fgd::FillStyle::mix:std::min(e.fillStyle,fgd::numFillStyles-1));
                ds.velocity=e.velocity;ds.accentGroove=(float)e.accent;ds.genreFeel=e.genreFeel;ds.crashAfterFill=e.afterFill;ds.grooveBars=e.grooveBars;ds.segmentOrder=e.segments;
                ds.laneSwing[(size_t)(ti-4)]=(float)e.laneSwing[0];ds.laneShift[(size_t)(ti-4)]=(float)e.laneShift[0];
                if(e.midiNote>=0)ds.notes[(size_t)(ti-4)]=e.midiNote;
                if(mixing){ds.fillEveryBars=0;ds.variationEvery=0;ds.crashAfterFill=false;}
                auto all=fgd::GrooveEngine::renderArrangement(ds,{{(fgd::Section)rhythmType,renderBars,ds.sectionVariation,-1}});

                clip.notes=notesFrom(all,beats,[ti](const fgd::NoteEvent& e){return e.lane==ti-4;});
            }
            if(ti>=4)for(auto& n:clip.notes){n.lane=ti-4;n.pitch=e.midiNote>=0?e.midiNote:fgd::defaultNote(ti-4);}
            shapeBandRoles(clip,p,ti,(int)si);
            shapeMixSection(clip,s.genre,ti,type,renderBars);
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
        double occupied=0;
        for(size_t si=0;si<t.clips.size()&&si<p.sections.size();++si) {
            const double begin=p.sectionStart((int)si),end=p.clipLength((int)ti,(int)si),from=std::max(0.,occupied-begin);
            occupied=std::max(occupied,begin+end);if(from>=end)continue;
            if(ti==0&&t.engine.pedalCC&&!t.clips[si].notes.empty()){
                const double chord=p.settings.chordSteps*.25;
                for(double at=0;at<end;at+=chord){if(at<from)continue;out.push_back({begin+at,(int)ti,64,0,0,64});out.push_back({begin+at+.02,(int)ti,64,127,0,64});}
                for(double at=0;at<end;at+=.25){if(at<from)continue;const int expression=p.sections[si].type==Section::buildup?std::clamp((int)(60+67*at/end),0,127):127;out.push_back({begin+at,(int)ti,11,expression,0,11});}
                if(from>0){out.push_back({begin+from,(int)ti,64,0,0,64});out.push_back({begin+from,(int)ti,11,127,0,11});}
                out.push_back({begin+end,(int)ti,64,0,0,64});out.push_back({begin+end,(int)ti,11,127,0,11});
            }
            for(const auto& n:t.clips[si].notes) {
                if(n.start>=end||n.start+n.length<=from)continue;
                out.push_back({begin+std::max(from,n.start),(int)ti,n.pitch,n.velocity,n.lane});
                out.push_back({begin+std::min(end,n.start+n.length),(int)ti,n.pitch,0,n.lane});
            }
        }
    }
    std::stable_sort(out.begin(),out.end(),[](const TimedEvent& a,const TimedEvent& b){if(a.beat!=b.beat)return a.beat<b.beat;if((a.controller>=0)!=(b.controller>=0))return a.controller>=0;return a.velocity<b.velocity;});return out;
}
}
