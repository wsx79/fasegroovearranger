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
    std::atomic<bool> inside{false},overlap{false};std::atomic<int> stateReads{0};bool slow=false,prepared=false,stateRestoredActive=false;int preparations=0;bool offlineObserved=false;int calls=0,samples=0,lastMidi=-1;
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
    void getStateInformation(juce::MemoryBlock& data)override{if(inside)overlap=true;++stateReads;juce::Thread::sleep(1);data.setSize(sizeof(float));const float value=gain->get();std::memcpy(data.getData(),&value,sizeof value);}
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
struct ParameterObserver final : juce::AudioProcessorParameter::Listener {
    int changes=0;float value=0;
    void parameterValueChanged(int,float v)override{++changes;value=v;}
    void parameterGestureChanged(int,bool)override{}
};
}
struct EngineAuditTests {
    static void effects(){
        AudioEngine engine(AudioEngine::Offline{});for(auto& i:engine.instruments)i->prepare(44100,512);auto p=newProject(32);p.sections={{Section::main,2,"FX"}};p.masterGain=1;
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
        AudioEngine engine(AudioEngine::Offline{});for(auto& prepared:engine.instruments)prepared->prepare(44100,512);auto project=newProject(32);project.sections={{Section::main,1,"Intro"}};project.normalise();project.masterGain=.1;for(auto& t:project.tracks){t.gain=0;t.volume.enabled=false;t.parameters.clear();}project.masterAutomation.enabled=false;
        auto& track=project.tracks[0];track.gain=1;track.instrument="plugin";track.effects={{1,"A","Probe","/A","","",false},{2,"B","Probe","/B","","",false}};
        auto& instrument=*engine.instruments[0];auto source=std::make_unique<AuditPlugin>();auto* original=source.get();instrument.plugin=std::move(source);instrument.mode=2;instrument.effects[0].plugin=std::make_unique<AuditPlugin>(true);instrument.effects[1].plugin=std::make_unique<AuditPlugin>(true);instrument.prepare(44100,512);engine.hostedCount=3;
        const int prepared=original->preparations;const auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fga-export-compatibility","",false);folder.createDirectory();
        for(bool effects:{false,true}){if(!effects)track.effects.clear();else track.effects={{1,"A","Probe","/A","","",false},{2,"B","Probe","/B","","",false}};const auto file=folder.getChildFile(effects?"instrument-two-effects.wav":"instrument.wav");requireAudit(engine.exportAudio(project,file,0,false,[]{return false;},[&](double){const auto at=engine.position();engine.play(true);engine.seek(99);requireAudit(!engine.isPlaying()&&engine.position()==at,"live playback and seek remain blocked throughout audio export");}).wasOk(),"export already loaded instrument with and without two inserts");juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(file.createInputStream().release(),true));juce::AudioBuffer<float> audio(2,8192);requireAudit(reader&&reader->read(&audio,0,8192,4096,true,true)&&audio.getMagnitude(0,8192)>.01f,"existing instrument and inserts produce audible WAV");requireAudit(instrument.plugin.get()==original&&original->preparations==prepared&&!original->offlineObserved,"same-rate export retains instance/program and playback DSP mode without reinitialisation");}
        folder.deleteRecursively();
    }
    static void stateRestoreOrder(){
        AudioEngine engine(AudioEngine::Offline{});for(auto& source:engine.instruments)source->prepare(44100,512);auto& instrument=*engine.instruments[0];auto next=std::make_unique<AuditPlugin>();auto* source=next.get();instrument.plugin=std::move(next);instrument.mode=2;instrument.prepare(44100,512);
        juce::MemoryBlock state;source->getStateInformation(state);engine.restorePluginState(0,state.toBase64Encoding());
        requireAudit(!source->stateRestoredActive&&source->prepared,"native state must be restored with processing inactive and reactivated before playback");
        requireAudit(instrument.plugin.get()==source&&instrument.scratch.getNumChannels()>=source->getTotalNumOutputChannels(),"state restore retains the instance and refreshes its channel buffers");
    }
    static void programEdits(){
        AudioEngine engine(AudioEngine::Offline{});for(auto& source:engine.instruments)source->prepare(44100,512);auto project=newProject(4);auto& instrument=*engine.instruments[0];
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
    static void run(){effects();exportCompatibility();stateRestoreOrder();programEdits();
        // Fixed-rate fixtures must not inherit the runner's physical audio device rate or latency.
        AudioEngine engine(AudioEngine::Offline{});for(auto& instrument:engine.instruments)instrument->prepare(44100,512);auto project=newProject(571);project.sections={{Section::main,2,"Audit"}};project.masterGain=1;project.settings.bpm=120;project.settings.freeTempo=true;
        for(auto& t:project.tracks){t.gain=0;for(auto& clip:t.clips)clip.notes.clear();}project.tracks[0].gain=1;project.normalise();
        auto probe=std::make_unique<AuditPlugin>();auto* plugin=probe.get();{const AudioEngine::EngineControlLock lock(engine);auto& i=*engine.instruments[0];i.plugin=std::move(probe);engine.hostedCount=1;i.mode=2;i.plugin->setPlayHead(&engine);i.prepare(44100,512);}
        engine.publish(project);engine.play(true);juce::AudioBuffer<float> block(2,512);juce::AudioIODeviceCallbackContext context;
        auto callback=[&]{engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);};callback();requireAudit(block.getMagnitude(0,512)>1.1f,"live floating-point output must retain headroom above unity");
        // Holding a control mutex alone cannot block the audio callback anymore.
        const int before=plugin->calls;engine.mutex.enter();juce::WaitableEvent returned;std::thread independent([&]{callback();returned.signal();});const bool nonblocking=returned.wait(10000);engine.mutex.exit();independent.join();requireAudit(nonblocking&&plugin->calls>before,"processBlock must run without acquiring the control mutex (bounded wait)");
        plugin->slow=true;std::atomic<bool> running{true};std::thread audio([&]{while(running)callback();});
        for(int n=0;n<20;++n){engine.capturePluginStates(project);engine.restorePluginState(0,"AAAAAA==");}running=false;audio.join();plugin->slow=false;requireAudit(!plugin->overlap&&!project.tracks[0].instrumentState.empty(),"plugin state snapshot executes without overlapping processBlock");
        plugin->stateReads=0;plugin->calls=0;engine.play(true);running=true;std::thread playing([&]{while(running)callback();});
        const int callsBefore=plugin->calls;const auto started=juce::Time::getMillisecondCounterHiRes();
        for(int n=0;n<40;++n){project.tracks[0].gain=.2+.01*(n%4);engine.updateMix(project);engine.reusePluginStates(project);}
        const double spent=juce::Time::getMillisecondCounterHiRes()-started;juce::Thread::sleep(20);const int callsAfter=plugin->calls;const int states=plugin->stateReads.load();running=false;playing.join();engine.stop();
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
fga::EngineAuditTests::run();}
