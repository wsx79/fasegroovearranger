// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_events/juce_events.h>
#include <algorithm>
#include <memory>
#include <vector>
namespace fga {
// Message-thread owner for cooperatively cancelled workers. Destruction happens
// only after run() and all stack-owned watchers have returned.
class RetiredJobs : private juce::Timer {
public:
    ~RetiredJobs(){stopTimer();for(auto& job:jobs)job->signalThreadShouldExit();for(auto& job:jobs)job->waitForThreadToExit(-1);}
    void retire(std::unique_ptr<juce::Thread> job){if(!job)return;job->signalThreadShouldExit();jobs.push_back(std::move(job));startTimer(50);}
    size_t size()const{return jobs.size();}
private:
    void timerCallback()override{jobs.erase(std::remove_if(jobs.begin(),jobs.end(),[](auto& job){return !job->isThreadRunning();}),jobs.end());if(jobs.empty())stopTimer();}
    std::vector<std::unique_ptr<juce::Thread>> jobs;
};
}
