// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
// Audio engine: edits while playing touch only what changed, loop wraps and stops release
// notes, publishing never waits for the audio thread, MP3 export is gapless.
#include "../Source/AudioEngine.h"
#include "../Source/core/Mp3Encoder.h"
#include <iostream>
#include <stdexcept>
static void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
int main(){
    juce::ScopedJuceInitialiser_GUI gui;
    try{
        fga::AudioEngine engine;engine.devices.closeAudioDevice(); // drive the callback by hand
        auto p=fga::newProject(1);p.sections={{fga::Section::main,4,"Main"}};p.normalise();
        for(auto& t:p.tracks)for(auto& c:t.clips)c.notes.clear();
        p.tracks[0].clips[0].notes={{0,12,60,100,0}};p.tracks[4].clips[0].notes={{0,.25,36,100,0}};
        engine.publish(p);engine.play(true);
        juce::AudioBuffer<float> out(2,512);juce::AudioIODeviceCallbackContext context;
        auto run=[&](int blocks){for(int b=0;b<blocks;++b)engine.audioDeviceIOCallbackWithContext(nullptr,0,out.getArrayOfWritePointers(),2,512,context);};
        while(engine.position()<2)run(1);
        check(engine.noteOnsSent(0)==1,"held piano note struck once");
        const auto kick=engine.noteOnsSent(4);
        p.tracks[5].clips[0].notes={{8,.25,38,100,0}};engine.publish(p);run(1);
        check(engine.noteOnsSent(0)==1&&engine.noteOnsSent(4)==kick,"editing another track does not retrigger held notes");
        p.tracks[0].clips[0].notes.push_back({.5,8,64,90,0});engine.publish(p);run(1);
        check(engine.noteOnsSent(0)==2,"only the added held note is struck");
        p.tracks[0].clips[0].notes={{0,12,60,70,0},{.5,8,64,90,0}};engine.publish(p);run(1);
        check(engine.noteOnsSent(0)==3,"a changed held note is restruck, the unchanged one keeps ringing");
        // Mix changes (fader, pan, mute) never restrike notes.
        p.tracks[0].gain=.3;p.tracks[0].pan=.5;engine.updateMix(p);run(2);
        check(engine.noteOnsSent(0)==3,"a fader move does not retrigger");
        // Several publishes before the audio thread runs: the newest wins, nothing leaks.
        for(int k=0;k<20;++k){p.tracks[6].clips[0].notes={{(double)k*.25,.25,39,100,0}};engine.publish(p);engine.collectGarbage();}
        run(1);check(engine.noteOnsSent(0)==3&&engine.isPlaying(),"burst of edits while playing");
        // The audio still sounds while edits stream in (no silent blocks from a busy lock).
        p.tracks[0].clips[0].notes={{0,16,60,100,0}};engine.publish(p);run(1);
        float level=0;for(int b=0;b<8;++b){p.tracks[0].gain=.5+.01*b;engine.updateMix(p);run(1);level=std::max(level,out.getMagnitude(0,512));}
        check(level>1e-4,"playback is audible while the mix is being edited");
        engine.stop();run(1);check(out.getMagnitude(0,512)==0,"stop is silent");
        // Loop: wrapping keeps playing and strikes the loop's notes again.
        p.tracks[0].clips[0].notes={{0,.5,60,100,0}};p.loop=true;p.loopStart=0;p.loopEnd=2;engine.publish(p);engine.setLoop(true,0,2);
        const auto beforeLoop=engine.noteOnsSent(0);engine.play(true);
        for(int b=0;b<200&&engine.noteOnsSent(0)<beforeLoop+3;++b)run(1);
        check(engine.isPlaying()&&engine.position()<2&&engine.noteOnsSent(0)>=beforeLoop+3,"loop wraps and replays its notes");
        engine.stop();run(1);
        // Gapless MP3: decoders that read the Info/LAME frame return exactly the WAV length.
        p.loop=false;
        auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("arranger-engine-tests","",false);dir.createDirectory();
        auto mp3=dir.getChildFile("song.mp3");auto r=engine.exportAudio(p,mp3,-1,true,[]{return false;},[](double){});check(r.wasOk(),"MP3 export");
        juce::MemoryBlock data;mp3.loadFileAsData(data);const auto* b=(const std::uint8_t*)data.getData();
        check(data.getSize()>2000&&juce::String((const char*)b+36,4)=="Info"&&juce::String((const char*)b+156,4)=="LAME","MP3 starts with the gapless Info/LAME frame");
        check(((b[177]<<4)|(b[178]>>4))==fga::Mp3Encoder::encoderDelay,"encoder delay recorded");
        const juce::int64 expected=(juce::int64)std::ceil((p.length()*60./p.settings.bpm+3.)*44100);
        if(juce::File("/usr/bin/ffmpeg").existsAsFile()){
            auto raw=dir.getChildFile("decoded.raw");juce::ChildProcess ffmpeg;
            check(ffmpeg.start(juce::StringArray{"/usr/bin/ffmpeg","-loglevel","error","-y","-i",mp3.getFullPathName(),"-f","s16le","-ac","2",raw.getFullPathName()}),"ffmpeg start");
            ffmpeg.waitForProcessToFinish(60000);check(raw.getSize()==expected*4,"decoded MP3 length equals the song exactly (no encoder delay/padding)");
        }
        dir.deleteRecursively();
        // After an export the engine plays again.
        engine.publish(p);engine.play(true);run(4);check(engine.isPlaying()&&engine.position()>0,"playback after export");
        std::cout<<"Audio engine resync, transport and gapless MP3 passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
