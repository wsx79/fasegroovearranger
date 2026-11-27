// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Project.h"
#include "GenreSound.h"
#include <set>
#include <optional>
#include <map>
#include <functional>
#include "ElectronicStyles.h"
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
// Shared by the band passes: notes in time order (a chord low to high), and one roll of a pass's dice.
static bool byStartPitch(const Note& a,const Note& b){return a.start==b.start?a.pitch<b.pitch:a.start<b.start;}
static double nextDice(std::uint32_t& r){r=mix(r+0x9e3779b9U);return (r&0xffffff)/16777216.;}
double finite(double x, double fallback, double lo, double hi) { return std::isfinite(x) ? std::clamp(x, lo, hi) : fallback; }
bool sameHarmony(const Settings& a,const Settings& b){
    if(a.chordCount!=b.chordCount)return false;
    for(int i=0;i<a.chordCount;++i){
        auto a0=a,b0=b;a0.harmonyOrigin=b0.harmonyOrigin=0;const auto x=chordAtBeat(a0,i*a.chordSteps*.25),y=chordAtBeat(b0,i*b.chordSteps*.25);
        if(x.rootPc!=y.rootPc||x.tones.size!=y.tones.size)return false;
        for(int n=0;n<x.tones.size;++n)if(x.tones[n]!=y.tones[n])return false;
    }
    return true;
}
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
    p.riffGenre=engineGenre(s.genre);
    auto pattern=sgk::makeRandomPattern(seed,density,engineGenre(s.genre));
    if(s.genre==latinHouseGenre){
        // Latin House plays a montuno over a tumbao left hand, never the Afro tresillo figure.
        static const char* const right[]{
            "x.x..x.x.x.x.x.x..x..x.x.x.x.x.x","x..x..x.x.x..x.x..x.x..x.x..x.x.",".x.x.xx..x.x.xx..x.x.xx..x.x.x.x",
            "x.x.x..x.x.x..x..x.x..x.x.x..x.x","x..x..x...x.x.....x..x..x.x.x...","x.x..x.x.x..x.x.x.x..x.x..x.x.x."};
        static const char* const left[]{
            "......x=....x=........x=....x=..","x.....x.....x...x.....x.....x...","x.....x...x.......x...x.....x...","......x=...x..x.......x=...x..x."};
        pattern.rightHand=right[mix(seed+41U)%6];pattern.leftHand=left[mix(seed+59U)%4];
    }
    if(s.genre==afroHouseGenre){
        // The Afro piano no longer always opens on the tresillo: the take picks the head of each bar
        // (the tresillo stays one of four), the engine's tail answers it.
        static const char* const heads[]{"x..x..x.","..x..x.x","x...x.x.",".x..x..x"};const char* head=heads[mix(seed+83U)%4];
        auto& rh=pattern.rightHand;for(size_t bar=0;bar+8<=rh.size();bar+=16)for(size_t k=0;k<8;++k)rh[bar+k]=head[k];
    }
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
// Competing lanes can humanize the same attack a few milliseconds apart.
// Keep one attack even when the vendor already cut the preceding voice short.
void monophonicLead(Clip& clip){
    constexpr double minimumAttack=1./64.;size_t written=0;
    for(size_t read=0;read<clip.notes.size();++read){const Note note=clip.notes[read];
        while(written>0){
            auto& previous=clip.notes[written-1];const double length=note.start-previous.start;
            if(length<minimumAttack){--written;continue;}
            if(previous.start+previous.length>note.start)previous.length=length;break;
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
Settings resolveSectionHarmony(const Project& p,int section){
    auto s=p.settings;if(section<0||section>=(int)p.sections.size())return s;s.harmonyOrigin=p.sectionStart(section);const auto& h=p.sections[(size_t)section].harmony;
    if(h.enabled){
        if(h.key>=0)s.key=std::clamp(h.key,0,11);
        if(h.scale>=0)s.scale=std::clamp(h.scale,0,12);
        // Defensive: callers divide by chordSteps and take modulo chordCount.
        s.chordCount=std::clamp(h.chordCount,1,8);
        s.chordSteps=(h.chordSteps==8||h.chordSteps==16||h.chordSteps==32||h.chordSteps==64)?h.chordSteps:16;
        for(size_t i=0;i<s.degrees.size();++i){
            s.degrees[i]=std::clamp(h.degrees[i],0,6);
            s.freeChords[i]=std::clamp(h.freeChords[i],-1,35);
            s.manualChords[i]=h.manualChords[i]&&s.freeChords[i]>=0;
        }
    }
    return s;
}
sgk::GrooveSettings pianoEngineSettings(const Project& p,int track,std::uint32_t seed,std::uint32_t take,const Clip* clip){
    auto s=p.settings;const auto& t=p.tracks[(size_t)track];const auto& e=t.engine;
    if(clip){int section=p.sectionAtBeat(clip->position);if(!t.freeTimeline)for(size_t i=0;i<t.clips.size();++i)if(&t.clips[i]==clip){section=(int)i;break;}s=resolveSectionHarmony(p,section);}
    if(clip){s.density=takeFeel(e.density,clip->feel[0],s.density);s.swing=takeFeel(e.swing,clip->feel[1],s.swing);s.humanize=takeFeel(e.humanize,clip->feel[2],s.humanize);}
    double energy=1;if(clip){const int section=t.freeTimeline?p.sectionAtBeat(clip->position):[&]{for(size_t i=0;i<t.clips.size();++i)if(&t.clips[i]==clip)return (int)i;return -1;}();if(section>=0&&section<(int)p.sections.size()){const auto& sec=p.sections[(size_t)section];
        // Auto energy: in a song with drops the main part (the verse) leaves room, so the drop is the fuller one.
        const bool drops=std::any_of(p.sections.begin(),p.sections.end(),[](const SongSection& x){return x.type==Section::drop;});
        energy=sec.energy>=0?energyScale(sec.energy):sec.type==Section::main&&drops&&!isElectronicGenre(p.settings.genre)&&!isDrumTrack(track)?.82:1.;}}   /* the groove stays: only the band thins */
    s.density=std::clamp(s.density*energy,0.,1.);
    auto trackSettings=s;if(e.density>=0)trackSettings.density=std::clamp(e.density*energy,0.,1.);
    // A regenerated track/song shares its musical take across sections. Legacy
    // projects retain the song motif until explicitly regenerated.
    const auto motifSeed=take?take:mix(s.seed+(std::uint32_t)track*131U);
    auto ps=pianoSettings(trackSettings,e.riffSeed?e.riffSeed:motifSeed);
    // Develop the selected bass figure without replacing its groove or its strong anchors.
    // Root/fifth/octave substitutions stay inside the current chord; pickups never
    // overwrite held notes. A take loops coherently through the song's sections.
    // Latin House: a real tumbao under the montuno, the bass anticipating the harmony (the fifth on the and of two,
    // the root on four, tied over the bar line, never on the one) instead of the Afro line it used to borrow.
    // Four tumbao figures (the take or the seed picks one), all anticipating: pickups and octave answers vary.
    static const std::string tumbaos[]{"......5=====R===......O=====R=.A","...R..5=....R===...R..5=...5R=.A","......5===.5R===......5===.OR===","..R...5=====R===..O...5=====R=.A"};
    const std::string& tumbao=tumbaos[mix((take?take:e.riffSeed?e.riffSeed:seed)^0x7b3c2a19U)%4];
    if(track==1&&s.genre==latinHouseGenre&&sgk::isBassLine(ps.bass)&&!(take||e.riffSeed))ps.bassLineOverride=tumbao;
    if(track==1&&(take||e.riffSeed)&&sgk::isBassLine(ps.bass)){
        auto line=s.genre==latinHouseGenre?tumbao:sgk::bassLines()[(size_t)sgk::bassLineIndex((int)ps.bass)].line;
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
            }else if(source[i]=='.'&&s.genre!=afroHouseGenre&&source[i-1]!='='&&source[(i+1)%source.size()]!='='&&choice%4==0)line[i]=(choice&8)?'R':'5';   // Afro stays dry: no extra pickups
        }
        ps.bassLineOverride=std::move(line);
    }
    ps.humanizeSeed=seed;   // NEW FEEL (e.humanizeSeed) is applied after generation: it must never change the notes
    ps.swing=(float)(e.swing<0?s.swing:e.swing);ps.humanize=(float)(e.humanize<0?s.humanize:e.humanize);ps.riffDensity=(float)(e.density<0?s.density:std::clamp(e.density*energy,0.,1.));
    ps.velocity=e.velocity;ps.gate=(float)e.gate;ps.strum=(float)e.strum;ps.centre=e.centre;ps.movement=e.movement;ps.octaveTop=e.octaveTop;
    ps.strumDirection=e.strumDirection;ps.topLine=(sgk::TopLine)e.topLine;ps.approach=(sgk::ApproachStyle)e.approach;ps.evolveBars=e.evolveBars;ps.humanizeStyle=e.humanizeStyle;ps.bassFeel=e.bassFeel;
    ps.rangeLow=e.rangeLow;ps.rangeHigh=e.rangeHigh;ps.realism=e.realism;ps.grooveBars=e.grooveBars;ps.segmentOrder=e.segments;
    ps.fillEveryBars=e.fillEvery;ps.fillStyle=(sgk::FillStyle)(e.fillStyle<0?(int)sgk::FillStyle::mix:std::min(e.fillStyle,sgk::numFillStyles-1));ps.variationEvery=e.variationEvery;
    ps.stepsPerChord=s.chordSteps;ps.inversions=e.inversions;ps.velocityOrder=e.velocityOrder;
    // Piano and bass are separate tracks: a shared pitch must not cut or steal the other's note.
    ps.separateBass=true;
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
        {122,128,125,32,false}, // Jackin' House: 122–128, centre 125, shuffle not speed garage
        {124,130,126,32,true},  // Jackin' Piano
        {130,135,132,32,true},  // Garage / UK
        {120,124,121,32,false}, // Afro House: percussion-led, swung, deep lift (120-124)
        {112,124,120,32,false}, // Funky / Disco
        {120,130,126,32,true},  // 90s Piano
        { 92,112, 98,16,false}, // Neo Soul / Broken: warm-up/broken-groove policy
        { 90,115,100,16,false}, // Chill / Pads: downtempo warm-up policy
        {124,130,128,32,true},  // Dance Stabs
        {126,145,132,32,true},  // Techno
        {124,132,128,32,true},  // Electro House
        {120,127,124,32,false}  // Latin House: clave, cowbell and congas, straighter
    };
    static_assert(std::size(profiles)==numGenres);
    return profiles[normaliseGenre(genre)];
}
std::vector<std::string> genres() { std::vector<std::string> out; for(auto s:sgk::grooveCategories()) out.emplace_back(s);out[(size_t)deepHouseGenre]="Deep House";out[(size_t)afroHouseGenre]="Afro House";out.emplace_back("Techno");out.emplace_back("Electro House");out.emplace_back("Latin House");return out; }
double Project::sectionStart(int index) const { double p=0; for(int i=0;i<std::clamp(index,0,(int)sections.size());++i) p+=sections[(size_t)i].bars*4.0; return p; }
double Project::clipLength(int track,int section) const {
    if(track>=0&&track<(int)tracks.size()&&tracks[(size_t)track].freeTimeline){const auto& clips=tracks[(size_t)track].clips;return section>=0&&section<(int)clips.size()?clips[(size_t)section].span:0;}
    if(section<0||section>=(int)sections.size())return 0;
    if(track>=0&&track<(int)tracks.size()&&section<(int)tracks[(size_t)track].clips.size()){
        const double span=tracks[(size_t)track].clips[(size_t)section].span;if(std::isfinite(span)&&span>0)return std::clamp(span,.25,maxClipBeats);
    }
    return sections[(size_t)section].bars*4.;
}
double Project::bpmAt(double beat) const{return tempoAutomation.active()?bpmFromAutomation(tempoAutomation.valueAt(beat)):settings.bpm;}
double Project::secondsAt(double beat) const{
    if(!tempoAutomation.active())return beat*60./settings.bpm;
    // Segment by segment between the lane's points (Simpson's rule, 16 steps each): the time of a position costs
    // a few dozen evaluations per point, not one per 1/64 beat from the song start (the ruler asks it per label).
    const double end=std::max(0.,beat);auto inv=[&](double b){return 60./bpmAt(b);};
    auto simpson=[&](double a,double z){if(z<=a)return 0.;constexpr int n=16;const double h=(z-a)/n;double sum=inv(a)+inv(z);for(int i=1;i<n;++i)sum+=inv(a+i*h)*(i%2?4:2);return sum*h/3;};
    double seconds=0,from=0;for(const auto& point:tempoAutomation.points){if(point.beat<=from)continue;if(point.beat>=end)break;seconds+=simpson(from,point.beat);from=point.beat;}
    return seconds+simpson(from,end);
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
bool repeatLoop(const std::vector<Note>& source,double period,double duration,std::vector<Note>& notes,double phase=0){
    if(!std::isfinite(period)||period<=0||!std::isfinite(duration)||duration<=0||!std::isfinite(phase)||phase<0||phase>=period)return false;
    if(source.empty()){notes.clear();return true;}
    const double cycles=std::ceil((duration+phase)/period);
    if(!std::isfinite(cycles)||cycles>2000000/source.size())return false;
    const auto repeats=(size_t)cycles;
    Clip rendered;
    for(size_t repeat=0;repeat<repeats;++repeat)for(auto n:source){n.start+=repeat*period-phase;const double end=std::min(n.start+n.length,duration);n.start=std::max(0.,n.start);if(end-n.start>=1./960.){n.length=end-n.start;rendered.notes.push_back(n);}}
    tidyNotes(rendered,duration);notes=std::move(rendered.notes);return true;
}
bool resizedLoop(Clip& clip,double previous,double duration){
    if(clip.audioOffset>=0){clip.audioDuration*=duration/previous;clip.loopLength=clip.loopPhase=0;clip.loopSource.clear();if(!clip.notes.empty()){clip.notes.resize(1);clip.notes[0].start=0;clip.notes[0].length=duration;}return true;}
    std::vector<Note> expected;
    const bool unchanged=clip.loopLength>0&&repeatLoop(clip.loopSource,clip.loopLength,previous,expected,clip.loopPhase)&&expected.size()==clip.notes.size()&&std::equal(expected.begin(),expected.end(),clip.notes.begin(),[](const Note& a,const Note& b){return a==b;});
    if(!unchanged){
        if(clip.loopLength>previous){
            // Edits in a shortened view must retain the hidden part of the source.
            std::vector<Note> source;for(auto n:clip.loopSource)if(n.start>=previous)source.push_back(n);
            for(auto n:clip.notes){auto it=std::lower_bound(clip.loopSource.begin(),clip.loopSource.end(),n,byStartPitch);
                if(it!=clip.loopSource.end()&&it->start==n.start&&it->pitch==n.pitch&&it->lane==n.lane&&n.length==std::min(it->length,previous-n.start))n.length=it->length;
                source.push_back(n);}
            Clip clean;clean.notes=std::move(source);tidyNotes(clean,clip.loopLength);clip.loopSource=std::move(clean.notes);
        }else{clip.loopSource=clip.notes;clip.loopLength=previous;clip.loopPhase=0;}
    }
    return repeatLoop(clip.loopSource,clip.loopLength,duration,clip.notes,clip.loopPhase);
}
}
bool rebuildClipLoop(Clip& clip,double duration){
    return repeatLoop(clip.loopSource,clip.loopLength,duration,clip.notes,clip.loopPhase);
}
void Project::refitAudioClips(){
    for(auto& t:tracks){if(t.instrument!="sample")continue;const auto& s=t.sampler;
        const double beatsPerSecond=(s.sync?s.sourceBpm:settings.bpm/std::pow(2.,(s.transpose+s.fine*.01)/12.))/60.;
        for(auto& c:t.clips)if(c.audioOffset>=0&&c.audioDuration>0){c.span=c.audioDuration*beatsPerSecond;c.loopLength=c.loopPhase=0;c.loopSource.clear();if(!c.notes.empty()){c.notes.resize(1);c.notes[0].start=0;c.notes[0].length=c.span;}}}
    normalise();
}
bool Project::trimClipStart(int track,int index,double start){
    if(track<0||track>=(int)tracks.size()||!tracks[(size_t)track].freeTimeline||index<0||index>=(int)tracks[(size_t)track].clips.size()||!std::isfinite(start))return false;
    auto& t=tracks[(size_t)track];auto& c=t.clips[(size_t)index];const double from=c.position,length=c.span,end=from+length;
    double lowest=0;if(index>0)lowest=t.clips[(size_t)index-1].position+t.clips[(size_t)index-1].span;
    start=std::clamp(std::round(start*960)/960.,lowest,end-.25);
    if(c.audioOffset>=0&&c.audioDuration>0&&length>0){const double secondsPerBeat=c.audioDuration/length;start=std::max(start,from-c.audioOffset/secondsPerBeat);
        const double delta=start-from;c.audioOffset=std::max(0.,c.audioOffset+delta*secondsPerBeat);c.audioDuration=(end-start)*secondsPerBeat;c.position=start;c.span=end-start;
        c.loopLength=c.loopPhase=0;c.loopSource.clear();if(c.notes.empty())c.notes.push_back({0,c.span,t.sampleRoot,127,0});c.notes.resize(1);c.notes[0].start=0;c.notes[0].length=c.span;normalise();return true;}
    const double delta=start-from;if(std::abs(delta)<1e-9)return true;const double newLength=end-start;
    if(c.loopLength>0){c.loopPhase=std::fmod(std::fmod(c.loopPhase+delta,c.loopLength)+c.loopLength,c.loopLength);if(!rebuildClipLoop(c,newLength))return false;}
    else{std::vector<Note> kept;for(auto n:c.notes){n.start-=delta;const double noteEnd=n.start+n.length;if(noteEnd<=1./960.)continue;if(n.start<0){n.length=noteEnd;n.start=0;}if(n.start>=newLength)continue;n.length=std::min(n.length,newLength-n.start);kept.push_back(n);}c.notes=std::move(kept);}
    c.position=start;c.span=newLength;normalise();return true;
}
bool Project::resizeClipLoop(int track,int section,double beats){
    if(track<0||track>=(int)tracks.size()||section<0||section>=(int)tracks[(size_t)track].clips.size()||!std::isfinite(beats))return false;
    beats=std::clamp(std::round(beats*960)/960.,.25,maxClipBeats);auto candidate=tracks[(size_t)track].clips[(size_t)section];
    if(candidate.audioOffset>=0){const double previous=clipLength(track,section);candidate.audioDuration*=beats/previous;candidate.loopLength=0;candidate.loopPhase=0;candidate.loopSource.clear();if(!candidate.notes.empty()){candidate.notes.resize(1);candidate.notes[0].start=0;candidate.notes[0].length=beats;}}
    else if(!resizedLoop(candidate,clipLength(track,section),beats))return false;
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
// Sections named "Audio take" are added only to make room for a long dropped or recorded take.
// Once the audio there is trimmed away, the empty ones at the end go, so the song (and its
// export) ends with the music instead of with silence.
void Project::dropUnusedAudioTakeSections(){
    if(sections.empty()||sections.back().name!="Audio take")return;
    std::vector<double> starts(sections.size()+1,0.);for(size_t i=0;i<sections.size();++i)starts[i+1]=starts[i]+sections[i].bars*4.;
    double used=0;
    for(const auto& t:tracks)for(size_t ci=0;ci<t.clips.size();++ci){const auto& c=t.clips[ci];
        const bool content=!c.notes.empty()||c.audioOffset>=0;if(!content)continue;
        const double from=t.freeTimeline?c.position:starts[std::min(ci,sections.size())];
        const double to=t.freeTimeline?c.position+c.span:ci<sections.size()?(c.span>0?from+c.span:starts[ci+1]):from;used=std::max(used,to);}
    while(sections.size()>1&&sections.back().name=="Audio take"){
        const size_t last=sections.size()-1;if(starts[last]<used-1e-9){const int bars=(int)std::ceil((used-starts[last])/4.-1e-9);if(bars<sections.back().bars)sections.back().bars=std::max(1,bars);break;}
        sections.pop_back();for(auto& t:tracks)if(!t.freeTimeline&&t.clips.size()>sections.size())t.clips.resize(sections.size());}
    const double end=[&]{double e=0;for(const auto& x:sections)e+=x.bars*4.;return e;}();if(loopEnd>end)loopEnd=end;if(loopStart>loopEnd)loopStart=0;
}
void Project::normalise() {
    dropUnusedAudioTakeSections();
    for(int b=0;b<2;++b){auto& rack=returnEffects[(size_t)b];rack.name=b==0?"Return A":"Return B";if(rack.effects.size()>maxTrackEffects)rack.effects.resize(maxTrackEffects);}
    masterEffects.name="Master";if(masterEffects.effects.size()>maxTrackEffects)masterEffects.effects.resize(maxTrackEffects);
    auto& s=settings;
    s.genre=normaliseGenre(s.genre); s.key=std::clamp(s.key,0,11); s.scale=std::clamp(s.scale,0,12);
    if(s.chordSteps!=8&&s.chordSteps!=16&&s.chordSteps!=32&&s.chordSteps!=64)s.chordSteps=16;
    s.extension=std::clamp(s.extension,0,10); s.voicing=std::clamp(s.voicing,0,10);
    const auto& tempo=genreProfile(s.genre);
    s.bpm=finite(s.bpm,tempo.preferredBpm,s.freeTempo?30:tempo.minBpm,s.freeTempo?300:tempo.maxBpm); s.swing=finite(s.swing,.2,0,1.5); s.density=finite(s.density,.55,0,1); s.humanize=finite(s.humanize,.2,0,1);
    s.targetSeconds=finite(s.targetSeconds,0,0,24*3600);if(!(s.targetSeconds>0))s.targetEnabled=false;s.targetToleranceSeconds=finite(s.targetToleranceSeconds,5,0,600);
    s.chordCount=std::clamp(s.chordCount,1,8); s.bassMode=std::clamp(s.bassMode,0,sgk::numBassModes-1); s.stringsMode=std::clamp(s.stringsMode,0,10);
    for(auto& d:s.degrees) d=std::clamp(d,0,6);
    for(auto& c:s.freeChords) c=std::clamp(c,-1,35);
    for(size_t i=0;i<s.manualChords.size();++i)if(s.freeChords[i]<0)s.manualChords[i]=false;
    if(sections.empty()) sections.push_back({Section::main,8,"Main"});
    if(sections.size()>64) sections.resize(64);
    for(auto& p:sections) {p.bars=std::clamp(p.bars,1,128); p.type=(Section)std::clamp((int)p.type,0,5);}
    if(tracks.size()>maxTracks) tracks.resize(maxTracks);
    while(tracks.size()<trackCount) { Track t; t.kind=(int)tracks.size(); t.name=t.kind==leadTrack?"Lead":t.kind<4?std::array<const char*,4>{"Piano","Bass","Strings","Guitar"}[(size_t)t.kind]:fgd::laneName(t.kind-4); if(t.kind==leadTrack){t.gain=.22;t.engine.density=.42;t.engine.roomForBand=false;t.engine.fillEvery=8;} tracks.push_back(t); }
    for(size_t i=0;i<tracks.size();++i) {
        auto& t=tracks[i]; t.kind=(int)i; if(!isManualTrack(t.kind)){t.vocalChops=false;t.part=0;}if(t.vocalChops)t.part=0; if(!t.freeTimeline)t.clips.resize(sections.size());else if(t.clips.size()>4096)t.clips.resize(4096);
        t.sampler.normalise();t.group=std::clamp(t.group,-1,1);for(auto& send:t.sends)send=finite(send,0,0,1);if(t.sampleHash.size()>64)t.sampleHash.clear();
        t.gain=finite(t.gain,.75,0,2); t.pan=finite(t.pan,0,-1,1); t.sampleRoot=std::clamp(t.sampleRoot,0,127); t.sound=std::clamp(t.sound,0,7)   /* 8 guide sounds (Sub Sine is 7) */;
        std::array<bool,maxTrackEffects> usedEffects{};
        t.effects.erase(std::remove_if(t.effects.begin(),t.effects.end(),[&](const auto& fx){if(fx.slot<1||fx.slot>maxTrackEffects||usedEffects[(size_t)(fx.slot-1)])return true;usedEffects[(size_t)(fx.slot-1)]=true;return false;}),t.effects.end());
        for(auto& fx:t.effects){if(fx.name.size()>384)fx.name.resize(384);if(fx.pluginName.empty())fx.pluginName="Effect";}
        t.volume.normalise(length());t.panAutomation.normalise(length(),1.);
        if(t.parameters.size()>(size_t)maxParameterLanes)t.parameters.resize((size_t)maxParameterLanes);
        for(auto& lane:t.parameters){if(lane.slot< -1||lane.slot>maxTrackEffects){lane.slot=0;lane.index=-1;}lane.index=std::max(-1,lane.index);if(t.kind!=0||lane.slot!=-1||lane.index!=0)lane.generatedTone=false;if(lane.name.size()>128)lane.name.resize(128);lane.curve.normalise(length(),1.);}
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
        for(size_t k=0;k<t.clips.size();++k){auto& c=t.clips[k];c.stringsStyleAtGeneration=std::clamp(c.stringsStyleAtGeneration,-1,10);if(i==2&&c.stringsStyleAtGeneration<0)c.stringsStyleAtGeneration=settings.stringsMode;c.role=std::clamp(c.role,-1,5);if(t.freeTimeline){c.position=finite(c.position,0,0,131072);if(c.role<0)c.role=(int)sections[(size_t)sectionAtBeat(c.position)].type;c.span=finite(c.span,4,1./960.,maxClipBeats);}c.span=finite(c.span,0,0,maxClipBeats);if(c.span>0)c.span=std::max(t.freeTimeline?1./960.:.25,c.span);c.loopLength=finite(c.loopLength,0,0,maxClipBeats);if(c.loopLength>0)c.loopLength=std::max(.25,c.loopLength);
            c.loopPhase=c.loopLength>0?std::fmod(finite(c.loopPhase,0,0,maxClipBeats),c.loopLength):0;
            c.clipFadeIn=finite(c.clipFadeIn,0,0,30);c.clipFadeOut=finite(c.clipFadeOut,0,0,30);for(auto& lane:c.expression)lane.normalise(clipLength((int)(&t-tracks.data()),(int)(&c-t.clips.data())),1);
            c.audioOffset=finite(c.audioOffset,-1,-1,maxAudioRegionSeconds);c.audioDuration=finite(c.audioDuration,0,0,maxAudioRegionSeconds);
            if(c.audioOffset<0||c.audioDuration<=0){c.audioOffset=-1;c.audioDuration=0;}
            for(size_t f=0;f<3;++f)c.feel[f]=finite(c.feel[f],-1,-1,f==1?1.5:1.);
            {std::string cleaned;cleaned.reserve(c.name.size());for(unsigned char ch:c.name)if(ch>=32)cleaned.push_back((char)ch);while(!cleaned.empty()&&cleaned.front()==' ')cleaned.erase(cleaned.begin());while(!cleaned.empty()&&cleaned.back()==' ')cleaned.pop_back();if(cleaned.size()>128)cleaned.resize(128);c.name=std::move(cleaned);}
            if(isDrumTrack((int)i))for(auto* notes:{&c.notes,&c.loopSource})for(auto& n:*notes){if(t.instrument!="sample"){const int base=e.midiNote>=0?e.midiNote:fgd::defaultNote((int)i-4);n.pitch=(int)i-4==(int)fgd::Lane::conga&&std::abs(n.pitch-base)<=3?n.pitch:base;}n.lane=(int)i-4;}   /* the congas keep their three drums (base, one above, two below) */
            tidyNotes(c,clipLength((int)i,(int)k));if(c.loopLength>0){Clip source;source.notes=std::move(c.loopSource);tidyNotes(source,c.loopLength);c.loopSource=std::move(source.notes);}else c.loopSource.clear();}
        if(t.freeTimeline)std::stable_sort(t.clips.begin(),t.clips.end(),[](const Clip& a,const Clip& b){return a.position<b.position;});
    }
    for(auto& section:sections){auto& h=section.harmony;h.key=std::clamp(h.key,-1,11);h.scale=std::clamp(h.scale,-1,12);h.chordCount=std::clamp(h.chordCount,1,8);h.chordSteps=std::clamp(h.chordSteps,8,64);if(h.chordSteps!=8&&h.chordSteps!=16&&h.chordSteps!=32&&h.chordSteps!=64)h.chordSteps=16;for(auto& d:h.degrees)d=std::clamp(d,0,6);for(auto& f:h.freeChords)f=std::clamp(f,-1,35);}for(auto& t:tracks)t.eq.normalise();
    for(auto& bus:buses)bus.normalise();
    for(auto* rack:{&masterEffects,&returnEffects[0],&returnEffects[1]}){
        if(rack->parameters.size()>(size_t)maxParameterLanes)rack->parameters.resize(maxParameterLanes);
        for(auto& lane:rack->parameters){lane.generatedTone=false;lane.curve.normalise(length(),1.);}
    }
    masterGain=finite(masterGain,.8,0,2); masterAutomation.normalise(length());tempoAutomation.normalise(length(),1.); loopStart=finite(loopStart,0,0,length()); loopEnd=finite(loopEnd,length(),loopStart,length());
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
    for(auto* rack:{&masterEffects,&returnEffects[0],&returnEffects[1]})for(auto& lane:rack->parameters)all.push_back({&lane.curve,1.});
    return all;
}
bool Project::withinNoteBudget()const{size_t count=0;for(const auto& t:tracks)for(const auto& c:t.clips){count+=c.notes.size()+c.loopSource.size();if(count>2000000)return false;}return true;}
bool Project::insertSection(int index,SongSection section,int copyFrom) {
    if(sections.size()>=64) return false;
    size_t budget=0;for(const auto& t:tracks){for(const auto& c:t.clips)budget+=c.notes.size()+c.loopSource.size();if(!t.freeTimeline&&copyFrom>=0&&copyFrom<(int)t.clips.size()){const auto& c=t.clips[(size_t)copyFrom];budget+=c.notes.size()+c.loopSource.size();}if(budget>2000000)return false;}
    index=std::clamp(index,0,(int)sections.size());
    const bool wholeSong=loopStart==0&&loopEnd==length();
    std::vector<Clip> copies;
    for(auto& t:tracks) copies.push_back(copyFrom>=0&&copyFrom<(int)t.clips.size()?t.clips[(size_t)copyFrom]:Clip{});
    const double start=sectionStart(index),duration=std::clamp(section.bars,1,128)*4.;
    if(!splitFreeAt(start))return false;shiftFreeFrom(start,duration);
    for(auto [envelope,ceiling]:envelopes()){auto& points=envelope->points;auto original=points;for(auto& point:points)if(point.beat>=start)point.beat+=duration;
        if(copyFrom>=0&&copyFrom<(int)sections.size()){const double from=sectionStart(copyFrom),end=sectionStart(copyFrom+1);for(auto point:original)if(point.beat>=from&&point.beat<end&&point.beat-from<duration){point.beat=start+point.beat-from;points.push_back(point);}}
    }
    if(loopStart>=start)loopStart+=duration;
    if(loopEnd>start)loopEnd+=duration;
    sections.insert(sections.begin()+index,std::move(section));
    for(size_t i=0;i<tracks.size();++i) if(!tracks[i].freeTimeline)tracks[i].clips.insert(tracks[i].clips.begin()+index,copies[i]);
    if(wholeSong){loopStart=0;loopEnd=length();}
    normalise();return true;
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
void Project::resizeSection(int index,int bars,bool shiftAfter){
    if(index<0||index>=(int)sections.size())return;
    bars=std::clamp(bars,1,128);const double start=sectionStart(index),end=sectionStart(index+1);
    const double duration=bars*4.,delta=duration-(end-start);
    // Guide edits only move what follows the section; proportional scaling belongs
    // to resizeSectionLoop, where the aligned loop is actually re-timed.
    auto map=[&](double beat){if(shiftAfter)return beat>=end?beat+delta:beat;return beat>=end?beat+delta:beat>start?start+(beat-start)*duration/(end-start):beat;};
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
void chooseProgression(Settings& s,int preset) {
    const auto& all=sgk::progressionPresets(); const auto& pr=all[(size_t)std::clamp(preset,0,(int)all.size()-1)];
    s.manualChords.fill(false);s.freeChords.fill(-1);
    s.scale=pr.scale; auto slots=sgk::presetSlots(pr,s.extension);
    s.chordCount=std::clamp((int)slots.size(),1,8);
    for(int i=0;i<s.chordCount;++i) {s.degrees[(size_t)i]=std::max(0,slots[(size_t)i].degree);s.freeChords[(size_t)i]=slots[(size_t)i].freeCode;}
}
void chooseProgression(Project& p,int preset) {chooseProgression(p.settings,preset);}
namespace { static const std::vector<int>& genreProgressions(int genre); }   // defined with the song harmony below
static void drawHarmony(Settings& s,std::uint32_t seed) {
    if(isElectronicGenre(s.genre)){
        const auto before=s;const int count=std::clamp(before.chordCount,1,8);applyElectronicHarmony(s,seed);
        // Explicitly authored chromatic chords keep their slots and progression length.
        if(std::any_of(before.manualChords.begin(),before.manualChords.begin()+count,[](bool manual){return manual;})){
            s.chordCount=count;
            for(int i=0;i<count;++i)if(before.manualChords[(size_t)i]){s.freeChords[(size_t)i]=before.freeChords[(size_t)i];s.degrees[(size_t)i]=before.degrees[(size_t)i];s.manualChords[(size_t)i]=true;}
        }
        return;
    }
    // Keep the progression length and explicit manual chords; reroll automatic
    // slots from the genre vocabulary, including generated chromatic chords.
    const int count=std::clamp(s.chordCount,1,8);
    const auto& all=sgk::progressionPresets();
    std::vector<int> candidates,exact,complete;
    for(int i=0;i<(int)all.size();++i)if(all[(size_t)i].scale==s.scale){
        candidates.push_back(i);const auto size=(int)sgk::presetSlots(all[(size_t)i],s.extension).size();
        if(size==count)exact.push_back(i);
        if(size>0&&count%size==0)complete.push_back(i);
    }
    // Preserve a written phrase and its cadence. Cutting an arbitrary rotated
    // window out of a longer progression used to leave half a harmonic sentence.
    // With few exact matches, shorter phrases that repeat whole inside the length join them.
    if(exact.size()>=8)candidates=std::move(exact);
    else if(!complete.empty())candidates=std::move(complete);
    else if(!exact.empty())candidates=std::move(exact);
    // Latin House draws from its own phrases (montuno turnarounds, ii-V, the Andalusian cadence).
    // One of its phrases, in the song's key, starting on any of its chords (a montuno turns around well).
    if(s.genre==latinHouseGenre&&std::none_of(s.manualChords.begin(),s.manualChords.begin()+count,[](bool m){return m;})){
        const auto& latin=genreProgressions(latinHouseGenre);std::vector<int> same;for(int id:latin){Settings probe=s;chooseProgression(probe,id);if(probe.scale==s.scale)same.push_back(id);}   // the song's mode stays
        if(!same.empty()){const int key=s.key;chooseProgression(s,same[(size_t)(mix(seed^0x6a09e667U)%same.size())]);s.key=key;
        const int n=std::clamp(s.chordCount,1,8),rot=n>1?(int)(mix(seed^0xbb67ae85U)%(unsigned)n):0;
        if(rot){std::rotate(s.degrees.begin(),s.degrees.begin()+rot,s.degrees.begin()+n);std::rotate(s.freeChords.begin(),s.freeChords.begin()+rot,s.freeChords.begin()+n);}
        return;}}
    // Afro House stays hypnotic: its own written vamps, or a two-chord vamp repeated over the
    // length; never a diminished or half-diminished degree (audit M7, Afro only).
    if(s.genre==afroHouseGenre){
        static constexpr int afroPresets[]{0,1,6,7,17,26,35,65,70,72,78,96};
        std::vector<int> own;for(int i:candidates)if(std::find(std::begin(afroPresets),std::end(afroPresets),i)!=std::end(afroPresets))own.push_back(i);
        if(!own.empty()&&mix(seed^0x2545f491U)%10<6){candidates=std::move(own);}
        else{
            auto diminished=[&](int d){const auto c=sgk::buildChord(s.key,s.scale,d,sgk::Extension::triad);return c.third==3&&c.fifth==6;};
            static constexpr int vamps[][2]{{0,3},{0,6},{0,4},{0,2},{3,0},{6,0},{4,0},{2,0},{3,4},{3,6},{2,6},{2,3},{6,3},{4,3},{6,2},{0,5},{5,6},{5,0},{5,4},{5,3},{3,5},{2,5},{0,1},{1,4},{1,0},{3,1}};
            std::vector<int> usable;for(int i=0;i<(int)std::size(vamps);++i)if(!diminished(vamps[i][0])&&!diminished(vamps[i][1]))usable.push_back(i);
            if(!usable.empty()){const auto& pair=vamps[usable[(size_t)(mix(seed^0x3c6ef372U)%usable.size())]];
                for(int i=0;i<count;++i)if(!s.manualChords[(size_t)i]){s.degrees[(size_t)i]=pair[i%2];s.freeChords[(size_t)i]=-1;}
                return;}
        }
    }
    // A scale with only a handful of written progressions of this length would repeat
    // itself after a few rolls: share the draw with the genre's progression generator.
    const size_t pool=std::max<size_t>(candidates.size()*2,32);
    if(candidates.empty()||(s.genre!=afroHouseGenre&&s.genre!=latinHouseGenre&&mix(seed^0x51ed270bU)%pool>=candidates.size())){
        auto degrees=sgk::randomProgression(seed,count,s.scale,engineGenre(s.genre));
        // Two-chord vamps (Deep, Jackin', Afro) leave the walk only one move, so it kept
        // returning the same dozen pairs. Dorian and Aeolian draw from the full set of
        // idiomatic house vamps instead; each mode skips its own diminished degree.
        if(count==2&&(s.scale==1||s.scale==2)){
            static constexpr int vamps[][2]{{0,3},{0,6},{0,4},{0,2},{3,0},{6,0},{4,0},{2,0},{3,4},{3,6},{2,6},{2,3},{6,3},{4,3},{6,2},{0,5},{5,6},{5,0},{5,4},{5,3},{3,5},{2,5},{0,1},{1,4},{1,0},{3,1}};
            const int avoid=s.scale==1?5:1;std::vector<int> usable;
            for(int i=0;i<(int)std::size(vamps);++i)if(vamps[i][0]!=avoid&&vamps[i][1]!=avoid)usable.push_back(i);
            const auto& pair=vamps[usable[(size_t)(mix(seed^0x3c6ef372U)%usable.size())]];degrees[0]=pair[0];degrees[1]=pair[1];
        }
        for(int i=0;i<count;++i)if(!s.manualChords[(size_t)i]){s.degrees[(size_t)i]=degrees[(size_t)i];s.freeChords[(size_t)i]=-1;}
        return;
    }
    const int preset=candidates[(size_t)(mix(seed)%candidates.size())];
    const auto slots=sgk::presetSlots(all[(size_t)preset],s.extension);
    if(slots.empty())return;
    for(int i=0;i<count;++i){
        if(s.manualChords[(size_t)i])continue;
        const auto& slot=slots[(size_t)(i%(int)slots.size())];
        s.degrees[(size_t)i]=std::max(0,slot.degree);s.freeChords[(size_t)i]=slot.freeCode;
    }
}
void randomiseHarmony(Settings& s,std::uint32_t seed) {
    const auto before=s;
    for(unsigned attempt=0;attempt<32;++attempt){
        s=before;drawHarmony(s,attempt?mix(seed+attempt*0x9e3779b9U):seed);
        if(!sameHarmony(s,before))return;
    }
}
void randomiseHarmony(Project& p,std::uint32_t seed) {randomiseHarmony(p.settings,seed);}
void applyGenre(Project& p,int genre) {
    p.settings.freeTempo=false;
    p.normalise();
    p.settings.genre=normaliseGenre(genre);
    static const int presets[]{4,2,0,8,17,28,20,0,33,27,29,6,4};
    const auto& profile=genreProfile(p.settings.genre);
    const int feelOffset=(int)std::lround((finite(p.settings.density,.55,0,1)-.55)*4);
    const int takeOffset=(int)(mix(p.settings.seed+(std::uint32_t)p.settings.genre)%3)-1;
    p.settings.bpm=std::clamp(profile.preferredBpm+feelOffset+takeOffset,profile.minBpm,profile.maxBpm);
    p.settings.swing=fgd::genreSwing(drumGenre(p.settings.genre));
    p.settings.density=genreSound(p.settings.genre).density;
    p.settings.humanize=p.settings.genre==(int)Genre::neoSoul?.32:p.settings.genre==(int)Genre::chill?.28:.18;
    // A named factory groove from the previous genre must not override the new
    // style. Preserve explicitly authored user patterns and all other controls.
    for(size_t ti=0;ti<p.tracks.size();++ti){
        if(!p.tracks[ti].locked)for(auto& clip:p.tracks[ti].clips)if(!clip.locked)clip.feel={-1,-1,-1};
        auto& pattern=p.tracks[ti].engine.pattern;
        const int factoryCount=ti<3?(int)sgk::groovePatterns().size():ti==3?(int)fgg::guitarPatterns().size():ti==leadTrack?(int)fgl::leadPatterns().size():(int)fgd::drumPatterns().size();
        if(pattern>=0&&pattern<factoryCount)pattern=-1;
    }
    static_assert(std::size(presets)==(int)Genre::danceStabs+1);
    chooseProgression(p,presets[engineGenre(p.settings.genre)]);
    // Match each style to an actual named bass line in the original library.
    static const int bassLines[]{8,23,16,11,22,13,18,15,20,12,10,16,12};
    static_assert(std::size(bassLines)==(int)Genre::danceStabs+1);
    p.settings.bassMode=sgk::bassModeForLine(bassLines[engineGenre(p.settings.genre)]);
    // Latin shares the Afro rows of these tables: a new Latin project starts on a Latin progression and on a
    // bass line the tumbao is written over (before, only a seeded GENERATE reached the Latin tables).
    if(p.settings.genre==latinHouseGenre){int first=3;for(int id:genreProgressions(latinHouseGenre)){const auto& all=sgk::progressionPresets();if(id<(int)all.size()&&sgk::presetSlots(all[(size_t)id],p.settings.extension).size()==4){first=id;break;}}
        chooseProgression(p,first);p.settings.bassMode=sgk::bassModeForLine(8);}   // a four-chord Latin phrase, not the two-chord ii-V
    applyGenreSound(p);
    if(isElectronicGenre(p.settings.genre)){
        applyElectronicHarmony(p.settings,p.settings.seed);applyElectronicFeel(p.settings);
        if(p.settings.genre==electroHouseGenre&&!p.tracks[0].locked)p.tracks[0].engine.pump=.3;
        auto& lead=p.tracks[(size_t)leadTrack];if(!lead.locked){lead.engine.density=p.settings.genre==technoGenre?.36:.62;lead.engine.gate=.45;if(lead.instrument=="internal"){lead.sound=p.settings.genre==technoGenre?3:2;lead.gain=p.settings.genre==technoGenre?.18:.24;lead.engine.tone=p.settings.genre==technoGenre?.5:.78;}}
        auto& bass=p.tracks[1];if(!bass.locked){bass.engine.gate=.65;bass.engine.humanize=.3;bass.engine.pump=.3;}
    }
    if(p.settings.genre!=technoGenre)addElectronicToneAutomation(p);
}
namespace {
std::vector<SongSection> genreForm(int genre,int variant=0) {
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
        {{Section::intro,32,"Intro"},{Section::main,32,"Stab theme"},{Section::breakdown,16,"Breakdown"},{Section::buildup,16,"Tension"},{Section::drop,32,"Stab drop"},{Section::main,16,"Reprise"},{Section::outro,32,"Outro"}},
        {{Section::intro,32,"Drum intro"},{Section::main,32,"Hypnotic groove"},{Section::breakdown,16,"Atmosphere"},{Section::buildup,16,"Pressure build"},{Section::drop,32,"Techno drop"},{Section::main,32,"Evolution"},{Section::outro,32,"Drum outro"}},
        {{Section::intro,32,"Drum intro"},{Section::main,16,"Electro hook"},{Section::breakdown,16,"Filter break"},{Section::buildup,16,"Pressure build"},{Section::drop,32,"Electro drop"},{Section::breakdown,8,"Filter break"},{Section::buildup,8,"Pressure build"},{Section::drop,32,"Final drop"},{Section::outro,32,"Drum outro"}},
        {{Section::intro,16,"Percussion intro"},{Section::main,32,"Latin groove"},{Section::drop,16,"Montuno"},{Section::breakdown,16,"Timbales break"},{Section::buildup,8,"Rise"},{Section::drop,32,"Fiesta"},{Section::outro,16,"Percussion outro"}}
    };

    static_assert(std::size(forms)==numGenres);
    auto form=forms[normaliseGenre(genre)];
    // Curated alternatives develop the same genre vocabulary into different
    // arcs. Mix-in/out remain the actual first and last sections.
    if(variant==1){
        auto at=form.end()-1;at=form.insert(at,{Section::breakdown,8,"Second break"});++at;
        if(genreProfile(genre).rollingBuild){at=form.insert(at,{Section::buildup,8,"Closing lift"});++at;}
        form.insert(at,{Section::drop,16,"Closing return"});
    }else if(variant==2){
        bool removed=false,seenMain=false;
        for(size_t i=1;i+1<form.size();++i)if(form[i].type==Section::main){if(seenMain){form.erase(form.begin()+(int)i);removed=true;break;}seenMain=true;}
        if(!removed){
            const auto first=std::find_if(form.begin(),form.end(),[](const SongSection& sec){return sec.type==Section::drop;});
            if(first!=form.end()&&std::count_if(form.begin(),form.end(),[](const SongSection& sec){return sec.type==Section::drop;})>1){
                auto from=first;
                if(from!=form.begin()&&(from-1)->type==Section::buildup){--from;if(from!=form.begin()&&(from-1)->type==Section::breakdown)--from;}
                form.erase(from,first+1);removed=true;
            }
        }
        if(!removed){const auto before=std::find_if(form.begin()+1,form.end(),[](const SongSection& sec){return sec.type==Section::breakdown;});form.insert(before,{Section::main,8,"Theme response"});}
    }else if(variant==3)form.insert(form.begin()+1,{Section::drop,8,"Theme preview"});
    // After the intro the track does not start at once: a short break (no kick, the harmony alone)
    // comes first, and the groove lands after it.
    // Genres built on a driving groove (Jackin', Piano Jack, UK Garage, Techno, Electro House) go straight in.
    const auto g=(Genre)normaliseGenre(genre);const bool driving=g==Genre::jackinHouse||g==Genre::pianoJack||g==Genre::ukGarage||g==Genre::techno||g==Genre::electroHouse;
    if(!driving&&form.size()>2&&form[0].type==Section::intro&&form[1].type!=Section::breakdown&&form[1].type!=Section::buildup)form.insert(form.begin()+1,{Section::breakdown,8,"First break"});
    return form;
}
// A form nobody restructured: the factory section sequence of some genre.
bool factoryForm(const Project& p,int genre) {
    for(int variant=0;variant<4;++variant){const auto form=genreForm(genre,variant);if(form.size()!=p.sections.size())continue;
        bool same=true;for(size_t i=0;i<form.size();++i)if(form[i].type!=p.sections[i].type||form[i].name!=p.sections[i].name){same=false;break;}
        if(same)return true;
    }
    // ...or that form as GENERATE SONG shaped it (naturalForm): a "Rise" build-up inserted before a drop or made
    // of an 8-bar breakdown, an opening breakdown become the "Groove" (with the main after it merged in)
    for(int variant=0;variant<4;++variant){const auto form=genreForm(genre,variant);const auto& s=p.sections;size_t i=0,j=0;bool same=true;
        while(same&&i<s.size()){
            if(s[i].type==Section::buildup&&s[i].name=="Rise"&&!(j<form.size()&&form[j].type==Section::buildup&&form[j].name=="Rise")){if(j<form.size()&&form[j].type==Section::breakdown&&j+1<form.size()&&form[j+1].type==Section::drop)++j;++i;continue;}
            if(j>=form.size()){same=false;break;}
            if(s[i].type==Section::main&&s[i].name=="Groove"&&form[j].type==Section::breakdown){++i;++j;if(j<form.size()&&form[j].type==Section::main&&!(i<s.size()&&s[i].type==Section::main&&s[i].name==form[j].name))++j;continue;}
            if(form[j].type!=s[i].type||form[j].name!=s[i].name)same=false;++i;++j;}
        if(same&&j==form.size())return true;}
    return false;
}
int freshFormVariant(const Project& p,std::uint32_t seed){
    const int first=(int)(mix(seed+(unsigned)p.settings.genre*733U+307U)%4);
    for(int attempt=0;attempt<4;++attempt){const int variant=(first+attempt)%4;const auto next=genreForm(p.settings.genre,variant);
        bool same=next.size()==p.sections.size();for(size_t i=0;same&&i<next.size();++i)same=next[i].type==p.sections[i].type;
        if(!same)return variant;
    }return first;
}
}
// About half the sections that lead into a drop end with a short full-band stop: one beat or
// two. The choice belongs to the form, so it is drawn when the form is drawn.
static void chooseDropGaps(Project& p,std::uint32_t seed){
    for(size_t si=0;si<p.sections.size();++si){auto& s=p.sections[si];s.dropGap=0;
        if(si==1&&si+1<p.sections.size()&&s.type==Section::breakdown&&p.sections[0].type==Section::intro&&p.sections[si+1].type!=Section::drop){s.dropGap=(mix(seed^0x3c6ef372U)&1)?4:2;continue;}   // the first break stops, then the track starts
        if(si+1>=p.sections.size()||p.sections[si+1].type!=Section::drop||s.type==Section::drop||s.type==Section::intro||s.bars<2)continue;
        const auto r=mix(seed^(0x68e31da4U+(std::uint32_t)si*0x1b873593U));const bool build=s.type==Section::buildup;const int roll=(int)((r>>8)%100);
        // A build-up nearly always empties before the drop lands: half a bar, a whole bar, or (a long build) two bars
        // of nothing. Other sections stop less often and for less.
        if(build)s.dropGap=roll>=88?0:s.bars>=16&&roll<18?8:roll<62?4:2;   // one build in eight runs straight into the drop
        else if(s.type==Section::main&&s.bars>=16)s.dropGap=roll<50?4:2;   // a verse that runs straight into a drop lifts and stops like a build
        else if((int)(r%100)<55)s.dropGap=roll<40?4:roll<80?2:1;}
}
static void arrangeSongVariant(Project& p,int variant) {
    p.normalise();
    // Each style has its own phrase lengths and dramatic arc.
    // Transitions belong to the musical form; the instrument engines shape intro/outro.
    // Mix-in/out development is part of these sections, never appended elsewhere.
    const int genre=normaliseGenre(p.settings.genre);
    auto sections=genreForm(genre,variant);
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
    // The break after the intro is a different length from song to song: eight or sixteen bars.
    if(sections.size()>2&&sections[1].type==Section::breakdown&&sections[1].name=="First break"){static constexpr int lengths[]{8,16,8,16,8,8};sections[1].bars=lengths[mix(p.settings.seed^0x5bd1e995U^((std::uint32_t)variant*0x27d4eb2fU))%6];   /* whole 8-bar phrases: the DJ grid holds */}
    for(auto section:sections){int part=1;while(section.bars>128){bounded.push_back({section.type,128,section.name+" "+std::to_string(part++)});section.bars-=128;}if(part>1)section.name+=" "+std::to_string(part);bounded.push_back(std::move(section));}
    p.sections=std::move(bounded);for(auto& t:p.tracks)if(!isManualTrack(t.kind)){t.freeTimeline=false;t.clips.assign(p.sections.size(),{});}
    chooseDropGaps(p,p.settings.seed^((std::uint32_t)variant*0x9e3779b9U));
    p.normalise();p.loopStart=0;p.loopEnd=p.length();
}
void arrangeSong(Project& p){
    // Preserve the established factory default; explicit new seeds also vary form.
    const int variant=p.settings.seed==Settings{}.seed?0:(int)(mix(p.settings.seed+(unsigned)normaliseGenre(p.settings.genre)*733U+307U)%4);
    arrangeSongVariant(p,variant);
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
    for(int i=0;i<s.chordCount;++i) out.push_back(sgk::chordName(sgk::GrooveEngine::chordForSlot(settings,i),s.key,s.scale));
    return out;
}
static std::string recognizeChordPitchesContext(const std::vector<int>& midiNotes,int key,int scale,const std::string& preferred,bool sixths){
    unsigned pcs=0;int bass=128;for(int note:midiNotes)if(note>=0&&note<=127){pcs|=1U<<(note%12);bass=std::min(bass,note);}
    // Exact pitch-class sets, including octave doubles and inversions. Never
    // invent a missing root/third from a melody or an incomplete dyad.
    struct Shape{const char* suffix;int count;int tones[6];bool sixth=false;};
    static constexpr Shape shapes[]{
        {"maj7",4,{0,4,7,11}},{"7",4,{0,4,7,10}},{"m(maj7)",4,{0,3,7,11}},{"m7",4,{0,3,7,10}},
        {"m7b5",4,{0,3,6,10}},{"dim7",4,{0,3,6,9}},{"",3,{0,4,7}},{"m",3,{0,3,7}},
        {"dim",3,{0,3,6}},{"aug",3,{0,4,8}},{"sus2",3,{0,2,7}},{"sus4",3,{0,5,7}},
        {"add9",4,{0,2,4,7}},{"m(add9)",4,{0,2,3,7}},{"maj9",5,{0,2,4,7,11}},
        {"9",5,{0,2,4,7,10}},{"m9",5,{0,2,3,7,10}},{"m(maj9)",5,{0,2,3,7,11}},
        {"7sus4",4,{0,5,7,10}},{"maj7sus4",4,{0,5,7,11}},
        {"9sus4",5,{0,2,5,7,10}},{"maj9sus4",5,{0,2,5,7,11}},
        {"6/9",5,{0,2,4,7,9}},{"m6/9",5,{0,2,3,7,9}},
        {"7(b9)",5,{0,1,4,7,10}},{"7(#9)",5,{0,3,4,7,10}},
        {"7#5",4,{0,4,8,10}},{"maj7#5",4,{0,4,8,11}},{"7b5",4,{0,4,6,10}},
        {"11",6,{0,2,4,5,7,10}},{"m11",6,{0,2,3,5,7,10}},
        {"13",6,{0,2,4,7,9,10}},{"maj13",6,{0,2,4,7,9,11}},{"m13",6,{0,2,3,7,9,10}},
        {"6",4,{0,4,7,9},true},{"m6",4,{0,3,7,9},true}
    };
    struct Hit{int root;const char* suffix;};std::vector<Hit> hits;
    for(const auto& shape:shapes){if(shape.sixth&&!sixths)continue;for(int root=0;root<12;++root){unsigned mask=0;for(int i=0;i<shape.count;++i)mask|=1U<<((root+shape.tones[i])%12);if(mask==pcs)hits.push_back({root,shape.suffix});}}
    auto name=[&](const Hit& hit){return std::string(key<0?sgk::noteName(hit.root):sgk::spelledNote(hit.root,key,scale))+hit.suffix;};
    // C6 and Am7 (or Cm6 and Am7b5) can be the same sounding set.
    // Keep the written harmony only when every pitch actually matches it.
    if(!preferred.empty())for(const auto& hit:hits)if(name(hit)==preferred)return preferred;
    if(hits.size()==1)return name(hits.front());
    if(bass<128){const Hit* chosen=nullptr;for(const auto& hit:hits)if(hit.root==bass%12){if(chosen)return {};chosen=&hit;}if(chosen)return name(*chosen);}
    return {};
}
std::string recognizeChordPitches(const std::vector<int>& midiNotes,int key,int scale){
    // Context-free callers retain the conventional seventh interpretation.
    return recognizeChordPitchesContext(midiNotes,key,scale,{},false);
}
std::string chordHeardInClip(const std::vector<Note>& notes,double from,double to,int key,int scale,const std::string& fallback){
    return chordHeardInClip(notes,from,to,key,scale,fallback,fallback);
}
std::string chordHeardInClip(const std::vector<Note>& notes,double from,double to,int key,int scale,const std::string& fallback,const std::string& preferred){
    if(!std::isfinite(from)||!std::isfinite(to)||!(to>from))return fallback;
    struct Event{double time;int pitch,delta;};std::vector<Event> events;
    for(const auto& n:notes)if(n.pitch>=0&&n.pitch<=127&&n.velocity>0&&std::isfinite(n.start)&&std::isfinite(n.length)&&n.length>0&&n.start<to&&n.start+n.length>from){events.push_back({std::max(from,n.start),n.pitch,1});events.push_back({std::min(to,n.start+n.length),n.pitch,-1});}
    std::sort(events.begin(),events.end(),[](const Event& a,const Event& b){return a.time<b.time;});
    std::array<int,128> active{};std::vector<std::pair<std::string,double>> heard;double polyphonic=0;
    const double minimumSlice=std::min(1./16.,(to-from)/8.);
    for(size_t i=0;i<events.size();){const double at=events[i].time;do{active[(size_t)events[i].pitch]+=events[i].delta;++i;}while(i<events.size()&&events[i].time<=at+1e-9);
        if(i==events.size())break;const double duration=events[i].time-at;if(duration<minimumSlice)continue;
        std::vector<int> sounding;unsigned pcs=0;for(int pitch=0;pitch<128;++pitch)if(active[(size_t)pitch]>0){sounding.push_back(pitch);pcs|=1U<<(pitch%12);}
        if((pcs&(pcs-1))==0)continue;int count=0;for(unsigned mask=pcs;mask;mask&=mask-1)++count;if(count<3)continue;
        polyphonic+=duration;const auto name=recognizeChordPitchesContext(sounding,key,scale,preferred,true);if(name.empty())continue;
        auto found=std::find_if(heard.begin(),heard.end(),[&](const auto& h){return h.first==name;});if(found==heard.end())heard.emplace_back(name,duration);else found->second+=duration;
    }
    std::string agreed;for(const auto& h:heard)if(h.second>=std::min(1./8.,(to-from)/4.)){if(!agreed.empty())return fallback;agreed=h.first;}
    if(agreed.empty())return fallback;const auto chosen=std::find_if(heard.begin(),heard.end(),[&](const auto& h){return h.first==agreed;});
    return chosen->second>=polyphonic*.5?agreed:fallback;
}
std::string chordNameAt(const Project& p,double beat){
    beat=std::max(0.,beat);const auto s=resolveSectionHarmony(p,p.sectionAtBeat(beat));
    return sgk::chordName(chordAtBeat(s,beat),s.key,s.scale);
}
void chordCell(const Project& p,double beat,double& start,double& end,std::string& name){
    beat=std::max(0.,beat);const int sec=p.sectionAtBeat(beat);const auto harmony=resolveSectionHarmony(p,sec);
    const double chordBeats=std::max(.25,harmony.chordSteps*.25);const double secStart=p.sectionStart(sec);
    const double cell=std::floor((beat-secStart)/chordBeats);start=std::max(secStart,secStart+cell*chordBeats);   // cells from the section start, as the chords cycle
    const double secEnd=p.sectionStart(sec+1);const double next=secStart+(cell+1)*chordBeats;end=secEnd>start?std::min(next,secEnd):next;
    if(end<=start)end=start+chordBeats;name=chordNameAt(p,start);
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
    static_assert(std::size(harmony)==(int)Genre::danceStabs+1);
    int entry=0;
    if(track==leadTrack)entry=24;
    else if(track<4)entry=harmony[engineGenre(genre)][isAfroLatinGenre(genre)&&track==2?1:track];   // Afro/Latin: the strings come in with the bass, never a pad over drums alone
    else{
        const int lane=track-4;
        // Kick, closed hats and shaker provide a countable pulse from bar one.
        static constexpr int drums[]{0,16,8,16,0,8,16,0,8,8,16,24,32};
        entry=drums[lane];
        if(isAfroLatinGenre(genre)&&(lane==7||lane==9))entry=0; // Afro/Latin shaker and congas lead
    }
    if(isElectronicGenre(genre)&&track==1)entry=genre==technoGenre?8:16;
    if(genre==technoGenre&&track==leadTrack)entry=16;
    int at=std::clamp(((entry*bars/32+7)/8)*8,0,bars);
    // A short Afro or Latin intro/outro (16 bars or less): its first 8 bars stay rhythm only (the DJ's blend),
    // then bass and strings come in together (never a pad alone over the drums); keys, guitar and lead
    // arrive with the main part.
    if(isAfroLatinGenre(genre)&&bars<=16&&(track<4||track==leadTrack))return (track==1||track==2)&&bars>=16?8:bars;
    // Strings scheduled on the final bar never sound: a 32-bar cue scaled onto
    // a shorter intro lands on the last bar and the whole part is removed.
    // Keep at least eight bars, and half of a longer intro or outro.
    if((track==2||(track==1&&isAfroLatinGenre(genre)))&&bars>=8&&at>bars-8){   // Afro/Latin: the bass moves with the strings (same entry)
        const int keep=std::min(bars,std::max(8,(bars/2/8)*8));
        at=bars-keep;
    }
    return at;
}
void shapeBandRoles(Clip& clip,const Project& p,int track,int section) {
    const auto& sec=p.sections[(size_t)section];const int genre=p.settings.genre;
    const auto& e=p.tracks[(size_t)track].engine;
    const int user=track<3?sgk::userPatternIndex():track==3?fgg::userPatternIndex():track==leadTrack?fgl::userPatternIndex():fgd::userPatternIndex();
    if(e.pattern==user)return;
    if(sec.type==Section::buildup&&genreProfile(genre).rollingBuild&&section+1<(int)p.sections.size()&&p.sections[(size_t)section+1].type==Section::drop&&(track==1||track==4)){
        const double end=sec.bars*4.-1.;
        // Treat an early humanised attack as belonging to the silent beat,
        // rather than leaving a millisecond fragment before the boundary.
        clip.notes.erase(std::remove_if(clip.notes.begin(),clip.notes.end(),[&](const Note& n){return n.start>=end-1./32.;}),clip.notes.end());
        for(auto& n:clip.notes)n.length=std::min(n.length,end-n.start);
    }
    if(sec.bars<8||(sec.type!=Section::main&&sec.type!=Section::drop))return;
    // A short explicit source must contain its phrase before it is repeated.
    // Applying a full-section entry mask here could loop silence indefinitely.
    if(clip.loopLength>0&&clip.loopLength<32)return;
    // A long verse or drop moves in waves instead of holding one level: it opens a little lighter,
    // climbs, empties out in the middle (the kick rests for a few bars, the answering parts leave,
    // then everything comes back) and ends on its loudest eight bars.
    if(const int blocks=sec.bars/8;blocks>=3&&clip.loopLength<=0){
        const int dip=blocks>=4?blocks/2:-1;const double kickRest=blocks>=6?16.:8.;
        clip.notes.erase(std::remove_if(clip.notes.begin(),clip.notes.end(),[&](const Note& n){const int block=(int)(n.start/32+1e-6);const double in=n.start-block*32.;
            if(block==dip){if(track==4)return in<kickRest-1./32.;if(track==leadTrack||track==3||track==10||track==12||track==14||track==15)return true;if(track==5||track==6)return in<kickRest-1./32.;}
            if(block==dip-1&&(track==4||track==1))return in>=32.-2.-1./32.;   // the last two beats before the dip: the floor drops away
            if(block==0&&blocks>=3&&sec.type==Section::main&&(track==12||track==14||track==15))return true;
            return false;}),clip.notes.end());
        if(track!=4&&track!=1)for(auto& n:clip.notes){const int block=std::min(blocks-1,(int)(n.start/32+1e-6));
            const double level=block==dip?.88:block==blocks-1?1.07:block==0?.95:block==dip-1?1.03:1.;n.velocity=std::clamp((int)std::lround(n.velocity*level),1,127);}
        for(auto& n:clip.notes){const int block=(int)(n.start/32+1e-6);if(block==dip-1&&(track==4||track==1)&&n.start+n.length>(block+1)*32.-2.)n.length=std::max(1./64.,(block+1)*32.-2.-n.start);}
    }
    // Repeated verses/drops develop across the song, independently of seed
    // variation. Preserve the kick/bass foundation and explicit user patterns.
    int preceding=0,total=0;
    for(int i=0;i<(int)p.sections.size();++i)if(p.sections[(size_t)i].type==Section::main||p.sections[(size_t)i].type==Section::drop){++total;if(i<section)++preceding;}
    const double development=total>1?double(preceding)/(total-1):0.;
    if(track!=1&&track!=4)for(auto& n:clip.notes)n.velocity=std::clamp((int)std::lround(n.velocity*(1.+.12*development)),1,127);
    const bool later=development>=.5;
    auto active=[&](double start){const int bar=(int)(start/4),phrase=bar%8;
        const int block=bar/8;
        if(track==leadTrack){
            if(isElectronicGenre(genre))return sec.type==Section::drop?(later||phrase<6):(phrase<4||phrase>=6);
            return sec.type==Section::drop?(later||phrase<6):phrase>=(later?2:4);
        }
        if(track==10||track==12){
            if(sec.type==Section::drop)return later||phrase>=2;
            return block>0?phrase>=2:phrase>=6;
        }
        // Strings follow the harmony for the whole verse and chorus. Gating them
        // to the last two bars of a phrase removed every chord that did not
        // start in that window, so most songs never grew a string part.
        if(track==3&&!p.settings.guitarLead){
            if(genre==(int)Genre::jackinHouse)return phrase%2==1; // percussive guitar answers the chopped keys
            if(isAfroLatinGenre(genre))return phrase%4>=(later?1:2); // percussion/guitar answers
            if(genre==(int)Genre::deepHouse||genre==(int)Genre::ninetiesHouse||genre==(int)Genre::danceStabs)return phrase>=(later?4:6);
            return phrase%4>=(later?1:2); // opening phrases leave room for the piano
        }
        // Different percussion layers announce phrases; never gate the kick,
        // backbeat or closed hat. These are compositional rests, not audio mute.
        if(track==14||track==15){
            if(genre==(int)Genre::jackinHouse&&(track==14||track==15))return true; // cowbell and tom are the jack, not a late answer
            return isAfroLatinGenre(genre)?phrase>=(later?1:2):phrase>=(later?2:4);
        }
        return true;
    };
    auto& notes=clip.notes;
    for(auto& n:notes){
        const double boundary=(std::floor(n.start/4)+1)*4;
        // An early humanised downbeat belongs to the following phrase. Avoid
        // chopping it into a click immediately before a compositional rest.
        if(boundary-n.start<=1./32.&&active(n.start)!=active(boundary)){
            if(!active(boundary))n.length=-1;
            else{n.length-=boundary-n.start;n.start=boundary;}
        }
    }
    notes.erase(std::remove_if(notes.begin(),notes.end(),[&](const Note& n){return n.length<=0||!active(n.start);}),notes.end());
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
// New takes choose visible global values from the genre's musical range.
void varyGenerationFeel(Project& p,std::uint32_t seed){
    auto& s=p.settings;
    auto pick=[&](double previous,int lo,int hi,unsigned salt){const int count=hi-lo+1;int value=lo+(int)(mix(seed+salt)%(unsigned)count);if(value==(int)std::lround(previous*100)&&count>1)value=lo+(value-lo+1)%count;return value/100.;};
    const int centre=(int)std::lround(genreSound(s.genre).density*100);
    s.density=pick(s.density,std::max(12,centre-18),std::min(90,centre+18),1700U);
    const int base=(int)std::lround(fgd::genreSwing(drumGenre(s.genre))*100);
    int low=std::max(0,base-12),high=std::min(120,base+18);
    if(s.genre==(int)Genre::jackinHouse){low=36;high=72;}
    if(s.genre==technoGenre){low=0;high=8;}
    if(s.genre==electroHouseGenre){low=4;high=20;}
    s.swing=pick(s.swing,low,high,1901U);
    int humanLow=10,humanHigh=35;
    if(s.genre==(int)Genre::neoSoul||s.genre==(int)Genre::chill){humanLow=22;humanHigh=48;}
    if(s.genre==technoGenre){humanLow=8;humanHigh=24;}
    s.humanize=pick(s.humanize,humanLow,humanHigh,2203U);
}
void varySongTempo(Project& p,std::uint32_t seed){
    auto& s=p.settings;const auto& profile=genreProfile(s.genre);
    const int current=(int)std::lround(s.bpm),span=profile.maxBpm-profile.minBpm;
    if(span>0){int pick=profile.minBpm+(int)(mix(seed+1301U)%(unsigned)span);if(pick>=current)++pick;s.bpm=pick;}
}
bool editableTake(const Project& p,Scope scope,int track,int section,int ti,int ci,int lastClip=-1){
    const auto& t=p.tracks[(size_t)ti];if(!hasGenerator(t)||t.locked||t.clips[(size_t)ci].locked)return false;
    if((scope==Scope::track||scope==Scope::clip)&&ti!=track)return false;
    if(scope==Scope::clip)return ci>=section&&ci<=(lastClip<0?section:lastClip);
    return scope!=Scope::section||(t.freeTimeline?p.sectionAtBeat(p.clipStart(ti,ci)):ci)==section;
}
void prepareGenerationFeel(Project& p,Scope scope,int track,int section,std::uint32_t seed,int lastClip=-1){
    bool editable=false;
    for(int ti=0;ti<(int)p.tracks.size();++ti)for(int ci=0;ci<(int)p.tracks[(size_t)ti].clips.size();++ci)
        editable=editable||editableTake(p,scope,track,section,ti,ci,lastClip);
    if(!editable)return;
    for(int ti=0;ti<(int)p.tracks.size();++ti){auto& t=p.tracks[(size_t)ti];if(!hasGenerator(t)||t.locked)continue;bool selected=false;
        for(int ci=0;ci<(int)t.clips.size();++ci){auto& c=t.clips[(size_t)ci];if(c.locked)continue;
            if(editableTake(p,scope,track,section,ti,ci,lastClip)){c.feel={-1,-1,-1};selected=true;}
            else c.feel={takeFeel(t.engine.density,c.feel[0],p.settings.density),takeFeel(t.engine.swing,c.feel[1],p.settings.swing),takeFeel(t.engine.humanize,c.feel[2],p.settings.humanize)};
        }
        if(selected)t.engine.density=t.engine.swing=t.engine.humanize=-1;
    }
    varyGenerationFeel(p,seed);
}
// Main sections can resolve the harmony and groove together. Very long/odd
// cycles continue across boundaries; mix passages keep their own DJ grid.
static int formPhraseBars(const Project& p){
    const int steps=std::max(1,p.settings.chordSteps);
    const int harmonic=std::max(1,(std::max(1,p.settings.chordCount)*steps)/16);
    int motif=1;for(const auto& t:p.tracks)motif=std::max(motif,t.engine.grooveBars);
    int phrase=8;
    for(int g:{harmonic,motif}){int a=phrase,b=g;while(b){const int r=a%b;a=b;b=r;}phrase=phrase/std::max(1,a)*g;}
    return phrase<=32&&phrase%8==0?phrase:8;
}
// A clock target (Target duration, with its tolerance) builds the form to that length: whole
// phrases first, then 4-bar steps, growing the shortest main/drop or trimming the longest
// section; the tempo moves only for the last few seconds, inside the genre's range.
// An empty target leaves the generated form exactly as before.
// A section resize by the form tools (VARY FORM, target duration): what follows moves with the section end,
// free MIDI and audio clips included. A free clip inside the bars a shorter section gives up is never cut:
// that section keeps its length.
static bool resizeFormSection(Project& p,int index,int bars){
    const bool free=std::any_of(p.tracks.begin(),p.tracks.end(),[](const Track& t){return t.freeTimeline&&!t.clips.empty();});
    if(!free){p.resizeSection(index,bars);return true;}
    const double oldEnd=p.sectionStart(index+1),newEnd=p.sectionStart(index)+std::clamp(bars,1,128)*4.;
    if(newEnd>oldEnd){if(!p.splitFreeAt(oldEnd))return false;p.shiftFreeFrom(oldEnd,newEnd-oldEnd);}
    else if(newEnd<oldEnd){for(const auto& t:p.tracks)if(t.freeTimeline)for(const auto& c:t.clips)if(c.position<oldEnd-1e-6&&c.position+c.span>newEnd+1e-6)return false;
        p.shiftFreeFrom(oldEnd,newEnd-oldEnd);}
    p.resizeSection(index,bars,true);return true;
}
void fitFormToTarget(Project& p){
    const auto& s=p.settings;if(!s.targetEnabled||!(s.targetSeconds>0)||p.sections.empty()||!(s.bpm>0))return;
    // Audio clips and handwritten tracks no longer freeze the form: they keep their place in time.
    if(std::any_of(p.tracks.begin(),p.tracks.end(),[](const Track& t){return t.locked||std::any_of(t.clips.begin(),t.clips.end(),[](const Clip& c){return c.locked;});}))return;
    const int phrase=std::clamp(formPhraseBars(p),4,32);
    auto totalBars=[&]{int n=0;for(const auto& x:p.sections)n+=x.bars;return n;};
    // the song's real length in seconds (the MASTER TEMPO lane included), as bars at the project tempo
    auto songBars=[&]{p.normalise();return p.secondsAt(p.length())*s.bpm/240.;};
    const auto body=[](Section t){return t==Section::main||t==Section::drop;};
    std::vector<char> fixed(p.sections.size(),0);   // a section a free clip keeps from shrinking
    for(int guard=0;guard<512;++guard){
        const double target=s.targetSeconds*s.bpm/240.,tolerance=s.targetToleranceSeconds*s.bpm/240.,diff=target-songBars();
        if(std::abs(diff)<=tolerance+1e-9)break;
        int step=std::abs(diff)>=phrase*.75?phrase:4;if(step==4&&std::abs(diff)<2)break;
        int pick=-1;
        if(diff>0){for(int pass=0;pass<2&&pick<0;++pass)for(size_t i=0;i<p.sections.size();++i){const auto& x=p.sections[i];if((pass==0)!=body(x.type)||x.bars+step>128)continue;if(pick<0||x.bars<p.sections[(size_t)pick].bars)pick=(int)i;}
            if(pick<0)break;if(!resizeFormSection(p,pick,p.sections[(size_t)pick].bars+step))break;}
        else{for(int attempt=0;attempt<2&&pick<0;++attempt){if(attempt){if(step<=4)break;step=4;}   // nothing can lose a whole phrase: breaks and builds can still lose four bars
            for(size_t i=0;i<p.sections.size();++i){const auto& x=p.sections[i];const int floor=body(x.type)?8:x.type==Section::buildup?4:x.type==Section::breakdown?4:8;if(fixed[i]||x.bars-step<floor)continue;if(pick<0||x.bars>p.sections[(size_t)pick].bars)pick=(int)i;}
            if(pick<0)break;if(!resizeFormSection(p,pick,p.sections[(size_t)pick].bars-step)){fixed[(size_t)pick]=1;continue;}}}
    }
    const double seconds=p.secondsAt(p.length());
    if(!p.tempoAutomation.active()&&std::abs(seconds-s.targetSeconds)>s.targetToleranceSeconds){const auto& profile=genreProfile(s.genre);const double low=s.freeTempo?30.:profile.minBpm,high=s.freeTempo?300.:profile.maxBpm;
        p.settings.bpm=std::clamp(std::round(totalBars()*240./s.targetSeconds*10)/10,low,high);}
    p.normalise();
}
void varyForm(Project& p,std::uint32_t seed) {
    const bool locked=std::any_of(p.tracks.begin(),p.tracks.end(),[](const Track& t){return t.locked||std::any_of(t.clips.begin(),t.clips.end(),[](const Clip& c){return c.locked;});});
    if(locked||p.sections.empty())return;   // audio and handwritten tracks keep their clips in time; only locks keep the form
    const int phrase=formPhraseBars(p);
    auto pickLength=[&](const int* choices,int count,int index,int grid)->int{
        std::vector<int> valid;for(int i=0;i<count;++i)if(choices[i]%grid==0)valid.push_back(choices[i]);
        if(valid.empty())return grid;
        return valid[(size_t)(mix(seed+(std::uint32_t)index*97U)%valid.size())];
    };
    std::vector<int> next;int before=0,after=0;
    for(size_t i=0;i<p.sections.size();++i){
        const auto type=p.sections[i].type;
        const int io[]{16,32},bb[]{8,16},body[]{16,24,32,48};
        const int* choices=body;int count=4;
        if(type==Section::intro||type==Section::outro){choices=io;count=2;}
        else if(type==Section::breakdown||type==Section::buildup){choices=bb;count=2;}
        const int grid=type==Section::main||type==Section::drop?phrase:8;
        const int shortPhrase[]{8,16};const bool preview=p.sections[i].name=="Theme preview"||p.sections[i].name=="Theme response";
        const int pick=preview?pickLength(shortPhrase,2,(int)i,8):pickLength(choices,count,(int)i,grid);
        next.push_back(pick);before+=p.sections[i].bars;after+=pick;
    }
    if(after==before){
        const auto type=p.sections.back().type;
        const int io[]{16,32},bb[]{8,16},body[]{16,24,32,48};
        const int* choices=body;int count=4;
        if(type==Section::intro||type==Section::outro){choices=io;count=2;}
        else if(type==Section::breakdown||type==Section::buildup){choices=bb;count=2;}
        for(int k=0;k<count;++k)if(choices[k]!=next.back()&&choices[k]%8==0){next.back()=choices[k];break;}
    }
    for(size_t i=0;i<next.size();++i)if(next[i]!=p.sections[i].bars)resizeFormSection(p,(int)i,next[i]);
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
    for(int genre=0;genre<numGenres;++genre)if(genre!=p.settings.genre&&factoryForm(p,genre))otherGenre=true;
    const bool authored=std::any_of(p.sections.begin(),p.sections.end(),[](const SongSection& sec){return (sec.harmony.enabled&&!sec.harmony.automatic)||sec.preDropSilence||sec.snareRoll||sec.reverseCrash;})
;   // GENERATE SONG renews everything but the genre: audio and handwritten tracks no longer keep the form
    if(!authored&&(otherGenre||factoryForm(p,p.settings.genre))){
        arrangeSongVariant(p,freshFormVariant(p,p.settings.seed));
        if(!hadRegion)p.clearLoop();else if(!wholeLoop){p.loopStart=std::min(from,p.length());p.loopEnd=std::min(to,p.length());}
        p.loop=wasLooping&&p.hasLoopRegion();
        return;
    }
    for(auto& t:p.tracks)if(t.freeTimeline&&!isManualTrack(t.kind)){t.freeTimeline=false;t.clips.assign(p.sections.size(),{});}
    p.normalise();
}
// The form as these genres are built: Afro and Deep House start their groove after the intro (the
// breakdown comes later); every drop is prepared by a build-up (Neo Soul and Chill, song forms, and the
// jackin styles, groove to groove, may go straight into it): the last 8 bars of a section of 16 or more before it become the build-up, or an
// 8-bar breakdown turns into one (eight-bar phrases stay whole).
// A drop that follows a drop continues it. Not applied when tracks or clips are locked.
static void naturalForm(Project& p,bool grooveAfterIntro=true){
    if(p.sections.size()<3||std::any_of(p.tracks.begin(),p.tracks.end(),[](const Track& t){return t.locked||std::any_of(t.clips.begin(),t.clips.end(),[](const Clip& c){return c.locked;});}))return;
    const int g=p.settings.genre;auto& s=p.sections;
    if(grooveAfterIntro&&(g==deepHouseGenre||g==(int)Genre::afroHouse)&&s[0].type==Section::intro&&s[1].type==Section::breakdown){s[1].type=Section::main;s[1].name="Groove";
        if(s[2].type==Section::main){s[1].bars+=s[2].bars;s.erase(s.begin()+2);}}   // one groove section (its length is the two)
    if(g==(int)Genre::neoSoul||g==(int)Genre::chill||g==(int)Genre::jackinHouse||g==(int)Genre::pianoJack)return;   // song forms; the jack goes from groove to groove
    for(size_t i=1;i<s.size();++i){if(s[i].type!=Section::drop||s[i-1].type==Section::buildup||s[i-1].type==Section::drop||s[i-1].type==Section::intro||s[i-1].bars<8)continue;   /* an intro drops straight into the groove (the DJ way) */
        if(s[i-1].bars>=16){s[i-1].bars-=8;SongSection rise;rise.type=Section::buildup;rise.bars=8;rise.name="Rise";rise.dropGap=s[i-1].dropGap>0?s[i-1].dropGap:4;s[i-1].dropGap=0;s.insert(s.begin()+(long)i,rise);++i;}   // eight-bar phrases stay whole
        else if(s[i-1].type==Section::breakdown&&s[i-1].bars==8&&i>=2&&s[i-2].type!=Section::buildup){s[i-1].type=Section::buildup;s[i-1].name="Rise";}}
    p.normalise();
}
// The genre's library of complete phrases (two, four or eight chords).
static const std::vector<int>& genreProgressions(int genre){
    static const std::vector<int> progressions[]{
        {4,5,8,11,12,19,27,33,54,59,60,76},
        {2,3,4,8,12,19,29,54,57,59,61,62},
        {0,1,6,7,20,27,65,66,67,68,69,70,71,72,73,94,95,97,101,104,105,109},
        {2,8,11,12,19,22,24,40,51,53,54,58,59,62,63,64,85,86},
        {17,18,28,30,31,34,35,37,74,75,78,79,80,94,95,97},
        {17,28,30,31,33,37,54,58,74,75,76,79,94,101},
        {9,17,18,27,28,31,34,37,67,71,75,78,79,95},
        {0,1,6,7,17,26,35,65,70,72,78,96},
        {9,21,32,33,4,12,54,57,60,74,76,77,80},
        {4,6,8,27,28,33,54,56,59,60,76},
        {2,3,19,24,29,12,40,41,46,51,53,54,57,61,85},
        {0,6,7,10,20,27,65,67,68,69,70,71,72,91},
        {4,6,9,17,27,28,33,54,56,60,74,76}
    };
    // Latin House is brighter and moves every bar (montuno turnarounds, ii-V, the
    // Andalusian cadence); Afro House stays on hypnotic minor and modal vamps.
    static const std::vector<int> latinProgressions{3,4,12,21,22,32,33,49,52,59,74,90,93};
    return genre==latinHouseGenre?latinProgressions:progressions[engineGenre(genre)];
}
// A song is not one loop: the drop (the chorus) answers the main part with another phrase of the genre in
// the same key, the breakdown takes two of its chords at half the speed, and the build-up's phrase turns
// on the dominant into the drop. Loop genres (Deep, Afro, Chill) keep the main loop in the drop; the
// electronic ones keep one loop everywhere. Sections with chords set by hand stay as they are.
static void sectionHarmonies(Project& p,std::uint32_t seed){
    const auto& s=p.settings;if(isElectronicGenre(s.genre))return;
    const bool loop=s.genre==deepHouseGenre||s.genre==(int)Genre::afroHouse||s.genre==(int)Genre::chill;
    // The drop starts on the tonic (I or i), so the build-up's dominant resolves; a later drop takes another
    // phrase, so the second chorus is not a copy of the first. A phrase that holds the tonic elsewhere is
    // rotated to start on it.
    const auto& choices=genreProgressions(s.genre);std::vector<Settings> drops;
    auto onTonic=[](Settings a){const int n=std::clamp(a.chordCount,1,8);for(int r=0;r<n;++r)if(a.degrees[(size_t)r]==0&&a.freeChords[(size_t)r]<0){std::rotate(a.degrees.begin(),a.degrees.begin()+r,a.degrees.begin()+n);std::rotate(a.freeChords.begin(),a.freeChords.begin()+r,a.freeChords.begin()+n);return std::optional<Settings>(a);}return std::optional<Settings>();};
    if(!loop){const auto first=mix(seed+4111U)%choices.size();
        for(size_t n=0;n<choices.size()&&drops.size()<2;++n){Settings answer=s;chooseProgression(answer,choices[(first+n)%choices.size()]);answer.key=s.key;
            if(answer.scale!=s.scale||sameHarmony(answer,s)||answer.chordCount<2)continue;const auto tonic=onTonic(answer);if(!tonic||sameHarmony(*tonic,s))continue;   // rotated, it must still contrast with the main part
            if(std::any_of(drops.begin(),drops.end(),[&](const Settings& d){return sameHarmony(d,*tonic);}))continue;drops.push_back(*tonic);}}
    // one phrase only: the later drop keeps it with another cadence (its last chord becomes IV, or vi)
    if(drops.size()==1&&drops.front().chordCount>=3){Settings again=drops.front();const size_t last=(size_t)(again.chordCount-1);again.degrees[last]=again.degrees[last]==3?5:3;again.freeChords[last]=-1;if(!sameHarmony(again,s)&&!sameHarmony(again,drops.front()))drops.push_back(again);}
    const bool found=!drops.empty();const Settings drop=found?drops.front():s;
    Settings breakdown=found?drop:s;{const int count=breakdown.chordCount;const int second=count>=4?2:count>1?1:0;
        breakdown.degrees={breakdown.degrees[0],breakdown.degrees[(size_t)second],0,0,0,0,0,0};breakdown.freeChords={breakdown.freeChords[0],breakdown.freeChords[(size_t)second],-1,-1,-1,-1,-1,-1};
        breakdown.chordCount=2;breakdown.chordSteps=std::min(64,s.chordSteps*2);}
    /* eight bars: the progression repeats to fill them and the turn comes only in the last one (a short vamp
       would otherwise hit the dominant every two or three bars) */
    Settings build=s;{const int perChord=build.chordSteps/16;const int slots=perChord>=1&&8%perChord==0?8/perChord:0;
        if(slots>build.chordCount&&slots<=8){const int count=std::max(1,build.chordCount);for(int i=count;i<slots;++i){build.degrees[(size_t)i]=build.degrees[(size_t)(i%count)];build.freeChords[(size_t)i]=build.freeChords[(size_t)(i%count)];}build.chordCount=slots;}}
    if(build.chordCount>=2){const size_t last=(size_t)(build.chordCount-1);
        if(s.genre==afroHouseGenre||s.scale==1){build.degrees[last]=6;build.freeChords[last]=10;}   // Afro and Dorian turn on the major bVII (in any scale), not a V7
        else{build.degrees[last]=4;build.freeChords[last]=24+7;}}   // a dominant on the fifth (major V/V7 even in minor, not v)
    auto store=[&](SectionHarmony& h,const Settings& from){h.enabled=true;h.automatic=true;h.key=from.key;h.scale=from.scale;h.chordCount=from.chordCount;h.chordSteps=from.chordSteps;h.degrees=from.degrees;h.freeChords=from.freeChords;h.manualChords.fill(false);};
    int dropIndex=0;
    for(auto& section:p.sections){auto& h=section.harmony;if(h.enabled&&!h.automatic)continue;h.enabled=false;h.automatic=false;   /* the user's own section chords stay */
        if(section.type==Section::drop&&found){store(h,drops[(size_t)(dropIndex++)%drops.size()]);}else if(section.type==Section::breakdown)store(h,breakdown);else if(section.type==Section::buildup&&s.chordCount>=2)store(h,build);}
}
void newSongHarmony(Project& p,std::uint32_t seed) {
    auto& s=p.settings;const auto before=s;
    if(isElectronicGenre(s.genre)){
        s.key=(before.key+1+(int)(mix(seed+907U)%11))%12;applyElectronicHarmony(s,seed);return;
    }
    // Complete genre phrases avoid the unrelated modal/diminished turns of an
    // unrestricted walk. All these library entries contain two, four or eight
    // chords, so automatic dance forms retain their phrase boundaries.
    const auto& choices=genreProgressions(s.genre);
    const auto first=mix(seed+701U)%choices.size();
    for(size_t i=0;i<choices.size();++i) {
        chooseProgression(p,choices[(first+i)%choices.size()]);
        if(s.chordCount==4&&mix(seed+349U)%3==0){
            const auto opening=chordAtBeat(s,0);const auto at=mix(seed+503U)%choices.size();
            for(size_t n=0;n<choices.size();++n){auto answer=s;chooseProgression(answer,choices[(at+n)%choices.size()]);
                if(answer.scale!=s.scale||answer.chordCount!=4||sameHarmony(answer,s))continue;
                const auto anchor=chordAtBeat(answer,0);
                if(anchor.rootPc!=opening.rootPc||anchor.third!=opening.third||anchor.fifth!=opening.fifth)continue;
                // An eight-chord A/B phrase returns to the same harmonic anchor
                // and develops it with another complete, compatible four-chord line.
                for(int slot=0;slot<4;++slot){s.degrees[(size_t)slot+4]=answer.degrees[(size_t)slot];s.freeChords[(size_t)slot+4]=answer.freeChords[(size_t)slot];}
                s.chordCount=8;break;
            }
        }
        if(!sameHarmony(s,before))break;
    }
    // Exclude the current key: a new take must audibly move its tonal centre.
    s.key=(before.key+1+(int)(mix(seed+907U)%11))%12;
    static const std::vector<int> bassLines[]{
        {8,12,13}, {10,11,23}, {7,16,17}, {10,11,23}, {13,14,22},
        {8,12,13,14}, {9,18,19}, {8,15,16}, {0,1,2,3,20,21,22},   /* Afro: offbeat, its own groove, the deep sub hold (no pumping octaves) */
        {8,12,13}, {4,5,10,23}, {7,16,17}, {8,12,13,14}
    };
    static const std::vector<int> latinBass{2,9,12};   // root/fifth/octave anticipations, no boogie or gospel walks
    const auto& bass=s.genre==latinHouseGenre?latinBass:bassLines[engineGenre(s.genre)];
    auto at=mix(seed+1103U)%bass.size();
    if(sgk::bassModeForLine(bass[at])==s.bassMode)at=(at+1)%bass.size();
    s.bassMode=sgk::bassModeForLine(bass[at]);
}
}
// A layout operation: reuse the existing takes, including silence. Never call
// the generators here. Work on a copy so locks/resource limits fail atomically.
bool rearrangeSong(Project& project,std::uint32_t seed){
    if(project.sections.empty())return false;
    for(const auto& t:project.tracks){if(t.locked)return false;for(const auto& c:t.clips)if(c.locked)return false;}
    auto plan=project;plan.tracks.resize(trackCount);
    for(auto& t:plan.tracks){t.freeTimeline=false;t.clips.clear();}
    arrangeSongVariant(plan,freshFormVariant(project,seed));varyForm(plan,seed);
    // Reusing MIDI must not shorten an already clean DJ outro into its harmonic
    // portion. Preserve factory mix passages at their original lengths.
    if(factoryForm(project,project.settings.genre)&&project.sections.front().type==Section::intro&&project.sections.back().type==Section::outro){plan.sections.front().bars=project.sections.front().bars;plan.sections.back().bars=project.sections.back().bars;}
    // Vary against the current form too, rather than occasionally choosing the
    // same layout again on a repeated press.
    bool identical=plan.sections.size()==project.sections.size();
    for(size_t s=0;identical&&s<plan.sections.size();++s)identical=plan.sections[s].type==project.sections[s].type&&plan.sections[s].bars==project.sections[s].bars;
    if(identical)plan.sections.back().bars=plan.sections.back().bars<=112?plan.sections.back().bars+16:plan.sections.back().bars-16;
    fitFormToTarget(plan);chooseDropGaps(plan,seed);
    std::vector<size_t> source;
    for(size_t s=0;s<plan.sections.size();++s){std::vector<size_t> matches;
        for(size_t old=0;old<project.sections.size();++old)if(project.sections[old].type==plan.sections[s].type)matches.push_back(old);
        if(matches.empty())for(size_t old=0;old<project.sections.size();++old)if(project.sections[old].type==Section::main||project.sections[old].type==Section::drop)matches.push_back(old);
        source.push_back(matches.empty()?std::min(s,project.sections.size()-1):matches[(size_t)(mix(seed+(std::uint32_t)s*97u)%matches.size())]);
    }
    auto next=project;
    if(!next.foldFreeTracks())return false;
    const auto folded=next.tracks;
    next.sections=plan.sections;
    auto remap=[&](VolumeAutomation& target,const VolumeAutomation& original)->bool{
        if(original.points.empty())return true;
        // Time-scale the source section's envelope, keeping curved segments.
        // An explicit last tick isolates discontinuities between reused sections.
        const bool enabled=original.enabled;auto curve=original;curve.enabled=true;target.points.clear();
        for(size_t s=0;s<source.size();++s){const double from=project.sectionStart((int)source[s]),until=project.sectionStart((int)source[s]+1),to=next.sectionStart((int)s),end=next.sectionStart((int)s+1),scale=(end-to)/(until-from);
            std::vector<double> cuts{from};for(const auto& point:curve.points)if(point.beat>from&&point.beat<until)cuts.push_back(point.beat);cuts.push_back(until);
            for(size_t c=0;c+1<cuts.size();++c){const double a=cuts[c],b=cuts[c+1],v=curve.valueAt(a),last=curve.valueAt(std::nextafter(b,a)),middle=curve.valueAt((a+b)*.5);
                const double bend=std::abs(last-v)>1.e-12?std::clamp(4.*(middle-(v+last)*.5)/(last-v),-1.,1.):0.;
                target.points.push_back({to+(a-from)*scale,v,bend});
            }
            target.points.push_back({s+1==source.size()?end:end-1./960.,curve.valueAt(std::nextafter(until,from)),0});
            if(target.points.size()>16384)return false;
        }
        target.enabled=enabled;return true;
    };
    size_t notes=0;
    for(size_t t=0;t<next.tracks.size();++t){auto& track=next.tracks[t];const bool wasFree=project.tracks[t].freeTimeline;const auto& old=wasFree?folded[t]:project.tracks[t];
        if(!track.freeTimeline){track.clips.clear();for(size_t s=0;s<source.size();++s){Clip clip;if(source[s]<old.clips.size())clip=old.clips[source[s]];
                const double duration=next.sections[s].bars*4.;
                const double previous=wasFree?project.sectionStart((int)source[s]+1)-project.sectionStart((int)source[s]):project.clipLength((int)t,(int)source[s]);
                if(!(previous>1./960.))return false;
                if(duration!=previous&&!resizedLoop(clip,previous,duration))return false;
                clip.span=0;clip.position=0;clip.role=(int)next.sections[s].type;track.clips.push_back(std::move(clip));
            }
            if(!remap(track.volume,old.volume)||!remap(track.panAutomation,old.panAutomation))return false;
            for(size_t lane=0;lane<track.parameters.size();++lane)if(!remap(track.parameters[lane].curve,old.parameters[lane].curve))return false;
        }
        for(const auto& clip:track.clips){notes+=clip.notes.size()+clip.loopSource.size();if(notes>2000000)return false;}
    }
    if(!remap(next.masterAutomation,project.masterAutomation))return false;
    for(int rack:{masterEffectTrack,returnEffectTrack,returnEffectTrack+1}){
        auto& target=next.effectTarget(rack).parameters;const auto& original=project.effectTarget(rack).parameters;
        for(size_t lane=0;lane<target.size();++lane)if(!remap(target[lane].curve,original[lane].curve))return false;
    }
    next.loopStart=0;next.loopEnd=next.length();next.normalise();project=std::move(next);return true;
}

// A free MIDI clip may cross independently harmonised sections. Partition only
// editable generated material; loop sources keep their period and phase through
// the normal timeline slice operation. Work on a copy so resource limits cannot
// leave a partially split selection behind.
static int partitionHarmonyClips(Project& p,Scope scope,int track,int section){
    struct Cut{int track,index;double beat;bool loop;};std::vector<Cut> cuts;
    for(int ti=(int)p.tracks.size()-1;ti>=0;--ti){const auto& t=p.tracks[(size_t)ti];
        if((!isHarmonicTrack(t.kind)&&!(t.vocalChops&&t.instrument=="internal"))||!t.freeTimeline||t.locked)continue;
        for(int ci=(int)t.clips.size()-1;ci>=0;--ci){const auto& c=t.clips[(size_t)ci];
            if(c.audioOffset>=0||!editableTake(p,scope,track,section,ti,ci))continue;
            const double begin=p.clipStart(ti,ci),end=begin+p.clipLength(ti,ci);
            for(int si=(int)p.sections.size()-1;si>0;--si){const double at=p.sectionStart(si);if(at<=begin+1./960.||at>=end-1./960.)continue;
                const auto a=resolveSectionHarmony(p,si-1),b=resolveSectionHarmony(p,si);
                if(a.key!=b.key||a.scale!=b.scale||a.chordSteps!=b.chordSteps||!sameHarmony(a,b))cuts.push_back({ti,ci,at,c.loopLength>0});
            }
        }
    }
    if(cuts.empty())return -1;
    auto edited=p;int lastClip=scope==Scope::clip?section:-1;
    for(const auto& cut:cuts){if(edited.splitClip(cut.track,cut.index,cut.beat)<0)throw std::length_error("Cannot partition the generated harmony selection");if(!cut.loop)for(int ci:{cut.index,cut.index+1}){auto& c=edited.tracks[(size_t)cut.track].clips[(size_t)ci];c.loopLength=c.loopPhase=0;c.loopSource.clear();}if(scope==Scope::clip)++lastClip;}
    // Keep track/engine references held by the inspector valid.
    std::vector<bool> changed(p.tracks.size());for(const auto& cut:cuts)changed[(size_t)cut.track]=true;
    for(size_t ti=0;ti<changed.size();++ti)if(changed[ti])p.tracks[ti].clips=std::move(edited.tracks[ti].clips);
    return lastClip;
}
static void generateNotes(Project& p,Scope scope,int track,int section,std::uint32_t seed,int lastClip=-1);
static void randomiseAutomationRange(Project&,Scope,int,int,std::uint32_t,int,double);
// The string engine renders one arrangement per harmony, so a new take used to return the
// same notes. Each take now chooses a register, a voicing (full, without the top, bottom or
// inner voices) and a bowing for long chords. Pitch classes never change.
// A genre has few written drum grooves, so a single lane often came back identical. Each
// take adds its own small ornaments on top of the groove: the kick may push into the next
// phrase, the other lanes get one or two quiet ghost hits every two bars. Nothing the groove wrote
// is removed: downbeats, backbeat and four-on-the-floor stay where they are.
static void ornamentDrumTake(Clip& clip,int track,double beats){
    const int lane=track-4;if(lane==12||clip.notes.size()<2)return;   // lane 12 is the crash
    const auto h=mix(clip.seed^(0x2545f491U+(std::uint32_t)lane*0x9e3779b1U));
    const auto occupied=[&](double at){return std::any_of(clip.notes.begin(),clip.notes.end(),[&](const Note& n){return std::abs(n.start-at)<.1;});};
    const Note model=clip.notes.front();std::vector<Note> added;
    if(lane==0){
        // Only complete four-on-the-floor bars at a phrase end take a push.
        const int choice=(int)(h%4);if(choice==0)return;
        for(double bar=28;bar+4<=beats+1e-9;bar+=32){int floor=0;for(const auto& n:clip.notes)if(n.start>=bar-1e-6&&n.start<bar+4&&std::abs(n.start-std::round(n.start))<.05)++floor;if(floor<4)continue;
            for(double at:{choice!=2?bar+3.5:-1.,choice>=2?bar+2.75:-1.})if(at>=0&&!occupied(at)){auto n=model;n.start=at;n.velocity=std::clamp(model.velocity-22,1,127);added.push_back(n);}}
    }else{
        const int mode=(int)(h%4);if(mode==0)return;
        for(double bar=0;bar+8<=beats+1e-9;bar+=8){const auto r=h;   // the same ornament every two bars: a groove, not noise
            for(int ghost=0;ghost<(mode==3?2:1);++ghost){const double at=bar+(double)((r>>(ghost*5))%8)+.5;   // the straight offbeat eighths: no swung sixteenth is touched
                if(!occupied(at)&&at<beats){auto n=model;n.start=at;n.length=std::min(model.length,.2);n.velocity=std::clamp(38+(int)((r>>12)%18),1,127);added.push_back(n);}}
        }
    }
    clip.notes.insert(clip.notes.end(),added.begin(),added.end());std::sort(clip.notes.begin(),clip.notes.end(),[](const Note& a,const Note& b){return a.start<b.start;});
}
// Some accompaniment voicings add fixed colour tones (a major 9th, a minor 7th) whatever the
// key: over a Phrygian or Mixolydian chord they land outside both the chord and the scale,
// and as held or on-beat notes they sound wrong. Those move to the nearest chord or scale
// tone; short off-beat passing and blue notes are left alone.
static void fitSustainedTensions(Clip& clip,const Project& p,int section,double clipStart,double period){
    const auto s=activeHarmony(p,section);const auto& scale=sgk::scaleIntervals(s.scale);
    const auto fits=[&](int pitch,const sgk::Chord& chord){const int pc=((pitch-s.key)%12+12)%12,rel=((pitch-chord.rootPc)%12+12)%12;
        if(std::find(scale.begin(),scale.end(),pc)!=scale.end()||rel==0)return true;
        for(int iv:{chord.third,chord.fifth,chord.seventh,chord.ninth,chord.eleventh,chord.thirteenth,chord.sixth})if(iv>=0&&iv%12==rel)return true;return false;};
    bool changed=false;
    for(auto& n:clip.notes){if(n.length<.5&&!(n.length>=.25&&std::abs(n.start-std::round(n.start))<.05))continue;
        const double wrapped=period>0&&clip.loopPhase>0?std::fmod(n.start-clip.loopPhase+period*2,period):n.start;const auto chord=chordAtBeat(s,clipStart+wrapped);
        if(fits(n.pitch,chord))continue;
        for(int step:{-1,1,-2,2})if(n.pitch+step>=0&&n.pitch+step<=127&&fits(n.pitch+step,chord)){n.pitch+=step;changed=true;break;}}
    if(!changed)return;
    // A moved tone may now double another voice of the same chord: keep one.
    std::sort(clip.notes.begin(),clip.notes.end(),byStartPitch);
    clip.notes.erase(std::unique(clip.notes.begin(),clip.notes.end(),[](const Note& a,const Note& b){return a.pitch==b.pitch&&std::abs(a.start-b.start)<1./64.;}),clip.notes.end());
}
// NEW FEEL: another performance of the same notes. Each attack (a chord moves as one) gets
// its own small timing offset and dynamics; pitches, order and lengths stay as written.
static void applyFeelSeed(Clip& clip,std::uint32_t feelSeed,double humanize){
    if(clip.notes.empty())return;const double amount=std::clamp(std::max(humanize,.25),0.,1.);
    const double reach=.006+.018*amount;const int accent=(int)std::lround(5+9*amount);
    std::sort(clip.notes.begin(),clip.notes.end(),byStartPitch);
    // Notes closer than a sixteenth-triplet (a chord, a grace note and its target) move together.
    double groupStart=-1,last=-1,offset=0,previousMoved=-1;int groupVelocity=0;std::uint32_t r=0;
    for(size_t i=0;i<clip.notes.size();++i){auto& n=clip.notes[i];
        if(groupStart<0||n.start-last>1./12.){groupStart=n.start;r=mix(feelSeed^(std::uint32_t)std::lround(n.start*960.)*0x9e3779b1U);
            offset=((double)(r&0xffff)/65535.-.5)*2.*reach;groupVelocity=(int)((r>>16)%(unsigned)(2*accent+1))-accent;
            // Never let a group reach the one before it: the order of attacks stays as written.
            if(previousMoved>=0)offset=std::max(offset,previousMoved+1./32.-n.start);}
        last=n.start;
        const double end=n.start+n.length,moved=std::max(0.,n.start+offset);n.start=moved;n.length=std::max(1./64.,end-moved);previousMoved=std::max(previousMoved,moved);
        n.velocity=std::clamp(n.velocity+groupVelocity+(int)(mix(r+(std::uint32_t)i)%5)-2,1,127);}
}
static void varyStringsTake(Clip& clip){
    if(clip.notes.empty())return;
    const auto v=mix(clip.seed^0x7f4a7c15U);const int registerChoice=(int)(v%3);int voicing=(int)((v>>4)%4);const int bowing=(int)((v>>9)%4);   // sustained, bowed again halfway, with a breath, half length
    int low=127,high=0;for(const auto& n:clip.notes){low=std::min(low,n.pitch);high=std::max(high,n.pitch);}
    const int shift=registerChoice==1&&high+12<=96?12:registerChoice==2&&low-12>=36?-12:0;
    std::vector<std::vector<Note>> chords;
    for(size_t i=0;i<clip.notes.size();){
        size_t j=i;while(j<clip.notes.size()&&std::abs(clip.notes[j].start-clip.notes[i].start)<1./32.)++j;
        chords.emplace_back(clip.notes.begin()+(long)i,clip.notes.begin()+(long)j);std::sort(chords.back().begin(),chords.back().end(),[](const Note& a,const Note& b){return a.pitch<b.pitch;});i=j;
    }
    const auto reduce=[](std::vector<Note> chord,int mode){
        if(chord.size()>=3){if(mode==1)chord.pop_back();else if(mode==2)chord.erase(chord.begin());else if(mode==3)chord={chord.front(),chord.back()};}
        return chord;};
    const auto pitches=[](const std::vector<Note>& chord){std::vector<int> p;for(const auto& n:chord)p.push_back(n.pitch);return p;};
    // A thinner voicing must never hide a chord change behind common tones.
    for(size_t k=1;voicing!=0&&k<chords.size();++k)if(pitches(chords[k])!=pitches(chords[k-1])&&pitches(reduce(chords[k],voicing))==pitches(reduce(chords[k-1],voicing)))voicing=0;
    std::vector<Note> out;
    for(const auto& full:chords)for(auto n:reduce(full,voicing)){n.pitch+=shift;
        if(bowing==1&&n.length>=4){const double half=std::floor(n.length*.5);auto second=n;second.start+=half;second.length=n.length-half;second.velocity=std::max(1,n.velocity-8);n.length=half;out.push_back(n);out.push_back(second);}
        else{if(n.length>=2)n.length*=bowing==2?.75:bowing==3?.5:1.;out.push_back(n);}}
    std::sort(out.begin(),out.end(),byStartPitch);clip.notes=std::move(out);
}
// Compare the audible entry and its phrase, rather than the distant turnaround.
// No velocity, octave/transposition or sub-sixteenth timing can count as variety.
static std::vector<std::pair<int,int>> primaryGuitarPhrase(const Clip& clip){
    std::vector<std::pair<int,int>> result;if(clip.notes.empty())return result;
    const double from=std::floor(clip.notes.front().start/4)*4,to=std::max(from+4,std::min(from+16,28.));int first=-1;
    for(const auto& note:clip.notes){if(note.start<from)continue;if(note.start>=to)break;if(note.length<1./64.)continue;if(first<0)first=note.pitch;result.emplace_back((int)std::lround((note.start-from)*4),note.pitch-first);}
    std::sort(result.begin(),result.end());result.erase(std::unique(result.begin(),result.end()),result.end());return result;
}
static std::uint32_t freshGuitarMotif(const Project& p,int index,std::uint32_t seed,std::uint32_t performance){
    const auto& previous=p.tracks[3].clips[(size_t)index];const auto old=primaryGuitarPhrase(previous);
    if(old.empty()||performance==1||(previous.loopLength>32&&previous.loopPhase>0))return seed;
    auto probe=p;
    // Keep form, harmony and instrument settings, but discard unrelated MIDI.
    // Candidate work is limited to an eight-bar source and sixteen family draws.
    for(auto& t:probe.tracks)for(auto& c:t.clips){c.notes.clear();c.loopSource.clear();}
    auto& track=probe.tracks[3];track.engine.pattern=-1;track.engine.riffSeed=0;
    auto& clip=track.clips[(size_t)index];clip.span=std::min(32.,p.clipLength(3,index));if(clip.loopLength>32)clip.loopLength=32;
    for(unsigned attempt=0;attempt<16;++attempt){const auto candidate=attempt?mix(seed+attempt*0x9e3779b9U):seed;
        clip.seed=performance;clip.motifSeed=candidate;clip.notes.clear();clip.loopSource.clear();
        generateNotes(probe,Scope::clip,3,index,0);
        const auto next=primaryGuitarPhrase(clip);if(!next.empty()&&next!=old)return candidate;
    }
    return seed;
}


// Guitar and lead share the piano's section colour. Their engines only have a few
// chord types, so each piano chord is reduced to the nearest one that does not
// put a third against a sus, or a natural 9 against b9/#9.
fgg::Chord bandChord(int key,int scale,int degree,int free,sgk::Extension ext){
    sgk::Chord c;
    if(free>=0){sgk::GrooveSettings g;g.keyRoot=key;g.scale=scale;g.extension=ext;g.progressionLength=1;g.freeChords[0]=free;g.progression[0]=0;c=sgk::GrooveEngine::chordForSlot(g,0);}
    else c=sgk::buildChord(key,scale,degree,ext);
    using T=fgg::ChordType;int type=(int)T::major;
    if(c.suspended)type=(int)T::sus4;
    else if(c.sixth>=0&&c.seventh<0&&c.third==4&&c.fifth==7)type=(int)T::sixNine;
    else if(c.third==3&&c.fifth==6)type=(int)(c.seventh==9?T::dim7:c.seventh==10?T::halfDim:T::dim);
    else if(c.third==4&&c.fifth==8)type=(int)T::aug;
    else if(c.third==3&&c.seventh==11)type=(int)T::mMaj7;
    else if(c.third==3&&c.eleventh==5)type=(int)T::m11;
    else if(c.third==4&&c.seventh==10&&c.thirteenth==9&&c.ninth!=1&&c.ninth!=3)type=(int)T::dom13;
    else if(c.third==3)type=c.seventh<0?(int)T::minor:(c.ninth==2?(int)T::m9:(int)T::m7);
    else if(c.seventh<0)type=(int)T::major;
    else if(c.seventh==11)type=c.ninth==2?(int)T::maj9:(int)T::maj7;
    else if(c.seventh==10)type=(int)(c.ninth==1?T::dom7b9:c.ninth==2?T::dom9:T::dom7);
    // The band chord must be a subset of what the piano plays: a type that adds a tone the piano
    // does not have (a b7 under a sus without seventh, a #11 under a diatonic 11) clashes. Keep
    // the type above when it fits, else the fullest type made only of the piano's tones.
    unsigned piano=1u;for(int i=0;i<c.tones.size;++i)piano|=1u<<(unsigned)(((c.tones[i])%12+12)%12);
    auto bitsIn=[](unsigned m){int n=0;for(;m;m&=m-1)++n;return n;};auto extra=[&](int t){return bitsIn(fgg::chordTypeMask(t)&~piano);};
    // When no type on the root fits (a sus chord with a major seventh), an upper triad made only
    // of the piano's tones takes its place (G major over C sus maj7).
    int root=c.rootPc;
    if(extra(type)>0){int best=type,score=-100000;
        for(int shift=0;shift<12;++shift)for(int t=0;t<fgg::numChordTypes;++t){unsigned m=fgg::chordTypeMask(t);m=((m<<(unsigned)shift)|(m>>(unsigned)(12-shift)))&0xfffu;
            const int value=10*bitsIn(m&piano)-100*bitsIn(m&~piano)-(shift?15:0)+(!shift&&c.third>=0&&((m>>(unsigned)c.third)&1u)?5:0);
            if(value>score){score=value;best=t;root=(c.rootPc+shift)%12;}}
        type=best;}
    return {root,type};
}
// Every section that follows a filled one gets a crash on its first beat: each
// section is rendered on its own, so the fill on the last bar had no answer.
// The note is inserted in order (not appended) and replaces any crash already on
// that beat, so rebuilding with the stored seeds produces exactly the same clip.
static void addSectionCrashes(Project& p,Scope scope,int track,int section){
    constexpr int crashLane=12;
    for(int ti=4;ti<(int)p.tracks.size();++ti){
        auto& t=p.tracks[(size_t)ti];
        if(!isDrumTrack(ti)||ti-4!=crashLane||t.locked)continue;
        if(!t.engine.afterFill)continue;
        for(int si=1;si<(int)p.sections.size();++si){
            const double beat=p.sectionStart(si);
            // Works for the fixed form and for tracks detached by timeline edits:
            // find the clip that actually covers the downbeat.
            for(size_t ci=0;ci<t.clips.size();++ci){
                auto& clip=t.clips[ci];
                if(!editableTake(p,scope,track,section,ti,(int)ci))continue;
                const double start=p.clipStart(ti,(int)ci);
                const double length=p.clipLength(ti,(int)ci);
                if(beat<start||beat>=start+length)continue;
                const double at=beat-start;
                clip.notes.erase(std::remove_if(clip.notes.begin(),clip.notes.end(),
                    [at,crashLane](const Note& n){return n.lane==crashLane&&std::abs(n.start-at)<1./16.;}),clip.notes.end());   // also a humanized crash a few ms off the downbeat: one hit, not a flam
                const auto place=std::upper_bound(clip.notes.begin(),clip.notes.end(),at,
                    [](double value,const Note& n){return value<n.start;});
                const double gate=std::min(defaultDrumNoteLength(ti),length-at);
                clip.notes.insert(place,{at,gate,fgd::defaultNote(crashLane),110,crashLane});
                break;
            }
        }
    }
}
// Into a drop, genres whose build-up has no snare roll still rise: over the last two bars before the
// stop (or the drop) a percussion riser speeds up and grows, in the genre's own voice: congas for
// Afro and Latin, toms for Soulful, Gospel, Disco and Neo Soul, a soft shaker for Deep and Chill.
// Repeated drops alternate the figure (eighths into sixteenths, or a steady sixteenth swell), so no
// two transitions sound the same. Never over a roll the user asked for; grid-locked; the stop
// before the drop (applied afterwards) keeps its silence.
static void preDropRisers(Project& p,Scope scope,int track,int section,int lastClip=-1){
    const auto g=(Genre)p.settings.genre;if(isElectronicGenre(p.settings.genre)||genreProfile(p.settings.genre).rollingBuild)return;
    const int ti=g==Genre::afroHouse||g==Genre::latinHouse?13:g==Genre::deepHouse||g==Genre::chill?11:g==Genre::soulful||g==Genre::gospel||g==Genre::discoHouse||g==Genre::neoSoul?15:-1;
    if(ti<0||ti>=(int)p.tracks.size()||p.tracks[(size_t)ti].locked||p.tracks[(size_t)ti].mute)return;
    const bool soft=ti==11;int occurrence=0;
    for(int si=0;si+1<(int)p.sections.size();++si){const auto& sec=p.sections[(size_t)si];if(p.sections[(size_t)si+1].type!=Section::drop||sec.snareRoll||sec.bars<4)continue;
        if(sec.type!=Section::buildup&&!(sec.type==Section::main&&sec.bars>=16)){++occurrence;continue;}
        const double finish=p.sectionStart(si+1)-std::max(0,sec.dropGap),from=finish-8.;const bool accelerate=occurrence++%2==0;
        auto& t=p.tracks[(size_t)ti];
        for(size_t ci=0;ci<t.clips.size();++ci){if(!editableTake(p,scope,track,section,ti,(int)ci,lastClip))continue;auto& c=t.clips[ci];if(c.audioOffset>=0||c.loopLength>0)continue;
            const double start=p.clipStart(ti,(int)ci),length=p.clipLength(ti,(int)ci);const double a=std::max(0.,from-start),b=std::min(length,finish-start);if(b<=a)continue;
            c.notes.erase(std::remove_if(c.notes.begin(),c.notes.end(),[&](const Note& n){return n.start>=a-1./32.&&n.start<b;}),c.notes.end());
            for(double at=a;at<b-1e-6;){const double done=(start+at-from)/8.,step=accelerate?(done<.5?.5:.25):.25;
                const int velocity=(int)std::lround((soft?38:52)+(soft?34:58)*done);c.notes.push_back({at,std::min(step*.6,b-at),fgd::defaultNote(ti-4),std::clamp(velocity,1,127),ti-4});at+=step;}
            tidyNotes(c,length);}}
}
static void transitionMacros(Project& p,Scope scope,int track,int section,int lastClip=-1){for(int si=0;si+1<(int)p.sections.size();++si){if(p.sections[(size_t)(si+1)].type!=Section::drop)continue;const auto& options=p.sections[(size_t)si];const double finish=p.sectionStart(si+1);for(int ti:{0,1,4,5,6,16}){auto& t=p.tracks[(size_t)ti];for(size_t ci=0;ci<t.clips.size();++ci){if(!editableTake(p,scope,track,section,ti,(int)ci,lastClip))continue;auto& c=t.clips[ci];const double start=p.clipStart(ti,(int)ci),length=p.clipLength(ti,(int)ci);if(start>=finish||start+length<=finish-4)continue;if(options.preDropSilence&&(ti==1||ti==4)){const double silence=finish-2-start;c.notes.erase(std::remove_if(c.notes.begin(),c.notes.end(),[silence](const Note& n){return n.start>=silence;}),c.notes.end());for(auto& n:c.notes)n.length=std::min(n.length,std::max(0.,silence-n.start));}if(options.snareRoll&&(ti==5||ti==6)){const double from=std::max(0.,finish-4-start),to=std::min(length,finish-start);c.notes.erase(std::remove_if(c.notes.begin(),c.notes.end(),[from,to](const Note& n){return n.start>=from&&n.start<to;}),c.notes.end());for(double at=from;at<to-1e-6;){const double remaining=to-at,step=remaining>2?.5:remaining>1?.25:.125;c.notes.push_back({at,std::min(step*.7,to-at),fgd::defaultNote(ti-4),std::clamp((int)(65+60*(1-remaining/4)),1,127),ti-4});at+=step;}}if(options.reverseCrash&&ti==16&&t.instrument=="sample"){const double from=std::max(0.,finish-4-start),to=std::min(length,finish-start);if(to>from)c.notes.push_back({from,to-from,t.sampleRoot,105,12,true});}tidyNotes(c,options.preDropSilence&&(ti==1||ti==4)?std::clamp(finish-2-start,0.,length):length);}}}}
// A build-up has to rise: the engines render it as flat as a main section except for the
// snare. Seeded per song and section: the bass usually leaves for the last bar or two, the
// kick sometimes for the last bar, and the percussion grows from a softer start.
static void shapeBuildups(Project& p,Scope scope,int track,int section,int lastClip=-1){
    for(int si=0;si+1<(int)p.sections.size();++si){const auto& sec=p.sections[(size_t)si];const bool build=sec.type==Section::buildup,lift=sec.type==Section::main&&sec.bars>=16;   // a form without a build-up: the verse's last eight bars do its job
        if((!build&&!lift)||p.sections[(size_t)(si+1)].type!=Section::drop||sec.bars<2)continue;
        const double finish=p.sectionStart(si+1),begin=lift?finish-32.:p.sectionStart(si),length=finish-begin;const auto r=mix(p.settings.seed^(0x7a3d5c1fU+(std::uint32_t)si*0x85ebca6bU));
        const double bassOut=(r%10)<7?(lift?4.:std::min(8.,std::max(4.,length/4))):0.,kickOut=lift?4.:length<8?0.:((r>>4)%10)<3&&length>=64?16.:((r>>4)%10)<6&&length>=32?8.:4.;   // the kick always leaves before the drop: the last bar, often two, sometimes four
        for(int ti=0;ti<(int)p.tracks.size();++ti){const bool bass=ti==1,kick=ti==4,percussion=isDrumTrack(ti)&&ti!=4&&ti!=16;if(!bass&&!kick&&!percussion)continue;
            const double out=bass?bassOut:kick?kickOut:0.;if(out<=0&&!percussion)continue;auto& t=p.tracks[(size_t)ti];
            for(size_t ci=0;ci<t.clips.size();++ci){if(!editableTake(p,scope,track,section,ti,(int)ci,lastClip))continue;auto& c=t.clips[ci];if(c.audioOffset>=0||c.loopLength>0)continue;
                const double start=p.clipStart(ti,(int)ci),clipLength=p.clipLength(ti,(int)ci);if(start>=finish||start+clipLength<=begin)continue;
                if(out>0){const double rest=finish-out-start;c.notes.erase(std::remove_if(c.notes.begin(),c.notes.end(),[&](const Note& n){return n.start>=rest-1./32.&&n.start<finish-start;}),c.notes.end());
                    for(auto& n:c.notes)if(n.start<rest&&n.start+n.length>rest)n.length=std::max(1./64.,rest-n.start);}
                if(percussion&&length>=8)for(auto& n:c.notes){const double at=start+n.start;if(at<begin||at>=finish)continue;n.velocity=std::clamp((int)std::lround(n.velocity*(.82+.18*(at-begin)/length)),1,127);}}}}
}
// The stop before a drop: every part rests for the last one or two beats, so the drop lands
// on silence. Crash and reverse crash (lane 12) and sustained FX keep ringing through it.
static void applyDropGaps(Project& p,Scope scope,int track,int section,int lastClip=-1){
    for(int si=0;si+1<(int)p.sections.size();++si){const int gap=p.sections[(size_t)si].dropGap;if(gap<=0)continue;
        const double finish=p.sectionStart(si+1),from=finish-gap;
        for(int ti=0;ti<(int)p.tracks.size();++ti){if(ti==16||(isManualTrack(ti)&&!p.tracks[(size_t)ti].vocalChops))continue;auto& t=p.tracks[(size_t)ti];
            for(size_t ci=0;ci<t.clips.size();++ci){if(!editableTake(p,scope,track,section,ti,(int)ci,lastClip))continue;auto& c=t.clips[ci];if(c.audioOffset>=0)continue;
                const double start=p.clipStart(ti,(int)ci),length=p.clipLength(ti,(int)ci);if(start>=finish||start+length<=from)continue;const double rest=from-start;
                if(c.loopLength>0)continue;   // a looped take keeps its own repetition
                // Humanised attacks can land a few milliseconds early: they belong to the stop too.
                const double edge=rest-1./32.;
                c.notes.erase(std::remove_if(c.notes.begin(),c.notes.end(),[&](const Note& n){return n.start>=edge&&n.start<finish-start-1./32.;}),c.notes.end());
                for(auto& n:c.notes)if(n.start<edge&&n.start+n.length>rest)n.length=rest-n.start;}}}
}
// The lead's melody, written per genre. The engine's line jumped between chord tones in a two-bar
// loop in every style; this keeps the engine's chords and writes the single-note melody again:
// the genre's own rhythm cells, a phrase form (motif, answer, the motif moved with the chord,
// cadence on a long chord tone), stepwise or repeated or leaping motion in the genre's balance,
// its register and articulation. Random lead patterns only; generated, unlocked clips only.
namespace {
struct LeadStyle {std::vector<const char*> cells;double step,repeat,leap;int centre,width;bool legato,pentatonic,hypnotic;double accent;};
const LeadStyle& leadStyle(int genre){
    static const LeadStyle styles[]{
        {{"x..x..x...x.x...","x.x...x.x...x...","..x...x...x.x.x."},.45,.15,.25,74,9,false,false,false,.25},   // Piano House: anthem hooks
        {{"x...x.x.....x...","x.....x.x..x....","..x.x...x......."},.65,.10,.10,72,8,true,true,false,.15},     // Soulful: sung, stepwise
        {{"x.....x...x.....","x.......x...x...","..x...x.......x.","x...x.....x.....","x.......x.x.....","....x...x...x..."},.60,.20,.10,67,7,true,false,true,.10},     // Deep: sparse, long
        {{"x.x.x...x.x.x...","x..x..x.x.......","..x.x.x.x.x....."},.65,.05,.20,74,10,true,true,false,.20},   // Gospel: runs
        {{"x.xx..x.x.xx..x.","..x...xx..x...x.","x..x.x..x..x.x.."},.30,.40,.20,70,6,false,false,false,.35},  // Jackin': chopped repeats
        {{"x..x..x.x..x..x.","x.x..x.x..x.x...","..x..x..x..x.x.."},.40,.30,.20,72,7,false,false,false,.30},  // Jackin' Piano
        {{"x...x..x..x...x.","..x..x....x..x..","x..x....x..x...."},.45,.25,.20,70,7,false,false,false,.25},  // Garage: skipping
        {{"x..x..x.x..x..x.","..x.x..x..x.x...","x.x..x..x.x..x.."},.40,.35,.15,69,7,false,true,true,.30},    // Afro: 3-3-2 calls, pentatonic
        {{"x.xx.x.xx.x..xx.","..x.xx.x..x.xx..","x..x.xx.x..x.x.."},.45,.25,.20,72,8,false,false,false,.35},  // Funky & Disco
        {{"x...x...x...x.x.","x..x..x..x..x...","x.x...x...x.x..."},.40,.20,.30,74,10,false,false,false,.25}, // '90s Piano House
        {{"x....x...x......","..x.....x..x....","x.......x.x.....","...x..x.....x...","x..x......x....."},.70,.05,.15,70,9,true,true,false,.10},    // Neo Soul: lazy, stepwise
        {{"x.......x.......","x.....x...x.....","..x.....x.......","x...........x...","x...x.......x...","....x.....x...x."},.70,.10,.10,69,7,true,false,true,.05},    // Chill: long notes
        {{"x.x.x.x.x.x.x.x.","x..x..x..x..x.x.","x.x..x.x..x.x.x."},.35,.30,.30,76,12,false,false,false,.30}, // Dance Stabs: big intervals
        {{"x..x..x.x..x..x.","x...x...x...x...","..x...x...x...x."},.15,.65,.15,67,5,false,false,true,.30},   // Techno: hypnotic repeats
        {{"x.x..x.x..x.x.x.","x..x.x..x..x.x..","..xx..x...xx..x."},.30,.35,.30,72,12,false,false,false,.35}, // Electro: octave jumps
        {{"x.x.x.x..x.x.x..","..x.x.x.x.x.x.x.","x.x..x.x.x..x.x."},.60,.10,.20,74,9,false,false,false,.25},  // Latin: montuno-like runs
    };
    return styles[(size_t)std::clamp(genre,0,(int)std::size(styles)-1)];
}
}
// A melody a listener can follow: in each 8-bar phrase the lead's third pair of bars returns to the
// motif of the first (A B A' B'), and the fourth to the second with its last note resolving on a chord
// tone. The motif keeps its rhythm and its shape; it moves by the diatonic step that puts the most of
// its beats on the new chord's tones (sequence), so it follows the harmony. A pair with no lead
// (the singer's bars, a rest) stays empty.
static void leadMotifs(Project& p,const std::function<bool(int,int)>& fresh){
    if((int)p.tracks.size()<=leadTrack)return;auto& t=p.tracks[(size_t)leadTrack];if(t.locked||t.instrument=="sample")return;
    for(size_t ci=0;ci<t.clips.size();++ci){auto& c=t.clips[ci];if(c.locked||c.loopLength>0||c.notes.size()<6||!fresh(leadTrack,(int)ci))continue;
        const double at=p.clipStart(leadTrack,(int)ci),length=p.clipLength(leadTrack,(int)ci);const int si=p.sectionAtBeat(at+.01);if(si<0)continue;
        const double phraseOrigin=p.sectionStart(si)-at;bool changed=false;
        int lo=127,hi=0;for(const auto& n:c.notes){lo=std::min(lo,n.pitch);hi=std::max(hi,n.pitch);}   // the motif stays inside the take's own range
        auto block=[&](double from){std::vector<Note> out;for(const auto& n:c.notes)if(n.start>=from-1./16.&&n.start<from+8-1./16.)out.push_back(n);return out;};
        auto inScale=[&](int pitch,double beat){const auto h=activeHarmony(p,std::max(0,p.sectionAtBeat(at+beat+.01)));const auto& iv=sgk::scaleIntervals(h.scale);const int pc=((pitch-h.key)%12+12)%12;return std::find(iv.begin(),iv.end(),pc)!=iv.end();};
        auto chordTone=[&](int pitch,double beat){const int s=std::max(0,p.sectionAtBeat(at+beat+.01));const auto ch=chordAtBeat(activeHarmony(p,s),at+beat+.01);const int iv=((pitch-ch.rootPc)%12+12)%12;return iv==0||iv==ch.third||iv==ch.fifth||(ch.seventh>0&&iv==ch.seventh);};
        auto place=[&](const std::vector<Note>& motif,double from,double to,bool resolve){
            const auto target=block(to);if(motif.size()<3||target.size()<2||to+8>length+1e-6)return;
            int best=0;double bestScore=-1e9;
            for(int shift=-5;shift<=5;++shift){double score=-std::abs(shift)*.05;bool ok=true;
                for(const auto& n:motif){const double b=n.start-from+to;const int q=n.pitch+shift;if(q<std::max(lo,t.engine.rangeLow)||q>std::min(hi,t.engine.rangeHigh)||!inScale(q,b)){ok=false;break;}
                    const bool onBeat=std::abs(b-std::round(b))<1./16.;if(onBeat)score+=chordTone(q,b)?1.:-.5;}
                if(ok&&score>bestScore){bestScore=score;best=shift;}}
            if(bestScore<=-1e8)return;
            c.notes.erase(std::remove_if(c.notes.begin(),c.notes.end(),[&](const Note& n){return n.start>=to-1./16.&&n.start<to+8-1./16.;}),c.notes.end());
            double first=1e9,last=0;for(const auto& n:target){first=std::min(first,n.start);last=std::max(last,n.start+n.length);}   // where the lead was playing: a stop or a rest stays silent
            for(size_t i=0;i<motif.size();++i){Note n=motif[i];n.start=n.start-from+to;n.pitch+=best;if(n.start<first-.5||n.start>=last)continue;n.length=std::min(n.length,last-n.start);
                if(resolve&&i+1==motif.size()&&!chordTone(n.pitch,n.start))for(int d:{-1,1,-2,2,-3,3})if(chordTone(n.pitch+d,n.start)&&n.pitch+d>=lo&&n.pitch+d<=hi){n.pitch+=d;break;}
                c.notes.push_back(n);}
            changed=true;};
        for(double phrase=phraseOrigin;phrase+32<=length+1e-6;phrase+=32){if(phrase<0)continue;
            place(block(phrase),phrase,phrase+16,false);place(block(phrase+8),phrase+8,phrase+24,true);}
        if(changed){std::sort(c.notes.begin(),c.notes.end(),[](const Note& a,const Note& b){return a.start<b.start;});tidyNotes(c,length);}}
}
static void shapeLeadMelody(Project& p,const std::function<bool(int,int)>& fresh){
    if((int)p.tracks.size()<=leadTrack)return;auto& t=p.tracks[(size_t)leadTrack];
    if(t.locked||t.engine.pattern>=0||isElectronicGenre(p.settings.genre))return;   // electronic leads keep their own engine path
    const auto& style=leadStyle(p.settings.genre);
    for(size_t ci=0;ci<t.clips.size();++ci){auto& c=t.clips[ci];if(c.locked||c.notes.empty()||!fresh(leadTrack,(int)ci))continue;
        const double at=p.clipStart(leadTrack,(int)ci),length=p.clipLength(leadTrack,(int)ci);
        // chords (two or more notes together) stay as the engine wrote them; the melody is rewritten
        // the lead is one voice: every engine note is part of the melody being rewritten
        std::vector<Note> chords,melody=c.notes;
        if(melody.size()<2)continue;
        // where the engine's melody is silent (a section entry, a break), the new one rests too
        // the new melody plays only where the engine's did: inside its notes or across gaps of a beat
        // at most; longer silences (the stop before a drop, response windows) stay silent
        std::sort(melody.begin(),melody.end(),[](const Note& a,const Note& b){return a.start<b.start;});
        auto playing=[&](double beat){const Note* prev=nullptr;const Note* next=nullptr;for(const auto& n:melody){if(n.start<=beat+1e-6)prev=&n;else{next=&n;break;}}
            if(!prev)return false;const double end=prev->start+prev->length;return beat<end||(next&&next->start-end<=1.&&beat<next->start);};
        /* the end of the run of melody notes (gaps of a beat at most) that plays at this beat, a gap after a note included */
        auto coverEnd=[&](double beat){if(!playing(beat))return beat;double end=beat;for(const auto& n:melody){const double e=n.start+n.length;if(n.start<=beat+1e-6)end=std::max(end,e);else if(n.start-end<=1.)end=std::max(end,e);else break;}return end;};
        double loudness=0;for(const auto& n:melody)loudness+=n.velocity;loudness/=melody.size();
        // the take's dice follow the engine's own take (its notes), so every regeneration is a new melody
        std::uint32_t r=mix(c.seed^mix(c.motifSeed+0x6c8e9cf5U)^mix(t.engine.riffSeed+0x2545f491U));for(const auto& n:melody)r=mix(r^(std::uint32_t)((n.pitch-melody.front().pitch+64)*131+std::lround(n.start*4)*7));/* intervals: an octave change keeps the take */// The hook: the song's motif (shared by every take of the lead) picks where the line starts and the first
        // notes of each phrase, so the same idea returns from section to section; the take's rhythm and answers vary.
        std::uint32_t h=mix(c.motifSeed^mix(t.engine.riffSeed+0x4b1d2c3eU));bool hook=false;auto dice=[&](){return nextDice(hook?h:r);};
        auto harmony=[&](double beat){return activeHarmony(p,std::max(0,p.sectionAtBeat(at+beat)));};
        auto scaleOk=[&](int pitch,double beat){const auto h=harmony(beat);const int pc=((pitch-h.key)%12+12)%12;std::array<bool,12> in{};
            if(style.pentatonic){const auto set=sgk::pentatonicNotes(0,h.scale,0,11);for(int i=0;i<set.size;++i)in[(size_t)(((set.notes[(size_t)i])%12+12)%12)]=true;}   // the scale's own pentatonic (minor: 1 b3 4 5 b7)
            else for(int d=0;d<7;++d){const int root=((sgk::buildChord(0,h.scale,d,sgk::Extension::triad).rootPc)%12+12)%12;in[(size_t)root]=true;}return in[(size_t)pc];};
        auto chordTone=[&](int pitch,double beat){const auto ch=chordAtBeat(harmony(beat),at+beat);const int iv=((pitch-ch.rootPc)%12+12)%12;return iv==0||iv==ch.third||iv==ch.fifth||iv==ch.seventh;};
        auto stepFrom=[&](int pitch,int dir,double beat){int q=pitch;for(int k=0;k<4;++k){q+=dir;if(scaleOk(q,beat))return q;}return pitch+dir*2;};
        auto nearestChordTone=[&](int pitch,double beat){for(int k=0;k<7;++k){if(chordTone(pitch+k,beat))return pitch+k;if(chordTone(pitch-k,beat))return pitch-k;}return pitch;};
        // the register follows the engine's take (global octave, room for the band); the width is the genre's
        double average=0;for(const auto& n:melody)average+=n.pitch;const int centre=(int)std::lround(average/melody.size());
        const int low=centre-style.width,high=centre+style.width;
        hook=true;std::vector<Note> out;std::vector<int> motif;int pitch=nearestChordTone(centre+(int)std::lround((dice()-.5)*8),0),dir=dice()<.5?1:-1;hook=false;
        const int bars=(int)std::ceil(length/4-1e-9);   /* a partial last bar is written too (its notes are cut to the clip below), so a free clip keeps its tail */
        // two rhythm cells from the genre's vocabulary, each varied a little: an onset moved by a 16th,
        // an added 8th, or a dropped weak hit, so takes differ while staying in the style
        auto varied=[&](const char* base){std::string cell(base);
            if(dice()<.6){for(int tries=0;tries<8;++tries){const int i=(int)(dice()*16);if(cell[(size_t)i]!='x'||i%4==0)continue;const int j=i+(dice()<.5?-1:1);if(j<0||j>15||cell[(size_t)j]=='x')continue;cell[(size_t)i]='.';cell[(size_t)j]='x';break;}}
            if(dice()<.35){const int i=(int)(dice()*8)*2;if(cell[(size_t)i]!='x')cell[(size_t)i]='x';}
            if(dice()<.25){for(int tries=0;tries<8;++tries){const int i=(int)(dice()*16);if(cell[(size_t)i]=='x'&&i%4!=0){cell[(size_t)i]='.';break;}}}
            return cell;};
        const std::string cellA=varied(style.cells[(size_t)(dice()*style.cells.size())]),cellB=varied(style.cells[(size_t)(dice()*style.cells.size())]);
        for(int bar=0;bar<bars;++bar){
            const int inPhrase=bar%4;hook=inPhrase==0;const bool cadence=inPhrase==3;const bool reuse=inPhrase==2||(style.hypnotic&&inPhrase!=3);
            const std::string& cell=inPhrase==1?cellB:cellA;std::vector<int> barMotif;int index=0;
            for(int step=0;step<16;++step){if(cell[step]!='x')continue;if(index>=3)hook=false;   /* the hook is the head of the phrase: its first notes */const double beat=bar*4+step*.25;if(!playing(beat)){++index;continue;}
                if(cadence&&step>=8)break;   // the phrase ends early, on a held note
                const bool strong=step%4==0;
                if(reuse&&index<(int)motif.size()){   // the motif again, moved with the chord (same steps from the chord)
                    pitch=nearestChordTone(motif[(size_t)index],beat);if(!strong)pitch=scaleOk(motif[(size_t)index],beat)?motif[(size_t)index]:stepFrom(motif[(size_t)index],1,beat);}
                else{const double roll=dice();
                    if(strong||roll<style.leap*.5){if(roll<style.leap){pitch=nearestChordTone(pitch+dir*(dice()<.5?4:7),beat);}else pitch=nearestChordTone(pitch,beat);}
                    else if(roll<style.leap*.5+style.repeat){/* the same note again */}
                    else pitch=stepFrom(pitch,dir,beat);
                    if(dice()<.25)dir=-dir;}
                while(pitch>high)pitch-=12;while(pitch<low)pitch+=12;if(pitch>high-2)dir=-1;if(pitch<low+2)dir=1;
                Note n;n.start=beat;n.pitch=pitch;n.lane=0;n.velocity=std::clamp((int)std::lround(loudness*(strong?1.+style.accent*.5:1.-style.accent*.4)+(dice()-.5)*8),20,127);
                double next=4;for(int k=step+1;k<16;++k)if(cell[k]=='x'){next=(k-step)*.25;break;}
                n.length=style.legato?std::min(next,2.)*.95:std::min(next*.6,.3);out.push_back(n);barMotif.push_back(pitch);++index;}
            if(cadence){const double beat=bar*4+(out.empty()||out.back().start<bar*4?0.:2.);if(playing(beat)){const auto ch=chordAtBeat(harmony(beat),at+beat);int end=pitch;for(int k=0;k<7;++k){const int iv1=((pitch+k-ch.rootPc)%12+12)%12,iv2=((pitch-k-ch.rootPc)%12+12)%12;if(iv1==0||iv1==ch.third){end=pitch+k;break;}if(iv2==0||iv2==ch.third){end=pitch-k;break;}}
                Note n;n.start=std::max(beat,out.empty()?0.:out.back().start+.25);n.length=std::max(.5,bar*4+4-n.start-.1);n.pitch=std::clamp(end,low-2,high+2);n.velocity=(int)std::lround(loudness);n.lane=0;if(n.start<length-.25)out.push_back(n);}}
            if(inPhrase==0)motif=barMotif;}
        out.erase(std::remove_if(out.begin(),out.end(),[&](const Note& n){return n.start>=length-1./32.||!playing(n.start);}),out.end());
        // one voice: each note ends before the next starts
        std::sort(out.begin(),out.end(),[](const Note& a,const Note& b){return a.start<b.start;});
        for(size_t i=0;i<out.size();++i){if(i+1<out.size())out[i].length=std::min(out[i].length,out[i+1].start-out[i].start-1./64.);out[i].length=std::min(out[i].length,coverEnd(out[i].start)-out[i].start);}
        /* a note left with less than a 16th before the next one or the end of the phrase is a click, not a note */
        out.erase(std::remove_if(out.begin(),out.end(),[](const Note& n){return n.length<1./16.;}),out.end());
        if(out.size()<2)continue;
        // a held melody note never sits a semitone from the accompaniment it sounds with: it moves to
        // the nearest chord tone that does not
        {struct Held{double from,to;int pitch;};std::vector<Held> band;for(int ti:{0,2,3}){if(ti>=(int)p.tracks.size()||p.tracks[(size_t)ti].mute)continue;const auto& bt=p.tracks[(size_t)ti];
            for(size_t bc=0;bc<bt.clips.size();++bc){const double bat=p.clipStart(ti,(int)bc)-at;for(const auto& m:bt.clips[bc].notes)if(bat+m.start<length&&bat+m.start+m.length>0)band.push_back({bat+m.start,bat+m.start+m.length,m.pitch});}}
         auto rubs=[&](int pitch,double from,double to){for(const auto& h:band){if(h.from>=to||h.to<=from||std::min(h.to,to)-std::max(h.from,from)<.25)continue;const int lo=std::min(pitch,h.pitch),hi=std::max(pitch,h.pitch);if(hi!=lo&&(hi-lo)%12==1)return true;}return false;};
         for(auto& n:out){if(n.length<.5||!rubs(n.pitch,n.start,n.start+n.length))continue;
            for(int k=1;k<7;++k){bool done=false;for(int q:{n.pitch-k,n.pitch+k})if(q>=low-2&&q<=high+2&&chordTone(q,n.start)&&!rubs(q,n.start,n.start+n.length)){n.pitch=q;done=true;break;}if(done)break;}}}
        // NEW FEEL and humanize stay audible: each note takes the micro-timing of the engine's nearest onset
        for(auto& n:out){const Note* near=nullptr;for(const auto& m:melody)if(!near||std::abs(m.start-n.start)<std::abs(near->start-n.start))near=&m;
            const double placed=n.start;   /* the cover is measured where the note was placed, before its micro-timing */
            if(near&&std::abs(near->start-n.start)<.5){const double offset=near->start-std::round(near->start*4)/4;if(std::abs(offset)<.1)n.start=std::max(0.,n.start+offset);}
            n.length=std::min(n.length,coverEnd(placed)-n.start);}
        out.erase(std::remove_if(out.begin(),out.end(),[](const Note& n){return n.length<1./16.;}),out.end());
        for(size_t i=0;i+1<out.size();++i)out[i].length=std::max(1./64.,std::min(out[i].length,out[i+1].start-out[i].start-1./128.));
        c.notes=std::move(chords);c.notes.insert(c.notes.end(),out.begin(),out.end());tidyNotes(c,length);
    }
}
// Voicings across the band, pitch classes untouched: where the lead plays, a strings chord of
// three or more notes moves its notes inside the lead's register an octave down (not below C3);
// a guitar chord of three or more notes lifts its lowest note an octave when it doubles the bass
// note sounding at that moment. Generated, unlocked clips only.
static void ensembleVoicing(Project& p,const std::function<bool(int,int)>& inScope){
    if((int)p.tracks.size()<=leadTrack||isElectronicGenre(p.settings.genre))return;
    struct Span{double from,to;int pitch;};
    auto spans=[&](int ti){std::vector<Span> out;const auto& t=p.tracks[(size_t)ti];if(t.mute)return out;for(size_t ci=0;ci<t.clips.size();++ci){const double at=p.clipStart(ti,(int)ci);for(const auto& n:t.clips[ci].notes)out.push_back({at+n.start,at+n.start+n.length,n.pitch});}return out;};
    const auto lead=spans(leadTrack),bass=spans(1);
    auto& strings=p.tracks[2];
    if(!strings.locked&&!lead.empty())for(size_t ci=0;ci<strings.clips.size();++ci){auto& c=strings.clips[ci];if(c.locked||!inScope(2,(int)ci))continue;const double at=p.clipStart(2,(int)ci);
        std::map<long,int> chord;for(const auto& n:c.notes)++chord[std::lround(n.start*8)];
        for(auto& n:c.notes){if(chord[std::lround(n.start*8)]<3)continue;const double from=at+n.start,to=from+n.length;int low=128,high=-1;
            for(const auto& l:lead)if(l.from<to&&l.to>from){low=std::min(low,l.pitch);high=std::max(high,l.pitch);}
            const bool taken=std::any_of(c.notes.begin(),c.notes.end(),[&](const Note& o){return std::abs(o.start-n.start)<1./16.&&o.pitch==n.pitch-12;});   // never stack two equal notes
            if(high>=0&&!taken&&n.pitch>=low-2&&n.pitch<=high+2&&n.pitch-12>=48)n.pitch-=12;}
        tidyNotes(c,p.clipLength(2,(int)ci));}
    // No semitone between parts: a strings (or guitar) note a minor second / minor ninth above or
    // below a piano note it sustains with moves an octave, to the side where the interval becomes a
    // major seventh; the piano keeps its voicing. Pitch classes stay, so the harmony is unchanged.
    {const auto piano=spans(0);auto declash=[&](int ti,std::vector<Span> against){auto& t=p.tracks[(size_t)ti];if(t.locked||t.mute||against.empty())return;
        // sorted by start, so each note looks only at the spans that can overlap it (the whole song, scanned per note, was quadratic)
        std::sort(against.begin(),against.end(),[](const Span& a,const Span& b){return a.from<b.from;});double longest=0;for(const auto& o:against)longest=std::max(longest,o.to-o.from);
        auto window=[&](double from){return std::lower_bound(against.begin(),against.end(),from-longest,[](const Span& s,double x){return s.from<x;});};
        for(size_t ci=0;ci<t.clips.size();++ci){auto& c=t.clips[ci];if(c.locked||c.notes.empty()||!inScope(ti,(int)ci))continue;const double at=p.clipStart(ti,(int)ci);bool moved=false;
            auto clashes=[&](int pitch,double from,double to){int count=0;for(auto it=window(from);it!=against.end()&&it->from<to;++it){const auto& o=*it;if(o.to<=from||std::min(o.to,to)-std::max(o.from,from)<.125)continue;const int lo=std::min(pitch,o.pitch),hi=std::max(pitch,o.pitch);if(hi!=lo&&(hi-lo)%12==1)++count;}return count;};
            for(auto& n:c.notes){const double from=at+n.start,to=from+n.length;const int now=clashes(n.pitch,from,to);if(!now)continue;
                int best=n.pitch,fewest=now;for(int shift:{-12,12,-24,24}){const int q=n.pitch+shift;if(q<36||q>96)continue;
                    if(std::any_of(c.notes.begin(),c.notes.end(),[&](const Note& o){return &o!=&n&&o.pitch==q&&std::abs(o.start-n.start)<1./16.;}))continue;
                    const int k=clashes(q,from,to);if(k<fewest){fewest=k;best=q;}}
                if(best!=n.pitch){n.pitch=best;moved=true;}
                // still a semitone: when the piano or this part already holds that pitch class in another
                // octave, this doubling goes
                const bool doubledHere=std::any_of(c.notes.begin(),c.notes.end(),[&](const Note& o){return &o!=&n&&o.velocity>0&&o.pitch!=n.pitch&&((o.pitch-n.pitch)%12+12)%12==0&&std::abs(o.start-n.start)<1./16.;});
                if(fewest>0&&(doubledHere||std::any_of(against.begin(),against.end(),[&](const Span& o){return o.from<to&&o.to>from&&((o.pitch-n.pitch)%12+12)%12==0;}))){n.velocity=0;moved=true;}}
            if(moved){c.notes.erase(std::remove_if(c.notes.begin(),c.notes.end(),[](const Note& n){return n.velocity==0;}),c.notes.end());tidyNotes(c,p.clipLength(ti,(int)ci));}}};
     declash(2,piano);auto withStrings=piano;const auto strings2=spans(2);withStrings.insert(withStrings.end(),strings2.begin(),strings2.end());declash(3,withStrings);}
    auto& guitar=p.tracks[3];
    if(!guitar.locked&&!bass.empty())for(size_t ci=0;ci<guitar.clips.size();++ci){auto& c=guitar.clips[ci];if(c.locked||!inScope(3,(int)ci))continue;const double at=p.clipStart(3,(int)ci);
        std::map<long,std::vector<size_t>> chords;for(size_t i=0;i<c.notes.size();++i)chords[std::lround(c.notes[i].start*8)].push_back(i);
        // the doubled root leaves the bass register: up an octave, or out when that octave is
        // already in the chord (its pitch class stays, so the bar's harmony is unchanged)
        std::vector<bool> drop(c.notes.size(),false);
        for(const auto& [step,members]:chords){if(members.size()<3)continue;size_t lowest=members[0];for(size_t i:members)if(c.notes[i].pitch<c.notes[lowest].pitch)lowest=i;
            const double beat=at+c.notes[lowest].start;bool doubled=false;for(const auto& b:bass)if(b.from<=beat+1e-6&&b.to>beat+1e-6&&((b.pitch-c.notes[lowest].pitch)%12+12)%12==0){doubled=true;break;}
            if(!doubled)continue;const int up=c.notes[lowest].pitch+12;const double start=c.notes[lowest].start;const bool there=std::any_of(c.notes.begin(),c.notes.end(),[&](const Note& o){return o.pitch==up&&std::abs(o.start-start)<1./16.;});
            if(there)drop[lowest]=true;else if(up<=84)c.notes[lowest].pitch=up;}
        std::vector<Note> kept;for(size_t i=0;i<c.notes.size();++i)if(!drop[i])kept.push_back(c.notes[i]);c.notes=std::move(kept);
        tidyNotes(c,p.clipLength(3,(int)ci));}   // the canonical order and overlaps, as normalise() leaves them
}
// A note held over a chord change ends at the change when it is not a tone of the new chord
// (a piano chord ringing into the next one, a strings pad late on the change). Tones the
// new chord shares keep ringing; anticipations (the next chord pushed early) are its tones.
static void trimHoldovers(Project& p,const std::function<bool(int,int)>& fresh){
    auto tone=[](int pitch,const sgk::Chord& c){const int r=((pitch-c.rootPc)%12+12)%12;if(r==0)return true;for(int iv:{c.third,c.fifth,c.seventh,c.ninth,c.eleventh,c.thirteenth,c.sixth})if(iv>=0&&iv%12==r)return true;return false;};
    for(int ti:{0,2,3,leadTrack}){if(ti>=(int)p.tracks.size())continue;auto& t=p.tracks[(size_t)ti];if(t.locked)continue;
        for(size_t ci=0;ci<t.clips.size();++ci){auto& c=t.clips[ci];if(c.locked||c.notes.empty()||!fresh(ti,(int)ci))continue;const double at=p.clipStart(ti,(int)ci);bool cut=false;
            for(auto& n:c.notes){const double a=at+n.start,b=a+n.length;
                for(double x=std::floor(a*2)/2+.5;x<b-.125;x+=.5){const int s0=std::max(0,p.sectionAtBeat(x-.01)),s1=std::max(0,p.sectionAtBeat(x+.01));
                    const auto before=chordAtBeat(activeHarmony(p,s0),x-.01),after=chordAtBeat(activeHarmony(p,s1),x+.01);
                    // the whole chord (sevenths and extensions too: Cmaj7 -> C7 is a change), and every change the note crosses, not only the first
                    if(before.rootPc==after.rootPc&&before.third==after.third&&before.fifth==after.fifth&&before.seventh==after.seventh&&before.ninth==after.ninth&&before.eleventh==after.eleventh&&before.thirteenth==after.thirteenth&&before.sixth==after.sixth)continue;
                    if(tone(n.pitch,after))continue;
                    if(x-a>=1./16.){n.length=x-a-1./64.;cut=true;}break;}}
            if(cut)tidyNotes(c,p.clipLength(ti,(int)ci));}}
}
// Band interplay (Afro and Latin House): one part leads, the others answer instead of all
// playing at once. Latin: the piano montuno leads, guitar and lead drop the hits that land on a
// montuno attack, so they answer in its gaps. Afro: kick, bass and the percussion ostinato stay;
// guitar and lead take turns phrase by phrase (4 bars) where both would play. Only generated,
// unlocked clips change, and a clip never loses more than 70% of its notes.
static void bandInterplay(Project& p,const std::function<bool(int,int)>& inScope){
    const int genre=p.settings.genre;if(genre!=afroHouseGenre&&genre!=latinHouseGenre)return;
    if((int)p.tracks.size()<=leadTrack)return;
    auto thin=[&](int ti,const std::function<bool(double)>& drop){auto& t=p.tracks[(size_t)ti];if(t.locked||t.mute)return;
        for(size_t ci=0;ci<t.clips.size();++ci){auto& c=t.clips[ci];if(c.locked||c.notes.empty()||!inScope(ti,(int)ci))continue;const double at=p.clipStart(ti,(int)ci);
            // weighted, not binary: a part keeps its own accents (the loudest hits and its bar downbeats) and about one
            // clash in four as an intended ensemble hit; only its weak, overlapping attacks give way
            int loudest=0;for(const auto& n:c.notes)loudest=std::max(loudest,n.velocity);
            auto accent=[&](const Note& n){const double beat=at+n.start;return n.velocity*100>=loudest*92||std::abs(beat/4-std::round(beat/4))<1./128.||mix((std::uint32_t)std::lround(beat*8)*0x9e3779b1U^(std::uint32_t)ti)%4==0;};
            std::vector<Note> kept;for(const auto& n:c.notes)if(accent(n)||!drop(at+n.start))kept.push_back(n);
            if(kept.size()*10>=c.notes.size()*3)c.notes=std::move(kept);}};
    if(genre==latinHouseGenre){
        std::set<long> montuno;const auto& piano=p.tracks[0];
        // A muted piano is not heard: nothing answers it.
        if(!piano.mute){
            for(size_t ci=0;ci<piano.clips.size();++ci){
                const double at=p.clipStart(0,(int)ci);
                for(const auto& n:piano.clips[ci].notes)montuno.insert(std::lround((at+n.start)*4));
            }
        }
        for(int ti:{3,leadTrack})thin(ti,[&](double beat){return montuno.count(std::lround(beat*4))>0;});
    }else{
        std::map<long,std::array<int,2>> busy;   // per 4-bar phrase: guitar and lead note counts
        for(int which=0;which<2;++which){const int ti=which?leadTrack:3;const auto& t=p.tracks[(size_t)ti];if(t.mute)continue;
            for(size_t ci=0;ci<t.clips.size();++ci){const double at=p.clipStart(ti,(int)ci);for(const auto& n:t.clips[ci].notes)++busy[(long)std::floor((at+n.start)/16.)][(size_t)which];}}
        for(int which=0;which<2;++which)thin(which?leadTrack:3,[&](double beat){const long phrase=(long)std::floor(beat/16.);const auto it=busy.find(phrase);
            return it!=busy.end()&&it->second[0]>0&&it->second[1]>0&&(phrase%2==0)==(which==0);});   // even phrases: the lead answers, odd: the guitar
    }
}
// The passes that run on freshly written takes, after the engines: the lead's melody, the band's
// interplay, ensemble voicing, held-over notes and the VST articulation profiles. Every path that
// writes takes (generation, a global feel change) runs them, so the result never depends on it.
static std::uint64_t engineOutput(const std::vector<Note>& notes){std::uint64_t h=1469598103934665603ULL;for(const auto& n:notes)for(long v:{std::lround(n.start*3840),std::lround(n.length*3840),(long)n.pitch,(long)n.velocity,(long)n.lane}){h^=(std::uint64_t)v;h*=1099511628211ULL;}return h?h:1;}
// A player is never a grid: the time drifts a few milliseconds and comes back (a slow random walk,
// not a dice roll per note), a section's chord enters voice by voice, and each phrase breathes (its
// downbeat leans in, the line swells toward the turnaround). Deterministic per take; the kick and
// the crash stay on the grid, electronic genres get half the drift.
// A bassist walks into the next chord: a short note on the last eighth before a change of root
// becomes a chromatic or scale step onto the new root (soul and gospel often, deep house sometimes,
// techno never), when the bass does land on that root.
static void bassApproach(Clip& c,const Project& p,double at,double beats){
    const auto g=(Genre)p.settings.genre;
    const double chance=g==Genre::soulful||g==Genre::gospel||g==Genre::neoSoul||g==Genre::discoHouse?.55:g==Genre::deepHouse||g==Genre::ninetiesHouse||g==Genre::pianoHouse||g==Genre::chill?.3:g==Genre::afroHouse||g==Genre::latinHouse||g==Genre::ukGarage?.2:0.;
    if(chance<=0||c.notes.size()<3)return;std::uint32_t r=mix(c.seed^0x7a3b91c5U);auto dice=[&](){return nextDice(r);};
    std::sort(c.notes.begin(),c.notes.end(),[](const Note& a,const Note& b){return a.start<b.start;});
    for(size_t i=0;i+1<c.notes.size();++i){auto& n=c.notes[i];const auto& next=c.notes[i+1];const double qs=std::round(n.start*8)/8,change=std::floor(qs+1e-6)+1;   // read on the written grid: a new feel never changes the choice
        if(change-qs>.5+1e-6||change-qs<.2||n.length>.5+1e-6||std::abs(next.start-change)>.06||change>=beats)continue;
        const int si=std::max(0,p.sectionAtBeat(at+n.start));const auto h=activeHarmony(p,si);const auto now=chordAtBeat(h,at+qs),then=chordAtBeat(h,at+change);
        if(now.rootPc==then.rootPc||((next.pitch-then.rootPc)%12+12)%12!=0||dice()>=chance)continue;
        const int step=dice()<.6?1:2,sign=dice()<.5?-1:1;
        // never a semitone under a tone of the chord still sounding: that passing note rubs against the piano
        auto rubs=[&](int pitch){const int iv=((pitch+1-now.rootPc)%12+12)%12;return iv==0||iv==now.third||iv==now.fifth||iv==now.seventh;};
        int approach=-1;for(const int d:{sign*step,-sign*step,sign*(3-step),-sign*(3-step)}){const int q=next.pitch+d;if(!rubs(q)&&std::abs(q-n.pitch)<=7){approach=q;break;}}
        if(approach>=0)n.pitch=approach;}
}
// A player's pocket, not dice: each note's distance from its grid point (16ths and 8th triplets) stays within
// 0.012 beats (about 6 ms) of the take's own median placement on that kind of step, so the part keeps its feel (behind,
// ahead, swung) without random scatter. The lead's dynamics follow the phrase: each velocity stays within 6 of
// the mean of its neighbours. The guitar keeps its strums as they are.
static void tightenPocket(std::vector<Note>& notes,int ti){
    if(notes.size()<4||ti==3)return;
    auto grid=[](double b){const double a=std::round(b*4)/4,t=std::round(b*6)/6;return std::abs(b-a)<=std::abs(b-t)+1e-9?a:t;};
    std::array<std::vector<double>,2> offsets;for(const auto& n:notes){const double g=grid(n.start);offsets[(size_t)(std::abs(std::fmod(g*2,1.))>1e-6?1:0)].push_back(n.start-g);}
    std::array<double,2> pocket{};for(int k=0;k<2;++k){auto& o=offsets[(size_t)k];if(o.empty())continue;std::nth_element(o.begin(),o.begin()+(long)o.size()/2,o.end());pocket[(size_t)k]=o[o.size()/2];}
    for(auto& n:notes){const double g=grid(n.start),off=n.start-g;if(std::abs(off)>.06)continue;const double target=pocket[(size_t)(std::abs(std::fmod(g*2,1.))>1e-6?1:0)];
        const double start=std::max(0.,g+target+std::clamp(off-target,-.012,.012));   /* a clamp, so a second pass changes nothing */
        if(std::any_of(notes.begin(),notes.end(),[&](const Note& o){return &o!=&n&&o.pitch==n.pitch&&o.start<n.start+n.length&&o.start+o.length>std::min(start,n.start)-1./32.&&(o.start<n.start||o.start<start+n.length);}))continue;   /* never into a note of the same pitch */n.start=start;}   /* the whole note moves: its length stays */
    if(ti==leadTrack&&notes.size()>=3){std::vector<int> v;for(const auto& n:notes)v.push_back(n.velocity);
        for(size_t i=1;i+1<notes.size();++i){const double mean=.5*(v[i-1]+v[i+1]);notes[i].velocity=std::clamp((int)std::lround(std::clamp((double)v[i],mean-6,mean+6)),1,127);}}   /* within 6 of its neighbours: idempotent too */
}
static void livePerformance(Clip& c,const Project& p,int ti,double beats){
    if(c.notes.empty())return;
    static constexpr double driftMs[legacyTrackCount+1]{5.5,4.5,8,5.5, 0,2,2,3,3.5,3,3,4,4,4,3.5,3,0, 7};
    const int slot=ti==leadTrack?legacyTrackCount:ti;if(slot<0||slot>legacyTrackCount)return;
    const auto& t=p.tracks[(size_t)ti];const double human=takeFeel(t.engine.humanize,c.feel[2],p.settings.humanize);if(human<=.01)return;   // HUMANIZE 0: exactly as written
    const double scale=(isElectronicGenre(p.settings.genre)?.5:1.)*std::clamp(.6+human*1.6,.6,1.4),ms=driftMs[slot]*scale;
    const double beat=60000./std::max(30.,p.settings.bpm);   // ms per beat
    std::uint32_t r=mix(c.seed^mix((std::uint32_t)ti*0x9e3779b1U+0x51ed270bU));auto dice=[&](){return nextDice(r);};
    // the drift: one value per beat, each pulled back toward zero (it wanders, it does not run away)
    const int count=(int)std::ceil(beats)+2;std::vector<double> walk((size_t)count);double x=(dice()-.5)*1.4;
    for(auto& w:walk){x=x*.72+(dice()-.5)*1.3;w=std::clamp(x,-1.,1.);}
    auto driftAt=[&](double b){const double f=std::clamp(b,0.,(double)count-1.001);const int i=(int)f;return walk[(size_t)i]+(walk[(size_t)i+1]-walk[(size_t)i])*(f-i);};
    std::sort(c.notes.begin(),c.notes.end(),byStartPitch);
    const bool drums=isDrumTrack(ti);
    for(size_t i=0;i<c.notes.size();){size_t j=i;while(j<c.notes.size()&&c.notes[j].start-c.notes[i].start<1./24.)++j;   // a chord moves together
        const double at=c.notes[i].start;const double shift=ms>0?((driftAt(at)+1.)*.5*ms+dice()*ms*.2)/beat:0;   // behind the grid, by an amount that wanders: never ahead of its beat or chord
        const int bar=(int)std::floor(at/4+1e-9),inPhrase=bar%8;
        // phrase dynamics: the first downbeat of each 4 bars leans in, and the line swells through 8
        const double swell=1.+.05*((inPhrase+std::fmod(at,4.)/4.)/8.-.5)+((bar%4==0&&std::fmod(at,4.)<.05)?.04:0.);
        for(size_t k=i;k<j;++k){auto& n=c.notes[k];
            double voice=0;int lift=0;
            if(ti==2&&j-i>=2){voice=dice()*ms*2.2/beat;lift=(k==j-1?4:0)+(int)std::lround((dice()-.5)*8);}   // strings: each desk enters on its own, the top line sings
            const double end=n.start+n.length,start=std::clamp(n.start+shift+voice,0.,std::max(0.,beats-1./64.));
            n.start=start;n.length=std::max(1./64.,n.length<.5?std::min({n.length,beats-start,std::ceil(end-1e-9)-start}):std::min(end+std::min(shift,0.),beats)-start);   // a short note keeps its length (shrinking it turned staccato into clicks) up to the next beat, where stops and phrases end; a long one keeps its release   // the release stays where it was: a held chord never crosses into the next phrase
            if(!drums||ti>=8)n.velocity=std::clamp((int)std::lround(n.velocity*swell)+lift,1,127);}
        i=j;}
    tidyNotes(c,beats);
    if(ti==leadTrack)for(size_t i=0;i+1<c.notes.size();++i)c.notes[i].length=std::max(1./64.,std::min(c.notes[i].length,c.notes[i+1].start-c.notes[i].start-1./64.));
}
// The piano states a two-bar idea and answers it: in each four-bar phrase the third bar plays the
// first bar's rhythm again (its accents and lengths), on the chord that bar holds; the second and
// fourth bars keep their own figure, so the variation lands at the turnaround. A bar the engine left
// sparse (a gap, an entry) keeps what it has.
// Deep and Chill keys hold long chords, so a new take barely changed them. Each take now picks its
// own voicing (the same chords one inversion up or down), whether a chord change is pushed an
// eighth early, and whether a long chord is struck again on beat three. Decided on the written grid.
static void chordTake(Clip& c,std::uint32_t salt,double beats,bool anticipate=true);
static void pianoTake(Clip& c,const Project& p,double beats){
    const auto g=(Genre)p.settings.genre;if(g!=Genre::deepHouse&&g!=Genre::chill)return;chordTake(c,0x2c1b3c6dU,beats);
}
// Held chords (Deep / Chill keys, and the string pads of every genre) vary per take the same way.
static void chordTake(Clip& c,std::uint32_t salt,double beats,bool anticipate){
    if(c.notes.size()<4)return;
    const std::uint32_t r=mix(c.seed^salt);const int inversion=(int)(r%3)-1;const bool push=anticipate&&((r>>2)&1),restrike=(r>>3)&1;
    std::sort(c.notes.begin(),c.notes.end(),byStartPitch);
    std::vector<Note> out;
    for(size_t i=0;i<c.notes.size();){size_t j=i;while(j<c.notes.size()&&std::round(c.notes[j].start*8)==std::round(c.notes[i].start*8))++j;
        std::vector<Note> chord(c.notes.begin()+(long)i,c.notes.begin()+(long)j);
        if(chord.size()>=3&&inversion!=0){if(inversion>0&&chord.front().pitch+12<=96){chord.front().pitch+=12;}else if(inversion<0&&chord.back().pitch-12>=36){chord.back().pitch-=12;}
            std::sort(chord.begin(),chord.end(),[](const Note& a,const Note& b){return a.pitch<b.pitch;});
            for(size_t k=1;k<chord.size();++k)if(chord[k].pitch==chord[k-1].pitch)chord[k].velocity=0;}   // never two equal notes
        const double at=std::round(chord.front().start*8)/8;const bool barStart=std::abs(at/4-std::round(at/4))<1e-6;
        if(push&&barStart&&at>=4&&((int)std::lround(at/4))%2==1)for(auto& n:chord){n.start=std::max(0.,n.start-.5);n.length+=.5;}   // anticipation: the chord lands on the and of four
        for(auto& n:chord){if(n.velocity==0)continue;
            if(restrike&&n.length>=3.5&&barStart){Note again=n;again.start=n.start+2;again.length=n.length-2;again.velocity=std::max(1,n.velocity-14);n.length=1.9;out.push_back(n);if(again.start<beats-1./32.)out.push_back(again);}
            else out.push_back(n);}
        i=j;}
    c.notes=out;tidyNotes(c,beats);
}
// Latin House guitar plays a montuno of its own instead of the Afro guitar cells the engine shares:
// a two-bar syncopated figure (the take picks one of three), on the chords the engine voiced.
static void latinMontuno(Clip& c,double beats){
    if(c.notes.size()<3)return;
    static const char* const cells[]{"x.x..x.x.x..x.x...x..x.x.x.x.x.x","..x.x..x..x.x..x..x.x..x.x..x.x.","x..x.x.x..x..x.x.x..x.x.x..x.x.x"};
    const char* cell=cells[mix(c.motifSeed^0x6d0e7a1fU)%3];
    std::sort(c.notes.begin(),c.notes.end(),byStartPitch);
    std::vector<std::vector<Note>> chords;for(const auto& n:c.notes){if(chords.empty()||n.start-chords.back().front().start>=1./24.)chords.push_back({});chords.back().push_back(n);}
    std::vector<Note> out;
    for(double bar=0;bar<beats-1e-9;bar+=8)for(int step=0;step<32;++step){if(cell[step]!='x')continue;const double at=bar+step*.25;if(at>=beats-1./32.)break;
        const std::vector<Note>* chord=nullptr;for(const auto& g:chords)if(g.front().start<=at+1e-6)chord=&g;else break;
        if(!chord){continue;}const double end=chord->front().start+chord->front().length;if(at-end>1.)continue;   // the engine rests here: so does the montuno
        for(const auto& n:*chord){Note m=n;m.start=at;m.length=std::min(.22,beats-at);m.velocity=std::clamp(n.velocity+(step%4==0?6:step%2?-8:0),1,127);out.push_back(m);}}
    if(out.size()>=3){c.notes=out;tidyNotes(c,beats);}
}
static void pianoMotif(Clip& c,const Project& p,double length){
    if(isElectronicGenre(p.settings.genre)||c.notes.size()<8)return;
    {   // decisions read the written grid (1/32), so a feel that moves a chord a few ms never changes them
        const int bars=(int)std::floor(length/4+1e-9);auto q=[](double x){return std::round(x*8)/8;};
        auto groups=[&](int bar){std::vector<std::vector<Note>> g;for(const auto& n:c.notes){if(q(n.start)<bar*4-1e-9||q(n.start)>=bar*4+4-1e-9)continue;if(g.empty()||n.start-g.back().front().start>=1./24.)g.push_back({});g.back().push_back(n);}return g;};
        std::sort(c.notes.begin(),c.notes.end(),byStartPitch);
        bool changed=false;
        for(int phrase=0;phrase+2<bars;phrase+=4){const int from=phrase,to=phrase+2;const auto src=groups(from),dst=groups(to);
            if(src.size()<2||dst.size()<2||dst.size()*2<src.size()||src.size()*2<dst.size())continue;
            if(q(dst.front().front().start)-to*4>1.+1e-9||q(dst.back().front().start)-to*4<2.-1e-9)continue;   // a bar with a gap keeps its own notes
            double srcV=0,dstV=0;int sn=0,dn=0;for(const auto& g:src)for(const auto& n:g){srcV+=n.velocity;++sn;}for(const auto& g:dst)for(const auto& n:g){dstV+=n.velocity;++dn;}const double scale=(dstV/dn)/std::max(1.,srcV/sn);
            std::vector<Note> made;
            for(const auto& g:src){const double rel=g.front().start-from*4,at=to*4+rel;
                const std::vector<Note>* chord=&dst.front();for(const auto& d:dst)if(q(d.front().start)<=q(at)+1e-6)chord=&d;   // the chord this bar holds at that moment
                double len=0;for(const auto& n:g)len=std::max(len,n.length);len=std::min(len,to*4+4-at-1./64.);if(len<1./32.)continue;
                const int vel=(int)std::lround(g.front().velocity*scale);
                for(const auto& n:*chord){Note m=n;m.start=at;m.length=len;m.velocity=std::clamp(vel+(n.velocity-chord->front().velocity),1,127);made.push_back(m);}}
            if(made.empty())continue;
            c.notes.erase(std::remove_if(c.notes.begin(),c.notes.end(),[&](const Note& n){return q(n.start)>=to*4-1e-9&&q(n.start)<to*4+4-1e-9;}),c.notes.end());
            c.notes.insert(c.notes.end(),made.begin(),made.end());changed=true;}
        if(changed)tidyNotes(c,length);}
}
// The lead listens: where the piano strikes it steps back a little, in the piano's gaps it speaks up.
static void leadAnswer(Project& p,const std::function<bool(int,int)>& fresh){
    if((int)p.tracks.size()<=leadTrack||p.tracks[(size_t)leadTrack].locked||isElectronicGenre(p.settings.genre))return;
    std::vector<std::pair<double,double>> piano;for(size_t ci=0;ci<p.tracks[0].clips.size();++ci){const double at=p.clipStart(0,(int)ci);for(const auto& n:p.tracks[0].clips[ci].notes)piano.push_back({at+n.start,at+n.start+n.length});}
    auto& t=p.tracks[(size_t)leadTrack];
    for(size_t ci=0;ci<t.clips.size();++ci){auto& c=t.clips[ci];if(c.locked||c.loopLength>0||!fresh(leadTrack,(int)ci))continue;
        // Dance Stabs: the line stays within two octaves of its own centre
        if(p.settings.genre==(int)Genre::danceStabs&&c.notes.size()>2){std::vector<int> ps;for(const auto& n:c.notes)ps.push_back(n.pitch);std::nth_element(ps.begin(),ps.begin()+(long)ps.size()/2,ps.end());const int mid=ps[ps.size()/2];for(auto& n:c.notes){while(n.pitch>mid+12)n.pitch-=12;while(n.pitch<mid-12)n.pitch+=12;}}}
    if(piano.empty()||p.tracks[0].mute)return;std::sort(piano.begin(),piano.end());
    for(size_t ci=0;ci<t.clips.size();++ci){auto& c=t.clips[ci];if(c.locked||c.loopLength>0||!fresh(leadTrack,(int)ci))continue;const double at=p.clipStart(leadTrack,(int)ci);
        for(auto& n:c.notes){const double x=at+n.start;bool hit=false,sounding=false;
            auto it=std::lower_bound(piano.begin(),piano.end(),std::make_pair(x-4.,0.));
            for(;it!=piano.end()&&it->first<=x+1./16.;++it){if(std::abs(it->first-x)<1./16.)hit=true;if(it->first<=x&&it->second>x)sounding=true;}
            if(hit)n.velocity=std::clamp((int)std::lround(n.velocity*.9),1,127);else if(!sounding)n.velocity=std::clamp((int)std::lround(n.velocity*1.06),1,127);}}
}
// Intros and outros keep the colour for later: piano, strings and guitar play triads and sevenths
// there, the added 9ths, 11ths and 13ths arrive with the body of the song.
static void sectionTension(Project& p,const std::function<bool(int,int)>& fresh){
    if(isElectronicGenre(p.settings.genre))return;
    for(int ti:{0,2,3}){if(ti>=(int)p.tracks.size()||p.tracks[(size_t)ti].locked)continue;auto& t=p.tracks[(size_t)ti];
        for(size_t ci=0;ci<t.clips.size();++ci){auto& c=t.clips[ci];if(c.locked||c.loopLength>0||c.notes.empty()||!fresh(ti,(int)ci))continue;const double at=p.clipStart(ti,(int)ci);
            const int si=std::max(0,p.sectionAtBeat(at));if(si>=(int)p.sections.size())continue;const auto type=p.sections[(size_t)si].type;if(type!=Section::intro&&type!=Section::outro)continue;
            const auto h=activeHarmony(p,si);
            std::sort(c.notes.begin(),c.notes.end(),byStartPitch);
            std::vector<Note> out;
            for(size_t i=0;i<c.notes.size();){size_t j=i;while(j<c.notes.size()&&c.notes[j].start-c.notes[i].start<1./24.)++j;
                const auto ch=chordAtBeat(h,at+c.notes[i].start);std::vector<Note> core,colour;
                for(size_t k=i;k<j;++k){const int iv=((c.notes[k].pitch-ch.rootPc)%12+12)%12;(iv==0||iv==ch.third||iv==ch.fifth||iv==ch.seventh?core:colour).push_back(c.notes[k]);}
                if(core.size()>=3||(core.size()>=2&&j-i>=4)){out.insert(out.end(),core.begin(),core.end());}else{for(size_t k=i;k<j;++k)out.push_back(c.notes[k]);}
                i=j;}
            c.notes=out;}}
}
// The piano never holds a note a minor ninth over a long bass note (the muddiest rub there is):
// in a chord of four or more that note is left out.
static void pianoClearsBass(Project& p,const std::function<bool(int,int)>& fresh){
    if(p.tracks.size()<2||p.tracks[0].locked||p.tracks[1].mute||isElectronicGenre(p.settings.genre))return;
    struct Held{double from,to;int pitch;};std::vector<Held> bass;for(size_t ci=0;ci<p.tracks[1].clips.size();++ci){const double at=p.clipStart(1,(int)ci);for(const auto& n:p.tracks[1].clips[ci].notes)if(n.length>.5)bass.push_back({at+n.start,at+n.start+n.length,n.pitch});}
    if(bass.empty())return;std::sort(bass.begin(),bass.end(),[](const Held& a,const Held& b){return a.from<b.from;});double longest=0;for(const auto& b:bass)longest=std::max(longest,b.to-b.from);auto& t=p.tracks[0];
    for(size_t ci=0;ci<t.clips.size();++ci){auto& c=t.clips[ci];if(c.locked||c.loopLength>0||!fresh(0,(int)ci))continue;const double at=p.clipStart(0,(int)ci);
        auto rubs=[&](const Note& n){const double from=at+n.start,to=from+n.length;   // only the bass notes that can overlap it
            for(auto it=std::lower_bound(bass.begin(),bass.end(),from-longest,[](const Held& h,double x){return h.from<x;});it!=bass.end()&&it->from<to;++it){const auto& b=*it;if(b.to<=from)continue;if(std::min(b.to,at+n.start+n.length)-std::max(b.from,at+n.start)<.25)continue;if(n.pitch>b.pitch&&(n.pitch-b.pitch)%12==1)return true;}return false;};
        std::sort(c.notes.begin(),c.notes.end(),byStartPitch);
        auto together=[&](size_t i){size_t a=i,b=i;while(a>0&&c.notes[i].start-c.notes[a-1].start<1./24.)--a;while(b+1<c.notes.size()&&c.notes[b+1].start-c.notes[i].start<1./24.)++b;return (int)(b-a+1);};
        std::vector<Note> out;for(size_t i=0;i<c.notes.size();++i)if(!(rubs(c.notes[i])&&together(i)>=4))out.push_back(c.notes[i]);c.notes=out;}
}
// VOCALS PRESENT: the band leaves room for a lead singer, from the one project setting every
// generation path reads. Verses (main): the lead and a solo guitar answer in the second half of
// each four-bar phrase (the singer takes the first), the piano, guitar and strings stay below the
// voice's presence range (a note from C6 up folds an octave down) and a little softer. Choruses
// (drops): the lead keeps its strong beats only, softer. Breaks: the lead rests where the singer
// would. Intros and outros stay as they are (DJ mixing); kick, bass and drums are never touched.
// A verse or a drop that comes back is not a copy of the first one. The k-th return of a section
// type (same harmony, same hook) varies by a fixed plan: odd returns and the final drop lift the lead
// an octave (when it fits) and push the piano's bar-two chords an eighth early; every return enters
// with one layer fewer for its first eight bars (the guitar on odd returns, the strings on even ones)
// so the section grows as it goes. Notes only move by octaves or eighths: harmony and identity stay.
static void repeatVariations(Project& p,const std::function<bool(int,int)>& fresh){
    if(isElectronicGenre(p.settings.genre))return;
    std::vector<int> occurrence(p.sections.size(),0);int lastDrop=-1,drops=0;
    {std::array<int,6> seen{};for(size_t si=0;si<p.sections.size();++si){occurrence[si]=seen[(size_t)p.sections[si].type]++;if(p.sections[si].type==Section::drop){lastDrop=(int)si;++drops;}}}
    auto sectionOf=[&](int ti,int ci){const int si=p.sectionAtBeat(p.clipStart(ti,ci)+.01);return si>=0&&si<(int)p.sections.size()&&(p.sections[(size_t)si].type==Section::main||p.sections[(size_t)si].type==Section::drop)?si:-1;};
    for(int ti:{0,2,3,leadTrack}){if(ti>=(int)p.tracks.size()||p.tracks[(size_t)ti].locked)continue;auto& t=p.tracks[(size_t)ti];
        for(size_t ci=0;ci<t.clips.size();++ci){auto& c=t.clips[ci];if(c.locked||c.loopLength>0||c.notes.empty()||!fresh(ti,(int)ci))continue;
            const int si=sectionOf(ti,(int)ci);if(si<0)continue;const int k=occurrence[(size_t)si];const bool finalDrop=drops>1&&si==lastDrop;if(k==0&&!finalDrop)continue;
            const double at=p.clipStart(ti,(int)ci),from=p.sectionStart(si),length=p.clipLength(ti,(int)ci);
            if(ti==leadTrack&&(k%2==1||finalDrop)){int top=0;for(const auto& n:c.notes)top=std::max(top,n.pitch);if(top+12<=88)for(auto& n:c.notes)n.pitch+=12;}
            if(ti==0&&k%2==1){auto clear=[&](double s,double e){return std::none_of(c.notes.begin(),c.notes.end(),[&](const Note& o){return o.start<e-1e-6&&o.start+o.length>s+1e-6;});};   // only into an eighth the piano leaves silent
                std::vector<size_t> push;for(size_t i=0;i<c.notes.size();++i){const auto& n=c.notes[i];const double rel=at+n.start-from;const int bar=(int)std::floor(rel/4+1e-9);if(bar%2==1&&std::abs(rel-bar*4)<1./32.&&n.start>=.5&&clear(n.start-.5,n.start))push.push_back(i);}
                for(size_t i:push){c.notes[i].start-=.5;c.notes[i].length+=.5;}}
            const bool layerOut=p.sections[(size_t)si].bars>=16&&((ti==3&&k%2==1&&!p.settings.guitarLead)||(ti==2&&k>=2&&k%2==0));
            if(layerOut)c.notes.erase(std::remove_if(c.notes.begin(),c.notes.end(),[&](const Note& n){return at+n.start-from<32.-1./32.;}),c.notes.end());
            tidyNotes(c,length);}}
}
static void vocalSpace(Project& p,const std::function<bool(int,int)>& fresh){
    if(!p.settings.vocalsPresent)return;
    const bool soloGuitar=p.settings.guitarLead;
    for(int ti:{0,2,3,leadTrack}){if(ti>=(int)p.tracks.size()||p.tracks[(size_t)ti].locked)continue;auto& t=p.tracks[(size_t)ti];const bool melody=ti==leadTrack||(ti==3&&soloGuitar);
        for(size_t ci=0;ci<t.clips.size();++ci){auto& c=t.clips[ci];if(c.locked||c.loopLength>0||c.notes.empty()||!fresh(ti,(int)ci))continue;
            const double at=p.clipStart(ti,(int)ci);const int si=std::max(0,p.sectionAtBeat(at+.01));if(si>=(int)p.sections.size())continue;const auto type=p.sections[(size_t)si].type;
            if(type==Section::intro||type==Section::outro)continue;
            const double from=p.sectionStart(si);std::vector<Note> out;
            for(auto n:c.notes){const double rel=at+n.start-from;const int bar=(int)std::floor(rel/4+1e-9);
                if(melody){
                    const auto& sung=p.settings.vocalBars;const int songBar=(int)std::floor((at+n.start)/4+1e-9);
                    const bool singer=p.settings.vocalPhrase==3?songBar>=0&&songBar<(int)sung.size()&&sung[(size_t)songBar]:p.settings.vocalPhrase==1?bar%4>=2:p.settings.vocalPhrase==2?bar%8<4:bar%4<2;
                    if(type==Section::main&&singer)continue;                         // the singer's half of the phrase
                    if(type==Section::breakdown&&bar%2==0)continue;                  // answers only
                    if(type==Section::drop){const double beat=std::fmod(rel,1.);if(beat>.05&&beat<.95&&n.length<.5)continue;n.velocity=std::max(1,(int)std::lround(n.velocity*.88));}
                }else if(type==Section::main||type==Section::drop){
                    if(n.pitch>=84){
                        // Out of the voice's 2-5 kHz presence band by re-voicing: the top note folds into the chord an
                        // octave down unless that lands on (or a semitone from) a note already sounding there; then the
                        // chord drops it, as long as something else still sounds with it.
                        const int low=n.pitch-12;bool clash=false,others=false;
                        for(const auto& o:c.notes){if((o.pitch==n.pitch&&o.start==n.start)||o.start>=n.start+n.length||n.start>=o.start+o.length)continue;others=true;if(std::abs(o.pitch-low)<=1)clash=true;}
                        if(!clash)n.pitch=low;else if(others)continue;
                    }
                    if(type==Section::main)n.velocity=std::max(1,(int)std::lround(n.velocity*.9));
                }
                out.push_back(n);}
            if(melody&&out.size()<2)continue;   // never empty a part completely
            c.notes=std::move(out);tidyNotes(c,p.clipLength(ti,(int)ci));}}
}
// Added band parts, written from the band the arranger just generated.
// SUB BASS: the root of the chord under the bass, one octave band (D1-C#2, 37-69 Hz), held while
// the bass plays and restruck only where the bass moves to another root, so the low end has one
// controlled fundamental instead of a doubled line; mono and low-passed in the engine. Silent in
// intros, outros and breaks, and on the beat before a drop's stop.
// COUNTER MELODY: short answers (two to four notes) in the gaps the lead leaves, on chord tones a
// fifth below the lead's register, stepping down to a resting tone; never sounding with a lead note,
// never a semitone from a held piano note. With VOCALS PRESENT it answers only where the singer rests.
static void generateParts(Project& p,Scope scope,int track,int section,int lastClip=-1){
    for(int ti=trackCount;ti<(int)p.tracks.size();++ti){auto& t=p.tracks[(size_t)ti];if(t.part<=0||t.locked||((scope==Scope::track||scope==Scope::clip)&&ti!=track))continue;
        struct Span{double from,to;int pitch;};
        auto spans=[&](int src){std::vector<Span> out;if(src>=(int)p.tracks.size())return out;const auto& s=p.tracks[(size_t)src];if(s.mute)return out;for(size_t ci=0;ci<s.clips.size();++ci){const double at=p.clipStart(src,(int)ci);for(const auto& n:s.clips[ci].notes)out.push_back({at+n.start,at+n.start+n.length,n.pitch});}
            std::sort(out.begin(),out.end(),[](const Span& a,const Span& b){return a.from<b.from;});return out;};
        const auto bass=spans(1),lead=spans(leadTrack),piano=spans(0),kick=spans(4);
        // phase with the kick: a sub attack within a 32nd of a kick hit starts exactly on it (the sub's oscillator restarts
        // at zero phase on every note), so the two fundamentals begin together instead of a few milliseconds apart
        auto onKick=[&](double beat){auto it=std::lower_bound(kick.begin(),kick.end(),beat-1./32.,[](const Span& s,double x){return s.from<x;});return it!=kick.end()&&std::abs(it->from-beat)<=1./32.?it->from:beat;};
        for(int ci=0;ci<(int)t.clips.size();++ci){auto& c=t.clips[(size_t)ci];
            if(c.locked||c.loopLength>0||(scope==Scope::clip&&(ci<section||ci>(lastClip<0?section:lastClip)))||(scope==Scope::section&&ci!=section))continue;   // a looped clip keeps its loop
            if(c.seed<=1)c.seed=mix(p.settings.seed+(std::uint32_t)ti*131U+(std::uint32_t)ci*977U);
            const double at=p.clipStart(ti,ci),length=p.clipLength(ti,ci);const int si=std::clamp(p.sectionAtBeat(at+.01),0,(int)p.sections.size()-1);const auto type=p.sections[(size_t)si].type;
            const auto harmony=activeHarmony(p,si);c.notes.clear();
            if(t.part==1){
                if(type==Section::intro||type==Section::outro||type==Section::breakdown)continue;
                const double stop=si+1<(int)p.sections.size()&&p.sections[(size_t)si+1].type==Section::drop?p.sectionStart(si+1)-std::max(1,p.sections[(size_t)si].dropGap):at+length;
                for(const auto& b:bass){if(b.to<=at||b.from>=std::min(at+length,stop))continue;const double from=std::max(onKick(b.from),at),to=std::min({b.to,at+length,stop});if(to-from<1./16.)continue;
                    const auto chord=chordAtBeat(harmony,std::round(from*4)/4);int pitch=26+((chord.rootPc-26)%12+12)%12;   // D1..C#2
                    if(!c.notes.empty()){auto& last=c.notes.back();if(last.pitch==pitch&&from-(at+last.start+last.length)<.5){last.length=to-at-last.start;continue;}}   // same root: hold, do not restrike
                    c.notes.push_back({from-at,to-from,pitch,100,0});}
            }else{
                const bool vocals=p.settings.vocalsPresent;const double sectionFrom=p.sectionStart(si);
                int centre=72;{double sum=0;int k=0;for(const auto& l:lead)if(l.from>=at&&l.from<at+length){sum+=l.pitch;++k;}if(k)centre=(int)std::lround(sum/k);}
                const int home=centre-7;std::uint32_t r=mix(c.seed^0x5c0ffee1U);auto dice=[&](){return nextDice(r);};
                const double density=std::clamp(takeFeel(t.engine.density,c.feel[0],p.settings.density),0.,1.);
                auto leadAt=[&](double a,double b){for(const auto& l:lead)if(l.from<b&&l.to>a)return true;return false;};
                double longest=0;for(const auto& s:piano)longest=std::max(longest,s.to-s.from);
                auto rubs=[&](int pitch,double a,double b){for(auto it=std::lower_bound(piano.begin(),piano.end(),a-longest,[](const Span& s,double x){return s.from<x;});it!=piano.end()&&it->from<b;++it)if(it->to>a&&std::abs(it->pitch-pitch)%12==1)return true;return false;};
                for(double bar=0;bar+4<=length+1e-9;bar+=2){const double a=at+bar,b=a+8;   // one answer at most every two bars, inside a gap of the lead
                    if(type==Section::intro||type==Section::outro)break;
                    if(vocals&&type==Section::main&&((int)std::floor((a-sectionFrom)/4+1e-9))%4<2)continue;
                    if(dice()>.35+.5*density)continue;
                    double gap=-1;for(double s=a;s<std::min(b,at+length)-1.5;s+=.5)if(!leadAt(s,s+1.5)){gap=s;break;}if(gap<0)continue;
                    const int notes=2+(int)(dice()*3);int pitch=home;
                    for(int k=0;k<notes;++k){const double s=gap+.5+k*.5;double len=k==notes-1?.9:.4;if(k==notes-1&&leadAt(s,s+len))len=.4;if(s+len>at+length||leadAt(s,s+len))break;
                        const auto chord=chordAtBeat(harmony,s);int best=pitch;int distance=99;
                        for(int q=pitch-4;q<=pitch+4;++q){const int iv=((q-chord.rootPc)%12+12)%12;if((iv==0||iv==chord.third||iv==chord.fifth||(k>0&&iv==chord.seventh))&&!rubs(q,s,s+len)&&std::abs(q-pitch)<distance&&(k==0||q<=pitch)){distance=std::abs(q-pitch);best=q;}}
                        if(distance==99)break;pitch=best;c.notes.push_back({s-at,len,pitch,k==0?92:80,0});pitch-=2;}
                }
            }
            tidyNotes(c,length);}}
}
int Project::addPartTrack(int kind){
    if(kind<1||kind>2)return -1;const int index=addMidiTrack();if(index<0)return index;auto& t=tracks[(size_t)index];
    t.part=kind;t.name=kind==1?"Sub bass":"Counter melody";t.freeTimeline=false;t.clips.assign(sections.size(),{});
    // the Sub Sine voice (a pure sine), ducked by every kick hit (sample-accurate sidechain on the kick's own notes)
    // so kick and sub never stack their low end; the engine's low-pass only trims what sits above it
    if(kind==1){t.sound=7;t.engine.tone=.3;t.gain=.5;t.engine.pump=.8;t.engine.pumpSource=1;t.engine.pumpRelease=.25;}
    else{t.sound=0;t.engine.tone=.45;t.gain=.38;t.engine.density=.5;}
    generateParts(*this,Scope::track,index,0);return index;
}
// The bass's strong beats belong to the chord: a note on beat 1 or 3 that is not its root, third, fifth or
// seventh (a 13th under the piano's seventh, a 9th under its root) moves to the nearest root, fifth or
// third; the passing notes on the weak 16ths stay.
static void bassOnChordTones(Project& p,const std::function<bool(int,int)>& inScope){
    if((int)p.tracks.size()<2)return;auto& t=p.tracks[1];if(t.locked||t.instrument=="sample")return;
    for(size_t ci=0;ci<t.clips.size();++ci){auto& c=t.clips[ci];if(c.locked||c.loopLength>0||c.notes.empty()||!inScope(1,(int)ci))continue;const double at=p.clipStart(1,(int)ci);bool moved=false;
        for(auto& n:c.notes){const double beat=at+n.start;const double inBeat=std::fmod(beat,1.);if(!(inBeat<1./16.||inBeat>1-1./16.))continue;
            const bool strong=std::abs(std::fmod(std::round(beat),2.))<.5;   // beats 1 and 3
            const int si=p.sectionAtBeat(beat+.01);if(si<0)continue;const auto ch=chordAtBeat(activeHarmony(p,si),beat+.01);const int iv=((n.pitch-ch.rootPc)%12+12)%12;
            if(iv==0||iv==ch.third||iv==ch.fifth||(ch.seventh>0&&iv==ch.seventh))continue;
            if(!strong&&(iv!=1&&iv!=11||n.length<.25))continue;   // beats 2 and 4: only a held note a semitone from the root
            int best=n.pitch,dist=99;for(int target:{0,ch.fifth,ch.third})for(int q=n.pitch-6;q<=n.pitch+6;++q)if(((q-ch.rootPc)%12+12)%12==target&&q>=24&&q<=62&&std::abs(q-n.pitch)+(target==0?0:1)<dist){dist=std::abs(q-n.pitch)+(target==0?0:1);best=q;}
            if(best!=n.pitch){n.pitch=best;moved=true;}}
        if(moved)tidyNotes(c,p.clipLength(1,(int)ci));}
}
// The last word on semitones between parts, after every other pass (the lead's answers included): the
// lead and the piano keep their notes; a strings note a minor second or ninth from a piano or lead note it
// sounds with for an 8th or more moves an octave (or goes, when the chord keeps two other notes); then the
// guitar against piano, strings and lead.
static void finalVoicing(Project& p,const std::function<bool(int,int)>& inScope){
    if((int)p.tracks.size()<=leadTrack||isElectronicGenre(p.settings.genre))return;
    struct Span{double from,to;int pitch;};
    auto spans=[&](int ti){std::vector<Span> out;const auto& t=p.tracks[(size_t)ti];if(t.mute)return out;for(size_t ci=0;ci<t.clips.size();++ci){const double at=p.clipStart(ti,(int)ci);for(const auto& n:t.clips[ci].notes)out.push_back({at+n.start,at+n.start+n.length,n.pitch});}return out;};
    auto declash=[&](int ti,std::vector<Span> against){auto& t=p.tracks[(size_t)ti];if(t.locked||t.mute||against.empty())return;
        std::sort(against.begin(),against.end(),[](const Span& a,const Span& b){return a.from<b.from;});double longest=0;for(const auto& o:against)longest=std::max(longest,o.to-o.from);
        auto window=[&](double from){return std::lower_bound(against.begin(),against.end(),from-longest,[](const Span& s,double x){return s.from<x;});};
        for(size_t ci=0;ci<t.clips.size();++ci){auto& c=t.clips[ci];if(c.locked||c.loopLength>0||c.notes.empty()||!inScope(ti,(int)ci))continue;const double at=p.clipStart(ti,(int)ci);bool moved=false;
            auto clashes=[&](int pitch,double from,double to){int count=0;for(auto it=window(from);it!=against.end()&&it->from<to;++it){const auto& o=*it;if(o.to<=from||std::min(o.to,to)-std::max(o.from,from)<.125)continue;const int lo=std::min(pitch,o.pitch),hi=std::max(pitch,o.pitch);if(hi!=lo&&(hi-lo)%12==1)++count;}return count;};
            for(auto& n:c.notes){const double from=at+n.start,to=from+n.length;const int now=clashes(n.pitch,from,to);if(!now)continue;
                int best=n.pitch,fewest=now;for(int shift:{12,-12,24,-24}){const int q=n.pitch+shift;if(q<40||q>96)continue;
                    if(std::any_of(c.notes.begin(),c.notes.end(),[&](const Note& o){return &o!=&n&&o.pitch==q&&std::abs(o.start-n.start)<1./16.;}))continue;
                    const int k=clashes(q,from,to);if(k<fewest){fewest=k;best=q;}}
                if(best!=n.pitch){n.pitch=best;moved=true;}
                // still rubbing: a chord note goes when the chord keeps two other notes; a single line note stays
                if(fewest>0&&std::count_if(c.notes.begin(),c.notes.end(),[&](const Note& o){return &o!=&n&&o.velocity>0&&std::abs(o.start-n.start)<1./16.;})>=2){n.velocity=0;moved=true;}}
            if(moved){c.notes.erase(std::remove_if(c.notes.begin(),c.notes.end(),[](const Note& n){return n.velocity==0;}),c.notes.end());tidyNotes(c,p.clipLength(ti,(int)ci));}}};
    // The low register is the bass's: a piano note below the bass note it sounds with, and a strings or
    // guitar note under E3 while the bass plays, move up an octave (or go, when that octave is already there).
    {const auto bass=spans(1);if(!bass.empty())for(int ti:{0,2,3}){auto& t=p.tracks[(size_t)ti];if(t.locked||t.mute)continue;
        for(size_t ci=0;ci<t.clips.size();++ci){auto& c=t.clips[ci];if(c.locked||c.loopLength>0||!inScope(ti,(int)ci))continue;const double at=p.clipStart(ti,(int)ci);bool moved=false;
            for(auto& n:c.notes){const double from=at+n.start,to=from+n.length;
                for(const auto& b:bass)if(b.from<to&&b.to>from&&std::min(b.to,to)-std::max(b.from,from)>=.125&&(n.pitch<b.pitch||(ti>0&&n.pitch<52))&&n.pitch+12<=96){
                    if(std::any_of(c.notes.begin(),c.notes.end(),[&](const Note& o){return &o!=&n&&o.pitch==n.pitch+12&&std::abs(o.start-n.start)<1./16.;}))n.velocity=0;else n.pitch+=12;moved=true;break;}}
            if(moved){c.notes.erase(std::remove_if(c.notes.begin(),c.notes.end(),[](const Note& n){return n.velocity==0;}),c.notes.end());tidyNotes(c,p.clipLength(ti,(int)ci));}}}}
    // the lead resolves instead: a lead note held a quarter beat or more a minor second or ninth from a piano
    // note moves to the nearest chord tone within a whole tone that rubs with nothing (an appoggiatura that resolves)
    {auto piano=spans(0);std::sort(piano.begin(),piano.end(),[](const Span& a,const Span& b){return a.from<b.from;});auto& t=p.tracks[(size_t)leadTrack];
     if(!t.locked&&!piano.empty())for(size_t ci=0;ci<t.clips.size();++ci){auto& c=t.clips[ci];if(c.locked||c.loopLength>0||!inScope(leadTrack,(int)ci))continue;const double at=p.clipStart(leadTrack,(int)ci);bool moved=false;
        auto rubs=[&](int pitch,double from,double to){for(const auto& o:piano){if(o.from>=to)break;if(o.to<=from||std::min(o.to,to)-std::max(o.from,from)<.25)continue;const int d=std::abs(pitch-o.pitch);if(d%12==1&&d<25)return true;}return false;};
        for(auto& n:c.notes){const double from=at+n.start,to=from+n.length;if(n.length<.25||!rubs(n.pitch,from,to))continue;const int si=p.sectionAtBeat(from+.01);if(si<0)continue;const auto ch=chordAtBeat(activeHarmony(p,si),from+.01);
            for(int d:{-1,1,-2,2}){const int q=n.pitch+d,iv=((q-ch.rootPc)%12+12)%12;if((iv==0||iv==ch.third||iv==ch.fifth||(ch.seventh>0&&iv==ch.seventh))&&q>=t.engine.rangeLow&&q<=t.engine.rangeHigh&&!rubs(q,from,to)){n.pitch=q;moved=true;break;}}}
        if(moved)tidyNotes(c,p.clipLength(leadTrack,(int)ci));}}
    // Pads and comping keep their place from section to section: when a strings or guitar section's centre lies 9
    // semitones or more from the section before, the whole section moves an octave toward it (a pad does not
    // jump an octave at a section change). Then no chord under C3 holds a third or a second (the low-interval
    // limit): the lower note goes up an octave, or out when that octave is already there.
    for(int ti:{2,3}){auto& t=p.tracks[(size_t)ti];if(t.locked||t.mute||(ti==3&&p.settings.guitarLead))continue;double previous=-1;
        for(size_t ci=0;ci<t.clips.size();++ci){auto& c=t.clips[ci];std::vector<int> pitches;for(const auto& n:c.notes)pitches.push_back(n.pitch);
            if(pitches.size()<4){continue;}std::nth_element(pitches.begin(),pitches.begin()+(long)pitches.size()/2,pitches.end());double centre=pitches[pitches.size()/2];
            if(previous>0&&!c.locked&&c.loopLength<=0&&inScope(ti,(int)ci)&&std::abs(centre-previous)>=9){const int shift=centre>previous?-12:12;
                const int ceiling=p.settings.vocalsPresent?83:96;const bool fits=std::all_of(c.notes.begin(),c.notes.end(),[&](const Note& n){return n.pitch+shift>=52&&n.pitch+shift<=ceiling;});   /* under the voice with VOCALS PRESENT */
                if(fits){for(auto& n:c.notes)n.pitch+=shift;centre+=shift;}}
            previous=centre;}}
    for(int ti:{0,2,3}){auto& t=p.tracks[(size_t)ti];if(t.locked||t.mute)continue;
        for(size_t ci=0;ci<t.clips.size();++ci){auto& c=t.clips[ci];if(c.locked||c.loopLength>0||!inScope(ti,(int)ci))continue;bool moved=false;
            for(int pass=0;pass<3;++pass){std::map<long,std::vector<Note*>> chords;for(auto& n:c.notes)if(n.velocity>0)chords[std::lround(n.start*16)].push_back(&n);
                bool any=false;for(auto& [at,ns]:chords){std::sort(ns.begin(),ns.end(),[](const Note* a,const Note* b){return a->pitch<b->pitch;});
                    for(size_t k=0;k+1<ns.size();++k)if(ns[k]->pitch<48&&ns[k+1]->pitch-ns[k]->pitch<5){Note* low=ns[k];const int up=low->pitch+12;
                        if(std::any_of(ns.begin(),ns.end(),[&](const Note* o){return o!=low&&o->pitch==up;}))low->velocity=0;else low->pitch=up;moved=any=true;break;}}
                if(!any)break;}
            if(moved){c.notes.erase(std::remove_if(c.notes.begin(),c.notes.end(),[](const Note& n){return n.velocity==0;}),c.notes.end());tidyNotes(c,p.clipLength(ti,(int)ci));}}}
    const auto lead=spans(leadTrack);   // the piano keeps its voicing (APPLY and harmony edits round-trip it exactly)
    auto upper=spans(0);upper.insert(upper.end(),lead.begin(),lead.end());declash(2,upper);
    const auto strings=spans(2);upper.insert(upper.end(),strings.begin(),strings.end());declash(3,upper);
}
// Two comping instruments, two roles: outside the drops the guitar leaves the piano's attacks to the piano
// and plays in the spaces between them (strums on the same 16th as a piano chord thin out), so the keys and
// the guitar answer each other instead of hitting together; in the drops both play. Every other shared
// strum goes, at most a third of them, so each take keeps its own figure; the solo guitar (GUITAR LEADS) is a melody and stays.
static void bandRoles(Project& p,const std::function<bool(int,int)>& inScope){
    if((int)p.tracks.size()<4||isElectronicGenre(p.settings.genre)||p.settings.guitarLead)return;auto& g=p.tracks[3];if(g.locked||g.mute||p.tracks[0].mute)return;
    std::vector<double> piano;{const auto& t=p.tracks[0];for(size_t ci=0;ci<t.clips.size();++ci){const double at=p.clipStart(0,(int)ci);for(const auto& n:t.clips[ci].notes)piano.push_back(at+n.start);}}
    std::sort(piano.begin(),piano.end());if(piano.empty())return;
    auto pianoAt=[&](double b){auto it=std::lower_bound(piano.begin(),piano.end(),b-1./32.);return it!=piano.end()&&*it<=b+1./32.;};
    for(size_t ci=0;ci<g.clips.size();++ci){auto& c=g.clips[ci];if(c.locked||c.loopLength>0||c.notes.size()<4||!inScope(3,(int)ci))continue;const double at=p.clipStart(3,(int)ci);
        const int si=p.sectionAtBeat(at+.01);if(si<0||p.sections[(size_t)si].type==Section::drop)continue;
        std::sort(c.notes.begin(),c.notes.end(),[](const Note& a,const Note& b){return a.start<b.start;});
        std::vector<std::pair<size_t,size_t>> groups;for(size_t i=0;i<c.notes.size();){size_t j=i+1;while(j<c.notes.size()&&c.notes[j].start-c.notes[i].start<1./16.)++j;groups.push_back({i,j});i=j;}
        if((double)groups.size()<3*p.clipLength(3,(int)ci)/4)continue;   // a sparse guitar (fewer than 3 strums a bar) already leaves room
        std::vector<bool> drop(groups.size(),false);size_t dropped=0;
        size_t together=0;for(size_t k=0;k<groups.size()&&dropped*3<groups.size();++k)if(pianoAt(at+c.notes[groups[k].first].start)&&together++%2==1){drop[k]=true;++dropped;}   // every other shared strum, at most a third: the take keeps its own figure
        if(!dropped)continue;
        for(size_t k=0;k<groups.size();++k)if(drop[k])for(size_t i=groups[k].first;i<groups[k].second;++i)c.notes[i].velocity=0;
        c.notes.erase(std::remove_if(c.notes.begin(),c.notes.end(),[](const Note& n){return n.velocity==0;}),c.notes.end());tidyNotes(c,p.clipLength(3,(int)ci));}
}
// The rhythm section: a bass note under E1 goes up an octave (the sub part owns that range); and a breakdown that
// leads into a main part or a drop without a build-up or a stop ends on a one-beat fill (16th snares,
// toms in Afro and Latin) rising into it.
static void rhythmSection(Project& p,Scope scope,int track,int section,int lastClip=-1){
    if((int)p.tracks.size()<17)return;const auto inScope=[&](int ti,int ci){return editableTake(p,scope,track,section,ti,ci,lastClip);};
    [[maybe_unused]] auto onsets=[&](int ti){std::vector<double> out;const auto& t=p.tracks[(size_t)ti];if(t.mute)return out;for(size_t ci=0;ci<t.clips.size();++ci){const double at=p.clipStart(ti,(int)ci);for(const auto& n:t.clips[ci].notes)out.push_back(at+n.start);}std::sort(out.begin(),out.end());return out;};
    {auto& bass=p.tracks[1];
     if(!bass.locked&&bass.instrument!="sample")for(size_t ci=0;ci<bass.clips.size();++ci){auto& c=bass.clips[ci];if(c.locked||c.loopLength>0||!inScope(1,(int)ci))continue;bool moved=false;
        for(auto& n:c.notes)if(n.pitch<28){n.pitch+=12;moved=true;}
        if(moved)tidyNotes(c,p.clipLength(1,(int)ci));}}
    const bool hands=isAfroLatinGenre(p.settings.genre);const int fillTrack=4+(int)(hands?fgd::Lane::tom:fgd::Lane::snare);auto& fill=p.tracks[(size_t)fillTrack];
    if(!fill.locked&&!isElectronicGenre(p.settings.genre))for(size_t si=1;si<p.sections.size()&&si<fill.clips.size()+1;++si){const auto& prev=p.sections[si-1];const auto type=p.sections[si].type;
        if(prev.type!=Section::breakdown||(type!=Section::main&&type!=Section::drop)||prev.dropGap>0||prev.bars<2||fill.freeTimeline)continue;const size_t ci=si-1;
        if(ci>=fill.clips.size()||fill.clips[ci].locked||!inScope(fillTrack,(int)ci))continue;auto& c=fill.clips[ci];const double end=prev.bars*4.;
        const bool busy=std::any_of(c.notes.begin(),c.notes.end(),[&](const Note& n){return n.start>=end-1-1e-6;});if(busy)continue;
        const int pitch=fill.engine.midiNote>=0?fill.engine.midiNote:fgd::defaultNote(fillTrack-4);
        for(int k=0;k<4;++k)c.notes.push_back({end-1+k*.25,.2,pitch+(hands?(k<2?2:0):0),56+k*14,fillTrack-4});
        std::sort(c.notes.begin(),c.notes.end(),[](const Note& a,const Note& b){return a.start<b.start;});}
}
static void bandPasses(Project& p,const std::function<bool(int,int)>& freshIn,const std::function<bool(int,int)>& articulate,const std::array<std::vector<std::vector<Note>>,4>* shaped=nullptr){
    // A clip whose engine output is the one already shaped gets its shaped notes back, untouched.
    static constexpr int bandTracks[4]{2,3,leadTrack,0};std::set<std::pair<int,int>> kept;
    for(int k=0;k<4;++k){const int ti=bandTracks[k];if(ti>=(int)p.tracks.size())continue;auto& clips=p.tracks[(size_t)ti].clips;
        for(size_t ci=0;ci<clips.size();++ci){auto& c=clips[ci];if(c.loopLength>0){kept.insert({ti,(int)ci});continue;}   // a loop clip is rebuilt from its source: left as the engine wrote it
            const auto source=engineOutput(c.notes)^(p.settings.vocalsPresent?0x9e3779b97f4a7c15ULL:0ULL);   // VOCALS PRESENT shapes the take: toggling it reshapes, never restores
            if(shaped&&c.bandSource==source&&ci<(*shaped)[(size_t)k].size()&&engineOutput((*shaped)[(size_t)k][ci])==c.bandResult){c.notes=(*shaped)[(size_t)k][ci];kept.insert({ti,(int)ci});}
            else if(freshIn(ti,(int)ci))c.bandSource=source;}}   // a clip this pass does not rewrite keeps its record
    const auto fresh=[&](int ti,int ci){return !kept.count({ti,ci})&&freshIn(ti,ci);};
    sectionTension(p,fresh);pianoClearsBass(p,fresh);shapeLeadMelody(p,fresh);leadMotifs(p,fresh);bandInterplay(p,fresh);repeatVariations(p,fresh);vocalSpace(p,fresh);ensembleVoicing(p,fresh);leadAnswer(p,fresh);if(!isElectronicGenre(p.settings.genre))trimHoldovers(p,fresh);bassOnChordTones(p,articulate);bandRoles(p,fresh);finalVoicing(p,fresh);
    for(int k=0;k<4;++k){const int ti=bandTracks[k];if(ti>=(int)p.tracks.size()||p.tracks[(size_t)ti].locked)continue;auto& clips=p.tracks[(size_t)ti].clips;
        for(size_t ci=0;ci<clips.size();++ci)if(!clips[ci].locked&&clips[ci].loopLength<=0&&fresh(ti,(int)ci))livePerformance(clips[ci],p,ti,p.clipLength(ti,(int)ci));}
    // only the takes this generation wrote: another clip of the track, or a locked one, keeps its articulation as it is
    for(size_t ti=0;ti<p.tracks.size();++ti)if(p.tracks[ti].articulation>0&&!p.tracks[ti].locked)for(size_t ci=0;ci<p.tracks[ti].clips.size();++ci)if(p.tracks[ti].clips[ci].loopLength<=0&&articulate((int)ti,(int)ci))applyArticulation(p,(int)ti,(int)ci);
    for(int k=0;k<4;++k){const int ti=bandTracks[k];if(ti>=(int)p.tracks.size())continue;auto& clips=p.tracks[(size_t)ti].clips;
        for(size_t ci=0;ci<clips.size();++ci)if(clips[ci].loopLength<=0&&(kept.count({ti,(int)ci})||freshIn(ti,(int)ci)))clips[ci].bandResult=engineOutput(clips[ci].notes);}
    // a note the passes cut at a clip's edge to a few milliseconds is a click, not a note
    for(int ti=0;ti<4&&ti<(int)p.tracks.size();++ti)if(!p.tracks[(size_t)ti].locked)for(auto& c:p.tracks[(size_t)ti].clips)if(!c.locked&&c.loopLength<=0)c.notes.erase(std::remove_if(c.notes.begin(),c.notes.end(),[](const Note& n){return n.length<1./128.;}),c.notes.end());
}
void generate(Project& p,Scope scope,int track,int section,std::uint32_t seed) {
    p.normalise();const int lastClip=scope==Scope::song?-1:partitionHarmonyClips(p,scope,track,section);
    // The band passes below touch only the clips this generation wrote (their notes changed):
    // regenerating one take, or APPLY with the same seed, gives the same result as the song pass.
    std::array<std::vector<std::vector<Note>>,4> written;const int bandTracks[4]{2,3,leadTrack,0};
    for(int k=0;k<4;++k)if(bandTracks[k]<(int)p.tracks.size())for(const auto& c:p.tracks[(size_t)bandTracks[k]].clips)written[(size_t)k].push_back(c.notes);
    if(seed&&(scope==Scope::track||scope==Scope::clip)&&isManualTrack(track)){p.normalise();prepareGenerationFeel(p,scope,track,section,seed,lastClip);}
    generateNotes(p,scope,track,section,seed,lastClip);
    generateVocalChops(p,scope,track,section,seed,lastClip);
    addSectionCrashes(p,scope,track,section);transitionMacros(p,scope,track,section,lastClip);shapeBuildups(p,scope,track,section,lastClip);preDropRisers(p,scope,track,section,lastClip);applyDropGaps(p,scope,track,section,lastClip);
    {const auto fresh=[&](int ti,int ci){for(int k=0;k<4;++k)if(bandTracks[k]==ti){const auto& before=written[(size_t)k];return scope==Scope::song||ci>=(int)before.size()||before[(size_t)ci]!=p.tracks[(size_t)ti].clips[(size_t)ci].notes;}return false;};
     bandPasses(p,fresh,[&](int ti,int ci){return editableTake(p,scope,track,section,ti,ci,lastClip);},&written);}
    rhythmSection(p,scope,track,section,lastClip);   // last: the bass locks to the kick after the band passes have voiced around it
    // the added parts follow the band they are written from: a new bass, piano or lead rewrites them too
    {const bool source=scope==Scope::song||((scope==Scope::track||scope==Scope::clip||scope==Scope::section)&&(track==0||track==1||track==leadTrack));if(source)generateParts(p,Scope::song,0,0);else generateParts(p,scope,track,section,lastClip);}
    // An explicit regeneration (a seed) also rolls the automation of that scope, when asked for.
    if(seed&&p.settings.randomAutomation)randomiseAutomationRange(p,scope,track,section,seed,-1,lastClip>=0?p.clipStart(track,lastClip)+p.clipLength(track,lastClip):-1.);
    if(scope==Scope::song)addElectronicToneAutomation(p);
}
static void generateNotes(Project& p,Scope scope,int track,int section,std::uint32_t seed,int lastClip) {
    p.normalise();
    if(isManualTrack(track)&&(scope==Scope::track||scope==Scope::clip))return;
    if(track<0||track>=(int)p.tracks.size()||section<0||(scope==Scope::clip?section>=(int)p.tracks[(size_t)track].clips.size():section>=(int)p.sections.size())) throw std::out_of_range("Generation selection");
    if(scope==Scope::section){const double start=p.sectionStart(section),end=p.sectionStart(section+1);for(int ti=0;ti<(int)p.tracks.size();++ti){if(!hasGenerator(p.tracks[(size_t)ti])||!p.tracks[(size_t)ti].freeTimeline||p.tracks[(size_t)ti].locked)continue;for(double boundary:{start,end}){const int at=p.clipAtBeat(ti,boundary);if(at>=0&&!p.tracks[(size_t)ti].clips[(size_t)at].locked&&p.clipStart(ti,at)<boundary)p.splitClip(ti,at,boundary);}}}
    sgk::GroovePattern previousPiano;
    if(seed&&!p.tracks[0].locked&&(scope==Scope::song||scope==Scope::section||track==0)){
        const auto& piano=p.tracks[0];
        for(size_t ci=0;ci<piano.clips.size();++ci){const auto& c=piano.clips[ci];const int guide=piano.freeTimeline?p.sectionAtBeat(p.clipStart(0,(int)ci)):(int)ci;
            if(c.locked||(scope==Scope::clip&&((int)ci<section||(int)ci>(lastClip<0?section:lastClip)))||(scope==Scope::section&&guide!=section))continue;
            previousPiano=sgk::GrooveEngine::basePattern(pianoEngineSettings(p,0,c.seed,c.motifSeed));break;}
    }
    if(seed)prepareGenerationFeel(p,scope,track,section,seed,lastClip);
    const auto previousSongSeed=p.settings.seed;
    if(seed&&scope==Scope::song) {
        p.settings.seed=seed;
        // Existing locked harmonic notes cannot follow a new global key/chord cycle.
        const bool harmonyLocked=std::any_of(p.tracks.begin(),p.tracks.end(),[](const Track& t){
            return isHarmonicTrack(t.kind)&& std::any_of(t.clips.begin(),t.clips.end(),[&](const Clip& c){return (t.locked||c.locked)&&!c.notes.empty();});
        });
        if(!harmonyLocked){newSongHarmony(p,seed);const auto feel=sgk::genreRiffFeel(engineGenre(p.settings.genre),seed);if(p.settings.genre==latinHouseGenre){static constexpr int latinFeel[][2]{{7,0},{7,1},{0,1},{8,1}};const auto& f=latinFeel[mix(seed+977U)%4];p.settings.voicing=f[0];p.settings.extension=f[1];}   // montuno: octave triads and close 7ths, not the Afro quartal stacks
        else if(!isElectronicGenre(p.settings.genre)){p.settings.voicing=feel.voicing;p.settings.extension=feel.extension;}}   // a new song, a new piano colour
        auto shape=[](const Project& q){std::vector<int> v;for(const auto& x:q.sections)v.push_back((int)x.type);return v;};const auto userForm=shape(p);   /* the order of section types */
        varySongTempo(p,seed);renewForm(p);varyForm(p,seed);fitFormToTarget(p);
        if(shape(p)!=userForm){   // a form the user wrote stays as written (renewForm kept it)
            // the genre's shape, but never the last song's form again: then the opening break stays, or the drawn form as it is
            Project natural=p;naturalForm(natural);if(shape(natural)==userForm){natural=p;naturalForm(natural,false);}if(shape(natural)!=userForm)p=natural;}if(!harmonyLocked)sectionHarmonies(p,seed);p.refitAudioClips();
    }
    if(scope==Scope::song)partitionHarmonyClips(p,scope,track,section);
    const auto& s=p.settings;
    const auto& profile=genreProfile(s.genre);
    for(int ti=0;ti<trackCount;++ti) {
        auto& t=p.tracks[(size_t)ti];
        if(t.locked||((scope==Scope::track||scope==Scope::clip)&&ti!=track)) continue;
        bool prepared=false;
        auto selectedMotif=mix(seed+(isDrumTrack(ti)?313U:(std::uint32_t)ti*131U));
        for(size_t si=0;si<t.clips.size();++si) {
            const int guide=t.freeTimeline?p.sectionAtBeat(p.clipStart(ti,(int)si)):(int)si;
            if((scope==Scope::section&&guide!=section)||(scope==Scope::clip&&((int)si<section||(int)si>(lastClip<0?section:lastClip))))continue;
            auto& clip=t.clips[si]; if(clip.locked) continue;
            if(seed&&!prepared) {
                // Explicit regeneration leaves fixed presets and seed overrides behind.
                // APPLY (zero seed) continues to render the chosen settings exactly.
                auto& e=t.engine;
                if(isElectronicGenre(s.genre)&&(ti==0||ti==1||ti==3||ti==leadTrack)
                    &&!std::any_of(e.segmentLocked.begin(),e.segmentLocked.end(),[](bool locked){return locked;})){
                    // A main/drop take is more representative than a sparse intro.
                    // Scan only editable targets so a locked/excluded take never
                    // steers a local regeneration, and share the result across sections.
                    size_t reference=si,best=0;
                    for(size_t ci=0;ci<t.clips.size();++ci)if(editableTake(p,scope,track,section,ti,(int)ci,lastClip)){
                        size_t attacks=0;for(const auto& note:t.clips[ci].notes){if(note.start>=16.)break;++attacks;}
                        if(attacks>best){best=attacks;reference=ci;}
                    }
                    const double start=p.clipStart(ti,(int)reference);const int guide=p.sectionAtBeat(start);
                    const double sourceBeats=t.clips[reference].loopLength>0?t.clips[reference].loopLength:p.clipLength(ti,(int)reference);
                    selectedMotif=freshElectronicMotif(p,t.clips[reference],ti,start,sourceBeats,p.sections[(size_t)guide].type,selectedMotif);
                }else if(ti==3&&!isElectronicGenre(s.genre)){
                    size_t reference=si,best=0;
                    for(size_t ci=0;ci<t.clips.size();++ci)if(editableTake(p,scope,track,section,ti,(int)ci,lastClip)){
                        const int guide=t.freeTimeline?p.sectionAtBeat(p.clipStart(ti,(int)ci)):(int)ci;
                        const auto role=p.sections[(size_t)guide].type;if(role!=Section::main&&role!=Section::drop)continue;
                        size_t notes=0;for(const auto& n:t.clips[ci].notes)if(n.start<28)++notes;
                        if(notes>best){best=notes;reference=ci;}
                    }
                    if(best)selectedMotif=freshGuitarMotif(p,(int)reference,selectedMotif,mix(seed+3U*131U+(std::uint32_t)reference*977U));
                }else if(ti==0){auto settings=s;if(scope!=Scope::song&&e.density>=0)settings.density=e.density;selectedMotif=freshPianoMotif(settings,previousPiano,selectedMotif);}
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
            const double period=clip.loopLength,duration=p.clipLength(ti,(int)si);
            clip.loopSource.clear();
            const double beats=period>0?period:duration;const int renderBars=(int)std::ceil(beats/4.); const auto type=p.sections[(size_t)guide].type;if(t.freeTimeline)clip.role=(int)type;
            const double begin=p.clipStart(ti,(int)si)-(period>0?clip.loopPhase:0.);
            const double chordBeats=resolveSectionHarmony(p,guide).chordSteps*.25;
            const double grid=std::max(4.,chordBeats);
            const double origin=p.sectionStart(guide);   /* the chord cycle counts from the section start, as chordAtBeat and the ruler do */
            const double prefix=(ti<4||ti==leadTrack)?std::fmod(std::fmod(begin-origin,grid)+grid,grid):0.;
            const double harmonicBeats=beats+prefix;
            const int harmonicBars=(int)std::ceil(harmonicBeats/4.);
            auto harmonicNotes=[&](const auto& events,auto filter){
                auto notes=notesFrom(events,harmonicBeats,filter);
                if(ti==0||ti==2||ti==leadTrack)tightenPocket(notes,ti);   // the engine's take, played in a pocket
                if(prefix>0){
                    notes.erase(std::remove_if(notes.begin(),notes.end(),[&](const Note& n){return n.start+n.length<=prefix||n.start>=prefix+beats;}),notes.end());
                    for(auto& n:notes){const double end=std::min(n.start+n.length,prefix+beats);n.start=std::max(n.start,prefix);n.length=end-n.start;n.start-=prefix;}
                }
                return notes;
            };
            const auto& e=t.engine;
            clip.feel={takeFeel(e.density,clip.feel[0],s.density),takeFeel(e.swing,clip.feel[1],s.swing),takeFeel(e.humanize,clip.feel[2],s.humanize)};
            // An explicit strings regenerate must write notes. The saved style stays
            // off, so a later song generate does not keep a pad the user never chose.
            const bool requestedPart=seed!=0&&(scope==Scope::track||scope==Scope::clip)&&ti==track;
            const bool stringsRequest=ti==2&&ti==track&&seed!=0&&(!isElectronicGenre(s.genre)||requestedPart)&&scope!=Scope::song;
            const bool keepStrings=ti==2&&s.stringsMode==0&&!clip.notes.empty()&&clip.stringsStyleAtGeneration==0&&!(seed&&scope==Scope::song);
            auto ps=pianoEngineSettings(p,ti,clip.seed,clip.motifSeed,&clip);
            if((stringsRequest||keepStrings)&&p.settings.stringsMode==0)ps.strings=sgk::StringsMode::pad;
            const bool mixing=type==Section::intro||type==Section::outro;
            // Soulful/gospel/afro lifts keep the groove rather than switching to
            // the plugin's generic accelerating snare/piano build pattern.
            const auto engineType=type==Section::buildup&&!profile.rollingBuild?Section::main:type;
            const auto rhythmType=mixing?Section::main:engineType;
            const int rawOffset=(int)std::llround((begin-origin-prefix)/(ps.stepsPerChord*.25));
            const int chordOffset=(rawOffset%ps.progressionLength+ps.progressionLength)%ps.progressionLength;
            for(auto* row:{&ps.progression,&ps.freeChords,&ps.inversions})
                std::rotate(row->begin(),row->begin()+chordOffset,row->begin()+ps.progressionLength);
            if(mixing){ps.fillEveryBars=0;ps.variationEvery=0;}
            const bool manualPart=requestedPart||keepStrings||(scope!=Scope::song&&!clip.notes.empty());
            const bool electronicRendered=isElectronicGenre(s.genre)&&e.pattern<0&&generateElectronicClip(p,clip,ti,begin,beats,type,manualPart,0,guide);
            if(!electronicRendered){
            if(ti<3) {
                ps.section=(sgk::Section)(ti==1?rhythmType:engineType); ps.sectionVariation=clip.seed;
                // The kit turns the corner at every section end; give the harmony the
                // same last-bar fill instead of one every 16 bars counted from the clip.
                if(!mixing&&renderBars>=2){ps.forcedFillBar=harmonicBars-1;ps.forcedFillStyle=-1;}
                auto all=sgk::GrooveEngine::renderArrangement(ps,{{ps.section,harmonicBars,clip.seed,-1}});
                if(ti==0&&s.guitarLead)all=sgk::Comping::comping(all,clip.seed*7919u+1u,ps,harmonicBeats);
                clip.notes=harmonicNotes(all,[ti](const sgk::NoteEvent& e){return ti==0?(e.part==sgk::Part::chords||e.part==sgk::Part::fill):ti==1?e.part==sgk::Part::bass:e.part==sgk::Part::strings;});
                // Answer stabs need a piano rest. A break holds one chord, so they
                // would otherwise stay silent. A pad still follows that harmony.
                if(ti==2&&(!isElectronicGenre(s.genre)||manualPart)&&ps.strings!=sgk::StringsMode::off&&clip.notes.empty()){
                    auto held=ps;held.strings=sgk::StringsMode::pad;
                    auto bed=sgk::GrooveEngine::renderArrangement(held,{{held.section,harmonicBars,clip.seed,-1}});
                    clip.notes=harmonicNotes(bed,[](const sgk::NoteEvent& e){return e.part==sgk::Part::strings;});
                }
            } else if(ti==3) {
                fgg::GuitarSettings gs; gs.pattern=e.pattern>=0&&e.pattern<(int)fgg::guitarPatterns().size()?e.pattern:fgg::randomPatternIndex(); gs.randomGenre=engineGenre(s.genre);
                if(s.genre==(int)Genre::jackinHouse&&e.pattern<0&&!clip.motifSeed){
                    const auto& patterns=fgg::guitarPatterns();
                    for(size_t i=0;i<patterns.size();++i)if(patterns[i].name=="Chicago Chank"){gs.pattern=(int)i;break;}
                }
                gs.riffSeed=e.riffSeed?e.riffSeed:(clip.motifSeed?clip.motifSeed:mix(s.seed+393U));gs.stepsPerChord=ps.stepsPerChord;
                gs.density=ps.riffDensity; gs.swing=ps.swing; gs.humanize=ps.humanize; gs.humanizeSeed=clip.seed;
                gs.tempo=s.bpm; gs.roomForBand=true; gs.numChords=ps.progressionLength; gs.fillEveryBars=e.fillEvery; gs.variationEvery=e.variationEvery; gs.fillStyle=(fgg::FillStyle)(e.fillStyle<0?(int)fgg::FillStyle::mix:std::min(e.fillStyle,fgg::numFillStyles-1));
                gs.velocity=e.velocity;gs.accentGroove=(float)e.accent;gs.strumMs=(float)(e.strum*80);gs.grooveBars=e.grooveBars;gs.segmentOrder=e.segments;gs.letRing=e.letRing;gs.roomForBand=e.roomForBand;gs.pushChanges=e.pushChanges;gs.strumAfterFill=e.afterFill;
                for(size_t l=0;l<12;++l){gs.levels[l]=(float)e.laneLevel[l];gs.laneSwing[l]=(float)e.laneSwing[l];gs.laneShift[l]=(float)e.laneShift[l];gs.octaves[l]=e.laneOctave[l];gs.muted[l]=e.laneMute[l];}
                sgk::GrooveSettings colour;colour.extension=(sgk::Extension)s.extension;colour.section=(sgk::Section)engineType;const auto ext=sgk::GrooveEngine::sectionExtension(colour);
                for(int i=0;i<ps.progressionLength;++i) gs.chords[(size_t)i]=bandChord(ps.keyRoot,(int)ps.scale,ps.progression[(size_t)i],ps.freeChords[(size_t)i],ext);
                if(mixing){gs.fillEveryBars=0;gs.variationEvery=0;}
                auto all=fgg::GrooveEngine::renderArrangement(gs,{{(fgg::Section)engineType,harmonicBars,clip.seed,-1}});
                clip.notes=harmonicNotes(all,[](const fgg::NoteEvent& e){return !e.isController()&&e.lane<fgg::numLanes;});
                // Articulation lanes are carried directly by notesFrom, in linear time.
            } else if(ti==leadTrack) {
                fgl::LeadSettings ls;
                ls.globalOctave=e.globalOctave;ls.key=ps.keyRoot;ls.scale=(int)ps.scale;ls.stepsPerChord=ps.stepsPerChord;
                ls.pattern=e.pattern>=0&&e.pattern<(int)fgl::leadPatterns().size()?e.pattern:fgl::randomPatternIndex();
                ls.randomGenre=engineGenre(s.genre);ls.riffSeed=e.riffSeed?e.riffSeed:(clip.motifSeed?clip.motifSeed:mix(s.seed+2227U));
                ls.density=ps.riffDensity;ls.swing=ps.swing;ls.humanize=ps.humanize;ls.humanizeSeed=clip.seed;
                ls.tempo=s.bpm;ls.numChords=ps.progressionLength;ls.fillEveryBars=mixing?0:e.fillEvery;ls.variationEvery=mixing?0:e.variationEvery;
                ls.fillStyle=(fgl::FillStyle)(e.fillStyle<0?(int)fgl::FillStyle::mix:std::min(e.fillStyle,fgl::numFillStyles-1));
                ls.velocity=e.velocity;ls.accentGroove=(float)e.accent;ls.attackMs=(float)(e.strum*80);ls.grooveBars=e.grooveBars;
                ls.segmentOrder=e.segments;ls.letRing=e.letRing;ls.roomForBand=e.roomForBand;ls.pushChanges=e.pushChanges;ls.resolveAfterFill=e.afterFill;
                for(size_t l=0;l<12;++l){ls.levels[l]=(float)e.laneLevel[l];ls.laneSwing[l]=(float)e.laneSwing[l];ls.laneShift[l]=(float)e.laneShift[l];ls.octaves[l]=e.laneOctave[l];ls.muted[l]=e.laneMute[l];}
                sgk::GrooveSettings colour;colour.extension=(sgk::Extension)s.extension;colour.section=(sgk::Section)engineType;const auto ext=sgk::GrooveEngine::sectionExtension(colour);
                for(int i=0;i<ps.progressionLength;++i){auto chord=bandChord(ps.keyRoot,(int)ps.scale,ps.progression[(size_t)i],ps.freeChords[(size_t)i],ext);ls.chords[(size_t)i]={chord.root,chord.type};}
                auto all=fgl::GrooveEngine::renderArrangement(ls,{{(fgl::Section)engineType,harmonicBars,clip.seed,-1}});
                clip.notes=harmonicNotes(all,[](const fgl::NoteEvent& event){return !event.isController()&&event.lane<fgl::numLanes;});
            } else {
                fgd::DrumSettings ds; ds.pattern=e.pattern>=0&&e.pattern<(int)fgd::drumPatterns().size()?e.pattern:fgd::randomPatternIndex(); ds.randomGenre=drumGenre(s.genre);
                // One kit seed for all drum stems keeps kick, backbeat and hats coherent.
                ds.riffSeed=e.riffSeed?e.riffSeed:(clip.motifSeed?clip.motifSeed:mix(s.seed+313U)); ds.sectionVariation=mix(clip.seed+(std::uint32_t)si*977U);
                ds.density=ps.riffDensity; ds.swing=ps.swing; ds.humanize=ps.humanize; ds.humanizeSeed=e.humanizeSeed?e.humanizeSeed:clip.seed;
                ds.tempo=s.bpm; ds.bassSpace=e.bassSpace; ds.fillEveryBars=e.fillEvery; ds.variationEvery=e.variationEvery; ds.fillStyle=(fgd::FillStyle)(e.fillStyle<0?(int)fgd::FillStyle::mix:std::min(e.fillStyle,fgd::numFillStyles-1));
                if(type==Section::buildup&&!profile.rollingBuild&&ds.fillStyle==fgd::FillStyle::mix&&e.pattern!=fgd::userPatternIndex()){
                    // The generic "mix" also selects snare/clap rolls. Organic
                    // lifts use small kit turnarounds, shared by all stems.
                    const fgd::FillStyle organic[]{fgd::FillStyle::hatLift,fgd::FillStyle::congaRun,fgd::FillStyle::rimShuffle,fgd::FillStyle::percBreak};
                    ds.fillStyle=organic[mix(clip.seed+(unsigned)si*977U)%std::size(organic)];
                }
                ds.velocity=e.velocity;ds.accentGroove=(float)e.accent;ds.genreFeel=e.genreFeel;ds.crashAfterFill=e.afterFill;ds.grooveBars=e.grooveBars;ds.segmentOrder=e.segments;
                ds.laneSwing[(size_t)(ti-4)]=(float)e.laneSwing[0];ds.laneShift[(size_t)(ti-4)]=(float)e.laneShift[0];
                if(e.midiNote>=0)ds.notes[(size_t)(ti-4)]=e.midiNote;
                if(mixing){ds.fillEveryBars=0;ds.variationEvery=0;ds.crashAfterFill=false;}
                auto all=fgd::GrooveEngine::renderArrangement(ds,{{(fgd::Section)rhythmType,renderBars,ds.sectionVariation,-1}});
                clip.notes=notesFrom(all,beats,[ti](const fgd::NoteEvent& e){return e.lane==ti-4;});
            }
            } // Vendor engines are used only when the dedicated generator declines.
            if(isDrumTrack(ti)&&e.pattern<0&&type!=Section::intro&&type!=Section::outro&&type!=Section::breakdown)ornamentDrumTake(clip,ti,beats);
            if(isDrumTrack(ti))for(auto& n:clip.notes){n.lane=ti-4;const int base=e.midiNote>=0?e.midiNote:fgd::defaultNote(ti-4);n.pitch=ti-4==(int)fgd::Lane::conga&&std::abs(n.pitch-(fgd::defaultNote(ti-4)))<=3?base+n.pitch-fgd::defaultNote(ti-4):base;}   /* congas: three drums around the lane note */
            shapeBandRoles(clip,p,ti,guide);
            shapeMixSection(clip,s.genre,ti,type,renderBars);
            if(ti==2&&(!isElectronicGenre(s.genre)||manualPart)&&ps.strings!=sgk::StringsMode::off&&clip.notes.empty()){
                auto held=ps;held.strings=sgk::StringsMode::pad;
                auto bed=sgk::GrooveEngine::renderArrangement(held,{{held.section,harmonicBars,clip.seed,-1}});
                clip.notes=harmonicNotes(bed,[](const sgk::NoteEvent& e){return e.part==sgk::Part::strings;});
                shapeMixSection(clip,s.genre,ti,type,renderBars);
            }
            if(p.settings.grooveEnabled)for(auto& n:clip.notes){const int slot=((int)std::llround(n.start*4)%16+16)%16;n.start=std::max(0.,n.start+p.settings.grooveOffsets[(size_t)slot]);n.velocity=std::clamp((int)std::lround(n.velocity*p.settings.grooveAccents[(size_t)slot]),1,127);}
            if(ti==0||ti==2||ti==3)fitSustainedTensions(clip,p,guide,p.clipStart(ti,(int)si),period);
            if(guide>=0&&guide<(int)p.sections.size()&&p.sections[(size_t)guide].energy>=0){const double v=energyVelocity(p.sections[(size_t)guide].energy);for(auto& n:clip.notes)n.velocity=std::clamp((int)std::lround(n.velocity*v),1,127);}
            if(ti==1&&e.pattern<0&&!isElectronicGenre(s.genre))bassApproach(clip,p,p.clipStart(ti,(int)si),beats);
            if(ti==0&&e.pattern<0){pianoMotif(clip,p,beats);pianoTake(clip,p,beats);}
            if(ti==2&&e.pattern<0&&!isElectronicGenre(s.genre))chordTake(clip,0x51a77e55U,beats,false);   // string pads: each take its own voicing and restrike (no anticipation: a pad holds its chord)
            if(ti==3&&e.pattern<0&&s.genre==latinHouseGenre&&!p.settings.guitarLead)latinMontuno(clip,beats);   // a chosen pattern plays as written
            if(t.engine.humanizeSeed&&(!isDrumTrack(ti)||isElectronicGenre(p.settings.genre)))applyFeelSeed(clip,t.engine.humanizeSeed,takeFeel(t.engine.humanize,clip.feel[2],p.settings.humanize));
            if(ti==2){varyStringsTake(clip);clip.stringsStyleAtGeneration=s.stringsMode;}
            if((ti!=0&&ti!=2&&ti!=3&&ti!=leadTrack)||period>0)livePerformance(clip,p,ti,beats);   // band loops: the passes leave them as written
            tidyNotes(clip,beats);if(ti==leadTrack)monophonicLead(clip);
            if(period>0){clip.loopSource=clip.notes;if(!rebuildClipLoop(clip,duration))throw std::length_error("Generated loop exceeds the note limit");}
        }
    }
}
void applyInstrumentChange(Project& p,int track,bool harmony) {
    p.normalise();
    if(track<0||track>=(int)p.tracks.size()||!hasGenerator(p.tracks[(size_t)track]))return;
    if(p.tracks[(size_t)track].vocalChops){generate(p,Scope::track,track,0,0);return;}
    for(int ti=0;ti<trackCount;++ti){
        if(ti!=track&&!(harmony&&isHarmonicTrack(ti)))continue;
        if(p.tracks[(size_t)ti].locked||p.tracks[(size_t)ti].clips.empty())continue;
        generate(p,Scope::track,ti,0,0);
    }
}
void applyGlobalFeel(Project& p,unsigned parameters){
    if(!(parameters&(densityFeel|swingFeel|humanizeFeel)))return;
    p.normalise();
    for(auto& t:p.tracks){
        if(!hasGenerator(t)||t.locked||std::none_of(t.clips.begin(),t.clips.end(),[](const Clip& c){return !c.locked;}))continue;
        if(parameters&densityFeel)t.engine.density=-1;
        if(parameters&swingFeel)t.engine.swing=-1;
        if(parameters&humanizeFeel)t.engine.humanize=-1;
        for(auto& c:t.clips)if(!c.locked){if(parameters&densityFeel)c.feel[0]=-1;if(parameters&swingFeel)c.feel[1]=-1;if(parameters&humanizeFeel)c.feel[2]=-1;}
    }
    // Apply the existing takes once; no new form, harmony, tempo or automation.
    std::array<std::vector<std::vector<Note>>,4> shaped;{const int bandTracks[4]{2,3,leadTrack,0};for(int k=0;k<4;++k)if(bandTracks[k]<(int)p.tracks.size())for(const auto& c:p.tracks[(size_t)bandTracks[k]].clips)shaped[(size_t)k].push_back(c.notes);}
    generateNotes(p,Scope::song,0,0,0);generateVocalChops(p,Scope::song,0,0,0);addSectionCrashes(p,Scope::song,0,0);transitionMacros(p,Scope::song,0,0);shapeBuildups(p,Scope::song,0,0);preDropRisers(p,Scope::song,0,0);applyDropGaps(p,Scope::song,0,0);bandPasses(p,[](int,int){return true;},[](int,int){return true;},&shaped);rhythmSection(p,Scope::song,0,0);generateParts(p,Scope::song,0,0);
}
bool regenerateClip(Project& p,int track,int clip,std::uint32_t seed) {
    if(track<0||track>=(int)p.tracks.size()||!hasGenerator(p.tracks[(size_t)track]))return false;
    auto& t=p.tracks[(size_t)track];
    if(t.locked||clip<0||clip>=(int)t.clips.size()||t.clips[(size_t)clip].locked)return false;
    prepareGenerationFeel(p,Scope::clip,track,clip,seed);
    auto& c=t.clips[(size_t)clip];
    // Keep the selected motif and explicit pattern while varying the performance
    // and the genre-based feel shown by the main controls.
    auto next=mix(seed^(c.seed*2654435761U)^((std::uint32_t)clip*977U));
    if(next==c.seed||next<=1)next=c.seed+2;
    const double selectionEnd=p.clipStart(track,clip)+p.clipLength(track,clip);
    c.seed=next;
    // The bass engine plays its line from the take: a new seed alone changed only the humanisation,
    // so REGENERATE on a bass clip gave the same notes. It also develops the figure (same groove and
    // anchors, other tones and pickups), unless a riff is pinned in the panel.
    if(track==1&&t.engine.pattern<0&&!t.engine.riffSeed)c.motifSeed=mix(next^0x51a7c0deU)|1U;
    // the lead's hook comes from the take's motif: a regenerated lead clip asks for a new idea, so it gets its own
    if(track==leadTrack&&t.engine.pattern<0&&!t.engine.riffSeed)c.motifSeed=mix(next^0x1ead5eedU)|1U;
    generate(p,Scope::clip,track,clip,0);
    if(p.settings.randomAutomation)randomiseAutomationRange(p,Scope::clip,track,clip,next,-1,selectionEnd);
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
// Movement for a plugin parameter over [a,b]: every roll draws the figure itself (LFO, ramp,
// steps, saw, random walk, swell, gate), then its rate, depth, centre and direction, so two
// rolls of the same lane differ in kind and not only in amount.
void pluginFigure(std::vector<VolumePoint>& add,AutomationDice& dice,double a,double b,double neutral,double tick){
    const double depth=dice.between(.12,.4),centre=std::clamp(neutral+dice.between(-.12,.12),depth,1.-depth);
    const auto lo=centre-depth,hi=centre+depth;const bool up=dice.chance(.5);
    static constexpr double rates[]{2,4,8,16};const double rate=rates[(int)(dice.next()*4)%4];
    const auto value=[](double v){return std::clamp(v,0.,1.);};
    switch((int)(dice.next()*7)%7){
    case 0:{bool high=up;const double bend=dice.between(-.5,.5);for(double x=a;x<b-1e-9;x+=rate*.5){add.push_back({x,value(high?hi:lo),high?bend:-bend});high=!high;}break;}            // triangle / sine-like LFO
    case 1:add.push_back({a,value(up?lo:hi),dice.between(-.7,.7)});add.push_back({b-tick,value(up?hi:lo),0});break;                                                       // one ramp across the part
    case 2:{double last=-1;for(double x=a;x<b-1e-9;x+=rate){double v=dice.between(lo,hi);if(std::abs(v-last)<depth*.3)v=v>centre?lo:hi;if(x>a)add.push_back({x-tick,value(last),0});add.push_back({x,value(v),0});last=v;}break;} // sample and hold
    case 3:for(double x=a;x<b-1e-9;x+=rate){add.push_back({x,value(up?lo:hi),dice.between(-.3,.6)});add.push_back({std::min(b,x+rate)-tick,value(up?hi:lo),0});}break;       // saw
    case 4:{double v=centre;for(double x=a;x<b-1e-9;x+=rate*.5){v=std::clamp(v+dice.between(-depth,depth),lo,hi);add.push_back({x,value(v),dice.between(-.4,.4)});}break;}        // random walk
    case 5:{const double peak=a+(b-a)*dice.between(.3,.8);add.push_back({a,value(up?lo:hi),dice.between(.2,.7)});add.push_back({peak,value(up?hi:lo),dice.between(-.7,-.2)});add.push_back({b-tick,value(up?lo:hi),0});break;} // swell
    default:{const double step=std::min(rate,4.)*.5,duty=dice.between(.3,.7);for(double x=a;x<b-1e-9;x+=step){add.push_back({x,value(hi),0});add.push_back({x+step*duty-tick,value(hi),0});add.push_back({x+step*duty,value(lo),0});add.push_back({std::min(b,x+step)-tick,value(lo),0});}break;} // gate
    }
    add.push_back({b,neutral,0});
}
}
static void randomiseAutomationRange(Project& p,Scope scope,int track,int index,std::uint32_t seed,int onlyLane,double clipEnd){
    p.normalise();
    if(track<0||track>=(int)p.tracks.size())return;
    double from=0,to=p.length();
    if(scope==Scope::section){if(index<0||index>=(int)p.sections.size())return;from=p.sectionStart(index);to=p.sectionStart(index+1);}
    if(scope==Scope::clip){const auto& clips=p.tracks[(size_t)track].clips;if(index<0||index>=(int)clips.size())return;from=p.clipStart(track,index);to=clipEnd>=0?clipEnd:from+p.clipLength(track,index);}
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
            if(kind>=2&&t.parameters[(size_t)(kind-2)].generatedTone){
                if(onlyLane<0)continue; // an owned envelope is rebuilt from the current sections below
                t.parameters[(size_t)(kind-2)].generatedTone=false;
            }
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
                    else{const double low=dice.between(.08,.3),high=dice.between(.82,1.);const int contour=(int)(dice.next()*3)%3;
                        if(contour==2&&length>=8){const int steps=length>=16?8:4;for(int k=0;k<steps;++k){const double x=a+length*k/steps,v=low+(high-low)*k/(steps-1.);if(k)add.push_back({x-tick,low+(high-low)*(k-1)/(steps-1.),0});add.push_back({x,v,0});}add.push_back({b-tick,high,0});}
                        else{add.push_back({a,low,contour==0?dice.between(-.2,.2):dice.between(.4,.8)});add.push_back({b-tick,high,0});}
                        add.push_back({b,neutral,0});}             // the filter sweep: straight, exponential or stepped
                    break;
                case Section::breakdown:
                    if(kind==0&&(ti==0||ti==2)){add.push_back({a,dice.between(.88,.95),.3});add.push_back({a+length*.5,dice.between(1.02,1.08),-.3});add.push_back({b-tick,dice.between(.94,1.),0});add.push_back({b,1,0});}
                    else if(kind==1&&(asked||dice.chance(.7))){const double width=dice.between(.08,.22);for(double x=a;x<b-1e-9;x+=8){const bool left=std::fmod((x-a)/8,2.)<.5;add.push_back({x,std::clamp(neutral+(left?-width:width),0.,1.),dice.between(-.3,.3)});}add.push_back({b,neutral,0});}
                    else if(kind>=2&&(asked||dice.chance(.8)))pluginFigure(add,dice,a,b,neutral,tick);
                    break;
                case Section::main:
                case Section::drop:
                    if(kind==0&&(asked||dice.chance(.5))){const double depth=dice.between(.04,.09);for(double x=a;x<b-1e-9;x+=8){add.push_back({x,1,.4});if(x+4<b)add.push_back({x+4,1-depth,-.4});}add.push_back({b,1,0});}
                    else if(kind==1&&(asked||dice.chance(.55))){const double width=dice.between(.06,.18),bar=dice.chance(.5)?4.:8.;for(double x=a;x<b-1e-9;x+=bar){const bool left=std::fmod((x-a)/bar,2.)<.5;add.push_back({x,std::clamp(neutral+(left?-width:width),0.,1.),dice.between(-.4,.4)});}add.push_back({b,neutral,0});}
                    else if(kind>=2&&(asked||dice.chance(.65)))pluginFigure(add,dice,a,b,neutral,tick);
                    break;
                default: if(kind>=2&&asked)pluginFigure(add,dice,a,b,neutral,tick);break;   // intro and outro: no fades; a plugin lane moves only when its own dice is rolled
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
void randomiseAutomation(Project& p,Scope scope,int track,int index,std::uint32_t seed,int onlyLane){randomiseAutomationRange(p,scope,track,index,seed,onlyLane,-1.);}
// Articulation profiles for hosted instruments (library-agnostic conventions):
// 1 the mod wheel (CC1, the modulation lane) follows the part's dynamics half-bar by half-bar;
// 2 keyswitches: C0 before a run of long notes, C#0 before short ones (chank, staccato);
// 3 legato: in a single line each note overlaps the next by a 32nd when the gap is small.
void applyArticulation(Project& p,int track,int ci){
    if(track<0||track>=(int)p.tracks.size())return;auto& t=p.tracks[(size_t)track];if(t.instrument!="plugin"||ci<0||ci>=(int)t.clips.size())return;
    auto& c=t.clips[(size_t)ci];if(t.locked||c.locked||c.notes.empty())return;const double length=p.clipLength(track,ci);
    constexpr int keyLong=24,keyShort=25;constexpr double tick=1./32.;
    c.notes.erase(std::remove_if(c.notes.begin(),c.notes.end(),[](const Note& n){return n.articulationKey;}),c.notes.end());
    if(t.articulation==1&&!c.expression[1].locked){auto& lane=c.expression[1];lane.enabled=true;lane.points.clear();   // a CC1 curve you locked is kept
        for(double at=0;at<length;at+=2){double sum=0;int count=0;for(const auto& n:c.notes)if(n.start>=at&&n.start<at+2){sum+=n.velocity;++count;}
            const double value=count?std::clamp(sum/count/127.*1.15,.2,1.):(lane.points.empty()?.5:lane.points.back().value);lane.points.push_back({at,value,0});}}
    else if(t.articulation==2){std::vector<Note> keys;int state=-1;
        for(const auto& n:c.notes){const int want=n.length<=.3?keyShort:keyLong;if(want==state)continue;state=want;Note k=n;k.pitch=want;k.velocity=100;k.start=std::max(0.,n.start-tick);k.length=tick;k.articulationKey=true;keys.push_back(k);}
        c.notes.insert(c.notes.end(),keys.begin(),keys.end());}
    else if(t.articulation==3){std::sort(c.notes.begin(),c.notes.end(),[](const Note& a,const Note& b){return a.start<b.start;});
        for(size_t i=0;i+1<c.notes.size();++i){auto& n=c.notes[i];const auto& next=c.notes[i+1];if(next.start-n.start<1./16.)continue;   // chords stay as they are
            const double gap=next.start-(n.start+n.length);if(gap>=0&&gap<.5)n.length=next.start-n.start+tick;}}
    tidyNotes(c,length);
}
bool setTrackArticulation(Project& p,int track,int profile){
    if(track<0||track>=(int)p.tracks.size()||profile<0||profile>3)return false;
    auto& t=p.tracks[(size_t)track];
    if(t.locked||t.instrument!="plugin"||t.articulation==profile)return false;
    const int before=t.articulation;t.articulation=profile;
    for(size_t ci=0;ci<t.clips.size();++ci){auto& c=t.clips[ci];if(c.locked)continue;
        if(before==1&&profile!=1&&!c.expression[1].locked)c.expression[1]={};
        applyArticulation(p,track,(int)ci);
    }
    return true;
}
void tidyNotes(Clip& c,double beats) {
    constexpr double tick=1./960.;
    c.notes.erase(std::remove_if(c.notes.begin(),c.notes.end(),[beats,tick](const Note& n){return !std::isfinite(n.start)||!std::isfinite(n.length)||n.start>beats-tick||n.length<=0||n.pitch<0||n.pitch>127||n.velocity<1||n.velocity>127;}),c.notes.end());
    for(auto& n:c.notes) {n.start=std::max(0.0,n.start);n.length=std::min(std::max(tick,n.length),beats-n.start);n.lane=std::clamp(n.lane,0,12);}
    std::stable_sort(c.notes.begin(),c.notes.end(),byStartPitch);
    // One MIDI channel per instrument: coalesce simultaneous guitar-string hits
    // and close retriggered pitches so a later note-off cannot cut the next note.
    std::vector<Note> clean;clean.reserve(c.notes.size());std::array<int,128> last;last.fill(-1);
    for(auto n:c.notes){int index=last[(size_t)n.pitch];if(index>=0){auto& prev=clean[(size_t)index];if(n.start-prev.start<tick){prev.length=std::max(prev.length,n.start+n.length-prev.start);prev.velocity=std::max(prev.velocity,n.velocity);prev.articulationKey=prev.articulationKey&&n.articulationKey;continue;}if(prev.start+prev.length>n.start)prev.length=n.start-prev.start;}
        last[(size_t)n.pitch]=(int)clean.size();clean.push_back(n);}
    c.notes=std::move(clean);
}
void transpose(Clip& c,int semitones) {if(semitones==0)return;for(const auto& n:c.notes){const int pitch=n.pitch+semitones;if(pitch<0||pitch>127)return;}for(auto& n:c.notes)n.pitch+=semitones;}
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
        int expressionSent=-1;size_t expressionIndex=0;
        auto expression=[&](double beat,int value){if(value==expressionSent)return;
            if(expressionSent>=0&&out[expressionIndex].beat==beat)out[expressionIndex].velocity=value;
            else{expressionIndex=out.size();out.push_back({beat,(int)ti,11,value,0,11});}expressionSent=value;};
        if(onlyTrack>=0&&(int)ti!=onlyTrack)continue;
        const auto& t=p.tracks[ti];
        double occupied=0,sectionBegin=0;
        for(size_t si=0;si<t.clips.size();++si) {
            // Running section start instead of the O(n) clipStart for section-aligned clips.
            const double begin=t.freeTimeline?t.clips[si].position:sectionBegin,end=p.clipLength((int)ti,(int)si),from=std::max(0.,occupied-begin);
            if(si<p.sections.size())sectionBegin+=p.sections[si].bars*4.0;
            occupied=std::max(occupied,begin+end);if(from>=end)continue;
            if(ti==0&&t.engine.pedalCC&&!t.clips[si].notes.empty()){
                const double chord=resolveSectionHarmony(p,t.freeTimeline?p.sectionAtBeat(begin):(int)si).chordSteps*.25;
                for(double at=0;at<end;at+=chord){if(at<from)continue;out.push_back({begin+at,(int)ti,64,0,0,64});out.push_back({begin+at+.02,(int)ti,64,127,0,64});}
                const Section role=t.freeTimeline&&t.clips[si].role>=0?(Section)t.clips[si].role:p.sections.empty()?Section::main:p.sections[(size_t)p.sectionAtBeat(begin)].type;
                auto level=[&](double at){return role==Section::buildup?std::clamp((int)(60+67*at/end),0,127):127;};
                if(from>0){out.push_back({begin+from,(int)ti,64,0,0,64});expression(begin+from,level(from));}
                for(double at=0;at<end;at+=.25){if(at<from)continue;expression(begin+at,level(at));}
                out.push_back({begin+end,(int)ti,64,0,0,64});expression(begin+end,127);
            }
            const auto& controlClip=t.clips[si];for(int lane=0;lane<3;++lane){const auto& curve=controlClip.expression[(size_t)lane];if(!curve.active())continue;const int controller=lane==0?64:lane==1?1:128;int previous=-1;for(double at=from,step=std::max(1./64.,(end-from)/16384.);at<end;at+=step){const int value=lane==0?(curve.valueAt(at)>=.5?127:0):(int)std::lround(curve.valueAt(at)*(lane==2?16383:127));if(value!=previous){out.push_back({begin+at,(int)ti,controller,value,0,controller});previous=value;}}for(const auto& point:curve.points)if(point.beat>=from&&point.beat<end){const int value=lane==0?(point.value>=.5?127:0):(int)std::lround(point.value*(lane==2?16383:127));out.push_back({begin+point.beat,(int)ti,controller,value,0,controller});}out.push_back({begin+end,(int)ti,controller,lane==2?8192:0,0,controller});}
            for(const auto& n:t.clips[si].notes) {
                if(n.start>=end||n.start+n.length<=from)continue;
                TimedEvent on{begin+std::max(from,n.start),(int)ti,n.pitch,n.velocity,n.lane};
                const auto& clip=t.clips[si];if(clip.audioOffset>=0){const double secondsPerBeat=clip.audioDuration/std::max(1e-9,end);on.audioOffset=clip.audioOffset+std::max(from,n.start)*secondsPerBeat;on.audioBeats=std::min(end,n.start+n.length)-std::max(from,n.start);on.audioDuration=on.audioBeats*secondsPerBeat;}
                on.reverse=n.reverse;if(n.reverse&&on.audioOffset<0){on.audioOffset=0;on.audioDuration=n.length*60./p.settings.bpm;on.audioBeats=n.length;}on.fadeIn=t.clips[si].clipFadeIn;on.fadeOut=t.clips[si].clipFadeOut;out.push_back(on);
                out.push_back({begin+std::min(end,n.start+n.length),(int)ti,n.pitch,0,n.lane});
            }
        }
    }
    // The bass locks to the kick as it is played and exported (the notes in the project stay as written, so the
    // band generated around them is unchanged): a bass attack 0-25 ms after a kick is played with it, not as a
    // flam in the low end; the note keeps its end.
    // A muted kick pulls nothing; the bass's own schedule (one track rebuilt after an edit) reads the kicks too.
    if((int)p.tracks.size()>4&&!p.tracks[1].mute&&!p.tracks[4].mute&&(onlyTrack<0||onlyTrack==1)){std::vector<double> kicks;
        if(onlyTrack<0){for(const auto& e:out)if(e.track==4&&e.velocity>0&&e.controller<0)kicks.push_back(e.beat);}
        else for(const auto& e:events(p,4))if(e.track==4&&e.velocity>0&&e.controller<0)kicks.push_back(e.beat);
        std::sort(kicks.begin(),kicks.end());
        if(!kicks.empty())for(auto& e:out)if(e.track==1&&e.velocity>0&&e.controller<0&&e.audioOffset<0){auto it=std::upper_bound(kicks.begin(),kicks.end(),e.beat);if(it!=kicks.begin()){const double k=*(it-1);
            const double window=.025*p.bpmAt(k)/60.;   /* 25 ms at the tempo where the kick plays (MASTER TEMPO lane) */
            if(e.beat-k>1e-6&&e.beat-k<=window)e.beat=k;}}}
    std::stable_sort(out.begin(),out.end(),[](const TimedEvent& a,const TimedEvent& b){if(a.beat!=b.beat)return a.beat<b.beat;if((a.controller>=0)!=(b.controller>=0))return a.controller>=0;return a.velocity<b.velocity;});return out;
}
}
