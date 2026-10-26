// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AudioEngine.h"
namespace fga {
juce::Result AudioEngine::setMidiInput(const juce::String& id){
    if(midiIdentifier==id)return juce::Result::ok();
    if(midiIdentifier.isNotEmpty()){devices.removeMidiInputDeviceCallback(midiIdentifier,&capture);if(ownsMidiInput)devices.setMidiInputDeviceEnabled(midiIdentifier,false);}
    midiIdentifier={};ownsMidiInput=false;if(id.isEmpty())return juce::Result::ok();
    auto list=juce::MidiInput::getAvailableDevices();if(std::none_of(list.begin(),list.end(),[&](const auto& d){return d.identifier==id;}))return juce::Result::fail("MIDI input is not available.");
    ownsMidiInput=!devices.isMidiInputDeviceEnabled(id);devices.setMidiInputDeviceEnabled(id,true);if(!devices.isMidiInputDeviceEnabled(id))return juce::Result::fail("Cannot open MIDI input.");devices.addMidiInputDeviceCallback(id,&capture);midiIdentifier=id;return juce::Result::ok();
}
void AudioEngine::setMidiMonitor(int track,bool enabled){const juce::ScopedLock lock(mutex);capture.target=std::clamp(track,0,maxTracks-1);capture.monitor=enabled;}
juce::Result AudioEngine::startRecording(int track,int bars){const juce::ScopedLock lock(mutex);if(exporting)return juce::Result::fail("Wait for audio export to finish.");if(track<0||track>=maxTracks)return juce::Result::fail("Invalid MIDI track.");if(capture.active)return juce::Result::fail("A MIDI take is already recording.");capture.resetRecordQueue();takeInfo={};takeInfo.track=track;takeInfo.start=currentBeat;takeInfo.loopStart=loopFrom;takeInfo.loopEnd=loopTo;takeInfo.looping=looping&&!loopBypassed&&loopTo>loopFrom;capture.target=track;capture.countIn=playing?0:std::clamp(bars,0,2)*4.;capture.active=true;recordWallStart=juce::Time::getMillisecondCounterHiRes()*.001;chaseRecord=true;if(!playing){playing=true;hardReset=true;needsReset=true;needsChase=true;}return juce::Result::ok();}
MidiTake AudioEngine::finishRecording(){const juce::ScopedLock lock(mutex);capture.active=false;MidiTake take=takeInfo;take.duration=capture.elapsed;take.overflow=capture.overflow;capture.drain(take.events);capture.countIn=0;return take;}
void AudioEngine::chaseLiveRecord(double beat){chaseRecord=false;const int target=capture.target;if(target<0||target>=maxTracks)return;for(int c=0;c<16;++c)for(int n=0;n<128;++n){auto value=liveHeld[(size_t)target][(size_t)(c*128+n)];if(value)capture.append({beat,capture.elapsed.load(),0x90|c,n,value});}}
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
            const int pitch=message.getNoteNumber(),ch=std::max(0,message.getChannel()-1);liveHeld[(size_t)track][(size_t)(ch*128+pitch)]=message.isNoteOn()?message.getVelocity():0;
            if(track==3)i.guitarEvents.push_back({at,pitch,message.isNoteOn()?message.getFloatVelocity():0.f,0});if(track==leadTrack)i.leadEvents.push_back({at,pitch,message.isNoteOn()?message.getFloatVelocity():0.f,0});
        }else if(message.isAllNotesOff()||message.isAllSoundOff())liveHeld[(size_t)track].fill(0);
        liveTail=std::max(liveTail,(int)sampleRate*2);
        if(recording&&event.recordable&&track==capture.target&&event.time+1.e-6>=recordWallStart&&event.size>=3){
            const double offset=(at-(hardware?count:0)-deviceOutputLatency)/sampleRate*tempo.load()/60+capture.timingOffsetMs.load()*.001*tempo.load()/60;
            const auto* bytes=message.getRawData();capture.append({std::max(0.,beat+offset),std::max(0.,capture.elapsed.load()+offset),bytes[0],bytes[1],bytes[2]});
        }
    }
    if(capture.panic.exchange(false)){for(auto& i:instruments){if(i->sampler)i->sampler->allNotesOff(0,false);i->synth.allNotesOff(0,false);i->midi.addEvent(juce::MidiMessage::allSoundOff(1),0);i->sounding.fill(false);i->resync=true;}for(auto& keys:liveHeld)keys.fill(0);needsResync=true;}
}
}
