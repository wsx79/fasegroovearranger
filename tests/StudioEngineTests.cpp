// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/AudioEngine.h"
#include "../Source/StudioIO.h"
#include "../Source/SampleNoteAnalysis.h"
#include <stdexcept>
#include <iostream>
namespace {void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}}
static void testSelectedNotesAndAnalysis(){
    using namespace fga;
    SamplePitchStatistics statistics;SamplePitchReading reading;reading.fresh=true;reading.confidence=1;reading.midi=60;statistics.add(reading);reading.midi=64;for(int i=0;i<3;++i)statistics.add(reading);
    require(statistics.reading(0).midi==64&&statistics.dominant()==64&&statistics.reading(1).midi==63,"pitch statistics retain last, average and dominant independently");
    reading.fresh=false;reading.midi=90;statistics.add(reading);require(statistics.reading(0).midi==64&&statistics.weight==4,"repainting does not count stale pitch readings");
    SampleAsset asset;asset.rate=11025;asset.frames=22050;asset.audio.setSize(2,asset.frames);
    for(int i=0;i<asset.frames;++i){const double hz=i<11025?261.625565:329.627557;const float value=.3f*(float)std::sin(2*juce::MathConstants<double>::pi*hz*i/asset.rate);asset.audio.setSample(0,i,value);asset.audio.setSample(1,i,-value);}
    auto regions=analyseSampleNotes(asset);bool c=false,e=false;for(const auto& region:regions){c=c||std::find(region.notes.begin(),region.notes.end(),60)!=region.notes.end();e=e||std::find(region.notes.begin(),region.notes.end(),64)!=region.notes.end();}require(c&&e,"sample map finds separate notes even in anti-phase stereo");
    for(int i=0;i<asset.frames;++i){float value=0;for(double hz:{261.625565,329.627557,391.995436})value+=.15f*(float)std::sin(2*juce::MathConstants<double>::pi*hz*i/asset.rate);asset.audio.setSample(0,i,value);asset.audio.setSample(1,i,value);}
    regions=analyseSampleNotes(asset);bool chord=false;for(const auto& region:regions)chord=chord||sampleRegionLabel(region.notes)=="C";require(chord,"sample map recognises a synthetic major triad");
    asset.audio.clear();require(analyseSampleNotes(asset).empty(),"sample map never assigns notes to silence");
    std::uint32_t random=73;for(int i=0;i<asset.frames;++i){random=random*1664525u+1013904223u;asset.audio.setSample(0,i,(float)((random>>8)/16777216.-.5));}require(analyseSampleNotes(asset).empty(),"sample map rejects unpitched noise");
    AudioEngine engine;engine.devices.closeAudioDevice();auto project=newProject(1);project.sections={{Section::main,2,"Selection"}};project.loop=false;project.normalise();for(auto& track:project.tracks){track.volume={};for(auto& clip:track.clips)clip.notes.clear();}
    project.tracks[0].clips[0].notes={{0,.25,60,100,0},{.5,.25,64,100,0},{1,.25,67,100,0}};project.tracks[1].clips[0].notes={{.5,.25,36,100,0}};const auto original=midiFile(project);engine.publish(project);
    juce::AudioBuffer<float> block(2,512);juce::AudioIODeviceCallbackContext context;auto tick=[&](int count){for(int n=0;n<count;++n)engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);};
    const auto first=engine.noteOnsSent(0),other=engine.noteOnsSent(1);engine.playSelectedNotes(0,{project.tracks[0].clips[0].notes[1]},0,false);tick(40);
    require(!engine.isPlaying()&&engine.noteOnsSent(0)==first+1&&engine.noteOnsSent(1)==other,"selected-note audition plays only chosen events and stops at their end");
    const auto once=engine.noteOnsSent(0);engine.playSelectedNotes(0,{project.tracks[0].clips[0].notes[1]},0,true);tick(80);
    require(engine.isPlaying()&&engine.noteOnsSent(0)>once+4&&engine.noteOnsSent(1)==other,"selected-note loop repeats only the selected phrase");
    const double auditionPosition=engine.position();auto shifted=project.tracks[0].clips[0].notes[1];shifted.pitch+=12;engine.playSelectedNotes(0,{shifted},0,true,false);require(std::abs(engine.position()-auditionPosition)<1.e-8,"editing a selected audition keeps its playhead position");tick(4);engine.stop();engine.seek(0);const auto resumed=engine.noteOnsSent(0);engine.play(true);tick(100);
    require(engine.noteOnsSent(0)>=resumed+3&&midiFile(project)==original&&!project.loop,"normal playback restores the complete schedule without changing MIDI or loop locators");engine.stop();
}
static void testSamplePitch(){
    using namespace fga;std::array<float,2048> source{};
    for(double hz:{55.,110.,261.625565,440.*std::pow(2.,17./1200.),880.,1568.}){
        for(int i=0;i<2048;++i)source[(size_t)i]=.4f*(float)std::sin(2*juce::MathConstants<double>::pi*hz*i/11025.);
        const auto reading=estimateSamplePitch(source.data(),2048,11025);require(reading.midi>=0&&std::abs(1200*std::log2(reading.hz/hz))<4,"tuner measures bass, melodic notes and detuned tones within four cents");
    }
    for(int i=0;i<2048;++i)source[(size_t)i]=.4f*(float)(std::sin(2*juce::MathConstants<double>::pi*55*i/11025.)*std::exp(-3.*i/2048.));
    auto reading=estimateSamplePitch(source.data(),2048,11025);require(reading.midi>=0&&std::abs(reading.hz-55)<1,"tuner detects a decaying tonal kick body");
    for(int i=0;i<2048;++i)source[(size_t)i]=.4f*(float)(.7*std::sin(2*juce::MathConstants<double>::pi*220*i/11025.)+.4*std::sin(2*juce::MathConstants<double>::pi*330*i/11025.));
    reading=estimateSamplePitch(source.data(),2048,11025);require(reading.midi>=0&&std::abs(reading.hz-110)<1,"tuner follows the fundamental rather than the strongest harmonic");
    source.fill(0);require(estimateSamplePitch(source.data(),2048,11025).midi<0,"silence has no fabricated note");
    std::uint32_t random=47;for(auto& v:source){random=random*1664525u+1013904223u;v=(float)((random>>8)/16777216.-.5);}require(estimateSamplePitch(source.data(),2048,11025).midi<0,"noise has no fabricated note");
    SamplePitchMonitor monitor;monitor.select(0);std::array<float,512> left{},right{};
    for(int block=0;block<30;++block){for(int i=0;i<512;++i){left[(size_t)i]=.3f*(float)std::sin(2*juce::MathConstants<double>::pi*440*(block*512+i)/44100.);right[(size_t)i]=-left[(size_t)i];}monitor.push(0,left.data(),right.data(),512,44100);reading=monitor.poll();}
    require(reading.midi==69&&std::abs(reading.cents)<4,"stereo anti-phase sources remain measurable after real-time decimation");
    monitor.select(1);require(monitor.poll().midi<0,"retargeting discards old sample pitch");monitor.push(0,left.data(),right.data(),512,44100);require(monitor.poll().midi<0,"unselected track cannot contaminate tuner");
}
static void testSampleAudition(){
    using namespace fga;
    auto file=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fga-audition", ".wav");
    struct Cleanup{juce::File f;~Cleanup(){f.deleteFile();}} cleanup{file};
    juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(file.createOutputStream().release(),44100,1,16,{},0));require(writer!=nullptr,"audition fixture writer");
    juce::AudioBuffer<float> source(1,4410);for(int n=0;n<4410;++n)source.setSample(0,n,.4f*std::sin(n*.08f));writer->writeFromAudioSampleBuffer(source,0,4410);writer.reset();
    AudioEngine engine(AudioEngine::Offline{});auto p=newProject(41);for(auto& t:p.tracks)for(auto& c:t.clips)c.notes.clear();
    auto& t=p.tracks[0];t.instrument="sample";t.instrumentPath=file.getFullPathName().toStdString();t.sampler.mode=1;t.sampler.end=.2;t.sampler.fadeIn=0;t.sampler.fadeOut=0;t.mute=true;t.gain=0;
    require(engine.loadSample(0,file,60,t.sampler).wasOk(),"load audition sample");engine.publish(p);engine.seek(2);engine.play(true);
    juce::AudioBuffer<float> block(2,512);juce::AudioIODeviceCallbackContext context;auto tick=[&]{engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);return block.getMagnitude(0,512);};
    require(engine.startSampleAudition(0,false),"start isolated sampler audition");require(tick()>.01f&&!engine.isPlaying()&&engine.position()==2&&engine.sampleAuditionNote(0)==60,"audition sounds before muted track mixer and preserves transport position");
    for(int n=0;n<8;++n)tick();require(!engine.isSampleAuditioning(),"one-pass audition finishes naturally");
    require(engine.startSampleAudition(0,true),"start repeat audition");for(int n=0;n<20;++n)require(tick()>.01f,"repeat retriggers completed regions");
    t.sampler.filter=true;t.sampler.cutoff=20;engine.updateMix(p);for(int n=0;n<10;++n)tick();require(tick()<.01f&&engine.isSampleAuditioning(),"filter edits are audible live during repeat");
    t.sampler.filter=false;t.sampler.start=.1;t.sampler.end=.4;t.sampler.reverse=true;t.sampleRoot=48;engine.updateMix(p);require(tick()>.01f&&engine.sampleAuditionNote(0)==48,"trim, reverse and root edits restart audition with new settings");
    engine.liveMidi(1,juce::MidiMessage::noteOn(1,60,1.f));tick();engine.stopSampleAudition();require(tick()==0,"stop is immediate and audition does not defer MIDI input until after stopping");
    require(!engine.startSampleAudition(-1,true)&&!engine.startSampleAudition(1,true),"invalid or missing sample rejected");
    engine.startSampleAudition(0,true);engine.setSampleAuditionLoop(false);for(int n=0;n<10;++n)tick();require(!engine.isSampleAuditioning(),"turning repeat off finishes the current pass");
    engine.startSampleAudition(0,true);engine.play(true);require(!engine.isSampleAuditioning(),"song playback stops audition");engine.stop();
    engine.startSampleAudition(0,true);require(engine.startRecording(0,0).wasOk()&&!engine.isSampleAuditioning(),"recording stops audition");engine.finishRecording();engine.stop();
    t.sampler.mode=2;t.sampler.sliceCount=2;t.sampler.sliceBase=36;t.sampler.start=0;t.sampler.end=1;t.sampler.slices[0]=0;t.sampler.slices[1]=.5;engine.updateMix(p);engine.startSampleAudition(0,true);require(tick()>.01f&&engine.sampleAuditionNote(0)==36,"sliced audition uses the first mapped slice");
    engine.useInternal(0);tick();require(!engine.isSampleAuditioning(),"instrument replacement stops audition");
}
static void testMuteAndLaneReleases(){
    using namespace fga;
    AudioEngine engine(AudioEngine::Offline{});
    auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fga-mute-regression","",false);folder.createDirectory();
    struct Cleanup{juce::File folder;~Cleanup(){folder.deleteRecursively();}}cleanup{folder};
    const auto sample=folder.getChildFile("sustain.wav");juce::WavAudioFormat wav;
    {std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(sample.createOutputStream().release(),44100,1,16,{},0));require(writer!=nullptr,"create sustained mute fixture");juce::AudioBuffer<float> source(1,44100);for(int n=0;n<44100;++n)source.setSample(0,n,.4f*std::sin(n*.0627f));writer->writeFromAudioSampleBuffer(source,0,44100);}
    auto project=newProject(821);project.sections={{Section::main,2,"Mute"}};project.normalise();project.masterAutomation.enabled=false;
    for(auto& track:project.tracks){track.volume.enabled=false;track.engine.reverb=track.engine.chorus=0;for(auto& clip:track.clips)clip.notes.clear();}
    auto& track=project.tracks[0];track.instrument="sample";track.instrumentPath=sample.getFullPathName().toStdString();track.sampler.mode=0;track.sampler.loop=true;track.sampler.attack=track.sampler.fadeIn=track.sampler.fadeOut=0;track.sampler.release=.01;
    require(engine.loadSample(0,sample,60,track.sampler).wasOk(),"load sustained mute fixture");engine.publish(project);
    juce::AudioBuffer<float> sound(2,512);juce::AudioIODeviceCallbackContext context;
    auto tick=[&]{engine.audioDeviceIOCallbackWithContext(nullptr,0,sound.getArrayOfWritePointers(),2,512,context);};
    // Include a non-default MIDI channel: releasing only channel 1 leaves voices held.
    engine.liveMidi(0,juce::MidiMessage::controllerEvent(3,64,127));engine.liveMidi(0,juce::MidiMessage::noteOn(3,60,(juce::uint8)110));tick();
    engine.liveMidi(0,juce::MidiMessage::noteOff(3,60));tick();require(engine.activeNotes(0)[60],"pedal holds the released sampler note before mute");
    track.mute=true;engine.updateMix(project);for(int n=0;n<64;++n)tick();
    require(!engine.activeNotes(0)[60],"mute releases the real sampler voice including sustain pedal");
    track.mute=false;engine.updateMix(project);tick();require(!engine.activeNotes(0)[60]&&sound.getMagnitude(0,512)<1.e-6f,"unmute does not revive the sustained live note");
    engine.useInternal(0);track.instrument="internal";
    for(int ti:{3,leadTrack}){
        engine.stop();tick();for(auto& t:project.tracks)for(auto& clip:t.clips)clip.notes.clear();
        auto& part=project.tracks[(size_t)ti];part.engine.laneMute.fill(false);part.clips[0].notes={{0,.5,60,100,1},{1,.5,64,100,0}};
        engine.publish(project);engine.play(true);tick();require(engine.activeNotes(ti)[60],"guitar/lead note starts before lane mute");
        part.engine.laneMute[1]=true;engine.updateMix(project);tick();require(!engine.activeNotes(ti)[60],"muting guitar/lead lane releases its active note");
        while(engine.position()<1.1)tick();require(engine.activeNotes(ti)[64],"an unmuted guitar/lead lane still plays");
        while(engine.position()<1.8)tick();require(!engine.activeNotes(ti)[60]&&!engine.activeNotes(ti)[64],"muted and unmuted lane note-offs leave no stuck notes");
    }
    engine.stop();tick();
}
static void testRecordedRegions(){
    using namespace fga;
    const auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fga-audio-regions","",false);folder.createDirectory();struct Cleanup{juce::File folder;~Cleanup(){folder.deleteRecursively();}}cleanup{folder};
    const auto file=folder.getChildFile("two-tones.wav");constexpr int rate=11025,frames=rate*4;juce::AudioBuffer<float> signal(2,frames);
    for(int n=0;n<frames;++n){const float value=.2f*(float)std::sin(juce::MathConstants<double>::twoPi*(n<2*rate?220.:880.)*n/rate);for(int ch=0;ch<2;++ch)signal.setSample(ch,n,value);}
    juce::WavAudioFormat wav;{auto stream=file.createOutputStream();std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(),rate,2,24,{},0));require(writer!=nullptr,"audio-region WAV fixture");stream.release();require(writer->writeFromAudioSampleBuffer(signal,0,frames),"write audio-region fixture");}
    Project p;p.normalise();p.sections={{Section::main,2,"Audio"}};p.settings.bpm=120;p.settings.freeTempo=true;p.normalise();for(auto& t:p.tracks)for(auto& c:t.clips)c.notes.clear();AudioTake take;take.file=file;take.rate=rate;take.frames=frames;take.tempo=120;const int track=addAudioTake(p,take,false);require(track>=0,"add recorded audio region");
    AudioEngine engine(AudioEngine::Offline{});require(engine.loadSample(track,file,60,p.tracks[(size_t)track].sampler).wasOk(),"load recorded region sample");
    auto render=[&](const Project& project,const juce::String& name){const auto output=folder.getChildFile(name);require(engine.exportAudio(project,output,track,false,[]{return false;},[](double){},nullptr,{rate,24}).wasOk(),"export edited audio region");std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(output.createInputStream().release(),true));require(reader&&reader->lengthInSamples>=frames,"read audio-region export");juce::AudioBuffer<float> samples(2,frames);require(reader->read(&samples,0,frames,0,true,true),"decode edited audio region");return samples;};
    const auto original=render(p,"original.wav");auto split=p;require(split.splitClip(track,0,4)==1,"split a recorded take in half");const auto edited=render(split,"split.wav");float error=0;for(int c=0;c<2;++c)for(int n=0;n<frames;++n)error=std::max(error,std::abs(original.getSample(c,n)-edited.getSample(c,n)));require(error<2.e-6f,"C3: split export preserves the original waveform instead of restarting and layering the sample");
    auto copy=split;copy.deleteClip(track,0);const int right=copy.clipAtBeat(track,4);require(right>=0&&copy.moveClip(track,right,track,0,false)>=0,"move the right audio fragment");const auto moved=render(copy,"moved.wav");for(int n=rate/2;n<rate*3/2;++n)require(std::abs(moved.getSample(0,n)-original.getSample(0,n+rate*2))<2.e-6f,"C3: moving a right fragment retains the source offset");
    auto copied=split;require(copied.moveClip(track,1,track,0,true)>=0,"copy the right audio fragment over the left");const auto duplicated=render(copied,"copied.wav");for(int n=rate/2;n<rate*3/2;++n){require(std::abs(duplicated.getSample(0,n)-original.getSample(0,n+rate*2))<2.e-6f,"C3: duplicated audio preserves its source offset");require(std::abs(duplicated.getSample(0,n+rate*2)-original.getSample(0,n+rate*2))<2.e-6f,"C3: copying preserves the original source fragment");}
    auto overwrite=p;Clip empty;empty.span=2;require(overwrite.placeClip(track,empty,2)>=0,"overwrite part of a recorded take");const auto replaced=render(overwrite,"overwrite.wav");require(replaced.getMagnitude(rate,rate)<2.e-6f,"C3: overwriting a region stops the left voice at its boundary");for(int n=rate*5/2;n<rate*7/2;++n)require(std::abs(replaced.getSample(0,n)-original.getSample(0,n))<2.e-6f,"C3: retained overwrite tail reads the correct source audio");
    engine.publish(split);engine.seek(5);engine.play(true);juce::AudioBuffer<float> block(2,512);juce::AudioIODeviceCallbackContext context;engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);require(block.getMagnitude(0,512)>.001,"audio seek chases the containing region");engine.stop();
    engine.publish(p);engine.seek(1);engine.play(true);engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);const auto beforeSwap=engine.noteOnsSent(track);auto swapped=split;require(swapped.moveClip(track,1,track,0,true)>=0,"replace playing audio with a different source offset");engine.publish(swapped);engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);require(engine.noteOnsSent(track)>beforeSwap,"C3: editing a sounding region chases its new source even with the same pitch, beat and velocity");engine.stop();
    // C4 uses a valid six-minute header with all frames present, at low sample
    // rate to keep the fixture small. It checks the real insertion path.
    const auto longFile=folder.getChildFile("six-minutes.wav");juce::AudioBuffer<float> silence(2,8000);silence.clear();{auto stream=longFile.createOutputStream();std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(),8000,2,16,{},0));require(writer!=nullptr,"long take writer");stream.release();for(int second=0;second<360;++second)require(writer->writeFromAudioSampleBuffer(silence,0,8000),"write complete long take");}
    AudioTake longTake;longTake.file=longFile;longTake.rate=8000;longTake.frames=8000*360;longTake.tempo=120;const int longTrack=addAudioTake(p,longTake,false);require(longTrack>=0&&p.clipLength(longTrack,0)==720&&p.tracks[(size_t)longTrack].clips[0].notes[0].length==720,"C4: six-minute take remains 720 beats on the timeline");
    std::cout<<"Recorded-region regressions: split max error="<<error<<", move/copy/overwrite verified, long take="<<p.clipLength(longTrack,0)<<" beats\n";
}
void runStudioEngineTests(){
    testRecordedRegions();
    testMuteAndLaneReleases();
    {
        auto asset=std::make_shared<fga::SampleAsset>();asset->audio.setSize(1,256);asset->audio.clear();asset->frames=256;fga::SamplerSettings initial;initial.mode=1;
        fga::SampleSynth synth(asset,initial,60,false);
        {auto external=std::make_unique<fga::SamplerSettings>(initial);external->start=.25;synth.configure(*external,60,120);external->start=.75;require(synth.settings().start==.25,"sampler settings belong to the synth, not a retiring publisher");}
        auto next=initial;next.start=.25;require(!synth.configure(next,60,120)&&synth.settings().start==.25,"reconfigure compares a live owned snapshot after publisher reclamation");
    }
    testSelectedNotesAndAnalysis();testSamplePitch();
    testSampleAudition();
    {fga::MixerBuses graph;graph.prepare(48000,512);auto settings=fga::defaultBuses();settings[0].gain=.5;graph.configure(settings,120,.5,true);graph.begin(512,false);graph.add(0,0,1,0,{0,0});juce::AudioBuffer<float> out(2,512);out.clear();graph.finish(out.getArrayOfWritePointers(),2,512,nullptr);require(std::abs(out.getSample(0,0)-.25)<1e-6,"group and master gain apply once");settings[1].solo=true;graph.configure(settings,120,.5,true);graph.begin(512,false);graph.add(0,0,1,0,{1,1});out.clear();graph.finish(out.getArrayOfWritePointers(),2,512,nullptr);require(out.getMagnitude(0,512)==0,"group solo excludes other groups and their sends");graph.reset();settings[1].solo=false;graph.configure(settings,120,0,true);graph.begin(512,false);graph.add(0,0,1,0,{1,1});out.clear();graph.finish(out.getArrayOfWritePointers(),2,512,nullptr);require(out.getMagnitude(0,512)==0,"master silences dry and shared returns");}
    {fga::AudioEngine recorder;recorder.devices.closeAudioDevice();auto project=fga::newProject(81);project.sections={{fga::Section::main,1,"Main"}};project.normalise();for(auto& track:project.tracks)for(auto& clip:track.clips)clip.notes.clear();recorder.publish(project);require(recorder.startRecording(0,1).wasOk(),"start actual MIDI capture");juce::AudioBuffer<float> audio(2,512);juce::AudioIODeviceCallbackContext context;auto callback=[&]{recorder.audioDeviceIOCallbackWithContext(nullptr,0,audio.getArrayOfWritePointers(),2,512,context);};callback();require(recorder.position()==0&&recorder.countInRemaining()>0,"count-in holds the song cursor");for(int i=0;i<1000&&recorder.countInRemaining()>0;++i)callback();recorder.liveMidi(0,juce::MidiMessage::noteOn(1,67,(juce::uint8)107));for(int i=0;i<20;++i)callback();recorder.liveMidi(0,juce::MidiMessage::noteOff(1,67));callback();auto captured=recorder.finishRecording();fga::assembleMidiTake(captured,0);require(captured.notes.size()==1&&captured.notes[0].pitch==67&&captured.notes[0].velocity==107&&captured.notes[0].length>.1,"audio callback records queued MIDI with duration and velocity");recorder.stop();require(recorder.startRecording(0,1).wasOk(),"start sustained count-in recording");recorder.liveMidi(0,juce::MidiMessage::controllerEvent(1,64,127));recorder.liveMidi(0,juce::MidiMessage::noteOn(1,69,(juce::uint8)91));callback();recorder.liveMidi(0,juce::MidiMessage::noteOff(1,69));callback();for(int i=0;i<1000&&recorder.countInRemaining()>0;++i)callback();for(int i=0;i<20;++i)callback();recorder.liveMidi(0,juce::MidiMessage::controllerEvent(1,64,0));callback();auto sustained=recorder.finishRecording();fga::assembleMidiTake(sustained,0);require(sustained.notes.size()==1&&sustained.notes[0].pitch==69&&sustained.notes[0].length>.1,"sustained note from count-in is captured until pedal release");recorder.stop();}
    fga::MidiTake take;take.track=0;take.start=0;take.duration=3;
    take.events={{.12,.12,0x90,60,100},{.2,.2,0xb0,64,127},{.4,.4,0x80,60,0},{1.2,1.2,0xb0,64,0}};
    fga::assembleMidiTake(take,.25);require(take.notes.size()==1&&take.notes[0].beat==0&&std::abs(take.notes[0].length-1.08)<1e-9,"quantisation preserves sustained duration");
    auto p=fga::newProject(12);for(auto& c:p.tracks[0].clips)c.notes.clear();require(fga::applyMidiTake(p,take,true),"recorded notes enter project");auto original=p;p.tracks[0].locked=true;require(!fga::applyMidiTake(p,take,false),"recording protects locked tracks");
    {auto sparse=fga::newProject(51);const int track=sparse.addMidiTrack();auto& timeline=sparse.tracks[(size_t)track];timeline.freeTimeline=true;timeline.clips.clear();fga::Clip first;first.position=0;first.span=1;first.notes={{0,.25,60,100,0}};timeline.clips.push_back(first);sparse.normalise();const auto previous=timeline.clips[0];fga::MidiTake gap;gap.track=track;gap.start=2;gap.duration=2;gap.notes={{2.5,1,72,105}};require(fga::applyMidiTake(sparse,gap,true)&&timeline.clips.size()==2&&timeline.clips[0]==previous&&timeline.clips[1].notes[0].pitch==72,"recording creates a clip in a free-timeline gap without overlapping or changing the preceding clip");}
    fga::MidiTake loop;loop.track=0;loop.looping=true;loop.loopStart=4;loop.loopEnd=8;loop.start=7;loop.duration=3;loop.events={{7.5,.5,0x90,64,90},{5,2,0x80,64,0}};fga::assembleMidiTake(loop,0);require(loop.notes.size()==2&&loop.notes[0].beat==4&&loop.notes[1].beat==7.5,"recorded notes split across loop wrap");
    auto asset=std::make_shared<fga::SampleAsset>();asset->rate=48000;asset->frames=48000;asset->audio.setSize(1,48004);asset->audio.clear();for(int i=0;i<48000;++i)asset->audio.setSample(0,i,.4f*std::sin(float(i*juce::MathConstants<double>::twoPi*440/48000)));
    auto render=[&](fga::SamplerSettings settings,int pitch,int off,double bpm){fga::SampleSynth synth(asset,settings,60,false);synth.setCurrentPlaybackSampleRate(48000);synth.configure(settings,60,bpm);juce::AudioBuffer<float> output(2,96000);output.clear();juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::noteOn(1,pitch,1.f),0);if(off>=0)midi.addEvent(juce::MidiMessage::noteOff(1,pitch),off);synth.renderNextBlock(output,midi,0,96000);return output;};
    fga::SamplerSettings settings;settings.mode=1;settings.attack=0;settings.fadeIn=0;settings.fadeOut=0;auto oneshot=render(settings,60,100,120);require(oneshot.getMagnitude(20000,1000)>.1f,"one-shot ignores early note-off");settings.mode=0;settings.release=.01;auto gate=render(settings,60,100,120);require(gate.getMagnitude(20000,1000)<1e-7,"gate obeys early note-off");
    settings.mode=1;settings.release=.1;settings.attack=0;settings.fadeIn=.05;settings.fadeOut=0;settings.fadeInCurve=0;auto linearFade=render(settings,60,-1,120);settings.fadeInCurve=1;auto bentFade=render(settings,60,-1,120);require(bentFade.getMagnitude(40,160)+1e-5f<linearFade.getMagnitude(40,160),"fade-in curve bends the sample amplitude");settings.fadeIn=0;settings.fadeInCurve=0;settings.attack=.05;settings.attackCurve=0;auto linearAttack=render(settings,60,-1,120);settings.attackCurve=1;auto bentAttack=render(settings,60,-1,120);require(bentAttack.getMagnitude(40,160)+1e-5f<linearAttack.getMagnitude(40,160),"attack curve bends the amplitude envelope");settings.attack=0;settings.attackCurve=0;
    {fga::SamplerSettings round;round.attackCurve=.5;round.fadeOutCurve=-.25;fga::SamplerSettings loaded;require(fga::readSampler(fga::samplerJson(round),loaded).wasOk()&&loaded.attackCurve==.5&&loaded.fadeOutCurve==-.25,"envelope curves save and load");auto legacy=fga::samplerJson(fga::SamplerSettings{});legacy.getDynamicObject()->removeProperty("attackCurve");fga::SamplerSettings omitted;require(fga::readSampler(legacy,omitted).wasOk()&&omitted.attackCurve==0,"an omitted curve stays linear");}
    settings.mode=1;settings.transpose=12;auto pitched=render(settings,60,-1,120);require(pitched.getMagnitude(30000,1000)<1e-7&&pitched.getMagnitude(10000,1000)>.1f,"classic octave doubles speed and halves duration");
    settings.sync=true;settings.sourceBpm=120;auto stretched=render(settings,60,-1,120);require(stretched.getMagnitude(30000,1000)>.05f,"tempo sync preserves duration across transposition");
    int crossings=0;for(int i=20001;i<30000;++i)if(stretched.getSample(0,i-1)<0&&stretched.getSample(0,i)>=0)++crossings;require(crossings>170&&crossings<195,"tempo sync octave transposes actual audio frequency");
    settings.transpose=0;auto faster=render(settings,60,-1,240);require(faster.getMagnitude(10000,1000)>.05f&&faster.getMagnitude(35000,1000)<1e-7,"tempo sync changes duration with BPM");
    settings.sync=false;settings.mode=2;settings.sliceCount=2;settings.slices[0]=0;settings.slices[1]=.5;settings.sliceBase=36;auto slice=render(settings,37,-1,120);require(slice.getMagnitude(10000,1000)>.1f&&slice.getMagnitude(30000,1000)<1e-7,"slice trigger plays bounded region");auto unmapped=render(settings,60,-1,120);require(unmapped.getMagnitude(0,96000)==0,"unmapped slice pitches are silent");
    settings=fga::SamplerSettings{};settings.mode=1;settings.attack=0;settings.fadeIn=settings.fadeOut=0;settings.end=.25;settings.reverse=true;auto reverse=render(settings,60,-1,120);require(std::abs(reverse.getSample(0,0)-asset->audio.getSample(0,11999))<1e-5&&reverse.getMagnitude(20000,1000)==0,"reverse begins at the crop end and respects trim length");
    settings.reverse=false;settings.end=1;settings.filter=true;settings.cutoff=80;settings.resonance=0;auto filtered=render(settings,60,-1,120);require(filtered.getMagnitude(20000,2000)<.04,"low-pass filter attenuates actual sample audio");
    settings.filter=false;settings.mode=0;settings.loop=true;settings.loopStart=.01;settings.loopEnd=.231;settings.crossfade=.01;auto looped=render(settings,60,-1,120);require(looped.getMagnitude(80000,1000)>.1f,"loop sustains beyond the source duration");require(std::abs(looped.getSample(0,11088)-looped.getSample(0,11087))<.1f,"crossfade softens a non-periodic loop seam");

    {
        const int at=11000;const double distance=11088.-at;
        const float weight=(float)(.5+.5*std::sin(distance/480.*juce::MathConstants<double>::halfPi));
        const float expected=weight*asset->audio.getSample(0,at)+(1-weight)*asset->audio.getSample(0,at-10608);
        require(std::abs(looped.getSample(0,at)-expected)<1.e-4f,"crossfade reads the incoming waveform forward instead of reflecting it");
    }
    for(bool reversed:{false,true}){
        settings.reverse=reversed;settings.loopStart=0;settings.loopEnd=1;settings.crossfade=.01;
        auto fullLoop=render(settings,60,-1,120);
        require(std::abs(fullLoop.getSample(0,48000)-fullLoop.getSample(0,47999))<.1f,"full-crop crossfade stays continuous in both playback directions");
    }

    {auto directory=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fga-studio-export","",false);directory.createDirectory();struct Cleanup{juce::File file;~Cleanup(){file.deleteRecursively();}}cleanup{directory};auto sample=directory.getChildFile("long.wav");juce::WavAudioFormat wav;{std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(sample.createOutputStream().release(),44100,1,16,{},0));require(writer!=nullptr,"create long export fixture");juce::AudioBuffer<float> source(1,44100*6);for(int n=0;n<source.getNumSamples();++n)source.setSample(0,n,.3f*std::sin(n*.041f));writer->writeFromAudioSampleBuffer(source,0,source.getNumSamples());}
        fga::AudioEngine engine;engine.devices.closeAudioDevice();auto project=fga::newProject(19);project.settings.bpm=120;project.sections={{fga::Section::main,1,"Main"}};project.normalise();for(auto& track:project.tracks)for(auto& clip:track.clips)clip.notes.clear();auto& track=project.tracks[0];track.instrument="sample";track.instrumentPath=sample.getFullPathName().toStdString();track.sampleRoot=60;track.sampler.mode=1;track.clips[0].notes={{3.5,.1,60,110,0}};require(engine.loadSample(0,sample,60,track.sampler).wasOk(),"load long one-shot");engine.publish(project);auto clone=engine.offlineCopy();require(clone!=nullptr,"sampler offline copy");auto first=directory.getChildFile("live.wav"),second=directory.getChildFile("copy.wav");require(engine.exportAudio(project,first,0,false,[]{return false;},[](double){}).wasOk()&&clone->exportAudio(project,second,0,false,[]{return false;},[](double){}).wasOk(),"export sampler and private copy");juce::MemoryBlock a,b;first.loadFileAsData(a);second.loadFileAsData(b);require(a==b,"offline copy preserves sample settings and identical audio");std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(first.createInputStream().release(),true));require(reader&&reader->lengthInSamples/reader->sampleRate>10,"export extends for complete one-shot tail");juce::AudioBuffer<float> tail(2,4410);reader->read(&tail,0,4410,44100*6,true,true);require(tail.getMagnitude(0,4410)>.01,"long sample remains audible after the old three-second tail limit");juce::AudioBuffer<float> noteBlock(2,512);juce::AudioIODeviceCallbackContext noteContext;engine.liveMidi(0,juce::MidiMessage::noteOn(1,60,1.f));engine.audioDeviceIOCallbackWithContext(nullptr,0,noteBlock.getArrayOfWritePointers(),2,512,noteContext);engine.liveMidi(0,juce::MidiMessage::noteOff(1,60));engine.audioDeviceIOCallbackWithContext(nullptr,0,noteBlock.getArrayOfWritePointers(),2,512,noteContext);require(engine.activeNotes(0)[60],"one-shot key indication follows the sounding tail after note-off");engine.stop();engine.audioDeviceIOCallbackWithContext(nullptr,0,noteBlock.getArrayOfWritePointers(),2,512,noteContext);require(!engine.activeNotes(0)[60],"stop clears the actual sounding-key indication");track.sampler.mode=0;track.sampler.loop=true;track.sampler.release=.01;engine.publish(project);engine.setMidiMonitor(0,true);engine.liveMidi(0,juce::MidiMessage::controllerEvent(1,64,127));engine.liveMidi(0,juce::MidiMessage::noteOn(1,60,1.f));juce::AudioBuffer<float> block(2,512);juce::AudioIODeviceCallbackContext context;engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);require(engine.level(0)>.001&&engine.activeNotes(0)[60],"live sampler monitoring plays input and exposes its sounding key");engine.liveMidi(0,juce::MidiMessage::noteOff(1,60));engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);require(engine.level(0)>.001,"sustain holds the released live key");engine.setMidiMonitor(1,true);for(int n=0;n<20;++n)engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);require(engine.level(0)<1e-6&&!engine.activeNotes(0)[60],"changing monitored track releases held notes and clears the sounding-key display");engine.setMidiMonitor(1,false);}

}

void runRecordedRegionTests(){testRecordedRegions();}
