// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "core/Project.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include "../ThirdParty/fasegroovepiano/Source/GuideVoice.h"
#include "../ThirdParty/fasegroovedrums/Source/GuideDrums.h"
#include "../ThirdParty/fasegrooveguitar/Source/GuideGuitar.h"
#include <atomic>
#include <juce_dsp/juce_dsp.h>
namespace fga {
// Everything the audio thread plays, built on the message thread and handed over without a
// lock (see Handoff). Schedules and automation are read in place: the audio thread never
// copies, allocates or frees them.
struct TrackMix {
    VolumeAutomation volume;
    double gain=.75,pan=0,tone=.5,reverb=.12,chorus=0,pump=0,pumpRelease=.2;
    bool mute=false,solo=false;
    int sound=0,pumpRate=0,pumpSource=0,drumNote=36;
    std::array<float,fgg::numLanes> laneVolume{};
    std::array<bool,fgg::numLanes> laneMute{};
};
struct MixState { std::array<TrackMix,trackCount> tracks; double master=.8,tempo=122; };
struct ScheduleState {
    std::array<std::vector<TimedEvent>,trackCount> schedules;
    std::array<std::uint64_t,trackCount> hashes{}; // which tracks changed since the last state
    double length=0,loopFrom=0,loopTo=0;
    bool looping=false;
};
// Single-producer/single-consumer swap: post() on a non-audio thread, adopt() on the audio
// thread. The state replaced on the audio thread is parked in `retired` and freed by the
// next post()/collect() on the posting side, so the audio thread never deletes anything.
template<class T> struct Handoff {
    std::atomic<T*> pending{nullptr},retired{nullptr};
    T* current=nullptr; // owned by the audio side
    ~Handoff(){delete pending.load();delete retired.load();delete current;}
    void post(std::unique_ptr<T> next){collect();delete pending.exchange(next.release());}
    void collect(){delete retired.exchange(nullptr);}
    T* adopt(){if(retired.load()!=nullptr)return nullptr;T* next=pending.exchange(nullptr);if(next==nullptr)return nullptr;retired.store(current);current=next;return next;}
};
struct Instrument {
    explicit Instrument(int track);
    void prepare(double rate,int block);
    void reset();
    int kind;
    std::atomic<float> tone{.5f}, sound{0};
    juce::Synthesiser synth;
    juce::Reverb reverb;
    juce::dsp::StateVariableTPTFilter<float> bassLowPass;
    juce::dsp::Chorus<float> chorus;
    double pump=0,pumpRelease=.2;int pumpRate=0,pumpSource=0;
    GuideDrums drums;
    GuideGuitar guitar;
    DrumSamples drumSamples;
    std::array<int,fgd::numLanes> drumNotes{};
    std::array<float,fgd::numLanes> drumLevels{};
    std::array<float,fgg::numLanes> guitarLevels{};
    std::array<bool,fgg::numLanes> guitarMuted{};
    std::vector<GuideGuitar::Event> guitarEvents;
    juce::AudioBuffer<float> scratch;
    juce::MidiBuffer midi,effectMidi;
    std::unique_ptr<juce::AudioPluginInstance> plugin,effect;
    const std::vector<TimedEvent>* schedule=nullptr; // inside the adopted ScheduleState
    const VolumeAutomation* volume=nullptr;         // inside the adopted MixState
    std::vector<float> volumeGains;
    double gain=.75,pan=0;
    juce::SmoothedValue<float> leftGain,rightGain;
    bool mute=false,solo=false;
    int mode=0; // 0 original guide, 1 user sampler, 2 user plugin, 3 missing instrument (silence)
    std::size_t cursor=0;
    int outputs=2; // channels the instrument writes: a mono plugin feeds both sides
    // Notes sent and not yet released: explicit note-offs on stop/seek/loop (many plugins
    // ignore All Notes Off), and edits while playing touch only the notes that changed.
    std::array<bool,128> sounding{};
    std::array<double,128> soundingStart{};
    std::array<int,128> soundingVelocity{};
    bool resync=false;
    std::uint64_t noteOns=0; // diagnostics and tests
};
class AudioEngine : public juce::AudioIODeviceCallback, public juce::AudioPlayHead {
public:
    AudioEngine();
    ~AudioEngine() override;
    juce::AudioDeviceManager devices;
    juce::AudioPluginFormatManager formats;
    // publish/updateMix never take the engine lock: playback keeps running while editing.
    void publish(const Project&);
    void updateMix(const Project&);
    void collectGarbage(){schedules.collect();mixes.collect();}
    void setMetronome(bool on){metronome=on;}
    bool metronomeOn() const {return metronome.load();}
    void play(bool);
    void stop();
    void seek(double beat);
    void setLoop(bool enabled,double from,double to);
    bool isPlaying() const {return playing.load();}
    double position() const {return currentBeat.load();}
    float level(int track) const {return meters[(size_t)track].load();}
    float masterLevel() const {return masterMeter.load();}
    bool hasClipped() const {return masterClip.load();}
    void clearClipping(){masterClip=false;}
    std::uint64_t noteOnsSent(int track) const {return instruments[(size_t)track]->noteOns;}
    juce::Result loadSample(int,const juce::File&,int root);
    void loadPlugin(int,const juce::PluginDescription&,std::function<void(juce::Result)>);
    void loadEffect(int,const juce::PluginDescription&,std::function<void(juce::Result)>);
    void clearEffect(int);
    void useInternal(int);
    juce::AudioPluginInstance* plugin(int);
    juce::AudioPluginInstance* effectPlugin(int);
    void capturePluginStates(Project&);
    void restorePluginState(int,const juce::String&);
    void restoreEffectState(int,const juce::String&);
    // Same processing graph for playback and export. Export temporarily owns the engine.
    juce::Result exportAudio(const Project&,const juce::File&,int onlyTrack,bool mp3,std::function<bool()> cancelled,std::function<void(double)> progress,double* rawPeak=nullptr);
    void markMissing(int);
    juce::Optional<PositionInfo> getPosition() const override;
    void audioDeviceAboutToStart(juce::AudioIODevice*) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(const float* const*,int,float* const*,int,int,const juce::AudioIODeviceCallbackContext&) override;
private:
    juce::CriticalSection mutex; // transport and instrument changes; never held for long
    std::array<std::unique_ptr<Instrument>,trackCount> instruments;
    std::array<std::atomic<float>,trackCount> meters;
    std::atomic<bool> playing{false},exporting{false},masterClip{false},metronome{false};
    int clickLeft=0,clickPos=0;double clickHz=1000;
    std::atomic<float> masterMeter{0};
    std::atomic<double> currentBeat{0},tempo{122};
    Handoff<ScheduleState> schedules;
    Handoff<MixState> mixes;
    double sampleRate=44100,length=0,loopFrom=0,loopTo=0,master=.8;
    int blockSize=512;
    double deviceRate=44100;
    int deviceBlock=512;
    bool looping=false,loopBypassed=false,needsReset=true,needsChase=false,needsResync=false,hardReset=true;
    std::shared_ptr<int> lifetime=std::make_shared<int>(0);
    void adoptState();
    void adoptWhileStopped();
    void applyMix(const MixState&,bool snap);
    void resetAt(double beat);
    void resyncAt(double beat);
    void flushIdle(int count);
    void click(float* const* output,int channels,int count,double beat);
    void render(float* const* output,int channels,int count,double beat,int onlyTrack=-1);
};
}
