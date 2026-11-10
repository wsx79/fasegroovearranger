// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_core/juce_core.h>
#include <functional>
namespace fga {
// Control/worker thread only. No shell; cancellation terminates the process tree.
juce::Result runGuideProcess(const juce::StringArray& arguments, const juce::File& log,
    int timeoutMs, const std::function<bool()>& cancelled);
}
