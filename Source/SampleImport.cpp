// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SampleImport.h"
#include "StudioIO.h"
namespace fga {
SampleImportJob::SampleImportJob(juce::File f,SamplerSettings s,int r,bool d,double sr,SampleLoadLimits l,bool slice,bool pre)
    :Thread("Sample import"),file(f),settings(s),root(r),drum(d),rate(sr),limits(l),autoSlice(slice),preset(pre){}
void SampleImportJob::run(){
    juce::String expectedHash;
    try{
        if(preset){SamplerPreset p;result=loadSamplerPreset(file,p,false);if(result.wasOk()){file=p.file;settings=p.settings;root=p.root;expectedHash=p.hash;}}
        else result=juce::Result::ok();
        if(result.wasOk())result=readSampleAsset(file,asset,limits,[this]{return threadShouldExit();},[this](double v){progress=v*.8;});
        if(result.wasOk()&&expectedHash.isNotEmpty()&&asset->hash!=expectedHash)result=juce::Result::fail("Preset sample changed. Restore its media folder.");
        if(result.wasOk()&&!threadShouldExit()){
            if(autoSlice){const auto points=detectSampleSlices(*asset,0,1,.5,16);settings.slices.fill(0);settings.sliceCount=(int)points.size();std::copy(points.begin(),points.end(),settings.slices.begin());}
            if(analyse){tempo=estimateTempo(asset->audio,asset->rate);key=estimateKey(asset->audio,asset->rate);}
            settings.normalise();prepared=std::make_unique<SampleSynth>(asset,settings,std::clamp(root,0,127),drum);prepared->setCurrentPlaybackSampleRate(rate);progress=1;
        }
    }catch(const std::exception& e){result=juce::Result::fail("Sample import failed: "+juce::String(e.what()));}
    if(threadShouldExit())result=juce::Result::fail("Sample import cancelled.");
    ready.store(true,std::memory_order_release);
    while(!threadShouldExit()&&!finish.wait(20)){}
    retired.reset();prepared.reset();asset.reset(); // large frees stay off the UI/audio threads
}
}
