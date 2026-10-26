// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AudioEngine.h"
namespace fga {
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
    const juce::ScopedLock lock(mutex);const int next=std::clamp(track,0,maxTracks-1),previous=capture.target;
    const bool release=previous!=next||(capture.monitor&&!enabled);
    if(release&&!capture.active&&previous>=0&&previous<maxTracks)for(int ch=0;ch<16;++ch){
        for(int note=0;note<128;++note)if(liveHeld[(size_t)previous][(size_t)(ch*128+note)])capture.enqueue(previous,juce::MidiMessage::noteOff(ch+1,note),false);
        capture.enqueue(previous,juce::MidiMessage::controllerEvent(ch+1,64,0),false);
    }
    capture.target=next;capture.monitor=enabled;
}
juce::Result AudioEngine::startRecording(int track,int bars){const juce::ScopedLock lock(mutex);if(exporting)return juce::Result::fail("Wait for audio export to finish.");if(track<0||track>=maxTracks)return juce::Result::fail("Invalid MIDI track.");if(capture.active)return juce::Result::fail("A MIDI take is already recording.");capture.resetRecordQueue();takeInfo={};takeInfo.track=track;takeInfo.start=currentBeat;takeInfo.loopStart=loopFrom;takeInfo.loopEnd=loopTo;takeInfo.looping=looping&&!loopBypassed&&loopTo>loopFrom;capture.target=track;capture.countIn=playing?0:std::clamp(bars,0,2)*4.;capture.active=true;recordWallStart=juce::Time::getMillisecondCounterHiRes()*.001;chaseRecord=true;if(!playing){playing=true;hardReset=true;needsReset=true;needsChase=true;}return juce::Result::ok();}
MidiTake AudioEngine::finishRecording(){const juce::ScopedLock lock(mutex);capture.active=false;MidiTake take=takeInfo;take.duration=capture.elapsed;take.overflow=capture.overflow;capture.drain(take.events);capture.countIn=0;return take;}
void AudioEngine::chaseLiveRecord(double beat){
    chaseRecord=false;const int target=capture.target;if(target<0||target>=maxTracks)return;
    for(int c=0;c<16;++c){if(livePedal[(size_t)target][(size_t)c])capture.append({beat,capture.elapsed.load(),0xb0|c,64,127});
        for(int n=0;n<128;++n){auto value=liveHeld[(size_t)target][(size_t)(c*128+n)];if(value){capture.append({beat,capture.elapsed.load(),0x90|c,n,value&127});if(value&128)capture.append({beat,capture.elapsed.load(),0x80|c,n,0});}}
    }
}
void AudioEngine::processLive(int count,double beat,bool recording){
    if(count<=0)return;
    const double now=juce::Time::getMillisecondCounterHiRes()*.001;const double previous=inputBlockTime>0?inputBlockTime:now-count/sampleRate;inputBlockTime=now;LiveMidiEvent event;
    for(int drained=0;drained<16384&&capture.pop(event);++drained){
        const bool hardware=event.track<0;int track=hardware?capture.target.load():event.track;
        if(track<0||track>=maxTracks||(hardware&&!capture.monitor&&!capture.active))continue;
        int at=event.offset>=0?std::clamp(event.offset,0,count-1):std::clamp((int)std::llround((event.time-previous)*sampleRate),0,count-1);
        juce::MidiMessage message(event.bytes.data(),event.size);auto& i=*instruments[(size_t)track];
        if(isDrumTrack(track)&&i.mode==0&&message.isNoteOnOrOff())message.setNoteNumber(i.drumNotes[(size_t)track-4]);
        i.midi.addEvent(message,at);if(message.isNoteOnOrOff()){
            const int pitch=message.getNoteNumber(),ch=std::max(0,message.getChannel()-1);auto& held=liveHeld[(size_t)track][(size_t)(ch*128+pitch)];if(message.isNoteOn())held=message.getVelocity();else if(held&&livePedal[(size_t)track][(size_t)ch])held=(unsigned char)(held|128);else held=0;
            if(track==3)i.guitarEvents.push_back({at,pitch,message.isNoteOn()?message.getFloatVelocity():0.f,0});if(track==leadTrack)i.leadEvents.push_back({at,pitch,message.isNoteOn()?message.getFloatVelocity():0.f,0});
        }else if(message.isController()&&message.getControllerNumber()==64){const int ch=std::max(0,message.getChannel()-1);const bool pedal=message.getControllerValue()>=64;livePedal[(size_t)track][(size_t)ch]=pedal;if(!pedal)for(int n=0;n<128;++n){auto& held=liveHeld[(size_t)track][(size_t)(ch*128+n)];if(held&128)held=0;}}
        else if(message.isAllNotesOff()||message.isAllSoundOff()){liveHeld[(size_t)track].fill(0);livePedal[(size_t)track].fill(false);}
        liveTail=std::max(liveTail,(int)sampleRate*2);
        if(recording&&event.recordable&&track==capture.target&&event.time+1.e-6>=recordWallStart&&event.size>=3){
            const double offset=(at-(hardware?count:0)-deviceOutputLatency)/sampleRate*tempo.load()/60+capture.timingOffsetMs.load()*.001*tempo.load()/60;
            const auto* bytes=message.getRawData();capture.append({std::max(0.,beat+offset),std::max(0.,capture.elapsed.load()+offset),bytes[0],bytes[1],bytes[2]});
        }
    }
    if(capture.panic.exchange(false)){for(auto& i:instruments){if(i->sampler)i->sampler->allNotesOff(0,false);i->synth.allNotesOff(0,false);i->midi.addEvent(juce::MidiMessage::allSoundOff(1),0);i->sounding.fill(false);i->resync=true;}for(auto& keys:liveHeld)keys.fill(0);for(auto& pedal:livePedal)pedal.fill(false);needsResync=true;}
}
}
