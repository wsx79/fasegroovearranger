// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
// An imported audio as a guide for the arranger: per song bar its pitch classes (chroma), level and
// attacks. CHORDS FROM AUDIO fits section chords to a loop or a song, GROOVE FROM AUDIO takes the
// timing and accents of its 16ths, SONG FORM FROM REFERENCE takes its form from its energy.
#include <juce_dsp/juce_dsp.h>
#include "core/VocalArrangement.h"
namespace fga {
struct AudioBar { std::array<double,12> chroma{}; double level=0; bool present=false; };
struct AudioReference {
    std::vector<AudioBar> bars;                       // per song bar
    std::vector<std::pair<double,double>> onsets;     // song beat, strength
    int key=-1,scale=0;
};
namespace audioref {
// Spectral chroma (110 Hz-2 kHz, 4096-point frames, hop 1024) and spectral-flux onsets.
inline AudioReference analyse(const std::vector<float>& x,double rate,double at,double beatsPerSecond,int songBars){
    AudioReference r;r.bars.assign((size_t)std::max(0,songBars),{});
    constexpr int order=12,size=1<<order,hop=1024;if(x.size()<(size_t)size||!(rate>=8000)||!(beatsPerSecond>0))return r;
    juce::dsp::FFT fft(order);std::vector<float> window(size),buffer(size*2);
    for(int i=0;i<size;++i)window[(size_t)i]=.5f-.5f*std::cos(juce::MathConstants<float>::twoPi*i/(size-1));
    std::vector<int> pcOf(size/2+1,-1);for(int b=1;b<=size/2;++b){const double f=b*rate/size;if(f>=110&&f<=2000)pcOf[(size_t)b]=((int)std::lround(69+12*std::log2(f/440.))%12+12)%12;}
    std::vector<double> levels;
    for(size_t start=0;start+(size_t)size<=x.size();start+=hop){
        double energy=0;for(int i=0;i<size;++i){const float v=x[start+(size_t)i];buffer[(size_t)i]=v*window[(size_t)i];energy+=v*v;}std::fill(buffer.begin()+size,buffer.end(),0.f);
        fft.performFrequencyOnlyForwardTransform(buffer.data());
        const double t=(start+size*.5)/rate,beat=at+t*beatsPerSecond;const int bar=(int)std::floor(beat/4);
        if(bar<0||bar>=(int)r.bars.size())continue;auto& out=r.bars[(size_t)bar];
        for(int b=1;b<=size/2;++b)if(pcOf[(size_t)b]>=0)out.chroma[(size_t)pcOf[(size_t)b]]+=buffer[(size_t)b];
        out.level+=std::sqrt(energy/size);levels.push_back(std::sqrt(energy/size));
    }
    auto sorted=levels;std::sort(sorted.begin(),sorted.end());const double gate=sorted.empty()?0:.1*sorted[(size_t)(sorted.size()*9/10)];
    std::array<double,12> total{};
    for(auto& b:r.bars)for(int i=0;i<12;++i)total[(size_t)i]+=b.chroma[(size_t)i];
    // a bar counts when its mean frame level is above the gate
    {const double framesPerBar=4/beatsPerSecond*rate/hop;for(auto& b:r.bars){b.level/=std::max(1.,framesPerBar);b.present=b.level>gate&&b.level>1e-5;}}
    vocal::estimateKey(total,r.key,r.scale);
    // Onsets: spectral flux on 1024-point frames every 256 samples (about 6 ms at 44.1 kHz), peaks above
    // 1.5 times the running median; the time is the frame centre less half a hop.
    {constexpr int o=10,n=1<<o,step=256;juce::dsp::FFT small(o);std::vector<float> w(n),buf(n*2),prev(n/2+1,0.f);std::vector<double> flux,when;
        for(int i=0;i<n;++i)w[(size_t)i]=.5f-.5f*std::cos(juce::MathConstants<float>::twoPi*i/(n-1));
        for(size_t start=0;start+(size_t)n<=x.size();start+=step){for(int i=0;i<n;++i)buf[(size_t)i]=x[start+(size_t)i]*w[(size_t)i];std::fill(buf.begin()+n,buf.end(),0.f);small.performFrequencyOnlyForwardTransform(buf.data());
            double f=0;for(int b=1;b<=n/2;++b){f+=std::max(0.f,buf[(size_t)b]-prev[(size_t)b]);prev[(size_t)b]=buf[(size_t)b];}flux.push_back(f);when.push_back(at+((start+n*.5-step*.5)/rate)*beatsPerSecond);}
        for(size_t i=1;i+1<flux.size();++i){const size_t from=i>16?i-16:0,to=std::min(flux.size(),i+17);std::vector<double> m(flux.begin()+(long)from,flux.begin()+(long)to);std::nth_element(m.begin(),m.begin()+(long)m.size()/2,m.end());
            if(flux[i]>flux[i-1]&&flux[i]>=flux[i+1]&&flux[i]>1.5*m[m.size()/2]+1e-6)r.onsets.push_back({when[i],flux[i]});}}
    return r;
}
// CHORDS FROM AUDIO: four one-bar chords per section the audio plays in, from its chroma.
inline int fitChords(Project& p,const AudioReference& a){
    if(a.key<0)return 0;p.settings.key=a.key;p.settings.scale=a.scale;int changed=0;
    for(int si=0;si<(int)p.sections.size();++si){const double from=p.sectionStart(si);const int bars=p.sections[(size_t)si].bars;if(bars<=0)continue;
        const int first=(int)std::lround(from/4);const int count=std::min(4,bars);std::vector<std::array<double,12>> slots((size_t)count);int heard=0;
        for(int b=0;b<bars;++b){const int bar=first+b;if(bar<0||bar>=(int)a.bars.size()||!a.bars[(size_t)bar].present)continue;++heard;double sum=0;for(double v:a.bars[(size_t)bar].chroma)sum+=v;if(sum<=0)continue;
            for(int i=0;i<12;++i)slots[(size_t)(b%count)][(size_t)i]+=a.bars[(size_t)bar].chroma[(size_t)i]/sum;}
        if(heard<2)continue;auto& h=p.sections[(size_t)si].harmony;const auto before=activeHarmony(p,si);
        h.enabled=true;h.key=a.key;h.scale=a.scale;h.chordCount=count;h.chordSteps=16;h.freeChords.fill(-1);h.manualChords.fill(false);
        h.degrees=vocal::chooseDegrees(slots,a.key,a.scale,before.degrees);++changed;}
    return changed;
}
// GROOVE FROM AUDIO: the mean offset (in 16ths) and level of the attacks near each 16th of the bar.
inline bool grooveFrom(const AudioReference& a,std::array<double,16>& offsets,std::array<double,16>& accents){
    std::array<double,16> sum{},level{};std::array<int,16> count{};double loudest=0;
    for(const auto& [beat,strength]:a.onsets){const double grid=std::round(beat*4)/4;const int slot=((int)std::llround(grid*4)%16+16)%16;const double off=(beat-grid)*4;if(std::abs(off)>.45)continue;
        sum[(size_t)slot]+=off;level[(size_t)slot]+=strength;++count[(size_t)slot];}
    int used=0;for(int i=0;i<16;++i)if(count[(size_t)i]){level[(size_t)i]/=count[(size_t)i];loudest=std::max(loudest,level[(size_t)i]);++used;}
    if(used<4||loudest<=0)return false;
    // The groove is relative to the beat: the beats' own mean offset (and the analysis window's delay) is the zero.
    double beatSum=0;int beats=0;for(int i=0;i<16;i+=4)if(count[(size_t)i]){beatSum+=sum[(size_t)i]/count[(size_t)i];++beats;}
    if(beats)for(int i=0;i<16;++i)if(count[(size_t)i])sum[(size_t)i]-=beatSum/beats*count[(size_t)i];
    for(int i=0;i<16;++i){offsets[(size_t)i]=count[(size_t)i]?std::clamp(sum[(size_t)i]/count[(size_t)i]*.25,-.125,.125):0;accents[(size_t)i]=count[(size_t)i]?std::clamp(.5+.6*level[(size_t)i]/loudest,.5,1.1):1;}
    return true;
}
// SONG FORM FROM REFERENCE: 8-bar blocks by energy -> intro / main / breakdown / buildup / drop / outro.
inline std::vector<SongSection> formFrom(const AudioReference& a){
    std::vector<double> blocks;for(size_t b=0;b<a.bars.size();b+=8){double e=0;int n=0;for(size_t k=b;k<std::min(a.bars.size(),b+8);++k){e+=a.bars[k].level;++n;}if(n>=4)blocks.push_back(e/n);}
    std::vector<SongSection> out;if(blocks.size()<3)return out;
    auto sorted=blocks;std::sort(sorted.begin(),sorted.end());const double lo=sorted[sorted.size()/4],hi=sorted[sorted.size()*3/4];
    for(size_t i=0;i<blocks.size();++i){Section type=Section::main;const double e=blocks[i];
        if(i==0&&e<=lo*1.05)type=Section::intro;else if(i+1==blocks.size()&&e<=lo*1.05)type=Section::outro;
        else if(e>=hi*.98)type=Section::drop;else if(e<=lo*1.05)type=Section::breakdown;
        else if(i+1<blocks.size()&&blocks[i+1]>=hi*.98&&e<blocks[i+1])type=Section::buildup;
        if(!out.empty()&&out.back().type==type&&out.back().bars<32)out.back().bars+=8;else{SongSection s;s.type=type;s.bars=8;out.push_back(s);}}
    static const char* names[]{"Main","Intro","Breakdown","Build-up","Drop","Outro"};int n=1;for(auto& s:out)s.name=std::string(names[(int)s.type])+(s.type==Section::main||s.type==Section::drop?" "+std::to_string(n++):"");
    return out;
}
}
}
