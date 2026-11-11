// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MixerBuses.h"
namespace fga {
void MixerBuses::prepare(double sr,int block){sr=std::isfinite(sr)&&sr>0?sr:44100;rate=sr;for(auto& signal:sourceSignal)signal.resize((size_t)block);for(auto& depth:depths){depth.reset(sr,.01);depth.setCurrentAndTargetValue(0);}settings=&fallback;dry.setSize(2,block);for(auto& b:groups)b.setSize(2,block);for(auto& b:returns)b.setSize(2,block);for(auto& v:groupGain)v.resize((size_t)block);delay.setSize(2,(int)std::ceil(sr*8)+block+4);reverb.setSampleRate(sr);for(auto& pair:gains)for(auto& g:pair){g.reset(sr,.01);g.setCurrentAndTargetValue(1);}masterGain.reset(sr,.01);masterGain.setCurrentAndTargetValue(.8f);delaySamples.reset(sr,.04);delaySamples.setCurrentAndTargetValue((float)(sr*.5));reset();}
void MixerBuses::reset(){duck.fill(0);for(auto& r:reductions)r.store(0,std::memory_order_relaxed);reverb.reset();delay.clear();write=0;damping.fill(0);for(auto& p:peaks)p=0;}
void MixerBuses::configure(const std::array<BusSettings,4>& p,double tempo,double master,bool snap){settings=&p;bpm=tempo;auto set=[snap](juce::SmoothedValue<float>& v,float target){if(snap)v.setCurrentAndTargetValue(target);else v.setTargetValue(target);};for(int b=0;b<4;++b){auto& v=p[(size_t)b];set(gains[(size_t)b][0],(float)(v.gain*std::min(1.,1.-v.pan)));set(gains[(size_t)b][1],(float)(v.gain*std::min(1.,1.+v.pan)));}for(int b=0;b<2;++b){const auto& v=p[(size_t)b];
        attack[(size_t)b]=v.sidechainAttackMs<=0?0.f:(float)std::exp(-1./(rate*v.sidechainAttackMs*.001));
        // Keep the exact historical 180-ms coefficient, including its float rounding.
        release[(size_t)b]=(float)std::exp(-1./(rate*(v.sidechainReleaseMs==180?.18:v.sidechainReleaseMs*.001)));
        set(depths[(size_t)b],(float)v.sidechainAmount);
    }set(masterGain,(float)master);const auto& r=p[2];juce::Reverb::Parameters rp;rp.roomSize=(float)r.amount;rp.damping=(float)r.damping;rp.wetLevel=1;rp.dryLevel=0;rp.width=1;reverb.setParameters(rp);set(delaySamples,(float)std::clamp(rate*60/std::max(1.,bpm)*bounded(p[3].division,1.,.125,4.),1.,delay.getNumSamples()-2.));}
void MixerBuses::begin(int count,bool single){count=std::clamp(count,0,dry.getNumSamples());stem=single;for(auto& signal:sourceSignal)std::fill(signal.begin(),signal.begin()+count,0.f);dry.clear(0,count);for(auto& b:groups)b.clear(0,count);for(auto& b:returns)b.clear(0,count);groupSolo=!stem&&((*settings)[0].solo||(*settings)[1].solo);returnSolo=!stem&&((*settings)[2].solo||(*settings)[3].solo);for(int b=0;b<2;++b)for(int c=0;c<2;++c)for(int i=0;i<count;++i)groupGain[(size_t)(b*2+c)][(size_t)i]=gains[(size_t)b][(size_t)c].getNextValue();}
void MixerBuses::add(int ch,int sample,float value,int group,const std::array<double,2>& sends){if(ch<0||ch>1||sample<0||sample>=dry.getNumSamples()||group>1)return;if(group>=0){const auto& p=(*settings)[(size_t)group];if(p.mute||(groupSolo&&!p.solo))return;value*=groupGain[(size_t)(group*2+ch)][(size_t)sample];groups[(size_t)group].addSample(ch,sample,value);}else{if(groupSolo)return;dry.addSample(ch,sample,value);}for(int b=0;b<2;++b)if(sends[(size_t)b]>0)returns[(size_t)b].addSample(ch,sample,value*(float)sends[(size_t)b]);}
void MixerBuses::finish(float* const* output,int channels,int count,const float* automation){
    count=std::clamp(count,0,dry.getNumSamples());float groupPeaks[2]{};
    auto& verb=returns[0];reverb.processStereo(verb.getWritePointer(0),verb.getWritePointer(1),count);
    const auto& d=(*settings)[3];auto& echo=returns[1];const int size=delay.getNumSamples();const float coefficient=(float)(.03+(1-d.damping)*.97);
    for(int i=0;i<count;++i){double read=std::fmod(write-(double)delaySamples.getNextValue(),(double)size);if(read<0)read+=size;int a=(int)read,b=(a+1)%size;float fraction=(float)(read-a),wet[2];for(int c=0;c<2;++c){auto* line=delay.getWritePointer(c);wet[c]=line[a]+fraction*(line[b]-line[a]);damping[(size_t)c]+=(wet[c]-damping[(size_t)c])*coefficient;}
        for(int c=0;c<2;++c){const float feedback=(float)((1-d.amount)*damping[(size_t)c]+d.amount*damping[(size_t)(1-c)]);delay.setSample(c,write,echo.getSample(c,i)+(float)d.feedback*feedback);echo.setSample(c,i,wet[c]);}write=(write+1)%size;
    }
    float returnPeaks[2]{};
    float maximumReduction[2]{};
    for(int i=0;i<count;++i){
        for(int b=0;b<2;++b){const auto index=(size_t)b;const float detector=std::min(1.f,sourceSignal[index][(size_t)i]*6.f);
            auto& envelope=duck[index];
            envelope=detector>envelope?detector+attack[index]*(envelope-detector):envelope*release[index];
            const auto curve=(*settings)[index].sidechainCurve;
            const float shaped=curve==1?envelope:std::pow(envelope,(float)curve);
            const float reduction=depths[index].getNextValue()*shaped;
            maximumReduction[b]=std::max(maximumReduction[b],reduction);
            for(int ch=0;ch<2;++ch){const float sample=groups[index].getSample(ch,i)*(1.f-reduction);groups[index].setSample(ch,i,sample);groupPeaks[b]=std::max(groupPeaks[b],std::abs(sample));}
        }
        const float master=masterGain.getNextValue()*(automation?automation[i]:1.f);
        for(int c=0;c<2;++c){float value=returnSolo?0:dry.getSample(c,i)+groups[0].getSample(c,i)+groups[1].getSample(c,i);
            for(int b=0;b<2;++b){float wet=returns[(size_t)b].getSample(c,i)*gains[(size_t)(b+2)][(size_t)c].getNextValue();const auto& p=(*settings)[(size_t)(b+2)];if(p.mute||(returnSolo&&!p.solo))wet=0;returnPeaks[b]=std::max(returnPeaks[b],std::abs(wet));value+=wet;}
            if(c<channels&&output[c])output[c][i]+=value*master;
        }
    }for(int b=0;b<2;++b)reductions[(size_t)b].store(maximumReduction[b],std::memory_order_relaxed);peaks[0]=groupPeaks[0];peaks[1]=groupPeaks[1];peaks[2]=returnPeaks[0];peaks[3]=returnPeaks[1];
}
}
