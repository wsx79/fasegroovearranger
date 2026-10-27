// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AudioEngine.h"
namespace fga {
namespace {
void restoreOwnedInput(juce::AudioDeviceManager& devices,const std::optional<juce::AudioDeviceManager::AudioDeviceSetup>& previous,const std::optional<juce::AudioDeviceManager::AudioDeviceSetup>& activated){
    if(!previous||!activated)return;juce::AudioDeviceManager::AudioDeviceSetup current;devices.getAudioDeviceSetup(current);
    if(current.inputDeviceName!=activated->inputDeviceName||current.inputChannels!=activated->inputChannels||current.useDefaultInputChannels!=activated->useDefaultInputChannels)return;
    current.inputDeviceName=previous->inputDeviceName;current.inputChannels=previous->inputChannels;current.useDefaultInputChannels=previous->useDefaultInputChannels;devices.setAudioDeviceSetup(current,true);
}
}
juce::Result AudioEngine::startAudioRecording(const juce::File& file,AudioSource source){
    if(recordingAudio||capture.active||exporting)return juce::Result::fail("Finish the current recording or export first.");
    struct RestoreInput {juce::AudioDeviceManager& devices;std::optional<juce::AudioDeviceManager::AudioDeviceSetup> previous;bool keep=false;std::optional<juce::AudioDeviceManager::AudioDeviceSetup> activated;~RestoreInput(){if(!keep)restoreOwnedInput(devices,previous,activated);}} restore{devices,{},false};
    if(source==AudioSource::input){
        auto* device=devices.getCurrentAudioDevice();if(!device)return juce::Result::fail("Select an audio input device before recording.");
        if(device->getActiveInputChannels().countNumberOfSetBits()==0){
            juce::AudioDeviceManager::AudioDeviceSetup setup;devices.getAudioDeviceSetup(setup);restore.previous=setup;auto* type=devices.getCurrentDeviceTypeObject();
            if(!type)return juce::Result::fail("No audio input device is available.");auto names=type->getDeviceNames(true);if(names.isEmpty())return juce::Result::fail("No audio input device is available.");
            if(!names.contains(setup.inputDeviceName))setup.inputDeviceName=names[std::clamp(type->getDefaultDeviceIndex(true),0,names.size()-1)];setup.useDefaultInputChannels=false;setup.inputChannels.clear();
            std::unique_ptr<juce::AudioIODevice> probe(type->createDevice(setup.outputDeviceName,setup.inputDeviceName));const int count=probe?probe->getInputChannelNames().size():0;probe.reset();if(count<=0)return juce::Result::fail("No audio input channel is available.");setup.inputChannels.setRange(0,std::min(2,count),true);
            restore.activated=setup;if(auto error=devices.setAudioDeviceSetup(setup,true);error.isNotEmpty())return juce::Result::fail(error);
            device=devices.getCurrentAudioDevice();if(!device||device->getActiveInputChannels().countNumberOfSetBits()==0)return juce::Result::fail("Enable an audio input channel before recording.");juce::AudioDeviceManager::AudioDeviceSetup active;devices.getAudioDeviceSetup(active);restore.activated=active;
        }
    }
    double rate=44100,at=0;{const EngineControlLock lock(*this);rate=deviceRate;at=currentBeat;}
    auto next=std::make_unique<AudioTakeRecorder>(AudioTake{file,rate,at});if(auto result=next->open();result.failed())return result;
    {const EngineControlLock lock(*this);if(recordingAudio||capture.active||exporting||deviceRate!=rate)return juce::Result::fail("Audio device or recording changed; try again.");
        next->info.start=currentBeat>=length?0:currentBeat.load();if(looping&&!loopBypassed&&currentBeat>=loopTo)next->info.start=loopFrom;refreshLatencies();next->info.tempo=tempo.load();next->info.start-=(reportedLatency.load()+(source==AudioSource::input?deviceOutputLatency+deviceInputLatency:0.))/rate*tempo.load()/60.;stopPreview();stopSampleAudition();audioInputRestore=restore.previous;audioInputActivated=restore.activated;restore.keep=true;audioSource=source;audioRecorder=std::move(next);recordedAudioSeconds=0;audioRecordFault=false;recordingAudio=true;}
    return juce::Result::ok();
}
AudioTake AudioEngine::finishAudioRecording(){
    std::unique_ptr<AudioTakeRecorder> finished;std::optional<juce::AudioDeviceManager::AudioDeviceSetup> restore;std::optional<juce::AudioDeviceManager::AudioDeviceSetup> activated;{const EngineControlLock lock(*this);recordingAudio=false;while(recordingReaders.load())juce::Thread::yield();finished=std::move(audioRecorder);restore=std::move(audioInputRestore);audioInputRestore.reset();activated=std::move(audioInputActivated);audioInputActivated.reset();}
    if(finished){auto take=finished->finish();restoreOwnedInput(devices,restore,activated);const EngineControlLock lock(*this);lastAudioTake=take;return take;}const EngineControlLock lock(*this);return lastAudioTake;
}
void AudioEngine::captureAudio(const float* const* input,int channels,int count,AudioSource source){
    if(!recordingAudio.load())return;recordingReaders.fetch_add(1);
    struct ReaderScope {std::atomic<unsigned>& count;~ReaderScope(){count.fetch_sub(1);}} reader{recordingReaders};
    if(!recordingAudio.load())return;auto* recorder=audioRecorder.get();if(!recorder||audioSource!=source)return;
    if(recorder->rateChanged.load()||(source==AudioSource::input&&channels<=0)||std::abs(positionRate.load()-recorder->info.rate)>.5){recorder->rateChanged=true;audioRecordFault=true;return;}
    for(int offset=0;offset<count;){const int n=std::min(512,count-offset);for(int c=0;c<2;++c){auto* target=audioInputScratch.getWritePointer(c);const int channel=std::min(c,channels-1);if(channel>=0&&input&&input[channel])for(int i=0;i<n;++i){float value=input[channel][offset+i];target[i]=std::isfinite(value)?value:0.f;}else std::fill_n(target,n,0.f);}recorder->append(audioInputScratch.getReadPointer(0),audioInputScratch.getReadPointer(1),n);offset+=n;}
    if(recorder->overflow)audioRecordFault=true;recordedAudioSeconds=recorder->frames.load()/recorder->info.rate;
}
juce::Result AudioEngine::setMidiInput(const juce::String& id){
    if(midiIdentifier==id)return juce::Result::ok();
    const bool monitoring=capture.monitor;setMidiMonitor(capture.target,false);capture.monitor=monitoring;
    if(midiIdentifier.isNotEmpty()){devices.removeMidiInputDeviceCallback(midiIdentifier,&capture);if(ownsMidiInput)devices.setMidiInputDeviceEnabled(midiIdentifier,false);}
    midiIdentifier={};ownsMidiInput=false;if(id.isEmpty())return juce::Result::ok();
    auto list=juce::MidiInput::getAvailableDevices();if(std::none_of(list.begin(),list.end(),[&](const auto& d){return d.identifier==id;}))return juce::Result::fail("MIDI input is not available.");
    ownsMidiInput=!devices.isMidiInputDeviceEnabled(id);devices.setMidiInputDeviceEnabled(id,true);if(!devices.isMidiInputDeviceEnabled(id))return juce::Result::fail("Cannot open MIDI input.");devices.addMidiInputDeviceCallback(id,&capture);midiIdentifier=id;return juce::Result::ok();
}
std::array<bool,128> AudioEngine::activeNotes(int track)const{
    std::array<bool,128> result{};if(track<0||track>=maxTracks)return result;
    for(int word=0;word<2;++word){const auto bits=activeKeyMasks[(size_t)track][(size_t)word].load(std::memory_order_relaxed);for(int bit=0;bit<64;++bit)result[(size_t)(word*64+bit)]=(bits&(std::uint64_t(1)<<bit))!=0;}return result;
}
// Audio-thread snapshot: the GUI never locks or touches synthesiser voices.
void AudioEngine::publishActiveNotes(int track){
    std::array<std::uint64_t,2> bits{};const auto& instrument=*instruments[(size_t)track];
    auto mark=[&](int note){if(note>=0&&note<128)bits[(size_t)note/64]|=std::uint64_t(1)<<(note%64);};
    if(instrument.mode==1&&instrument.sampler){for(int v=0;v<instrument.sampler->getNumVoices();++v){const auto* voice=instrument.sampler->getVoice(v);if(voice->isVoiceActive())mark(voice->getCurrentlyPlayingNote());}}
    else for(int note=0;note<128;++note){bool sounding=instrument.sounding[(size_t)note];for(int channel=0;channel<16&&!sounding;++channel)sounding=liveHeld[(size_t)track][(size_t)(channel*128+note)]!=0;if(sounding)mark(note);}
    for(int word=0;word<2;++word)activeKeyMasks[(size_t)track][(size_t)word].store(bits[(size_t)word],std::memory_order_relaxed);
}
void AudioEngine::setMidiMonitor(int track,bool enabled){
    const EngineControlLock lock(*this);const int next=std::clamp(track,0,maxTracks-1),previous=capture.target;
    const bool release=previous!=next||(capture.monitor&&!enabled);
    if(release&&!capture.active&&previous>=0&&previous<maxTracks)for(int ch=0;ch<16;++ch){
        for(int note=0;note<128;++note)if(liveHeld[(size_t)previous][(size_t)(ch*128+note)])capture.enqueue(previous,juce::MidiMessage::noteOff(ch+1,note),false);
        capture.enqueue(previous,juce::MidiMessage::controllerEvent(ch+1,64,0),false);
    }
    capture.target=next;capture.monitor=enabled;
}
juce::Result AudioEngine::startRecording(int track,int bars){const EngineControlLock lock(*this);if(exporting)return juce::Result::fail("Wait for audio export to finish.");if(track<0||track>=maxTracks)return juce::Result::fail("Invalid MIDI track.");if(capture.active||recordingAudio)return juce::Result::fail("A take is already recording.");stopSampleAudition();capture.resetRecordQueue();takeInfo={};takeInfo.track=track;takeInfo.start=currentBeat;takeInfo.loopStart=loopFrom;takeInfo.loopEnd=loopTo;takeInfo.looping=looping&&!loopBypassed&&loopTo>loopFrom;capture.target=track;capture.countIn=playing?0:std::clamp(bars,0,2)*timeNumerator.load()*4./timeDenominator.load();capture.active=true;recordWallStart=juce::Time::getMillisecondCounterHiRes()*.001;chaseRecord=true;if(!playing){playing=true;hardReset=true;needsReset=true;needsChase=true;}return juce::Result::ok();}
MidiTake AudioEngine::finishRecording(){const EngineControlLock lock(*this);capture.active=false;MidiTake take=takeInfo;take.duration=capture.elapsed;take.overflow=capture.overflow;capture.drain(take.events);capture.countIn=0;return take;}
void AudioEngine::chaseLiveRecord(double beat){
    chaseRecord=false;const int target=capture.target;if(target<0||target>=maxTracks)return;
    for(int c=0;c<16;++c){if(livePedal[(size_t)target][(size_t)c])capture.append({beat,capture.elapsed.load(),0xb0|c,64,127});
        for(int n=0;n<128;++n){auto value=liveHeld[(size_t)target][(size_t)(c*128+n)];if(value){capture.append({beat,capture.elapsed.load(),0x90|c,n,value&127});if(value&128)capture.append({beat,capture.elapsed.load(),0x80|c,n,0});}}
    }
}
// Timestamp and order incoming MIDI once per device callback, not once per loop
// fragment. Explicitly timed UI events are deferred to their actual fragment.
void AudioEngine::prepareLiveInput(int count){
    pendingLiveCount=pendingLiveCursor=0;inputCallbackCount=count;inputSegmentOffset=0;LiveMidiEvent event;
    const int countInEnd=capture.active&&capture.countIn>0?std::min(count,std::max(1,(int)std::ceil(capture.countIn.load()*sampleRate*60/tempo.load()))):count;
    while(pendingLiveCount<(int)pendingLive->size()&&capture.pop(event)){
        auto& pending=(*pendingLive)[(size_t)pendingLiveCount];pending.event=event;pending.sample=event.offset>=0?std::clamp(event.offset,0,count-1):(int)std::llround(std::clamp((event.time-previousInputBlockTime)*sampleRate,0.,(double)(count-1)));
        // Hardware input already arrived before this callback; when count-in ends
        // inside it, retain its held state before chasing notes at recording zero.
        if(event.track<0&&countInEnd<count)pending.sample=std::min(pending.sample,countInEnd-1);
        pending.order=pendingLiveCount++;
    }
    std::sort(pendingLive->begin(),pendingLive->begin()+pendingLiveCount,[](const auto& a,const auto& b){return a.sample<b.sample||(a.sample==b.sample&&a.order<b.order);});
}
void AudioEngine::processLive(int count,double beat,bool recording){
    if(count<=0)return;if(recording)refreshLatencies();
    while(pendingLiveCursor<pendingLiveCount&&(*pendingLive)[(size_t)pendingLiveCursor].sample<inputSegmentOffset+count){
        const auto& pending=(*pendingLive)[(size_t)pendingLiveCursor++];const auto& event=pending.event;
        const bool hardware=event.track<0;int track=hardware?capture.target.load():event.track;
        if(track<0||track>=maxTracks||(hardware&&!capture.monitor&&!capture.active))continue;
        int at=std::clamp(pending.sample-inputSegmentOffset,0,count-1);
        juce::MidiMessage message(event.bytes.data(),event.size);auto& i=*instruments[(size_t)track];
        if(isDrumTrack(track)&&i.mode==0&&message.isNoteOnOrOff())message.setNoteNumber(i.drumNotes[(size_t)track-4]);
        i.midi.addEvent(message,at);if(message.isNoteOnOrOff()){
            const int pitch=message.getNoteNumber(),ch=std::max(0,message.getChannel()-1);auto& held=liveHeld[(size_t)track][(size_t)(ch*128+pitch)];if(message.isNoteOn())held=message.getVelocity();else if(held&&livePedal[(size_t)track][(size_t)ch])held=(unsigned char)(held|128);else held=0;
            if(track==3&&i.mode==0)i.guitarEvents.push_back({at,pitch,message.isNoteOn()?message.getFloatVelocity():0.f,0});if(track==leadTrack&&i.mode==0)i.leadEvents.push_back({at,pitch,message.isNoteOn()?message.getFloatVelocity():0.f,0});
        }else if(message.isController()&&message.getControllerNumber()==64){const int ch=std::max(0,message.getChannel()-1);const bool pedal=message.getControllerValue()>=64;livePedal[(size_t)track][(size_t)ch]=pedal;if(!pedal)for(int n=0;n<128;++n){auto& held=liveHeld[(size_t)track][(size_t)(ch*128+n)];if(held&128)held=0;}}
        else if(message.isAllNotesOff()||message.isAllSoundOff()){liveHeld[(size_t)track].fill(0);livePedal[(size_t)track].fill(false);}
        liveTail=std::max(liveTail,(int)sampleRate*2);
        if(recording&&event.recordable&&track==capture.target&&event.time+std::max(0,event.offset)/sampleRate+1.e-6>=recordWallStart&&event.size>=3){
            const double offset=(at-(hardware?inputCallbackCount:0)-deviceOutputLatency-instruments[(size_t)track]->latency-instruments[(size_t)track]->delay)/sampleRate*tempo.load()/60+capture.timingOffsetMs.load()*.001*tempo.load()/60;
            const auto* bytes=message.getRawData();const double elapsed=std::max(0.,capture.elapsed.load()+offset);double position=takeInfo.start+elapsed;if(takeInfo.looping&&position>=takeInfo.loopEnd){const double span=takeInfo.loopEnd-takeInfo.loopStart;position=takeInfo.loopStart+std::fmod(std::fmod(position-takeInfo.loopStart,span)+span,span);}capture.append({position,elapsed,bytes[0],bytes[1],bytes[2]});
        }
    }

}
}
