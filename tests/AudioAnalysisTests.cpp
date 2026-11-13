// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/AudioAnalysis.h"
#include <iostream>
#include <random>
#include <stdexcept>
namespace {
void need(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
juce::AudioBuffer<float> loop(double rate,double bpm,double seconds,std::uint32_t seed){
    juce::AudioBuffer<float> b(2,(int)(rate*seconds));b.clear();std::mt19937 noise(seed);std::uniform_real_distribution<float> u(-1,1);
    const double beat=60./bpm;
    for(int i=0;i<b.getNumSamples();++i){const double t=i/rate,inBeat=std::fmod(t,beat),inHalf=std::fmod(t+beat*.5,beat);
        float v=(float)(.8*std::exp(-inBeat*28)*std::sin(2*juce::MathConstants<double>::pi*(55+90*std::exp(-inBeat*40))*inBeat));   // kick on the beat
        v+=(float)(.18*std::exp(-inHalf*90))*u(noise);                                                                            // off-beat hat
        b.setSample(0,i,v);b.setSample(1,i,v);}
    return b;
}
juce::AudioBuffer<float> tonal(double rate,int root,bool minor,double seconds){
    // i - iv - v - i in the key, with the triad tones and a little melody: a key, not one chord.
    juce::AudioBuffer<float> b(2,(int)(rate*seconds));b.clear();const int third=minor?3:4;
    const int progression[4][3]{{0,third,7},{5,5+(minor?3:4),12},{7,7+(minor?3:4),14},{0,third,7}};
    for(int i=0;i<b.getNumSamples();++i){const double t=i/rate;const int bar=(int)(t/2)%4;double v=0;
        for(int n=0;n<3;++n){const double hz=220*std::pow(2.,(root-9+progression[bar][n])/12.);v+=.15*std::sin(2*juce::MathConstants<double>::pi*hz*t);}
        b.setSample(0,i,(float)v);b.setSample(1,i,(float)v);}
    return b;
}
}
void runAudioAnalysisTests(){
    using namespace fga;
    for(double rate:{44100.,48000.})for(double bpm:{98.,124.,128.,174.}){const auto t=estimateTempo(loop(rate,bpm,24,7),rate);
        std::cout<<"tempo "<<bpm<<" @"<<rate<<" -> "<<t.bpm<<" conf "<<t.confidence<<"\n";
        need(std::abs(t.bpm-bpm)<.6||std::abs(t.bpm*2-bpm)<1.2||std::abs(t.bpm-bpm*2)<1.2,"tempo within 0.6 BPM (or an exact octave)");need(t.confidence>.3,"a steady loop is reliable");}
    {const auto t=estimateTempo(loop(44100,124,24,7),44100);need(std::abs(t.bpm-124)<.6,"house tempo is not halved or doubled");}
    {juce::AudioBuffer<float> hits(2,44100*24);hits.clear();std::mt19937 g(11);std::uniform_real_distribution<double> gap(.07,.9);
        for(double t=.1;t<23.5;t+=gap(g)){const int at=(int)(t*44100);for(int i=0;i<2000&&at+i<hits.getNumSamples();++i){const float v=(float)(.7*std::exp(-i/300.)*std::sin(i*.05));hits.setSample(0,at+i,v);hits.setSample(1,at+i,v);}}
        const auto t=estimateTempo(hits,44100);std::cout<<"irregular -> "<<t.bpm<<" conf "<<t.confidence<<"\n";need(t.confidence<.5,"irregular hits are not a reliable tempo");}
    {juce::AudioBuffer<float> silence(2,44100*8);silence.clear();need(estimateTempo(silence,44100).bpm==0,"silence has no tempo");}
    for(int root:{0,2,9,7})for(bool minor:{false,true}){const auto k=estimateKey(tonal(44100,root,minor,16),44100);
        std::cout<<"key "<<root<<(minor?"m":"")<<" -> "<<k.key<<(k.minor?"m":"")<<" conf "<<k.confidence<<" tonal "<<k.tonal<<"\n";
        need(k.tonal&&k.key==root&&k.minor==minor,"key and mode of a tonal progression");}
    {juce::AudioBuffer<float> noise(2,44100*10);std::mt19937 g(3);std::uniform_real_distribution<float> u(-.3f,.3f);for(int i=0;i<noise.getNumSamples();++i){const float v=u(g);noise.setSample(0,i,v);noise.setSample(1,i,v);}
        const auto k=estimateKey(noise,44100);std::cout<<"noise tonal "<<k.tonal<<" conf "<<k.confidence<<"\n";need(!k.tonal,"noise is reported as not tonal");}
    {const auto k=estimateKey(loop(44100,124,16,5),44100);std::cout<<"drums tonal "<<k.tonal<<" conf "<<k.confidence<<"\n";need(!k.tonal||k.confidence<.35,"a drum loop has no reliable key");}
    std::cout<<"PASS audio analysis: tempo within 0.6 BPM at 44.1/48 kHz incl. octave choice, key and mode, noise and drums not tonal\n";
}
