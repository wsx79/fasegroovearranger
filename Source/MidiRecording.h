// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "core/Project.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <memory>
#include <atomic>
namespace fga {
struct LiveMidiEvent {std::array<unsigned char,3> bytes{};int size=0,track=-1,offset=-1;double time=0;bool recordable=true;};
struct CapturedMidi {double beat=0,elapsed=0;int status=0,note=0,value=0;};
struct RecordedNote {double beat=0,length=0;int pitch=60,velocity=100;};
struct MidiTake {
    int track=-1;double start=0,duration=0,loopStart=0,loopEnd=0;bool looping=false,overflow=false;
    std::vector<CapturedMidi> events;
    std::vector<RecordedNote> notes;
};
// MIDI-device -> audio and UI-keyboard -> audio are separate SPSC queues.
class MidiCapture : public juce::MidiInputCallback {
public:
    void handleIncomingMidiMessage(juce::MidiInput*,const juce::MidiMessage&)override;
    void enqueue(int track,const juce::MidiMessage&,bool recordable=true,int offset=0);
    bool pop(LiveMidiEvent&);
    void append(CapturedMidi); // audio producer
    void drain(std::vector<CapturedMidi>&); // message-thread consumer
    void resetRecordQueue(); // audio producer quiescent under the engine control gate
    std::atomic<int> target{-1};std::atomic<bool> monitor{false},active{false},overflow{false},inputOverflow{false};
    std::atomic<double> elapsed{0},countIn{0},timingOffsetMs{0};
private:
    static constexpr int inputCapacity=8192,recordCapacity=32768;
    juce::AbstractFifo deviceFifo{inputCapacity},uiFifo{inputCapacity},recordFifo{recordCapacity};
    std::unique_ptr<std::array<LiveMidiEvent,inputCapacity>> deviceEvents=std::make_unique<std::array<LiveMidiEvent,inputCapacity>>(),uiEvents=std::make_unique<std::array<LiveMidiEvent,inputCapacity>>();
    std::unique_ptr<std::array<CapturedMidi,recordCapacity>> recorded=std::make_unique<std::array<CapturedMidi,recordCapacity>>();
    void push(juce::AbstractFifo&,std::array<LiveMidiEvent,inputCapacity>&,LiveMidiEvent);
};
void assembleMidiTake(MidiTake&,double grid);
std::vector<std::pair<double,double>> recordedRanges(const MidiTake&);
bool applyMidiTake(Project&,const MidiTake&,bool overdub);
}
