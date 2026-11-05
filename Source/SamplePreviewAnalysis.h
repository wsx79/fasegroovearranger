// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "SamplePitchMonitor.h"
#include <juce_audio_basics/juce_audio_basics.h>
namespace fga {
struct PreviewAnalysis {double bpm=120;int midi=60;bool pitched=false;};
inline PreviewAnalysis analysePreview(const juce::AudioBuffer<float>& audio,double rate){
    PreviewAnalysis result;if(audio.getNumChannels()<1||audio.getNumSamples()<4096||rate<8000)return result;
    const int hop=std::max(1,(int)(rate*.01)),bins=audio.getNumSamples()/hop;std::vector<double> envelope((size_t)bins);double old=0,bestEnergy=0;int loudest=0;
    for(int i=0;i<bins;++i){double energy=0;for(int n=i*hop;n<(i+1)*hop;++n){const double x=audio.getSample(0,n);energy+=x*x;}energy=std::sqrt(energy/hop);envelope[(size_t)i]=std::max(0.,energy-old);old=energy;if(energy>bestEnergy){bestEnergy=energy;loudest=i*hop;}}
    double best=0;int lagBest=50;for(int lag=30;lag<=100&&lag<bins/2;++lag){double sum=0,norm=0;for(int i=lag;i<bins;++i){sum+=envelope[(size_t)i]*envelope[(size_t)(i-lag)];norm+=envelope[(size_t)i]*envelope[(size_t)i];}const double score=norm>1e-10?sum/norm:0;if(score>best){best=score;lagBest=lag;}}
    if(best>.1)result.bpm=6000./lagBest;
    const int start=std::clamp(loudest,0,audio.getNumSamples()-4096);const auto pitch=estimateSamplePitch(audio.getReadPointer(0,start),4096,rate);if(pitch.midi>=0&&pitch.confidence>.85){result.midi=pitch.midi;result.pitched=true;}return result;
}
}
