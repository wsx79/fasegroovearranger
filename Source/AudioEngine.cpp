// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AudioEngine.h"
#include "core/Mp3Encoder.h"
#include <cmath>
#include <cstring>
#include <optional>
#include <limits>
namespace fga {
namespace {
const std::vector<TimedEvent> noEvents;
bool voicesActive(juce::Synthesiser& synth){for(int i=0;i<synth.getNumVoices();++i)if(synth.getVoice(i)->isVoiceActive())return true;return false;}
void mixTargets(Instrument& i,double,bool snap){
    const float left=(float)(i.gain*std::min(1.,1.-i.pan)),right=(float)(i.gain*std::min(1.,1.+i.pan));
    if(snap){i.leftGain.setCurrentAndTargetValue(left);i.rightGain.setCurrentAndTargetValue(right);i.baseGain.setCurrentAndTargetValue((float)(i.gain));}
    else{i.leftGain.setTargetValue(left);i.rightGain.setTargetValue(right);i.baseGain.setTargetValue((float)(i.gain));}
}
std::unique_ptr<MixState> mixState(const Project& p){
    auto m=std::make_unique<MixState>();m->buses=p.buses;m->master=p.masterGain;m->tempo=p.settings.bpm;m->masterAutomation=p.masterAutomation;
    for(size_t ti=0;ti<(size_t)maxTracks&&ti<p.tracks.size();++ti){const auto& t=p.tracks[ti];auto& x=m->tracks[ti];
        x.group=t.group;x.sends=t.sends;
        x.sampler=t.sampler;x.sampleRoot=t.sampleRoot;
        x.volume=t.volume;x.panAutomation=t.panAutomation;x.parameters=t.parameters;x.gain=t.gain;x.pan=t.pan;x.mute=t.mute;x.solo=t.solo;x.sound=t.sound;x.tone=t.engine.tone;x.reverb=t.engine.reverb;x.chorus=t.engine.chorus;
        x.pump=t.engine.pump;x.pumpRelease=t.engine.pumpRelease;x.pumpRate=std::clamp(t.engine.pumpRate,0,3);x.pumpSource=std::clamp(t.engine.pumpSource,0,1);
        x.drumNote=isDrumTrack((int)ti)?(t.engine.midiNote>=0?t.engine.midiNote:fgd::defaultNote((int)ti-4)):60;
        for(size_t lane=0;lane<x.laneVolume.size();++lane){x.laneVolume[lane]=(float)t.engine.laneVolume[lane];x.laneMute[lane]=t.engine.laneMute[lane];}}
    return m;
}
std::uint64_t hashEvents(const std::vector<TimedEvent>& events){
    std::uint64_t h=1469598103934665603ull;auto add=[&h](std::uint64_t v){h=(h^v)*1099511628211ull;};
    for(const auto& e:events){std::uint64_t bits;std::memcpy(&bits,&e.beat,sizeof bits);add(bits);add((std::uint64_t)(std::uint32_t)e.pitch|((std::uint64_t)(std::uint32_t)e.velocity<<32));add((std::uint64_t)(std::uint32_t)e.lane|((std::uint64_t)(std::uint32_t)e.controller<<32));}
    return h^events.size();
}
const std::vector<TimedEvent>& eventsOf(const Instrument& i){return i.schedule?*i.schedule:noEvents;}
bool laneMuted(const Instrument& i,const TimedEvent& e){return (i.kind==3||i.kind==leadTrack)&&e.controller<0&&i.guitarMuted[(size_t)std::clamp(e.lane,0,fgg::numLanes-1)];}
size_t cursorAt(const Instrument& i,double beat){const auto& s=eventsOf(i);return (size_t)(std::lower_bound(s.begin(),s.end(),beat,[](const TimedEvent& e,double b){return e.beat<b;})-s.begin());}
// What the schedule holds at the cursor: the pitches sounding (held or sustained), their
// velocity, lane and start beat, and the last value of each controller.
struct Hold{std::array<int,128> vel{},lane{},cc;std::array<double,128> start{};std::array<bool,128> pressed{};};
Hold holdState(const Instrument& i){Hold h;h.cc.fill(-1);bool sustain=false;const auto& s=eventsOf(i);
    for(size_t k=0;k<i.cursor&&k<s.size();++k){const auto& e=s[k];if(laneMuted(i,e))continue;
        if(e.controller>=0){h.cc[(size_t)std::clamp(e.controller,0,127)]=e.velocity;if(e.controller==64){sustain=e.velocity>=64;if(!sustain)for(size_t n=0;n<128;++n)if(!h.pressed[n])h.vel[n]=0;}}
        else{auto n=(size_t)std::clamp(e.pitch,0,127);h.pressed[n]=e.velocity>0;if(e.velocity>0){h.vel[n]=e.velocity;h.start[n]=e.beat;}else if(!sustain)h.vel[n]=0;h.lane[n]=e.lane;}}
    return h;}
int instrumentLatency(const Instrument& i){const long long source=i.mode==2&&i.plugin?std::max(0,i.plugin->getLatencySamples()):i.mode==1&&i.sampler?static_cast<SampleSynth*>(i.sampler.get())->latencySamples():0;return (int)std::min((long long)std::numeric_limits<int>::max(),source+(i.effect?std::max(0,i.effect->getLatencySamples()):0));}
void noteOn(Instrument& i,int n,int velocity,int lane,double start){i.midi.addEvent(juce::MidiMessage::noteOn(1,n,(juce::uint8)velocity),0);if(i.kind==3&&i.mode==0)i.guitarEvents.push_back({0,n,velocity/127.f,lane});if(i.kind==leadTrack&&i.mode==0)i.leadEvents.push_back({0,n,velocity/127.f,lane});i.sounding[(size_t)n]=true;i.soundingStart[(size_t)n]=start;i.soundingVelocity[(size_t)n]=velocity;++i.noteOns;}
void noteOff(Instrument& i,int n,int lane){i.midi.addEvent(juce::MidiMessage::noteOff(1,n),0);if(i.kind==3&&i.mode==0)i.guitarEvents.push_back({0,n,0.f,lane});if(i.kind==leadTrack&&i.mode==0)i.leadEvents.push_back({0,n,0.f,lane});i.sounding[(size_t)n]=false;}
}

Instrument::Instrument(int track):kind(track){
    for(int i=0;i<fgd::numLanes;++i)drumNotes[(size_t)i]=fgd::defaultNote(i);
    delayLine.clear();drumLevels.fill(1);guitarLevels.fill(1);if(kind==3)guitarEvents.prepare();if(kind==leadTrack)leadEvents.prepare();midi.ensureSize(262144);effectMidi.ensureSize(262144);automationMidi.ensureSize(262144);
    if(kind<3||isManualTrack(kind)){for(int i=0;i<48;++i)synth.addVoice(new GuideVoice(tone,sound,kind==1));if(kind==2)synth.addSound(new GuideStringsTag(1));else synth.addSound(new GuideSoundTag);}
}
void Instrument::prepare(double rate,int block){
    leftGain.reset(rate,.01);rightGain.reset(rate,.01);baseGain.reset(rate,.01);parameterSent.fill(-1.f);
    synth.setCurrentPlaybackSampleRate(rate);if(sampler)sampler->setCurrentPlaybackSampleRate(rate);if(isDrumTrack(kind))drums.prepare(rate);if(kind==3)guitar.prepare(rate);if(kind==leadTrack)lead.prepare(rate);
    auto preparePlugin=[&](juce::AudioPluginInstance* p){if(!p)return;p->releaseResources();p->setRateAndBufferSizeDetails(rate,block);p->prepareToPlay(rate,block);};
    preparePlugin(plugin.get());preparePlugin(effect.get());
    volumeGains.resize((size_t)block);panLeft.resize((size_t)block);panRight.resize((size_t)block);adoptChannels(block);bassLowPass.prepare({rate,(juce::uint32)block,2});bassLowPass.setType(juce::dsp::StateVariableTPTFilterType::lowpass);bassLowPass.setCutoffFrequency((float)(100+1400*tone.load()));reverb.setSampleRate(rate);chorus.prepare({rate,(juce::uint32)block,2});reset();
}
void Instrument::adoptChannels(int block){
    int channels=2;for(auto* p:{plugin.get(),effect.get()})if(p)channels=std::max({channels,p->getTotalNumOutputChannels(),p->getTotalNumInputChannels()});
    outputs=mode==2&&plugin?std::max(1,plugin->getTotalNumOutputChannels()):2;scratch.setSize(channels,block);automationBlock.setSize(channels,block);
}
void Instrument::applyDelay(juce::AudioBuffer<float>& block,int count){
    if(delay<=0)return;
    delayUsed=true;auto* left=delayLine.getWritePointer(0);auto* right=delayLine.getWritePointer(1);auto* inL=block.getWritePointer(0);auto* inR=block.getWritePointer(1);int w=delayWrite;
    for(int s=0;s<count;++s){const int r=(w-delay)&(maxDelay-1);left[w]=inL[s];right[w]=inR[s];inL[s]=left[r];inR[s]=right[r];w=(w+1)&(maxDelay-1);}
    delayWrite=w;
}
void Instrument::releaseVoices(){if(sampler)sampler->allNotesOff(0,false);synth.allNotesOff(0,false);drums.allNotesOff();guitar.allNotesOff();lead.allNotesOff();midi.clear();effectMidi.clear();guitarEvents.clear();leadEvents.clear();}
void Instrument::reset(){if(delayUsed){delayLine.clear();delayUsed=false;delayWrite=0;}releaseVoices();bassLowPass.reset();reverb.reset();chorus.reset();if(plugin)plugin->reset();if(effect)effect->reset();}
AudioEngine::AudioEngine(){
    formats.addDefaultFormats();for(int i=0;i<maxTracks;++i){instruments[(size_t)i]=std::make_unique<Instrument>(i);instruments[(size_t)i]->prepare(sampleRate,blockSize);meters[(size_t)i]=0;}
    busGraph.prepare(sampleRate,blockSize);
    devices.initialise(0,2,nullptr,true);devices.addAudioCallback(this);
}
AudioEngine::AudioEngine(Offline){busGraph.prepare(sampleRate,blockSize);for(int i=0;i<maxTracks;++i){instruments[(size_t)i]=std::make_unique<Instrument>(i);meters[(size_t)i]=0;}}
AudioEngine::~AudioEngine(){finishAudioRecording();setMidiInput({});lifetime.reset();devices.removeAudioCallback(this);const EngineControlLock lock(*this);for(auto& i:instruments){if(i->plugin)i->plugin->releaseResources();if(i->effect)i->effect->releaseResources();}}
void AudioEngine::publish(const Project& source,bool normalised,int onlyTrack){
    std::optional<Project> canonical;if(!normalised){canonical=source;canonical->normalise();}const auto& p=canonical?*canonical:source;
    auto next=std::make_unique<ScheduleState>();const double songLength=p.length();
    {
        // The edited project's last schedules are kept here: when one track changed (same song
        // length) only that track's events are rebuilt. Other publishers (export, tests) drop them.
        const juce::ScopedLock cacheLock(publishedLock);
        const bool reuse=normalised&&publishedValid&&onlyTrack>=0&&onlyTrack<maxTracks&&songLength==published.length;
        if(reuse){*next=published;next->schedules[(size_t)onlyTrack].clear();for(auto& e:events(p,onlyTrack))next->schedules[(size_t)onlyTrack].push_back(e);next->hashes[(size_t)onlyTrack]=hashEvents(next->schedules[(size_t)onlyTrack]);}
        else{for(auto& e:events(p))if(e.track>=0&&e.track<maxTracks)next->schedules[(size_t)e.track].push_back(e);
            for(size_t ti=0;ti<(size_t)maxTracks;++ti)next->hashes[ti]=hashEvents(next->schedules[ti]);}
        publishedValid=normalised;next->length=songLength;if(normalised)published=*next;
    }
    next->length=songLength;next->looping=p.loop;next->loopFrom=p.loopStart;next->loopTo=p.loopEnd;
    tempo=p.settings.bpm;
    mixes.post(mixState(p));schedules.post(std::move(next));adoptWhileStopped();
}
void AudioEngine::updateMix(const Project& p){tempo=p.settings.bpm;mixes.post(mixState(p));adoptWhileStopped();}
// Stopped, the output is silent anyway: take the new state at once, so transport calls made
// right after an edit (seek, loop, length) already see it, with or without a running device.
void AudioEngine::adoptWhileStopped(){if(playing.load()||exporting.load())return;const EngineControlLock lock(*this);if(playing.load()||exporting.load())return;collectGarbage();adoptState();collectGarbage();}
void AudioEngine::applyMix(const MixState& m,bool snap){
    busGraph.configure(m.buses,m.tempo,m.master,snap);
    master=m.master;masterAutomation=&m.masterAutomation;
    for(size_t ti=0;ti<instruments.size();++ti){auto& i=*instruments[ti];const auto& t=m.tracks[ti];
        if(i.parameters!=&t.parameters)i.parameterSent.fill(-1.f);   // other lanes: set every parameter again
        if(auto* sampler=dynamic_cast<SampleSynth*>(i.sampler.get()))if(sampler->configure(t.sampler,t.sampleRoot,m.tempo)&&playing){i.sounding.fill(false);i.resync=true;needsResync=true;}
        i.group=t.group;i.sends=t.sends;
        i.volume=&t.volume;i.panAutomation=&t.panAutomation;i.parameters=&t.parameters;i.gain=t.gain;i.pan=t.pan;mixTargets(i,master,snap);i.mute=t.mute;i.solo=t.solo;i.sound=(float)t.sound;i.tone=(float)t.tone;
        i.bassLowPass.setCutoffFrequency((float)(100+1400*t.tone));i.drums.setKit(t.sound);i.drums.setTone((float)t.tone);i.guitar.setSound(t.sound);i.guitar.setTone((float)t.tone);i.lead.setSound(t.sound);i.lead.setTone((float)t.tone);
        for(size_t lane=0;lane<i.guitarLevels.size();++lane){i.guitarLevels[lane]=t.laneVolume[lane];if(i.guitarMuted[lane]!=t.laneMute[lane]){i.guitarMuted[lane]=t.laneMute[lane];if(playing){i.resync=true;needsResync=true;}}}
        juce::Reverb::Parameters rp;rp.roomSize=.4f;rp.wetLevel=(float)t.reverb*.4f;rp.dryLevel=.5f;rp.width=1;i.reverb.setParameters(rp);i.chorus.setMix((float)t.chorus*.5f);i.chorus.setRate(.35f);i.chorus.setDepth(.25f);
        i.pump=t.pump;i.pumpRelease=t.pumpRelease;i.pumpRate=t.pumpRate;i.pumpSource=t.pumpSource;if(isDrumTrack((int)ti))i.drumNotes[ti-4]=t.drumNote;}
}
// Audio side (or export, under the lock): take the newest published state, if any.
void AudioEngine::adoptState(){
    if(auto* m=mixes.adopt())applyMix(*m,!playing.load());
    // The replaced state is parked for the message thread to free: read its hashes before.
    const bool hadOld=schedules.current!=nullptr;std::array<std::uint64_t,maxTracks> oldHashes{};if(hadOld)oldHashes=schedules.current->hashes;
    if(auto* s=schedules.adopt()){
        length=s->length;looping=s->looping;loopFrom=std::clamp(s->loopFrom,0.,length.load());loopTo=std::clamp(s->loopTo,loopFrom.load(),length.load());
        currentBeat=std::min(currentBeat.load(),length.load());loopBypassed=currentBeat.load()>=loopTo.load();publishTransport();
        for(size_t ti=0;ti<instruments.size();++ti){auto& i=*instruments[ti];i.schedule=&s->schedules[ti];
            if(hadOld&&oldHashes[ti]==s->hashes[ti])continue; // same events: the cursor still points at the same place
            i.cursor=cursorAt(i,currentBeat);if(playing){i.resync=true;needsResync=true;}}
    }
}
void AudioEngine::play(bool enabled){stopSampleAudition();if(enabled&&currentBeat.load()>=length.load()){currentBeat=looping.load()?loopFrom.load():0;loopBypassed=false;}if(enabled!=playing.load())hardReset=true;playing=enabled;if(!enabled)clearMeters();needsReset=true;needsChase=enabled;publishTransport();}
void AudioEngine::stop(){play(false);seek(0);capture.inputOverflow=false;}
// An explicit seek beyond the loop must remain audible there until the song ends.
void AudioEngine::seek(double beat){currentBeat=std::clamp(beat,0.0,length.load());loopBypassed=currentBeat.load()>=loopTo.load();publishTransport();needsReset=true;needsChase=playing.load();}
void AudioEngine::setLoop(bool enabled,double from,double to){const auto revision=positionRevision.load();if(!(revision&1)&&positionLoopSetting.load()==enabled&&positionLoopFrom.load()==from&&positionLoopTo.load()==to&&revision==positionRevision.load())return;looping=enabled;loopFrom=std::clamp(from,0.0,length.load());loopTo=std::clamp(to,loopFrom.load(),length.load());loopBypassed=currentBeat.load()>=loopTo.load();publishTransport();}
void AudioEngine::resetAt(double beat){
    // Stopped or starting: a full reset (silent anyway). Playing (loop wrap, seek): release the
    // sounding notes with their natural release and keep effect tails, so the jump is click-free.
    const bool hard=!playing||hardReset;if(hard){busGraph.reset();liveTail=0;if(!capture.active){for(auto& keys:liveHeld)keys.fill(0);for(auto& pedal:livePedal)pedal.fill(false);}}
    for(auto& ptr:instruments){auto& i=*ptr;if(hard)i.reset();i.resync=false;i.cursor=cursorAt(i,beat);
        // Explicit note-offs first: many plugins ignore All Notes Off (CC 123).
        for(int n=0;n<128;++n)if(i.sounding[(size_t)n])noteOff(i,n,0);
        i.midi.addEvent(juce::MidiMessage::controllerEvent(1,64,0),0);i.midi.addEvent(juce::MidiMessage::controllerEvent(1,11,127),0);
        if(hard){i.midi.addEvent(juce::MidiMessage::allNotesOff(1),0);if(capture.active)for(int channel=0;channel<16;++channel){if(livePedal[(size_t)i.kind][(size_t)channel])i.midi.addEvent(juce::MidiMessage::controllerEvent(channel+1,64,127),0);for(int note=0;note<128;++note){auto velocity=liveHeld[(size_t)i.kind][(size_t)(channel*128+note)];if(velocity){i.midi.addEvent(juce::MidiMessage::noteOn(channel+1,note,(juce::uint8)(velocity&127)),0);if(velocity&128)i.midi.addEvent(juce::MidiMessage::noteOff(channel+1,note),0);}}}}
        if(needsChase){const auto h=holdState(i);
            for(int c=0;c<128;++c)if(h.cc[(size_t)c]>=0)i.midi.addEvent(juce::MidiMessage::controllerEvent(1,c,h.cc[(size_t)c]),0);
            for(int n=0;n<128;++n)if(h.vel[(size_t)n]>0){noteOn(i,n,h.vel[(size_t)n],h.lane[(size_t)n],h.start[(size_t)n]);if(!h.pressed[(size_t)n])noteOff(i,n,h.lane[(size_t)n]);}}
    }needsReset=false;needsChase=false;needsResync=false;hardReset=false;
}
void AudioEngine::resyncAt(double beat){
    // An edit while playing: no reset. Only notes whose MIDI actually changed are released
    // or (re)struck; everything else keeps ringing untouched.
    for(auto& ptr:instruments){auto& i=*ptr;if(!i.resync)continue;i.resync=false;i.cursor=cursorAt(i,beat);const auto h=holdState(i);
        for(int c=0;c<128;++c)if(h.cc[(size_t)c]>=0)i.midi.addEvent(juce::MidiMessage::controllerEvent(1,c,h.cc[(size_t)c]),0);
        for(int n=0;n<128;++n){const auto k=(size_t)n;const bool want=h.vel[k]>0&&h.pressed[k];
            if(i.sounding[k]&&(!want||i.soundingStart[k]!=h.start[k]||i.soundingVelocity[k]!=h.vel[k]))noteOff(i,n,h.lane[k]);
            if(want&&!i.sounding[k])noteOn(i,n,h.vel[k],h.lane[k],h.start[k]);}
    }needsResync=false;
}
void AudioEngine::flushIdle(int count){
    // Stopped: deliver pending note-offs to plugins now, so no voice is left hanging inside a
    // plugin until the next PLAY. Their audio is discarded (a stopped transport is silent).
    const int n=std::max(1,std::min(count,blockSize));
    for(auto& ptr:instruments){auto& i=*ptr;
        if(i.mode==2&&i.plugin&&!i.midi.isEmpty()){i.scratch.setSize(i.scratch.getNumChannels(),n,false,false,true);i.scratch.clear();i.plugin->processBlock(i.scratch,i.midi);}
        i.midi.clear();i.guitarEvents.clear();i.leadEvents.clear();}
}
void AudioEngine::setTimeSignature(int numerator,int denominator){const EngineControlLock lock(*this);timeNumerator=std::clamp(numerator,1,32);timeDenominator=(denominator>0&&(denominator&(denominator-1))==0)?std::min(64,denominator):4;}
void AudioEngine::click(float* const* output,int channels,int count,double beat,bool force){
    if(!metronome.load()&&!force){clickLeft=0;return;}
    const double samplesPerBeat=sampleRate*60./tempo.load();const double pulse=4./timeDenominator.load();double next=std::ceil(beat/pulse-1e-9)*pulse;
    auto triggerAt=[&]{return (int)std::llround((next-beat)*samplesPerBeat);};int trigger=triggerAt();
    for(int s=0;s<count;++s){
        if(s==trigger){clickLeft=(int)(sampleRate*.03);clickPos=0;clickHz=((long long)std::llround(next/pulse)%timeNumerator.load()==0)?1760.:1100.;next+=pulse;trigger=triggerAt();}
        if(clickLeft<=0)continue;
        const float v=(float)(.22*std::sin(juce::MathConstants<double>::twoPi*clickHz*clickPos/sampleRate)*std::exp(-clickPos/(sampleRate*.007)));
        for(int ch=0;ch<channels;++ch)if(output[ch])output[ch][s]+=v;
        ++clickPos;--clickLeft;}
}
// Hosted lanes are evaluated at exact breakpoint boundaries and bounded ramp intervals.
void AudioEngine::applyParameterAutomation(Instrument& i,double beat,int slot){
    if(i.parameters==nullptr)return;
    const size_t lanes=std::min(i.parameters->size(),(size_t)maxParameterLanes);
    for(size_t k=0;k<lanes;++k){const auto& lane=(*i.parameters)[k];if(lane.slot!=slot||!lane.curve.active()||lane.index<0)continue;
        auto* target=lane.slot==0?(i.mode==2?i.plugin.get():nullptr):i.effect.get();if(target==nullptr)continue;
        const auto& params=target->getParameters();if(lane.index>=params.size())continue;
        const float value=(float)std::clamp(lane.curve.valueAt(beat),0.,1.);
        if(std::abs(value-i.parameterSent[k])<1.e-5f)continue;
        i.parameterSent[k]=value;params[lane.index]->setValue(value);}
}
// Split only the hosted processor: MIDI offsets, bus DSP and transport keep the
// original device-block clock. Owned work buffers also handle >32 plugin channels
// without AudioBuffer views allocating channel-pointer arrays in the callback.
void AudioEngine::processHosted(Instrument& instrument,juce::AudioPluginInstance& plugin,juce::AudioBuffer<float>& block,juce::MidiBuffer& midi,double beat,int slot){
    bool automated=false;if(instrument.parameters)for(const auto& lane:*instrument.parameters)automated=automated||(lane.slot==slot&&lane.index>=0&&lane.index<plugin.getParameters().size()&&lane.curve.active());
    if(!automated){plugin.processBlock(block,midi);return;}
    const double samplesPerBeat=sampleRate*60./tempo.load();const int channels=block.getNumChannels();
    for(int offset=0;offset<block.getNumSamples();){const double at=beat+offset/samplesPerBeat;int n=std::min(64,block.getNumSamples()-offset);
        for(const auto& lane:*instrument.parameters){if(lane.slot!=slot||!lane.curve.active()||lane.index<0||lane.index>=plugin.getParameters().size())continue;
            auto next=std::upper_bound(lane.curve.points.begin(),lane.curve.points.end(),at+1e-10,[](double b,const auto& point){return b<point.beat;});
            if(next!=lane.curve.points.end()&&(next->beat-at)*samplesPerBeat<n)n=std::min(n,std::max(1,(int)std::ceil((next->beat-at)*samplesPerBeat-1e-8)));
        }
        instrument.automationBlock.setSize(channels,n,false,false,true);for(int c=0;c<channels;++c)instrument.automationBlock.copyFrom(c,0,block,c,offset,n);
        instrument.automationMidi.clear();instrument.automationMidi.addEvents(midi,offset,n,-offset);applyParameterAutomation(instrument,at,slot);pluginBeatOffset=offset/samplesPerBeat;plugin.processBlock(instrument.automationBlock,instrument.automationMidi);pluginBeatOffset=0;
        for(int c=0;c<channels;++c)block.copyFrom(c,offset,instrument.automationBlock,c,0,n);offset+=n;
    }
}
void AudioEngine::refreshLatencies(){
    // Plugin delay compensation: instrument + insert latency, every other track delayed to match.
    int maxLatency=0;for(auto& ptr:instruments){auto& i=*ptr;i.latency=instrumentLatency(i);maxLatency=std::max(maxLatency,i.latency);}
    latencyLimited=maxLatency>Instrument::maxDelay-1;maxLatency=std::min(maxLatency,Instrument::maxDelay-1);reportedLatency=maxLatency;
    for(auto& ptr:instruments)ptr->delay=std::clamp(maxLatency-ptr->latency,0,Instrument::maxDelay-1);
}
void AudioEngine::render(float* const* output,int channels,int count,double beat,int onlyTrack,bool scheduled){
    const double samplesPerBeat=sampleRate*60./tempo.load(),end=beat+count/samplesPerBeat;
    busGraph.begin(count,onlyTrack>=0);
    bool anySolo=false;for(auto& i:instruments)anySolo=anySolo||i->solo;
    refreshLatencies();
    // Master automation: one gain per sample for every track (the buffer is allocated once).
    const bool masterActive=masterAutomation&&masterAutomation->active()&&count<=(int)masterGains.size();
    if(masterActive)for(int s=0;s<count;++s)masterGains[(size_t)s]=(float)masterAutomation->valueAt(beat+s/samplesPerBeat);
    for(int ti=0;ti<maxTracks;++ti){auto& i=*instruments[(size_t)ti];const auto& schedule=eventsOf(i);int queued=i.midi.getNumEvents();bool dense=queued>16384;
        while(scheduled&&i.cursor<schedule.size()&&schedule[i.cursor].beat<end){if(queued>=16384){dense=true;i.cursor=cursorAt(i,end);break;}++queued;const auto& e=schedule[i.cursor++];if(e.beat<beat-1e-8||laneMuted(i,e))continue;int at=std::clamp((int)std::llround((e.beat-beat)*samplesPerBeat),0,count-1);
            i.midi.addEvent(e.controller>=0?juce::MidiMessage::controllerEvent(1,e.controller,e.velocity):e.velocity?juce::MidiMessage::noteOn(1,e.pitch,(juce::uint8)e.velocity):juce::MidiMessage::noteOff(1,e.pitch),at);
            if(e.controller<0){const auto k=(size_t)std::clamp(e.pitch,0,127);i.sounding[k]=e.velocity>0;if(e.velocity>0){i.soundingStart[k]=e.beat;i.soundingVelocity[k]=e.velocity;++i.noteOns;}}
            if(ti==3&&i.mode==0&&e.controller<0)i.guitarEvents.push_back({at,e.pitch,e.velocity/127.f,e.lane});
            if(ti==leadTrack&&i.mode==0&&e.controller<0)i.leadEvents.push_back({at,e.pitch,e.velocity/127.f,e.lane});
        }
        if(dense){
            // Bound work and MIDI storage for malformed/dense schedules. Recover only this
            // track; unrelated instruments and the transport keep running.
            i.midi.clear();for(int note=0;note<128;++note)if(i.sounding[(size_t)note])i.midi.addEvent(juce::MidiMessage::noteOff(1,note),0);
            for(int channel=0;channel<16;++channel){for(int note=0;note<128;++note)if(liveHeld[(size_t)ti][(size_t)(channel*128+note)])i.midi.addEvent(juce::MidiMessage::noteOff(channel+1,note),0);i.midi.addEvent(juce::MidiMessage::allSoundOff(channel+1),0);}
            liveHeld[(size_t)ti].fill(0);livePedal[(size_t)ti].fill(false);i.sounding.fill(false);i.releaseVoices();capture.inputOverflow=true;
        }
        // Resized in place (never reallocated: prepare() sized it for the whole block), even with >32 plugin channels.
        i.scratch.setSize(i.scratch.getNumChannels(),count,false,false,true);auto& block=i.scratch;block.clear();
        if(i.effect){i.effectMidi.clear();i.effectMidi.addEvents(i.midi,0,count,0);}
        const auto ordered=[](const auto& a,const auto& b){if(a.sample!=b.sample)return a.sample<b.sample;if((a.velocity<=0)!=(b.velocity<=0))return a.velocity<=0;return a.note<b.note;};
        if(i.guitarEvents.size()>1&&!std::is_sorted(i.guitarEvents.begin(),i.guitarEvents.end(),ordered))std::sort(i.guitarEvents.begin(),i.guitarEvents.end(),ordered);
        if(i.leadEvents.size()>1&&!std::is_sorted(i.leadEvents.begin(),i.leadEvents.end(),ordered))std::sort(i.leadEvents.begin(),i.leadEvents.end(),ordered);
        if(i.guitarEvents.dropped){i.guitar.allNotesOff();i.guitarEvents.clear();capture.inputOverflow=true;}
        if(i.leadEvents.dropped){i.lead.allNotesOff();i.leadEvents.clear();capture.inputOverflow=true;}
        if(i.mode==2&&i.plugin)processHosted(i,*i.plugin,block,i.midi,beat,0);
        else if(i.mode==1){if(i.sampler)i.sampler->renderNextBlock(block,i.midi,0,count);}
        else if(i.mode==0){if(ti<3||isManualTrack(ti))i.synth.renderNextBlock(block,i.midi,0,count);else if(ti==3)i.guitar.render(block,i.guitarEvents,i.guitarLevels);else if(ti==leadTrack)i.lead.render(block,i.leadEvents,i.guitarLevels);else i.drums.render(block,i.midi,i.drumNotes,i.drumLevels,i.drumSamples);}
        if(i.mode==1)pitchMonitor.push(ti,block.getReadPointer(0),block.getNumChannels()>1?block.getReadPointer(1):nullptr,count,sampleRate);
        publishActiveNotes(ti);
        if(i.outputs==1&&block.getNumChannels()>1)block.copyFrom(1,0,block,0,0,count); // mono instrument: both sides
        if(i.mode==0){
            juce::dsp::AudioBlock<float> stereo(block.getArrayOfWritePointers(),2,(size_t)count);juce::dsp::ProcessContextReplacing<float> context(stereo);if(ti==1)i.bassLowPass.process(context);i.chorus.process(context);
            i.reverb.processStereo(block.getWritePointer(0),block.getWritePointer(1),count);
            if(i.pump>0){static const double rates[]{1,.5,2,.25};const auto& kick=eventsOf(*instruments[4]);
                // Kick source: the last kick at or before the block start is found once, then each
                // sample only steps over the kicks it passes (times grow through the block).
                auto next=kick.begin();bool hasKick=false;double lastKick=0;
                if(i.pumpSource==1){next=std::upper_bound(kick.begin(),kick.end(),beat,[](double t,const TimedEvent& e){return t<e.beat;});for(auto it=next;it!=kick.begin();){--it;if(it->controller<0&&it->velocity>0){hasKick=true;lastKick=it->beat;break;}}}
                for(int sample=0;sample<count;++sample){const double time=beat+sample/samplesPerBeat;double phase=std::fmod(time,rates[std::clamp(i.pumpRate,0,3)]);if(i.pumpSource==1){for(;next!=kick.end()&&!(time<next->beat);++next)if(next->controller<0&&next->velocity>0){hasKick=true;lastKick=next->beat;}phase=hasKick?time-lastKick:100;}const float gain=(float)(1-i.pump*std::exp(-phase/std::max(.05,i.pumpRelease)));for(int ch=0;ch<2;++ch)block.setSample(ch,sample,block.getSample(ch,sample)*gain);}}
        }
        if(i.effect)processHosted(i,*i.effect,block,i.effectMidi,beat,1);
        i.applyDelay(block,count);
        i.midi.clear();i.guitarEvents.clear();i.leadEvents.clear();
        const bool audible=onlyTrack>=0?ti==onlyTrack:(!i.mute&&(!anySolo||i.solo));
        float meter=0,side[2]{};
        if(audible){if(i.volume&&i.volume->enabled&&!i.volume->points.empty())for(int s=0;s<count;++s)i.volumeGains[(size_t)s]=(float)i.volume->valueAt(beat+s/samplesPerBeat);else std::fill(i.volumeGains.begin(),i.volumeGains.begin()+count,1.f);

            // Pan automation: its own pan law per sample (as the static pan), on gain * master.
            const bool panActive=i.panAutomation&&i.panAutomation->active();
            if(panActive){for(int s=0;s<count;++s){const double pan=panFromAutomation(i.panAutomation->valueAt(beat+s/samplesPerBeat));const float base=i.baseGain.getNextValue();i.panLeft[(size_t)s]=base*(float)std::min(1.,1.-pan);i.panRight[(size_t)s]=base*(float)std::min(1.,1.+pan);}}
            else i.baseGain.skip(count);
            for(int ch=0;ch<std::min(2,channels);++ch){if(!output[ch]){(ch==0?i.leftGain:i.rightGain).skip(count);continue;}auto* in=block.getReadPointer(std::min(ch,block.getNumChannels()-1));auto& gain=ch==0?i.leftGain:i.rightGain;
                if(panActive){gain.skip(count);const auto& pg=ch==0?i.panLeft:i.panRight;for(int s=0;s<count;++s){const float v=in[s]*pg[(size_t)s]*i.volumeGains[(size_t)s];busGraph.add(ch,s,v,i.group,i.sends);side[ch]=std::max(side[ch],std::abs(v));}meter=std::max(meter,side[ch]);continue;}
                for(int s=0;s<count;++s){const float v=in[s]*gain.getNextValue()*i.volumeGains[(size_t)s];busGraph.add(ch,s,v,i.group,i.sends);side[ch]=std::max(side[ch],std::abs(v));}meter=std::max(meter,side[ch]);}
        }else{i.leftGain.skip(count);i.rightGain.skip(count);i.baseGain.skip(count);}
        meters[(size_t)ti]=meter;sideMeters[(size_t)ti][0]=side[0];sideMeters[(size_t)ti][1]=side[1];
    }
    busGraph.finish(output,channels,count,masterActive?masterGains.data():nullptr);
}
void AudioEngine::prepareBuses(double rate,int block){busGraph.prepare(rate,block);if(mixes.current)busGraph.configure(mixes.current->buses,mixes.current->tempo,mixes.current->master,true);}
void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* d){const EngineControlLock lock(*this);if(recordingAudio&&audioRecorder){audioRecorder->rateChanged=true;audioRecordFault=true;}deviceRate=std::isfinite(d->getCurrentSampleRate())&&d->getCurrentSampleRate()>0?d->getCurrentSampleRate():44100;deviceBlock=std::max(16,d->getCurrentBufferSizeSamples());if(exporting)return;sampleRate=deviceRate;blockSize=deviceBlock;publishTransport();prepareBuses(sampleRate,blockSize);deviceOutputLatency=d->getOutputLatencyInSamples();deviceInputLatency=d->getInputLatencyInSamples();for(auto& i:instruments){i->prepare(sampleRate,blockSize);i->sounding.fill(false);}needsReset=true;needsChase=playing;hardReset=true;}
void AudioEngine::audioDeviceStopped(){stopSampleAudition();const EngineControlLock lock(*this);if(recordingAudio&&audioRecorder){audioRecorder->rateChanged=true;audioRecordFault=true;}if(exporting)return;clearMeters();for(auto& i:instruments){i->reset();i->sounding.fill(false);}}
void AudioEngine::audioDeviceIOCallbackWithContext(const float* const* input,int inputChannels,float* const* output,int channels,int count,const juce::AudioIODeviceCallbackContext&){
    if(count<=0)return;
    for(int ch=0;ch<channels;++ch)if(output[ch])juce::FloatVectorOperations::clear(output[ch],count);
    const double now=juce::Time::getMillisecondCounterHiRes()*.001;previousInputBlockTime=inputBlockTime>0?inputBlockTime:now-count/positionRate.load();inputBlockTime=now;
    captureAudio(input,inputChannels,count,AudioSource::input);
    if(maintenance.load()||processing.exchange(true)){finalizeOutput(output,channels,count);return;}
    const ProcessingScope scope{processing};struct OutputScope {AudioEngine& engine;float* const* output;int channels,count;~OutputScope(){engine.finalizeOutput(output,channels,count);}} finishOutput{*this,output,channels,count};if(maintenance.load())return;
    mixPreview(output,channels,count);if(exporting)return;
    adoptState();
    if(needsReset)resetAt(currentBeat);else if(needsResync&&playing)resyncAt(currentBeat);
    prepareLiveInput(count);
    const bool audition=auditioning.load();
    if(audition){pendingLiveCursor=pendingLiveCount;mixSampleAudition(output,channels,count);}
    if(!audition&&!playing){processLive(std::min(count,blockSize),currentBeat,false);bool voices=false;for(auto& i:instruments)voices=voices||(i->sampler&&voicesActive(*i->sampler))||voicesActive(i->synth);if(voices)liveTail=std::max(liveTail,(int)sampleRate*2);if(capture.monitor||liveTail>0||pendingLiveCursor<pendingLiveCount){int done=0;while(done<count){int n=std::min(blockSize,count-done);float* pointers[2]{};const int used=std::min(channels,2);for(int ch=0;ch<used;++ch)pointers[ch]=output[ch]?output[ch]+done:nullptr;if(done>0){inputSegmentOffset=done;processLive(n,currentBeat,false);}render(pointers,used,n,currentBeat,-1,false);done+=n;}liveTail=std::max(0,liveTail-count);}else{flushIdle(count);clearMeters();return;}}
    int done=0;
    while(!audition&&done<count&&playing){double beat=currentBeat;
        if(capture.active&&capture.countIn>0){const double remaining=capture.countIn.load();const int n=std::min({blockSize,count-done,std::max(1,(int)std::ceil(remaining*sampleRate*60/tempo.load()))});float* pointers[2]{};const int used=std::min(2,channels);for(int c=0;c<used;++c)pointers[c]=output[c]?output[c]+done:nullptr;inputSegmentOffset=done;processLive(n,beat,false);render(pointers,used,n,beat,-1,false);click(pointers,used,n,-remaining,true);capture.countIn=std::max(0.,remaining-n*tempo.load()/(sampleRate*60));done+=n;if(capture.countIn<=0){recordWallStart=juce::Time::getMillisecondCounterHiRes()*.001;chaseRecord=true;}continue;}
        const bool hasLoop=looping.load()&&!loopBypassed.load()&&loopTo.load()-loopFrom.load()>=1./960.;double boundary=hasLoop?loopTo.load():length.load();
        if(beat>=boundary-1e-9){if(hasLoop){currentBeat=loopFrom;needsChase=true;resetAt(loopFrom);continue;}playing=false;needsReset=true;resetAt(beat);flushIdle(count-done);break;}
        const int remain=std::max(1,(int)std::ceil((boundary-beat)*sampleRate*60./tempo.load()));int n=std::min({count-done,blockSize,remain});
        float* pointers[2]{};int used=std::min(2,channels);for(int ch=0;ch<used;++ch)pointers[ch]=output[ch]?output[ch]+done:nullptr;
        if(capture.active&&chaseRecord)chaseLiveRecord(beat);inputSegmentOffset=done;processLive(n,beat,capture.active.load());render(pointers,used,n,beat);if(capture.active)capture.elapsed=capture.elapsed.load()+n*tempo.load()/(sampleRate*60);click(pointers,used,n,beat);currentBeat=beat+n*tempo.load()/(sampleRate*60.);done+=n;
    }
    // Finalization also covers stopped preview, audition and all early returns.
}
void AudioEngine::finalizeOutput(float* const* output,int channels,int count){
    float outputPeak=0,sides[2]{};for(int ch=0;ch<channels;++ch)if(output[ch])for(int s=0;s<count;++s){if(!std::isfinite(output[ch][s])){output[ch][s]=0;masterClip=true;}const float v=std::abs(output[ch][s]);outputPeak=std::max(outputPeak,v);sides[std::min(ch,1)]=std::max(sides[std::min(ch,1)],v);}
    masterMeter=outputPeak;masterSides[0]=sides[0];masterSides[1]=channels>1?sides[1]:sides[0];if(outputPeak>=1)masterClip=true;
    captureAudio(const_cast<const float* const*>(output),channels,count,AudioSource::master);
}
juce::Optional<juce::AudioPlayHead::PositionInfo> AudioEngine::getPosition() const{
    PositionInfo p;const double beat=currentBeat.load()+pluginBeatOffset.load(),bpm=tempo.load();double rate=44100,from=0,to=0;bool loop=false;
    for(int attempt=0;attempt<3;++attempt){const auto before=positionRevision.load();if(before&1)continue;rate=positionRate.load();from=positionLoopFrom.load();to=positionLoopTo.load();loop=positionLooping.load();if(before==positionRevision.load())break;}
    const int numerator=timeNumerator.load(),denominator=timeDenominator.load();const double bar=numerator*4./denominator;
    p.setBpm(bpm);p.setPpqPosition(beat);p.setTimeInSeconds(beat*60./bpm);p.setTimeInSamples((juce::int64)std::llround(beat*60./bpm*rate));
    p.setIsPlaying(playing.load()||exporting.load());p.setTimeSignature(juce::AudioPlayHead::TimeSignature{numerator,denominator});
    p.setPpqPositionOfLastBarStart(std::floor(beat/bar)*bar);p.setBarCount((juce::int64)std::floor(beat/bar));
    p.setIsLooping(loop&&!exporting.load());if(loop)p.setLoopPoints(juce::AudioPlayHead::LoopPoints{from,to});
    return p;
}
namespace {
// A track changes instrument while the song may be playing: forget what the old one was holding
// and let resync strike the held notes on the new one. Only this track is touched.
void restrike(Instrument& i){i.releaseVoices();i.sounding.fill(false);i.resync=true;}
bool configure(juce::AudioPluginInstance& next,bool effect,juce::String& error){
    next.enableAllBuses();auto layout=next.getBusesLayout();
    if(effect){if(!layout.inputBuses.isEmpty())layout.inputBuses.getReference(0)=juce::AudioChannelSet::stereo();}
    else for(auto& b:layout.inputBuses)b=juce::AudioChannelSet::disabled();
    if(!layout.outputBuses.isEmpty())layout.outputBuses.getReference(0)=juce::AudioChannelSet::stereo();
    if(next.checkBusesLayoutSupported(layout))next.setBusesLayout(layout);
    if(next.getTotalNumOutputChannels()<1||next.getTotalNumOutputChannels()>64){error="Il plugin deve avere da 1 a 64 uscite audio.";return false;}
    return true;
}
}
// Loading never holds the engine lock for the slow part: files are read, plugins created and
// prepared, and old instruments destroyed outside it. Under the lock only pointers are swapped.
void AudioEngine::detachSource(int track,int mode,std::unique_ptr<juce::Synthesiser> sampler,std::unique_ptr<juce::AudioPluginInstance>& oldPlugin,std::unique_ptr<juce::Synthesiser>& oldSampler){
    auto& i=*instruments[(size_t)track];if(i.plugin)hostedCount.fetch_sub(1);oldPlugin=std::move(i.plugin);oldSampler=std::move(i.sampler);i.sampler=std::move(sampler);i.mode=mode;i.adoptChannels(blockSize);restrike(i);needsResync=true;
}
void AudioEngine::useInternal(int track){
    std::unique_ptr<juce::AudioPluginInstance> oldPlugin;std::unique_ptr<juce::Synthesiser> oldSampler;
    {const EngineControlLock lock(*this);auto& i=*instruments[(size_t)track];if(i.mode==0&&!i.plugin&&!i.sampler)return;detachSource(track,0,{},oldPlugin,oldSampler);}
    if(oldPlugin)oldPlugin->releaseResources();
}
std::shared_ptr<const SampleAsset> AudioEngine::sampleAsset(int track)const{if(track<0||track>=maxTracks)return {};auto* sampler=dynamic_cast<SampleSynth*>(instruments[(size_t)track]->sampler.get());return sampler?sampler->sharedAsset():nullptr;}
juce::Result AudioEngine::loadSample(int track,const juce::File& file,int root,const SamplerSettings& settings){
    if(track<0||track>=maxTracks)return juce::Result::fail("Invalid sample track.");
    std::shared_ptr<const SampleAsset> asset;if(auto r=readSampleAsset(file,asset);r.failed())return r;
    auto next=std::make_unique<SampleSynth>(asset,settings,std::clamp(root,0,127),isDrumTrack(track));
    double rate=0;{const EngineControlLock lock(*this);rate=sampleRate;}next->setCurrentPlaybackSampleRate(rate);
    std::unique_ptr<juce::AudioPluginInstance> oldPlugin;std::unique_ptr<juce::Synthesiser> oldSampler;
    {const EngineControlLock lock(*this);if(rate!=sampleRate)next->setCurrentPlaybackSampleRate(sampleRate);detachSource(track,1,std::move(next),oldPlugin,oldSampler);}
    if(oldPlugin)oldPlugin->releaseResources();return juce::Result::ok();
}
void AudioEngine::swapInstrument(int track,std::unique_ptr<juce::AudioPluginInstance> next,bool effect,const juce::PluginDescription& description){
    invalidatePluginStates();
    for(;;){double rate=0;int block=0;{const EngineControlLock lock(*this);rate=sampleRate;block=blockSize;}
        next->setPlayHead(this);next->setRateAndBufferSizeDetails(rate,block);next->prepareToPlay(rate,block);
        std::unique_ptr<juce::AudioPluginInstance> old;std::unique_ptr<juce::Synthesiser> oldSampler;bool retry=false;
        {const EngineControlLock lock(*this);retry=rate!=sampleRate||block!=blockSize;
            if(!retry){auto& i=*instruments[(size_t)track];if(effect){if(!i.effect)hostedCount.fetch_add(1);old=std::move(i.effect);i.effect=std::move(next);i.effectDescription=description;i.effectMidi.clear();}
                else{if(!i.plugin)hostedCount.fetch_add(1);old=std::move(i.plugin);oldSampler=std::move(i.sampler);i.plugin=std::move(next);i.pluginDescription=description;i.mode=2;restrike(i);needsResync=true;}i.adoptChannels(blockSize);}}
        if(retry){next->releaseResources();continue;}if(old)old->releaseResources();return;
    }
}
void AudioEngine::loadHosted(int track,const juce::PluginDescription& description,bool effect,std::function<void(juce::Result)> callback){
    double rate=44100;int size=512;{const EngineControlLock lock(*this);rate=sampleRate;size=blockSize;}
    formats.createPluginInstanceAsync(description,rate,size,[this,track,callback,description,effect,weak=std::weak_ptr<int>(lifetime)](std::unique_ptr<juce::AudioPluginInstance> next,const juce::String& error){
        if(weak.expired())return;
        if(!next){callback(juce::Result::fail(error));return;}
        juce::String problem;if(!configure(*next,effect,problem)){callback(juce::Result::fail(problem));return;}
        swapInstrument(track,std::move(next),effect,description);callback(juce::Result::ok());
    });
}
void AudioEngine::loadPlugin(int track,const juce::PluginDescription& description,std::function<void(juce::Result)> callback){loadHosted(track,description,false,std::move(callback));}
void AudioEngine::loadEffect(int track,const juce::PluginDescription& description,std::function<void(juce::Result)> callback){loadHosted(track,description,true,std::move(callback));}
void AudioEngine::clearEffect(int track){std::unique_ptr<juce::AudioPluginInstance> old;{const EngineControlLock lock(*this);auto& i=*instruments[(size_t)track];if(!i.effect)return;hostedCount.fetch_sub(1);old=std::move(i.effect);i.effectMidi.clear();i.adoptChannels(blockSize);}old->releaseResources();}
std::unique_ptr<AudioEngine> AudioEngine::offlineCopy(){
    struct PluginSnapshot {juce::PluginDescription description;juce::AudioProcessor::BusesLayout layout;juce::MemoryBlock state;bool present=false;};
    struct TrackSnapshot {int mode=0;std::shared_ptr<const SampleAsset> sample;PluginSnapshot instrument,effect;};
    auto snapshots=std::make_unique<std::array<TrackSnapshot,maxTracks>>();double rate=44100;int size=512;
    {
        // No processBlock can overlap state reads. Release the control gate before factory
        // creation/prepareToPlay, which can take seconds for a hosted instrument.
        const EngineControlLock lock(*this);if(exporting)return nullptr;rate=deviceRate;size=deviceBlock;
        for(size_t ti=0;ti<instruments.size();++ti){const auto& live=*instruments[ti];auto& saved=(*snapshots)[ti];saved.mode=live.mode;if(live.mode==3)return nullptr;
            if(live.mode==1){auto* sampler=dynamic_cast<SampleSynth*>(live.sampler.get());if(!sampler)return nullptr;saved.sample=sampler->sharedAsset();}
            auto captureState=[](juce::AudioPluginInstance* plugin,const juce::PluginDescription& description,PluginSnapshot& saved){if(!plugin)return;saved.present=true;saved.description=description;saved.layout=plugin->getBusesLayout();plugin->getStateInformation(saved.state);};
            if(live.mode==2)captureState(live.plugin.get(),live.pluginDescription,saved.instrument);captureState(live.effect.get(),live.effectDescription,saved.effect);
        }
    }
    auto copy=std::make_unique<AudioEngine>(Offline{});copy->sampleRate=copy->deviceRate=rate;copy->positionRate=rate;copy->blockSize=copy->deviceBlock=size;copy->busGraph.prepare(rate,size);
    auto duplicate=[&](const PluginSnapshot& saved,bool effect)->std::unique_ptr<juce::AudioPluginInstance>{juce::String error;auto next=formats.createPluginInstance(saved.description,rate,size,error);if(!next)return nullptr;if(!next->setBusesLayout(saved.layout)&&!configure(*next,effect,error))return nullptr;next->setStateInformation(saved.state.getData(),(int)saved.state.getSize());next->setPlayHead(copy.get());return next;};
    for(size_t ti=0;ti<instruments.size();++ti){const auto& saved=(*snapshots)[ti];auto& to=*copy->instruments[ti];to.mode=saved.mode;
        if(saved.sample)to.sampler=std::make_unique<SampleSynth>(saved.sample,SamplerSettings{},60,isDrumTrack((int)ti));
        if(saved.instrument.present){to.plugin=duplicate(saved.instrument,false);if(!to.plugin)return nullptr;to.pluginDescription=saved.instrument.description;copy->hostedCount.fetch_add(1);}
        if(saved.effect.present){to.effect=duplicate(saved.effect,true);if(!to.effect)return nullptr;to.effectDescription=saved.effect.description;copy->hostedCount.fetch_add(1);}
        to.prepare(rate,size);
    }return copy;
}
bool AudioEngine::startSampleAudition(int track,bool repeat){
    if(track<0||track>=maxTracks||isRecording()||isAudioRecording()||exporting)return false;
    auto asset=sampleAsset(track);if(!asset)return false;
    auto next=std::make_unique<SampleAudition>();next->track=track;
    next->synth=std::make_unique<SampleSynth>(asset,next->settings,60,isDrumTrack(track));
    next->synth->setCurrentPlaybackSampleRate(deviceRate);
    stopPreview();
    {const EngineControlLock lock(*this);if(isRecording()||exporting)return false;
        playing=false;needsReset=true;hardReset=true;auditionLoop=repeat;
        clearMeters();next->source=instruments[(size_t)track]->sampler.get();sampleAudition.swap(next);auditionTrack=track;auditionNote=-1;auditioning=true;}
    return true;
}
void AudioEngine::mixSampleAudition(float* const* output,int channels,int count){
    auto* a=sampleAudition.get();if(!a||!mixes.current||instruments[(size_t)a->track]->mode!=1){auditioning=false;return;}
    if(a->source!=instruments[(size_t)a->track]->sampler.get()){auditioning=false;return;}
    const auto& mix=mixes.current->tracks[(size_t)a->track];
    // Keep the synth's parameter pointer owned here, independent of retired mix states.
    const bool mixChanged=a->seenTempo!=mixes.current->tempo||a->seenRoot!=mix.sampleRoot||!(a->settings==mix.sampler);
    bool restart=false;
    if(mixChanged){restart=a->synth->configure(mix.sampler,mix.sampleRoot,mixes.current->tempo);a->settings=mix.sampler;a->seenTempo=mixes.current->tempo;a->seenRoot=mix.sampleRoot;a->synth->configure(a->settings,a->seenRoot,a->seenTempo);}
    const int note=a->synth->mode()==2?a->settings.sliceBase:mix.sampleRoot;
    if(note!=a->note){a->synth->allNotesOff(0,false);restart=true;}a->note=note;
    if(restart)a->started=false;
    if(a->synth->getSampleRate()!=deviceRate){auditioning=false;return;}
    for(int at=0;at<count;){
        if(!voicesActive(*a->synth)){
            if(a->started&&!auditionLoop){auditioning=false;break;}
            a->synth->noteOn(1,note,1.f);a->started=true;
            if(!voicesActive(*a->synth)){auditioning=false;break;}
        }
        auditionNote=note;const int n=std::min(64,count-at);a->buffer.clear(0,n);
        a->synth->renderNextBlock(a->buffer,a->midi,0,n);
        pitchMonitor.push(a->track,a->buffer.getReadPointer(0),a->buffer.getReadPointer(1),n,deviceRate);
        for(int ch=0;ch<std::min(2,channels);++ch)if(output[ch])
            juce::FloatVectorOperations::addWithMultiply(output[ch]+at,a->buffer.getReadPointer(ch),.8f,n);
        at+=n;
    }
    if(!auditionLoop&&!voicesActive(*a->synth))auditioning=false;
}
void AudioEngine::startPreview(juce::AudioBuffer<float>&& audio,double rate){
    stopSampleAudition();
    auto clip=std::make_unique<PreviewClip>();clip->audio=std::move(audio);clip->rate=rate>0?rate:44100.;
    previewing=clip->audio.getNumSamples()>1;previews.post(std::move(clip));
}
void AudioEngine::stopPreview(){previewing=false;previews.post(std::make_unique<PreviewClip>());}
// Audio thread: the audition plays on top of the output, stopped or playing.
void AudioEngine::mixPreview(float* const* output,int channels,int count){
    previews.adopt();auto* clip=previews.current;if(clip==nullptr)return;
    const auto& a=clip->audio;const int frames=a.getNumSamples(),sources=a.getNumChannels();if(frames<2||sources<1||clip->position>=frames-1)return;
    const double step=clip->rate/std::max(1.,deviceRate);double pos=clip->position;const float gain=.8f;
    for(int s=0;s<count;++s){const int k=(int)pos;if(k>=frames-1){pos=frames;break;}const float frac=(float)(pos-k);
        for(int ch=0;ch<std::min(2,channels);++ch)if(output[ch]){const float* d=a.getReadPointer(std::min(ch,sources-1));output[ch][s]+=gain*(d[k]+frac*(d[k+1]-d[k]));}
        pos+=step;}
    clip->position=pos;if(pos>=frames-1)previewing=false;
}
juce::AudioPluginInstance* AudioEngine::plugin(int track){return instruments[(size_t)track]->plugin.get();}
juce::AudioPluginInstance* AudioEngine::effectPlugin(int track){return instruments[(size_t)track]->effect.get();}
// State serialization/restoration is control-thread work and cannot overlap processing.
// The callback never waits on the control mutex; it emits silence during this short snapshot.
void AudioEngine::capturePluginStates(Project& p){
    if(hostedCount.load()==0||exporting.load())return;
    // getStateInformation runs under the engine lock, which interrupts the audio: reuse the
    // serialised blob until something invalidates it (load, restore, editor closed).
    // While the transport is moving, never call into the plugin: keep the last snapshot.
    if(playing.load()||pluginStateCaptured==pluginStateGeneration){
        for(size_t ti=0;ti<p.tracks.size()&&ti<cachedInstrumentStates.size();++ti){
            p.tracks[ti].instrumentState=cachedInstrumentStates[ti];
            p.tracks[ti].effectState=cachedEffectStates[ti];
        }
        return;
    }
    const EngineControlLock lock(*this);if(exporting)return;
    cachedInstrumentStates.resize(instruments.size());cachedEffectStates.resize(instruments.size());
    for(size_t ti=0;ti<instruments.size()&&ti<p.tracks.size();++ti){
        if(auto* plugin=instruments[ti]->plugin.get()){juce::MemoryBlock state;plugin->getStateInformation(state);cachedInstrumentStates[ti]=state.toBase64Encoding().toStdString();p.tracks[ti].instrumentState=cachedInstrumentStates[ti];}
        if(auto* effect=instruments[ti]->effect.get()){juce::MemoryBlock state;effect->getStateInformation(state);cachedEffectStates[ti]=state.toBase64Encoding().toStdString();p.tracks[ti].effectState=cachedEffectStates[ti];}
    }
    pluginStateCaptured=pluginStateGeneration;
}
void AudioEngine::restoreHosted(int track,const juce::String& state,bool effect){invalidatePluginStates();const EngineControlLock lock(*this);if(exporting)return;juce::MemoryBlock data;if(data.fromBase64Encoding(state))if(auto* p=effect?instruments[(size_t)track]->effect.get():instruments[(size_t)track]->plugin.get())p->setStateInformation(data.getData(),(int)data.getSize());}
void AudioEngine::restorePluginState(int track,const juce::String& state){restoreHosted(track,state,false);}
void AudioEngine::restoreEffectState(int track,const juce::String& state){restoreHosted(track,state,true);}
void AudioEngine::markMissing(int track){const EngineControlLock lock(*this);instruments[(size_t)track]->mode=3;}
juce::Result AudioEngine::exportAudio(const Project& source,const juce::File& file,int onlyTrack,bool mp3,std::function<bool()> cancelled,std::function<void(double)> progress,double* rawPeak){
    if(isAudioRecording())return juce::Result::fail("Finish audio recording before exporting.");
    if(rawPeak)*rawPeak=0;
    if(onlyTrack < -1 || onlyTrack>=maxTracks)return juce::Result::fail("Invalid export track");
    auto p=source;p.normalise();
    bool expected=false;if(!exporting.compare_exchange_strong(expected,true))return juce::Result::fail("Esportazione gia in corso.");
    stopSampleAudition();
    struct Guard{AudioEngine& e;~Guard(){const EngineControlLock lock(e);e.sampleRate=e.deviceRate;e.blockSize=e.deviceBlock;e.publishTransport();e.prepareBuses(e.sampleRate,e.blockSize);for(auto& i:e.instruments){if(i->plugin)i->plugin->setNonRealtime(false);if(i->effect)i->effect->setNonRealtime(false);i->prepare(e.sampleRate,e.blockSize);}e.playing=false;e.currentBeat=0;e.loopBypassed=false;e.publishTransport();e.needsReset=true;e.hardReset=true;e.exporting=false;}} guard{*this};
    publish(p);
    int compensation=0;
    {const EngineControlLock lock(*this);for(int ti=0;ti<maxTracks;++ti)if(instruments[(size_t)ti]->mode==3&&(onlyTrack<0||ti==onlyTrack))return juce::Result::fail("Ricollega gli strumenti mancanti prima di esportare.");
        playing=false;currentBeat=0;needsChase=false;collectGarbage();adoptState();if(mp3)sampleRate=44100;publishTransport();prepareBuses(sampleRate,blockSize);
        for(auto& i:instruments){if(i->plugin)i->plugin->setNonRealtime(true);if(i->effect)i->effect->setNonRealtime(true);i->prepare(sampleRate,blockSize);}resetAt(0);
        // Delay compensation moves every track later by the largest latency: render that much
        // more and drop it from the start, so the file lines up with the grid.
        for(auto& i:instruments)compensation=std::max(compensation,instrumentLatency(*i));
        compensation=std::clamp(compensation,0,Instrument::maxDelay-1);}
    juce::TemporaryFile temp(file);auto stream=temp.getFile().createOutputStream();if(!stream)return juce::Result::fail("Impossibile scrivere il file audio.");
    juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(),sampleRate,2,24,{},0));if(!writer)return juce::Result::fail("Encoder WAV non disponibile.");stream.release();
    // Keep identical stem lengths and retain natural sample/effect tails beyond the song.
    double endSeconds=p.length()*60./p.settings.bpm;
    for(size_t ti=0;ti<p.tracks.size();++ti){const auto& track=p.tracks[ti];auto asset=sampleAsset((int)ti);if(track.instrument!="sample"||!asset)continue;
        const auto& settings=track.sampler;const int mode=settings.mode<0?(isDrumTrack((int)ti)?1:0):settings.mode;
        for(size_t ci=0;ci<track.clips.size();++ci)for(const auto& note:track.clips[ci].notes){double from=settings.start,to=settings.end;
            if(mode==2){int slice=note.pitch-settings.sliceBase;if(slice<0||slice>=settings.sliceCount)continue;from=settings.slices[(size_t)slice];to=slice+1<settings.sliceCount?settings.slices[(size_t)slice+1]:settings.end;}
            const double pitch=(mode==2?0:note.pitch-track.sampleRoot)+settings.transpose+settings.fine*.01;
            const double speed=settings.sync?p.settings.bpm/settings.sourceBpm:std::pow(2.,pitch/12.);
            double duration=(to-from)*asset->seconds()/speed;
            if(mode==0){const double gate=note.length*60./p.settings.bpm+settings.release;duration=settings.loop?gate:std::min(duration,gate);}
            endSeconds=std::max(endSeconds,(p.clipStart((int)ti,(int)ci)+note.start)*60./p.settings.bpm+duration);
        }
    }
    double effectTail=3.;bool delayUsed=false,reverbUsed=false;for(const auto& track:p.tracks){reverbUsed=reverbUsed||track.sends[0]>0;delayUsed=delayUsed||track.sends[1]>0;}
    if(reverbUsed)effectTail=std::max(effectTail,3.+p.buses[2].amount*12.);
    if(delayUsed){const auto& delay=p.buses[3];const double repeats=delay.feedback>0?std::ceil(std::log(.00003)/std::log(std::max(.00001,delay.feedback)))+1:1;effectTail=std::max(effectTail,repeats*delay.division*60./p.settings.bpm);}
    juce::AudioBuffer<float> buffer(2,blockSize);const auto total=(juce::int64)std::ceil((endSeconds+effectTail)*sampleRate);
    const juce::int64 renderTotal=total+compensation;
    for(juce::int64 at=0;at<renderTotal;){if(cancelled())return juce::Result::fail("Esportazione annullata.");int n=(int)std::min<juce::int64>(blockSize,renderTotal-at);buffer.clear();
        const int skip=(int)std::clamp<juce::int64>(compensation-at,0,n);
        {const EngineControlLock lock(*this);currentBeat=at*p.settings.bpm/(sampleRate*60.);render(buffer.getArrayOfWritePointers(),2,n,currentBeat,onlyTrack);}
        for(int ch=0;ch<2;++ch)for(int sample=0;sample<n;++sample){
            const float value=buffer.getSample(ch,sample);
            if(!std::isfinite(value))return juce::Result::fail("Audio non finito: controlla lo strumento e i livelli.");
            if(rawPeak)*rawPeak=std::max(*rawPeak,(double)std::abs(value));
            buffer.setSample(ch,sample,std::clamp(value,-.999f,.999f));
        }
        if(n>skip&&!writer->writeFromAudioSampleBuffer(buffer,skip,n-skip))return juce::Result::fail("Scrittura WAV fallita (spazio disco?).");at+=n;progress((double)at/(double)renderTotal*(mp3?.8:1.));
    }writer.reset();
    if(!mp3)return temp.overwriteTargetFileWithTemporary()?juce::Result::ok():juce::Result::fail("Impossibile completare il WAV.");
    auto wavStream=temp.getFile().createInputStream();if(!wavStream)return juce::Result::fail("WAV temporaneo non leggibile.");
    std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(wavStream.release(),true));if(!reader)return juce::Result::fail("WAV temporaneo non valido.");
    Mp3Encoder encoder;if(!encoder.valid())return juce::Result::fail("Encoder MP3 non disponibile.");
    juce::TemporaryFile encoded(file);auto out=encoded.getFile().createOutputStream();if(!out)return juce::Result::fail("Impossibile scrivere MP3.");
    const int frame=encoder.frameSize();juce::AudioBuffer<float> audio(2,frame);std::vector<std::int16_t> pcm((size_t)frame*2);
    // Gapless: an Info/LAME frame first (patched at the end) tells decoders to skip the encoder
    // delay and padding; enough silence is encoded after the song that the decoder's own delay
    // never cuts the tail. The MP3 then lines up sample-exactly with the WAV.
    if(!out->write(Mp3Encoder::infoFrame(0,0,0).data(),(size_t)Mp3Encoder::infoFrameBytes))return juce::Result::fail("Scrittura MP3 fallita.");
    std::uint32_t frames=0;const juce::int64 encodeTotal=total+Mp3Encoder::encoderDelay+Mp3Encoder::decoderDelay;
    for(juce::int64 at=0;at<encodeTotal;at+=frame){++frames;if(cancelled())return juce::Result::fail("Esportazione annullata.");audio.clear();if(at<total)reader->read(&audio,0,(int)std::min<juce::int64>(frame,total-at),at,true,true);
        for(int i=0;i<frame;++i)for(int ch=0;ch<2;++ch)pcm[(size_t)i*2+(size_t)ch]=(std::int16_t)std::lround(std::clamp(audio.getSample(ch,i),-.999f,.999f)*32767.f);
        auto bytes=encoder.encode(pcm.data());if(!out->write(bytes.data(),bytes.size()))return juce::Result::fail("Scrittura MP3 fallita.");progress(.8+.2*std::min(1.,(double)(at+frame)/(double)encodeTotal));
    }auto bytes=encoder.flush();if(!out->write(bytes.data(),bytes.size()))return juce::Result::fail("Scrittura MP3 fallita.");
    const auto info=Mp3Encoder::infoFrame(frames,(std::uint32_t)out->getPosition(),total);
    if(!out->setPosition(0)||!out->write(info.data(),info.size()))return juce::Result::fail("Scrittura MP3 fallita.");out->flush();if(out->getStatus().failed())return juce::Result::fail("Scrittura MP3 fallita.");out.reset();
    return encoded.overwriteTargetFileWithTemporary()?juce::Result::ok():juce::Result::fail("Impossibile completare MP3.");
}
}
