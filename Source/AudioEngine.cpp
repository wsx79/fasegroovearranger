// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AudioEngine.h"
#include "core/Mp3Encoder.h"
#include <cmath>
#include <cstring>
#include <optional>
namespace fga {
namespace {
const std::vector<TimedEvent> noEvents;
void mixTargets(Instrument& i,double master,bool snap){
    const float left=(float)(i.gain*master*std::min(1.,1.-i.pan)),right=(float)(i.gain*master*std::min(1.,1.+i.pan));
    if(snap){i.leftGain.setCurrentAndTargetValue(left);i.rightGain.setCurrentAndTargetValue(right);i.baseGain.setCurrentAndTargetValue((float)(i.gain*master));}
    else{i.leftGain.setTargetValue(left);i.rightGain.setTargetValue(right);i.baseGain.setTargetValue((float)(i.gain*master));}
}
std::unique_ptr<MixState> mixState(const Project& p){
    auto m=std::make_unique<MixState>();m->master=p.masterGain;m->tempo=p.settings.bpm;m->masterAutomation=p.masterAutomation;
    for(size_t ti=0;ti<(size_t)trackCount&&ti<p.tracks.size();++ti){const auto& t=p.tracks[ti];auto& x=m->tracks[ti];
        x.volume=t.volume;x.panAutomation=t.panAutomation;x.parameters=t.parameters;x.gain=t.gain;x.pan=t.pan;x.mute=t.mute;x.solo=t.solo;x.sound=t.sound;x.tone=t.engine.tone;x.reverb=t.engine.reverb;x.chorus=t.engine.chorus;
        x.pump=t.engine.pump;x.pumpRelease=t.engine.pumpRelease;x.pumpRate=std::clamp(t.engine.pumpRate,0,3);x.pumpSource=std::clamp(t.engine.pumpSource,0,1);
        x.drumNote=ti>=4?(t.engine.midiNote>=0?t.engine.midiNote:fgd::defaultNote((int)ti-4)):60;
        for(size_t lane=0;lane<x.laneVolume.size();++lane){x.laneVolume[lane]=(float)t.engine.laneVolume[lane];x.laneMute[lane]=t.engine.laneMute[lane];}}
    return m;
}
std::uint64_t hashEvents(const std::vector<TimedEvent>& events){
    std::uint64_t h=1469598103934665603ull;auto add=[&h](std::uint64_t v){h=(h^v)*1099511628211ull;};
    for(const auto& e:events){std::uint64_t bits;std::memcpy(&bits,&e.beat,sizeof bits);add(bits);add((std::uint64_t)(std::uint32_t)e.pitch|((std::uint64_t)(std::uint32_t)e.velocity<<32));add((std::uint64_t)(std::uint32_t)e.lane|((std::uint64_t)(std::uint32_t)e.controller<<32));}
    return h^events.size();
}
const std::vector<TimedEvent>& eventsOf(const Instrument& i){return i.schedule?*i.schedule:noEvents;}
bool laneMuted(const Instrument& i,const TimedEvent& e){return i.kind==3&&e.controller<0&&i.guitarMuted[(size_t)std::clamp(e.lane,0,fgg::numLanes-1)];}
size_t cursorAt(const Instrument& i,double beat){const auto& s=eventsOf(i);return (size_t)(std::lower_bound(s.begin(),s.end(),beat,[](const TimedEvent& e,double b){return e.beat<b;})-s.begin());}
// What the schedule holds at the cursor: the pitches sounding (held or sustained), their
// velocity, lane and start beat, and the last value of each controller.
struct Hold{std::array<int,128> vel{},lane{},cc;std::array<double,128> start{};std::array<bool,128> pressed{};};
Hold holdState(const Instrument& i){Hold h;h.cc.fill(-1);bool sustain=false;const auto& s=eventsOf(i);
    for(size_t k=0;k<i.cursor&&k<s.size();++k){const auto& e=s[k];if(laneMuted(i,e))continue;
        if(e.controller>=0){h.cc[(size_t)std::clamp(e.controller,0,127)]=e.velocity;if(e.controller==64){sustain=e.velocity>=64;if(!sustain)for(size_t n=0;n<128;++n)if(!h.pressed[n])h.vel[n]=0;}}
        else{auto n=(size_t)std::clamp(e.pitch,0,127);h.pressed[n]=e.velocity>0;if(e.velocity>0){h.vel[n]=e.velocity;h.start[n]=e.beat;}else if(!sustain)h.vel[n]=0;h.lane[n]=e.lane;}}
    return h;}
void noteOn(Instrument& i,int n,int velocity,int lane,double start){i.midi.addEvent(juce::MidiMessage::noteOn(1,n,(juce::uint8)velocity),0);if(i.kind==3)i.guitarEvents.push_back({0,n,velocity/127.f,lane});i.sounding[(size_t)n]=true;i.soundingStart[(size_t)n]=start;i.soundingVelocity[(size_t)n]=velocity;++i.noteOns;}
void noteOff(Instrument& i,int n,int lane){i.midi.addEvent(juce::MidiMessage::noteOff(1,n),0);if(i.kind==3)i.guitarEvents.push_back({0,n,0.f,lane});i.sounding[(size_t)n]=false;}
}

Instrument::Instrument(int track):kind(track){
    for(int i=0;i<fgd::numLanes;++i)drumNotes[(size_t)i]=fgd::defaultNote(i);
    delayLine.clear();drumLevels.fill(1);guitarLevels.fill(1);guitarEvents.reserve(8192);midi.ensureSize(262144);effectMidi.ensureSize(262144);
    if(kind<3){for(int i=0;i<48;++i)synth.addVoice(new GuideVoice(tone,sound,kind==1));if(kind==2)synth.addSound(new GuideStringsTag(1));else synth.addSound(new GuideSoundTag);}
}
void Instrument::prepare(double rate,int block){
    leftGain.reset(rate,.01);rightGain.reset(rate,.01);baseGain.reset(rate,.01);parameterSent.fill(-1.f);
    synth.setCurrentPlaybackSampleRate(rate);if(sampler)sampler->setCurrentPlaybackSampleRate(rate);if(kind>=4)drums.prepare(rate);if(kind==3)guitar.prepare(rate);
    auto preparePlugin=[&](juce::AudioPluginInstance* p){if(!p)return;p->releaseResources();p->setRateAndBufferSizeDetails(rate,block);p->prepareToPlay(rate,block);};
    preparePlugin(plugin.get());preparePlugin(effect.get());
    volumeGains.resize((size_t)block);panLeft.resize((size_t)block);panRight.resize((size_t)block);adoptChannels(block);bassLowPass.prepare({rate,(juce::uint32)block,2});bassLowPass.setType(juce::dsp::StateVariableTPTFilterType::lowpass);bassLowPass.setCutoffFrequency((float)(100+1400*tone.load()));reverb.setSampleRate(rate);chorus.prepare({rate,(juce::uint32)block,2});reset();
}
void Instrument::adoptChannels(int block){
    int channels=2;for(auto* p:{plugin.get(),effect.get()})if(p)channels=std::max({channels,p->getTotalNumOutputChannels(),p->getTotalNumInputChannels()});
    outputs=mode==2&&plugin?std::max(1,plugin->getTotalNumOutputChannels()):2;scratch.setSize(channels,block);
}
void Instrument::applyDelay(juce::AudioBuffer<float>& block,int count){
    if(delay<=0)return;
    delayUsed=true;auto* left=delayLine.getWritePointer(0);auto* right=delayLine.getWritePointer(1);auto* inL=block.getWritePointer(0);auto* inR=block.getWritePointer(1);int w=delayWrite;
    for(int s=0;s<count;++s){const int r=(w-delay)&(maxDelay-1);left[w]=inL[s];right[w]=inR[s];inL[s]=left[r];inR[s]=right[r];w=(w+1)&(maxDelay-1);}
    delayWrite=w;
}
void Instrument::reset(){if(delayUsed){delayLine.clear();delayUsed=false;delayWrite=0;}if(sampler)sampler->allNotesOff(0,false);bassLowPass.reset();reverb.reset();chorus.reset();synth.allNotesOff(0,false);drums.allNotesOff();guitar.allNotesOff();if(plugin)plugin->reset();if(effect)effect->reset();midi.clear();effectMidi.clear();guitarEvents.clear();}
AudioEngine::AudioEngine(){
    formats.addDefaultFormats();for(int i=0;i<trackCount;++i){instruments[(size_t)i]=std::make_unique<Instrument>(i);instruments[(size_t)i]->prepare(sampleRate,blockSize);meters[(size_t)i]=0;}
    devices.initialise(0,2,nullptr,true);devices.addAudioCallback(this);
}
AudioEngine::AudioEngine(Offline){for(int i=0;i<trackCount;++i){instruments[(size_t)i]=std::make_unique<Instrument>(i);meters[(size_t)i]=0;}}
AudioEngine::~AudioEngine(){lifetime.reset();devices.removeAudioCallback(this);const juce::ScopedLock lock(mutex);for(auto& i:instruments){if(i->plugin)i->plugin->releaseResources();if(i->effect)i->effect->releaseResources();}}
void AudioEngine::publish(const Project& source,bool normalised,int onlyTrack){
    std::optional<Project> canonical;if(!normalised){canonical=source;canonical->normalise();}const auto& p=canonical?*canonical:source;
    auto next=std::make_unique<ScheduleState>();const double songLength=p.length();
    {
        // The edited project's last schedules are kept here: when one track changed (same song
        // length) only that track's events are rebuilt. Other publishers (export, tests) drop them.
        const juce::ScopedLock cacheLock(publishedLock);
        const bool reuse=normalised&&publishedValid&&onlyTrack>=0&&onlyTrack<trackCount&&songLength==published.length;
        if(reuse){*next=published;next->schedules[(size_t)onlyTrack].clear();for(auto& e:events(p,onlyTrack))next->schedules[(size_t)onlyTrack].push_back(e);next->hashes[(size_t)onlyTrack]=hashEvents(next->schedules[(size_t)onlyTrack]);}
        else{for(auto& e:events(p))if(e.track>=0&&e.track<trackCount)next->schedules[(size_t)e.track].push_back(e);
            for(size_t ti=0;ti<(size_t)trackCount;++ti)next->hashes[ti]=hashEvents(next->schedules[ti]);}
        publishedValid=normalised;next->length=songLength;if(normalised)published=*next;
    }
    next->length=songLength;next->looping=p.loop;next->loopFrom=p.loopStart;next->loopTo=p.loopEnd;
    tempo=p.settings.bpm;
    mixes.post(mixState(p));schedules.post(std::move(next));adoptWhileStopped();
}
void AudioEngine::updateMix(const Project& p){tempo=p.settings.bpm;mixes.post(mixState(p));adoptWhileStopped();}
// Stopped, the output is silent anyway: take the new state at once, so transport calls made
// right after an edit (seek, loop, length) already see it, with or without a running device.
void AudioEngine::adoptWhileStopped(){if(playing.load())return;const juce::ScopedLock lock(mutex);if(playing.load())return;collectGarbage();adoptState();collectGarbage();}
void AudioEngine::applyMix(const MixState& m,bool snap){
    master=m.master;masterAutomation=&m.masterAutomation;
    for(size_t ti=0;ti<instruments.size();++ti){auto& i=*instruments[ti];const auto& t=m.tracks[ti];
        if(i.parameters!=&t.parameters)i.parameterSent.fill(-1.f);   // other lanes: set every parameter again
        i.volume=&t.volume;i.panAutomation=&t.panAutomation;i.parameters=&t.parameters;i.gain=t.gain;i.pan=t.pan;mixTargets(i,master,snap);i.mute=t.mute;i.solo=t.solo;i.sound=(float)t.sound;i.tone=(float)t.tone;
        i.bassLowPass.setCutoffFrequency((float)(100+1400*t.tone));i.drums.setKit(t.sound);i.drums.setTone((float)t.tone);i.guitar.setSound(t.sound);i.guitar.setTone((float)t.tone);
        for(size_t lane=0;lane<i.guitarLevels.size();++lane){i.guitarLevels[lane]=t.laneVolume[lane];if(i.guitarMuted[lane]!=t.laneMute[lane]){i.guitarMuted[lane]=t.laneMute[lane];if(playing){i.resync=true;needsResync=true;}}}
        juce::Reverb::Parameters rp;rp.roomSize=.4f;rp.wetLevel=(float)t.reverb*.4f;rp.dryLevel=.5f;rp.width=1;i.reverb.setParameters(rp);i.chorus.setMix((float)t.chorus*.5f);i.chorus.setRate(.35f);i.chorus.setDepth(.25f);
        i.pump=t.pump;i.pumpRelease=t.pumpRelease;i.pumpRate=t.pumpRate;i.pumpSource=t.pumpSource;if(ti>=4)i.drumNotes[ti-4]=t.drumNote;}
}
// Audio side (or export, under the lock): take the newest published state, if any.
void AudioEngine::adoptState(){
    if(auto* m=mixes.adopt())applyMix(*m,!playing.load());
    // The replaced state is parked for the message thread to free: read its hashes before.
    const bool hadOld=schedules.current!=nullptr;std::array<std::uint64_t,trackCount> oldHashes{};if(hadOld)oldHashes=schedules.current->hashes;
    if(auto* s=schedules.adopt()){
        length=s->length;looping=s->looping;loopFrom=std::clamp(s->loopFrom,0.,length);loopTo=std::clamp(s->loopTo,loopFrom,length);
        currentBeat=std::min(currentBeat.load(),length);loopBypassed=currentBeat>=loopTo;
        for(size_t ti=0;ti<instruments.size();++ti){auto& i=*instruments[ti];i.schedule=&s->schedules[ti];
            if(hadOld&&oldHashes[ti]==s->hashes[ti])continue; // same events: the cursor still points at the same place
            i.cursor=cursorAt(i,currentBeat);if(playing){i.resync=true;needsResync=true;}}
    }
}
void AudioEngine::play(bool enabled){const juce::ScopedLock lock(mutex);if(enabled&&currentBeat>=length){currentBeat=looping?loopFrom:0;loopBypassed=false;}if(enabled!=playing.load())hardReset=true;playing=enabled;if(!enabled)clearMeters();needsReset=true;needsChase=enabled;}
void AudioEngine::stop(){play(false);seek(0);}
// An explicit seek beyond the loop must remain audible there until the song ends.
void AudioEngine::seek(double beat){const juce::ScopedLock lock(mutex);currentBeat=std::clamp(beat,0.0,length);loopBypassed=currentBeat>=loopTo;needsReset=true;needsChase=playing;}
void AudioEngine::setLoop(bool enabled,double from,double to){const juce::ScopedLock lock(mutex);looping=enabled;loopFrom=std::clamp(from,0.0,length);loopTo=std::clamp(to,loopFrom,length);loopBypassed=currentBeat>=loopTo;}
void AudioEngine::resetAt(double beat){
    // Stopped or starting: a full reset (silent anyway). Playing (loop wrap, seek): release the
    // sounding notes with their natural release and keep effect tails, so the jump is click-free.
    const bool hard=!playing||hardReset;
    for(auto& ptr:instruments){auto& i=*ptr;if(hard)i.reset();i.resync=false;i.cursor=cursorAt(i,beat);
        // Explicit note-offs first: many plugins ignore All Notes Off (CC 123).
        for(int n=0;n<128;++n)if(i.sounding[(size_t)n])noteOff(i,n,0);
        i.midi.addEvent(juce::MidiMessage::controllerEvent(1,64,0),0);i.midi.addEvent(juce::MidiMessage::controllerEvent(1,11,127),0);
        if(hard)i.midi.addEvent(juce::MidiMessage::allNotesOff(1),0);
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
        i.midi.clear();i.guitarEvents.clear();}
}
void AudioEngine::click(float* const* output,int channels,int count,double beat){
    if(!metronome.load()){clickLeft=0;return;}
    const double samplesPerBeat=sampleRate*60./tempo.load();double next=std::ceil(beat-1e-9);
    auto triggerAt=[&]{return (int)std::llround((next-beat)*samplesPerBeat);};int trigger=triggerAt();
    for(int s=0;s<count;++s){
        if(s==trigger){clickLeft=(int)(sampleRate*.03);clickPos=0;clickHz=((long long)next%4==0)?1760.:1100.;next+=1;trigger=triggerAt();}
        if(clickLeft<=0)continue;
        const float v=(float)(.22*std::sin(juce::MathConstants<double>::twoPi*clickHz*clickPos/sampleRate)*std::exp(-clickPos/(sampleRate*.007)));
        for(int ch=0;ch<channels;++ch)if(output[ch])output[ch][s]+=v;
        ++clickPos;--clickLeft;}
}
// Hosted plugin parameters follow their lanes once per rendered block (the device block, or
// smaller at a loop end): set only when the value moves, before the plugin processes the block.
void AudioEngine::applyParameterAutomation(Instrument& i,double beat){
    if(i.parameters==nullptr)return;
    const size_t lanes=std::min(i.parameters->size(),(size_t)maxParameterLanes);
    for(size_t k=0;k<lanes;++k){const auto& lane=(*i.parameters)[k];if(!lane.curve.active()||lane.index<0)continue;
        auto* target=lane.slot==0?(i.mode==2?i.plugin.get():nullptr):i.effect.get();if(target==nullptr)continue;
        const auto& params=target->getParameters();if(lane.index>=params.size())continue;
        const float value=(float)std::clamp(lane.curve.valueAt(beat),0.,1.);
        if(std::abs(value-i.parameterSent[k])<1.e-5f)continue;
        i.parameterSent[k]=value;params[lane.index]->setValue(value);}
}
void AudioEngine::render(float* const* output,int channels,int count,double beat,int onlyTrack){
    const double samplesPerBeat=sampleRate*60./tempo.load(),end=beat+count/samplesPerBeat;
    bool anySolo=false;for(auto& i:instruments)anySolo=anySolo||i->solo;
    // Plugin delay compensation: instrument + insert latency, every other track delayed to match.
    int maxLatency=0;for(auto& ptr:instruments){auto& i=*ptr;i.latency=(i.mode==2&&i.plugin?std::max(0,i.plugin->getLatencySamples()):0)+(i.effect?std::max(0,i.effect->getLatencySamples()):0);maxLatency=std::max(maxLatency,i.latency);}
    maxLatency=std::min(maxLatency,Instrument::maxDelay-1);
    for(auto& ptr:instruments)ptr->delay=std::clamp(maxLatency-ptr->latency,0,Instrument::maxDelay-1);
    // Master automation: one gain per sample for every track (the buffer is allocated once).
    const bool masterActive=masterAutomation&&masterAutomation->active()&&count<=(int)masterGains.size();
    if(masterActive)for(int s=0;s<count;++s)masterGains[(size_t)s]=(float)masterAutomation->valueAt(beat+s/samplesPerBeat);
    for(int ti=0;ti<trackCount;++ti){auto& i=*instruments[(size_t)ti];const auto& schedule=eventsOf(i);
        while(i.cursor<schedule.size()&&schedule[i.cursor].beat<end){const auto& e=schedule[i.cursor++];if(e.beat<beat-1e-8||laneMuted(i,e))continue;int at=std::clamp((int)std::llround((e.beat-beat)*samplesPerBeat),0,count-1);
            i.midi.addEvent(e.controller>=0?juce::MidiMessage::controllerEvent(1,e.controller,e.velocity):e.velocity?juce::MidiMessage::noteOn(1,e.pitch,(juce::uint8)e.velocity):juce::MidiMessage::noteOff(1,e.pitch),at);
            if(e.controller<0){const auto k=(size_t)std::clamp(e.pitch,0,127);i.sounding[k]=e.velocity>0;if(e.velocity>0){i.soundingStart[k]=e.beat;i.soundingVelocity[k]=e.velocity;++i.noteOns;}}
            if(ti==3&&e.controller<0)i.guitarEvents.push_back({at,e.pitch,e.velocity/127.f,e.lane});
        }
        // Resized in place (never reallocated: prepare() sized it for the whole block), even with >32 plugin channels.
        i.scratch.setSize(i.scratch.getNumChannels(),count,false,false,true);auto& block=i.scratch;block.clear();
        if(i.effect){i.effectMidi.clear();i.effectMidi.addEvents(i.midi,0,count,0);}
        applyParameterAutomation(i,beat+.5*count/samplesPerBeat);
        if(i.mode==2&&i.plugin)i.plugin->processBlock(block,i.midi);
        else if(i.mode==1){if(i.sampler)i.sampler->renderNextBlock(block,i.midi,0,count);}
        else if(i.mode==0){if(ti<3)i.synth.renderNextBlock(block,i.midi,0,count);else if(ti==3)i.guitar.render(block,i.guitarEvents,i.guitarLevels);else i.drums.render(block,i.midi,i.drumNotes,i.drumLevels,i.drumSamples);}
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
        if(i.effect)i.effect->processBlock(block,i.effectMidi);
        i.applyDelay(block,count);
        i.midi.clear();i.guitarEvents.clear();
        const bool audible=onlyTrack>=0?ti==onlyTrack:(!i.mute&&(!anySolo||i.solo));
        float meter=0,side[2]{};
        if(audible){if(i.volume&&i.volume->enabled&&!i.volume->points.empty())for(int s=0;s<count;++s)i.volumeGains[(size_t)s]=(float)i.volume->valueAt(beat+s/samplesPerBeat);else std::fill(i.volumeGains.begin(),i.volumeGains.begin()+count,1.f);
            if(masterActive)for(int s=0;s<count;++s)i.volumeGains[(size_t)s]*=masterGains[(size_t)s];
            // Pan automation: its own pan law per sample (as the static pan), on gain * master.
            const bool panActive=i.panAutomation&&i.panAutomation->active();
            if(panActive){for(int s=0;s<count;++s){const double pan=panFromAutomation(i.panAutomation->valueAt(beat+s/samplesPerBeat));const float base=i.baseGain.getNextValue();i.panLeft[(size_t)s]=base*(float)std::min(1.,1.-pan);i.panRight[(size_t)s]=base*(float)std::min(1.,1.+pan);}}
            else i.baseGain.skip(count);
            for(int ch=0;ch<std::min(2,channels);++ch){if(!output[ch]){(ch==0?i.leftGain:i.rightGain).skip(count);continue;}auto* in=block.getReadPointer(std::min(ch,block.getNumChannels()-1));auto& gain=ch==0?i.leftGain:i.rightGain;
                if(panActive){gain.skip(count);const auto& pg=ch==0?i.panLeft:i.panRight;for(int s=0;s<count;++s){const float v=in[s]*pg[(size_t)s]*i.volumeGains[(size_t)s];output[ch][s]+=v;side[ch]=std::max(side[ch],std::abs(v));}meter=std::max(meter,side[ch]);continue;}
                for(int s=0;s<count;++s){const float v=in[s]*gain.getNextValue()*i.volumeGains[(size_t)s];output[ch][s]+=v;side[ch]=std::max(side[ch],std::abs(v));}meter=std::max(meter,side[ch]);}
        }else{i.leftGain.skip(count);i.rightGain.skip(count);i.baseGain.skip(count);}
        meters[(size_t)ti]=meter;sideMeters[(size_t)ti][0]=side[0];sideMeters[(size_t)ti][1]=side[1];
    }
}
void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* d){const juce::ScopedLock lock(mutex);deviceRate=d->getCurrentSampleRate()>0?d->getCurrentSampleRate():44100;deviceBlock=std::max(16,d->getCurrentBufferSizeSamples());if(exporting)return;sampleRate=deviceRate;blockSize=deviceBlock;for(auto& i:instruments){i->prepare(sampleRate,blockSize);i->sounding.fill(false);}needsReset=true;needsChase=playing;hardReset=true;}
void AudioEngine::audioDeviceStopped(){const juce::ScopedLock lock(mutex);if(exporting)return;for(auto& i:instruments){i->reset();i->sounding.fill(false);}}
void AudioEngine::audioDeviceIOCallbackWithContext(const float* const*,int,float* const* output,int channels,int count,const juce::AudioIODeviceCallbackContext&){
    for(int ch=0;ch<channels;++ch)if(output[ch])juce::FloatVectorOperations::clear(output[ch],count);
    mixPreview(output,channels,count);
    if(exporting)return;
    const juce::ScopedTryLock lock(mutex);if(!lock.isLocked()||exporting)return;
    adoptState();
    if(needsReset)resetAt(currentBeat);else if(needsResync&&playing)resyncAt(currentBeat);
    if(!playing){flushIdle(count);clearMeters();return;}
    int done=0;
    while(done<count&&playing){double beat=currentBeat;const bool hasLoop=looping&&!loopBypassed&&loopTo-loopFrom>=1./960.;double boundary=hasLoop?loopTo:length;
        if(beat>=boundary-1e-9){if(hasLoop){currentBeat=loopFrom;needsChase=true;resetAt(loopFrom);continue;}playing=false;needsReset=true;resetAt(beat);flushIdle(count-done);break;}
        const int remain=std::max(1,(int)std::ceil((boundary-beat)*sampleRate*60./tempo.load()));int n=std::min({count-done,blockSize,remain});
        float* pointers[2]{};int used=std::min(2,channels);for(int ch=0;ch<used;++ch)pointers[ch]=output[ch]?output[ch]+done:nullptr;
        render(pointers,used,n,beat);click(pointers,used,n,beat);currentBeat=beat+n*tempo.load()/(sampleRate*60.);done+=n;
    }
    float outputPeak=0,sides[2]{};for(int ch=0;ch<channels;++ch)if(output[ch])for(int s=0;s<count;++s){if(!std::isfinite(output[ch][s])){output[ch][s]=0;masterClip=true;}const float v=std::abs(output[ch][s]);outputPeak=std::max(outputPeak,v);sides[std::min(ch,1)]=std::max(sides[std::min(ch,1)],v);}
    masterMeter=outputPeak;masterSides[0]=sides[0];masterSides[1]=channels>1?sides[1]:sides[0];if(outputPeak>=1)masterClip=true;
    // Final safety ceiling; export reports and preserves the floating-point mix before encoding.
    for(int ch=0;ch<channels;++ch)if(output[ch])for(int s=0;s<count;++s)output[ch][s]=std::clamp(output[ch][s],-.999f,.999f);
}
juce::Optional<juce::AudioPlayHead::PositionInfo> AudioEngine::getPosition() const{
    PositionInfo p;const double beat=currentBeat.load(),bpm=tempo.load();p.setBpm(bpm);p.setPpqPosition(beat);p.setTimeInSeconds(beat*60./bpm);p.setTimeInSamples((juce::int64)std::llround(beat*60./bpm*sampleRate));
    p.setIsPlaying(playing.load()||exporting.load());p.setTimeSignature(juce::AudioPlayHead::TimeSignature{4,4});
    // Bar start and loop let tempo-synced plugins (arpeggiators, LFOs, delays) lock to the bar.
    p.setPpqPositionOfLastBarStart(std::floor(beat/4.)*4.);p.setBarCount((juce::int64)std::floor(beat/4.));
    p.setIsLooping(looping&&!exporting.load());if(looping)p.setLoopPoints(juce::AudioPlayHead::LoopPoints{loopFrom,loopTo});
    return p;
}
namespace {
// A track changes instrument while the song may be playing: forget what the old one was holding
// and let resync strike the held notes on the new one. Only this track is touched.
void restrike(Instrument& i){i.synth.allNotesOff(0,false);i.drums.allNotesOff();i.guitar.allNotesOff();i.sounding.fill(false);i.midi.clear();i.effectMidi.clear();i.guitarEvents.clear();i.resync=true;}
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
void AudioEngine::useInternal(int track){
    std::unique_ptr<juce::AudioPluginInstance> oldPlugin;std::unique_ptr<juce::Synthesiser> oldSampler;
    {const juce::ScopedLock lock(mutex);auto& i=*instruments[(size_t)track];if(i.mode==0&&!i.plugin&&!i.sampler)return;
        oldPlugin=std::move(i.plugin);oldSampler=std::move(i.sampler);i.mode=0;i.adoptChannels(blockSize);restrike(i);needsResync=true;}
    if(oldPlugin)oldPlugin->releaseResources();
}
juce::Result AudioEngine::loadSample(int track,const juce::File& file,int root){
    juce::AudioFormatManager fm;fm.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(file));
    if(!r||r->lengthInSamples<=0)return juce::Result::fail("Campione audio non leggibile.");
    if(r->lengthInSamples>r->sampleRate*60)return juce::Result::fail("Usare campioni fino a 60 secondi.");
    juce::BigInteger range;range.setRange(0,128,true);
    auto next=std::make_unique<juce::Synthesiser>();for(int v=0;v<32;++v)next->addVoice(new juce::SamplerVoice);
    next->addSound(new juce::SamplerSound(file.getFileName(),*r,range,std::clamp(root,0,127),.002,.1,60.));
    double rate=0;{const juce::ScopedLock lock(mutex);rate=sampleRate;}next->setCurrentPlaybackSampleRate(rate);
    std::unique_ptr<juce::AudioPluginInstance> oldPlugin;std::unique_ptr<juce::Synthesiser> oldSampler;
    {const juce::ScopedLock lock(mutex);auto& i=*instruments[(size_t)track];if(rate!=sampleRate)next->setCurrentPlaybackSampleRate(sampleRate);
        oldPlugin=std::move(i.plugin);oldSampler=std::move(i.sampler);i.sampler=std::move(next);i.mode=1;i.adoptChannels(blockSize);restrike(i);needsResync=true;}
    if(oldPlugin)oldPlugin->releaseResources();
    return juce::Result::ok();
}
void AudioEngine::swapInstrument(int track,std::unique_ptr<juce::AudioPluginInstance> next,bool effect,const juce::PluginDescription& description){
    double rate=0;int block=0;{const juce::ScopedLock lock(mutex);rate=sampleRate;block=blockSize;}
    next->setPlayHead(this);next->setRateAndBufferSizeDetails(rate,block);next->prepareToPlay(rate,block);
    std::unique_ptr<juce::AudioPluginInstance> old;std::unique_ptr<juce::Synthesiser> oldSampler;
    {const juce::ScopedLock lock(mutex);
        if(rate!=sampleRate||block!=blockSize){next->releaseResources();next->setRateAndBufferSizeDetails(sampleRate,blockSize);next->prepareToPlay(sampleRate,blockSize);}
        auto& i=*instruments[(size_t)track];
        if(effect){old=std::move(i.effect);i.effect=std::move(next);i.effectDescription=description;i.effectMidi.clear();}
        else{old=std::move(i.plugin);oldSampler=std::move(i.sampler);i.plugin=std::move(next);i.pluginDescription=description;i.mode=2;restrike(i);needsResync=true;}
        i.adoptChannels(blockSize);}
    if(old)old->releaseResources();
}
void AudioEngine::loadPlugin(int track,const juce::PluginDescription& description,std::function<void(juce::Result)> callback){
    formats.createPluginInstanceAsync(description,sampleRate,blockSize,[this,track,callback,description,weak=std::weak_ptr<int>(lifetime)](std::unique_ptr<juce::AudioPluginInstance> next,const juce::String& error){
        if(weak.expired())return;
        if(!next){callback(juce::Result::fail(error));return;}
        juce::String problem;if(!configure(*next,false,problem)){callback(juce::Result::fail(problem));return;}
        swapInstrument(track,std::move(next),false,description);callback(juce::Result::ok());
    });
}
void AudioEngine::loadEffect(int track,const juce::PluginDescription& description,std::function<void(juce::Result)> callback){
    formats.createPluginInstanceAsync(description,sampleRate,blockSize,[this,track,callback,description,weak=std::weak_ptr<int>(lifetime)](std::unique_ptr<juce::AudioPluginInstance> next,const juce::String& error){
        if(weak.expired())return;
        if(!next){callback(juce::Result::fail(error));return;}
        juce::String problem;if(!configure(*next,true,problem)){callback(juce::Result::fail(problem));return;}
        swapInstrument(track,std::move(next),true,description);callback(juce::Result::ok());
    });
}
void AudioEngine::clearEffect(int track){std::unique_ptr<juce::AudioPluginInstance> old;{const juce::ScopedLock lock(mutex);auto& i=*instruments[(size_t)track];if(!i.effect)return;old=std::move(i.effect);i.effectMidi.clear();i.adoptChannels(blockSize);}old->releaseResources();}
std::unique_ptr<AudioEngine> AudioEngine::offlineCopy(){
    if(exporting.load())return nullptr;
    auto copy=std::make_unique<AudioEngine>(Offline{});
    copy->sampleRate=copy->deviceRate=deviceRate;copy->blockSize=copy->deviceBlock=deviceBlock;
    // Plugins are re-created from their description and state (message thread, like any load).
    auto duplicate=[this,&copy](juce::AudioPluginInstance& live,const juce::PluginDescription& description,bool effect)->std::unique_ptr<juce::AudioPluginInstance>{
        juce::String error;auto next=formats.createPluginInstance(description,copy->sampleRate,copy->blockSize,error);
        if(!next)return nullptr;
        if(!next->setBusesLayout(live.getBusesLayout())&&!configure(*next,effect,error))return nullptr;
        juce::MemoryBlock state;live.getStateInformation(state);next->setStateInformation(state.getData(),(int)state.getSize());next->setPlayHead(copy.get());return next;};
    for(size_t ti=0;ti<instruments.size();++ti){auto& from=*instruments[ti];auto& to=*copy->instruments[ti];
        if(from.mode==3)return nullptr;
        to.mode=from.mode;
        if(from.mode==1&&from.sampler){auto sampler=std::make_unique<juce::Synthesiser>();for(int v=0;v<32;++v)sampler->addVoice(new juce::SamplerVoice);
            for(int k=0;k<from.sampler->getNumSounds();++k)sampler->addSound(from.sampler->getSound(k)); // sample data is shared, read-only
            to.sampler=std::move(sampler);}
        if(from.mode==2&&from.plugin){to.plugin=duplicate(*from.plugin,from.pluginDescription,false);if(!to.plugin)return nullptr;}
        if(from.effect){to.effect=duplicate(*from.effect,from.effectDescription,true);if(!to.effect)return nullptr;}
        to.prepare(copy->sampleRate,copy->blockSize);
    }
    return copy;
}
void AudioEngine::startPreview(juce::AudioBuffer<float>&& audio,double rate){
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
// Message thread only (the only thread that replaces plugins), so no engine lock: holding it
// while a plugin serialises its state would silence playback during save/autosave/undo.
void AudioEngine::capturePluginStates(Project& p){for(size_t ti=0;ti<instruments.size();++ti){if(auto* plugin=instruments[ti]->plugin.get()){juce::MemoryBlock state;plugin->getStateInformation(state);p.tracks[ti].instrumentState=state.toBase64Encoding().toStdString();}if(auto* effect=instruments[ti]->effect.get()){juce::MemoryBlock state;effect->getStateInformation(state);p.tracks[ti].effectState=state.toBase64Encoding().toStdString();}}}
void AudioEngine::restorePluginState(int track,const juce::String& state){juce::MemoryBlock data;if(data.fromBase64Encoding(state)){if(auto* p=instruments[(size_t)track]->plugin.get())p->setStateInformation(data.getData(),(int)data.getSize());}}
void AudioEngine::restoreEffectState(int track,const juce::String& state){juce::MemoryBlock data;if(data.fromBase64Encoding(state)){if(auto* p=instruments[(size_t)track]->effect.get())p->setStateInformation(data.getData(),(int)data.getSize());}}
void AudioEngine::markMissing(int track){const juce::ScopedLock lock(mutex);instruments[(size_t)track]->mode=3;}
juce::Result AudioEngine::exportAudio(const Project& source,const juce::File& file,int onlyTrack,bool mp3,std::function<bool()> cancelled,std::function<void(double)> progress,double* rawPeak){
    if(rawPeak)*rawPeak=0;
    if(onlyTrack < -1 || onlyTrack>=trackCount)return juce::Result::fail("Invalid export track");
    auto p=source;p.normalise();
    bool expected=false;if(!exporting.compare_exchange_strong(expected,true))return juce::Result::fail("Esportazione gia in corso.");
    struct Guard{AudioEngine& e;~Guard(){const juce::ScopedLock lock(e.mutex);e.sampleRate=e.deviceRate;e.blockSize=e.deviceBlock;for(auto& i:e.instruments){if(i->plugin)i->plugin->setNonRealtime(false);if(i->effect)i->effect->setNonRealtime(false);i->prepare(e.sampleRate,e.blockSize);}e.playing=false;e.currentBeat=0;e.loopBypassed=false;e.needsReset=true;e.hardReset=true;e.exporting=false;}} guard{*this};
    publish(p);
    int compensation=0;
    {const juce::ScopedLock lock(mutex);for(int ti=0;ti<trackCount;++ti)if(instruments[(size_t)ti]->mode==3&&(onlyTrack<0||ti==onlyTrack))return juce::Result::fail("Ricollega gli strumenti mancanti prima di esportare.");
        playing=false;currentBeat=0;needsChase=false;collectGarbage();adoptState();if(mp3)sampleRate=44100;
        for(auto& i:instruments){if(i->plugin)i->plugin->setNonRealtime(true);if(i->effect)i->effect->setNonRealtime(true);i->prepare(sampleRate,blockSize);}resetAt(0);
        // Delay compensation moves every track later by the largest latency: render that much
        // more and drop it from the start, so the file lines up with the grid.
        for(auto& i:instruments)compensation=std::max(compensation,(i->mode==2&&i->plugin?i->plugin->getLatencySamples():0)+(i->effect?i->effect->getLatencySamples():0));
        compensation=std::clamp(compensation,0,Instrument::maxDelay-1);}
    juce::TemporaryFile temp(file);auto stream=temp.getFile().createOutputStream();if(!stream)return juce::Result::fail("Impossibile scrivere il file audio.");
    juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(),sampleRate,2,24,{},0));if(!writer)return juce::Result::fail("Encoder WAV non disponibile.");stream.release();
    juce::AudioBuffer<float> buffer(2,blockSize);const auto total=(juce::int64)std::ceil((p.length()*60./p.settings.bpm+3.)*sampleRate);
    const juce::int64 renderTotal=total+compensation;
    for(juce::int64 at=0;at<renderTotal;){if(cancelled())return juce::Result::fail("Esportazione annullata.");int n=(int)std::min<juce::int64>(blockSize,renderTotal-at);buffer.clear();
        const int skip=(int)std::clamp<juce::int64>(compensation-at,0,n);
        {const juce::ScopedLock lock(mutex);currentBeat=at*p.settings.bpm/(sampleRate*60.);render(buffer.getArrayOfWritePointers(),2,n,currentBeat,onlyTrack);}
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
