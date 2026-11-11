// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/AudioEngine.h"
#include "../Source/CompressorDSP.h"
#include "../Source/NativeCompressor.h"
#include "../Source/ProjectIO.h"
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
#include <thread>
namespace {
thread_local bool countAllocations=false;
thread_local size_t allocations=0;
void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
struct AllocationScope {AllocationScope(){allocations=0;countAllocations=true;}~AllocationScope(){countAllocations=false;}};
}
// Test-only regular C++ allocation counter; inactive outside the measured scopes.
void* operator new(std::size_t size){if(countAllocations)++allocations;if(void* memory=std::malloc(size?size:1))return memory;throw std::bad_alloc();}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* p)noexcept{std::free(p);}void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
namespace {
void realtimeEq(){
    fga::EqSettings eq;eq.enabled=true;eq.ensureBands();fga::ChannelStrip strip;
    {AllocationScope scope;strip.configure(eq,48000);}
    std::cout<<"EQ callback configuration allocations="<<allocations<<"\n";
    require(allocations==0,"native channel EQ configuration allocates on the audio side");
    eq.bands.clear();{AllocationScope scope;strip.configure(eq,48000);}require(allocations==0,"legacy EQ fallback allocates");
}
void dspChecks(){using namespace fga;
    for(double rate:{44100.,48000.,96000.})for(int block:{32,64,128,512,1024}){
        CompressorDSP dsp;auto values=CompressorDSP::defaults;values={-18,4,0,20,150,0};dsp.prepare(rate,values);dsp.setBypassed(false);
        juce::AudioBuffer<float> audio(2,block);int processed=0;const int total=(int)(rate*2);
        while(processed<total){audio.clear();for(int s=0;s<block;++s){audio.setSample(0,s,(float)std::pow(10.,-6./20));audio.setSample(1,s,(float)(std::pow(10.,-6./20)*.37));}{AllocationScope scope;dsp.configure(values);dsp.process(audio.getWritePointer(0),audio.getWritePointer(1),block);}require(allocations==0,"compressor processing allocates");processed+=block;}
        require(std::abs(dsp.reductionDb()-9)<.01,"compressor hard-knee gain reduction");
        require(std::abs(audio.getSample(0,block-1)-std::pow(10.,-15./20))<.0002,"compressor steady gain curve");
        require(std::abs(audio.getSample(1,block-1)-audio.getSample(0,block-1)*.37)<1e-7,"compressor stereo link");
        dsp.setBypassed(true);double jump=0,previous=audio.getSample(0,block-1);
        for(int n=0;n<(int)std::ceil(rate*.03/block);++n){for(int s=0;s<block;++s)audio.setSample(0,s,(float)std::pow(10.,-6./20));dsp.process(audio.getWritePointer(0),nullptr,block);for(int s=0;s<block;++s){jump=std::max(jump,std::abs(audio.getSample(0,s)-previous));previous=audio.getSample(0,s);}}
        require(jump<.002&&std::abs(previous-std::pow(10.,-6./20))<1e-7,"native bypass must ramp to exact dry audio");
    }
    require(std::abs(CompressorDSP::reduction(-18,-18,4,6)-.5625)<1e-12,"compressor knee midpoint");
    require(CompressorDSP::reduction(-21,-18,4,6)==0&&std::abs(CompressorDSP::reduction(-15,-18,4,6)-2.25)<1e-12,"compressor knee boundaries");
    CompressorDSP dsp;auto values=CompressorDSP::defaults;values={-18,4,0,20,150,0};dsp.prepare(48000,values);dsp.setBypassed(false);
    std::array<float,480> silence{};for(int n=0;n<2;++n)dsp.process(silence.data(),nullptr,480);
    std::vector<float> step(960,(float)std::pow(10.,-6./20));dsp.process(step.data(),nullptr,(int)step.size());
    require(std::abs(dsp.reductionDb()-9*(1-std::exp(-1.)))<.01,"attack is the stated 63-percent gain-reduction time");
    const double before=dsp.reductionDb();std::vector<float> release(7200);dsp.process(release.data(),nullptr,(int)release.size());
    require(std::abs(dsp.reductionDb()-before*std::exp(-1.))<.01,"release is the stated 37-percent gain-reduction time");
    for(int rate:{44100,48000,96000})for(double amplitude:{.01,.5}){
        CompressorDSP measured;measured.prepare(rate,CompressorDSP::defaults);measured.setBypassed(false);
        std::array<float,512> signal{};double energy=0,sine=0,cosine=0;
        for(int at=0;at<rate*3;at+=512){const int n=std::min(512,rate*3-at);for(int s=0;s<n;++s)signal[(size_t)s]=(float)(amplitude*std::sin(2*juce::MathConstants<double>::pi*997*(at+s)/rate));measured.process(signal.data(),nullptr,n);
            for(int s=0;s<n;++s)if(at+s>=rate*2){const double sample=signal[(size_t)s],phase=2*juce::MathConstants<double>::pi*997*(at+s)/rate;energy+=sample*sample;sine+=sample*std::sin(phase);cosine+=sample*std::cos(phase);}}
        const double fundamental=2*(sine*sine+cosine*cosine)/(rate*(double)rate),thdn=100*std::sqrt(std::max(0.,energy/rate-fundamental)/std::max(1e-30,fundamental));
        std::cout<<"Native compressor 997 Hz: rate="<<rate<<", amplitude="<<amplitude<<", THD+N="<<thdn<<"%, GR="<<measured.reductionDb()<<" dB\n";
        require(thdn<(amplitude<.1?.001:.5),"conservative compressor sine distortion exceeds acceptance limit");
    }
    values={-18,1,0,20,150,0};dsp.prepare(48000,values);dsp.setBypassed(false);std::array<float,4> exceptional{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),std::numeric_limits<float>::max(),0};dsp.process(exceptional.data(),nullptr,4);for(float sample:exceptional)require(std::isfinite(sample),"non-finite compressor output");
    std::array<float,4> neutral{.17f,-.37f,.9f,-.7f},original=neutral;dsp.prepare(48000,values);dsp.setBypassed(false);dsp.process(neutral.data(),nullptr,4);require(neutral==original,"ratio-one/makeup-zero must be exact unity");
    auto plugin=createNativeCompressor();plugin->prepareToPlay(48000,512);juce::AudioBuffer<float> block(2,512);juce::MidiBuffer midi;
    std::atomic<bool> failed{false};std::atomic<size_t> audioAllocations{0};
    std::thread audio([&]{AllocationScope scope;for(int n=0;n<1000;++n){for(int ch=0;ch<2;++ch)juce::FloatVectorOperations::fill(block.getWritePointer(ch),.1f,512);plugin->processBlock(block,midi);if(!std::isfinite(block.getMagnitude(0,512)))failed=true;}audioAllocations=allocations;});
    for(int n=0;n<200;++n){plugin->getParameters()[0]->setValue((n%20)/20.f);juce::MemoryBlock state;plugin->getStateInformation(state);}audio.join();
    require(!failed&&audioAllocations==0,"concurrent native parameter/state reads must remain finite and allocation-free on audio");
    plugin->getParameters()[0]->setValue(.6f);plugin->getParameters()[1]->setValue(3.f/19);plugin->prepareToPlay(48000,512);
    const auto start=juce::Time::getMillisecondCounterHiRes();for(int n=0;n<2000;++n){for(int ch=0;ch<2;++ch)juce::FloatVectorOperations::fill(block.getWritePointer(ch),.1f,512);plugin->processBlock(block,midi);}
    std::cout<<"Native compressor local benchmark: 1024000 stereo frames, ms="<<juce::Time::getMillisecondCounterHiRes()-start<<", DSP bytes="<<sizeof(CompressorDSP)<<"\n";
    std::cout<<"PASS compressor: 15 rate/buffer configurations, curve/knee/timing/stereo/smooth bypass, zero measured allocations\n";
}
struct Deadline final : juce::Timer {bool expired=false;Deadline(){startTimer(5000);}void timerCallback()override{expired=true;juce::MessageManager::getInstance()->stopDispatchLoop();}};
void hostChecks(){using namespace fga;
    AudioEngine engine(AudioEngine::Offline{});const auto description=nativeCompressorDescription();
    auto malformed=description;malformed.fileOrIdentifier="fga:unknown";require(isNativeCompressor(description)&&!isNativeCompressor(malformed),"native identity must be exact");
    int returned=0;bool installed=true,cancelled=false;auto complete=[&]{if(++returned==4)juce::MessageManager::getInstance()->stopDispatchLoop();};
    for(int slot:{1,2})engine.loadEffect(0,description,[&](juce::Result result){installed=installed&&result.wasOk();complete();},slot);
    engine.loadEffect(masterEffectTrack,description,[&](juce::Result result){installed=installed&&result.wasOk();complete();},1);
    engine.loadEffect(1,description,[&](juce::Result result){cancelled=result.failed();complete();},1);engine.clearEffect(1,1);
    Deadline deadline;juce::MessageManager::getInstance()->runDispatchLoop();deadline.stopTimer();
    require(!deadline.expired&&returned==4&&installed&&cancelled&&!engine.effectPlugin(1,1),"native async load/cancellation lifecycle");
    auto project=newProject(0);project.settings.bpm=120;project.settings.freeTempo=true;project.sections={{Section::main,1,"Compressor"}};project.normalise();project.masterGain=1;project.masterAutomation.enabled=false;
    for(auto& track:project.tracks){track.gain=0;track.pan=0;track.volume.enabled=false;track.sends={};for(auto& clip:track.clips)clip.notes.clear();}
    auto& track=project.tracks[0];track.gain=1;track.instrument="sample";track.sampleRoot=60;track.sampler.mode=1;track.sampler.attack=track.sampler.decay=0;track.sampler.sustain=1;track.sampler.velocity=0;track.clips[0].notes={{0,4,60,127,0}};
    const auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fga-dynamics","",false);folder.createDirectory();struct Cleanup{juce::File folder;~Cleanup(){folder.deleteRecursively();}}cleanup{folder};
    const auto source=folder.getChildFile("source.wav");juce::WavAudioFormat wav;{std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(source.createOutputStream().release(),44100,2,24,{},0));juce::AudioBuffer<float> dc(2,88200);for(int c=0;c<2;++c)juce::FloatVectorOperations::fill(dc.getWritePointer(c),.1f,88200);require(writer&&writer->writeFromAudioSampleBuffer(dc,0,88200),"compressor routing fixture");}
    track.instrumentPath=source.getFullPathName().toStdString();require(engine.loadSample(0,source,60,track.sampler).wasOk(),"load compressor source");
    for(int slot:{1,2}){EffectSlot effect;effect.slot=slot;effect.path=description.fileOrIdentifier.toStdString();effect.pluginName=description.name.toStdString();effect.pluginId=description.createXml()->toString().toStdString();track.effects.push_back(effect);}
    project.masterEffects.effects={track.effects[0]};project.masterEffects.effects[0].bypassed=true;
    auto set=[&](juce::AudioPluginInstance* plugin,int index,float plain){const float normalized=(plain-CompressorDSP::minimum[(size_t)index])/(CompressorDSP::maximum[(size_t)index]-CompressorDSP::minimum[(size_t)index]);plugin->getParameters()[index]->setValue(normalized);};
    set(engine.effectPlugin(0,1),1,1);set(engine.effectPlugin(0,1),5,12);
    set(engine.effectPlugin(0,2),0,-18);set(engine.effectPlugin(0,2),1,4);set(engine.effectPlugin(0,2),2,0);
    auto render=[&](const char* name){auto copy=engine.offlineCopy();require(copy!=nullptr,"native inserts clone for offline export");const auto file=folder.getChildFile(name);require(copy->exportAudio(project,file,0,false,[]{return false;},[](double){}).wasOk(),"native inserts render through existing Transparent export");std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(file.createInputStream().release(),true));juce::AudioBuffer<float> block(2,1000);require(reader&&reader->read(&block,0,1000,44100,true,true),"read native insert render");return block.getRMSLevel(0,0,1000);};
    const float first=render("first.wav");track.moveEffect(2,0);const float second=render("second.wav");require(first>.1f&&second>first*2,"native insert order uses the serial rack");
    engine.capturePluginStates(project);Project loaded;require(projectFromJson(projectJson(project),loaded).wasOk()&&loaded.tracks[0].effects.size()==2&&!loaded.tracks[0].effects[0].state.empty()&&loaded.masterEffects.effects.size()==1,"native insert state uses existing project format");
    auto fresh=createNativeCompressor();juce::MemoryBlock state;engine.effectPlugin(0,1)->getStateInformation(state);fresh->setStateInformation(state.getData(),(int)state.getSize());require(std::abs(fresh->getParameters()[5]->getValue()-engine.effectPlugin(0,1)->getParameters()[5]->getValue())<1e-7&&fresh->getLatencySamples()==0,"native state and zero latency");
    ParameterAutomation lane;lane.slot=2;lane.index=0;lane.parameterId="threshold";lane.curve.enabled=true;lane.curve.points={{0,.2,0},{4,.7,0}};track.parameters={lane};engine.publish(project);engine.play(true);juce::AudioBuffer<float> block(2,512);juce::AudioIODeviceCallbackContext context;
    for(int n=0;n<100;++n)engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);
    require(engine.effectPlugin(0,2)->getParameters()[0]->getValue()>.2f&&nativeCompressorReduction(engine.effectPlugin(0,2))>0&&engine.latencySamples()==0,"native automation/meter and PDC remain valid");engine.stop();
    engine.play(true);engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);engine.collectGarbage();project.tracks[0].gain=.9;engine.updateMix(project);
    {AllocationScope scope;engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);}
    require(allocations==0,"adopting native mix/EQ updates must not allocate on the callback");engine.stop();
    std::cout<<"PASS native factory/state/track+master/order/automation/cancel/offline clone, zero latency\n";
}
}
void runDynamicsTests(){realtimeEq();dspChecks();hostChecks();}
