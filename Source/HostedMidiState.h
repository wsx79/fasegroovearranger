// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
namespace fga {
// Desired processor state, including live input. Fixed storage also bounds recovery
// after an arbitrarily long callback-lock hold or suspension: no event queue grows.
struct HostedMidiState {
    std::array<unsigned char,16*128> notes{};
    std::array<unsigned char,16*128> sentNotes{};
    std::array<short,16*128> controllers;
    std::array<short,16> wheel,pressure;
    bool dirty=false;
    HostedMidiState(){clear();dirty=false;}
    void clear(){notes.fill(0);controllers.fill(-1);wheel.fill(-1);pressure.fill(-1);dirty=true;}
    void observe(const juce::MidiBuffer& midi){
        for(auto event:midi){const auto m=event.getMessage();const int channel=m.getChannel()-1;if(channel<0||channel>=16)continue;const size_t base=(size_t)channel*128;
            const bool pedal=controllers[base+64]>=64;
            if(m.isNoteOn())notes[base+(size_t)m.getNoteNumber()]=m.getVelocity();
            else if(m.isNoteOff()){auto& n=notes[base+(size_t)m.getNoteNumber()];n=pedal&&n?(unsigned char)(n|128):0;}
            else if(m.isController()){
                const int cc=m.getControllerNumber(),value=m.getControllerValue();
                if(cc==120){for(size_t n=0;n<128;++n)notes[base+n]=0;}
                else if(cc==123){for(size_t n=0;n<128;++n)if(notes[base+n])notes[base+n]=pedal?(unsigned char)(notes[base+n]|128):0;}
                else if(cc==121){for(size_t n=0;n<128;++n){controllers[base+n]=-1;if(notes[base+n]&128)notes[base+n]=0;}wheel[(size_t)channel]=8192;pressure[(size_t)channel]=0;}
                else {controllers[base+(size_t)cc]=(short)value;if(cc==64&&value<64)for(size_t n=0;n<128;++n)if(notes[base+n]&128)notes[base+n]=0;}
            }else if(m.isPitchWheel())wheel[(size_t)channel]=(short)m.getPitchWheelValue();
            else if(m.isChannelPressure())pressure[(size_t)channel]=(short)m.getChannelPressureValue();
        }
    }
    void restore(juce::MidiBuffer& midi)const{
        for(int ch=1;ch<=16;++ch){const size_t base=(size_t)(ch-1)*128;
            midi.addEvent(juce::MidiMessage::allSoundOff(ch),0);
            midi.addEvent(juce::MidiMessage::controllerEvent(ch,64,0),0);
            // Release explicitly too: some processors ignore channel-mode panic.
            for(int n=0;n<128;++n)if(sentNotes[base+(size_t)n])midi.addEvent(juce::MidiMessage::noteOff(ch,n),0);
            for(int cc=0;cc<128;++cc)if(controllers[base+(size_t)cc]>=0)midi.addEvent(juce::MidiMessage::controllerEvent(ch,cc,controllers[base+(size_t)cc]),0);
            if(wheel[(size_t)ch-1]>=0)midi.addEvent(juce::MidiMessage::pitchWheel(ch,wheel[(size_t)ch-1]),0);
            if(pressure[(size_t)ch-1]>=0)midi.addEvent(juce::MidiMessage::channelPressureChange(ch,pressure[(size_t)ch-1]),0);
            for(int n=0;n<128;++n)if(auto v=notes[base+(size_t)n]){midi.addEvent(juce::MidiMessage::noteOn(ch,n,(juce::uint8)(v&127)),0);if(v&128)midi.addEvent(juce::MidiMessage::noteOff(ch,n),0);}
        }
    }
};
}
