// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <limits>
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
        for(auto& value:smoothed)value.reset(rate,.01);wet.reset(rate,.01);
        configure(values,true);reset();
    }
    void reset(){gainReduction=0;wet.setCurrentAndTargetValue(wet.getTargetValue());}
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
            const double mix=wet.getNextValue(),gain=1+mix*(std::pow(10.,(makeup-gainReduction)/20)-1);
            left[s]=(float)std::clamp(l*gain,-limit,limit);if(right)right[s]=(float)std::clamp(r*gain,-limit,limit);
        }
    }
private:
    double rate=44100,gainReduction=0;
    std::array<juce::SmoothedValue<double>,6> smoothed;
    juce::SmoothedValue<double> wet;
};
}
