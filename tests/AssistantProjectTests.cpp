// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/AssistantProject.h"
#include "../Source/AudioEngine.h"
#include "../Source/NativeCompressor.h"
#include "../Source/ProjectIO.h"
#include <iostream>
#include <stdexcept>
void installEffectForReturnTests(fga::AudioEngine&,int,std::unique_ptr<juce::AudioPluginInstance>,int,const juce::PluginDescription&);
namespace {
void require(bool value,const char* text){if(!value)throw std::runtime_error(text);}
class Constant:public juce::AudioPluginInstance {
public:
    Constant():AudioPluginInstance(BusesProperties().withInput("In",juce::AudioChannelSet::stereo()).withOutput("Out",juce::AudioChannelSet::stereo())){}
    const juce::String getName()const override{return "Analysis DC fixture";}
    void fillInPluginDescription(juce::PluginDescription& d)const override{d.name=getName();}
    void prepareToPlay(double,int)override{}void releaseResources()override{}
    void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&)override{for(int c=0;c<b.getNumChannels();++c)juce::FloatVectorOperations::fill(b.getWritePointer(c),.8f,b.getNumSamples());}
    bool acceptsMidi()const override{return false;}bool producesMidi()const override{return false;}double getTailLengthSeconds()const override{return 0;}
    int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}const juce::String getProgramName(int)override{return {}; }void changeProgramName(int,const juce::String&)override{}
    bool hasEditor()const override{return false;}juce::AudioProcessorEditor* createEditor()override{return nullptr;}
    void getStateInformation(juce::MemoryBlock&)override{}void setStateInformation(const void*,int)override{}
};
class Impulse final:public Constant {
    bool fired=false;
public:
    void reset()override{fired=false;}
    void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&)override{b.clear();if(!fired){b.setSample(0,0,1);b.setSample(1,0,1);fired=true;}}
};
class Delay final:public Constant {
    std::array<std::array<float,64>,2> data{};int position=0;
public:
    void prepareToPlay(double,int)override{setLatencySamples(64);reset();}
    void reset()override{for(auto& channel:data)channel.fill(0);position=0;}
    void processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer&)override{for(int n=0;n<b.getNumSamples();++n){for(int c=0;c<2;++c){const float value=b.getSample(c,n);b.setSample(c,n,data[(size_t)c][(size_t)position]);data[(size_t)c][(size_t)position]=value;}position=(position+1)%64;}}
};
}
void runAssistantProjectTests(){
    using namespace fga;
    AssistantAudio files;require(files.directory.createDirectory().wasOk(),"create analysis directory");Project p=newProject(730);p.sections={{Section::main,1,"Project analysis"}};p.settings.bpm=120;p.normalise();
    for(auto& t:p.tracks){t.mute=true;t.gain=1;t.pan=0;t.volume={};t.panAutomation={};t.parameters.clear();t.eq={};t.sends={};t.group=-1;for(auto& clip:t.clips)clip.notes.clear();}
    p.masterAutomation={};p.masterGain=2;p.tracks[0].mute=false;p.tracks[0].gain=2;p.tracks[0].group=0;p.tracks[0].sends={.2,.1};p.buses[0].sidechainAmount=.5;p.buses[0].sidechainSource=4;p.tracks[4].mute=false;p.tracks[4].gain=.5;
    AudioEngine engine(AudioEngine::Offline{});juce::PluginDescription description;Constant fixture;fixture.fillInPluginDescription(description);
    installEffectForReturnTests(engine,0,std::make_unique<Constant>(),1,description);installEffectForReturnTests(engine,4,std::make_unique<Constant>(),1,description);
    for(int ti:{0,4}){EffectSlot slot;slot.slot=1;p.tracks[(size_t)ti].effects.push_back(slot);}
    engine.publish(p);const auto json=juce::JSON::toString(projectJson(p));
    const auto renderStart=juce::Time::getMillisecondCounterHiRes();
    require(engine.renderAnalysis(p,files.directory,{0,4},[]{return false;},[](double){}).wasOk(),"single-pass analysis render with routing and sidechain");
    const auto renderMs=juce::Time::getMillisecondCounterHiRes()-renderStart;
    std::cout<<"Local raw analysis (mix + 2 taps, block 512, stereo float): "<<renderMs<<" ms; capture buffers="<<(2*2*512*sizeof(float))<<" bytes; not a hardware/dropout benchmark\n";
    juce::WavAudioFormat wav;auto read=[&](const juce::File& f){return std::unique_ptr<juce::AudioFormatReader>(wav.createReaderFor(f.createInputStream().release(),true));};
    auto mix=read(files.mix()),track=read(files.stem(0));require(mix&&track&&mix->usesFloatingPointData&&mix->bitsPerSample==32&&track->lengthInSamples==mix->lengthInSamples,"raw float and aligned tap lengths");
    juce::AudioBuffer<float> samples(2,8192);require(mix->read(&samples,0,8192,8192,true,true)&&samples.getMagnitude(0,8192)>1,"analysis exposes over-level before Transparent");
    require(track->read(&samples,0,8192,8192,true,true)&&std::abs(samples.getSample(0,4096)-1.6f)<1e-5,"track tap post-fader remains in full sidechain context, before group/master");
    const auto exportFile=files.directory.getChildFile("protected.wav");require(engine.exportAudio(p,exportFile,-1,false,[]{return false;},[](double){}).wasOk(),"ordinary Transparent still exports");auto protectedWav=read(exportFile);require(protectedWav&&protectedWav->read(&samples,0,8192,8192,true,true)&&samples.getMagnitude(0,8192)<.9,"Transparent protection unchanged");
    require(juce::JSON::toString(projectJson(p))==json&&!engine.isExporting(),"render does not modify project or leave export ownership");
    for(int attempt=0;attempt<3;++attempt){const auto dir=files.directory.getChildFile("cancel-"+juce::String(attempt));dir.createDirectory();int calls=0;require(engine.renderAnalysis(p,dir,{0},[&]{return ++calls>attempt;},[](double){}).failed()&&!engine.isExporting()&&dir.findChildFiles(juce::File::findFilesAndDirectories,false).isEmpty(),"cancellation releases ownership and removes partial files");}
    const auto retry=files.directory.getChildFile("retry");retry.createDirectory();require(engine.renderAnalysis(p,retry,{0},[]{return false;},[](double){}).wasOk(),"new analysis after repeated cancellations");
    const auto existing=files.directory.getChildFile("existing");existing.createDirectory();existing.getChildFile("keep.wav").replaceWithText("untouched");require(engine.renderAnalysis(p,existing,{0},[]{return false;},[](double){}).failed()&&existing.getChildFile("keep.wav").loadFileAsString()=="untouched","refuses nonempty user directory without touching files");
    p.tracks[1].mute=false;MixReport report;report.suggestions={{"gain",p.tracks[1].sessionId,-1.5}};auto changes=assistantGainChanges(p,report);assistantEqChanges(p,report,changes);require(changes.size()==2,"structured gain and alternative EQ");for(auto& c:changes)c.selected=true;
    Project candidate;require(assistantCandidate(p,engine,changes,candidate).wasOk()&&candidate.tracks[1].gain<p.tracks[1].gain&&candidate.tracks[1].eq.enabled,"selected concrete gain and EQ values");require(candidate.tracks[1].volume==p.tracks[1].volume&&candidate.tracks[1].clips==p.tracks[1].clips,"patch preserves automation/MIDI");
    auto stale=p;stale.tracks[1].gain*=.5;require(assistantCandidate(stale,engine,changes,candidate).failed(),"stale changes rejected atomically");
    // Compressor detector taps are intentionally pre-insert: provide a real
    // instrument source, not an effect that creates DC after that tap.
    const auto source=files.directory.getChildFile("detector-source.wav");
    {auto output=source.createOutputStream();std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(output.get(),44100,2,24,{},0));require(writer!=nullptr,"detector fixture writer");output.release();juce::AudioBuffer<float> signal(2,44100);for(int c=0;c<2;++c)juce::FloatVectorOperations::fill(signal.getWritePointer(c),.8f,signal.getNumSamples());require(writer->writeFromAudioSampleBuffer(signal,0,signal.getNumSamples())&&writer->writeFromAudioSampleBuffer(signal,0,signal.getNumSamples()),"detector fixture audio");}
    p.tracks[4].instrument="sample";p.tracks[4].sampleRoot=60;p.tracks[4].sampler.mode=0;p.tracks[4].clips[0].notes={{0,1.8,60,127,0}};require(engine.loadSample(4,source,60,p.tracks[4].sampler).wasOk(),"load real detector source");
    const auto compressor=nativeCompressorDescription();installEffectForReturnTests(engine,0,createNativeCompressor(),2,compressor);EffectSlot detector;detector.slot=2;p.tracks[0].effects.push_back(detector);engine.effectPlugin(0,2)->getParameters()[6]->setValue(5.f/maxTracks);
    const auto driven=files.directory.getChildFile("detector-active");driven.createDirectory();require(engine.renderAnalysis(p,driven,{0},[]{return false;},[](double){}).wasOk(),"capture preserves an unselected external detector track");auto drivenTap=read(driven.getChildFile("track-0.wav"));require(drivenTap&&drivenTap->read(&samples,0,8192,8192,true,true),"read driven detector tap");const float ducked=samples.getMagnitude(0,8192);
    p.tracks[4].mute=true;const auto silentDetector=files.directory.getChildFile("detector-muted");silentDetector.createDirectory();require(engine.renderAnalysis(p,silentDetector,{0},[]{return false;},[](double){}).wasOk(),"muted source detector context");auto freeTap=read(silentDetector.getChildFile("track-0.wav"));const bool readFree=freeTap&&freeTap->read(&samples,0,8192,8192,true,true);std::cout<<"Detector capture: active="<<ducked<<" muted="<<samples.getMagnitude(0,8192)<<" source="<<nativeCompressorSource(engine.effectPlugin(0,2))<<"\n";require(readFree&&samples.getMagnitude(0,8192)>ducked*1.5,"track tap reflects full-project compressor sidechain rather than a solo render");engine.clearEffect(0,2);p.tracks[0].removeEffect(2);
    // Track taps discard track PDC, not the additional master/return latency.
    p.tracks[0].gain=1;p.tracks[0].group=-1;p.tracks[0].sends={};p.tracks[4].mute=true;p.masterGain=1;
    installEffectForReturnTests(engine,0,std::make_unique<Impulse>(),1,description);installEffectForReturnTests(engine,masterEffectTrack,std::make_unique<Delay>(),1,description);EffectSlot delay;delay.slot=1;p.masterEffects.effects={delay};
    const auto aligned=files.directory.getChildFile("pdc");aligned.createDirectory();require(engine.renderAnalysis(p,aligned,{0},[]{return false;},[](double){}).wasOk(),"PDC project analysis render");
    auto alignedMix=read(aligned.getChildFile("mix.wav")),alignedTap=read(aligned.getChildFile("track-0.wav"));juce::AudioBuffer<float> first(2,1),tap(2,1);
    require(alignedMix&&alignedTap&&alignedMix->read(&first,0,1,0,true,true)&&alignedTap->read(&tap,0,1,0,true,true)&&first.getSample(0,0)==1&&tap.getSample(0,0)==1&&alignedMix->lengthInSamples==alignedTap->lengthInSamples,"master latency cannot discard/shift the first track impulse");
    const auto native=nativeCompressorDescription();installEffectForReturnTests(engine,masterEffectTrack,createNativeCompressor(),1,native);EffectSlot slot;slot.slot=1;p.masterEffects.effects={slot};engine.publish(p);
    changes.clear();MixMeasure measure;measure.peak=.8;measure.rms=.6;assistantCompressorChanges(engine,p,masterEffectTrack,measure,changes);require(!changes.empty(),"existing native compressor generates numeric settings");for(auto& c:changes)c.selected=true;require(assistantCandidate(p,engine,changes,candidate).wasOk(),"native parameter candidates validate");
    assistantParameters(engine,p,changes,true);require(std::abs(engine.effectPlugin(masterEffectTrack,1)->getParameters()[changes[0].parameter]->getValue()-changes[0].after)<1e-6,"native settings really apply");assistantParameters(engine,p,changes,false);
    ParameterAutomation lane;lane.slot=1;lane.index=changes[0].parameter;lane.curve.enabled=true;lane.curve.points={{0,.4,0}};p.masterEffects.parameters.push_back(lane);require(assistantCandidate(p,engine,changes,candidate).failed(),"active native parameter automation is preserved");
    p.masterEffects.parameters.clear();installEffectForReturnTests(engine,masterEffectTrack,std::make_unique<Constant>(),1,description);
    require(assistantCandidate(p,engine,changes,candidate).failed(),"a replaced compressor with fewer parameters is safely rejected");
    std::cout<<"PASS raw project/taps/routing/sidechain, unchanged Transparent, cancellation/cleanup/retry, structured gain/EQ/native compressor and automation guards\n";
}
