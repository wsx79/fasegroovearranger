// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
namespace fga {
constexpr double maxSampleSeconds=1800.;
constexpr std::int64_t maxDecodedSampleBytes=512ll*1024*1024;
inline bool fitsSampleMemory(std::int64_t frames,double rate,int channels){return frames>=2&&channels>0&&std::isfinite(rate)&&rate>=8000&&rate<=384000&&frames<=rate*maxSampleSeconds&&(frames+4)<=maxDecodedSampleBytes/(std::clamp(channels,1,2)*(int)sizeof(float));}
inline double bounded(double v,double fallback,double lo,double hi){return std::isfinite(v)?std::clamp(v,lo,hi):fallback;}
struct SamplerSettings {
    // -1 preserves the old track-dependent gate/one-shot behaviour on legacy projects.
    int mode=-1,polyphony=32,transpose=0,sliceBase=36,sliceCount=0;
    bool loop=false,reverse=false,snap=true,filter=false,sync=false;
    double start=0,end=1,loopStart=0,loopEnd=1,crossfade=.01,fadeIn=.002,fadeOut=.005;
    double attack=.002,decay=.1,sustain=1,release=.1,fine=0,velocity=1,cutoff=20000,resonance=.1,sourceBpm=120,sensitivity=.5;
    std::array<double,128> slices{};
    void normalise(){
        mode=std::clamp(mode,-1,2);polyphony=std::clamp(polyphony,1,32);transpose=std::clamp(transpose,-48,48);sliceBase=std::clamp(sliceBase,0,127);sliceCount=std::clamp(sliceCount,0,std::min(128,128-sliceBase));
        start=bounded(start,0,0,.99999);end=bounded(end,1,start+.00001,1);loopStart=bounded(loopStart,start,start,end-.000001);loopEnd=bounded(loopEnd,end,loopStart+.000001,end);
        crossfade=bounded(crossfade,.01,0,.25);fadeIn=bounded(fadeIn,.002,0,2);fadeOut=bounded(fadeOut,.005,0,2);
        attack=bounded(attack,.002,0,10);decay=bounded(decay,.1,0,10);sustain=bounded(sustain,1,0,1);release=bounded(release,.1,.001,20);
        fine=bounded(fine,0,-100,100);velocity=bounded(velocity,1,0,1);cutoff=bounded(cutoff,20000,20,20000);resonance=bounded(resonance,.1,0,1);sourceBpm=bounded(sourceBpm,120,30,300);sensitivity=bounded(sensitivity,.5,0,1);
        for(int i=0;i<sliceCount;++i)slices[(size_t)i]=bounded(slices[(size_t)i],start,start,end-.000001);
        std::sort(slices.begin(),slices.begin()+sliceCount);auto last=std::unique(slices.begin(),slices.begin()+sliceCount,[](double a,double b){return std::abs(a-b)<.000001;});sliceCount=(int)(last-slices.begin());std::fill(slices.begin()+sliceCount,slices.end(),0);
    }
    bool operator==(const SamplerSettings& s)const{
        return mode==s.mode&&polyphony==s.polyphony&&transpose==s.transpose&&sliceBase==s.sliceBase&&sliceCount==s.sliceCount&&loop==s.loop&&reverse==s.reverse&&snap==s.snap&&filter==s.filter&&sync==s.sync&&start==s.start&&end==s.end&&loopStart==s.loopStart&&loopEnd==s.loopEnd&&crossfade==s.crossfade&&fadeIn==s.fadeIn&&fadeOut==s.fadeOut&&attack==s.attack&&decay==s.decay&&sustain==s.sustain&&release==s.release&&fine==s.fine&&velocity==s.velocity&&cutoff==s.cutoff&&resonance==s.resonance&&sourceBpm==s.sourceBpm&&sensitivity==s.sensitivity&&slices==s.slices;
    }
    bool isDefault()const{return *this==SamplerSettings{};}
};
struct BusSettings {
    std::string name;
    double gain=1,pan=0,amount=.5,damping=.5,feedback=.35,division=1;
    bool mute=false,solo=false;
    void normalise(){if(name.size()>40)name.resize(40);gain=bounded(gain,1,0,2);pan=bounded(pan,0,-1,1);amount=bounded(amount,.5,0,1);damping=bounded(damping,.5,0,1);feedback=bounded(feedback,.35,0,.9);division=bounded(division,1,.125,4);}
    bool operator==(const BusSettings& s)const{return name==s.name&&gain==s.gain&&pan==s.pan&&amount==s.amount&&damping==s.damping&&feedback==s.feedback&&division==s.division&&mute==s.mute&&solo==s.solo;}
};
inline std::array<BusSettings,4> defaultBuses(){std::array<BusSettings,4> b;b[0].name="GROUP A";b[1].name="GROUP B";b[2].name="REVERB";b[3].name="DELAY";return b;}
}
