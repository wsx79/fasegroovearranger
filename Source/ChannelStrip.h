// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "core/StudioSettings.h"
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
        if (p.bands.size() == 4) {
            if (p.highPass != p.bands[0].freq || p.low != p.bands[1].gain || p.lowFreq != p.bands[1].freq
                || p.mid != p.bands[2].gain || p.midFreq != p.bands[2].freq || p.high != p.bands[3].gain || p.highFreq != p.bands[3].freq) {
                p.syncBandsFromLegacy();
            }
        }
        p.normalise();
        active = p.enabled;
        rate = std::max(8000., rate);
        activeCount = 0;
        if (!active) return;
        for (size_t i = 0; i < p.bands.size() && activeCount < maxFilters; ++i) {
            const auto& b = p.bands[i];
            if (!b.enabled) continue;
            const double hz = std::clamp(b.freq, 20.0, rate * 0.45);
            const double qVal = std::clamp(b.q, 0.1, 18.0);
            const double w = 6.283185307179586 * hz / rate;
            const double c = std::cos(w);
            const double s = std::sin(w);
            const double alpha = s / (2.0 * qVal);
            auto& f = filters[activeCount++];

            switch (b.type) {
                case 1: // LowCut (High-Pass)
                    f.set((1.0 + c) * 0.5, -(1.0 + c), (1.0 + c) * 0.5, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
                    break;
                case 4: // HighCut (Low-Pass)
                    f.set((1.0 - c) * 0.5, 1.0 - c, (1.0 - c) * 0.5, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
                    break;
                case 5: // Notch
                    f.set(1.0, -2.0 * c, 1.0, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
                    break;
                case 2: { // LowShelf
                    const double A = std::pow(10.0, b.gain / 40.0);
                    const double beta = 2.0 * std::sqrt(A) * alpha;
                    f.set(A * ((A + 1.0) - (A - 1.0) * c + beta),
                          2.0 * A * ((A - 1.0) - (A + 1.0) * c),
                          A * ((A + 1.0) - (A - 1.0) * c - beta),
                          (A + 1.0) + (A - 1.0) * c + beta,
                          -2.0 * ((A - 1.0) + (A + 1.0) * c),
                          (A + 1.0) + (A - 1.0) * c - beta);
                    break;
                }
                case 3: { // HighShelf
                    const double A = std::pow(10.0, b.gain / 40.0);
                    const double beta = 2.0 * std::sqrt(A) * alpha;
                    f.set(A * ((A + 1.0) + (A - 1.0) * c + beta),
                          -2.0 * A * ((A - 1.0) + (A + 1.0) * c),
                          A * ((A + 1.0) + (A - 1.0) * c - beta),
                          (A + 1.0) - (A - 1.0) * c + beta,
                          2.0 * ((A - 1.0) - (A + 1.0) * c),
                          (A + 1.0) - (A - 1.0) * c - beta);
                    break;
                }
                case 0: // Bell (Peaking)
                default: {
                    const double A = std::pow(10.0, b.gain / 40.0);
                    f.set(1.0 + alpha * A, -2.0 * c, 1.0 - alpha * A, 1.0 + alpha / A, -2.0 * c, 1.0 - alpha / A);
                    break;
                }
            }
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
