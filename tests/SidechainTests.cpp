// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/AudioEngine.h"
#include "../Source/StudioIO.h"
#include "../Source/ProjectIO.h"
#include "../Source/NativeCompressor.h"
#include "../Source/NativeReturnEffects.h"
#include "../Source/CompressorDSP.h"
#include <iostream>
#include <stdexcept>
void runReturnPdcTests();
void installEffectForReturnTests(fga::AudioEngine&,int,std::unique_ptr<juce::AudioPluginInstance>,int,const juce::PluginDescription&);
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
void liveEdits(){
    double worst=0;int cases=0;
    for(double rate:{44100.,48000.,96000.})for(int block:{32,512})for(int edit=0;edit<4;++edit){
        Fixture f(rate,block);f.settings[0].sidechainAmount=.8;
        f.settings[0].sidechainAttackMs=edit==1?200:0;f.settings[0].sidechainReleaseMs=2000;f.configure();
        int position=0;float previous=0;double first=0,maximum=0;
        auto tick=[&](bool changed){f.begin(block);for(int s=0;s<block;++s){f.bus.source(4,s,changed&&edit==3?0.f:edit==0?.04f:.16f);f.bus.source(1,s,.16f);f.add(s);}f.finish(block);
            for(int s=0;s<block;++s){const float next=f.output.getSample(0,s);if(changed){const double jump=std::abs(next-previous);if(position==0)first=jump;maximum=std::max(maximum,jump);require(std::abs(f.output.getSample(1,s)-next*.5f)<1.e-7,"edited sidechain remains stereo linked");++position;}previous=next;}};
        // Start a slow attack only shortly before the edit; others reach steady state.
        const int warm=(int)(rate*(edit==1?.01:.1));for(int n=0;n<warm;n+=block)tick(false);
        if(edit==0)f.settings[0].sidechainSource=1;
        if(edit==1)f.settings[0].sidechainAttackMs=0;
        if(edit==2)f.settings[0].sidechainCurve=4;
        if(edit==3)f.settings[0].sidechainReleaseMs=10;
        f.configure(120,false);for(int n=0;n<(int)(rate*.02);n+=block)tick(true);
        std::cout<<"Live edit rate="<<rate<<" block="<<block<<" kind="<<edit<<" first_step="<<first<<" max_step="<<maximum<<std::endl;
        worst=std::max(worst,maximum);++cases;
    }
    Fixture rapid(48000,32);rapid.settings[0].sidechainAmount=.8;rapid.configure();float previous=.25f;
    for(int n=0;n<200;++n){rapid.settings[0].sidechainSource=n%2?1:4;rapid.settings[0].sidechainCurve=n%2?4:.25;rapid.settings[0].sidechainAttackMs=n%2?0:100;rapid.configure(120,false);rapid.begin(32);for(int s=0;s<32;++s){rapid.bus.source(4,s,.04f);rapid.bus.source(1,s,.16f);rapid.add(s);}rapid.finish(32);if(n>0)require(std::abs(rapid.output.getSample(0,0)-previous)<.001,"rapid edits must splice from the last applied reduction");for(int s=0;s<32;++s)require(rapid.output.getSample(0,s)>=0&&rapid.output.getSample(0,s)<=.25f,"edit correction must not amplify or invert gain");previous=rapid.output.getSample(0,31);}
    require(worst<.001,"sidechain source/timing/curve edit creates an avoidable gain discontinuity");
    std::cout<<"Live detector edits PASS "<<cases<<" cases; max carrier step="<<worst<<" (DC amplitude .25)\n";
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
void integratedInserts(AudioEngine& engine,Project project,const juce::File& folder){
    const auto description=nativeCompressorDescription();bool installed=true;int completed=0;
    struct Deadline final:juce::Timer{bool expired=false;Deadline(){startTimer(5000);}void timerCallback()override{expired=true;juce::MessageManager::getInstance()->stopDispatchLoop();}} deadline;
    for(int track:{0,masterEffectTrack})engine.loadEffect(track,description,[&](juce::Result result){installed=installed&&result.wasOk();if(++completed==2)juce::MessageManager::getInstance()->stopDispatchLoop();},1);
    juce::MessageManager::getInstance()->runDispatchLoop();deadline.stopTimer();require(!deadline.expired&&installed&&completed==2,"integrated native track/master install");
    EffectSlot effect;effect.slot=1;effect.path=description.fileOrIdentifier.toStdString();effect.pluginName=description.name.toStdString();effect.pluginId=description.createXml()->toString().toStdString();
    project.tracks[0].effects={effect};project.masterEffects.effects={effect};project.tracks[0].gain=2;
    auto set=[&](int track,int index,float value){engine.effectPlugin(track,1)->getParameters()[index]->setValue((value-CompressorDSP::minimum[(size_t)index])/(CompressorDSP::maximum[(size_t)index]-CompressorDSP::minimum[(size_t)index]));};
    set(0,0,-30);set(0,1,2);set(0,5,12);set(masterEffectTrack,1,1);set(masterEffectTrack,5,12);
    auto& group=project.buses[0];group.sidechainSource=1;group.sidechainAmount=.8;group.sidechainAttackMs=20;group.sidechainReleaseMs=180;group.sidechainCurve=1;
    ParameterAutomation lane;lane.slot=1;lane.index=0;lane.parameterId="threshold";lane.curve.enabled=true;lane.curve.points={{0,.4,0},{4,.6,0}};project.tracks[0].parameters={lane};
    engine.capturePluginStates(project);const auto saved=folder.getChildFile("integrated.fga");require(saveProject(project,saved).wasOk(),"save combined mixer/insert project");Project reopened;require(loadProject(saved,reopened).wasOk()&&reopened.buses==project.buses&&reopened.tracks[0].effects[0].state==project.tracks[0].effects[0].state&&reopened.tracks[0].parameters[0].curve.points==project.tracks[0].parameters[0].curve.points,"reopen sidechain/native state and automation together");
    engine.publish(reopened);engine.play(true);juce::AudioBuffer<float> block(2,512);juce::AudioIODeviceCallbackContext context;
    auto callback=[&]{engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);};
    for(int i=0;i<20;++i)callback();require(nativeCompressorReduction(engine.effectPlugin(0,1))>0,"compression and group ducking active together");
    auto copy=engine.offlineCopy();require(copy!=nullptr,"combined insert/source graph clones offline");
    const auto output=folder.getChildFile("integrated.wav");double rawPeak=0;int ticks=0;
    require(copy->exportAudio(reopened,output,-1,false,[]{return false;},[&](double){if(ticks++<10)callback();},&rawPeak,{48000,24,AudioExportMode::transparent}).wasOk(),"combined Transparent export");
    require(rawPeak>juce::Decibels::decibelsToGain(-1.)&&engine.isPlaying()&&engine.position()>0,"Transparent handles combined headroom without taking live transport ownership");
    juce::MemoryBlock before,after;require(output.loadFileAsData(before),"load previous combined export");bool cancel=false;
    require(copy->exportAudio(reopened,output,-1,false,[&]{return cancel;},[&](double progress){if(progress>=1)cancel=true;},nullptr,{44100,16,AudioExportMode::transparent}).failed()&&cancel&&output.loadFileAsData(after)&&before==after&&!copy->isExporting(),"late cancellation preserves integrated export and releases ownership");
    require(copy->exportAudio(reopened,output,-1,false,[]{return false;},[](double){},nullptr,{48000,16,AudioExportMode::transparent,-3}).wasOk(),"combined atomic overwrite with different PCM depth/ceiling");
    juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(output.createInputStream().release(),true));require(reader&&reader->sampleRate==48000&&reader->bitsPerSample==16,"combined export format");
    // Retain just this new integration fixture for independent 32x QA when requested.
    const auto audit=juce::SystemStats::getEnvironmentVariable("FGA_MIX_INTEGRATION_DIR",{});if(audit.isNotEmpty()){const juce::File directory(audit);require(directory.createDirectory().wasOk()&&output.copyFileTo(directory.getChildFile("integrated-48000-16.wav")),"retain integrated QA WAV");}
    const auto blocked=folder.getChildFile("blocked.wav");require(blocked.createDirectory().wasOk()&&copy->exportAudio(reopened,blocked,-1,false,[]{return false;},[](double){}).failed()&&!copy->isExporting(),"combined output error restores export state");
    std::array<double,100> callbackMs{};size_t activeAllocations=0;
    for(auto& duration:callbackMs){const double start=juce::Time::getMillisecondCounterHiRes();activeAllocations+=countRealtimeCppAllocations(callback);duration=juce::Time::getMillisecondCounterHiRes()-start;}
    require(activeAllocations==0&&engine.isPlaying(),"active integrated playback remains finite and allocation-free");std::sort(callbackMs.begin(),callbackMs.end());
    std::cout<<"Local integrated callback, 512/"<<engine.spectrumSampleRate()<<": median_ms="<<callbackMs[50]<<" p99_ms="<<callbackMs[98]<<" max_ms="<<callbackMs[99]<<" (100 calls; not hardware dropout validation)\n";
    project=reopened;project.tracks[0].effects[0].bypassed=true;project.masterEffects.effects[0].bypassed=true;project.buses[0].sidechainSource=4;project.buses[0].sidechainCurve=2;engine.updateMix(project);
    size_t allocations=0;for(int i=0;i<10;++i)allocations+=countRealtimeCppAllocations(callback);
    require(allocations==0&&std::isfinite(block.getRMSLevel(0,0,512))&&nativeCompressorReduction(engine.effectPlugin(0,1))==0&&engine.latencySamples()==0,"combined bypass/control adoption has zero C++ allocations and no added PDC");engine.stop();
    std::cout<<"Integrated track/master compression + sidechain + automation + project reopen + private Transparent export/late cancel/overwrite/error + live bypass PASS; raw peak="<<rawPeak<<"; callback allocations="<<allocations<<"\n";
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
    integratedInserts(engine,project,folder);
    std::cout<<"WAV/live agreement (including pan-automated source), internal source routing, group mute pre-tap, track mute/solo and absent source in stem PASS; ducked/dry="<<ducked/stem<<"\n";
}
void detectorAlignment(){
    double maximum=0;size_t allocations=0;
    for(double rate:{44100.,48000.,96000.})for(auto delays:{std::pair<int,int>{0,137},{137,0},{53,137},{137,137}}){
        const int target=delays.first,source=delays.second,aligned=std::max(target,source);auto processor=createNativeCompressor();auto values=CompressorDSP::Values{-24,3,0,.1f,150,0};
        for(int i=0;i<6;++i)processor->getParameters()[i]->setValue((values[(size_t)i]-CompressorDSP::minimum[(size_t)i])/(CompressorDSP::maximum[(size_t)i]-CompressorDSP::minimum[(size_t)i]));
        processor->getParameters()[6]->setValue(2.f/maxTracks);require(!processor->getParameters()[6]->isAutomatable(),"source routing remains a control-thread edit");processor->setRateAndBufferSizeDetails(rate,512);processor->prepareToPlay(rate,512);prepareNativeCompressorRouting(processor.get(),target,source);
        require(processor->getLatencySamples()==std::max(0,source-target)&&nativeCompressorDetectorSource(processor.get())==1,"detector routing reports actual added input delay");
        CompressorDSP reference;reference.prepare(rate,values);reference.setBypassed(false);juce::AudioBuffer<float> block(2,512),expected(2,512);std::array<float,512> detector{},alignedDetector{};juce::MidiBuffer midi;
        for(int n=0;n<12;++n){if(n==4){processor->getParameters()[6]->setValue(3.f/maxTracks);prepareNativeCompressorRouting(processor.get(),target,source);require(nativeCompressorDetectorSource(processor.get())==2,"control routing adopts the new source identity");}if(n==6){setNativeCompressorBypass(processor.get(),true);reference.setBypassed(true);}for(int j=0;j<512;++j){const int time=n*512+j;
            auto signal=[&](int delay){const int at=time-delay;return at>=400?(float)(.25*std::sin(juce::MathConstants<double>::twoPi*997*at/rate)):0.f;};
            for(int c=0;c<2;++c){block.setSample(c,j,signal(target)*(c==0?1.f:.37f));expected.setSample(c,j,signal(aligned)*(c==0?1.f:.37f));}
            detector[(size_t)j]=time-source>=400?.7f:0;alignedDetector[(size_t)j]=time-aligned>=400?.7f:0;}
            setNativeCompressorDetector(processor.get(),detector.data());allocations+=countRealtimeCppAllocations([&]{processor->processBlock(block,midi);});reference.process(expected.getWritePointer(0),expected.getWritePointer(1),512,alignedDetector.data());
            for(int c=0;c<2;++c)for(int j=0;j<512;++j)maximum=std::max(maximum,std::abs((double)block.getSample(c,j)-expected.getSample(c,j)));
        }
    }
    require(maximum<1e-6&&allocations==0,"detector/input delay compensation must match aligned reference without callback allocation, including bypass");
    std::cout<<"External detector alignment 12 rate/latency cases + same-latency source switch + bypass PASS; maximum residual="<<maximum<<"; C++ allocations="<<allocations<<"\n";
}
void returnModeReset(){
    Fixture f(44100,512);f.settings[3].division=.125;f.settings[3].feedback=0;f.configure();f.begin(512);for(int c=0;c<2;++c)f.bus.add(c,0,.25f,-1,{{0,1}});f.finish(512);
    const auto allocations=countRealtimeCppAllocations([&]{f.bus.configure(f.settings,120,1,true,true);f.bus.configure(f.settings,120,1,true,false);});
    for(int n=0;n<8;++n){f.begin(512);f.finish(512);require(f.output.getMagnitude(0,512)==0,"return mode changes must not replay stale fixed-delay audio");}
    require(allocations==0,"return mode reset allocates on callback");std::cout<<"Legacy/editable return mode reset: no stale delayed audio, C++ allocations=0 PASS\n";
}
void tempoReturns(){
    struct Head final : juce::AudioPlayHead{double bpm=120;juce::Optional<PositionInfo> getPosition()const override{PositionInfo p;p.setBpm(bpm);return p;}};
    int cases=0;size_t allocations=0;
    for(double rate:{44100.,48000.,96000.})for(double bpm:{90.,120.,150.,300.})for(double division:{.125,.25,.5,1.,2.,4.}){
        Head head;head.bpm=bpm;auto effect=createNativeReturn(nativeReturnDescription(true));effect->setPlayHead(&head);effect->getParameters()[1]->setValue(0);effect->getParameters()[0]->setValue(effect->getParameters()[0]->getValueForText(juce::String(division,3)));effect->setRateAndBufferSizeDetails(rate,512);effect->prepareToPlay(rate,512);
        const float delay=(float)(rate*60/bpm*division),fraction=delay-std::floor(delay);const int expected=(int)std::floor(delay);require(std::abs(effect->getTailLengthSeconds()-division*60/bpm)<1e-7,"delay tail reads project tempo before first processing");
        juce::AudioBuffer<float> block(2,512);juce::MidiBuffer midi;int first=-1;
        for(int base=0;base<expected+1024;base+=512){block.clear();if(base==0){block.setSample(0,0,.25f);block.setSample(1,0,.125f);}allocations+=countRealtimeCppAllocations([&]{effect->processBlock(block,midi);});for(int n=0;n<512;++n)if(std::abs(block.getSample(0,n))>1e-7f&&first<0){first=base+n;require(std::abs(block.getSample(0,n)-.25f*(1-fraction))<1e-7f&&std::abs(block.getSample(1,n)-.125f*(1-fraction))<1e-7f,"delay preserves fractional impulse interpolation and stereo ratio");}}
        require(first==expected,"return delay must land on the correct project sample, including short startup divisions");++cases;
    }
    require(allocations==0,"tempo return processing allocates");std::cout<<"Tempo delay 72 rate/BPM/division cases PASS; exact first echo, fractional interpolation and tempo-aware tails; C++ allocations="<<allocations<<"\n";
}
void editableReturns(){
    auto project=newProject(0);project.settings.bpm=120;project.settings.freeTempo=true;project.sections={{Section::main,1,"Editable returns"}};project.normalise();project.masterGain=1;project.masterAutomation.enabled=false;
    for(auto& track:project.tracks){track.gain=0;track.pan=0;track.panAutomation.enabled=false;track.volume.enabled=false;track.sends={};track.engine.reverb=track.engine.chorus=0;for(auto& clip:track.clips)clip.notes.clear();}
    require(!project.returnChains,"historic core fixture remains in the fixed return path");
    project.enableReturnChains();project.normalise();Project reopened;
    require((int)projectJson(project)["version"]==7&&projectFromJson(projectJson(project),reopened).wasOk()&&reopened.returnChains&&reopened.returnEffects[0].effects.size()==1&&reopened.returnEffects[1].effects.size()==1,"editable return defaults persist without migrating historic projects");
    auto invalid=projectJson(project);invalid.getDynamicObject()->setProperty("returnChains",juce::Array<juce::var>{});require(projectFromJson(invalid,reopened).failed(),"malformed return chains rejected");
    auto legacy=project;legacy.returnChains=false;for(auto& rack:legacy.returnEffects)rack.effects.clear();require(projectFromJson(projectJson(legacy),reopened).wasOk()&&!reopened.returnChains,"historic projects retain fixed returns");
    auto oldCompressor=nativeCompressorDescription();oldCompressor.version="1";EffectSlot legacyEffect;legacyEffect.path=oldCompressor.fileOrIdentifier.toStdString();legacyEffect.pluginId=oldCompressor.createXml()->toString().toStdString();legacy.tracks[0].effects={legacyEffect};
    require((int)projectJson(legacy)["version"]<7,"unmodified historic compressor metadata retains historic format");legacy.tracks[0].effects[0].pluginId=nativeCompressorDescription().createXml()->toString().toStdString();const auto modern=projectJson(legacy);
    require((int)modern["version"]==7&&(bool)modern["effectRouting"]&&projectFromJson(modern,reopened).wasOk()&&!reopened.returnChains,"current compressor format gate does not convert historic return DSP");
    const auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fga-editable-returns","",false);require(folder.createDirectory().wasOk(),"return fixture folder");struct Cleanup{juce::File file;~Cleanup(){file.deleteRecursively();}}cleanup{folder};
    AudioEngine engine(AudioEngine::Offline{});juce::WavAudioFormat wav;
    for(int t=0;t<2;++t){const auto file=folder.getChildFile("source"+juce::String(t)+".wav");{std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(file.createOutputStream().release(),44100,2,24,{},0));juce::AudioBuffer<float> dc(2,88200);for(int ch=0;ch<2;++ch)juce::FloatVectorOperations::fill(dc.getWritePointer(ch),t==0?.1f:.5f,88200);require(writer&&writer->writeFromAudioSampleBuffer(dc,0,88200),"write detector fixture");}
        auto& track=project.tracks[(size_t)t];track.gain=1;track.instrument="sample";track.instrumentPath=file.getFullPathName().toStdString();track.sampleRoot=60;track.sampler.mode=1;track.sampler.attack=track.sampler.decay=0;track.sampler.sustain=1;track.sampler.velocity=0;track.clips[0].notes={{0,4,60,127,0}};require(engine.loadSample(t,file,60,track.sampler).wasOk(),"load return fixture");}
    project.tracks[1].group=1;project.buses[1].mute=true;
    auto install=[&](int track,const juce::PluginDescription& description,int slot){installEffectForReturnTests(engine,track,isNativeCompressor(description)?createNativeCompressor():createNativeReturn(description),slot,description);require(engine.effectPlugin(track,slot)!=nullptr,"install native return chain effect");auto& rack=project.effectTarget(track);if(!rack.effect(slot)){EffectSlot added;added.slot=slot;rack.effects.push_back(added);}auto& fx=*rack.effect(slot);fx.pluginName=description.name.toStdString();fx.path=description.fileOrIdentifier.toStdString();fx.pluginId=description.createXml()->toString().toStdString();};
    install(returnEffectTrack,nativeReturnDescription(false),1);install(returnEffectTrack+1,nativeReturnDescription(true),1);install(0,nativeCompressorDescription(),1);
    auto* compressor=engine.effectPlugin(0,1);require(compressor&&compressor->getParameters().size()==8,"compressor keeps six historic controls, source and mix");
    compressor->getParameters()[0]->setValueNotifyingHost(.5f); // -30 dB
    compressor->getParameters()[6]->setValueNotifyingHost(2.f/maxTracks); // source track 1, rendered after target 0
    project.tracks[0].sends={.5,.5};engine.publish(project);engine.play(true);
    juce::AudioBuffer<float> block(2,512);juce::AudioIODeviceCallbackContext context;auto callback=[&]{block.clear();engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);};
    for(int n=0;n<100;++n)callback();
    const float reduction=nativeCompressorReduction(compressor);const double expected=CompressorDSP::reduction(20*std::log10(.5),-30,3,6);
    require(std::abs(reduction-expected)<.03,"selected later track drives compressor without track-order dependence");
    require(engine.busLevel(2)>.0001f&&engine.busLevel(3)>.0001f,"both native return chains produce wet audio");
    size_t allocations=0;const double start=juce::Time::getMillisecondCounterHiRes();for(int n=0;n<100;++n)allocations+=countRealtimeCppAllocations(callback);const double elapsed=juce::Time::getMillisecondCounterHiRes()-start;
    require(allocations==0&&engine.latencySamples()==0,"native return chains and detector introduce no callback allocations or latency");
    engine.stop();engine.capturePluginStates(project);require(!project.returnEffects[0].effects[0].state.empty()&&!project.returnEffects[1].effects[0].state.empty(),"return states captured");
    require(projectFromJson(projectJson(project),reopened).wasOk()&&reopened.returnEffects[0].effects[0].state==project.returnEffects[0].effects[0].state,"return states round trip");
    auto copy=engine.offlineCopy();require(copy&&copy->effectPlugin(returnEffectTrack,1)&&copy->effectPlugin(returnEffectTrack+1,1)&&nativeCompressorSource(copy->effectPlugin(0,1))==1,"private export clones return chains and source");
    AudioExportOptions options;options.mode=AudioExportMode::transparent;const auto output=folder.getChildFile("returns.wav");require(copy->exportAudio(reopened,output,-1,false,[]{return false;},[](double){},nullptr,options).wasOk()&&output.getSize()>1000,"editable returns export actual WAV");
    juce::MemoryBlock preserved;require(output.loadFileAsData(preserved),"capture existing return WAV");bool lateCancel=false;require(copy->exportAudio(reopened,output,-1,false,[&]{return lateCancel;},[&](double progress){if(progress>.99)lateCancel=true;},nullptr,options).failed(),"late cancellation applies to return render");require(lateCancel,"return cancellation actually reaches the late progress gate");juce::MemoryBlock afterCancel;require(output.loadFileAsData(afterCancel)&&afterCancel==preserved&&!copy->isExporting(),"cancelled return render preserves previous WAV and restores state");
    options.sampleRate=48000;options.bitDepth=24;require(copy->exportAudio(reopened,output,-1,false,[]{return false;},[](double){},nullptr,options).wasOk(),"return WAV can be overwritten after cancellation");{std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(output.createInputStream().release(),true));require(reader&&reader->sampleRate==48000&&reader->bitsPerSample==24,"return overwrite respects sample rate and bit depth");}
    engine.clearEffect(returnEffectTrack,1);project.returnEffects[0].removeEffect(1);engine.publish(project);engine.play(true);for(int n=0;n<100;++n)callback();
    require(std::abs(engine.busLevel(2)-.05f*(float)std::pow(10.,-expected/20))<.0001f,"removed reverb leaves an exact dry parallel return");
    engine.stop();require(project.returnEffects[0].effects.empty()&&engine.effectPlugin(returnEffectTrack,1)==nullptr,"removing reverb really removes its processor");
    project.tracks[1].mute=true;engine.publish(project);engine.play(true);for(int n=0;n<100;++n)callback();require(nativeCompressorReduction(compressor)<.03,"muted selected source does not trigger compressor");engine.stop();
    engine.clearEffect(returnEffectTrack+1,1);project.returnEffects[1].effects.clear();project.buses[2].amount=1;project.buses[3].feedback=.9;const auto empty=folder.getChildFile("empty-returns.wav");AudioExportOptions emptyOptions;emptyOptions.mode=AudioExportMode::transparent;
    require(engine.exportAudio(project,empty,-1,false,[]{return false;},[](double){},nullptr,emptyOptions).wasOk(),"empty editable returns export");{std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(empty.createInputStream().release(),true));require(reader&&reader->lengthInSamples==(juce::int64)std::ceil((project.length()*60/project.settings.bpm+3)*reader->sampleRate),"removed devices must not leave legacy reverb/delay tail estimates");}
    std::cout<<"Editable A/B returns, version-7 state/legacy separation, native wet audio/removal, independent source order/mute, private Transparent WAV PASS; external_GR="<<reduction<<" dB expected="<<expected<<"; 51200 stereo frames="<<elapsed<<" ms; C++ allocations="<<allocations<<"; native latency=0\n";
}

}
void runSidechainTests(){legacy();liveEdits();controls();routing();persistence();realtime();exportAndTrackGating();runReturnPdcTests();detectorAlignment();returnModeReset();tempoReturns();editableReturns();}
