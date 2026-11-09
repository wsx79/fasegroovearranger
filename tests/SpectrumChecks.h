// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <thread>
inline void runSpectrumChecks(){
    fga::AudioEngine audio(fga::AudioEngine::Offline{});auto song=fga::newProject(0);song.sections={{fga::Section::main,8,"Spectrum"}};
    for(auto& t:song.tracks){t.clips.assign(1,{});t.engine.reverb=t.engine.chorus=0;}
    song.tracks[0].clips[0].notes={{0,32,60,100,0}};song.tracks[0].eq.enabled=true;song.tracks[0].eq.bands={{true,0,500,9,1}};song.tracks[0].eq.syncLegacyFromBands();song.normalise();audio.publish(song);audio.setSpectrumTrack(0);audio.play(true);
    std::atomic<bool> finished{false},bad{false};std::thread reader([&]{std::vector<float> pre,post;while(!finished.load()){audio.readSpectrum(0,pre,post,4096,true);if(pre.size()!=8192||post.size()!=8192)bad=true;for(float v:pre)if(!std::isfinite(v))bad=true;for(float v:post)if(!std::isfinite(v))bad=true;}});
    juce::AudioBuffer<float> block(2,512);juce::AudioIODeviceCallbackContext context;
    for(int i=0;i<120;++i)audio.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);
    finished=true;reader.join();check(!bad.load(),"concurrent spectrum capture/read has invalid or non-finite frames");
    std::vector<float> pre,post;audio.readSpectrum(0,pre,post,50000,true);check(pre.size()==8192&&post.size()==8192,"spectrum reads exceed fixed storage");
    double dry=0,wet=0;for(float v:pre)dry+=v*v;for(float v:post)wet+=v*v;check(dry>0&&std::abs(wet-dry)>1.e-6,"pre/post spectrum taps do not reflect the actual EQ audio");
    check(audio.spectrumRevision()>0&&audio.spectrumSampleRate()>=8000,"spectrum clock/rate unavailable");audio.setSpectrumTrack(-1);audio.readSpectrum(0,pre,post,4096,true);check(std::all_of(pre.begin(),pre.end(),[](float v){return v==0;}),"disabled spectrum exposes old track data");
    std::cout<<"PASS concurrent stereo pre/post spectrum, fixed storage and actual processing rate\n";
}
