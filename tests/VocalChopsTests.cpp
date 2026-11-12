// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/Sampler.h"
#include "../Source/AudioEngine.h"
#include "../Source/StudioIO.h"
#include "../Source/ProjectIO.h"
#include <iostream>
#include <stdexcept>
namespace {
std::shared_ptr<fga::SampleAsset> fixture(int frames=48000){
    auto a=std::make_shared<fga::SampleAsset>();a->rate=48000;a->frames=frames;
    a->audio.setSize(2,frames+4);a->audio.clear();return a;
}
juce::AudioBuffer<float> render(const std::shared_ptr<fga::SampleAsset>& asset,fga::SamplerSettings settings,
                                const juce::MidiBuffer& events,int frames,int block=256,double bpm=125){
    fga::SampleSynth synth(asset,settings,60,false);synth.setCurrentPlaybackSampleRate(48000);synth.configure(settings,60,bpm);
    juce::AudioBuffer<float> result(2,frames);result.clear();
    for(int at=0;at<frames;at+=block)synth.renderNextBlock(result,events,at,std::min(block,frames-at));
    return result;
}
}
void runVocalChopsTests(){
    using namespace fga;
    int failures=0;auto check=[&](bool ok,const char* message){if(!ok){++failures;std::cout<<"FAIL "<<message<<std::endl;}};
    auto attacks=fixture();for(int i=12000;i<24000;++i)for(int c=0;c<2;++c)attacks->audio.setSample(c,i,.5f);
    const auto one=detectSampleSlices(*attacks,0,1,1,1);
    std::cout<<"slice limit=1 actual="<<one.size()<<std::endl;
    check(one.size()==1,"transient detection must respect a one-slice MIDI budget");
    auto asset=fixture(1000);for(int c=0;c<2;++c)for(int i=500;i<1000;++i)asset->audio.setSample(c,i,1);
    SamplerSettings s;s.mode=2;s.sliceBase=60;s.sliceCount=2;s.slices[0]=0;s.slices[1]=.5;
    s.polyphony=1;s.attack=s.decay=0;s.sustain=1;s.fadeIn=s.fadeOut=0;s.transpose=-12;
    juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
    auto isolated=render(asset,s,midi,1100);
    std::cout<<"slice boundary leakage="<<isolated.getMagnitude(0,1100)<<std::endl;
    check(isolated.getMagnitude(0,1100)==0,"fractional interpolation cannot read the following slice");
    asset=fixture();for(int i=0;i<asset->frames;++i)for(int c=0;c<2;++c)asset->audio.setSample(c,i,i<24000?.6f:-.6f);
    s.transpose=0;s.fadeIn=.002;s.fadeOut=.005;midi.clear();midi.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);midi.addEvent(juce::MidiMessage::noteOn(1,61,1.f),4096);
    auto cut=render(asset,s,midi,8192);float jump=0;for(int i=4000;i<4400;++i)jump=std::max(jump,std::abs(cut.getSample(0,i)-cut.getSample(0,i-1)));
    auto oddBlocks=render(asset,s,midi,8192,63);
    float blockError=0;for(int i=0;i<8192;++i)blockError=std::max(blockError,std::abs(cut.getSample(0,i)-oddBlocks.getSample(0,i)));
    check(blockError<1e-6f,"retrigger fades are independent of host block boundaries");
    std::cout<<"mono retrigger maximum jump="<<jump<<std::endl;
    check(jump<.025f,"monophonic chop retrigger must fade the interrupted voice");
    {
        constexpr int sliceFrames=5000,slicesN=33,frames=sliceFrames*slicesN;
        auto crowdedAsset=fixture(frames);
        for(int i=0;i<frames;++i){const float v=i>=sliceFrames*(slicesN-1)?-.6f:.6f;for(int c=0;c<2;++c)crowdedAsset->audio.setSample(c,i,v);}
        auto wide=s;wide.polyphony=32;wide.sliceBase=36;wide.sliceCount=slicesN;wide.fadeIn=wide.fadeOut=0;wide.sync=false;wide.reverse=false;wide.transpose=0;
        for(int i=0;i<slicesN;++i)wide.slices[(size_t)i]=i*(double)sliceFrames/frames;
        juce::MidiBuffer chord;for(int i=0;i<32;++i)chord.addEvent(juce::MidiMessage::noteOn(1,36+i,1.f),0);
        chord.addEvent(juce::MidiMessage::noteOn(1,36+32,1.f),4096);
        SampleSynth timing(crowdedAsset,wide,60,false);timing.setCurrentPlaybackSampleRate(48000);const int lag=timing.latencySamples();
        auto crowded=render(crowdedAsset,wide,chord,8192+lag);
        float crowdedJump=0;const int from=4096+lag,to=std::min(crowded.getNumSamples()-1,from+400);
        for(int i=from+1;i<=to;++i)crowdedJump=std::max(crowdedJump,std::abs(crowded.getSample(0,i)-crowded.getSample(0,i-1)));
        std::cout<<"polyphony 32 steal jump="<<crowdedJump<<" lag="<<lag<<std::endl;
        check(crowdedJump<.025f,"default 32-voice polyphony crossfades a stolen slice");
        constexpr int pairFrames=5000,pairN=34,pairLen=pairFrames*pairN;
        auto pairAsset=fixture(pairLen);
        for(int i=0;i<pairLen;++i){const float v=i>=pairFrames*32?-.6f:.6f;for(int c=0;c<2;++c)pairAsset->audio.setSample(c,i,v);}
        auto pair=wide;pair.sliceCount=pairN;for(int i=0;i<pairN;++i)pair.slices[(size_t)i]=i*(double)pairFrames/pairLen;
        juce::MidiBuffer two;for(int i=0;i<32;++i)two.addEvent(juce::MidiMessage::noteOn(1,36+i,1.f),0);
        two.addEvent(juce::MidiMessage::noteOn(1,36+32,1.f),4096);two.addEvent(juce::MidiMessage::noteOn(1,36+33,1.f),4096);
        auto both=render(pairAsset,pair,two,8192+lag);float pairJump=0;
        for(int i=from+1;i<=to&&i<both.getNumSamples();++i)pairJump=std::max(pairJump,std::abs(both.getSample(0,i)-both.getSample(0,i-1)));
        std::cout<<"polyphony 32 two-note jump="<<pairJump<<std::endl;
        check(pairJump<.05f,"two new notes in one block at 32 voices do not hard-cut");
    }
    s.reverse=true;auto reversed=render(asset,s,midi,8192);float reverseJump=0;for(int i=4000;i<4400;++i)reverseJump=std::max(reverseJump,std::abs(reversed.getSample(0,i)-reversed.getSample(0,i-1)));
    check(reverseJump<.025f,"reverse monophonic slices also release interrupted audio");s.reverse=false;
    s.sync=true;s.sourceBpm=125;int lag=0;
    {SampleSynth timing(asset,s,60,false);timing.setCurrentPlaybackSampleRate(48000);lag=timing.latencySamples();}
    auto syncedCut=render(asset,s,midi,8192);const float bridge=syncedCut.getMagnitude(4096+lag/2,64);
    std::cout<<"sync retrigger bridge magnitude="<<bridge<<" latency="<<lag<<std::endl;
    check(bridge>.05f,"sync retrigger release accounts for stretcher latency without a silent gap");
    juce::MidiBuffer held;held.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
    auto fullSlice=render(asset,s,held,27000);int last=0;for(int i=0;i<27000;++i)if(std::abs(fullSlice.getSample(0,i))>.001f)last=i;
    std::cout<<"sync slice last audible frame="<<last<<" expected near="<<24000+lag<<std::endl;
    check(last>24000+lag-480,"sync slice retains full duration after latency warmup");
    auto shortSettings=s;shortSettings.end=.01;shortSettings.slices[1]=.005;
    auto shortSlice=render(asset,shortSettings,held,2048);
    std::cout<<"short sync slice magnitude="<<shortSlice.getMagnitude(0,2048)<<std::endl;
    check(shortSlice.getMagnitude(0,2048)>1e-4f,"sync slices shorter than latency must still sound");s.sync=false;
    midi.addEvent(juce::MidiMessage::allSoundOff(1),4120);auto stopped=render(asset,s,midi,8192);
    check(stopped.getMagnitude(4120,8192-4120)==0,"panic clears both current and retiring slice voices");
    s.polyphony=2;midi.clear();midi.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);midi.addEvent(juce::MidiMessage::noteOn(2,61,1.f),4096);midi.addEvent(juce::MidiMessage::allSoundOff(1),4120);
    check(render(asset,s,midi,8192).getMagnitude(4300,100)>.05f,"panic respects MIDI channels");s.polyphony=1;
    s.transpose=-12;midi.clear();midi.addEvent(juce::MidiMessage::noteOn(1,60,1.f),0);
    asset=fixture(1001);for(int i=0;i<asset->frames;++i)for(int c=0;c<2;++c)asset->audio.setSample(c,i,i<501?0.f:1.f);
    auto fractional=render(asset,s,midi,1200);check(fractional.getMagnitude(0,1200)==0,"fractional markers round to source samples without neighbour bleed");
    asset=fixture();for(int i=0;i<asset->frames;++i)for(int c=0;c<2;++c)asset->audio.setSample(c,i,.4f*std::sin(i*.034));s.transpose=0;
    s.sync=true;s.sourceBpm=125;midi.clear();const int duration=737280;
    for(int i=0;i<32;++i)midi.addEvent(juce::MidiMessage::noteOn(1,60+i%2,1.f),i*23040);
    const auto start=juce::Time::getMillisecondCounterHiRes();auto bars=render(asset,s,midi,duration);
    std::cout<<"8-bar sync fixture: ms="<<juce::Time::getMillisecondCounterHiRes()-start<<" output_bytes="<<duration*2*sizeof(float)<<" source_bytes="<<asset->audio.getNumSamples()*2*sizeof(float)<<std::endl;
    // Real sample load, persistence, timeline seek/loop and live tempo change.
    const auto directory=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fga-chops","",false);directory.createDirectory();
    struct Cleanup{juce::File folder;~Cleanup(){folder.deleteRecursively();}} cleanup{directory};
    const auto file=directory.getChildFile("synthetic.wav");juce::WavAudioFormat wav;
    {std::unique_ptr<juce::OutputStream> stream=file.createOutputStream();auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(32));if(!writer||!writer->writeFromAudioSampleBuffer(asset->audio,0,asset->frames))throw std::runtime_error("chops WAV fixture");}
    std::shared_ptr<const SampleAsset> loaded;if(readSampleAsset(file,loaded).failed())throw std::runtime_error("load chops fixture");
    const auto original=loaded;auto invalid=directory.getChildFile("invalid.wav");invalid.replaceWithText("not an audio file");
    check(readSampleAsset(invalid,loaded).failed()&&loaded==original,"invalid sample preserves previous decoded asset");
    auto empty=directory.getChildFile("empty.wav");empty.create();
    check(readSampleAsset(empty,loaded).failed()&&readSampleAsset(directory.getChildFile("missing.wav"),loaded).failed(),"empty and missing samples fail cleanly");
    Project project;project.sections={{Section::main,8,"Chops"}};project.normalise();project.settings.bpm=125;
    const int track=project.addVocalChopsTrack();for(auto& t:project.tracks)for(auto& c:t.clips)c.notes.clear();
    auto& t=project.tracks[(size_t)track];t.sampler=s;t.instrument="sample";t.instrumentPath=file.getFullPathName().toStdString();t.sampleRoot=60;
    for(int beat=0;beat<32;++beat)t.clips[0].notes.push_back({(double)beat,.125,60+beat%2,100,0});
    AudioEngine engine(AudioEngine::Offline{});if(engine.loadSample(track,file,60,s).failed())throw std::runtime_error("load chops engine");
    const auto retained=engine.sampleAsset(track);check(engine.loadSample(track,invalid,60,s).failed()&&engine.sampleAsset(track)==retained,"failed replacement preserves sounding source");
    engine.publish(project);engine.play(true);juce::AudioBuffer<float> output(2,256);juce::AudioIODeviceCallbackContext context;
    auto callback=[&]{engine.audioDeviceIOCallbackWithContext(nullptr,0,output.getArrayOfWritePointers(),2,256,context);};
    for(int block=0;block<2800;++block)callback();
    check(engine.noteOnsSent(track)==32,"eight bars at 125 BPM trigger each authored chop exactly once");
    engine.stop();project.setLoopRegion(4,8);engine.publish(project);engine.seek(7.75);engine.play(true);
    const auto before=engine.noteOnsSent(track);for(int block=0;block<150;++block)callback();
    check(engine.position()>=4&&engine.position()<8&&engine.noteOnsSent(track)>before,"chop seek and timeline loop stay in beat region");
    project.settings.bpm=150;engine.updateMix(project);const auto beats=engine.position();for(int block=0;block<20;++block)callback();
    check(engine.position()>beats&&output.getMagnitude(0,256)>1e-5f,"tempo change keeps chopped audio active");engine.stop();
    Project reopened;const auto saved=directory.getChildFile("chops.fga");
    check(saveProject(project,saved).wasOk()&&loadProject(saved,reopened).wasOk()&&reopened.tracks.size()==project.tracks.size()&&reopened.tracks[(size_t)track].sampler==t.sampler&&reopened.tracks[(size_t)track].clips==t.clips,"non-destructive slice settings and MIDI survive save/reopen");
    const auto stem=directory.getChildFile("chopped-stem.wav");
    check(engine.exportAudio(project,stem,track,false,[]{return false;},[](double){}).wasOk(),"chopped track exports through existing stem path");
    auto reader=std::unique_ptr<juce::AudioFormatReader>(wav.createReaderFor(stem.createInputStream().release(),true));
    if(!reader)throw std::runtime_error("read exported chops");juce::AudioBuffer<float> read(2,4096);reader->read(&read,0,4096,4096,true,true);
    check(read.getMagnitude(0,4096)>1e-5f,"exported chops contain actual sample audio");reader.reset();
    std::shared_ptr<const SampleAsset> unchanged;check(readSampleAsset(file,unchanged).wasOk()&&unchanged->hash==original->hash,"source file remains unmodified");
    if(failures)throw std::runtime_error("Vocal Chops regression failures: "+std::to_string(failures));
    std::cout<<"PASS Vocal Chops targeted regressions\n";
}
