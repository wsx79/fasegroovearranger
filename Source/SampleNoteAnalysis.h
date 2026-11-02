// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "Sampler.h"
#include "SamplePitchMonitor.h"
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <set>
namespace fga {
struct SampleNoteRegion {double start=0,end=0;std::vector<int> notes;};
// An advisory spectral map of the original sample, never a MIDI transcription.
// Runs off the audio/message threads; caps work for long recordings.
inline std::vector<SampleNoteRegion> analyseSampleNotes(const SampleAsset& asset,const std::atomic<unsigned>* epoch=nullptr,unsigned expected=0){
    constexpr int size=4096,window=2048;juce::dsp::FFT fft(12);
    const int stride=std::max(1,(int)std::ceil(asset.rate/11025.));const double rate=asset.rate/stride;
    const int total=asset.frames/stride,hop=std::max((int)(rate*.08),(total+4095)/4096);
    std::array<float,size*2> spectrum{};std::array<float,window> timeDomain{};std::vector<SampleNoteRegion> result;
    for(int centre=0;centre<total;centre+=hop){
        if(epoch&&epoch->load()!=expected)return {};
        spectrum.fill(0);timeDomain.fill(0);double energy=0;int sourceChannel=0;double strongestChannel=0;
        for(int ch=0;ch<asset.audio.getNumChannels();++ch){double channelEnergy=0;for(int j=0;j<window;++j){const int frame=(centre+j-window/2)*stride;if(frame>=0&&frame<asset.frames){const double v=asset.audio.getSample(ch,frame);channelEnergy+=v*v;}}if(channelEnergy>strongestChannel){strongestChannel=channelEnergy;sourceChannel=ch;}}

        for(int j=0;j<window;++j){const int frame=(centre+j-window/2)*stride;if(frame<0||frame>=asset.frames)continue;float value=asset.audio.getSample(sourceChannel,frame);timeDomain[(size_t)j]=value;value*=.5f-.5f*std::cos(2.f*juce::MathConstants<float>::pi*j/(window-1));spectrum[(size_t)j]=value;energy+=value*value;}
        if(energy/window<1.e-7)continue;fft.performFrequencyOnlyForwardTransform(spectrum.data());
        struct Peak {int bin;float magnitude;double hz;};std::vector<Peak> peaks;double totalEnergy=0;
        for(int i=1;i<size/2;++i)totalEnergy+=spectrum[(size_t)i]*spectrum[(size_t)i];
        for(int i=2;i<size/2-1;++i)if(spectrum[(size_t)i]>spectrum[(size_t)i-1]&&spectrum[(size_t)i]>=spectrum[(size_t)i+1]){
            const double a=spectrum[(size_t)i-1],b=spectrum[(size_t)i],c=spectrum[(size_t)i+1],den=a-2*b+c;
            const double delta=std::abs(den)>1.e-12?std::clamp(.5*(a-c)/den,-.5,.5):0.;const double hz=(i+delta)*rate/size;
            if(hz>=65&&hz<=2100)peaks.push_back({i,(float)b,hz});}
        std::sort(peaks.begin(),peaks.end(),[](const Peak& a,const Peak& b){return a.magnitude>b.magnitude;});
        double tonalEnergy=0;std::set<int> covered;for(size_t i=0;i<std::min(peaks.size(),size_t(8));++i)for(int k=-2;k<=2;++k){const int bin=peaks[i].bin+k;if(bin>=0&&covered.insert(bin).second)tonalEnergy+=spectrum[(size_t)bin]*spectrum[(size_t)bin];}
        if(peaks.empty()||tonalEnergy<std::max(1.e-12,totalEnergy)*.35)continue;
        const float strongest=peaks.front().magnitude;std::vector<int> notes;
        for(const auto& peak:peaks){if(peak.magnitude<strongest*.2f||notes.size()>=4)break;bool harmonic=false;
            for(const auto& lower:peaks)if(lower.hz<peak.hz*.7&&lower.magnitude>=peak.magnitude*.2f){const double ratio=peak.hz/lower.hz;if(ratio>=1.95&&ratio<=6.05&&std::abs(ratio-std::round(ratio))<.035){harmonic=true;break;}}
            if(harmonic)continue;const double midi=69+12*std::log2(peak.hz/440.);const int note=(int)std::lround(midi);if(std::abs(midi-note)>.35||note<36||note>96)continue;if(std::find(notes.begin(),notes.end(),note)==notes.end())notes.push_back(note);}
        if(notes.empty())continue;std::sort(notes.begin(),notes.end());
        // Sharp amplitude changes can create adjacent spectral sidebands around one tone.
        // Only resolve an adjacent-note ambiguity when independent periodicity is very strong.
        bool adjacent=false;for(size_t i=1;i<notes.size();++i)adjacent=adjacent||notes[i]-notes[i-1]==1;
        if(adjacent){const auto periodic=estimateSamplePitch(timeDomain.data(),window,rate);if(periodic.confidence>.94&&std::find(notes.begin(),notes.end(),periodic.midi)!=notes.end())notes={periodic.midi};else continue;}

        const double from=(double)centre/total,to=std::min(1.,(double)(centre+hop)/total);
        if(!result.empty()&&result.back().notes==notes&&from-result.back().end<1.e-6)result.back().end=to;else result.push_back({from,to,std::move(notes)});
    }
    return result;
}
inline juce::String sampleRegionLabel(const std::vector<int>& notes){
    std::set<int> classes;for(int note:notes)classes.insert(note%12);
    const std::array<std::pair<const char*,std::vector<int>>,4> chords{{{"",{0,4,7}},{"m",{0,3,7}},{"7",{0,4,7,10}},{"maj7",{0,4,7,11}}}};
    for(int root:classes)for(const auto& chord:chords){std::set<int> expected;for(int interval:chord.second)expected.insert((root+interval)%12);if(classes==expected)return juce::MidiMessage::getMidiNoteName(root,true,false,3)+chord.first;}
    juce::StringArray names;for(int note:notes)names.add(juce::MidiMessage::getMidiNoteName(note,true,true,3));return names.joinIntoString(" / ");
}
}
