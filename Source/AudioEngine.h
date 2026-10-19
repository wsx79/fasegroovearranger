// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "core/Project.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include "../ThirdParty/fasegroovepiano/Source/GuideVoice.h"
#include "../ThirdParty/fasegroovedrums/Source/GuideDrums.h"
#include "../ThirdParty/fasegrooveguitar/Source/GuideGuitar.h"
#include <atomic>
namespace fga {
struct Instrument {
    explicit Instrument(int track);
    void prepare(double rate,int block);
    void reset();
    int kind;
    std::atomic<float> tone{.5f}, sound{0};
    juce::Synthesiser synth;
    GuideDrums drums;
    GuideGuitar guitar;
    DrumSamples drumSamples;
    std::array<int,fgd::numLanes> drumNotes{};
    std::array<float,fgd::numLanes> drumLevels{};
    std::array<float,fgg::numLanes> guitarLevels{};
    std::vector<GuideGuitar::Event> guitarEvents;
    juce::AudioBuffer<float> scratch;
    juce::MidiBuffer midi;
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    std::vector<TimedEvent> schedule;
    double gain=.75,pan=0;
    bool mute=false,solo=false;
    int mode=0; // 0 original guide, 1 user sampler, 2 user plugin, 3 missing instrument (silence)
    std::size_t cursor=0;
};
class AudioEngine : public juce::AudioIODeviceCallback, public juce::AudioPlayHead {
public:
    AudioEngine();
    ~AudioEngine() override;
    juce::AudioDeviceManager devices;
    juce::AudioPluginFormatManager formats;
    void publish(const Project&);
    void updateMix(const Project&);
    void play(bool);
    void stop();
    void seek(double beat);
    void setLoop(bool enabled,double from,double to);
    bool isPlaying() const {return playing.load();}
    double position() const {return currentBeat.load();}
    float level(int track) const {return meters[(size_t)track].load();}
    juce::Result loadSample(int,const juce::File&,int root);
    void loadPlugin(int,const juce::PluginDescription&,std::function<void(juce::Result)>);
    void useInternal(int);
    juce::AudioPluginInstance* plugin(int);
    void capturePluginStates(Project&);
    void restorePluginState(int,const juce::String&);
    // Same processing graph for playback and export. Export temporarily owns the engine.
    juce::Result exportAudio(const Project&,const juce::File&,int onlyTrack,bool mp3,std::function<bool()> cancelled,std::function<void(double)> progress);
    void markMissing(int);
    juce::Optional<PositionInfo> getPosition() const override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(const float* const*,int,float* const*,int,int,const juce::AudioIODeviceCallbackContext&) override;
private:
    juce::CriticalSection mutex;
    std::array<std::unique_ptr<Instrument>,trackCount> instruments;
    std::array<std::atomic<float>,trackCount> meters;
    std::atomic<bool> playing{false},exporting{false};
    std::atomic<double> currentBeat{0},tempo{122};
    double sampleRate=44100,length=0,loopFrom=0,loopTo=0,master=.8;
    int blockSize=512;
    bool looping=false,needsReset=true,needsChase=false;
    std::shared_ptr<int> lifetime=std::make_shared<int>(0);
    void resetAt(double beat);
    void render(float* const* output,int channels,int count,double beat,int onlyTrack=-1);
};
}
