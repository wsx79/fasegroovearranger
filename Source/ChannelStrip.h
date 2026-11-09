// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "core/EqResponse.h"
#include <juce_audio_basics/juce_audio_basics.h>
namespace fga {
// Fixed storage, direct-form II filters: coefficient updates never allocate.
class ChannelStrip {
public:
    static constexpr int maxFilters = 16;
    struct Filter {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1[2]{}, z2[2]{};
        float run(float x, int ch) {
            const double y = b0 * x + z1[ch];
            z1[ch] = b1 * x - a1 * y + z2[ch];
            z2[ch] = b2 * x - a2 * y;
            return (float)y;
        }
        void set(double b, double c, double d, double a, double e, double f) {
            b0 = b / a; b1 = c / a; b2 = d / a; a1 = e / a; a2 = f / a;
        }
        void reset() {
            z1[0] = z1[1] = 0;
            z2[0] = z2[1] = 0;
        }
    };
private:
    Filter filters[maxFilters];
    int activeCount = 0;
    bool active = false;
public:
    void reset() {
        for (auto& f : filters) f.reset();
    }
    void configure(EqSettings p, double rate) {
        p.ensureBands();
        p.normalise();
        active = p.enabled;
        rate = std::max(8000., rate);
        activeCount = 0;
        if (!active) return;
        for (size_t i = 0; i < p.bands.size() && activeCount < maxFilters; ++i) {
            const auto& b = p.bands[i];
            if (!b.enabled) continue;
            const auto c=eqCoefficients(b,rate);
            auto& f=filters[activeCount++];
            f.b0=c.b0;f.b1=c.b1;f.b2=c.b2;f.a1=c.a1;f.a2=c.a2;
        }
    }
    void process(juce::AudioBuffer<float>& audio, int count) {
        if (!active || activeCount <= 0) return;
        count = std::min(count, audio.getNumSamples());
        for (int ch = 0; ch < std::min(2, audio.getNumChannels()); ++ch) {
            auto* p = audio.getWritePointer(ch);
            for (int n = 0; n < count; ++n) {
                for (int i = 0; i < activeCount; ++i) {
                    p[n] = filters[i].run(p[n], ch);
                }
            }
        }
    }
};
}
