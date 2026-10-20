// Fase Groove Arranger - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_events/juce_events.h>
namespace fga::Updater {
// Family update preference/checker. Arranger's portable packages are downloaded
// from Releases on request; this standalone host never swaps a running binary.
bool autoCheckEnabled();
void setAutoCheckEnabled(bool);
bool isNewer(const juce::String&,const juce::String&);
struct Result {
    enum class Status { upToDate, noRelease, failed, ready };
    Status status=Status::failed;
    juce::String version, notes;
};
class Checker : private juce::Thread {
public:
    Checker();
    ~Checker() override;
    void start(bool manual);
    bool isBusy()const{return isThreadRunning();}
    std::function<void(const Result&,bool)> onResult;
private:
    void run() override;
    bool manualCheck=false;
    JUCE_DECLARE_WEAK_REFERENCEABLE(Checker)
};
}
