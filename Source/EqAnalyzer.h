// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_dsp/juce_dsp.h>
namespace fga {
class EqAnalyzer {
public:
    static constexpr int size=4096,points=128;
    using Trace=std::array<float,points>;
    EqAnalyzer(){for(int i=0;i<size;++i)window[(size_t)i]=(float)(.5-.5*std::cos(juce::MathConstants<double>::twoPi*i/(size-1)));}
    void analyse(const std::vector<float>& samples,double rate,Trace& target,int speed){
        std::array<float,size/2+1> power{};
        const int channels=samples.size()>=(size_t)(size*2)?2:1;
        if(samples.size()>=(size_t)size){
            for(int ch=0;ch<channels;++ch){
                work.fill(0);for(int i=0;i<size;++i)work[(size_t)i]=samples[(size_t)(ch*size+i)]*window[(size_t)i];
                fft.performFrequencyOnlyForwardTransform(work.data());
                for(int i=1;i<size/2;++i){const float amplitude=work[(size_t)i]*(4.f/(size-1));power[(size_t)i]+=amplitude*amplitude/channels;}
            }
        }
        rate=std::max(8000.,rate);const double maxHz=std::min(20000.,rate*.499),ratio=std::pow(maxHz/20.,1./(points-1));
        const float attack=speed==0?.85f:speed==1?.55f:.25f,release=speed==0?.8f:speed==1?.93f:.975f;
        for(int point=0;point<points;++point){
            const double hz=20.*std::pow(ratio,point),bin=std::clamp(hz*size/rate,1.,size/2.-1.);
            const int low=(int)bin,high=std::min(low+1,size/2);const float fraction=(float)(bin-low);
            float peak=power[(size_t)low]*(1-fraction)+power[(size_t)high]*fraction;
            const int first=std::max(1,(int)std::ceil(bin/std::sqrt(ratio))),last=std::min(size/2-1,(int)std::floor(bin*std::sqrt(ratio)));
            for(int i=first;i<=last;++i)peak=std::max(peak,power[(size_t)i]);
            const float db=10.f*std::log10(std::max(1.e-12f,peak)),value=std::clamp((db+96.f)/96.f,0.f,1.f);
            auto& current=target[(size_t)point];current=value>current?current+(value-current)*attack:std::max(value,current*release);
        }
    }
private:
    juce::dsp::FFT fft{12};std::array<float,size*2> work{};std::array<float,size> window{};
};
}
