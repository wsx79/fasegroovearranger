// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>
namespace fga {
// One delay allocation. Immutable after it is published.
struct PublishedAudio {
    PublishedAudio(){live.fetch_add(1,std::memory_order_relaxed);}
    ~PublishedAudio(){live.fetch_sub(1,std::memory_order_relaxed);}
    juce::AudioBuffer<float> audio;
    int frames=0,channels=0;
    static std::atomic<int> live;
};
// Control thread allocates and publishes. The audio thread only loads the
// pointer it will use for the rest of that callback. A replaced buffer is
// deleted on the control thread after a later callback has observed the new one.
class DelayPublication {
public:
    DelayPublication()=default;
    DelayPublication(const DelayPublication&)=delete;
    DelayPublication& operator=(const DelayPublication&)=delete;
    ~DelayPublication(){std::lock_guard<std::mutex> guard(control);releaseAll();}
    int getNumSamples()const noexcept{auto* p=active.load(std::memory_order_acquire);return p?p->frames:0;}
    int getNumChannels()const noexcept{auto* p=active.load(std::memory_order_acquire);return p?p->channels:0;}
    void setSize(int channels,int frames){
        std::lock_guard<std::mutex> guard(control);
        if(channels<=0||frames<=0){publish(nullptr);return;}
        auto next=std::make_unique<PublishedAudio>();
        next->channels=channels;next->frames=frames;next->audio.setSize(channels,frames);next->audio.clear();
        publish(next.release());
    }
    PublishedAudio* load()noexcept{auto* p=active.load(std::memory_order_acquire);observed.store(p,std::memory_order_release);return p;}
    void collect(){std::lock_guard<std::mutex> guard(control);reclaim();}
private:
    void publish(PublishedAudio* next){auto* old=active.exchange(next,std::memory_order_acq_rel);if(old)retired.push_back(old);reclaim();}
    void reclaim(){if(observed.load(std::memory_order_acquire)!=active.load(std::memory_order_acquire))return;for(auto* p:retired)delete p;retired.clear();}
    void releaseAll(){auto* current=active.exchange(nullptr,std::memory_order_acq_rel);if(current)retired.push_back(current);for(auto* p:retired)delete p;retired.clear();}
    std::atomic<PublishedAudio*> active{nullptr},observed{nullptr};
    std::vector<PublishedAudio*> retired;
    std::mutex control;
};
}
