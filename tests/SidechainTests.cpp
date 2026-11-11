// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/AudioEngine.h"
#include "../Source/StudioIO.h"
#include "../Source/ProjectIO.h"
#include <iostream>
#include <stdexcept>
size_t countRealtimeCppAllocations(const std::function<void()>&);
namespace {
using namespace fga;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
struct Fixture {
    std::array<BusSettings,4> settings=defaultBuses();
    MixerBuses bus;
    juce::AudioBuffer<float> output;
    Fixture(double rate,int block):output(2,block){bus.prepare(rate,block);configure();}
    void configure(double bpm=120,bool snap=true){bus.configure(settings,bpm,1,snap);}
    void begin(int count,bool stem=false){output.clear();bus.begin(count,stem);}
    void add(int sample,int group=0,float value=.25f){bus.add(0,sample,value,group,{});bus.add(1,sample,value*.5f,group,{});}
    void finish(int count){bus.finish(output.getArrayOfWritePointers(),2,count,nullptr);}
};
void legacy(){
    int cases=0;
    for(double rate:{44100.,48000.,96000.})for(int block:{1,31,128,511}){
        Fixture f(rate,block);f.settings[0].sidechainAmount=.7;f.configure();float duck=0;
        const int total=(int)(rate*.4);const float coefficient=(float)std::exp(-1./(rate*.18));
        for(int at=0;at<total;at+=block){const int count=std::min(block,total-at);f.begin(count);
            for(int s=0;s<count;++s){const int t=at+s;const float source=t%997<5?(t%2?-.2f:.1f):0;f.bus.kick(s,source);f.bus.kick(s,-source*.3f);f.add(s);}
            f.finish(count);
            for(int s=0;s<count;++s){const int t=at+s;const float source=t%997<5?(t%2?-.2f:.1f):0;const float detector=std::min(1.f,std::abs(source)*6.f);duck=detector>duck?detector:duck*coefficient;
                const float expected=.25f*(1.f-.7f*duck);
                require(f.output.getSample(0,s)==expected&&f.output.getSample(1,s)==expected*.5f,"default ducking differs from the exact historical sample law");}
            f.configure(at<total/2?60:240,false);
        }++cases;
    }
    std::cout<<"Legacy detector/180ms decay/stereo/BPM/block reference PASS "<<cases<<" cases (sample exact)\n";
}
void controls(){
    Fixture f(48000,480);f.settings[0].sidechainAmount=.8;f.settings[0].sidechainAttackMs=20;f.settings[0].sidechainReleaseMs=100;
    f.settings[1].sidechainAmount=.5;f.settings[1].sidechainSource=1;f.configure();
    for(int block=0;block<2;++block){f.begin(480);for(int s=0;s<480;++s){f.bus.kick(s,-1);f.bus.source(2,s,1);f.add(s);f.add(s,1);}f.finish(480);}
    const float envelope=(2.f-f.output.getSample(0,479)/.25f)/.8f; // group B contributes .25, unducked
    require(std::abs(envelope-(1-std::exp(-1.)))<3.e-5,"20ms attack time constant");
    require(f.bus.reduction(1)==0,"unselected source must not trigger group B");
    f.settings[1].mute=true;f.settings[0].sidechainCurve=2;f.configure();
    const float before=.8f*envelope*envelope;
    for(int block=0;block<10;++block){f.begin(480);for(int s=0;s<480;++s)f.add(s);f.finish(480);}
    const float reduction=1-f.output.getSample(0,479)/.25f;
    require(std::abs(reduction-before*std::exp(-2.))<1.e-4,"release/curve shaping is applied once per linked stereo frame");
    f.bus.reset();f.settings[0].sidechainAttackMs=0;f.configure();f.begin(480);for(int s=0;s<480;++s){f.bus.source(1,s,1);f.add(s);}f.finish(480);
    require(f.output.getSample(0,479)==.25f,"reset/missing selected source produces no fabricated ducking");
    f.begin(480);f.bus.kick(-1,1);f.bus.kick(480,1);f.bus.kick(0,std::numeric_limits<float>::quiet_NaN());f.bus.kick(1,std::numeric_limits<float>::infinity());for(int s=0;s<480;++s)f.add(s);f.finish(480);
    require(f.output.getSample(0,0)==.25f&&f.output.getSample(0,1)==.25f,"invalid detector inputs are ignored");
    f.settings[0].sidechainCurve=1;f.settings[0].sidechainAmount=0;f.configure(120,true);float previous=.25f,step=0;
    f.settings[0].sidechainAmount=1;f.configure(120,false);f.begin(480);for(int s=0;s<480;++s){f.bus.kick(s,1);f.add(s);}f.finish(480);
    for(int s=0;s<480;++s){const float next=f.output.getSample(0,s);step=std::max(step,std::abs(next-previous));previous=next;}
    require(step<.0006f&&std::abs(previous-.25f*(1.f-(float)std::exp(-1./(48000*.1))))<1.e-7,"live depth changes use the existing 10ms smoothing convention");
    std::cout<<"Attack/release/curve, independent sources, silence/reset, invalid samples and depth ramp PASS\n";
}
void routing(){
    Fixture f(48000,64);f.settings[0].sidechainAmount=1;f.settings[0].solo=true;f.configure();
    f.begin(64);for(int s=0;s<64;++s){f.add(s,0);f.add(s,1);f.add(s,-1);}f.finish(64);
    require(f.output.getSample(0,0)==.25f,"group solo excludes dry and other group");
    f.settings[0].mute=true;f.configure();f.begin(64);for(int s=0;s<64;++s)f.add(s);f.finish(64);
    require(f.output.getMagnitude(0,64)==0,"group mute blocks its routed audio");
    f.settings[0].mute=false;f.settings[0].solo=false;f.settings[1].solo=true;f.configure();f.begin(64,true);for(int s=0;s<64;++s)f.add(s);f.finish(64);
    require(f.output.getSample(0,0)==.25f,"stem ignores bus solos and missing kick does not duck");
    f.bus.reset();f.settings[1].solo=false;f.settings[0].sidechainAmount=.75;f.settings[1].sidechainAmount=.5;f.settings[1].sidechainSource=1;f.configure();
    f.begin(64);for(int s=0;s<64;++s){f.bus.kick(s,1);f.bus.source(1,s,1);f.add(s);f.add(s,1);}f.finish(64);
    require(f.output.getSample(0,0)==.1875f&&f.bus.reduction(0)==.75f&&f.bus.reduction(1)==.5f,"two independently sourced groups and reduction meters");
    std::cout<<"Group routing, mute/solo, stem and linked reduction meters PASS\n";
}
void persistence(){
    static_assert(trackCount==18,"Update allowed stable sidechain sources when generated track IDs change");
    auto p=newProject(0);p.buses[0].sidechainAmount=.63;p.buses[0].sidechainAttackMs=12;p.buses[0].sidechainReleaseMs=420;p.buses[0].sidechainCurve=1.7;p.buses[0].sidechainSource=17;
    Project restored;require(projectFromJson(projectJson(p),restored).wasOk()&&restored.buses==p.buses,"extended sidechain project roundtrip");
    auto old=busesJson(p.buses);for(auto& b:*old.getArray())for(auto key:{"sidechainAttackMs","sidechainReleaseMs","sidechainCurve","sidechainSource"})b.getDynamicObject()->removeProperty(key);
    auto defaults=defaultBuses();require(readBuses(old,defaults).wasOk(),"legacy buses readable");
    require(defaults[0].sidechainAmount==.63&&defaults[0].sidechainSource==4&&defaults[0].sidechainAttackMs==0&&defaults[0].sidechainReleaseMs==180&&defaults[0].sidechainCurve==1,"old project defaults must preserve its ducking");
    auto invalid=busesJson(p.buses);auto* bus=(*invalid.getArray())[0].getDynamicObject();bus->setProperty("sidechainAttackMs",std::numeric_limits<double>::quiet_NaN());bus->setProperty("sidechainReleaseMs",-9);bus->setProperty("sidechainCurve",99);bus->setProperty("sidechainSource",9999999999.);
    require(readBuses(invalid,defaults).wasOk()&&defaults[0].sidechainAttackMs==0&&defaults[0].sidechainReleaseMs==10&&defaults[0].sidechainCurve==4&&defaults[0].sidechainSource==17,"malformed sidechain fields are finite and bounded before conversion");
    std::cout<<"Optional JSON fields, old defaults and invalid metadata PASS\n";
}
void realtime(){
    Fixture f(48000,512);f.settings[0].sidechainAmount=.7;f.settings[1].sidechainAmount=.6;f.settings[1].sidechainSource=1;f.settings[1].sidechainAttackMs=20;f.settings[1].sidechainReleaseMs=350;f.settings[1].sidechainCurve=1.5;f.configure();
    auto run=[&]{f.configure(140,false);f.begin(512);for(int s=0;s<512;++s){f.bus.kick(s,.1f);f.bus.source(1,s,.05f);f.add(s);f.add(s,1);}f.finish(512);};
    run();const auto allocations=countRealtimeCppAllocations([&]{for(int n=0;n<200;++n)run();});require(allocations==0,"sidechain/mixer configuration and processing allocate C++ memory");
    const double start=juce::Time::getMillisecondCounterHiRes();for(int n=0;n<2000;++n)run();const double elapsed=juce::Time::getMillisecondCounterHiRes()-start;
    std::cout<<"Local mixer/2 active groups incl curve: 1024000 stereo frames="<<elapsed<<" ms; regular C++ callback allocations="<<allocations<<"; detector buffers="<<2*512*sizeof(float)<<" bytes at block512 (was "<<512*sizeof(float)<<")\n";
}
void exportAndTrackGating(){
    auto project=newProject(0);project.settings.bpm=120;project.settings.freeTempo=true;project.sections={{Section::main,1,"Sidechain"}};project.normalise();project.masterGain=1;project.masterAutomation.enabled=false;
    for(auto& track:project.tracks){track.gain=0;track.pan=0;track.volume.enabled=false;track.sends={};for(auto& clip:track.clips)clip.notes.clear();}
    const auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fga-sidechain","",false);require(folder.createDirectory().wasOk(),"sidechain temp folder");struct Cleanup{juce::File folder;~Cleanup(){folder.deleteRecursively();}}cleanup{folder};
    const auto source=folder.getChildFile("source.wav");juce::WavAudioFormat wav;
    {std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(source.createOutputStream().release(),44100,2,24,{},0));juce::AudioBuffer<float> dc(2,88200);for(int c=0;c<2;++c)juce::FloatVectorOperations::fill(dc.getWritePointer(c),.1f,88200);require(writer&&writer->writeFromAudioSampleBuffer(dc,0,88200),"sidechain sample fixture");}
    AudioEngine engine(AudioEngine::Offline{});
    for(int t:{0,1}){auto& track=project.tracks[(size_t)t];track.gain=1;track.group=t;track.instrument="sample";track.instrumentPath=source.getFullPathName().toStdString();track.sampleRoot=60;track.sampler.mode=1;track.sampler.attack=track.sampler.decay=0;track.sampler.sustain=1;track.sampler.velocity=0;track.clips[0].notes={{0,4,60,127,0}};require(engine.loadSample(t,source,60,track.sampler).wasOk(),"load sidechain source/target");}
    project.buses[1].mute=true;project.buses[0].sidechainSource=1;project.buses[0].sidechainAmount=.8;
    auto render=[&](const char* name,int only=-1){const auto file=folder.getChildFile(name);require(engine.exportAudio(project,file,only,false,[]{return false;},[](double){}).wasOk(),"sidechain WAV export");std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(file.createInputStream().release(),true));juce::AudioBuffer<float> block(2,1000);require(reader&&reader->read(&block,0,1000,4410,true,true),"read sidechain WAV");return block.getRMSLevel(0,0,1000);};
    const float ducked=render("ducked.wav"),stem=render("stem.wav",0);require(stem>.05f&&std::abs(ducked/stem-.52f)<.002,"offline uses selected source before group mute; isolated stem has no source");
    project.tracks[1].mute=true;const float muted=render("muted.wav");require(std::abs(muted/stem-1)<.0001,"track mute excludes detector");
    project.tracks[1].mute=false;project.tracks[0].solo=true;const float solo=render("solo.wav");require(std::abs(solo/stem-1)<.0001,"track solo excludes non-solo detector");
    project.tracks[0].solo=false;project.tracks[1].panAutomation.enabled=true;project.tracks[1].panAutomation.points={{0,.25,0},{4,.75,0}};engine.publish(project);engine.play(true);juce::AudioBuffer<float> live(2,512);juce::AudioIODeviceCallbackContext context;for(int n=0;n<20;++n)engine.audioDeviceIOCallbackWithContext(nullptr,0,live.getArrayOfWritePointers(),2,512,context);
    require(std::abs(live.getRMSLevel(0,0,512)-ducked)<.0001,"live and offline share the same group ducking path");project.buses[0].sidechainAttackMs=10;project.buses[0].sidechainReleaseMs=350;project.buses[0].sidechainCurve=.75;project.buses[0].sidechainSource=0;project.buses[0].sidechainAmount=.7;engine.updateMix(project);
    const auto allocations=countRealtimeCppAllocations([&]{engine.audioDeviceIOCallbackWithContext(nullptr,0,live.getArrayOfWritePointers(),2,512,context);});
    require(allocations==0&&std::isfinite(live.getRMSLevel(0,0,512)),"new sidechain controls adopt on callback without C++ allocations");engine.stop();
    std::cout<<"WAV/live agreement (including pan-automated source), internal source routing, group mute pre-tap, track mute/solo and absent source in stem PASS; ducked/dry="<<ducked/stem<<"\n";
}
}
void runSidechainTests(){legacy();controls();routing();persistence();realtime();exportAndTrackGating();}
