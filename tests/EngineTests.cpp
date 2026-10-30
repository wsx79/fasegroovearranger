// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
// Audio engine: edits while playing touch only what changed, loop wraps and stops release
// notes, publishing never waits for the audio thread, MP3 export is gapless.
#include "../Source/AudioEngine.h"
#include "../Source/ExportSupport.h"
#include "../Source/ArrangementAudit.h"
#include "../Source/core/Mp3Encoder.h"
#include <iostream>
#include <stdexcept>
static void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
void runStudioEngineTests();
void runEngineAuditTests();
int main(int argc,char** argv){
    juce::ScopedJuceInitialiser_GUI gui;
    try{
        if(argc==3&&juce::String(argv[1])=="--club-audit")return fga::writeClubAudioAudit(juce::File(argv[2]))?0:1;
        runStudioEngineTests();runEngineAuditTests();
        fga::AudioEngine engine;engine.devices.closeAudioDevice(); // drive the callback by hand
        {
            auto manual=fga::newProject(1);manual.sections={{fga::Section::main,4,"Main"}};manual.normalise();
            for(auto& t:manual.tracks)for(auto& c:t.clips)c.notes.clear();int ti=manual.addMidiTrack();manual.tracks[(size_t)ti].clips[0].notes={{0,1,72,110,0}};
            engine.publish(manual);engine.play(true);juce::AudioBuffer<float> buffer(2,512);juce::AudioIODeviceCallbackContext context;float peak=0;
            for(int block=0;block<48;++block){engine.audioDeviceIOCallbackWithContext(nullptr,0,buffer.getArrayOfWritePointers(),2,512,context);peak=std::max(peak,buffer.getMagnitude(0,512));}
            check(peak>.001f&&engine.noteOnsSent(ti)>0,"handwritten MIDI track produces guide audio");engine.stop();
        }
        {
            auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fga-crash-tail","",false);dir.createDirectory();const auto sample=dir.getChildFile("Crash.wav"),exported=dir.getChildFile("Crash-export.wav");
            juce::WavAudioFormat wav;{std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(sample.createOutputStream().release(),44100,2,16,{},0));check(writer!=nullptr,"create crash sample");juce::AudioBuffer<float> source(2,52920);for(int ch=0;ch<2;++ch)for(int n=0;n<source.getNumSamples();++n)source.setSample(ch,n,.4f*std::sin(n*.071f)*(1.f-n/52920.f));writer->writeFromAudioSampleBuffer(source,0,source.getNumSamples());}
            auto crash=fga::newProject(1);crash.sections={{fga::Section::main,4,"Main"}};crash.normalise();crash.masterAutomation.enabled=false;for(auto& t:crash.tracks){t.volume.enabled=false;for(auto& c:t.clips)c.notes.clear();}
            crash.tracks[16].clips[0].notes={{0,.25,49,110,12}};crash.tracks[16].instrument="sample";crash.tracks[16].instrumentPath=sample.getFullPathName().toStdString();crash.tracks[16].sampleRoot=49;
            check(engine.loadSample(16,sample,49).wasOk(),"load real Crash sample");engine.publish(crash);engine.play(true);juce::AudioBuffer<float> block(2,512);juce::AudioIODeviceCallbackContext context;float tail=0;
            for(int n=0;n<80;++n){engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);if(n>=40)tail=std::max(tail,block.getMagnitude(0,512));}
            check(tail>.001f,"short handwritten drum notes preserve the sample's natural tail");engine.stop();engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);check(block.getMagnitude(0,512)==0,"forced stop silences the ringing Crash sample");
            auto copy=engine.offlineCopy();check(copy!=nullptr,"copy drum sampler");check(copy->exportAudio(crash,exported,16,false,[]{return false;},[](double){}).wasOk(),"export Crash sample tail");
            juce::AudioFormatManager formats;formats.registerBasicFormats();{std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(exported));check(reader!=nullptr,"read exported crash");juce::AudioBuffer<float> audio(2,4096);reader->read(&audio,0,4096,26460,true,true);check(audio.getMagnitude(0,4096)>.001f,"private-copy export retains the complete Crash tail after note-off");}
            engine.useInternal(16);copy.reset();dir.deleteRecursively();
        }
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
        // Export encoding, tempo metadata and filename/arrangement interchange contract.
        {auto named=p;named.settings.bpm=128.5;named.settings.freeTempo=true;named.settings.key=1;named.settings.scale=0;named.sections={{fga::Section::intro,16,"Intro"},{fga::Section::main,32,"Jackin groove"}};named.normalise();
            check(fga::exportSuffix(named)=="_128.5bpm_Db","filename preserves decimal BPM and flat key spelling");
            check(fga::exportNamedFile(dir.getChildFile("Piano.mid"),named,"mid").getFileName()=="Piano_128.5bpm_Db.mid","MIDI includes tempo and key");
            check(fga::exportNamedFile(dir.getChildFile("Piano_128.5bpm_Db.mid"),named,"mid").getFileName()=="Piano_128.5bpm_Db.mid","filename suffix is not duplicated");
            check(fga::exportTime(90.1234)=="01:30.123"&&fga::exportTime(3601.001)=="01:00:01.001","arrangement timestamps retain milliseconds and hours");
            const auto text=fga::exportArrangementText(named);check(text.contains("00:00.000 - "+fga::exportTime(64*60./128.5)+"  Intro")&&text.contains(fga::exportTime(64*60./128.5)+" - "+fga::exportTime(192*60./128.5)+"  Jackin groove"),"section ranges derive from beats at the actual tempo");
            for(auto settings:{fga::AudioExportOptions{44100,16},fga::AudioExportOptions{48000,24},fga::AudioExportOptions{96000,24}}){auto file=dir.getChildFile("encoding-"+juce::String(settings.sampleRate,0)+".wav");check(engine.exportAudio(p,file,0,false,[]{return false;},[](double){},nullptr,settings).wasOk(),"export requested WAV encoding");juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(file.createInputStream().release(),true));check(reader&&reader->sampleRate==settings.sampleRate&&reader->bitsPerSample==(unsigned)settings.bitDepth,"WAV header records selected sample rate and depth");check(std::abs(reader->metadataValues.getValue(juce::WavAudioFormat::acidTempo,"").getDoubleValue()-p.settings.bpm)<.001&&reader->metadataValues.getValue(juce::WavAudioFormat::acidOneShot,"")=="1","tempo metadata preserves full-song tails without declaring an incorrect loop");juce::AudioBuffer<float> samples(2,8192);reader->read(&samples,0,8192,4096,true,true);check(samples.getMagnitude(0,8192)>1e-4,"encoded WAV contains audible music");}
            auto cancelled=dir.getChildFile("cancelled.wav");check(engine.exportAudio(p,cancelled,0,false,[]{return true;},[](double){},nullptr,{48000,16}).failed()&&!cancelled.existsAsFile()&&!engine.isExporting(),"cancelled export restores the engine and leaves no partial WAV");
            check(engine.exportAudio(p,dir.getChildFile("invalid.wav"),0,false,[]{return false;},[](double){},nullptr,{48000,12}).failed()&&!engine.isExporting(),"invalid encoding leaves engine available");
        }
        dir.deleteRecursively();
        // After an export the engine plays again.
        engine.publish(p);engine.play(true);run(4);check(engine.isPlaying()&&engine.position()>0,"playback after export");
        // Export on a private copy: the live engine keeps playing, untouched, and the copy's
        // sampler (shared sample data) renders the same audio as the live one.
        {
            auto sampleDir=juce::File::createTempFile("fga-sample");sampleDir.createDirectory();auto wav=sampleDir.getChildFile("hit.wav");
            {juce::WavAudioFormat wf;std::unique_ptr<juce::AudioFormatWriter> w(wf.createWriterFor(wav.createOutputStream().release(),44100,1,16,{},0));juce::AudioBuffer<float> hit(1,4410);for(int s=0;s<4410;++s)hit.setSample(0,s,(float)std::sin(s*.07)*(1.f-s/4410.f));w->writeFromAudioSampleBuffer(hit,0,4410);}
            check(engine.loadSample(4,wav,36).wasOk(),"sample loads off the audio thread");
            engine.publish(p);engine.play(true);run(2);const double before=engine.position();
            auto copy=engine.offlineCopy();check(copy!=nullptr,"offline copy of built-in and sampler tracks");
            auto a=sampleDir.getChildFile("live.wav"),b=sampleDir.getChildFile("copy.wav");
            check(copy->exportAudio(p,b,4,false,[]{return false;},[](double){}).wasOk(),"export on the private copy");
            check(engine.isPlaying()&&engine.position()==before,"the live engine keeps playing during a private export");
            run(1);check(engine.position()>before,"and still advances");
            check(engine.exportAudio(p,a,4,false,[]{return false;},[](double){}).wasOk(),"reference export");
            juce::AudioFormatManager fm;fm.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> ra(fm.createReaderFor(a)),rb(fm.createReaderFor(b));
            check(ra&&rb&&ra->lengthInSamples==rb->lengthInSamples,"same export length");
            juce::AudioBuffer<float> ba(2,(int)ra->lengthInSamples),bb(2,(int)rb->lengthInSamples);ra->read(&ba,0,ba.getNumSamples(),0,true,true);rb->read(&bb,0,bb.getNumSamples(),0,true,true);
            float diff=0;for(int ch=0;ch<2;++ch)for(int s=0;s<ba.getNumSamples();++s)diff=std::max(diff,std::abs(ba.getSample(ch,s)-bb.getSample(ch,s)));
            check(diff<1e-6f&&ba.getMagnitude(0,0,ba.getNumSamples())>1e-3f,"the private export equals the live engine's export");
            ra.reset();rb.reset();sampleDir.deleteRecursively();engine.useInternal(4);
        }
        // Sample browser audition: plays with the transport stopped, stops at once.
        {
            engine.stop();run(1);juce::AudioBuffer<float> tone(1,44100);for(int s=0;s<44100;++s)tone.setSample(0,s,.5f*(float)std::sin(s*.05));
            engine.startPreview(std::move(tone),44100);run(1);check(engine.isPreviewing()&&out.getMagnitude(0,512)>.1f&&out.getMagnitude(1,512)>.1f,"preview sounds on both sides while stopped");
            engine.stopPreview();run(1);check(!engine.isPreviewing()&&out.getMagnitude(0,512)==0,"preview stops at once");
            juce::AudioBuffer<float> blip(2,64);blip.clear();blip.setSample(0,0,1);engine.startPreview(std::move(blip),44100);run(2);check(!engine.isPreviewing(),"a finished preview reports it");
        }
        // Delay compensation line: a track is delayed by exactly the difference in latency.
        {
            fga::Instrument track(4);track.delay=100;juce::AudioBuffer<float> block(2,256);block.clear();block.setSample(0,3,1);block.setSample(1,3,-1);
            track.applyDelay(block,256);check(block.getSample(0,103)==1&&block.getSample(1,103)==-1&&block.getMagnitude(0,0,103)==0,"delay line shifts by the latency difference");
            block.clear();track.applyDelay(block,256);check(block.getMagnitude(0,256)==0,"nothing left behind");
            track.reset();check(!track.delayUsed,"reset clears the line");
        }
        std::cout<<"Audio engine resync, transport and gapless MP3 passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
