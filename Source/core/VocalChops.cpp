// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Project.h"
#include "ElectronicStyles.h"
#include "../../ThirdParty/fasegroovepiano/Source/engine/Theory.h"
#include "../../ThirdParty/fasegroovepiano/Source/engine/GrooveEngine.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <stdexcept>
namespace fga {
namespace {
std::uint32_t hash(std::uint32_t x){x^=x>>16;x*=0x7feb352dU;x^=x>>15;x*=0x846ca68bU;return x^(x>>16);}
}
SamplerSettings vocalSamplerSettings(){
    SamplerSettings s;s.mode=2;s.polyphony=1;s.sliceBase=60;s.sliceCount=8;s.release=.04;
    for(int i=0;i<8;++i)s.slices[(size_t)i]=i/8.;return s;
}
int Project::addVocalChopsTrack(){
    const int index=addMidiTrack();if(index<0)return index;auto& t=tracks[(size_t)index];
    t.vocalChops=true;t.name="Vocal chops";int suffix=2;
    while(std::any_of(tracks.begin(),tracks.begin()+index,[&](const Track& other){return other.name==t.name;}))t.name="Vocal chops "+std::to_string(suffix++);
    t.freeTimeline=false;t.clips.assign(sections.size(),{});t.sampler=vocalSamplerSettings();t.gain=.45;
    t.engine.density=.25;t.engine.gate=.5;t.engine.humanize=.1;t.engine.swing=-1;
    generateVocalChops(*this,Scope::track,index,0,settings.seed);return index;
}
void generateVocalChops(Project& p,Scope scope,int track,int section,std::uint32_t seed){generateVocalChops(p,scope,track,section,seed,-1);}
void generateVocalChops(Project& p,Scope scope,int track,int section,std::uint32_t seed,int lastClip){
    if(std::none_of(p.tracks.begin(),p.tracks.end(),[](const Track& t){return t.vocalChops;}))return;
    p.normalise();
    for(int ti=trackCount;ti<(int)p.tracks.size();++ti){
        if(!p.tracks[(size_t)ti].vocalChops||p.tracks[(size_t)ti].locked||((scope==Scope::track||scope==Scope::clip)&&ti!=track))continue;
        // splitClip replaces the Project; reacquire every reference after each split.
        if(scope==Scope::section&&p.tracks[(size_t)ti].freeTimeline){for(double at:{p.sectionStart(section),p.sectionStart(section+1)}){const int ci=p.clipAtBeat(ti,at);if(ci>=0&&!p.tracks[(size_t)ti].clips[(size_t)ci].locked&&p.clipStart(ti,ci)<at)p.splitClip(ti,ci,at);}}
        auto& t=p.tracks[(size_t)ti];
        for(int ci=0;ci<(int)t.clips.size();++ci){auto& c=t.clips[(size_t)ci];const int guide=t.freeTimeline?p.sectionAtBeat(p.clipStart(ti,ci)):ci;
            if(c.locked||(scope==Scope::clip&&(ci<section||ci>(lastClip<0?section:lastClip)))||(scope==Scope::section&&guide!=section))continue;
            if(seed)c.seed=hash(seed+ti*131U+ci*977U);else if(c.seed<=1)c.seed=hash(p.settings.seed+ti*131U+ci*977U);
            c.feel={takeFeel(t.engine.density,c.feel[0],p.settings.density),takeFeel(t.engine.swing,c.feel[1],p.settings.swing),takeFeel(t.engine.humanize,c.feel[2],p.settings.humanize)};
            const double duration=p.clipLength(ti,ci),period=c.loopLength,beats=period>0?period:duration,density=takeFeel(t.engine.density,c.feel[0],p.settings.density);
            const auto role=p.sections[(size_t)guide].type;if(t.freeTimeline)c.role=(int)role;
            c.notes.clear();c.loopSource.clear();
            if(density<=0)continue;
            // Jacking-house chops: a one-bar syncopated hook played call / echo / call / answer
            // over four bars, with a same-slice stutter ("ja-ja-jack") closing the phrase and a
            // 16th roll at the end of build-ups. Repetition is the point; variation lives in bar 4.
            static constexpr double cells[][4]{{.75,1.5,2.75,-1},{.5,1.75,2.5,3.25},{.25,1.,2.75,3.5},{1.5,2.25,3.75,-1},{.75,2.,2.75,3.5},{.5,1.25,3.,3.75}};
            struct Hit{double at;int slot;int velocity;};std::vector<Hit> hits;
            const bool sparse=role==Section::intro||role==Section::outro||role==Section::breakdown;
            const int phrases=(int)std::ceil(beats/16.);
            for(int phrase=0;phrase<phrases;++phrase){const auto draw=hash(c.seed+phrase*313U);const double base=phrase*16.;
                const auto& cell=cells[draw%6];const int hook=(int)(draw>>8),second=(int)(draw>>12)+1,answer=(int)(draw>>16)+3;
                if(sparse){if(phrase%2==0)continue;hits.push_back({base+15.5,answer,96});if(density>.5)hits.push_back({base+15.75,answer,84});continue;}
                const int perBar=std::clamp(1+(int)std::lround(density*4),1,4);
                for(int bar=0;bar<3;++bar){
                    // Bar 2 is the echo: only the tail of the hook, softer.
                    const bool echo=bar==1;int placed=0;
                    for(int k=0;k<4&&placed<perBar;++k){if(cell[k]<0)continue;if(echo&&placed>=std::max(1,perBar-2))break;
                        const bool accent=std::fmod(cell[k],1.)==.75||std::fmod(cell[k],1.)==.25;
                        hits.push_back({base+bar*4+cell[k],placed%2?second:hook,(echo?82:accent?112:98)});++placed;}
                }
                const bool roll=role==Section::buildup&&(phrase==phrases-1);
                if(roll){const double from=density>.2?2.:3.;const int steps=(int)std::lround((4.-from)*4);for(int k=0;k<steps;++k)hits.push_back({base+12+from+k*.25,hook,70+50*k/std::max(1,steps-1)});continue;}
                // Bar 4: answer slice, then a straight 8th same-slice stutter into the next phrase.
                hits.push_back({base+12.75,answer,108});
                const int stutter=std::clamp(2+(int)std::lround(density*2),2,4);
                for(int k=0;k<stutter;++k)hits.push_back({base+16-(stutter-k)*.5,hook,88+20*k/std::max(1,stutter-1)});
            }
            std::vector<double> onsets;std::vector<Hit> kept;
            const double swing=takeFeel(t.engine.swing,c.feel[1],p.settings.swing),human=takeFeel(t.engine.humanize,c.feel[2],p.settings.humanize);
            for(const auto& h:hits){auto r=hash(c.seed+(std::uint32_t)std::lround(h.at*16)*197U);double at=h.at;
                if(((int)std::lround(at*4)&1)!=0)at+=swing*.25;at+=((int)(r%201)-100)*human*.0002;
                if(at<0||at>=beats)continue;onsets.push_back(at);kept.push_back({at,h.slot,h.velocity});}
            for(size_t n=0;n<kept.size();++n){
                const double at=kept[n].at;double length=std::min(beats-at,std::clamp(t.engine.gate*.5,.0625,.75));
                for(size_t m=0;m<kept.size();++m)if(kept[m].at>at)length=std::min(length,kept[m].at-at);
                const double wrapped=period>0&&c.loopPhase>0?std::fmod(at-c.loopPhase+period*2,period):at;
                if(period>0)length=std::min(length,period-wrapped);
                if(length<=0)continue;
                const auto harmony=resolveSectionHarmony(p,guide);const double cycle=harmony.chordCount*harmony.chordSteps*.25;
                const double absolute=p.clipStart(ti,ci)+wrapped;
                const auto chord=chordAtBeat(harmony,std::fmod(std::fmod(absolute,cycle)+cycle,cycle));
                const unsigned slot=(unsigned)std::max(0,kept[n].slot);int pitch=t.sampleRoot;
                if(t.instrument=="internal"){const int intervals[]{0,chord.third,chord.fifth,12};pitch=60+chord.rootPc+intervals[slot%4];}
                else if(t.sampler.mode==2&&t.sampler.sliceCount>0)pitch=t.sampler.sliceBase+(int)(slot%(unsigned)t.sampler.sliceCount);
                c.notes.push_back({at,length,pitch,std::clamp(kept[n].velocity*t.engine.velocity/100,1,127),0});
            }
            std::sort(c.notes.begin(),c.notes.end(),[](const Note& x,const Note& y){return x.start<y.start;});
            tidyNotes(c,beats);if(period>0){c.loopSource=c.notes;if(!rebuildClipLoop(c,duration))throw std::length_error("Generated vocal loop exceeds the note limit");}
        }
    }
}
void remapVocalChops(Project& p,int track,const SamplerSettings& previous,bool force){
    if(track<trackCount||track>=(int)p.tracks.size())return;auto& t=p.tracks[(size_t)track];if(!t.vocalChops||t.locked)return;
    const auto& s=t.sampler;if(!force&&s.mode==previous.mode&&s.sliceCount==previous.sliceCount&&s.sliceBase==previous.sliceBase)return;
    for(auto& c:t.clips){if(c.locked)continue;for(auto* notes:{&c.notes,&c.loopSource})for(auto& n:*notes){const int old=previous.mode==2?n.pitch-previous.sliceBase:0;n.pitch=s.mode==2&&s.sliceCount>0?s.sliceBase+((old%s.sliceCount)+s.sliceCount)%s.sliceCount:t.sampleRoot;}}
}
}
