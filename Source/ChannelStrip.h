// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "core/StudioSettings.h"
#include <juce_audio_basics/juce_audio_basics.h>
namespace fga {
// Fixed storage, direct-form II filters: coefficient updates never allocate.
class ChannelStrip {
    struct Filter {double b0=1,b1=0,b2=0,a1=0,a2=0;double z1[2]{},z2[2]{};
        float run(float x,int ch){const double y=b0*x+z1[ch];z1[ch]=b1*x-a1*y+z2[ch];z2[ch]=b2*x-a2*y;return (float)y;}
        void set(double b,double c,double d,double a,double e,double f){b0=b/a;b1=c/a;b2=d/a;a1=e/a;a2=f/a;}
    } filters[4];bool active=false;
public:
    void reset(){for(auto& f:filters){for(auto& x:f.z1)x=0;for(auto& x:f.z2)x=0;}}
    void configure(EqSettings p,double rate){p.normalise();active=p.enabled;rate=std::max(8000.,rate);
        for(int i=0;i<4;++i){const double hz=std::min(rate*.45,i==0?p.highPass:i==1?120.:i==2?1000.:6000.);const double w=6.283185307179586*hz/rate,c=std::cos(w),s=std::sin(w),alpha=s/(2*.70710678118);auto& f=filters[i];
            if(i==0){f.set((1+c)/2,-1-c,(1+c)/2,1+alpha,-2*c,1-alpha);continue;}
            const double db=i==1?p.low:i==2?p.mid:p.high,A=std::pow(10.,db/40.),beta=2*std::sqrt(A)*alpha;
            if(i==2)f.set(1+alpha*A,-2*c,1-alpha*A,1+alpha/A,-2*c,1-alpha/A);
            else if(i==1)f.set(A*((A+1)-(A-1)*c+beta),2*A*((A-1)-(A+1)*c),A*((A+1)-(A-1)*c-beta),(A+1)+(A-1)*c+beta,-2*((A-1)+(A+1)*c),(A+1)+(A-1)*c-beta);
            else f.set(A*((A+1)+(A-1)*c+beta),-2*A*((A-1)+(A+1)*c),A*((A+1)+(A-1)*c-beta),(A+1)-(A-1)*c+beta,2*((A-1)-(A+1)*c),(A+1)-(A-1)*c-beta);
        }
    }
    void process(juce::AudioBuffer<float>& audio,int count){if(!active)return;count=std::min(count,audio.getNumSamples());for(int ch=0;ch<std::min(2,audio.getNumChannels());++ch){auto* p=audio.getWritePointer(ch);for(int n=0;n<count;++n)for(auto& f:filters)p[n]=f.run(p[n],ch);}}
};
}
