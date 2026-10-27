// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
namespace fga {
struct SamplePitchReading {double hz=0,cents=0,confidence=0;int midi=-1;bool fresh=false;std::uint64_t generation=0;};
// YIN on the message thread. A periodic source is required; noise/silence has no note.
inline SamplePitchReading estimateSamplePitch(const float* samples,int count,double rate){
    SamplePitchReading result;if(count<256||rate<=0)return result;
    double mean=0,energy=0;for(int i=0;i<count;++i)mean+=samples[i];mean/=count;
    for(int i=0;i<count;++i){const double v=samples[i]-mean;energy+=v*v;}
    if(energy/count<.00000025)return result;
    const int span=count/2,maxLag=std::min({span-1,1023,(int)std::ceil(rate/35.)}),minLag=std::max(2,(int)(rate/1600.));
    std::array<double,1025> difference{},raw{};double sum=0;
    for(int lag=1;lag<=maxLag;++lag){double d=0;for(int i=0;i<span;++i){double v=samples[i]-samples[i+lag];d+=v*v;}raw[(size_t)lag]=d;sum+=d;difference[(size_t)lag]=sum>1e-20?d*lag/sum:1.;}
    int best=-1;for(int lag=2;lag<maxLag;++lag)if(difference[(size_t)lag]<.12){while(lag+1<maxLag&&difference[(size_t)(lag+1)]<difference[(size_t)lag])++lag;best=lag;break;}
    if(best<0||best<minLag-1)return result;
    const double a=raw[(size_t)(best-1)],b=raw[(size_t)best],c=raw[(size_t)(best+1)],den=a-2*b+c;
    const double lag=best+(std::abs(den)>1e-12?std::clamp(.5*(a-c)/den,-.5,.5):0.);
    const double hz=rate/lag;if(hz<35||hz>1600)return result;
    const double note=69.+12.*std::log2(hz/440.);result.midi=(int)std::lround(note);result.cents=100*(note-result.midi);result.hz=hz;result.confidence=1-difference[(size_t)best];return result;
}
// Audio producer / Studio consumer. No locks, allocation or pitch search in the callback.
class SamplePitchMonitor {
public:
    void select(int track,bool restart=false){if(track==selected&&!restart)return;selected=track;uiToken+=256;token.store(uiToken+(std::uint64_t)(track+1),std::memory_order_release);used=0;historyWrite=0;reading={};}
    void push(int track,const float* left,const float* right,int count,double rate){
        const auto key=token.load(std::memory_order_acquire);if((int)(key&255)!=track+1)return;
        if(key!=producerToken||rate!=producerRate){producerToken=key;producerRate=rate;phase=0;low1=low2=0;}
        const int step=std::max(1,(int)std::ceil(rate/24000.));const float alpha=(float)(1-std::exp(-2*3.141592653589793*2200/std::max(1.,rate)));
        const float* source=left;if(right){double l=0,r=0;for(int i=0;i<count;++i){l+=left[i]*left[i];r+=right[i]*right[i];}if(r>l)source=right;}
        for(int i=0;i<count;++i){float v=source[i];if(!std::isfinite(v))v=0;low1+=alpha*(v-low1);low2+=alpha*(low1-low2);
            if(++phase<step)continue;phase=0;const auto w=write.load(std::memory_order_relaxed),next=(w+1)%capacity;
            if(next==read.load(std::memory_order_acquire))continue;(*queue)[w]={low2,(float)(rate/step),key};write.store(next,std::memory_order_release);
        }
        if(std::abs(low1)<1e-20f)low1=0;if(std::abs(low2)<1e-20f)low2=0;
    }
    SamplePitchReading poll(){
        const auto key=token.load(std::memory_order_acquire);bool fresh=false;double rate=0;auto r=read.load(std::memory_order_relaxed);const auto w=write.load(std::memory_order_acquire);
        while(r!=w){const auto packet=(*queue)[r];r=(r+1)%capacity;if(packet.key!=key)continue;history[(size_t)historyWrite]=packet.sample;historyWrite=(historyWrite+1)%window;used=std::min(window,used+1);rate=packet.rate;fresh=true;}
        read.store(r,std::memory_order_release);if(!fresh){auto previous=reading;previous.fresh=false;previous.generation=key;return previous;}
        std::array<float,window> ordered{};for(int i=0;i<used;++i)ordered[(size_t)i]=history[(size_t)((historyWrite-used+i+window)%window)];
        reading=estimateSamplePitch(ordered.data(),used,rate);reading.fresh=true;reading.generation=key;return reading;
    }
private:
    static constexpr unsigned capacity=8192;static constexpr int window=2048;
    struct Packet {float sample=0,rate=0;std::uint64_t key=0;};
    std::unique_ptr<std::array<Packet,capacity>> queue=std::make_unique<std::array<Packet,capacity>>();std::atomic<unsigned> read{0},write{0};std::atomic<std::uint64_t> token{0};
    std::uint64_t uiToken=0,producerToken=0;int selected=-1,phase=0;double producerRate=0;float low1=0,low2=0;
    std::array<float,window> history{};int historyWrite=0,used=0;SamplePitchReading reading;
};
}
