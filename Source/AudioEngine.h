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
    VolumeAutomation volume,panAutomation;
    std::vector<ParameterAutomation> parameters; // copied on the message thread, only read by the audio thread
    double gain=.75,pan=0,tone=.5,reverb=.12,chorus=0,pump=0,pumpRelease=.2;
    bool mute=false,solo=false;
    int sound=0,pumpRate=0,pumpSource=0,drumNote=36;
    std::array<float,fgg::numLanes> laneVolume{};
    std::array<bool,fgg::numLanes> laneMute{};
};
struct MixState { std::array<TrackMix,trackCount> tracks; VolumeAutomation masterAutomation; double master=.8,tempo=122; };
// The built-in guide sounds of each track (piano/bass, strings, guitar, drums).
inline int builtInSoundCount(int track){return track==2?1:track<2?numGuideSounds:4;}
inline juce::String builtInSoundName(int track,int sound){return track==2?juce::String("String ensemble"):track<2?juce::String(guideSoundName(sound)):track==3?juce::String(GuideGuitar::toneName(sound)):juce::String(GuideDrums::kitName(sound));}
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
    juce::Synthesiser synth;                    // built-in guide voices (tracks 0-2)
    std::unique_ptr<juce::Synthesiser> sampler; // user sample: built off the audio thread, swapped in
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
    juce::PluginDescription pluginDescription,effectDescription; // as loaded: re-creates them for a private export
    const std::vector<TimedEvent>* schedule=nullptr; // inside the adopted ScheduleState
    const VolumeAutomation* volume=nullptr;         // inside the adopted MixState
    const VolumeAutomation* panAutomation=nullptr;  // inside the adopted MixState
    const std::vector<ParameterAutomation>* parameters=nullptr; // inside the adopted MixState
    std::array<float,maxParameterLanes> parameterSent{}; // last value set on each lane's parameter (-1: none yet)
    std::vector<float> volumeGains,panLeft,panRight;
    juce::SmoothedValue<float> baseGain;            // gain * master: the pan automation applies its own pan law
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
    // Plugin delay compensation: every track is delayed to line up with the slowest one.
    // The line is allocated once here, never on the audio thread.
    static constexpr int maxDelay=1<<15;
    juce::AudioBuffer<float> delayLine{2,maxDelay};
    int delayWrite=0,latency=0,delay=0;
    bool delayUsed=false;
    void adoptChannels(int block); // after a plugin/effect swap: channel count and scratch size
    void applyDelay(juce::AudioBuffer<float>&,int count);
};
// A file being auditioned by the sample browser: decoded off the audio thread, then handed over.
struct PreviewClip {
    juce::AudioBuffer<float> audio;
    double rate=44100,position=0;
};
class AudioEngine : public juce::AudioIODeviceCallback, public juce::AudioPlayHead {
public:
    AudioEngine();
    struct Offline {};
    explicit AudioEngine(Offline); // no audio device: a private copy used to render exports
    ~AudioEngine() override;
    // Message thread: a copy of the current instruments (samples shared, plugins re-created from
    // their state) that renders an export while this engine keeps playing. nullptr when a plugin
    // cannot be duplicated: exportAudio on this engine is then the fallback.
    std::unique_ptr<AudioEngine> offlineCopy();
    // Sample browser audition, mixed after the master and independent of the transport.
    void startPreview(juce::AudioBuffer<float>&& audio,double rate);
    void stopPreview();
    bool isPreviewing() const {return previewing.load();}
    juce::AudioDeviceManager devices;
    juce::AudioPluginFormatManager formats;
    // publish/updateMix never take the engine lock: playback keeps running while editing.
    // `normalised`: the caller has just normalised the project, so it is used as is (no copy).
    // onlyTrack: only that track's notes changed since the last normalised publish.
    void publish(const Project&,bool normalised=false,int onlyTrack=-1);
    void updateMix(const Project&);
    void collectGarbage(){schedules.collect();mixes.collect();previews.collect();}
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
    // Per side (0 left, 1 right) peaks of the last block, for the stereo meters.
    float level(int track,int side) const {return sideMeters[(size_t)track][(size_t)(side&1)].load();}
    float masterLevel(int side) const {return masterSides[(size_t)(side&1)].load();}
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
    std::array<std::array<std::atomic<float>,2>,trackCount> sideMeters{};
    std::array<std::atomic<float>,2> masterSides{};
    void clearMeters(){masterMeter=0;for(auto& meter:meters)meter=0;for(auto& side:masterSides)side=0;for(auto& pair:sideMeters)for(auto& side:pair)side=0;}
    std::atomic<double> currentBeat{0},tempo{122};
    Handoff<ScheduleState> schedules;
    juce::CriticalSection publishedLock;ScheduleState published;bool publishedValid=false;
    Handoff<MixState> mixes;
    Handoff<PreviewClip> previews;
    std::atomic<bool> previewing{false};
    void mixPreview(float* const* output,int channels,int count);
    void swapInstrument(int track,std::unique_ptr<juce::AudioPluginInstance> next,bool effect,const juce::PluginDescription&);
    double sampleRate=44100,length=0,loopFrom=0,loopTo=0,master=.8;
    const VolumeAutomation* masterAutomation=nullptr; // inside the adopted MixState
    std::vector<float> masterGains=std::vector<float>((size_t)Instrument::maxDelay,1.f); // allocated once
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
    void applyParameterAutomation(Instrument&,double beat);
};
}
