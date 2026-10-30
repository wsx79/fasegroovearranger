// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Project.h"
#include "../../ThirdParty/fasegroovepiano/Source/engine/Theory.h"
#include <cmath>
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
void generateVocalChops(Project& p,Scope scope,int track,int section,std::uint32_t seed){
    if(std::none_of(p.tracks.begin(),p.tracks.end(),[](const Track& t){return t.vocalChops;}))return;
    p.normalise();
    for(int ti=trackCount;ti<(int)p.tracks.size();++ti){
        if(!p.tracks[(size_t)ti].vocalChops||p.tracks[(size_t)ti].locked||((scope==Scope::track||scope==Scope::clip)&&ti!=track))continue;
        // splitClip replaces the Project; reacquire every reference after each split.
        if(scope==Scope::section&&p.tracks[(size_t)ti].freeTimeline){for(double at:{p.sectionStart(section),p.sectionStart(section+1)}){const int ci=p.clipAtBeat(ti,at);if(ci>=0&&!p.tracks[(size_t)ti].clips[(size_t)ci].locked&&p.clipStart(ti,ci)<at)p.splitClip(ti,ci,at);}}
        auto& t=p.tracks[(size_t)ti];
        for(int ci=0;ci<(int)t.clips.size();++ci){auto& c=t.clips[(size_t)ci];const int guide=t.freeTimeline?p.sectionAtBeat(p.clipStart(ti,ci)):ci;
            if(c.locked||(scope==Scope::clip&&ci!=section)||(scope==Scope::section&&guide!=section))continue;
            if(seed)c.seed=hash(seed+ti*131U+ci*977U);else if(c.seed<=1)c.seed=hash(p.settings.seed+ti*131U+ci*977U);
            const double beats=p.clipLength(ti,ci),density=t.engine.density>=0?t.engine.density:.25;
            const auto role=p.sections[(size_t)guide].type;if(t.freeTimeline)c.role=(int)role;
            c.notes.clear();c.loopSource.clear();c.loopLength=0;
            if(density<=0)continue;
            // Two to three short calls per four bars at the default density.
            static constexpr double figures[][6]{{1.5,6.75,10.5,14.75,4.5,12.75},{2.75,7.5,9.5,14.5,5.75,12.5},{.75,5.5,11.5,15.25,7.75,13.5},{3.5,6.5,10.75,14.75,1.5,8.5}};
            for(int phrase=0;phrase*16.<beats;++phrase){const auto draw=hash(c.seed+phrase*313U);const bool mixing=role==Section::intro||role==Section::outro;
                if((mixing||role==Section::breakdown)&&(phrase%2==0))continue;
                int count=1+(int)std::lround(density*5);if(mixing||role==Section::breakdown)count=1;
                const auto chord=sgk::buildChord(p.settings.key,p.settings.scale,p.settings.degrees[(size_t)((int)((p.clipStart(ti,ci)+phrase*16)/(p.settings.chordSteps*.25))%p.settings.chordCount)],(sgk::Extension)p.settings.extension);
                for(int n=0;n<count;++n){auto r=hash(draw+n*197U);double at=phrase*16.+figures[draw%4][n];
                    const double swing=t.engine.swing<0?p.settings.swing:t.engine.swing;at+=swing*.16;
                    const double human=t.engine.humanize<0?p.settings.humanize:t.engine.humanize;at+=((int)(r%201)-100)*human*.0002;
                    if(at>=beats)continue;
                    const double length=std::min(beats-at,std::clamp(t.engine.gate*.5,.0625,.75));
                    int pitch=t.sampleRoot;
                    if(t.instrument=="internal"){const int intervals[]{0,chord.third,7,12};pitch=60+chord.rootPc+intervals[(r>>8)%4];}
                    else if(t.sampler.mode==2&&t.sampler.sliceCount>0)pitch=t.sampler.sliceBase+(int)((r>>8)%(unsigned)t.sampler.sliceCount);
                    c.notes.push_back({at,length,pitch,std::clamp((88+(int)((r>>16)%25))*t.engine.velocity/100,1,127),0});
                }
            }
            tidyNotes(c,beats);
        }
    }
}
void remapVocalChops(Project& p,int track,const SamplerSettings& previous,bool force){
    if(track<trackCount||track>=(int)p.tracks.size())return;auto& t=p.tracks[(size_t)track];if(!t.vocalChops||t.locked)return;
    const auto& s=t.sampler;if(!force&&s.mode==previous.mode&&s.sliceCount==previous.sliceCount&&s.sliceBase==previous.sliceBase)return;
    for(auto& c:t.clips){if(c.locked)continue;for(auto* notes:{&c.notes,&c.loopSource})for(auto& n:*notes){const int old=previous.mode==2?n.pitch-previous.sliceBase:0;n.pitch=s.mode==2&&s.sliceCount>0?s.sliceBase+((old%s.sliceCount)+s.sliceCount)%s.sliceCount:t.sampleRoot;}}
}
}
