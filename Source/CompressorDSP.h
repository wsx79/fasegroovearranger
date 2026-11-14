// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <limits>
#include <vector>
#include <cmath>
#include <algorithm>
namespace fga {
// Feed-forward, stereo-linked gain computer. No lookahead, allocation or latency.
class CompressorDSP {
public:
    using Values=std::array<float,6>; // threshold dB, ratio, knee dB, attack/release ms, makeup dB
    inline static constexpr Values defaults{-18,3,6,20,150,0};
    inline static constexpr Values minimum{-60,1,0,.1f,10,-12},maximum{0,20,24,100,1000,24};
    static float bounded(float value,int index){return std::isfinite(value)?std::clamp(value,minimum[(size_t)index],maximum[(size_t)index]):defaults[(size_t)index];}
    static double reduction(double level,double threshold,double ratio,double knee){
        const double slope=1-1/ratio,above=level-threshold;
        if(knee<=0)return slope*std::max(0.,above);
        if(above<=-knee*.5)return 0;
        if(above>=knee*.5)return slope*above;
        return slope*std::pow(above+knee*.5,2)/(2*knee);
    }
    void prepare(double sampleRate,const Values& values){
        rate=std::isfinite(sampleRate)?std::clamp(sampleRate,8000.,192000.):44100.;
        for(auto& value:smoothed)value.reset(rate,.01);wet.reset(rate,.01);userMix.reset(rate,.02);
        userMix.setCurrentAndTargetValue(1);
        configure(values,true);reset();
    }
    void reset(){gainReduction=0;wet.setCurrentAndTargetValue(wet.getTargetValue());userMix.setCurrentAndTargetValue(userMix.getTargetValue());}
    void setMix(double mix,bool snap=false){const double value=std::isfinite(mix)?std::clamp(mix,0.,1.):1.;if(snap)userMix.setCurrentAndTargetValue(value);else userMix.setTargetValue(value);}
    void configure(const Values& values,bool snap=false){
        for(int i=0;i<6;++i){double value=bounded(values[(size_t)i],i);
            if(i==3||i==4)value=std::exp(-1./(rate*value*.001));
            if(snap)smoothed[(size_t)i].setCurrentAndTargetValue(value);else smoothed[(size_t)i].setTargetValue(value);
        }
    }
    void setBypassed(bool bypassed){wet.setTargetValue(bypassed?0.:1.);}
    double reductionDb()const{return gainReduction*wet.getCurrentValue();}
    void process(float* left,float* right,int count,const float* detector=nullptr){
        constexpr double limit=std::numeric_limits<float>::max();
        for(int s=0;s<count;++s){
            const double l=std::isfinite(left[s])?left[s]:0,r=right&&std::isfinite(right[s])?right[s]:0;
            const double threshold=smoothed[0].getNextValue(),ratio=smoothed[1].getNextValue(),knee=smoothed[2].getNextValue();
            const double attack=smoothed[3].getNextValue(),release=smoothed[4].getNextValue(),makeup=smoothed[5].getNextValue();
            const double detection=detector?(std::isfinite(detector[s])?std::abs((double)detector[s]):0.):std::max(std::abs(l),std::abs(r));
            const double level=20*std::log10(std::max(1e-12,detection));
            const double target=reduction(level,threshold,ratio,knee);
            gainReduction=target+(target>gainReduction?attack:release)*(gainReduction-target);
            if(gainReduction<1e-12)gainReduction=0;
            const double mix=wet.getNextValue()*userMix.getNextValue(),gain=1+mix*(std::pow(10.,(makeup-gainReduction)/20)-1);
            left[s]=(float)std::clamp(l*gain,-limit,limit);if(right)right[s]=(float)std::clamp(r*gain,-limit,limit);
        }
    }
private:
    double rate=44100,gainReduction=0;
    std::array<juce::SmoothedValue<double>,6> smoothed;
    juce::SmoothedValue<double> wet,userMix{1};
};

// Brickwall lookahead limiter (limiter mode of the native compressor): input gain, then a gain
// that never lets a sample exceed the ceiling. Sliding minimum of the required gain over the
// lookahead, a release towards unity, then a moving average of the same length: every averaged
// value is at most the gain the delayed sample needs, so the ceiling holds without clipping.
class LimiterDSP {
public:
    void prepare(double sampleRate){rate=std::isfinite(sampleRate)?std::clamp(sampleRate,8000.,192000.):44100.;length=std::max(2,(int)std::lround(rate*.0015));
        delay.assign((size_t)length*2,0.f);need.assign((size_t)length+1,1.f);held.assign((size_t)length,1.f);reset();}
    int latency()const{return length;}
    void reset(){std::fill(delay.begin(),delay.end(),0.f);std::fill(need.begin(),need.end(),1.f);std::fill(held.begin(),held.end(),1.f);sum=length;hold=1;at=0;needAt=0;reduction=0;}
    void configure(double ceilingDb,double driveDb,double releaseMs){ceiling=std::pow(10.,std::clamp(ceilingDb,-60.,0.)/20);drive=std::pow(10.,std::clamp(driveDb,-12.,24.)/20);release=std::exp(-1./(rate*std::clamp(releaseMs,1.,2000.)*.001));}
    void process(float* left,float* right,int count){
        for(int s=0;s<count;++s){
            const double l=(std::isfinite(left[s])?left[s]:0)*drive,r=right&&std::isfinite(right[s])?right[s]*drive:l;
            const double peak=std::max(std::abs(l),std::abs(r)),required=peak>ceiling?ceiling/peak:1.;
            need[(size_t)needAt]=(float)required;needAt=(needAt+1)%(int)need.size();double window=1;for(float v:need)window=std::min(window,(double)v);   // short window: a direct minimum
            hold=std::min(window,hold+(1-hold)*(1-release));
            sum+=hold-held[(size_t)at];held[(size_t)at]=(float)hold;const double gain=std::min(1.,sum/length);
            const size_t write=(size_t)at*2;const float outL=delay[write],outR=delay[write+1];delay[write]=(float)l;delay[write+1]=(float)r;
            // the sample written L steps ago leaves now: its required gain is inside every one of the
            // last L minimum windows (each L+1 long), so their average cannot exceed it
            const float c=(float)ceiling;left[s]=std::clamp((float)(outL*gain),-c,c);if(right)right[s]=std::clamp((float)(outR*gain),-c,c);
            reduction=-20*std::log10(std::max(1e-9,gain));at=(at+1)%length;
        }
    }
    double reductionDb()const{return reduction;}
private:
    double rate=44100,ceiling=1,drive=1,release=.999,sum=0,hold=1,reduction=0;int length=2,at=0,needAt=0;
    std::vector<float> delay,need,held;
};
}
