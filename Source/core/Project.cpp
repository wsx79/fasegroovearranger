// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Project.h"
#include "GenreSound.h"
#include "../../ThirdParty/fasegroovepiano/Source/engine/Comping.h"
#include "../../ThirdParty/fasegroovepiano/Source/engine/GrooveEngine.h"
#include "../../ThirdParty/fasegroovedrums/Source/engine/GrooveEngine.h"
#include "../../ThirdParty/fasegrooveguitar/Source/engine/GrooveEngine.h"
#include "../../ThirdParty/fasegroovelead/Source/engine/GrooveEngine.h"
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
    // The groove's genre writes the figure. House, '90s house and dance stabs stay on the
    // classic loops. A saved library preset or hand-edited grid still replaces this below.
    const float density=(float)std::clamp(s.density,0.0,1.0);
    p.riffGenre=s.genre;
    auto pattern=sgk::makeRandomPattern(seed,density,s.genre);
    if(s.density<.35) pattern=sgk::applyIntention(pattern,sgk::Intention::minimal,seed);
    if(s.density>.72) pattern=sgk::addGhostNotes(pattern,seed);
    sgk::repairHolds(pattern.rightHand);sgk::repairHolds(pattern.leftHand);
    if(pattern.rightHand.empty())pattern.rightHand="x...";
    if(pattern.leftHand.empty())pattern.leftHand="x...";
    p.pattern=sgk::userPatternIndex();
    for(int i=0;i<sgk::userPatternLength;++i) {
        p.userRight[(size_t)i]=pattern.rightHand[(size_t)i%pattern.rightHand.size()];
        p.userLeft[(size_t)i]=pattern.leftHand[(size_t)i%pattern.leftHand.size()];
    }
    return p;
}
// Compare attacks, not accents, held ticks or humanisation: another take should
// be a recognisably different rhythmic figure from the same genre vocabulary.
std::uint32_t pianoAttacks(const sgk::GroovePattern& pattern) {
    std::uint32_t bits=0;if(pattern.rightHand.empty())return bits;
    for(int i=0;i<32;++i){const char c=pattern.rightHand[(size_t)i%pattern.rightHand.size()];if(c!='.'&&c!='='&&c!='g')bits|=1U<<i;}
    return bits;
}
int bitCount(std::uint32_t bits){int count=0;for(;bits;bits&=bits-1)++count;return count;}
std::uint32_t freshPianoMotif(const Settings& settings,const sgk::GroovePattern& previous,std::uint32_t seed){
    const auto old=pianoAttacks(previous);if(!old)return seed;
    auto best=seed;double bestScore=-1;
    // Draw only from the plugin's own genre generator. Keep its timing cells,
    // harmonic anchors and density, rather than scattering arbitrary extra hits.
    for(unsigned attempt=0;attempt<32;++attempt){
        const auto candidate=attempt?mix(seed+attempt*0x9e3779b9U):seed;
        const auto attacks=pianoAttacks(sgk::GrooveEngine::basePattern(pianoSettings(settings,candidate)));
        const int changes=bitCount(old^attacks),all=bitCount(old|attacks);
        const double contrast=all?(double)changes/all:0;
        const double score=changes+contrast;
        if(score>bestScore){best=candidate;bestScore=score;}
        if(changes>=6&&contrast>=.35)return candidate;
    }
    return best;
}
// MIDI clips have a one-tick minimum duration. Very close humanized attacks must
// not turn that minimum into a second lead voice; keep the later attack instead.
void monophonicLead(Clip& clip){
    constexpr double tick=1./960.;size_t written=0;
    for(size_t read=0;read<clip.notes.size();++read){const Note note=clip.notes[read];
        while(written>0&&clip.notes[written-1].start+clip.notes[written-1].length>note.start){
            auto& previous=clip.notes[written-1];const double length=note.start-previous.start;
            if(length<tick){--written;continue;}previous.length=length;break;
        }
        clip.notes[written++]=note;
    }
    clip.notes.resize(written);
}
template<class E, class Filter> std::vector<Note> notesFrom(const std::vector<E>& stream, double beats, Filter filter) {
    // Note-offs pair per engine voice (piano part, guitar/drum lane): the same pitch in two
    // voices (a fill over a held chord) must not let one voice's note-off cut the other's
    // note to a millisecond stub. tidyNotes() then coalesces overlaps for the one MIDI channel.
    constexpr bool laned=std::is_same_v<E,fgl::NoteEvent>||std::is_same_v<E,fgg::NoteEvent>||std::is_same_v<E,fgd::NoteEvent>;
    constexpr int voices=32;
    constexpr double stub=1./32.;
    std::array<int,128*voices> active; active.fill(-1);
    std::array<int,128> latest; latest.fill(-1);
    std::vector<Note> notes;notes.reserve(stream.size()/2);std::vector<char> closed;closed.reserve(stream.size()/2);
    auto close=[&](int index,double length){auto& n=notes[(size_t)index];n.length=closed[(size_t)index]?std::max(n.length,length):length;closed[(size_t)index]=1;};
    for(const auto& e:stream) {
        if(!filter(e)||e.note<0||e.note>127) continue;
        int voice=0;if constexpr(laned)voice=e.lane;else voice=(int)e.part;
        auto& index=active[(size_t)e.note*voices+(size_t)std::clamp(voice,0,voices-1)];
        if(e.isNoteOn()) {
            // A unison inside a strum (another string or lane, the chord and a fill) lands a few ms
            // after the first one: one attack, not a flam or a millisecond stub. Drum flams stay.
            if constexpr(!std::is_same_v<E,fgd::NoteEvent>&&!std::is_same_v<E,fgl::NoteEvent>){const int last=latest[(size_t)e.note];
                if(last>=0&&e.ppq>=notes[(size_t)last].start&&e.ppq-notes[(size_t)last].start<stub){
                    auto& n=notes[(size_t)last];n.velocity=std::max(n.velocity,std::clamp(e.velocity,1,127));
                    if(index>=0&&index!=last)close(index,std::max(.001,e.ppq-notes[(size_t)index].start));
                    index=last;continue;}}   // its note-off now ends the merged note
            if(index>=0) close(index,std::max(.001,e.ppq-notes[(size_t)index].start));
            const double start=std::clamp(e.ppq,0.0,beats);
            if(start>=beats) {index=-1;continue;}
            int lane=0;if constexpr(laned)lane=e.lane;
            notes.push_back({start,.25,e.note,std::clamp(e.velocity,1,127),lane});closed.push_back(0);index=(int)notes.size()-1;latest[(size_t)e.note]=index;
        } else if(index>=0) {
            // A note humanised a hair before the section end belongs to the next bar: clipped
            // there it would only be a millisecond click, so it is dropped.
            const auto& n=notes[(size_t)index];
            close(index,e.ppq>=beats-1e-9&&n.start>beats-stub?-1.:std::max(.001,std::min(beats,e.ppq)-n.start)); index=-1;
        }
    }
    for(int i:active) if(i>=0&&!closed[(size_t)i]) {auto& n=notes[(size_t)i];n.length=n.start>beats-stub?-1.:std::max(.001,beats-n.start);closed[(size_t)i]=1;}
    notes.erase(std::remove_if(notes.begin(),notes.end(),[](const Note& n){return n.length<0;}),notes.end());
    return notes;
}
}
sgk::GrooveSettings pianoEngineSettings(const Project& p,int track,std::uint32_t seed,std::uint32_t take){
    const auto& s=p.settings;const auto& e=p.tracks[(size_t)track].engine;auto trackSettings=s;if(e.density>=0)trackSettings.density=e.density;
    // A regenerated track/song shares its musical take across sections. Legacy
    // projects retain the song motif until explicitly regenerated.
    const auto motifSeed=take?take:mix(s.seed+(std::uint32_t)track*131U);
    auto ps=pianoSettings(trackSettings,e.riffSeed?e.riffSeed:motifSeed);
    // Develop the selected bass figure without replacing its groove or its strong anchors.
    // Root/fifth/octave substitutions stay inside the current chord; pickups never
    // overwrite held notes. A take loops coherently through the song's sections.
    if(track==1&&(take||e.riffSeed)&&sgk::isBassLine(ps.bass)){
        auto line=sgk::bassLines()[(size_t)sgk::bassLineIndex((int)ps.bass)].line;
        const auto source=line;
        for(size_t i=1;i<line.size();++i){
            const auto choice=mix((e.riffSeed?e.riffSeed:take)+(std::uint32_t)i*7919U);
            const auto barStart=source.begin()+(i/16)*16;
            const bool firstTone=std::none_of(barStart,source.begin()+i,[](char c){return c!='.'&&c!='='&&c!='G';});
            if(firstTone||i%8==0||e.segmentLocked[(i/8)%4]||source[i]=='=')continue;
            if(source[i]=='R'||source[i]=='O'||source[i]=='5'){
                static constexpr char tones[]{'R','O','5'};
                line[i]=tones[choice%3];
                if(i%4!=0&&source[(i+1)%source.size()]!='='&&choice%3==0)line[i]='.';
            }else if(source[i]=='.'&&source[i-1]!='='&&source[(i+1)%source.size()]!='='&&choice%4==0)line[i]=(choice&8)?'R':'5';
        }
        ps.bassLineOverride=std::move(line);
    }
    ps.humanizeSeed=e.humanizeSeed?e.humanizeSeed:seed;
    ps.swing=(float)(e.swing<0?s.swing:e.swing);ps.humanize=(float)(e.humanize<0?s.humanize:e.humanize);ps.riffDensity=(float)(e.density<0?s.density:e.density);
    ps.velocity=e.velocity;ps.gate=(float)e.gate;ps.strum=(float)e.strum;ps.centre=e.centre;ps.movement=e.movement;ps.octaveTop=e.octaveTop;
    ps.strumDirection=e.strumDirection;ps.topLine=(sgk::TopLine)e.topLine;ps.approach=(sgk::ApproachStyle)e.approach;ps.evolveBars=e.evolveBars;ps.humanizeStyle=e.humanizeStyle;ps.bassFeel=e.bassFeel;
    ps.rangeLow=e.rangeLow;ps.rangeHigh=e.rangeHigh;ps.realism=e.realism;ps.grooveBars=e.grooveBars;ps.segmentOrder=e.segments;
    ps.fillEveryBars=e.fillEvery;ps.fillStyle=(sgk::FillStyle)(e.fillStyle<0?(int)sgk::FillStyle::mix:std::min(e.fillStyle,sgk::numFillStyles-1));ps.variationEvery=e.variationEvery;
    ps.stepsPerChord=s.chordSteps;ps.inversions=e.inversions;ps.velocityOrder=e.velocityOrder;
    // Piano and bass are separate tracks: a shared pitch must not cut or steal the other's note.
    ps.separateBass=true;
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

int trackAtRow(int row) { static constexpr int order[]{4,5,6,7,8,9,10,11,12,13,14,15,16,1,0,2,3,leadTrack}; return row>=trackCount?std::clamp(row,trackCount,maxTracks-1):order[std::clamp(row,0,trackCount-1)]; }
double defaultDrumNoteLength(int track){
    // Same gates as fasegroovedrums::GrooveEngine::noteLength (in beats).
    if(!isDrumTrack(track))return .25;
    switch((fgd::Lane)(track-4)){
        case fgd::Lane::closedHat:case fgd::Lane::shaker:case fgd::Lane::tambourine:return .225;
        case fgd::Lane::openHat:return .5;
        case fgd::Lane::crash:return 2.;
        default:return 1.;
    }
}
int rowForTrack(int track) { if(isManualTrack(track))return track;for(int i=0;i<trackCount;++i)if(trackAtRow(i)==track)return i;return 0; }
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
        {120,127,125,32,false}, // Jackin': 120–127, shuffle not speed garage
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
    if(track>=0&&track<(int)tracks.size()&&tracks[(size_t)track].freeTimeline){const auto& clips=tracks[(size_t)track].clips;return section>=0&&section<(int)clips.size()?clips[(size_t)section].span:0;}
    if(section<0||section>=(int)sections.size())return 0;
    if(track>=0&&track<(int)tracks.size()&&section<(int)tracks[(size_t)track].clips.size()){
        const double span=tracks[(size_t)track].clips[(size_t)section].span;if(std::isfinite(span)&&span>0)return std::clamp(span,.25,512.);
    }
    return sections[(size_t)section].bars*4.;
}
double Project::length() const {
    // Running section starts: length() runs on every timer tick and paint.
    std::vector<double> starts(sections.size()+1,0.);for(size_t i=0;i<sections.size();++i)starts[i+1]=starts[i]+sections[i].bars*4.0;
    double end=starts.back();
    for(size_t ti=0;ti<tracks.size();++ti)for(size_t si=0;si<tracks[ti].clips.size();++si)
        if(tracks[ti].clips[si].span>0)end=std::max(end,(tracks[ti].freeTimeline?tracks[ti].clips[si].position:starts[std::min(si,sections.size())])+clipLength((int)ti,(int)si));
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
    if(track<0||track>=(int)tracks.size()||section<0||section>=(int)tracks[(size_t)track].clips.size()||!std::isfinite(beats))return false;
    beats=std::clamp(std::round(beats*960)/960.,.25,512.);auto candidate=tracks[(size_t)track].clips[(size_t)section];
    if(!resizedLoop(candidate,clipLength(track,section),beats))return false;
    size_t count=candidate.notes.size()+candidate.loopSource.size();if(count>2000000)return false;
    for(size_t ti=0;ti<tracks.size();++ti)for(size_t si=0;si<tracks[ti].clips.size();++si)if((int)ti!=track||(int)si!=section){const auto& c=tracks[ti].clips[si];count+=c.notes.size()+c.loopSource.size();if(count>2000000)return false;}
    const double before=length();const bool wholeLoop=loopStart==0&&loopEnd==before;
    candidate.span=tracks[(size_t)track].freeTimeline?beats:std::abs(beats-sections[(size_t)section].bars*4.)<1./960.?0:beats;
    if(tracks[(size_t)track].freeTimeline){auto edited=*this;const double at=candidate.position;edited.tracks[(size_t)track].clips.erase(edited.tracks[(size_t)track].clips.begin()+section);const int placed=edited.placeClip(track,std::move(candidate),at);if(placed<0)return false;if(wholeLoop)edited.loopEnd=edited.length();*this=std::move(edited);return true;}
    tracks[(size_t)track].clips[(size_t)section]=std::move(candidate);if(wholeLoop)loopEnd=length();normalise();return true;
}
bool Project::resizeSectionLoop(int index,int bars){
    if(index<0||index>=(int)sections.size()||std::any_of(tracks.begin(),tracks.end(),[](const Track& t){return t.freeTimeline;}))return false;bars=std::clamp(bars,1,128);
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
    s.bpm=finite(s.bpm,tempo.preferredBpm,s.freeTempo?30:tempo.minBpm,s.freeTempo?300:tempo.maxBpm); s.swing=finite(s.swing,.2,0,1.5); s.density=finite(s.density,.55,0,1); s.humanize=finite(s.humanize,.2,0,1);
    s.chordCount=std::clamp(s.chordCount,1,8); s.bassMode=std::clamp(s.bassMode,0,sgk::numBassModes-1); s.stringsMode=std::clamp(s.stringsMode,0,10);
    for(auto& d:s.degrees) d=std::clamp(d,0,6);
    for(auto& c:s.freeChords) c=std::clamp(c,-1,35);
    if(sections.empty()) sections.push_back({Section::main,8,"Main"});
    if(sections.size()>64) sections.resize(64);
    for(auto& p:sections) {p.bars=std::clamp(p.bars,1,128); p.type=(Section)std::clamp((int)p.type,0,5);}
    if(tracks.size()>maxTracks) tracks.resize(maxTracks);
    while(tracks.size()<trackCount) { Track t; t.kind=(int)tracks.size(); t.name=t.kind==leadTrack?"Lead":t.kind<4?std::array<const char*,4>{"Piano","Bass","Strings","Guitar"}[(size_t)t.kind]:fgd::laneName(t.kind-4); if(t.kind==leadTrack){t.gain=.22;t.engine.density=.42;t.engine.roomForBand=false;t.engine.fillEvery=8;} tracks.push_back(t); }
    for(size_t i=0;i<tracks.size();++i) {
        auto& t=tracks[i]; t.kind=(int)i; if(!t.freeTimeline)t.clips.resize(sections.size());else if(t.clips.size()>4096)t.clips.resize(4096);
        t.sampler.normalise();t.group=std::clamp(t.group,-1,1);for(auto& send:t.sends)send=finite(send,0,0,1);if(t.sampleHash.size()>64)t.sampleHash.clear();
        t.gain=finite(t.gain,.75,0,2); t.pan=finite(t.pan,0,-1,1); t.sampleRoot=std::clamp(t.sampleRoot,0,127); t.sound=std::clamp(t.sound,0,6);
        t.volume.normalise(length());t.panAutomation.normalise(length(),1.);
        if(t.parameters.size()>(size_t)maxParameterLanes)t.parameters.resize((size_t)maxParameterLanes);
        for(auto& lane:t.parameters){lane.slot=std::clamp(lane.slot,0,1);lane.index=std::max(-1,lane.index);if(lane.name.size()>128)lane.name.resize(128);lane.curve.normalise(length(),1.);}
        auto& e=t.engine;
        e.globalOctave=std::clamp(e.globalOctave,-2,2);e.pattern=std::clamp(e.pattern,-1,10000);e.velocity=std::clamp(e.velocity,1,127);e.grooveBars=std::clamp(e.grooveBars,1,4);
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
        for(size_t k=0;k<t.clips.size();++k){auto& c=t.clips[k];c.role=std::clamp(c.role,-1,5);if(t.freeTimeline){c.position=finite(c.position,0,0,131072);if(c.role<0)c.role=(int)sections[(size_t)sectionAtBeat(c.position)].type;c.span=finite(c.span,4,1./960.,512);}c.span=finite(c.span,0,0,512);if(c.span>0)c.span=std::max(t.freeTimeline?1./960.:.25,c.span);c.loopLength=finite(c.loopLength,0,0,512);if(c.loopLength>0)c.loopLength=std::max(.25,c.loopLength);
            if(isDrumTrack((int)i))for(auto* notes:{&c.notes,&c.loopSource})for(auto& n:*notes){if(t.instrument!="sample")n.pitch=e.midiNote>=0?e.midiNote:fgd::defaultNote((int)i-4);n.lane=(int)i-4;}
            tidyNotes(c,clipLength((int)i,(int)k));if(c.loopLength>0){Clip source;source.notes=std::move(c.loopSource);tidyNotes(source,c.loopLength);c.loopSource=std::move(source.notes);}else c.loopSource.clear();}
        if(t.freeTimeline)std::stable_sort(t.clips.begin(),t.clips.end(),[](const Clip& a,const Clip& b){return a.position<b.position;});
    }
    for(auto& bus:buses)bus.normalise();
    masterGain=finite(masterGain,.8,0,2); masterAutomation.normalise(length()); loopStart=finite(loopStart,0,0,length()); loopEnd=finite(loopEnd,length(),loopStart,length());
    if(loopEnd-loopStart<1./960.)loop=false;
}
bool Project::deleteMidiTrack(int track) {
    if(!isManualTrack(track)||track>=(int)tracks.size())return false;
    tracks.erase(tracks.begin()+track);normalise();return true;
}
int Project::addMidiTrack() {
    normalise();if(tracks.size()>=maxTracks)return -1;
    Track t;t.kind=(int)tracks.size();int number=t.kind-trackCount+1;
    do{t.name="MIDI "+std::to_string(number++);}while(std::any_of(tracks.begin(),tracks.end(),[&](const Track& existing){return existing.name==t.name;}));
    t.freeTimeline=true;t.gain=.6;
    Clip c;c.position=0;c.span=sections.front().bars*4.;c.role=(int)sections.front().type;
    t.clips.push_back(c);tracks.push_back(std::move(t));normalise();return (int)tracks.size()-1;
}
bool Project::loopMatches(double from,double to) const {
    const double a=std::clamp(std::min(from,to),0.,length()),b=std::clamp(std::max(from,to),0.,length());
    return std::abs(loopStart-a)<1e-6&&std::abs(loopEnd-b)<1e-6;
}
void Project::setLoopRegion(double from,double to) {
    loopStart=std::clamp(std::min(from,to),0.,length());loopEnd=std::clamp(std::max(from,to),loopStart,length());
    loop=hasLoopRegion();if(!loop)loopStart=loopEnd=0;
}
bool Project::toggleLoopRegion(double from,double to) {
    if(loop&&loopMatches(from,to)){clearLoop();return false;}
    setLoopRegion(from,to);return loop;
}
bool Project::toggleLoop() {
    if(!hasLoopRegion()){loopStart=0;loopEnd=length();}
    loop=!loop&&hasLoopRegion();return loop;
}
void Project::clearLoop() {loop=false;loopStart=loopEnd=0;}
std::vector<std::pair<VolumeAutomation*,double>> Project::envelopes(){
    std::vector<std::pair<VolumeAutomation*,double>> all{{&masterAutomation,2.}};
    for(auto& t:tracks){all.push_back({&t.volume,2.});all.push_back({&t.panAutomation,1.});for(auto& lane:t.parameters)all.push_back({&lane.curve,1.});}
    return all;
}
void Project::insertSection(int index,SongSection section,int copyFrom) {
    if(sections.size()>=64) return;
    index=std::clamp(index,0,(int)sections.size());
    const bool wholeSong=loopStart==0&&loopEnd==length();
    std::vector<Clip> copies;
    for(auto& t:tracks) copies.push_back(copyFrom>=0&&copyFrom<(int)t.clips.size()?t.clips[(size_t)copyFrom]:Clip{});
    const double start=sectionStart(index),duration=std::clamp(section.bars,1,128)*4.;
    for(auto [envelope,ceiling]:envelopes()){auto& points=envelope->points;auto original=points;for(auto& point:points)if(point.beat>=start)point.beat+=duration;
        if(copyFrom>=0&&copyFrom<(int)sections.size()){const double from=sectionStart(copyFrom),end=sectionStart(copyFrom+1);for(auto point:original)if(point.beat>=from&&point.beat<end&&point.beat-from<duration){point.beat=start+point.beat-from;points.push_back(point);}}
    }
    if(loopStart>=start)loopStart+=duration;
    if(loopEnd>start)loopEnd+=duration;
    sections.insert(sections.begin()+index,std::move(section));
    for(size_t i=0;i<tracks.size();++i) if(!tracks[i].freeTimeline)tracks[i].clips.insert(tracks[i].clips.begin()+index,copies[i]);
    if(wholeSong){loopStart=0;loopEnd=length();}
    normalise();
}
void Project::removeSection(int index) {
    if(sections.size()<2||index<0||index>=(int)sections.size()) return;
    const double start=sectionStart(index),end=sectionStart(index+1);
    for(auto [envelope,ceiling]:envelopes()){auto& points=envelope->points;points.erase(std::remove_if(points.begin(),points.end(),[&](const VolumePoint& p){return p.beat>=start&&p.beat<end;}),points.end());for(auto& point:points)if(point.beat>=end)point.beat-=end-start;}
    auto shift=[&](double b){return b<=start?b:b>=end?b-(end-start):start;};
    loopStart=shift(loopStart);loopEnd=shift(loopEnd);
    sections.erase(sections.begin()+index); for(auto& t:tracks)if(!t.freeTimeline)t.clips.erase(t.clips.begin()+index); normalise();
}
void Project::moveSection(int from,int to) {
    if(from<0||to<0||from>=(int)sections.size()||to>=(int)sections.size()||from==to) return;
    const bool wholeSong=loopStart==0&&loopEnd==length();
    const double oldStart=sectionStart(from),oldEnd=sectionStart(from+1),destination=from<to?sectionStart(to+1)-(oldEnd-oldStart):sectionStart(to);
    for(auto [envelope,ceiling]:envelopes()){for(auto& point:envelope->points){if(point.beat>=oldStart&&point.beat<oldEnd)point.beat+=destination-oldStart;else if(from<to&&point.beat>=oldEnd&&point.beat<destination+oldEnd-oldStart)point.beat-=oldEnd-oldStart;else if(from>to&&point.beat>=destination&&point.beat<oldStart)point.beat+=oldEnd-oldStart;}envelope->normalise(length(),ceiling);}
    auto moveMarker=[&](double beat,bool ending){
        const double probe=ending?std::nextafter(beat,-INFINITY):beat;
        if(probe>=oldStart&&probe<oldEnd)return beat+destination-oldStart;
        if(from<to&&probe>=oldEnd&&probe<destination+oldEnd-oldStart)return beat-(oldEnd-oldStart);
        if(from>to&&probe>=destination&&probe<oldStart)return beat+oldEnd-oldStart;
        return beat;
    };
    loopStart=moveMarker(loopStart,false);loopEnd=moveMarker(loopEnd,true);
    auto s=sections[(size_t)from]; sections.erase(sections.begin()+from); sections.insert(sections.begin()+to,s);
    for(auto& t:tracks) {if(t.freeTimeline)continue;auto c=std::move(t.clips[(size_t)from]); t.clips.erase(t.clips.begin()+from); t.clips.insert(t.clips.begin()+to,std::move(c));}
    if(wholeSong){loopStart=0;loopEnd=length();}
    normalise();
}
void Project::resizeSection(int index,int bars){
    if(index<0||index>=(int)sections.size())return;
    bars=std::clamp(bars,1,128);const double start=sectionStart(index),end=sectionStart(index+1);
    const double duration=bars*4.,delta=duration-(end-start);
    auto map=[&](double beat){return beat>=end?beat+delta:beat>start?start+(beat-start)*duration/(end-start):beat;};
    for(auto [envelope,ceiling]:envelopes())for(auto& point:envelope->points)point.beat=map(point.beat);
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
    // Keep the progression length and any chord chosen by hand: only the diatonic
    // slots are rerolled, from the genre vocabulary rather than four fixed degrees.
    const int count=std::clamp(p.settings.chordCount,1,8);
    const auto& all=sgk::progressionPresets();
    std::vector<int> candidates;
    for(int i=0;i<(int)all.size();++i)if(all[(size_t)i].scale==p.settings.scale)candidates.push_back(i);
    if(candidates.empty()){
        auto degrees=sgk::randomProgression(seed,count,p.settings.scale,p.settings.genre);
        for(int i=0;i<count;++i)if(p.settings.freeChords[(size_t)i]<0)p.settings.degrees[(size_t)i]=degrees[(size_t)i];
        return;
    }
    const int preset=candidates[(size_t)(mix(seed)%candidates.size())];
    const auto slots=sgk::presetSlots(all[(size_t)preset],p.settings.extension);
    if(slots.empty())return;
    const int shift=(int)(mix(seed+17U)%(std::uint32_t)slots.size());
    for(int i=0;i<count;++i){
        if(p.settings.freeChords[(size_t)i]>=0)continue;
        p.settings.degrees[(size_t)i]=std::max(0,slots[(size_t)((i+shift)%(int)slots.size())].degree);
    }
}
void applyGenre(Project& p,int genre) {
    p.settings.freeTempo=false;
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
        const int factoryCount=ti<3?(int)sgk::groovePatterns().size():ti==3?(int)fgg::guitarPatterns().size():ti==leadTrack?(int)fgl::leadPatterns().size():(int)fgd::drumPatterns().size();
        if(pattern>=0&&pattern<factoryCount)pattern=-1;
    }
    chooseProgression(p,presets[p.settings.genre]);
    // Each song takes one of the genre's piano voicings and colours.
    {const auto feel=sgk::genreRiffFeel(p.settings.genre,p.settings.seed);p.settings.voicing=feel.voicing;p.settings.extension=feel.extension;}
    // Match each style to an actual named bass line in the original library.
    static const int bassLines[]{8,23,16,11,22,13,18,15,20,12,10,16,12};
    p.settings.bassMode=sgk::bassModeForLine(bassLines[p.settings.genre]);
    applyGenreSound(p);
}
namespace {
const std::vector<SongSection>& genreForm(int genre) {
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
    return forms[std::clamp(genre,0,12)];
}
// A form nobody restructured: the factory section sequence of some genre.
bool factoryForm(const Project& p,int genre) {
    const auto& form=genreForm(genre);
    if(form.size()!=p.sections.size())return false;
    for(size_t i=0;i<form.size();++i)if(form[i].type!=p.sections[i].type||form[i].name!=p.sections[i].name)return false;
    return true;
}
}
void arrangeSong(Project& p) {
    p.normalise();
    // Phrase lengths and dramatic arcs are specific to the original thirteen styles.
    // Transitions belong to the musical form; the instrument engines shape intro/outro.
    // Mix-in/out development is part of these sections, never appended elsewhere.
    const int genre=std::clamp(p.settings.genre,0,12);
    auto sections=genreForm(genre);
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
        }else if(ti==leadTrack&&engine.pattern>=0&&engine.pattern<(int)fgl::leadPatterns().size())
            motif=std::max(motif,(fgl::leadPatterns()[(size_t)engine.pattern].length()+15)/16);
        else if(isDrumTrack((int)ti)&&engine.pattern>=0&&engine.pattern<(int)fgd::drumPatterns().size())
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
    p.sections=std::move(bounded);for(auto& t:p.tracks)if(!isManualTrack(t.kind)){t.freeTimeline=false;t.clips.assign(p.sections.size(),{});}
    p.normalise();p.loopStart=0;p.loopEnd=p.length();
}
Project newProject(int genre) {
    Project p;
    // Balanced genre-specific guide levels leave summing headroom. Saved
    // projects keep their own master and mix; new songs use the curated balance.
    p.masterGain=.45;
    applyGenre(p,genre);arrangeSong(p);generate(p);p.settings.randomAutomation=true;
    randomiseAutomation(p,Scope::song,0,0,p.settings.seed?p.settings.seed:1u);   // a new song already moves
    return p;
}
std::vector<std::string> chordNames(const Settings& s) {
    auto settings=pianoSettings(s,s.seed); std::vector<std::string> out;
    for(int i=0;i<s.chordCount;++i) out.push_back(sgk::chordName(sgk::GrooveEngine::chordForSlot(settings,i)));
    return out;
}
namespace {
// Entrances measured on a 32-bar club intro. The outro reverses these layers.
// Shorter intros scale in complete eight-bar phrases. Rhythmic identities still
// come exclusively from the four original genre engines.
int mixEntry(int genre,int track,int bars) {
    static constexpr int harmony[13][4]{
        {16,24,24,24},{16,24,24,24},{24,16,16,24},{16,32,24,24},
        {24,16,32,24},{16,24,32,24},{24,24,16,24},{24,24,16,24},
        {24,16,24,16},{16,24,24,24},{16,32,24,24},{16,32,16,24},
        {24,24,32,24}
    };
    int entry=0;
    if(track==leadTrack)entry=24;
    else if(track<4)entry=harmony[genre][track];
    else{
        const int lane=track-4;
        // Kick, closed hats and shaker provide a countable pulse from bar one.
        static constexpr int drums[]{0,16,8,16,0,8,16,0,8,8,16,24,32};
        entry=drums[lane];
        if(genre==7&&(lane==7||lane==9))entry=0; // Afro shaker/congas lead
    }
    int at=std::clamp(((entry*bars/32+7)/8)*8,0,bars);
    // Strings scheduled on the final bar never sound: a 32-bar cue scaled onto
    // a shorter intro lands on the last bar and the whole part is removed.
    // Keep at least eight bars, and half of a longer intro or outro.
    if(track==2&&bars>=8&&at>bars-8){
        const int keep=std::min(bars,std::max(8,(bars/2/8)*8));
        at=bars-keep;
    }
    return at;
}
void shapeBandRoles(Clip& clip,const Project& p,int track,int section) {
    const auto& sec=p.sections[(size_t)section];const int genre=p.settings.genre;
    if(sec.bars<8||(sec.type!=Section::main&&sec.type!=Section::drop))return;
    const auto& e=p.tracks[(size_t)track].engine;
    const int user=track<3?sgk::userPatternIndex():track==3?fgg::userPatternIndex():track==leadTrack?fgl::userPatternIndex():fgd::userPatternIndex();
    if(e.pattern==user)return;
    auto active=[&](double start){const int bar=(int)(start/4),phrase=bar%8;
        // Strings follow the harmony for the whole verse and chorus. Gating them
        // to the last two bars of a phrase removed every chord that did not
        // start in that window, so most songs never grew a string part.
        if(track==3&&!p.settings.guitarLead){
            if(genre==4)return phrase%2==1; // percussive guitar answers the chopped keys
            if(genre==7)return phrase%4>=2; // percussion/guitar answers
            if(genre==2||genre==9||genre==12)return phrase>=6;
            return phrase%4>=2; // leave the first half to the piano
        }
        // Different percussion layers announce phrases; never gate the kick,
        // backbeat or closed hat. These are compositional rests, not audio mute.
        if(track==10||track==12||track==14||track==15){
            if(genre==4&&(track==14||track==15))return true; // cowbell and tom are the jack, not a late answer
            return genre==7?phrase>=2:phrase>=4;
        }
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
        // A hit humanised a hair early belongs to the bar after the boundary: kept at an
        // entrance (on the phrase line), dropped at an outro cut (it would be a stub).
        constexpr double early=1./32.;
        return type==Section::intro?n.start<entrance-early:n.start>cutoff-early;
    }),notes.end());
    for(auto& n:notes){
        if(type==Section::intro&&n.start<entrance){n.length=std::max(1./960.,n.length-(entrance-n.start));n.start=entrance;}
        if(type==Section::outro)n.length=std::min(n.length,cutoff-n.start);
    }
}
}
namespace {
void varySongFeel(Project& p,std::uint32_t seed) {
    auto& s=p.settings;const auto& profile=genreProfile(s.genre);
    const int current=(int)std::lround(s.bpm),span=profile.maxBpm-profile.minBpm;
    if(span>0){int pick=profile.minBpm+(int)(mix(seed+1301U)%(unsigned)span);if(pick>=current)++pick;s.bpm=pick;}
    double density=0.35+(mix(seed+1700U)%46)/100.0;
    if(std::abs(density-s.density)<0.02)density=s.density<0.55?0.72:0.38;
    s.density=density;
    static const double offsets[]{-0.12,-0.06,0.06,0.12,0.18};
    const double base=fgd::genreSwing(s.genre);const int which=(int)(mix(seed+1901U)%5);
    // Jackin' house shuffle stays near 56–62% (swing 0.36–0.72). A hotter offset
    // lands on the triplet and the beat starts to feel like garage.
    const bool jackinHouse=s.genre==4;
    for(int k=0;k<5;++k){const double swing=std::clamp(base+offsets[(which+k)%5],jackinHouse?0.36:0.0,jackinHouse?0.72:1.0);if(std::abs(swing-s.swing)>1e-4){s.swing=swing;break;}}
}
// The form must answer the Harmonic cycle and the groove, not a random list: every
// section length is a multiple of this phrase, as arrangeSong already assumes.
static int formPhraseBars(const Project& p){
    const int steps=std::max(1,p.settings.chordSteps);
    const int harmonic=std::max(1,(std::max(1,p.settings.chordCount)*steps)/16);
    int motif=1;for(const auto& t:p.tracks)motif=std::max(motif,t.engine.grooveBars);
    int phrase=8;
    for(int g:{harmonic,motif}){int a=phrase,b=g;while(b){const int r=a%b;a=b;b=r;}phrase=phrase/std::max(1,a)*g;}
    return std::clamp(phrase,4,48);
}
void varyForm(Project& p,std::uint32_t seed) {
    const bool locked=std::any_of(p.tracks.begin(),p.tracks.end(),[](const Track& t){return t.locked||std::any_of(t.clips.begin(),t.clips.end(),[](const Clip& c){return c.locked;});});
    const bool free=std::any_of(p.tracks.begin(),p.tracks.end(),[](const Track& t){return t.freeTimeline;});
    if(locked||free||p.sections.empty())return;
    const int phrase=formPhraseBars(p);
    auto pickLength=[&](const int* choices,int count,int index)->int{
        std::vector<int> valid;for(int i=0;i<count;++i)if(choices[i]%phrase==0)valid.push_back(choices[i]);
        if(valid.empty())return std::clamp(phrase,8,48);
        return valid[(size_t)(mix(seed+(std::uint32_t)index*97U)%valid.size())];
    };
    std::vector<int> next;int before=0,after=0;
    for(size_t i=0;i<p.sections.size();++i){
        const auto type=p.sections[i].type;
        const int io[]{16,24,32},bb[]{8,16},body[]{8,16,24,32,48};
        const int* choices=body;int count=5;
        if(type==Section::intro||type==Section::outro){choices=io;count=3;}
        else if(type==Section::breakdown||type==Section::buildup){choices=bb;count=2;}
        const int pick=pickLength(choices,count,(int)i);
        next.push_back(pick);before+=p.sections[i].bars;after+=pick;
    }
    if(after==before){
        const auto type=p.sections.back().type;
        const int io[]{16,24,32},bb[]{8,16},body[]{8,16,24,32,48};
        const int* choices=body;int count=5;
        if(type==Section::intro||type==Section::outro){choices=io;count=3;}
        else if(type==Section::breakdown||type==Section::buildup){choices=bb;count=2;}
        for(int k=0;k<count;++k)if(choices[k]!=next.back()&&choices[k]%phrase==0){next.back()=choices[k];break;}
    }
    for(size_t i=0;i<next.size();++i)if(next[i]!=p.sections[i].bars)p.resizeSection((int)i,next[i]);
}
// Without locks a new song follows the current settings: a factory form of
// another genre becomes this genre's form, and tracks detached by timeline
// edits return to the sections so the form can vary. User-built forms stay.
void renewForm(Project& p) {
    const bool locked=std::any_of(p.tracks.begin(),p.tracks.end(),[](const Track& t){return t.locked||std::any_of(t.clips.begin(),t.clips.end(),[](const Clip& c){return c.locked;});});
    if(locked||p.sections.empty())return;
    const bool wasLooping=p.loop,hadRegion=p.hasLoopRegion(),wholeLoop=p.loopStart==0&&p.loopEnd==p.length();
    const double from=p.loopStart,to=p.loopEnd;
    bool otherGenre=false;
    for(int genre=0;genre<13;++genre)if(genre!=p.settings.genre&&factoryForm(p,genre))otherGenre=true;
    if(otherGenre&&!factoryForm(p,p.settings.genre)){
        arrangeSong(p);
        if(!hadRegion)p.clearLoop();else if(!wholeLoop){p.loopStart=std::min(from,p.length());p.loopEnd=std::min(to,p.length());}
        p.loop=wasLooping&&p.hasLoopRegion();
        return;
    }
    for(auto& t:p.tracks)if(t.freeTimeline&&!isManualTrack(t.kind)){t.freeTimeline=false;t.clips.assign(p.sections.size(),{});}
    p.normalise();
}
void newSongHarmony(Project& p,std::uint32_t seed) {
    auto& s=p.settings;const auto before=s;
    auto differs=[&]{return s.scale!=before.scale||s.chordCount!=before.chordCount
        ||!std::equal(s.degrees.begin(),s.degrees.begin()+s.chordCount,before.degrees.begin())
        ||!std::equal(s.freeChords.begin(),s.freeChords.begin()+s.chordCount,before.freeChords.begin());};
    if(mix(seed)%3!=0){
        int length=4;const auto roll=mix(seed+53U)%5;if(roll==0)length=8;else if(roll==1)length=3;
        const auto degrees=sgk::randomProgression(seed,length,s.scale,s.genre);
        if((int)degrees.size()>=length){s.chordCount=length;s.freeChords.fill(-1);std::copy(degrees.begin(),degrees.begin()+length,s.degrees.begin());}
        if(!differs())s=before;
    }
    // Curated alternatives retain each style's harmonic vocabulary.
    static const std::vector<int> progressions[]{
        {4,8,27,33,19,12}, {2,3,4,8,19,29,12}, {0,1,6,7,20,27},
        {2,8,19,22,24,12}, {17,18,28,30,31,37}, {17,28,30,31,33,37},
        {9,17,18,27,28,31}, {0,9,16,20,21,28}, {9,21,32,33,4,12},
        {4,6,8,27,28,33}, {2,3,19,24,29,12}, {0,6,7,10,20,27},
        {4,6,9,17,27,28}
    };
    if(!differs()){
    const auto& choices=progressions[s.genre];
    const auto first=mix(seed+701U)%choices.size();
    for(size_t i=0;i<choices.size();++i) {
        chooseProgression(p,choices[(first+i)%choices.size()]);
        if(differs())break;
    }
    }
    // Exclude the current key: a new take must audibly move its tonal centre.
    s.key=(before.key+1+(int)(mix(seed+907U)%11))%12;
    static const std::vector<int> bassLines[]{
        {8,12,13}, {10,11,23}, {7,16,17}, {10,11,23}, {13,14,22},
        {8,12,13,14}, {9,18,19}, {15,9,14}, {0,1,2,3,20,21,22},
        {8,12,13}, {4,5,10,23}, {7,16,17}, {8,12,13,14}
    };
    const auto& bass=bassLines[s.genre];
    auto at=mix(seed+1103U)%bass.size();
    if(sgk::bassModeForLine(bass[at])==s.bassMode)at=(at+1)%bass.size();
    s.bassMode=sgk::bassModeForLine(bass[at]);
}
}
static void generateNotes(Project& p,Scope scope,int track,int section,std::uint32_t seed);
// Every section that follows a filled one gets a crash on its first beat: each
// section is rendered on its own, so the fill on the last bar had no answer.
// The note is inserted in order (not appended) and replaces any crash already on
// that beat, so rebuilding with the stored seeds produces exactly the same clip.
static void addSectionCrashes(Project& p){
    constexpr int crashLane=12;
    for(int ti=4;ti<(int)p.tracks.size();++ti){
        auto& t=p.tracks[(size_t)ti];
        if(!isDrumTrack(ti)||ti-4!=crashLane||t.locked||t.freeTimeline)continue;
        if(!t.engine.afterFill)continue;
        for(int si=1;si<(int)p.sections.size()&&si<(int)t.clips.size();++si){
            auto& clip=t.clips[(size_t)si];
            if(clip.locked)continue;
            const auto previous=p.sections[(size_t)si-1].type;
            if(previous==Section::intro||previous==Section::outro)continue;
            const double at=p.sectionStart(si)-p.clipStart(ti,si);
            if(at<0||at>=p.clipLength(ti,si))continue;
            clip.notes.erase(std::remove_if(clip.notes.begin(),clip.notes.end(),
                [at,crashLane](const Note& n){return n.lane==crashLane&&std::abs(n.start-at)<1e-6;}),clip.notes.end());
            const auto place=std::upper_bound(clip.notes.begin(),clip.notes.end(),at,
                [](double value,const Note& n){return value<n.start;});
            clip.notes.insert(place,{at,.25,fgd::defaultNote(crashLane),110,crashLane});
        }
    }
}
void generate(Project& p,Scope scope,int track,int section,std::uint32_t seed) {
    generateNotes(p,scope,track,section,seed);
    if(scope==Scope::song||scope==Scope::section)addSectionCrashes(p);
    // An explicit regeneration (a seed) also rolls the automation of that scope, when asked for.
    if(seed&&p.settings.randomAutomation)randomiseAutomation(p,scope,track,section,seed);
}
static void generateNotes(Project& p,Scope scope,int track,int section,std::uint32_t seed) {
    p.normalise();
    if(isManualTrack(track)&&(scope==Scope::track||scope==Scope::clip))return;
    if(track<0||track>=(int)p.tracks.size()||section<0||(scope==Scope::clip?section>=(int)p.tracks[(size_t)track].clips.size():section>=(int)p.sections.size())) throw std::out_of_range("Generation selection");
    if(scope==Scope::section){const double start=p.sectionStart(section),end=p.sectionStart(section+1);for(int ti=0;ti<trackCount;++ti){auto& t=p.tracks[(size_t)ti];if(!t.freeTimeline||t.locked)continue;for(double boundary:{start,end}){const int at=p.clipAtBeat(ti,boundary);if(at>=0&&!t.clips[(size_t)at].locked&&p.clipStart(ti,at)<boundary)p.splitClip(ti,at,boundary);}}}
    sgk::GroovePattern previousPiano;
    if(seed&&!p.tracks[0].locked&&(scope==Scope::song||scope==Scope::section||track==0)){
        const auto& piano=p.tracks[0];
        for(size_t ci=0;ci<piano.clips.size();++ci){const auto& c=piano.clips[ci];const int guide=piano.freeTimeline?p.sectionAtBeat(p.clipStart(0,(int)ci)):(int)ci;
            if(c.locked||(scope==Scope::clip&&(int)ci!=section)||(scope==Scope::section&&guide!=section))continue;
            previousPiano=sgk::GrooveEngine::basePattern(pianoEngineSettings(p,0,c.seed,c.motifSeed));break;}
    }
    const auto previousSongSeed=p.settings.seed;
    if(seed&&scope==Scope::song) {
        p.settings.seed=seed;
        // Existing locked harmonic notes cannot follow a new global key/chord cycle.
        const bool harmonyLocked=std::any_of(p.tracks.begin(),p.tracks.end(),[](const Track& t){
            return isHarmonicTrack(t.kind)&& std::any_of(t.clips.begin(),t.clips.end(),[&](const Clip& c){return (t.locked||c.locked)&&!c.notes.empty();});
        });
        if(!harmonyLocked){newSongHarmony(p,seed);const auto feel=sgk::genreRiffFeel(p.settings.genre,seed);p.settings.voicing=feel.voicing;p.settings.extension=feel.extension;}   // a new song, a new piano colour
        varySongFeel(p,seed);renewForm(p);varyForm(p,seed);
    }
    const auto& s=p.settings;
    const auto& profile=genreProfile(s.genre);
    for(int ti=0;ti<trackCount;++ti) {
        auto& t=p.tracks[(size_t)ti];
        if(t.locked||((scope==Scope::track||scope==Scope::clip)&&ti!=track)) continue;
        bool prepared=false;
        auto selectedMotif=mix(seed+(isDrumTrack(ti)?313U:(std::uint32_t)ti*131U));
        for(size_t si=0;si<t.clips.size();++si) {
            const int guide=t.freeTimeline?p.sectionAtBeat(p.clipStart(ti,(int)si)):(int)si;
            if((scope==Scope::section&&guide!=section)||(scope==Scope::clip&&(int)si!=section))continue;
            auto& clip=t.clips[si]; if(clip.locked) continue;
            if(seed&&!prepared) {
                // Explicit regeneration leaves fixed presets and seed overrides behind.
                // APPLY (zero seed) continues to render the chosen settings exactly.
                auto& e=t.engine;
                if(ti==0){auto settings=s;if(scope!=Scope::song&&e.density>=0)settings.density=e.density;selectedMotif=freshPianoMotif(settings,previousPiano,selectedMotif);}
                if(ti<3&&std::any_of(e.segmentLocked.begin(),e.segmentLocked.end(),[](bool locked){return locked;})) {
                    const auto previousMotif=clip.motifSeed?clip.motifSeed:mix(previousSongSeed+(std::uint32_t)ti*131U);
                    const auto source=sgk::GrooveEngine::basePattern(pianoEngineSettings(p,ti,clip.seed,previousMotif));
                    auto settings=s;if(e.density>=0)settings.density=e.density;
                    auto target=sgk::GrooveEngine::basePattern(pianoSettings(settings,selectedMotif));
                    for(int segment=0;segment<4;++segment)if(e.segmentLocked[(size_t)segment])sgk::copySegment(target,source,segment);
                    sgk::repairHolds(target.rightHand);sgk::repairHolds(target.leftHand);
                    setPianoPattern(e,target);
                }else e.pattern=-1;
                e.riffSeed=0;
                if(scope==Scope::song)e.density=-1;
                prepared=true;
            }
            if(seed) clip.motifSeed=selectedMotif;
            if(seed) clip.seed=isDrumTrack(ti)?mix(seed+313U):mix(seed+(std::uint32_t)ti*131U+(std::uint32_t)si*977U);
            else if(clip.seed==1) clip.seed=isDrumTrack(ti)?mix(s.seed+313U):mix(s.seed+(std::uint32_t)ti*131U+(std::uint32_t)si*977U);
            clip.loopSource.clear();clip.loopLength=0;
            const double beats=p.clipLength(ti,(int)si);const int renderBars=(int)std::ceil(beats/4.); const auto type=p.sections[(size_t)guide].type;if(t.freeTimeline)clip.role=(int)type;
            const auto& e=t.engine;
            // An explicit strings regenerate must write notes. Jackin' stays off
            // for a factory song, but asking again on this track turns a pad on.
            const bool stringsRequest=ti==2&&seed!=0&&scope!=Scope::song&&(scope==Scope::section||ti==track);
            if(stringsRequest&&p.settings.stringsMode==0)p.settings.stringsMode=(int)sgk::StringsMode::pad;
            auto ps=pianoEngineSettings(p,ti,clip.seed,clip.motifSeed);
            const bool mixing=type==Section::intro||type==Section::outro;
            // Soulful/gospel/afro lifts keep the groove rather than switching to
            // the plugin's generic accelerating snare/piano build pattern.
            const auto engineType=type==Section::buildup&&!profile.rollingBuild?Section::main:type;
            const auto rhythmType=mixing?Section::main:engineType;
            const int chordOffset=(int)(p.clipStart(ti,(int)si)/(s.chordSteps*.25))%s.chordCount;
            for(auto* row:{&ps.progression,&ps.freeChords,&ps.inversions})
                std::rotate(row->begin(),row->begin()+chordOffset,row->begin()+s.chordCount);
            if(mixing){ps.fillEveryBars=0;ps.variationEvery=0;}
            if(ti<3) {
                ps.section=(sgk::Section)(ti==1?rhythmType:engineType); ps.sectionVariation=clip.seed;
                // The kit turns the corner at every section end; give the harmony the
                // same last-bar fill instead of one every 16 bars counted from the clip.
                if(!mixing&&renderBars>=2){ps.forcedFillBar=renderBars-1;ps.forcedFillStyle=-1;}
                auto all=sgk::GrooveEngine::renderArrangement(ps,{{ps.section,renderBars,clip.seed,-1}});
                if(ti==0&&s.guitarLead)all=sgk::Comping::comping(all,clip.seed*7919u+1u,ps,beats);
                clip.notes=notesFrom(all,beats,[ti](const sgk::NoteEvent& e){return ti==0?(e.part==sgk::Part::chords||e.part==sgk::Part::fill):ti==1?e.part==sgk::Part::bass:e.part==sgk::Part::strings;});
                // Answer stabs need a piano rest. A break holds one chord, so they
                // would otherwise stay silent. A pad still follows that harmony.
                if(ti==2&&ps.strings!=sgk::StringsMode::off&&clip.notes.empty()){
                    auto held=ps;held.strings=sgk::StringsMode::pad;
                    auto bed=sgk::GrooveEngine::renderArrangement(held,{{held.section,renderBars,clip.seed,-1}});
                    clip.notes=notesFrom(bed,beats,[](const sgk::NoteEvent& e){return e.part==sgk::Part::strings;});
                }
            } else if(ti==3) {
                fgg::GuitarSettings gs; gs.pattern=e.pattern>=0&&e.pattern<(int)fgg::guitarPatterns().size()?e.pattern:fgg::randomPatternIndex(); gs.randomGenre=s.genre;
                if(s.genre==4&&e.pattern<0&&!clip.motifSeed){
                    const auto& patterns=fgg::guitarPatterns();
                    for(size_t i=0;i<patterns.size();++i)if(patterns[i].name=="Chicago Chank"){gs.pattern=(int)i;break;}
                }
                gs.riffSeed=e.riffSeed?e.riffSeed:(clip.motifSeed?clip.motifSeed:mix(s.seed+393U));gs.stepsPerChord=s.chordSteps;
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
            } else if(ti==leadTrack) {
                fgl::LeadSettings ls;
                ls.globalOctave=e.globalOctave;ls.key=s.key;ls.scale=s.scale;ls.stepsPerChord=s.chordSteps;
                ls.pattern=e.pattern>=0&&e.pattern<(int)fgl::leadPatterns().size()?e.pattern:fgl::randomPatternIndex();
                ls.randomGenre=s.genre;ls.riffSeed=e.riffSeed?e.riffSeed:(clip.motifSeed?clip.motifSeed:mix(s.seed+2227U));
                ls.density=ps.riffDensity;ls.swing=ps.swing;ls.humanize=ps.humanize;ls.humanizeSeed=e.humanizeSeed?e.humanizeSeed:clip.seed;
                ls.tempo=s.bpm;ls.numChords=s.chordCount;ls.fillEveryBars=mixing?0:e.fillEvery;ls.variationEvery=mixing?0:e.variationEvery;
                ls.fillStyle=(fgl::FillStyle)(e.fillStyle<0?(int)fgl::FillStyle::mix:std::min(e.fillStyle,fgl::numFillStyles-1));
                ls.velocity=e.velocity;ls.accentGroove=(float)e.accent;ls.attackMs=(float)(e.strum*80);ls.grooveBars=e.grooveBars;
                ls.segmentOrder=e.segments;ls.letRing=e.letRing;ls.roomForBand=e.roomForBand;ls.pushChanges=e.pushChanges;ls.resolveAfterFill=e.afterFill;
                for(size_t l=0;l<12;++l){ls.levels[l]=(float)e.laneLevel[l];ls.laneSwing[l]=(float)e.laneSwing[l];ls.laneShift[l]=(float)e.laneShift[l];ls.octaves[l]=e.laneOctave[l];ls.muted[l]=e.laneMute[l];}
                for(int i=0;i<s.chordCount;++i)ls.chords[(size_t)i]=s.freeChords[(size_t)i]>=0?fgl::codeChord(s.key,s.freeChords[(size_t)i],std::min(s.extension,2)):fgl::diatonicChord(s.key,s.scale,s.degrees[(size_t)i],std::min(s.extension,2));
                std::rotate(ls.chords.begin(),ls.chords.begin()+chordOffset,ls.chords.begin()+s.chordCount);
                auto all=fgl::GrooveEngine::renderArrangement(ls,{{(fgl::Section)engineType,renderBars,clip.seed,-1}});
                clip.notes=notesFrom(all,beats,[](const fgl::NoteEvent& event){return !event.isController()&&event.lane<fgl::numLanes;});
            } else {
                fgd::DrumSettings ds; ds.pattern=e.pattern>=0&&e.pattern<(int)fgd::drumPatterns().size()?e.pattern:fgd::randomPatternIndex(); ds.randomGenre=s.genre;
                // One kit seed for all drum stems keeps kick, backbeat and hats coherent.
                ds.riffSeed=e.riffSeed?e.riffSeed:(clip.motifSeed?clip.motifSeed:mix(s.seed+313U)); ds.sectionVariation=mix(clip.seed+(std::uint32_t)si*977U);
                ds.density=ps.riffDensity; ds.swing=ps.swing; ds.humanize=ps.humanize; ds.humanizeSeed=e.humanizeSeed?e.humanizeSeed:clip.seed;
                ds.tempo=s.bpm; ds.bassSpace=e.bassSpace; ds.fillEveryBars=e.fillEvery; ds.variationEvery=e.variationEvery; ds.fillStyle=(fgd::FillStyle)(e.fillStyle<0?(int)fgd::FillStyle::mix:std::min(e.fillStyle,fgd::numFillStyles-1));
                ds.velocity=e.velocity;ds.accentGroove=(float)e.accent;ds.genreFeel=e.genreFeel;ds.crashAfterFill=e.afterFill;ds.grooveBars=e.grooveBars;ds.segmentOrder=e.segments;
                ds.laneSwing[(size_t)(ti-4)]=(float)e.laneSwing[0];ds.laneShift[(size_t)(ti-4)]=(float)e.laneShift[0];
                if(e.midiNote>=0)ds.notes[(size_t)(ti-4)]=e.midiNote;
                if(mixing){ds.fillEveryBars=0;ds.variationEvery=0;ds.crashAfterFill=false;}
                auto all=fgd::GrooveEngine::renderArrangement(ds,{{(fgd::Section)rhythmType,renderBars,ds.sectionVariation,-1}});
                clip.notes=notesFrom(all,beats,[ti](const fgd::NoteEvent& e){return e.lane==ti-4;});
            }
            if(isDrumTrack(ti))for(auto& n:clip.notes){n.lane=ti-4;n.pitch=e.midiNote>=0?e.midiNote:fgd::defaultNote(ti-4);}
            shapeBandRoles(clip,p,ti,guide);
            shapeMixSection(clip,s.genre,ti,type,renderBars);
            if(ti==2&&ps.strings!=sgk::StringsMode::off&&clip.notes.empty()){
                auto held=ps;held.strings=sgk::StringsMode::pad;
                auto bed=sgk::GrooveEngine::renderArrangement(held,{{held.section,renderBars,clip.seed,-1}});
                clip.notes=notesFrom(bed,beats,[](const sgk::NoteEvent& e){return e.part==sgk::Part::strings;});
                shapeMixSection(clip,s.genre,ti,type,renderBars);
            }
            tidyNotes(clip,beats);if(ti==leadTrack)monophonicLead(clip);
        }
    }
}
void applyInstrumentChange(Project& p,int track,bool harmony) {
    p.normalise();
    if(track<0||track>=trackCount)return;
    for(int ti=0;ti<trackCount;++ti){
        if(ti!=track&&!(harmony&&isHarmonicTrack(ti)))continue;
        if(p.tracks[(size_t)ti].locked||p.tracks[(size_t)ti].clips.empty())continue;
        generate(p,Scope::track,ti,0,0);
    }
}
bool regenerateClip(Project& p,int track,int clip,std::uint32_t seed) {
    if(track<0||track>=trackCount)return false;
    auto& t=p.tracks[(size_t)track];
    if(t.locked||clip<0||clip>=(int)t.clips.size()||t.clips[(size_t)clip].locked)return false;
    auto& c=t.clips[(size_t)clip];
    // The musical take (motifSeed, or the song's legacy motif) is kept, so the
    // groove chosen in the panel stays; only the performance variation changes.
    auto next=mix(seed^(c.seed*2654435761U)^((std::uint32_t)clip*977U));
    if(next==c.seed||next<=1)next=c.seed+2;
    c.seed=next;
    generate(p,Scope::clip,track,clip,0);
    if(p.settings.randomAutomation)randomiseAutomation(p,Scope::clip,track,clip,next);
    return true;
}
namespace {
struct AutomationDice {
    std::uint32_t state;
    explicit AutomationDice(std::uint32_t seed):state(mix(seed^0x9e3779b9U)){if(!state)state=1;}
    double next(){state^=state<<13;state^=state>>17;state^=state<<5;return (state&0xffffff)/16777216.;}
    double between(double a,double b){return a+(b-a)*next();}
    bool chance(double p){return next()<p;}
};
// Which tracks move: the harmony and the percussion that colours the groove. Kick, bass,
// snare and clap keep the floor (they stay where they are).
bool volumeMoves(int ti){return ti==0||ti==2||ti==3||ti==leadTrack||ti==8||ti==9||ti==11||ti==12||ti==13;}
bool panMoves(int ti){return ti==2||ti==3||ti==leadTrack||ti==9||ti==11||ti==12||ti==13||ti==14;}
}
void randomiseAutomation(Project& p,Scope scope,int track,int index,std::uint32_t seed,int onlyLane){
    p.normalise();
    if(track<0||track>=(int)p.tracks.size())return;
    double from=0,to=p.length();
    if(scope==Scope::section){if(index<0||index>=(int)p.sections.size())return;from=p.sectionStart(index);to=p.sectionStart(index+1);}
    if(scope==Scope::clip){const auto& clips=p.tracks[(size_t)track].clips;if(index<0||index>=(int)clips.size())return;from=p.clipStart(track,index);to=from+p.clipLength(track,index);}
    if(to-from<1./960.)return;
    const double tick=1./16.;
    for(int ti=0;ti<(int)p.tracks.size();++ti){
        if(isManualTrack(ti)&&(scope==Scope::song||scope==Scope::section))continue;
        if((scope==Scope::track||scope==Scope::clip)&&ti!=track)continue;
        auto& t=p.tracks[(size_t)ti];if(t.locked)continue;
        if(scope==Scope::clip&&t.clips[(size_t)index].locked)continue;
        if(scope==Scope::section){const int at=p.clipAtBeat(ti,from);if(at>=0&&t.clips[(size_t)at].locked)continue;}
        // Every lane this track automates: kind 0 volume, 1 pan, 2 + k plugin lane k.
        for(int kind=0;kind<2+(int)t.parameters.size();++kind){
            if(onlyLane>=0&&kind!=onlyLane)continue;
            const bool asked=onlyLane>=0&&scope!=Scope::song;   // a lane's own dice moves it even on another track
            if(kind==0&&!volumeMoves(ti)&&!asked)continue;
            if(kind==1&&!panMoves(ti)&&!asked)continue;
            auto& env=kind==0?t.volume:kind==1?t.panAutomation:t.parameters[(size_t)(kind-2)].curve;
            if(env.locked)continue;
            const double neutral=kind==0?1.:kind==1?automationFromPan(t.pan):(env.points.empty()?.5:env.points.front().value);   // a plugin lane rests where it starts
            auto& points=env.points;
            if(points.empty())points={{0,neutral,0},{p.length(),neutral,0}};
            points.erase(std::remove_if(points.begin(),points.end(),[&](const VolumePoint& v){return v.beat>=from&&v.beat<to;}),points.end());
            std::vector<VolumePoint> add{{from,neutral,0}};
            double start=0;
            for(size_t si=0;si<p.sections.size();++si){
                const auto& section=p.sections[si];const double s0=start,s1=start+section.bars*4.;start=s1;
                const double a=std::max(s0,from),b=std::min(s1,to);if(b-a<1.)continue;
                AutomationDice dice(seed^mix((std::uint32_t)ti*7919U+(std::uint32_t)si*104729U+(std::uint32_t)kind*31U+1U));
                const double length=b-a;
                // Each section starts from the lane's resting value (held up to its first beat),
                // so a section without movement stays flat and a riser never leaks backwards.
                if(a-1./64.>from+1e-9)add.push_back({a-1./64.,neutral,0});
                add.push_back({a,neutral,0});
                switch(section.type){
                case Section::buildup:
                    if(kind==0){add.push_back({a,dice.between(.72,.86),dice.between(.3,.6)});add.push_back({b-tick,dice.between(1.08,1.2),0});add.push_back({b,1,0});}         // the riser into the drop
                    else if(kind==1){const double side=dice.chance(.5)?1:-1,width=dice.between(.12,.3);add.push_back({a,neutral,.2});add.push_back({b-tick,std::clamp(neutral+side*width,0.,1.),0});add.push_back({b,neutral,0});} // it opens up
                    else{add.push_back({a,dice.between(.12,.3),dice.between(.4,.7)});add.push_back({b-tick,dice.between(.82,1.),0});add.push_back({b,neutral,0});}             // the filter sweep
                    break;
                case Section::breakdown:
                    if(kind==0&&(ti==0||ti==2)){add.push_back({a,dice.between(.88,.95),.3});add.push_back({a+length*.5,dice.between(1.02,1.08),-.3});add.push_back({b-tick,dice.between(.94,1.),0});add.push_back({b,1,0});}
                    else if(kind==1&&dice.chance(.7)){const double width=dice.between(.08,.22);for(double x=a;x<b-1e-9;x+=8){const bool left=std::fmod((x-a)/8,2.)<.5;add.push_back({x,std::clamp(neutral+(left?-width:width),0.,1.),dice.between(-.3,.3)});}add.push_back({b,neutral,0});}
                    else if(kind>=2){add.push_back({a,neutral,.3});add.push_back({a+length*.5,dice.between(.28,.45),-.3});add.push_back({b-tick,neutral,0});}
                    break;
                case Section::main:
                case Section::drop:
                    if(kind==0&&dice.chance(.5)){const double depth=dice.between(.04,.09);for(double x=a;x<b-1e-9;x+=8){add.push_back({x,1,.4});if(x+4<b)add.push_back({x+4,1-depth,-.4});}add.push_back({b,1,0});}
                    else if(kind==1&&dice.chance(.55)){const double width=dice.between(.06,.18),bar=dice.chance(.5)?4.:8.;for(double x=a;x<b-1e-9;x+=bar){const bool left=std::fmod((x-a)/bar,2.)<.5;add.push_back({x,std::clamp(neutral+(left?-width:width),0.,1.),dice.between(-.4,.4)});}add.push_back({b,neutral,0});}
                    else if(kind>=2&&dice.chance(.35)){const double depth=dice.between(.08,.2);for(double x=a;x+16<=b+1e-9;x+=16){add.push_back({x,neutral,.5});add.push_back({x+8,std::clamp(neutral+depth,0.,1.),-.5});}add.push_back({b,neutral,0});}
                    break;
                default: break;   // intro and outro: no fades, the level stays as mixed
                }
            }
            // What follows the range keeps its own first point; without one the lane comes back to neutral.
            const bool following=std::any_of(points.begin(),points.end(),[&](const VolumePoint& v){return std::abs(v.beat-to)<1./960.;});
            if(!following&&to<p.length())add.push_back({to,neutral,0});
            for(auto v:add){v.beat=std::clamp(v.beat,from,to);if(following&&v.beat>to-1./960.)continue;points.push_back(v);}
            env.enabled=true;env.normalise(p.length(),kind==0?2.:1.);
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
void quantise(Clip& c,double grid,double strength,double limit) {
    if(!std::isfinite(grid)||grid<=0)return;
    strength=std::clamp(strength,0.0,1.0);
    const bool bounded=std::isfinite(limit)&&limit>0;constexpr double tick=1./960.;
    for(auto& n:c.notes){
        if(!std::isfinite(n.start)||!std::isfinite(n.length))continue;
        const double end=n.start+n.length;
        n.start+=(std::round(n.start/grid)*grid-n.start)*strength;
        double newEnd=end+(std::round(end/grid)*grid-end)*strength;
        if(newEnd<n.start+tick)newEnd=n.start+tick;
        n.length=newEnd-n.start;
        if(bounded){
            if(n.start<0)n.start=0;
            if(n.start>limit-tick)n.start=std::max(0.0,std::floor((limit-tick)/grid)*grid);
            if(n.start+n.length>limit)n.length=std::max(tick,limit-n.start);
        }
    }
}
void humanize(Clip& c,double amount,std::uint32_t seed,double beats) {
    if(!std::isfinite(amount)||amount<=0||!std::isfinite(beats)||beats<=0)return;
    amount=std::clamp(amount,0.0,1.0);constexpr double tick=1./960.;
    for(size_t i=0;i<c.notes.size();++i){
        auto& n=c.notes[i];if(!std::isfinite(n.start)||!std::isfinite(n.length))continue;
        const auto h=mix(seed+(std::uint32_t)i*977u+(std::uint32_t)n.pitch*13u);
        const double u=(h&65535)/65535.0;
        n.start=std::clamp(n.start+(u*2-1)*amount*0.04,0.0,std::max(0.0,beats-tick));
        n.velocity=std::clamp(n.velocity+(int)std::lround((((int)((h>>16)&255)-128)/128.0)*amount*18),1,127);
        if(n.start+n.length>beats)n.length=std::max(tick,beats-n.start);
    }
}
std::vector<TimedEvent> events(const Project& p,int onlyTrack) {
    std::vector<TimedEvent> out;
    for(size_t ti=0;ti<p.tracks.size();++ti) {
        if(onlyTrack>=0&&(int)ti!=onlyTrack)continue;
        const auto& t=p.tracks[ti];
        double occupied=0,sectionBegin=0;
        for(size_t si=0;si<t.clips.size();++si) {
            // Running section start instead of the O(n) clipStart for section-aligned clips.
            const double begin=t.freeTimeline?t.clips[si].position:sectionBegin,end=p.clipLength((int)ti,(int)si),from=std::max(0.,occupied-begin);
            if(si<p.sections.size())sectionBegin+=p.sections[si].bars*4.0;
            occupied=std::max(occupied,begin+end);if(from>=end)continue;
            if(ti==0&&t.engine.pedalCC&&!t.clips[si].notes.empty()){
                const double chord=p.settings.chordSteps*.25;
                for(double at=0;at<end;at+=chord){if(at<from)continue;out.push_back({begin+at,(int)ti,64,0,0,64});out.push_back({begin+at+.02,(int)ti,64,127,0,64});}
                const Section role=t.freeTimeline&&t.clips[si].role>=0?(Section)t.clips[si].role:p.sections.empty()?Section::main:p.sections[(size_t)p.sectionAtBeat(begin)].type;
                for(double at=0;at<end;at+=.25){if(at<from)continue;const int expression=role==Section::buildup?std::clamp((int)(60+67*at/end),0,127):127;out.push_back({begin+at,(int)ti,11,expression,0,11});}
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
