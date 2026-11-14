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
        double tb0 = 1, tb1 = 0, tb2 = 0, ta1 = 0, ta2 = 0;
        double z1[2]{}, z2[2]{};
        int ramp = 0;
        bool used = false;
        int type = -1;
        double freq = 0;
        float run(float x, int ch) {
            const double y = b0 * x + z1[ch];
            z1[ch] = b1 * x - a1 * y + z2[ch];
            z2[ch] = b2 * x - a2 * y;
            return (float)y;
        }
        void advance() {
            if (ramp <= 0) return;
            const double t = 1.0 / ramp;
            b0 += (tb0 - b0) * t; b1 += (tb1 - b1) * t; b2 += (tb2 - b2) * t;
            a1 += (ta1 - a1) * t; a2 += (ta2 - a2) * t;
            if (--ramp == 0) { b0 = tb0; b1 = tb1; b2 = tb2; a1 = ta1; a2 = ta2; }
        }
        void snap(const EqCoefficients& c) {
            b0 = tb0 = c.b0; b1 = tb1 = c.b1; b2 = tb2 = c.b2; a1 = ta1 = c.a1; a2 = ta2 = c.a2;
            ramp = 0;
        }
        void target(const EqCoefficients& c, int samples) {
            if (samples <= 1) { snap(c); return; }
            tb0 = c.b0; tb1 = c.b1; tb2 = c.b2; ta1 = c.a1; ta2 = c.a2;
            if (std::abs(tb0 - b0) < 1e-12 && std::abs(tb1 - b1) < 1e-12 && std::abs(tb2 - b2) < 1e-12
                && std::abs(ta1 - a1) < 1e-12 && std::abs(ta2 - a2) < 1e-12) { ramp = 0; return; }
            ramp = samples;
        }
        void reset() {
            z1[0] = z1[1] = 0;
            z2[0] = z2[1] = 0;
        }
    };
private:
    Filter filters[maxFilters];
    int activeCount = 0;
    int rampSamples = 1;
    bool active = false;
public:
    bool engaged() const { return active && activeCount > 0; }
    void reset() {
        for (auto& f : filters) { f.reset(); f.used = false; f.ramp = 0; f.type = -1; f.freq = 0; }
    }
    void configure(const EqSettings& p, double rate) {
        active = p.enabled;
        const double sr = std::max(8000., rate);
        rampSamples = std::max(1, (int)std::lround(sr * 0.004));
        activeCount = 0;
        if (!active) return;
        const auto append=[&](EqBand b){
            b.normalise();if (!b.enabled) return;
            const auto c=eqCoefficients(b,sr);
            auto& f=filters[activeCount++];
            const double ratio=f.freq>0?b.freq/f.freq:1;
            const bool moved=!f.used||f.type!=b.type||ratio>4.||ratio<.25;
            f.used=true;f.type=b.type;f.freq=b.freq;
            if(moved){f.reset();f.snap(c);}else f.target(c,rampSamples);
        };
        if(p.bands.empty()){for(const auto& b:p.legacyBands())append(b);}
        else for(size_t i=0;i<std::min(p.bands.size(),(size_t)maxFilters);++i)append(p.bands[i]);
        for(int i=activeCount;i<maxFilters;++i){filters[i].used=false;filters[i].type=-1;filters[i].freq=0;filters[i].reset();filters[i].snap({1,0,0,0,0});}
    }
    void process(juce::AudioBuffer<float>& audio, int count) {
        if (!active || activeCount <= 0) return;
        count = std::min(count, audio.getNumSamples());
        const int channels = std::min(2, audio.getNumChannels());
        for (int n = 0; n < count; ++n) {
            for (int i = 0; i < activeCount; ++i) filters[i].advance();
            for (int ch = 0; ch < channels; ++ch) {
                auto* p = audio.getWritePointer(ch);
                for (int i = 0; i < activeCount; ++i) p[n] = filters[i].run(p[n], ch);
            }
        }
    }
};
}
