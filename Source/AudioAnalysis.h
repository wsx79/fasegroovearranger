// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
// Offline analysis of imported audio, run on the import worker (never on the audio thread).
// Tempo: spectral-flux onset envelope, autocorrelation over 60-200 BPM with parabolic
// refinement and a house-range octave preference. Key: chromagram matched against the
// Krumhansl-Kessler major/minor profiles. Both report how far they can be trusted; key is
// "not tonal" when the chroma is flat (drums, noise, speech) - key is not chord detection.
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <vector>
namespace fga {
struct TempoEstimate {double bpm=0,confidence=0;};          // bpm 0 = not found
struct KeyEstimate {int key=-1;bool minor=false;double confidence=0;bool tonal=false;};

namespace audioanalysis {
inline std::vector<float> mono(const juce::AudioBuffer<float>& audio,double rate,double& outRate,int maxSeconds=90){
    // Mono, decimated to ~11 kHz: enough for onsets and pitch classes up to ~5 kHz.
    const int factor=std::max(1,(int)std::floor(rate/11025.));outRate=rate/factor;
    const int frames=std::min(audio.getNumSamples(),(int)(rate*maxSeconds));std::vector<float> out((size_t)(frames/factor));
    for(size_t i=0;i<out.size();++i){double sum=0;for(int k=0;k<factor;++k){const int at=(int)i*factor+k;for(int ch=0;ch<audio.getNumChannels();++ch)sum+=audio.getSample(ch,at);}out[i]=(float)(sum/(factor*std::max(1,audio.getNumChannels())));}
    return out;
}
}

inline TempoEstimate estimateTempo(const juce::AudioBuffer<float>& audio,double rate){
    TempoEstimate result;double r=0;const auto x=audioanalysis::mono(audio,rate,r);
    constexpr int order=10,size=1<<order,hop=128;if(x.size()<(size_t)(size*8)||r<4000)return result;
    juce::dsp::FFT fft(order);std::vector<float> window(size),buffer(size*2),previous(size/2+1,0.f);
    for(int i=0;i<size;++i)window[(size_t)i]=.5f-.5f*std::cos(juce::MathConstants<float>::twoPi*i/(size-1));
    std::vector<double> flux;
    for(size_t at=0;at+size<x.size();at+=hop){
        std::fill(buffer.begin(),buffer.end(),0.f);for(int i=0;i<size;++i)buffer[(size_t)i]=x[at+(size_t)i]*window[(size_t)i];
        fft.performFrequencyOnlyForwardTransform(buffer.data());double sum=0;
        for(int k=1;k<=size/2;++k){const float m=std::log1p(100.f*buffer[(size_t)k]);sum+=std::max(0.f,m-previous[(size_t)k]);previous[(size_t)k]=m;}
        flux.push_back(sum);
    }
    if(flux.size()<64)return result;
    // Remove the slow trend so sustained loudness changes do not look like a pulse.
    std::vector<double> onset(flux.size());const int smooth=16;double mean=0;
    for(size_t i=0;i<flux.size();++i){double local=0;int n=0;for(int k=-smooth;k<=smooth;++k){const long j=(long)i+k;if(j>=0&&j<(long)flux.size()){local+=flux[(size_t)j];++n;}}onset[i]=std::max(0.,flux[i]-local/n);mean+=onset[i];}
    mean/=onset.size();if(mean<=1e-9)return result;
    const double fps=r/hop;auto lagFor=[&](double bpm){return fps*60./bpm;};
    const int lo=(int)std::floor(lagFor(200)),hi=(int)std::ceil(lagFor(60));if(hi+2>=(int)onset.size()/2)return result;
    std::vector<double> ac((size_t)hi+3,0.);double zero=0;for(double v:onset)zero+=v*v;
    for(int lag=lo-1;lag<=hi+1;++lag){double s=0;for(size_t i=(size_t)lag;i<onset.size();++i)s+=onset[i]*onset[i-(size_t)lag];ac[(size_t)lag]=s/(onset.size()-lag);}
    // Score a tempo by its lag plus its double (a 4/4 pulse also repeats every two beats);
    // prefer 100-140 BPM, where house sits, when two octaves score alike.
    double bestScore=-1;int best=-1;
    for(int lag=lo;lag<=hi;++lag){if(ac[(size_t)lag]<ac[(size_t)lag-1]||ac[(size_t)lag]<ac[(size_t)lag+1])continue;
        const int twice=std::min(2*lag,(int)onset.size()-1);double s2=0;for(size_t i=(size_t)twice;i<onset.size();++i)s2+=onset[i]*onset[i-(size_t)twice];s2/=std::max<size_t>(1,onset.size()-twice);
        const double bpm=60.*fps/lag,prior=std::exp(-.5*std::pow(std::log2(bpm/122.)/.6,2));const double score=(ac[(size_t)lag]+.5*s2)*(.6+.4*prior);
        if(score>bestScore){bestScore=score;best=lag;}}
    if(best<0)return result;
    const double a=ac[(size_t)best-1],b=ac[(size_t)best],c=ac[(size_t)best+1],den=a-2*b+c;const double shift=std::abs(den)>1e-12?std::clamp(.5*(a-c)/den,-.5,.5):0.;
    result.bpm=60.*fps/(best+shift);
    // Confidence: how much the chosen periodicity stands above the average correlation.
    // Normalised autocorrelation at the beat lag (1 = perfectly periodic), minus the average
    // over all candidate lags: irregular hits score low even when they are loud.
    double avg=0;int n=0;for(int lag=lo;lag<=hi;++lag){avg+=ac[(size_t)lag];++n;}avg/=std::max(1,n);const double power=zero/onset.size();
    result.confidence=std::clamp((b-avg)/(power+1e-12)*2.,0.,1.);
    // Snap to a whole BPM when it is within 0.15: loops are usually made at integer tempi.
    if(std::abs(result.bpm-std::round(result.bpm))<.15)result.bpm=std::round(result.bpm);
    return result;
}

inline KeyEstimate estimateKey(const juce::AudioBuffer<float>& audio,double rate){
    KeyEstimate result;double r=0;const auto x=audioanalysis::mono(audio,rate,r);
    constexpr int order=13,size=1<<order;if(x.size()<(size_t)size*2||r<4000)return result;
    juce::dsp::FFT fft(order);std::vector<float> window(size),buffer(size*2);std::array<double,12> chroma{};
    for(int i=0;i<size;++i)window[(size_t)i]=.5f-.5f*std::cos(juce::MathConstants<float>::twoPi*i/(size-1));
    for(size_t at=0;at+size<x.size();at+=size/2){
        std::fill(buffer.begin(),buffer.end(),0.f);for(int i=0;i<size;++i)buffer[(size_t)i]=x[at+(size_t)i]*window[(size_t)i];
        fft.performFrequencyOnlyForwardTransform(buffer.data());std::array<double,12> frame{};double total=0;
        for(int k=1;k<size/2;++k){const double hz=k*r/size;if(hz<55||hz>2000)continue;const double m=buffer[(size_t)k];
            // Only spectral peaks: broadband noise (hats, snares) spreads evenly and is ignored.
            if(m<=buffer[(size_t)k-1]||m<buffer[(size_t)k+1])continue;
            const double midi=69+12*std::log2(hz/440.);const int pc=((int)std::lround(midi)%12+12)%12;frame[(size_t)pc]+=m*m;total+=m*m;}
        if(total>0)for(int pc=0;pc<12;++pc)chroma[(size_t)pc]+=frame[(size_t)pc]/total;
    }
    double sum=0;for(double v:chroma)sum+=v;if(sum<=0)return result;for(auto& v:chroma)v/=sum;
    // Flat chroma (percussion, noise): no key.
    double peak=0;for(double v:chroma)peak=std::max(peak,v);const double flatness=peak*12;   // 1 = perfectly flat
    static constexpr double major[12]{6.35,2.23,3.48,2.33,4.38,4.09,2.52,5.19,2.39,3.66,2.29,2.88};
    static constexpr double minor[12]{6.33,2.68,3.52,5.38,2.60,3.53,2.54,4.75,3.98,2.69,3.34,3.17};
    auto correlate=[&](const double* profile,int key){double mx=0,my=0;for(int i=0;i<12;++i){mx+=chroma[(size_t)((i+key)%12)];my+=profile[i];}mx/=12;my/=12;double sxy=0,sxx=0,syy=0;
        for(int i=0;i<12;++i){const double dx=chroma[(size_t)((i+key)%12)]-mx,dy=profile[i]-my;sxy+=dx*dy;sxx+=dx*dx;syy+=dy*dy;}return sxy/std::sqrt(sxx*syy+1e-18);};
    double best=-2,second=-2;for(int key=0;key<12;++key)for(int m=0;m<2;++m){const double c=correlate(m?minor:major,key);if(c>best){second=best;best=c;result.key=key;result.minor=m==1;}else if(c>second)second=c;}
    result.tonal=flatness>1.6&&best>.55;
    result.confidence=result.tonal?std::clamp((best-.55)*2.2+(best-second)*4.,0.,1.):0.;
    return result;
}
}
