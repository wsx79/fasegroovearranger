// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
namespace fga {
constexpr double maxSampleSeconds=1800.;
constexpr std::int64_t maxDecodedSampleBytes=512ll*1024*1024;
inline bool fitsSampleMemory(std::int64_t frames,double rate,int channels){return frames>=2&&channels>0&&std::isfinite(rate)&&rate>=8000&&rate<=384000&&frames<=rate*maxSampleSeconds&&(frames+4)<=maxDecodedSampleBytes/(std::clamp(channels,1,2)*(int)sizeof(float));}
inline double bounded(double v,double fallback,double lo,double hi){return std::isfinite(v)?std::clamp(v,lo,hi):fallback;}
struct SamplerSettings {
    // -1 preserves the old track-dependent gate/one-shot behaviour on legacy projects.
    int mode=-1,polyphony=32,transpose=0,sliceBase=36,sliceCount=0;
    bool loop=false,reverse=false,snap=true,filter=false,sync=false;
    double sliceSnapBeats=0; // optional musical slice grid; zero preserves legacy snap behavior
    double start=0,end=1,loopStart=0,loopEnd=1,crossfade=.01,fadeIn=.002,fadeOut=.005;
    double attack=.002,decay=.1,sustain=1,release=.1,fine=0,velocity=1,cutoff=20000,resonance=.1,sourceBpm=120,sensitivity=.5;
    // Segment bend: -1 is logarithmic (fast start), 0 is linear, +1 is exponential (slow start).
    double fadeInCurve=0,fadeOutCurve=0,attackCurve=0,decayCurve=0,releaseCurve=0;
    double gainDb=0;   // sample level in dB (-24..+24), for loops and stems that need balancing
    std::array<double,128> slices{};
    void normalise(){
        mode=std::clamp(mode,-1,2);polyphony=std::clamp(polyphony,1,32);transpose=std::clamp(transpose,-48,48);sliceBase=std::clamp(sliceBase,0,127);sliceCount=std::clamp(sliceCount,0,std::min(128,128-sliceBase));
        constexpr std::array<double,11> grids{{0,.125,1./6.,.25,1./3.,.5,1,2,4,8,16}};
        double grid=0;for(double candidate:grids)if(std::isfinite(sliceSnapBeats)&&std::abs(sliceSnapBeats-candidate)<1e-8)grid=candidate;sliceSnapBeats=grid;
        start=bounded(start,0,0,.99999);end=bounded(end,1,start+.00001,1);loopStart=bounded(loopStart,start,start,end-.000001);loopEnd=bounded(loopEnd,end,loopStart+.000001,end);
        crossfade=bounded(crossfade,.01,0,.25);fadeIn=bounded(fadeIn,.002,0,2);fadeOut=bounded(fadeOut,.005,0,2);
        attack=bounded(attack,.002,0,10);decay=bounded(decay,.1,0,10);sustain=bounded(sustain,1,0,1);release=bounded(release,.1,.001,20);
        fadeInCurve=bounded(fadeInCurve,0,-1,1);fadeOutCurve=bounded(fadeOutCurve,0,-1,1);attackCurve=bounded(attackCurve,0,-1,1);decayCurve=bounded(decayCurve,0,-1,1);releaseCurve=bounded(releaseCurve,0,-1,1);
        fine=bounded(fine,0,-100,100);velocity=bounded(velocity,1,0,1);cutoff=bounded(cutoff,20000,20,20000);resonance=bounded(resonance,.1,0,1);sourceBpm=bounded(sourceBpm,120,30,300);sensitivity=bounded(sensitivity,.5,0,1);gainDb=bounded(gainDb,0,-24,24);
        for(int i=0;i<sliceCount;++i)slices[(size_t)i]=bounded(slices[(size_t)i],start,start,end-.000001);
        std::sort(slices.begin(),slices.begin()+sliceCount);auto last=std::unique(slices.begin(),slices.begin()+sliceCount,[](double a,double b){return std::abs(a-b)<.000001;});sliceCount=(int)(last-slices.begin());std::fill(slices.begin()+sliceCount,slices.end(),0);
    }
    bool operator==(const SamplerSettings& s)const{
        return gainDb==s.gainDb&&sliceSnapBeats==s.sliceSnapBeats&&mode==s.mode&&polyphony==s.polyphony&&transpose==s.transpose&&sliceBase==s.sliceBase&&sliceCount==s.sliceCount&&loop==s.loop&&reverse==s.reverse&&snap==s.snap&&filter==s.filter&&sync==s.sync&&start==s.start&&end==s.end&&loopStart==s.loopStart&&loopEnd==s.loopEnd&&crossfade==s.crossfade&&fadeIn==s.fadeIn&&fadeOut==s.fadeOut&&attack==s.attack&&decay==s.decay&&sustain==s.sustain&&release==s.release&&fine==s.fine&&velocity==s.velocity&&cutoff==s.cutoff&&resonance==s.resonance&&sourceBpm==s.sourceBpm&&sensitivity==s.sensitivity&&fadeInCurve==s.fadeInCurve&&fadeOutCurve==s.fadeOutCurve&&attackCurve==s.attackCurve&&decayCurve==s.decayCurve&&releaseCurve==s.releaseCurve&&slices==s.slices;
    }
    bool isDefault()const{return *this==SamplerSettings{};}
};
// 0 stays exactly linear so an untouched fade or ADSR stage matches the previous sound.
inline double sampleEnvelopeCurve(double t,double curve){
    t=std::clamp(t,0.,1.);
    if(!(std::abs(curve)>1e-6))return t;
    return std::pow(t,std::exp(std::clamp(curve,-1.,1.)*1.3862943611198906));
}
enum class EqFilterType : int {
    Bell = 0,
    LowCut = 1,
    LowShelf = 2,
    HighShelf = 3,
    HighCut = 4,
    Notch = 5
};

struct EqBand {
    bool enabled = true;
    int type = 0; // EqFilterType
    double freq = 1000.0;
    double gain = 0.0;
    double q = 0.707;

    void normalise() {
        type = std::clamp(type, 0, 5);
        freq = bounded(freq, 1000.0, 20.0, 20000.0);
        gain = bounded(gain, 0.0, -24.0, 24.0);
        q = bounded(q, 0.707, 0.1, 18.0);
    }

    bool operator==(const EqBand& o) const {
        return enabled == o.enabled && type == o.type
            && std::abs(freq - o.freq) < 1e-3
            && std::abs(gain - o.gain) < 1e-3
            && std::abs(q - o.q) < 1e-3;
    }
};

struct EqSettings {
    bool enabled = false;
    double highPass = 20, low = 0, mid = 0, high = 0;
    double lowFreq = 120, midFreq = 1000, highFreq = 6000, midQ = 0.707;
    std::vector<EqBand> bands;

    std::array<EqBand,4> legacyBands() const {
        return {{{true,(int)EqFilterType::LowCut,highPass,0.0,0.707},
                 {true,(int)EqFilterType::LowShelf,lowFreq,low,0.707},
                 {true,(int)EqFilterType::Bell,midFreq,mid,midQ},
                 {true,(int)EqFilterType::HighShelf,highFreq,high,0.707}}};
    }
    void ensureBands() {
        if (bands.empty()) {
            const auto legacy=legacyBands();bands.assign(legacy.begin(),legacy.end());
        }
    }

    void syncLegacyFromBands() {
        std::array<bool,6> seen{};
        for (const auto& b : bands) {
            if(b.type<0||b.type>=6||seen[(size_t)b.type])continue;
            seen[(size_t)b.type]=true;
            if (b.type == (int)EqFilterType::LowCut) { highPass = b.freq; }
            else if (b.type == (int)EqFilterType::LowShelf) { lowFreq = b.freq; low = b.gain; }
            else if (b.type == (int)EqFilterType::Bell) { midFreq = b.freq; mid = b.gain; midQ = b.q; }
            else if (b.type == (int)EqFilterType::HighShelf) { highFreq = b.freq; high = b.gain; }
        }
    }

    // The compact inspector edits one band, without collapsing every bell or
    // shelf onto the legacy frequency/gain. Explicit bands are authoritative.
    bool setLegacyControl(int control,double value){
        ensureBands();const int types[]{1,2,0,3};if(control<0||control>3)return false;
        auto it=std::find_if(bands.begin(),bands.end(),[&](const EqBand& b){return b.type==types[control];});
        if(it==bands.end()){if(bands.size()>=16)return false;EqBand b;b.type=types[control];b.freq=control==0?highPass:control==1?lowFreq:control==2?midFreq:highFreq;bands.push_back(b);it=bands.end()-1;}
        if(control==0)it->freq=value;else it->gain=value;it->enabled=true;it->normalise();syncLegacyFromBands();return true;
    }
    void flatten(){ensureBands();for(auto& b:bands){b.gain=0;if(b.type==1||b.type==4||b.type==5)b.enabled=false;}syncLegacyFromBands();}

    void syncBandsFromLegacy() {
        ensureBands();
        for (auto& b : bands) {
            if (b.type == (int)EqFilterType::LowCut) { b.freq = highPass; }
            else if (b.type == (int)EqFilterType::LowShelf) { b.freq = lowFreq; b.gain = low; }
            else if (b.type == (int)EqFilterType::Bell) { b.freq = midFreq; b.gain = mid; b.q = midQ; }
            else if (b.type == (int)EqFilterType::HighShelf) { b.freq = highFreq; b.gain = high; }
        }
    }

    void normalise() {
        highPass = bounded(highPass, 20, 20, 400);
        low = bounded(low, 0, -24, 24);
        mid = bounded(mid, 0, -24, 24);
        high = bounded(high, 0, -24, 24);
        lowFreq = bounded(lowFreq, 120, 20, 800);
        midFreq = bounded(midFreq, 1000, 100, 12000);
        highFreq = bounded(highFreq, 6000, 1000, 20000);
        midQ = bounded(midQ, 0.707, 0.1, 18.0);
        if (bands.size() > 16) bands.resize(16);
        for (auto& b : bands) b.normalise();
    }

    bool operator==(const EqSettings& o) const {
        return enabled == o.enabled && highPass == o.highPass && low == o.low && mid == o.mid && high == o.high
            && lowFreq == o.lowFreq && midFreq == o.midFreq && highFreq == o.highFreq && midQ == o.midQ
            && bands == o.bands;
    }
};
struct BusSettings {
    std::string name;
    double gain=1,pan=0,amount=.5,damping=.5,feedback=.35,division=1,sidechainAmount=0;
    // Only the 18 stable generated track slots can be detector sources.
    int sidechainSource=4;
    double sidechainAttackMs=0,sidechainReleaseMs=180,sidechainCurve=1;
    bool mute=false,solo=false;
    void normalise(){sidechainSource=std::clamp(sidechainSource,0,17);sidechainAttackMs=bounded(sidechainAttackMs,0,0,200);sidechainReleaseMs=bounded(sidechainReleaseMs,180,10,2000);sidechainCurve=bounded(sidechainCurve,1,.25,4);if(name.size()>40)name.resize(40);sidechainAmount=bounded(sidechainAmount,0,0,1);gain=bounded(gain,1,0,2);pan=bounded(pan,0,-1,1);amount=bounded(amount,.5,0,1);damping=bounded(damping,.5,0,1);feedback=bounded(feedback,.35,0,.9);division=bounded(division,1,.125,4);}
    bool operator==(const BusSettings& s)const{return sidechainSource==s.sidechainSource&&sidechainAttackMs==s.sidechainAttackMs&&sidechainReleaseMs==s.sidechainReleaseMs&&sidechainCurve==s.sidechainCurve&&sidechainAmount==s.sidechainAmount&&name==s.name&&gain==s.gain&&pan==s.pan&&amount==s.amount&&damping==s.damping&&feedback==s.feedback&&division==s.division&&mute==s.mute&&solo==s.solo;}
};
inline std::array<BusSettings,4> defaultBuses(){std::array<BusSettings,4> b;b[0].name="GROUP A";b[1].name="GROUP B";b[2].name="REVERB";b[3].name="DELAY";return b;}
}
