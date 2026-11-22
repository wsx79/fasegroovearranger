// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
// ARRANGE FROM VOCAL: reads an imported vocal (a guide or an a cappella), finds its notes,
// key and phrases, and fits the song to it: chords under the melody, the band answering
// in the singer's rests. Pure functions on mono samples, so the analysis is testable.
#include "Project.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
namespace fga {
struct VocalNote { double beat=0,length=0; int pitch=60; double weight=0; };
struct VocalAnalysis {
    std::vector<VocalNote> notes;      // in song beats
    std::vector<std::uint8_t> bars;    // per song bar: 1 = the voice sings in it
    int key=-1,scale=0;                // scale 0 major, 2 minor (natural)
    double voicedSeconds=0;
};
namespace vocal {
// YIN pitch per 10 ms frame (0 = unvoiced) on audio decimated to about 11 kHz.
inline std::vector<double> trackPitch(const std::vector<float>& input,double rate,std::vector<double>& rms,double& frameSeconds,double& windowSeconds){
    const int factor=std::max(1,(int)std::floor(rate/11025.));const double r=rate/factor;
    std::vector<float> x(input.size()/(size_t)factor);
    for(size_t i=0;i<x.size();++i){float s=0;for(int k=0;k<factor;++k)s+=input[i*(size_t)factor+(size_t)k];x[i]=s/(float)factor;}
    const int hop=(int)std::lround(r*.01),window=(int)std::lround(r*.04),minLag=(int)(r/1000.),maxLag=(int)(r/70.);
    frameSeconds=hop/r;windowSeconds=window/r;std::vector<double> f0;rms.clear();
    if((int)x.size()<window+maxLag+1)return f0;
    std::vector<double> d((size_t)maxLag+1);
    for(size_t at=0;at+(size_t)(window+maxLag)<x.size();at+=(size_t)hop){
        double energy=0;for(int i=0;i<window;++i)energy+=x[at+(size_t)i]*x[at+(size_t)i];rms.push_back(std::sqrt(energy/window));
        for(int tau=minLag;tau<=maxLag;++tau){double sum=0;const float* a=&x[at];const float* b=&x[at+(size_t)tau];for(int i=0;i<window;++i){const double diff=a[i]-b[i];sum+=diff*diff;}d[(size_t)tau]=sum;}
        double running=0;int best=-1;
        for(int tau=1;tau<=maxLag;++tau){if(tau<minLag){running+=d[(size_t)tau];continue;}running+=d[(size_t)tau];const double cmnd=d[(size_t)tau]*tau/std::max(1e-12,running);
            if(cmnd<.15){while(tau+1<=maxLag&&d[(size_t)tau+1]*(tau+1)/std::max(1e-12,running+d[(size_t)tau+1])<cmnd)++tau;best=tau;break;}}
        if(best<=minLag||best>=maxLag){f0.push_back(0);continue;}
        const double a=d[(size_t)best-1],b=d[(size_t)best],c=d[(size_t)best+1],den=a-2*b+c;const double shift=std::abs(den)>1e-12?.5*(a-c)/den:0;
        f0.push_back(r/(best+std::clamp(shift,-1.,1.)));
    }
    return f0;
}
// Krumhansl-Kessler profiles: the key whose profile best correlates with the pitch classes.
inline void estimateKey(const std::array<double,12>& pc,int& key,int& scale){
    static constexpr double major[12]{6.35,2.23,3.48,2.33,4.38,4.09,2.52,5.19,2.39,3.66,2.29,2.88},minor[12]{6.33,2.68,3.52,5.38,2.60,3.53,2.54,4.75,3.98,2.69,3.34,3.17};
    double total=0;for(double v:pc)total+=v;key=-1;if(total<=0)return;double bestScore=-2;
    for(int k=0;k<12;++k)for(int m=0;m<2;++m){const double* prof=m?minor:major;double mx=0,my=0;for(int i=0;i<12;++i){mx+=pc[(size_t)((i+k)%12)];my+=prof[i];}mx/=12;my/=12;
        double sxy=0,sxx=0,syy=0;for(int i=0;i<12;++i){const double a=pc[(size_t)((i+k)%12)]-mx,b=prof[i]-my;sxy+=a*b;sxx+=a*a;syy+=b*b;}
        const double score=sxy/std::sqrt(std::max(1e-12,sxx*syy));if(score>bestScore){bestScore=score;key=k;scale=m?2:0;}}
}
// The triad on each degree of the major / natural minor scale, as pitch classes above the key.
inline std::array<int,3> triad(int scale,int degree){
    static constexpr int majorScale[7]{0,2,4,5,7,9,11},minorScale[7]{0,2,3,5,7,8,10};const int* s=scale==2?minorScale:majorScale;
    return {s[degree%7],s[(degree+2)%7],s[(degree+4)%7]};
}
// One chord per slot: the triad that holds the most of the melody (weighted by length and level),
// with a cost for melody notes a semitone above a chord tone, a light pull to the common degrees
// and against repeating the previous chord. An empty slot keeps the chord it had.
inline std::array<int,8> chooseDegrees(const std::vector<std::array<double,12>>& slots,int key,int scale,const std::array<int,8>& previous){
    auto out=previous;static constexpr double prior[7]{.12,.02,.03,.08,.1,.08,.0};int last=-1;
    for(size_t k=0;k<slots.size()&&k<8;++k){double total=0;for(double v:slots[k])total+=v;if(total<=0){last=out[k];continue;}
        int best=out[k];double bestScore=-1e9;
        for(int dgr=0;dgr<7;++dgr){if(scale==0&&dgr==6)continue;if(scale==2&&dgr==1)continue;   // no diminished triads
            const auto t=triad(scale,dgr);double score=0;
            for(int pc=0;pc<12;++pc){const double w=slots[k][(size_t)((pc+key)%12)]/total;bool tone=false,avoid=false;for(int c:t){tone=tone||pc==c;avoid=avoid||pc==(c+1)%12;}score+=tone?w:avoid?-.6*w:-.1*w;}
            score+=prior[dgr]+(dgr==last?-.08:0)+(k==0&&dgr==0?.05:0);
            if(score>bestScore){bestScore=score;best=dgr;}}
        out[k]=best;last=best;}
    return out;
}
}
// samples: mono vocal; at: song beat of samples[0]; beatsPerSecond: tempo the audio plays at.
inline VocalAnalysis analyseVocal(const std::vector<float>& samples,double rate,double at,double beatsPerSecond,int songBars){
    VocalAnalysis a;a.bars.assign((size_t)std::max(0,songBars),0);
    if(!(rate>=8000)||!(beatsPerSecond>0)||samples.empty())return a;
    std::vector<double> rms;double frame=.01,window=.04;const auto f0=vocal::trackPitch(samples,rate,rms,frame,window);if(f0.empty())return a;
    auto sorted=rms;std::sort(sorted.begin(),sorted.end());const double gate=std::max(1e-4,.12*sorted[(size_t)(sorted.size()*9/10)]);
    // Semitone per frame, smoothed by a 5-frame median; runs of one semitone of at least 60 ms are notes.
    std::vector<int> semis(f0.size(),-1);
    for(size_t i=0;i<f0.size();++i)if(f0[i]>70&&f0[i]<1100&&rms[i]>=gate)semis[i]=(int)std::lround(69+12*std::log2(f0[i]/440.));
    std::vector<int> smooth(semis);
    for(size_t i=2;i+2<semis.size();++i){std::array<int,5> w{semis[i-2],semis[i-1],semis[i],semis[i+1],semis[i+2]};if(std::count(w.begin(),w.end(),-1)>2){smooth[i]=-1;continue;}std::sort(w.begin(),w.end());smooth[i]=w[2]==-1?w[3]:w[2];}
    std::array<double,12> pcs{};
    for(size_t i=0;i<smooth.size();){if(smooth[i]<0){++i;continue;}size_t j=i;double level=0;while(j<smooth.size()&&smooth[j]==smooth[i]){level+=rms[j];++j;}
        // a dip of up to 3 frames back to the same note is one note
        while(j+3<smooth.size()&&smooth[j]!=smooth[i]){size_t k=j;while(k<smooth.size()&&k<j+3&&smooth[k]!=smooth[i])++k;if(k<smooth.size()&&k<j+3&&smooth[k]==smooth[i]){j=k;while(j<smooth.size()&&smooth[j]==smooth[i]){level+=rms[j];++j;}}else break;}
        const double seconds=(j-i)*frame;
        if(seconds>=.06){VocalNote n;n.beat=at+(i*frame+window*.5)*beatsPerSecond;/* a frame stands for the middle of its window */n.length=seconds*beatsPerSecond;n.pitch=std::clamp(smooth[i],0,127);n.weight=seconds*level/(j-i);a.notes.push_back(n);pcs[(size_t)(n.pitch%12)]+=n.weight;a.voicedSeconds+=seconds;}
        i=j;}
    vocal::estimateKey(pcs,a.key,a.scale);
    // A bar sings when notes fill at least a quarter of it.
    std::vector<double> filled(a.bars.size(),0);
    for(const auto& n:a.notes){for(double b=n.beat;b<n.beat+n.length;){const int bar=(int)std::floor(b/4);const double end=std::min(n.beat+n.length,(bar+1)*4.);if(bar>=0&&bar<(int)filled.size())filled[(size_t)bar]+=end-b;b=end;}}
    for(size_t i=0;i<filled.size();++i)a.bars[i]=filled[i]>=1.?1:0;
    return a;
}
// Fits the project to the analysis: key, one chord per bar in four-chord loops under the melody of
// each section the voice sings in, VOCALS PRESENT following the real phrases. Returns the sections changed.
inline int fitToVocal(Project& p,const VocalAnalysis& a){
    if(a.key<0||a.notes.empty())return 0;
    p.settings.key=a.key;p.settings.scale=a.scale;p.settings.vocalsPresent=true;p.settings.vocalPhrase=3;p.settings.vocalBars=a.bars;
    int changed=0;
    for(int si=0;si<(int)p.sections.size();++si){const double from=p.sectionStart(si);const int bars=p.sections[(size_t)si].bars;if(bars<=0)continue;
        int sung=0;for(int b=0;b<bars;++b){const int bar=(int)std::lround(from/4)+b;if(bar>=0&&bar<(int)a.bars.size())sung+=a.bars[(size_t)bar];}
        if(sung<2)continue;
        const int count=std::min(4,bars);std::vector<std::array<double,12>> slots((size_t)count);
        for(const auto& n:a.notes){if(n.beat+n.length<=from||n.beat>=from+bars*4.)continue;
            // the downbeat and a long note count most: the chord must hold them
            for(double b=std::max(n.beat,from);b<std::min(n.beat+n.length,from+bars*4.);){const int bar=(int)std::floor((b-from)/4);const double end=std::min({n.beat+n.length,from+(bar+1)*4.});
                const double onBeat=std::fmod(b-from,1.)<.1?1.5:1.;slots[(size_t)(bar%count)][(size_t)(n.pitch%12)]+=(end-b)*onBeat*n.weight;b=end;}}
        auto& h=p.sections[(size_t)si].harmony;const auto before=activeHarmony(p,si);
        h.enabled=true;h.key=a.key;h.scale=a.scale;h.chordCount=count;h.chordSteps=16;h.freeChords.fill(-1);h.manualChords.fill(false);
        h.degrees=vocal::chooseDegrees(slots,a.key,a.scale,before.degrees);++changed;}
    return changed;
}
}
