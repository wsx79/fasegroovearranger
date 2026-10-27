// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <cmath>
#include "core/Project.h"
namespace fga {
struct AudioTake {
    juce::File file;double rate=44100,start=0,tempo=120;juce::int64 frames=0;bool overflow=false,rateChanged=false;
    double seconds()const{return frames/std::max(1.,rate);}
    juce::int64 trimFrames()const{if(!std::isfinite(start)||!std::isfinite(tempo)||tempo<=0||!std::isfinite(rate)||rate<=0)return frames;if(start>=0)return 0;return (juce::int64)std::clamp(std::ceil(-start*60./tempo*rate-1e-7),0.,(double)std::max<juce::int64>(0,frames));}
    double playableSeconds()const{return std::max<juce::int64>(0,frames-trimFrames())/std::max(1.,rate);}

};
// The project keeps its existing sample + MIDI representation; no destructive audio edit.
inline int addAudioTake(Project& project,const AudioTake& take,bool muted){
    if(!fitsSampleMemory(take.frames,take.rate,2)||!std::isfinite(take.start)||take.playableSeconds()<=0||!take.file.existsAsFile()||project.tracks.size()>=(size_t)maxTracks)return -1;
    const double start=std::max(0.,take.start),duration=take.playableSeconds()*project.settings.bpm/60.,end=start+duration;
    int bars=(int)std::ceil(std::max(0.,end-project.length())/4.);if((int)project.sections.size()+(bars+127)/128>64)return -1;
    while(bars>0){int count=std::min(128,bars);project.sections.push_back({Section::main,count,"Audio take"});bars-=count;}
    const int index=project.addMidiTrack();if(index<0)return -1;auto& track=project.tracks[(size_t)index];
    track.name=take.file.getFileNameWithoutExtension().toStdString();track.instrument="sample";track.instrumentPath=take.file.getFullPathName().toStdString();track.sampleRoot=60;track.gain=1;track.mute=muted;track.freeTimeline=true;
    track.sampler.mode=1;track.sampler.start=(double)take.trimFrames()/take.frames;track.sampler.sourceBpm=take.tempo;track.sampler.attack=0;track.sampler.decay=0;track.sampler.sustain=1;track.sampler.fadeIn=0;track.sampler.fadeOut=0;track.sampler.velocity=0;
    track.clips.clear();Clip clip;clip.position=start;clip.span=duration;clip.notes={{0,duration,60,127,0}};track.clips.push_back(std::move(clip));project.normalise();return index;
}
// The callback only enqueues already allocated stereo frames. Encoding, disk I/O,
// WAV header finalization and destruction belong to the control/disk threads.
class AudioTakeRecorder {
public:
    explicit AudioTakeRecorder(AudioTake take):info(std::move(take)),disk("Audio take writer"){}
    juce::Result open(){
        if(!std::isfinite(info.rate)||info.rate<=0)return juce::Result::fail("Invalid audio recording sample rate.");
        auto stream=info.file.createOutputStream();if(!stream)return juce::Result::fail("Cannot create audio take.");
        stream->setPosition(0);stream->truncate();juce::WavAudioFormat format;
        auto* encoder=format.createWriterFor(stream.get(),info.rate,2,24,{},0);if(!encoder)return juce::Result::fail("Cannot create WAV recorder.");
        stream.release();disk.startThread();writer=std::make_unique<juce::AudioFormatWriter::ThreadedWriter>(encoder,disk,131072);return juce::Result::ok();
    }
    void append(const float* left,const float* right,int count)noexcept{
        const float* channels[]{left,right};if(!writer||!writer->write(channels,count)){overflow=true;return;}frames.fetch_add(count,std::memory_order_relaxed);
    }
    AudioTake finish(){writer.reset();disk.stopThread(10000);info.frames=frames.load();info.overflow=overflow.load();info.rateChanged=rateChanged.load();juce::WavAudioFormat format;auto stream=info.file.createInputStream();std::unique_ptr<juce::AudioFormatReader> reader(stream?format.createReaderFor(stream.release(),true):nullptr);if(!reader){info.overflow=true;info.frames=0;}else if(reader->lengthInSamples!=info.frames){info.overflow=true;info.frames=std::max<juce::int64>(0,std::min(info.frames,reader->lengthInSamples));}return info;}
    ~AudioTakeRecorder(){writer.reset();disk.stopThread(10000);}
    AudioTake info;std::atomic<juce::int64> frames{0};std::atomic<bool> overflow{false},rateChanged{false};
private:
    juce::TimeSliceThread disk;std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
};
}
