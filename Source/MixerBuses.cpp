// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MixerBuses.h"
namespace fga {
void MixerBuses::prepare(double sr,int block){rate=sr;settings=&fallback;dry.setSize(2,block);for(auto& b:groups)b.setSize(2,block);for(auto& b:returns)b.setSize(2,block);for(auto& v:groupGain)v.resize((size_t)block);delay.setSize(2,(int)std::ceil(sr*8)+block+4);reverb.setSampleRate(sr);for(auto& pair:gains)for(auto& g:pair){g.reset(sr,.01);g.setCurrentAndTargetValue(1);}masterGain.reset(sr,.01);masterGain.setCurrentAndTargetValue(.8f);delaySamples.reset(sr,.04);delaySamples.setCurrentAndTargetValue((float)(sr*.5));reset();}
void MixerBuses::reset(){reverb.reset();delay.clear();write=0;damping.fill(0);for(auto& p:peaks)p=0;}
void MixerBuses::configure(const std::array<BusSettings,4>& p,double tempo,double master,bool snap){settings=&p;bpm=tempo;auto set=[snap](juce::SmoothedValue<float>& v,float target){if(snap)v.setCurrentAndTargetValue(target);else v.setTargetValue(target);};for(int b=0;b<4;++b){auto& v=p[(size_t)b];set(gains[(size_t)b][0],(float)(v.gain*std::min(1.,1.-v.pan)));set(gains[(size_t)b][1],(float)(v.gain*std::min(1.,1.+v.pan)));}set(masterGain,(float)master);const auto& r=p[2];juce::Reverb::Parameters rp;rp.roomSize=(float)r.amount;rp.damping=(float)r.damping;rp.wetLevel=1;rp.dryLevel=0;rp.width=1;reverb.setParameters(rp);delaySamples.setTargetValue((float)std::clamp(rate*60/bpm*p[3].division,1.,delay.getNumSamples()-2.));}
void MixerBuses::begin(int count,bool single){stem=single;dry.clear(0,count);for(auto& b:groups)b.clear(0,count);for(auto& b:returns)b.clear(0,count);groupSolo=!stem&&((*settings)[0].solo||(*settings)[1].solo);returnSolo=!stem&&((*settings)[2].solo||(*settings)[3].solo);for(int b=0;b<2;++b)for(int c=0;c<2;++c)for(int i=0;i<count;++i)groupGain[(size_t)(b*2+c)][(size_t)i]=gains[(size_t)b][(size_t)c].getNextValue();}
void MixerBuses::add(int ch,int sample,float value,int group,const std::array<double,2>& sends){if(ch>1)return;if(group>=0){const auto& p=(*settings)[(size_t)group];if(p.mute||(groupSolo&&!p.solo))return;value*=groupGain[(size_t)(group*2+ch)][(size_t)sample];groups[(size_t)group].addSample(ch,sample,value);}else{if(groupSolo)return;dry.addSample(ch,sample,value);}for(int b=0;b<2;++b)if(sends[(size_t)b]>0)returns[(size_t)b].addSample(ch,sample,value*(float)sends[(size_t)b]);}
void MixerBuses::finish(float* const* output,int channels,int count,const float* automation){
    for(int b=0;b<2;++b)peaks[(size_t)b]=groups[(size_t)b].getMagnitude(0,count);
    auto& verb=returns[0];reverb.processStereo(verb.getWritePointer(0),verb.getWritePointer(1),count);
    const auto& d=(*settings)[3];auto& echo=returns[1];const int size=delay.getNumSamples();const float coefficient=(float)(.03+(1-d.damping)*.97);
    for(int i=0;i<count;++i){double read=write-delaySamples.getNextValue();if(read<0)read+=size;int a=(int)read,b=(a+1)%size;float fraction=(float)(read-a),wet[2];for(int c=0;c<2;++c){auto* line=delay.getWritePointer(c);wet[c]=line[a]+fraction*(line[b]-line[a]);damping[(size_t)c]+=(wet[c]-damping[(size_t)c])*coefficient;}
        for(int c=0;c<2;++c){const float feedback=(float)((1-d.amount)*damping[(size_t)c]+d.amount*damping[(size_t)(1-c)]);delay.setSample(c,write,echo.getSample(c,i)+(float)d.feedback*feedback);echo.setSample(c,i,wet[c]);}write=(write+1)%size;
    }
    float returnPeaks[2]{};
    for(int i=0;i<count;++i){const float master=masterGain.getNextValue()*(automation?automation[i]:1.f);
        for(int c=0;c<2;++c){float value=returnSolo?0:dry.getSample(c,i)+groups[0].getSample(c,i)+groups[1].getSample(c,i);
            for(int b=0;b<2;++b){float wet=returns[(size_t)b].getSample(c,i)*gains[(size_t)(b+2)][(size_t)c].getNextValue();const auto& p=(*settings)[(size_t)(b+2)];if(p.mute||(returnSolo&&!p.solo))wet=0;returnPeaks[b]=std::max(returnPeaks[b],std::abs(wet));value+=wet;}
            if(c<channels&&output[c])output[c][i]+=value*master;
        }
    }peaks[2]=returnPeaks[0];peaks[3]=returnPeaks[1];
}
}
