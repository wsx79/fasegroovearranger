// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/AudioEngine.h"
#include "../Source/RetiredJobs.h"
#include <thread>
#include <stdexcept>
namespace fga {
namespace {
void requireAudit(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
class AuditGain final : public juce::HostedAudioProcessorParameter {
public:
    float get()const{return value.load()*2.f;}float getValue()const override{return value.load();}void setValue(float v)override{value=std::clamp(v,0.f,1.f);}float getDefaultValue()const override{return .75f;}
    juce::String getName(int)const override{return "Gain";}juce::String getLabel()const override{return {};}
    juce::String getText(float v,int)const override{return juce::String(v*2,2);}float getValueForText(const juce::String& s)const override{return s.getFloatValue()/2;}
    juce::String getParameterID()const override{return "gain";}
private:std::atomic<float> value{.75f};
};
class AuditPlugin : public juce::AudioPluginInstance {
public:
    AuditPlugin(bool effect=false,float shift=0,bool mono=false,float right=1):AudioPluginInstance(effect?BusesProperties().withInput("Input",mono?juce::AudioChannelSet::mono():juce::AudioChannelSet::stereo(),true).withOutput("Output",mono?juce::AudioChannelSet::mono():juce::AudioChannelSet::stereo(),true):BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)),transform(effect),offset(shift),rightScale(right){auto parameter=std::make_unique<AuditGain>();gain=parameter.get();addHostedParameter(std::move(parameter));}
    std::atomic<bool> inside{false},overlap{false};std::atomic<int> stateReads{0};juce::WaitableEvent* snapshotStarted=nullptr;juce::WaitableEvent* snapshotResume=nullptr;bool slow=false,prepared=false,stateRestoredActive=false;int preparations=0;bool offlineObserved=false;int calls=0,samples=0,lastMidi=-1;
    std::array<int,1024> sizes{},starts{};std::array<float,1024> values{};std::array<double,1024> positions{};
    const juce::String getName()const override{return "Audit probe";}
    void fillInPluginDescription(juce::PluginDescription& description)const override{description.name=getName();}
    void prepareToPlay(double,int)override{prepared=true;++preparations;}
    void releaseResources()override{prepared=false;}
    void processBlock(juce::AudioBuffer<float>& block,juce::MidiBuffer& midi)override{
        inside=true;offlineObserved=offlineObserved||isNonRealtime();if(slow)juce::Thread::sleep(2);if(calls<(int)sizes.size()){sizes[(size_t)calls]=block.getNumSamples();starts[(size_t)calls]=samples;values[(size_t)calls]=gain->get();if(auto* head=getPlayHead())if(auto p=head->getPosition())positions[(size_t)calls]=p->getPpqPosition().orFallback(-1.);}++calls;
        for(auto event:midi)if(event.getMessage().isNoteOn())lastMidi=samples+event.samplePosition;
        for(int c=0;c<block.getNumChannels();++c){if(transform){block.applyGain(c,0,block.getNumSamples(),gain->get());for(int i=0;i<block.getNumSamples();++i)block.addSample(c,i,offset);}else juce::FloatVectorOperations::fill(block.getWritePointer(c),gain->get()*(c==1?rightScale:1.f),block.getNumSamples());}samples+=block.getNumSamples();inside=false;
    }
    bool acceptsMidi()const override{return true;}bool producesMidi()const override{return false;}bool isMidiEffect()const override{return false;}
    double getTailLengthSeconds()const override{return 0;}int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
    const juce::String getProgramName(int)override{return "Audit";}void changeProgramName(int,const juce::String&)override{}
    bool hasEditor()const override{return false;}juce::AudioProcessorEditor* createEditor()override{return nullptr;}
    void getStateInformation(juce::MemoryBlock& data)override{if(snapshotStarted)snapshotStarted->signal();if(snapshotResume)snapshotResume->wait(5000);if(inside)overlap=true;++stateReads;juce::Thread::sleep(1);data.setSize(sizeof(float));const float value=gain->get();std::memcpy(data.getData(),&value,sizeof value);}
    void setStateInformation(const void*,int)override{if(inside)overlap=true;stateRestoredActive=prepared;}
private:AuditGain* gain=nullptr;bool transform=false;float offset=0,rightScale=1;
};
// A native chunk may contain sound edits but leave program selection to the
// host. Reloading that program afterwards destroys edits that are not exposed.
class ProgramStatePlugin final : public AuditPlugin {
public:
    int program=0;float privateEdit=.1f;
    int getNumPrograms()override{return 3;}int getCurrentProgram()override{return program;}
    void setCurrentProgram(int p)override{program=p;privateEdit=.1f;}
    const juce::String getProgramName(int p)override{return juce::String(p);}
    void getStateInformation(juce::MemoryBlock& data)override{data.replaceAll(&privateEdit,sizeof privateEdit);}
    void setStateInformation(const void* data,int bytes)override{if(bytes==sizeof privateEdit)std::memcpy(&privateEdit,data,sizeof privateEdit);}
};
class CompletePresetPlugin final : public AuditPlugin {
public:
    bool full=true;
    void getStateInformation(juce::MemoryBlock& dest)override{
        const juce::String preset=full?R"({"data_type":"program","general_program_information":{"name":"Saved piano","triton_bank_number":16,"triton_program_number":0},"dsp_settings":[{"index":0,"key":"gain","value":0.81}]})":R"({"data_type":"partial","dsp_settings":[]})";
        juce::MemoryBlock component(preset.toRawUTF8(),preset.getNumBytesAsUTF8()+1);juce::XmlElement xml("VST3PluginState");xml.createNewChildElement("IComponent")->addTextElement(component.toBase64Encoding());copyXmlToBinary(xml,dest);
    }
    void setStateInformation(const void*,int)override{stateRestoredActive=prepared;getParameters()[0]->setValue(.81f);}
};
class MidiDeliveryProbe final : public AuditPlugin {
public:
    std::array<bool,2048> pressed{},ringing{};std::array<bool,16> pedal{};
    void processBlock(juce::AudioBuffer<float>& block,juce::MidiBuffer& midi)override{
        ++calls;inside=true;
        for(auto e:midi){const auto m=e.getMessage();const int ch=m.getChannel()-1;if(ch<0)continue;const size_t base=(size_t)ch*128;
            if(m.isNoteOn()){pressed[base+(size_t)m.getNoteNumber()]=ringing[base+(size_t)m.getNoteNumber()]=true;}
            else if(m.isNoteOff()){pressed[base+(size_t)m.getNoteNumber()]=false;if(!pedal[(size_t)ch])ringing[base+(size_t)m.getNoteNumber()]=false;}
            else if(m.isController()&&m.getControllerNumber()==64){pedal[(size_t)ch]=m.getControllerValue()>=64;if(!pedal[(size_t)ch])for(size_t n=0;n<128;++n)if(!pressed[base+n])ringing[base+n]=false;}
            else if(m.isAllSoundOff()||m.isAllNotesOff())for(size_t n=0;n<128;++n)pressed[base+n]=ringing[base+n]=false;
        }
        const bool active=std::any_of(ringing.begin(),ringing.end(),[](bool n){return n;});block.clear();if(active)for(int c=0;c<block.getNumChannels();++c)juce::FloatVectorOperations::fill(block.getWritePointer(c),.02f,block.getNumSamples());inside=false;
    }
};
struct ParameterObserver final : juce::AudioProcessorParameter::Listener {
    int changes=0;float value=0;
    void parameterValueChanged(int,float v)override{++changes;value=v;}
    void parameterGestureChanged(int,bool)override{}
};
// Drives the real device lifecycle without depending on the runner's audio hardware.
class AuditDevice final : public juce::AudioIODevice {
public:
    AuditDevice(double rate,int block):AudioIODevice("Audit device","Test"),rate(rate),block(block){}
    juce::StringArray getOutputChannelNames()override{return {"Left","Right"};}
    juce::StringArray getInputChannelNames()override{return {"Left","Right"};}
    juce::Array<double> getAvailableSampleRates()override{return {44100,48000,96000};}
    juce::Array<int> getAvailableBufferSizes()override{return {32,64,128,512,1024};}
    int getDefaultBufferSize()override{return block;}
    juce::String open(const juce::BigInteger&,const juce::BigInteger&,double,int)override{return {};}
    void close()override{}
    bool isOpen()override{return true;}
    void start(juce::AudioIODeviceCallback*)override{}
    void stop()override{}
    bool isPlaying()override{return true;}
    juce::String getLastError()override{return {};}
    int getCurrentBufferSizeSamples()override{return block;}
    double getCurrentSampleRate()override{return rate;}
    int getCurrentBitDepth()override{return 32;}
    juce::BigInteger getActiveOutputChannels()const override{return juce::BigInteger(3);}
    juce::BigInteger getActiveInputChannels()const override{return juce::BigInteger(3);}
    int getOutputLatencyInSamples()override{return block;}
    int getInputLatencyInSamples()override{return block/2;}
private:
    double rate;int block;
};
}
struct EngineAuditTests {
    static void deviceTransportMatrix(){
        AudioEngine engine(AudioEngine::Offline{});auto project=newProject(0);
        project.settings.bpm=120;project.settings.freeTempo=true;
        project.sections={{Section::main,2,"Device matrix"}};project.normalise();
        project.loop=true;project.loopStart=1;project.loopEnd=2;
        for(auto& track:project.tracks){track.gain=0;track.volume.enabled=false;for(auto& clip:track.clips)clip.notes.clear();}
        project.tracks[0].gain=1;project.tracks[0].clips[0].notes={{0,8,60,100,0}};
        juce::AudioIODeviceCallbackContext context;
        for(double rate:{44100.,48000.,96000.})for(int size:{32,64,128,512,1024}){
            AuditDevice device(rate,size);engine.audioDeviceAboutToStart(&device);
            auto source=std::make_unique<MidiDeliveryProbe>();auto* plugin=source.get();
            engine.swapInstrument(0,std::move(source),0,{});engine.publish(project);engine.play(true);
            juce::AudioBuffer<float> block(2,size);
            auto tick=[&]{
                engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,size,context);
                for(int ch=0;ch<2;++ch)for(int s=0;s<size;++s)requireAudit(std::isfinite(block.getSample(ch,s)),"device matrix: non-finite callback output");
            };
            const double advance=size*120./(rate*60.);
            engine.seek(2-advance*.5);tick();
            requireAudit(engine.position()>=1&&engine.position()<1+advance&&plugin->ringing[60],"device matrix: loop split/seek must chase the held note");
            project.tracks[0].mute=true;engine.updateMix(project);engine.collectGarbage();tick();
            requireAudit(engine.level(0)==0,"device matrix: mute must silence its track");
            project.tracks[0].mute=false;project.tracks[1].solo=true;engine.updateMix(project);engine.collectGarbage();tick();
            requireAudit(engine.level(0)==0,"device matrix: another solo must silence the track");
            project.tracks[1].solo=false;engine.updateMix(project);engine.collectGarbage();tick();
            requireAudit(engine.level(0)>0,"device matrix: removing mute/solo must restore audio");
            for(int n=0;n<24;++n){
                project.tracks[0].clips[0].notes[0].velocity=80+n;
                engine.publish(project,true,0);engine.collectGarbage();tick();
            }
            auto replacement=std::make_unique<MidiDeliveryProbe>();plugin=replacement.get();
            engine.swapInstrument(0,std::move(replacement),0,{});tick();
            requireAudit(plugin->ringing[60]&&engine.isPlaying(),"device matrix: swapping a plugin must recover held MIDI without stopping transport");
            engine.stop();tick();
            requireAudit(std::none_of(plugin->ringing.begin(),plugin->ringing.end(),[](bool on){return on;})&&block.getMagnitude(0,size)==0,
                         "device matrix: stop must release hosted notes and silence output");
            engine.play(true);tick();
            requireAudit(plugin->ringing[60]&&engine.position()>0,"device matrix: play after stop must restart MIDI");
            engine.stop();engine.audioDeviceStopped();
        }
        std::cout<<"Device transport matrix: 15 configurations (32/64/128/512/1024, 44.1/48/96 kHz) passed\n";
    }
    static void exportDeviceChanges(){
        for(bool cancel:{false,true})for(auto configuration:{std::pair<double,int>{44100,1024},{48000,512},{96000,1024}}){
            AudioEngine engine(AudioEngine::Offline{});auto project=newProject(0);
            project.sections={{Section::main,1,"Export device change"}};project.normalise();
            for(auto& track:project.tracks)for(auto& clip:track.clips)clip.notes.clear();
            auto probe=std::make_unique<AuditPlugin>();auto* plugin=probe.get();
            engine.swapInstrument(0,std::move(probe),0,{});engine.publish(project);engine.play(true);
            const auto file=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fga-export-device",".wav",false);
            AuditDevice device(configuration.first,configuration.second);bool changed=false,exportIntact=true;
            juce::AudioBuffer<float> block(2,configuration.second);juce::AudioIODeviceCallbackContext context;
            const auto result=engine.exportAudio(project,file,0,false,[&]{return cancel&&changed;},[&](double){
                if(changed)return;changed=true;engine.audioDeviceStopped();engine.audioDeviceAboutToStart(&device);
                const auto calls=plugin->calls;
                engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,block.getNumSamples(),context);
                exportIntact=engine.sampleRate==44100&&engine.blockSize==512&&plugin->getSampleRate()==44100&&plugin->calls==calls;
            });
            bool wavValid=!file.existsAsFile();
            if(!cancel){juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(file.createInputStream().release(),true));wavValid=reader&&reader->sampleRate==44100;}
            file.deleteFile();
            requireAudit(changed&&exportIntact&&wavValid&&result.wasOk()!=cancel&&!engine.isExporting(),"device restart must not alter the active export or leave a cancelled WAV");
            requireAudit(plugin->getSampleRate()==configuration.first&&plugin->getBlockSize()==configuration.second,
                         "export completion/cancellation must reprepare plugins for a changed device rate/buffer");
            for(const auto& instrument:engine.instruments)
                requireAudit(instrument->scratch.getNumSamples()==configuration.second&&instrument->volumeGains.size()==(size_t)configuration.second,
                             "export restore must prepare all realtime buffers before playback resumes");
            requireAudit(engine.deviceOutputLatency==configuration.second&&engine.deviceInputLatency==configuration.second/2,
                         "device restart during export must refresh recording latency");
            engine.play(true);engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,block.getNumSamples(),context);
            requireAudit(plugin->prepared&&std::isfinite(block.getMagnitude(0,block.getNumSamples()))&&engine.position()>0,
                         "playback resumes safely after an export device change");
        }
        std::cout<<"Export device restart: rate-only/buffer-only/both, completion/cancellation passed\n";
    }
    static void densePanic(){
        AudioEngine engine(AudioEngine::Offline{});auto project=newProject(0);for(auto& t:project.tracks)for(auto& c:t.clips)c.notes.clear();auto probe=std::make_unique<MidiDeliveryProbe>();auto* plugin=probe.get();auto& i=*engine.instruments[0];i.plugin=std::move(probe);i.mode=2;i.prepare(44100,512);engine.publish(project);engine.play(true);juce::AudioBuffer<float> block(2,512);juce::AudioIODeviceCallbackContext context;auto tick=[&]{engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);};engine.liveMidi(0,juce::MidiMessage::noteOn(2,64,(juce::uint8)100),false);tick();requireAudit(plugin->ringing[128+64],"dense panic fixture note did not start");
        i.midi.clear();for(int n=0;n<16400;++n)i.midi.addEvent(juce::MidiMessage::noteOn(1,60,(juce::uint8)90),0);tick();requireAudit(std::none_of(plugin->ringing.begin(),plugin->ringing.end(),[](bool on){return on;}),"dense schedule recovery dropped its panic");
        const unsigned char malformed[]{0x91,0xff,0xff};engine.liveMidi(0,juce::MidiMessage(malformed,3),false);tick();requireAudit(plugin->ringing[128+127],"malformed MIDI data was not bounded");
        plugin->setLatencySamples(513);engine.prepareDelayLines();size_t reserved=0;for(const auto& ptr:engine.instruments)reserved+=(size_t)ptr->delayLine.getNumSamples();requireAudit(reserved>0&&reserved<engineTrackCount*2048,"PDC reserves maximum storage instead of required delay");auto& delayed=*engine.instruments[1];block.clear();block.setSample(0,0,1);delayed.applyDelay(block,512);engine.hardReset=false;engine.resetAt(1);block.clear();delayed.applyDelay(block,512);requireAudit(block.getMagnitude(0,512)==0,"loop/seek reset replays stale PDC audio");block.clear();delayed.applyDelay(block,512);requireAudit(block.getMagnitude(0,512)==0,"loop/seek reset leaves a delayed impulse");plugin->setLatencySamples(0);engine.prepareDelayLines();for(const auto& ptr:engine.instruments)requireAudit(ptr->delayLine.getNumSamples()==0,"zero-latency tracks retain PDC storage");
        ParameterAutomation lane;lane.slot=0;lane.index=0;lane.curve.enabled=true;lane.curve.points={{0,.25,0}};project.tracks[0].parameters={lane};engine.stop();engine.capture.monitor=false;engine.liveTail=0;engine.publish(project);ParameterObserver observer;auto* parameter=plugin->getParameters()[0];parameter->addListener(&observer);tick();requireAudit(std::abs(parameter->getValue()-.25f)<1e-6f&&observer.changes==0,"stopped callback notifies plugin listeners on the audio thread");engine.flushParameterNotifications();requireAudit(observer.changes>0,"stopped automation notification never reaches message thread");parameter->removeListener(&observer);
    }
    static void handoffLifetime(){
        struct State {std::atomic<int>& destroyed;std::vector<int> lanes{37};explicit State(std::atomic<int>& count):destroyed(count){}~State(){++destroyed;}};
        std::atomic<int> destroyed{0};Handoff<State> handoff;handoff.post(std::make_unique<State>(destroyed));handoff.adopt();auto* borrowed=handoff.current;
        handoff.post(std::make_unique<State>(destroyed));juce::WaitableEvent migrating,resume;bool alive=false;
        std::thread audio([&]{handoff.adopt([&](const State&){migrating.signal();resume.wait(5000);alive=borrowed->lanes[0]==37&&destroyed==0;});});
        const bool started=migrating.wait(5000);handoff.collect();const int whileMigrating=destroyed;resume.signal();audio.join();handoff.collect();
        requireAudit(started&&whileMigrating==0&&alive&&destroyed==1,"C1: control-side collection cannot destroy the state while audio references migrate");
    }
    static void handoffStress(){
        // Reproduce the last-post interleaving: the producer can publish while
        // audio migrates, before the old current state is parked in retired.
        {Handoff<int> handoff;handoff.post(std::make_unique<int>(1));handoff.adopt();handoff.post(std::make_unique<int>(2));
         handoff.adopt([&](const int&){handoff.post(std::make_unique<int>(3));});
         requireAudit(handoff.pending.load()!=nullptr&&handoff.retired.load()!=nullptr&&handoff.adopt()==nullptr,"handoff backpressure fixture must retain a final pending state");
         handoff.collect();auto* final=handoff.adopt();requireAudit(final&&*final==3,"control collection must allow the final pending state to migrate");}
        std::atomic<int> destroyed{0},audioDeletes{0};std::atomic<bool> done{false},finished{false},abort{false};
        int adopted=0;bool intact=true;
        struct State {
            std::atomic<int>& destroyed;std::atomic<int>& audioDeletes;std::thread::id audio;
            int revision;std::array<int,8> data;
            State(std::atomic<int>& d,std::atomic<int>& a,std::thread::id thread,int r):destroyed(d),audioDeletes(a),audio(thread),revision(r){data.fill(r);}
            ~State(){++destroyed;if(std::this_thread::get_id()==audio)++audioDeletes;}
        };
        {
            Handoff<State> handoff;
            std::thread audio([&]{
                while(!abort.load()&&(!done.load()||handoff.pending.load()!=nullptr)){
                    if(auto* state=handoff.adopt()){
                        ++adopted;for(int value:state->data)intact=intact&&value==state->revision;
                    }
                    std::this_thread::yield();
                }
                finished=true;
            });
            for(int revision=0;revision<10000;++revision)
                handoff.post(std::make_unique<State>(destroyed,audioDeletes,audio.get_id(),revision));
            done=true;
            // post() normally collects, but after its final call the control
            // side must keep collecting until the consumer has drained pending.
            const double deadline=juce::Time::getMillisecondCounterHiRes()+10000;
            while(!finished.load()&&juce::Time::getMillisecondCounterHiRes()<deadline){handoff.collect();std::this_thread::yield();}
            const bool drained=finished.load();abort=true;audio.join();handoff.collect();
            requireAudit(drained,"handoff stress must drain with control-side collection before its deadline");
        }
        requireAudit(adopted>0&&intact&&destroyed==10000&&audioDeletes==0,
                     "rapid handoff publication must retain valid states and reclaim every allocation off the audio thread");
        std::cout<<"Handoff concurrent stress: 10000 states reclaimed, zero audio-thread deletions\n";
    }
    static void liveMidiRecovery(){
        AudioEngine engine(AudioEngine::Offline{});auto project=newProject(4);project.sections={{Section::main,4,"Recovery"}};project.normalise();for(auto& t:project.tracks)for(auto& c:t.clips)c.notes.clear();
        auto probe=std::make_unique<MidiDeliveryProbe>();auto* plugin=probe.get();auto& instrument=*engine.instruments[0];instrument.plugin=std::move(probe);instrument.mode=2;instrument.prepare(44100,512);engine.hostedCount=1;
        engine.publish(project);engine.play(true);juce::AudioBuffer<float> block(2,512);juce::AudioIODeviceCallbackContext context;auto callback=[&]{engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);};callback();
        auto miss=[&](const juce::MidiMessage& event){juce::WaitableEvent locked,release;std::thread editor([&]{const juce::ScopedLock lock(plugin->getCallbackLock());locked.signal();release.wait(5000);});const bool ready=locked.wait(5000);engine.liveMidi(0,event);callback();release.signal();editor.join();requireAudit(ready,"processor callback-lock fixture");};
        const size_t key=2*128+67;
        miss(juce::MidiMessage::noteOn(3,67,(juce::uint8)100));requireAudit(!plugin->ringing[key],"missed live note was not delivered concurrently");callback();requireAudit(plugin->ringing[key]&&block.getMagnitude(0,512)>.001,"C2: missed live Note On recovers on its original MIDI channel");
        miss(juce::MidiMessage::noteOff(3,67));requireAudit(plugin->ringing[key],"missed live Note Off reproduces a held plugin voice");callback();requireAudit(!plugin->ringing[key],"C2: missed live Note Off releases the plugin voice");
        engine.liveMidi(0,juce::MidiMessage::noteOn(3,67,(juce::uint8)100));callback();miss(juce::MidiMessage::controllerEvent(3,64,127));callback();engine.liveMidi(0,juce::MidiMessage::noteOff(3,67));callback();requireAudit(plugin->ringing[key]&&!plugin->pressed[key],"recovered sustain holds a released note");
        miss(juce::MidiMessage::controllerEvent(3,64,0));callback();requireAudit(!plugin->ringing[key],"C2: missed pedal release clears sustained voices");
        plugin->suspendProcessing(true);engine.liveMidi(0,juce::MidiMessage::controllerEvent(7,64,127));engine.liveMidi(0,juce::MidiMessage::noteOn(7,72,(juce::uint8)91));callback();engine.liveMidi(0,juce::MidiMessage::noteOff(7,72));callback();
        plugin->suspendProcessing(false);callback();requireAudit(plugin->ringing[6*128+72]&&!plugin->pressed[6*128+72]&&plugin->pedal[6],"C2: suspension recovers released but sustained notes on every channel");
        plugin->suspendProcessing(true);engine.liveMidi(0,juce::MidiMessage::controllerEvent(7,64,0));callback();engine.liveTail=0;plugin->suspendProcessing(false);engine.playing=false;callback();requireAudit(!plugin->ringing[6*128+72],"C2: idle recovery also delivers the final pedal release");
        engine.stop();
    }
    static void recordedMidiEffects(){
        AudioEngine engine(AudioEngine::Offline{});auto p=newProject(0);for(auto& t:p.tracks)for(auto& c:t.clips)c.notes.clear();p.tracks[0].freeTimeline=true;Clip c;c.span=4;c.audioOffset=0;c.audioDuration=1;c.notes={{0,4,60,100,0}};p.tracks[0].clips={c};p.tracks[0].sampler.mode=1;p.tracks[0].effects={{1,"MIDI effect","Probe","/probe","","",false}};
        auto asset=std::make_shared<SampleAsset>();asset->rate=44100;asset->frames=44100;asset->audio.setSize(2,44104);asset->audio.clear();auto& i=*engine.instruments[0];i.sampler=std::make_unique<SampleSynth>(asset,p.tracks[0].sampler,60,false);i.mode=1;auto effect=std::make_unique<AuditPlugin>(true);auto* probe=effect.get();i.effects[0].plugin=std::move(effect);i.prepare(44100,512);engine.hostedCount=1;
        engine.publish(p);engine.play(true);juce::AudioBuffer<float> output(2,512);juce::AudioIODeviceCallbackContext context;engine.audioDeviceIOCallbackWithContext(nullptr,0,output.getArrayOfWritePointers(),2,512,context);requireAudit(probe->lastMidi==0,"C3: sampler-region triggers reach inserts as standard MIDI notes");engine.stop();
    }
    static void selectiveSnapshots(){
        AudioEngine engine(AudioEngine::Offline{});auto project=newProject(4);
        auto a=std::make_unique<AuditPlugin>(),b=std::make_unique<AuditPlugin>(),master=std::make_unique<AuditPlugin>(true);
        auto* first=a.get();auto* second=b.get();auto* effect=master.get();engine.instruments[0]->plugin=std::move(a);engine.instruments[1]->plugin=std::move(b);
        engine.instruments[(size_t)masterEffectTrack]->effects[0].plugin=std::move(master);engine.hostedCount=3;project.masterEffects.effects={{1,"Master","Probe","/probe","","",false}};
        engine.capturePluginStates(project);const auto old=project.tracks[0].instrumentState;
        auto reads=[&]{return first->stateReads.load()+second->stateReads.load()+effect->stateReads.load();};
        const int initial=reads();const auto start=juce::Time::getMillisecondCounterHiRes();
        for(int i=0;i<20;++i){engine.invalidatePluginStates();engine.capturePluginStates(project);}const auto baselineTime=juce::Time::getMillisecondCounterHiRes()-start;const int baseline=reads()-initial;
        const int before=reads();const auto cachedStart=juce::Time::getMillisecondCounterHiRes();for(int i=0;i<20;++i)engine.capturePluginStates(project);const auto cachedTime=juce::Time::getMillisecondCounterHiRes()-cachedStart;
        requireAudit(baseline==60&&reads()==before,"unchanged checkpoints reuse all native chunks instead of serializing every plugin");
        const int untouched=second->stateReads,masterReads=effect->stateReads;first->getParameters()[0]->setValue(.27f);std::thread edit([&]{engine.invalidatePluginState(0,0);});edit.join();engine.capturePluginStates(project);
        requireAudit(reads()==before+1&&second->stateReads==untouched&&effect->stateReads==masterReads&&project.tracks[0].instrumentState!=old,"one editor notification refreshes only its own instance and captures its new sound");
        const int firstReads=first->stateReads;engine.invalidatePluginState(masterEffectTrack,1);engine.capturePluginStates(project);requireAudit(effect->stateReads==masterReads+1&&first->stateReads==firstReads,"master editor refresh does not serialize track plugins");
        std::cout<<"Snapshot audit: 20 edits, baseline reads="<<baseline<<", cached reads=0, edited instance reads=1; baseline_ms="<<baselineTime<<", cached_ms="<<cachedTime<<"\n";
    }
    static void monoMaster(){
        AudioEngine engine(AudioEngine::Offline{});auto project=newProject(4);project.sections={{Section::main,4,"Mono"}};project.normalise();project.masterGain=1;project.masterAutomation={};
        for(auto& track:project.tracks){track.gain=0;track.volume={};track.parameters.clear();for(auto& clip:track.clips)clip.notes.clear();}project.tracks[0].gain=1;
        auto& source=*engine.instruments[0];source.plugin=std::make_unique<AuditPlugin>();source.plugin->getParameters()[0]->setValue(.25f);source.mode=2;source.prepare(44100,512);
        auto& rack=*engine.instruments[(size_t)masterEffectTrack];rack.effects[0].plugin=std::make_unique<AuditPlugin>(true,0,true);rack.effects[0].plugin->getParameters()[0]->setValue(.25f);rack.prepare(44100,512);engine.hostedCount=2;
        project.masterEffects.effects={{1,"Mono","Probe","/mono","","",true}};juce::AudioBuffer<float> block(1,512);juce::AudioIODeviceCallbackContext context;
        auto render=[&]{engine.publish(project);engine.play(true);for(int i=0;i<16;++i)engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),1,512,context);return block.getSample(0,500);};
        const float dry=render();project.masterEffects.effects[0].bypassed=false;const float processed=render();std::cout<<"Mono master audit: dry="<<dry<<", processed="<<processed<<"\n";
        requireAudit(dry>.1f&&std::abs(processed/dry-.5f)<1.e-5f,"mono output preserves master level through a mono insert without an extra six dB loss");engine.stop();
    }
    static void effects(){
        AudioEngine engine(AudioEngine::Offline{});engine.prepareDelayLines();for(auto& i:engine.instruments)i->prepare(44100,512);auto p=newProject(32);p.sections={{Section::main,2,"FX"}};p.masterGain=1;
        for(auto& t:p.tracks){t.gain=0;t.parameters.clear();for(auto& c:t.clips)c.notes.clear();}p.tracks[0].gain=1;p.tracks[0].pan=0;p.tracks[0].effects={{1,"A","Probe","/A","","",false},{5,"B","Probe","/B","","",false}};p.normalise();auto& i=*engine.instruments[0];i.plugin=std::make_unique<AuditPlugin>();i.mode=2;i.effects[0].plugin=std::make_unique<AuditPlugin>(true,.1f);i.effects[4].plugin=std::make_unique<AuditPlugin>(true);i.effects[4].plugin->getParameters()[0]->setValue(1);i.effects[0].plugin->setLatencySamples(13);i.effects[4].plugin->setLatencySamples(29);i.prepare(44100,512);engine.hostedCount=3;
        juce::AudioBuffer<float> block(2,512);juce::AudioIODeviceCallbackContext context;auto render=[&](float expected){engine.publish(p);engine.play(true);for(int k=0;k<5;++k)engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);requireAudit(std::abs(block.getSample(0,500)-expected)<.001f,"serial effects preserve measurable order and gain");};
        render(4.7f);requireAudit(engine.latencySamples()==42,"PDC sums enabled insert latencies");p.tracks[0].moveEffect(5,0);render(4.6f);
        ParameterAutomation lane;lane.slot=1;lane.index=0;lane.curve.enabled=true;lane.curve.points={{0,.25,0},{8,.25,0}};p.tracks[0].parameters.push_back(lane);render(1.6f);i.effects[0].plugin->getParameters()[0]->setValue(.9f);engine.invalidatePluginStates();engine.capturePluginStates(p);const auto recalled=p.tracks[0].effect(1)->state;render(1.6f);engine.restoreEffectState(0,juce::String::fromUTF8(recalled.c_str()),1);render(1.6f);p.tracks[0].moveEffect(1,0);render(1.7f);
        p.tracks[0].effect(1)->bypassed=true;render(3);requireAudit(engine.latencySamples()==29,"bypassed effect does not contribute latency");i.effects[0].plugin->getParameters()[0]->setValue(.9f);p.tracks[0].effect(1)->bypassed=false;render(1.7f);lane.slot=5;p.tracks[0].parameters.push_back(lane);render(.425f);
        const auto preserved=p.tracks[0].parameters[1].curve.points;p.tracks[0].removeEffect(1);engine.clearEffect(0,1);render(.75f);requireAudit(p.tracks[0].parameters.size()==1&&p.tracks[0].parameters[0].slot==5&&p.tracks[0].parameters[0].curve.points.size()==preserved.size(),"removal keeps the other instance's automation attached");
        engine.stop();engine.clearEffect(0);i.plugin=std::make_unique<AuditPlugin>(false,0,false,1.f/3);i.effects[0].plugin=std::make_unique<AuditPlugin>(true,0,true);engine.hostedCount=2;i.prepare(44100,512);p.tracks[0].effects={{1,"Mono","Mono probe","/mono","","",false}};p.tracks[0].parameters.clear();render(1.5f);requireAudit(std::abs(block.getSample(1,500)-1.5f)<.001f,"mono inserts downmix both input sides and return to stereo");
        engine.stop();engine.useInternal(0);engine.clearEffect(0);p.tracks[0].effects.clear();p.tracks[0].parameters.clear();lane.slot=-1;lane.index=0;lane.curve.points={{0,.2,0},{8,.2,0}};p.tracks[0].parameters.push_back(lane);engine.publish(p);engine.play(true);engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);requireAudit(std::abs(i.tone.load()-.2)<1.e-6,"native guide tone automation reaches the sound");
        engine.stop();auto asset=std::make_shared<SampleAsset>();asset->rate=44100;asset->frames=4410;asset->audio.setSize(2,4410);asset->audio.clear();i.sampler=std::make_unique<SampleSynth>(asset,SamplerSettings{},60,false);i.mode=1;i.prepare(44100,512);p.tracks[0].instrument="sample";p.tracks[0].parameters[0].index=4;p.tracks[0].parameters[0].curve.points={{0,0,0},{8,0,0}};engine.publish(p);engine.play(true);engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);auto* sampler=dynamic_cast<SampleSynth*>(i.sampler.get());requireAudit(sampler&&sampler->settings().filter&&sampler->settings().cutoff==20&&!p.tracks[0].sampler.filter,"sample filter automation affects playback without changing stored sampler settings");p.tracks[0].parameters[0].curve.enabled=false;engine.updateMix(p);engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);requireAudit(!sampler->settings().filter&&sampler->settings().cutoff==p.tracks[0].sampler.cutoff,"native automation bypass returns to stored sound settings");engine.stop();
    }
    static void exportCompatibility(){
        AudioEngine engine(AudioEngine::Offline{});engine.prepareDelayLines();for(auto& prepared:engine.instruments)prepared->prepare(44100,512);auto project=newProject(32);project.sections={{Section::main,1,"Intro"}};project.normalise();project.masterGain=.1;for(auto& t:project.tracks){t.gain=0;t.volume.enabled=false;t.parameters.clear();}project.masterAutomation.enabled=false;
        auto& track=project.tracks[0];track.gain=1;track.instrument="plugin";track.effects={{1,"A","Probe","/A","","",false},{2,"B","Probe","/B","","",false}};
        auto& instrument=*engine.instruments[0];auto source=std::make_unique<AuditPlugin>();auto* original=source.get();instrument.plugin=std::move(source);instrument.mode=2;instrument.effects[0].plugin=std::make_unique<AuditPlugin>(true);instrument.effects[1].plugin=std::make_unique<AuditPlugin>(true);instrument.prepare(44100,512);engine.hostedCount=3;
        const int prepared=original->preparations;const auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fga-export-compatibility","",false);folder.createDirectory();
        for(bool effects:{false,true}){if(!effects)track.effects.clear();else track.effects={{1,"A","Probe","/A","","",false},{2,"B","Probe","/B","","",false}};const auto file=folder.getChildFile(effects?"instrument-two-effects.wav":"instrument.wav");requireAudit(engine.exportAudio(project,file,0,false,[]{return false;},[&](double){const auto at=engine.position();engine.play(true);engine.seek(99);requireAudit(!engine.isPlaying()&&engine.position()==at,"live playback and seek remain blocked throughout audio export");}).wasOk(),"export already loaded instrument with and without two inserts");juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(file.createInputStream().release(),true));juce::AudioBuffer<float> audio(2,8192);requireAudit(reader&&reader->read(&audio,0,8192,4096,true,true)&&audio.getMagnitude(0,8192)>.01f,"existing instrument and inserts produce audible WAV");requireAudit(instrument.plugin.get()==original&&original->preparations==prepared&&!original->offlineObserved,"same-rate export retains instance/program and playback DSP mode without reinitialisation");}
        folder.deleteRecursively();
    }
    static void stateRestoreOrder(){
        AudioEngine engine(AudioEngine::Offline{});engine.prepareDelayLines();for(auto& source:engine.instruments)source->prepare(44100,512);auto& instrument=*engine.instruments[0];auto next=std::make_unique<AuditPlugin>();auto* source=next.get();instrument.plugin=std::move(next);instrument.mode=2;instrument.prepare(44100,512);
        juce::MemoryBlock state;source->getStateInformation(state);engine.restorePluginState(0,state.toBase64Encoding());
        requireAudit(!source->stateRestoredActive&&source->prepared,"native state must be restored with processing inactive and reactivated before playback");
        requireAudit(instrument.plugin.get()==source&&instrument.scratch.getNumChannels()>=source->getTotalNumOutputChannels(),"state restore retains the instance and refreshes its channel buffers");
    }
    static void programEdits(){
        AudioEngine engine(AudioEngine::Offline{});engine.prepareDelayLines();for(auto& source:engine.instruments)source->prepare(44100,512);auto project=newProject(4);auto& instrument=*engine.instruments[0];
        auto source=std::make_unique<ProgramStatePlugin>();source->setCurrentProgram(2);source->privateEdit=.73f;source->getParameters()[0]->setValue(.37f);
        instrument.plugin=std::move(source);instrument.mode=2;engine.hostedCount=1;engine.capturePluginStates(project);
        auto loaded=std::make_unique<ProgramStatePlugin>();auto* probe=loaded.get();instrument.plugin=std::move(loaded);
        engine.restorePluginState(0,juce::String::fromUTF8(project.tracks[0].instrumentState.c_str()));
        requireAudit(probe->program==2&&std::abs(probe->privateEdit-.73f)<1.e-6f&&std::abs(probe->getParameters()[0]->getValue()-.37f)<1.e-6f,"program selection must precede opaque edits and exposed parameter restore");
        ParameterObserver listener;auto* parameter=probe->getParameters()[0];parameter->addListener(&listener);
        ParameterAutomation lane;lane.slot=0;lane.index=0;lane.curve.enabled=true;lane.curve.points={{0,.62,0}};
        project.tracks[0].parameters={lane};engine.publish(project);engine.applyParameterAutomation(instrument,0,0);parameter->removeListener(&listener);
        requireAudit(listener.changes>0&&std::abs(listener.value-.62f)<1.e-6f,"automated hosted parameters notify editor listeners");
    }
    static void nativePresetAuthority(){
        AudioEngine engine(AudioEngine::Offline{});for(auto& i:engine.instruments)i->prepare(44100,512);auto p=newProject(4);auto& i=*engine.instruments[0];
        for(bool full:{true,false}){auto source=std::make_unique<CompletePresetPlugin>();source->full=full;source->getParameters()[0]->setValue(.1f);i.plugin=std::move(source);i.mode=2;engine.hostedCount=1;engine.invalidatePluginStates();engine.capturePluginStates(p);
            engine.restorePluginState(0,juce::String::fromUTF8(p.tracks[0].instrumentState.c_str()));auto* loaded=static_cast<CompletePresetPlugin*>(i.plugin.get());
            requireAudit(!loaded->stateRestoredActive&&loaded->prepared,"native program restore remains inactive then prepared");
            requireAudit(std::abs(loaded->getParameters()[0]->getValue()-(full?.81f:.1f))<1.e-6f,"complete native program wins over stale controller snapshot; partial formats keep fallback");
        }
    }
    static void isolatedClipPlayback(){
        AudioEngine engine(AudioEngine::Offline{});for(auto& i:engine.instruments)i->prepare(44100,512);auto p=newProject(4);p.sections={{Section::main,2,"Test"}};p.normalise();p.loop=true;p.loopStart=0;p.loopEnd=1;
        for(auto& t:p.tracks){t.gain=0;t.volume.enabled=false;t.panAutomation.enabled=false;t.parameters.clear();}p.tracks[0].gain=p.tracks[1].gain=1;
        for(int track:{0,1}){auto& i=*engine.instruments[(size_t)track];i.plugin=std::make_unique<AuditPlugin>();i.mode=2;i.prepare(44100,512);}engine.hostedCount=2;engine.publish(p);
        juce::AudioBuffer<float> block(2,512);juce::AudioIODeviceCallbackContext context;auto callback=[&]{engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);};
        engine.playClip(0,2,3);callback();requireAudit(engine.isClipPlaying()&&engine.position()>=2&&engine.position()<3&&engine.level(0)>0&&engine.level(1)==0,"clip play isolates audio and overrides song loop without muting tracks");
        engine.seek(2.999);callback();requireAudit(engine.position()>=2&&engine.position()<2.1,"clip audition loops at its own bounds");
        p.tracks[0].clips[0].notes.push_back({2,.25,72,100,0});engine.publish(p);callback();requireAudit(engine.isClipPlaying()&&engine.position()>=2&&engine.position()<3,"editing notes preserves clip audition");
        engine.play(false);requireAudit(!engine.isClipPlaying(),"stop clears clip audition");engine.play(true);callback();requireAudit(!engine.isClipPlaying()&&engine.level(1)>0&&!p.tracks[1].mute&&!p.tracks[0].solo,"normal transport restores complete mix");engine.stop();
    }
    static void snapshotKeepsOtherTracksPlaying(){
        AudioEngine engine(AudioEngine::Offline{});engine.prepareDelayLines();for(auto& i:engine.instruments)i->prepare(44100,512);
        auto p=newProject(0);p.sections={{Section::main,2,"Snapshot"}};p.normalise();for(auto& t:p.tracks){t.gain=0;for(auto& c:t.clips)c.notes.clear();}
        p.tracks[1].gain=1;p.tracks[1].clips[0].notes={{0,7,40,110,0}};p.tracks[0].gain=1;
        auto probe=std::make_unique<AuditPlugin>();auto* plugin=probe.get();engine.swapInstrument(0,std::move(probe),0,{});engine.publish(p);engine.play(true);
        juce::AudioBuffer<float> block(2,512);juce::AudioIODeviceCallbackContext context;auto callback=[&]{engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);};
        for(int i=0;i<5;++i)callback();juce::WaitableEvent started,resume,returned;plugin->snapshotStarted=&started;plugin->snapshotResume=&resume;
        std::thread snapshot([&]{engine.capturePluginStates(p);});const bool began=started.wait(2000);
        std::thread audio([&]{callback();returned.signal();});const bool nonblocking=returned.wait(1000);const bool otherAudible=nonblocking&&engine.level(1)>.001f;
        resume.signal();snapshot.join();audio.join();requireAudit(began&&nonblocking&&otherAudible&&!plugin->overlap,"slow state reads do not block the callback, race the processor or mute unrelated tracks");
    }
    static void stoppedStateCapture(){
        AudioEngine engine(AudioEngine::Offline{});auto project=newProject(0);auto next=std::make_unique<MidiDeliveryProbe>();auto* plugin=next.get();engine.swapInstrument(0,std::move(next),0,{});engine.publish(project);
        auto& instrument=*engine.instruments[0];juce::AudioBuffer<float> block(2,512);juce::AudioIODeviceCallbackContext context;
        engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context); // settle the pending transport reset before injecting idle MIDI
        instrument.midi.addEvent(juce::MidiMessage::noteOn(1,60,(juce::uint8)100),0);engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);
        instrument.midi.addEvent(juce::MidiMessage::noteOff(1,60),0);
        juce::WaitableEvent started,resume;plugin->snapshotStarted=&started;plugin->snapshotResume=&resume;
        std::thread snapshot([&]{engine.capturePluginStates(project);});const bool began=started.wait(2000);
        const int calls=plugin->calls;
        engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);
        const bool deferred=plugin->calls==calls&&plugin->ringing[60];resume.signal();snapshot.join();plugin->snapshotStarted=plugin->snapshotResume=nullptr;
        engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);
        requireAudit(began&&deferred&&plugin->calls>calls&&!plugin->ringing[60]&&!plugin->overlap,"stopped state capture defers DSP without losing pending note-offs");
        instrument.midi.addEvent(juce::MidiMessage::noteOn(1,61,(juce::uint8)100),0);engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);
        instrument.midi.addEvent(juce::MidiMessage::noteOff(1,61),0);plugin->suspendProcessing(true);const int suspendedCalls=plugin->calls;
        engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);
        requireAudit(plugin->calls==suspendedCalls&&plugin->ringing[61],"stopped suspended plugins retain pending note-offs");plugin->suspendProcessing(false);
        engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);requireAudit(!plugin->ringing[61],"resume delivers stopped note-offs");
    }
    static void sourceReplacementCheckpoint(){
        AudioEngine engine(AudioEngine::Offline{});auto project=newProject(0);project.tracks[0].instrument="plugin";
        auto original=std::make_unique<ProgramStatePlugin>();auto* live=original.get();live->privateEdit=.25f;engine.swapInstrument(0,std::move(original),0,{});engine.capturePluginStates(project);live->privateEdit=.75f;
        auto file=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fga-source-checkpoint",".wav",false);
        juce::WavAudioFormat wav;juce::AudioBuffer<float> silence(1,128);silence.clear();{std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(file.createOutputStream().release(),44100,1,16,{},0));requireAudit(writer&&writer->writeFromAudioSampleBuffer(silence,0,128),"sample replacement fixture");}
        int checkpoints=0;auto checkpoint=[&]{++checkpoints;engine.invalidatePluginState(0,0);engine.capturePluginStates(project);};
        const auto bad=engine.loadSample(0,file.getSiblingFile("missing-audit-sample.wav"),60,{},checkpoint);requireAudit(bad.failed()&&checkpoints==0&&engine.plugin(0)==live,"failed sample loading preserves source and Undo");
        const auto good=engine.loadSample(0,file,60,{},checkpoint);file.deleteFile();requireAudit(good.wasOk()&&checkpoints==1&&!engine.plugin(0),"successful replacement captures exactly once before detach");
        auto restored=std::make_unique<ProgramStatePlugin>();auto* probe=restored.get();engine.swapInstrument(0,std::move(restored),0,{});engine.restorePluginState(0,juce::String::fromUTF8(project.tracks[0].instrumentState.c_str()));
        requireAudit(std::abs(probe->privateEdit-.75f)<1e-6,"replacement Undo snapshot retains the latest opaque sound edit");
    }
    static void run(){exportDeviceChanges();deviceTransportMatrix();densePanic();handoffLifetime();handoffStress();liveMidiRecovery();recordedMidiEffects();stoppedStateCapture();sourceReplacementCheckpoint();selectiveSnapshots();monoMaster();snapshotKeepsOtherTracksPlaying();effects();exportCompatibility();stateRestoreOrder();programEdits();nativePresetAuthority();isolatedClipPlayback();
        // Fixed-rate fixtures must not inherit the runner's physical audio device rate or latency.
        AudioEngine engine(AudioEngine::Offline{});engine.prepareDelayLines();for(auto& instrument:engine.instruments)instrument->prepare(44100,512);auto project=newProject(571);project.sections={{Section::main,2,"Audit"}};project.masterGain=1;project.settings.bpm=120;project.settings.freeTempo=true;
        for(auto& t:project.tracks){t.gain=0;for(auto& clip:t.clips)clip.notes.clear();}project.tracks[0].gain=1;project.normalise();
        auto probe=std::make_unique<AuditPlugin>();auto* plugin=probe.get();{const AudioEngine::EngineControlLock lock(engine);auto& i=*engine.instruments[0];i.plugin=std::move(probe);engine.hostedCount=1;i.mode=2;i.plugin->setPlayHead(&engine);i.prepare(44100,512);}
        engine.publish(project);engine.play(true);juce::AudioBuffer<float> block(2,512);juce::AudioIODeviceCallbackContext context;
        auto callback=[&]{engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);};callback();requireAudit(block.getMagnitude(0,512)>1.1f,"live floating-point output must retain headroom above unity");
        // Holding a control mutex alone cannot block the audio callback anymore.
        const int before=plugin->calls;engine.mutex.enter();juce::WaitableEvent returned;std::thread independent([&]{callback();returned.signal();});const bool nonblocking=returned.wait(10000);engine.mutex.exit();independent.join();requireAudit(nonblocking&&plugin->calls>before,"processBlock must run without acquiring the control mutex (bounded wait)");
        plugin->setLatencySamples(256);callback();requireAudit(engine.delayStorageNeeded.load(),"latency change must publish a control-side storage request");
        engine.restorePluginState(0,"AAAAAA==");
        requireAudit(!engine.delayStorageNeeded.load()&&engine.latencySamples()==256&&std::all_of(engine.instruments.begin(),engine.instruments.end(),[](const auto& i){return i->delay==0||i->delayLine.getNumSamples()>i->delay;}),"plugin restore must service the published delay request under the control gate");
        plugin->setLatencySamples(0);callback();engine.collectGarbage();
        plugin->slow=true;std::atomic<bool> running{true};std::thread audio([&]{while(running)callback();});
        for(int n=0;n<20;++n){engine.capturePluginStates(project);engine.restorePluginState(0,"AAAAAA==");}running=false;audio.join();plugin->slow=false;requireAudit(!plugin->overlap&&!project.tracks[0].instrumentState.empty(),"plugin state snapshot executes without overlapping processBlock");
        plugin->stateReads=0;plugin->calls=0;engine.play(true);const int callsBefore=plugin->calls;running=true;std::thread playing([&]{while(running)callback();});
        const auto started=juce::Time::getMillisecondCounterHiRes();
        for(int n=0;n<40;++n){project.tracks[0].gain=.2+.01*(n%4);engine.updateMix(project);engine.reusePluginStates(project);}
        const double spent=juce::Time::getMillisecondCounterHiRes()-started;juce::Thread::sleep(20);const int states=plugin->stateReads.load();running=false;playing.join();
        // Probe calls is not atomic: only sample it before start/after join.
        const int callsAfter=plugin->calls;engine.stop();
        std::cout<<"Mix handoff latency: controls_ms="<<spent<<", snapshot_reads="<<states<<", callbacks="<<callsBefore<<"->"<<callsAfter<<"\n";
        requireAudit(states==0&&spent<80&&callsAfter>callsBefore,"volume changes stay lock-free: no plugin snapshot and processBlock keeps running");
        engine.stop();project.tracks[0].parameters.clear();ParameterAutomation lane;lane.index=0;lane.curve.enabled=true;const double point=141.*120/(44100*60);lane.curve.points={{0,0,0},{point,1,0},{1,1,0}};project.tracks[0].parameters.push_back(lane);
        plugin->calls=plugin->samples=0;plugin->lastMidi=-1;engine.publish(project);engine.play(true);engine.liveMidi(0,juce::MidiMessage::noteOn(1,65,.8f),false); // zero-offset UI input
        // An explicitly timed live event must retain its position through plugin subblocks.
        engine.capture.enqueue(0,juce::MidiMessage::noteOn(1,69,.8f),false,400);callback();bool boundary=false;
        for(int i=0;i<plugin->calls&&i<(int)plugin->starts.size();++i){requireAudit(plugin->sizes[(size_t)i]<=64,"automated plugin ramp update bound");if(plugin->starts[(size_t)i]==128)boundary=plugin->sizes[(size_t)i]==13;requireAudit(std::abs(plugin->positions[(size_t)i]*44100*.5-plugin->starts[(size_t)i])<1e-5,"playhead follows each plugin subblock");}
        requireAudit(boundary&&plugin->lastMidi==400,"automation cuts exact breakpoint without moving live MIDI offsets");engine.stop();
        project.tracks[0].parameters.clear();project.loop=true;project.loopStart=1;project.loopEnd=2;project.settings.bpm=120;project.settings.freeTempo=true;engine.publish(project);engine.seek(1.95);
        requireAudit(engine.startRecording(0,0).wasOk(),"start recording across loop");engine.liveMidi(0,juce::MidiMessage::noteOn(1,70,.8f));for(int n=0;n<18;++n)callback();engine.liveMidi(0,juce::MidiMessage::noteOff(1,70));callback();auto take=engine.finishRecording();assembleMidiTake(take,0);requireAudit(!take.notes.empty(),"loop capture creates notes");for(const auto& note:take.notes)requireAudit(note.beat>=1&&note.beat<2,"loop MIDI positions never accumulate at zero");engine.stop();
        plugin->calls=plugin->samples=0;plugin->lastMidi=-1;engine.publish(project);engine.seek(1.99);engine.play(true);engine.capture.enqueue(0,juce::MidiMessage::noteOn(1,73,.8f),false,400);callback();requireAudit(plugin->lastMidi==400,"explicit live MIDI offsets survive a loop boundary inside a callback");engine.stop();
        // Latency subtraction is tested after the beginning of the take, without preroll clipping.
        plugin->setLatencySamples(2205);engine.publish(project);engine.seek(1.2);engine.startRecording(0,0);for(int n=0;n<20;++n)callback();const double elapsed=engine.capture.elapsed.load();engine.liveMidi(0,juce::MidiMessage::noteOn(1,72,.8f));callback();auto timed=engine.finishRecording();auto event=std::find_if(timed.events.begin(),timed.events.end(),[](const auto& e){return e.status==0x90&&e.note==72;});requireAudit(event!=timed.events.end()&&std::abs(event->elapsed-(elapsed-.1))<1e-6,"recording subtracts plugin latency in continuous beat units");engine.stop();
        plugin->setLatencySamples(50000);engine.publish(project);engine.play(true);callback();requireAudit(engine.latencySamples()==50000&&!engine.latencyIsLimited(),"compensation supports more than the old 32767-frame ceiling");plugin->setLatencySamples(300000);callback();requireAudit(engine.latencyIsLimited()&&engine.latencySamples()==engine.latencyCapacity(),"declared latency capacity flags an unsupported plugin delay");plugin->setLatencySamples(0);engine.stop();
        engine.setTimeSignature(3,8);engine.seek(3.2);auto position=engine.getPosition();requireAudit(position&&position->getTimeSignature()->numerator==3&&position->getTimeSignature()->denominator==8&&std::abs(*position->getPpqPositionOfLastBarStart()-3)<1e-9,"playhead bar position follows configured meter");engine.startRecording(0,1);requireAudit(std::abs(engine.countInRemaining()-1.5)<1e-9,"count-in follows non-quarter meter");engine.finishRecording();engine.stop();juce::AudioBuffer<float> clicks(2,44100);clicks.clear();engine.click(clicks.getArrayOfWritePointers(),2,44100,0,true);requireAudit(std::abs(clicks.getSample(0,1)-clicks.getSample(0,33076))<1e-6&&std::abs(clicks.getSample(0,1)-clicks.getSample(0,11026))>.005,"3/8 metronome accents every three eighth-note pulses");engine.setTimeSignature(4,4);
        MidiCapture queue;for(int i=0;i<9000;++i)queue.enqueue(0,juce::MidiMessage::noteOn(1,60,.5f),true);requireAudit(queue.inputOverflow&&!queue.overflow,"input overflow is diagnostic and does not interrupt the record queue");
        RealtimeEvents<GuideLead::Event> events;events.prepare();const auto* address=events.data();for(int i=0;i<20000;++i)events.push_back({i,60,.5f,0});requireAudit(events.dropped&&events.size()==events.limit&&address==events.data(),"lead/guitar event capacity cannot grow on the audio thread");
        {MixerBuses buses;auto settings=defaultBuses();settings[3].gain=1;settings[3].feedback=0;settings[3].division=.125;buses.prepare(8000,64);buses.configure(settings,300,1,true);float echoed=0;juce::AudioBuffer<float> out(2,64);for(int start=0;start<320;start+=64){out.clear();buses.begin(64);if(start==0)buses.add(0,0,1,-1,{0,1});buses.finish(out.getArrayOfWritePointers(),2,64,nullptr);if(start<=200&&start+64>200)echoed=out.getSample(0,200-start);}requireAudit(echoed>.9f,"snapped bus delay uses its requested interval immediately");settings[3].division=4;buses.configure(settings,1,1,true);for(int n=0;n<1100;++n){out.clear();buses.begin(64);buses.finish(out.getArrayOfWritePointers(),2,64,nullptr);requireAudit(std::isfinite(out.getMagnitude(0,64)),"long bus delay remains inside its circular buffer");}}
        {auto dense=std::make_unique<ScheduleState>();dense->length=8;TimedEvent event;event.beat=0;event.track=3;event.pitch=60;event.velocity=90;event.lane=0;event.controller=-1;dense->schedules[3].assign(20000,event);dense->hashes[3]=1;const auto* storage=engine.instruments[3]->guitarEvents.data();engine.schedules.post(std::move(dense));engine.play(true);callback();requireAudit(engine.midiInputOverflow()&&engine.instruments[3]->cursor==20000&&engine.instruments[3]->guitarEvents.data()==storage,"dense schedules skip excess work without growing real-time MIDI/guitar storage");engine.stop();}
        auto directory=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fga-audit-audio","",false);directory.createDirectory();
        const auto file=directory.getChildFile("input.wav");AudioTakeRecorder writer(AudioTake{file,48000,2});requireAudit(writer.open().wasOk(),"open threaded audio recorder");std::array<float,512> left{},right{};left.fill(.25f);right.fill(-.5f);for(int n=0;n<20;++n)writer.append(left.data(),right.data(),512);auto audioTake=writer.finish();requireAudit(audioTake.frames==10240&&!audioTake.overflow,"audio take retains queued frames");
        juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(file.createInputStream().release(),true));requireAudit(reader&&reader->lengthInSamples==10240&&reader->sampleRate==48000,"flushed audio take is a valid stereo WAV");juce::AudioBuffer<float> read(2,512);reader->read(&read,0,512,0,true,true);requireAudit(std::abs(read.getSample(0,0)-.25f)<1e-5&&std::abs(read.getSample(1,0)+.5f)<1e-5,"input channels retain their samples");
        const auto original=project;int index=addAudioTake(project,audioTake,true);requireAudit(index>=trackCount&&project.tracks[(size_t)index].mute&&project.tracks[(size_t)index].clips[0].position==2&&project.tracks[0].clips==original.tracks[0].clips,"audio take imports as a muted sample clip without changing existing MIDI");
        {auto compensated=audioTake;compensated.start=-.1;auto imported=original;const int slot=addAudioTake(imported,compensated,false);requireAudit(slot>=trackCount&&imported.tracks[(size_t)slot].clips[0].position==0&&std::abs(imported.tracks[(size_t)slot].sampler.start-2400./10240)<1e-9,"negative audio latency preroll trims non-destructively on import");}
        {const auto capturedFile=directory.getChildFile("controlled-input.wav");auto recorder=std::make_unique<AudioTakeRecorder>(AudioTake{capturedFile,44100,0});requireAudit(recorder->open().wasOk(),"open controlled input take");{const AudioEngine::EngineControlLock lock(engine);engine.audioRecorder=std::move(recorder);engine.audioSource=AudioEngine::AudioSource::input;engine.recordingAudio=true;const float* input[]{left.data(),right.data()};std::thread inputThread([&]{engine.audioDeviceIOCallbackWithContext(input,2,block.getArrayOfWritePointers(),2,512,context);});inputThread.join();}engine.audioDeviceStopped();requireAudit(engine.audioRecordingFailed(),"device interruption is reported for an active audio take");auto captured=engine.finishAudioRecording();requireAudit(captured.frames==512&&captured.rateChanged&&!captured.overflow,"raw input is retained even during a control-side snapshot");}
        {const auto longFile=directory.getChildFile("long-take.wav");juce::AudioBuffer<float> silence(2,8000);silence.clear();std::unique_ptr<juce::AudioFormatWriter> longWriter(wav.createWriterFor(longFile.createOutputStream().release(),8000,2,16,{},0));requireAudit(longWriter!=nullptr,"create long take fixture");for(int n=0;n<61;++n)requireAudit(longWriter->writeFromAudioSampleBuffer(silence,0,8000),"write long take fixture");longWriter.reset();std::shared_ptr<const SampleAsset> longAsset;requireAudit(readSampleAsset(longFile,longAsset).wasOk()&&longAsset->frames==61*8000,"audio takes longer than 60 seconds can be imported");requireAudit(!fitsSampleMemory(1801*8000,8000,2)&&!fitsSampleMemory(90000000,48000,2),"decoded sample memory and duration bounds are explicit");}
        {juce::AudioBuffer<float> invalidPreview(2,512);invalidPreview.clear();invalidPreview.setSample(0,0,std::numeric_limits<float>::quiet_NaN());engine.clearClipping();engine.startPreview(std::move(invalidPreview),44100);callback();requireAudit(std::isfinite(block.getSample(0,0))&&engine.hasClipped(),"idle preview is finalized and non-finite device samples are removed");engine.stopPreview();}
        project=original;project.tracks[1].gain=.5;project.tracks[1].clips[0].notes={{0,8,60,100,0}};engine.publish(project);engine.seek(0);engine.play(true);callback();requireAudit(engine.activeNotes(1)[60],"other track starts a sustained note");for(int n=0;n<9000;++n)engine.capture.enqueue(0,juce::MidiMessage::noteOn(1,64,.5f),false);callback();requireAudit(engine.activeNotes(1)[60]&&engine.midiInputOverflow(),"lost input does not panic unrelated instruments");engine.stop();
        project=original;engine.publish(project);engine.seek(0);auto masterFile=directory.getChildFile("master.wav");requireAudit(engine.startAudioRecording(masterFile,AudioEngine::AudioSource::master).wasOk(),"record master mix");engine.play(true);for(int n=0;n<4;++n)callback();auto master=engine.finishAudioRecording();requireAudit(master.frames==2048&&master.file.existsAsFile(),"callback writes master audio take");engine.stop();reader.reset();directory.deleteRecursively();
    }
};
}
void runEngineAuditTests(){
    struct DelayedJob:juce::Thread{juce::WaitableEvent& entered;juce::WaitableEvent& release;std::atomic<bool>& destroyed;
        DelayedJob(juce::WaitableEvent& e,juce::WaitableEvent& r,std::atomic<bool>& d):Thread("cancellation ownership test"),entered(e),release(r),destroyed(d){}
        ~DelayedJob()override{signalThreadShouldExit();waitForThreadToExit(-1);destroyed=true;}
        void run()override{entered.signal();release.wait(-1);}
    };
    juce::WaitableEvent entered,release;std::atomic<bool> destroyed{false};
    {fga::RetiredJobs pending;auto job=std::make_unique<DelayedJob>(entered,release,destroyed);auto* worker=job.get();job->startThread();if(!entered.wait(10000)){release.signal();throw std::runtime_error("worker startup deadline");}
     pending.retire(std::move(job));const bool retained=!destroyed&&worker->isThreadRunning()&&worker->threadShouldExit();release.signal();const bool returned=worker->waitForThreadToExit(10000);if(!retained||!returned)throw std::runtime_error("cancel must return before a worker's outstanding I/O and keep it alive");}
    if(!destroyed)throw std::runtime_error("retired job must be reclaimed after cooperative shutdown");
{
    fga::AudioEngine offline(fga::AudioEngine::Offline{});
    auto project=fga::newProject(1);project.sections={{fga::Section::main,1,"Offline"}};project.normalise();
    for(auto& track:project.tracks)for(auto& clip:track.clips)clip.notes.clear();
    project.tracks[0].clips[0].notes={{0,1,69,100,0}};
    const auto file=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fga-first-offline-export",".wav",false);
    if(offline.exportAudio(project,file,-1,false,[]{return false;},[](double){}).failed())throw std::runtime_error("fresh offline engine exports at its default sample rate");
    juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(file.createInputStream().release(),true));
    juce::AudioBuffer<float> block(2,8192);const bool audible=reader&&reader->read(&block,0,8192,4096,true,true)&&block.getMagnitude(0,8192)>.001f;
    reader.reset();file.deleteFile();if(!audible)throw std::runtime_error("fresh offline export contains audible prepared guide audio");
    }
    fga::EngineAuditTests::run();}

void runDeepAuditEngineTests(){fga::EngineAuditTests::handoffLifetime();fga::EngineAuditTests::liveMidiRecovery();fga::EngineAuditTests::recordedMidiEffects();fga::EngineAuditTests::stoppedStateCapture();std::cout<<"Deep audit handoff and hosted MIDI regressions passed\n";}
