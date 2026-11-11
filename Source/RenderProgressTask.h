// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "Localisation.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>

namespace fga {
// Export/freeze follow the cooperative polling lifecycle already used by
// MediaTask. Never join (or forcibly stop) a renderer from a UI timer: plugin
// preparation/cleanup may itself need the message thread.
class RenderProgressTask : public juce::Thread, private juce::Timer {
public:
    explicit RenderProgressTask(const juce::String& title)
        : Thread(title), alert(title, {}, juce::MessageBoxIconType::NoIcon) {
        alert.addProgressBarComponent(displayProgress);
        alert.addButton(tr("Cancel"), 0, juce::KeyPress(juce::KeyPress::escapeKey));
        alert.setEscapeKeyCancels(false);
        alert.getButton(0)->onClick = [this] { requestCancel(); };
    }
    ~RenderProgressTask() override {
        stopTimer();
        // Owners retain the task (and its engine) until the worker has returned.
        // Normal app close also goes through MainComponent::requestClose.
        jassert(!isThreadRunning());
    }
    void launchThread() {
        jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
        alert.centreWithSize(440, 150);
        alert.enterModalState(true, nullptr, false);
        launchFailed = !startThread();
        startTimer(30);
    }
    juce::AlertWindow* getAlertWindow() { return &alert; }
    void requestCancel() {
        // Message thread only. Repeated clicks/close requests are harmless.
        cancelRequested = true;
        signalThreadShouldExit();
        alert.getButton(0)->setEnabled(false);
        alert.setMessage(tr("Cancelling export..."));
    }
    void setProgress(double value) noexcept {
        progress.store(std::clamp(value, 0., 1.), std::memory_order_relaxed);
    }
    double progressValue() const noexcept { return progress.load(std::memory_order_relaxed); }
    void setStatusMessage(const juce::String& text) {
        const juce::ScopedLock lock(statusLock);
        status = text;
    }
    bool failedToStart() const noexcept { return launchFailed; }
    virtual void threadComplete(bool cancelled) = 0;
private:
    void timerCallback() override {
        displayProgress = progressValue();
        if (isThreadRunning()) {
            if (!alert.isCurrentlyModal(false) && !cancelRequested) requestCancel();
            const juce::ScopedTryLock lock(statusLock);
            if (lock.isLocked() && !cancelRequested) alert.setMessage(status);
            return; // Keep dispatching until run(), stack destructors and cleanup finish.
        }
        stopTimer();
        const bool cancelled = cancelRequested || threadShouldExit();
        alert.exitModalState(1);
        alert.setVisible(false);
        threadComplete(cancelled); // May retire this object; no accesses after this call.
    }
    juce::AlertWindow alert;
    juce::CriticalSection statusLock;
    juce::String status;
    std::atomic<double> progress{0};
    double displayProgress = 0; // ProgressBar reads this only on the message thread.
    bool cancelRequested = false, launchFailed = false;
};
}
