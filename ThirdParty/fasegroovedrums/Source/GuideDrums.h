// Fase Groove Drums - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

// The guide drum kit: every piece synthesized (no samples), so the groove can be heard
// before it is dragged into the DAW and played with your own drums.
// Four kits: House 909, Deep 808, Disco Live, Lo-Fi Soul.

#include "engine/Drums.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>
#include <memory>

/** A drum's own sample, loaded by the user in place of the built-in sound. */
struct DrumSample
{
    juce::AudioBuffer<float> audio;
    double sampleRate = 44100.0;
    juce::String path;
};
using DrumSamples = std::array<std::shared_ptr<const DrumSample>, fgd::numLanes>;

class GuideDrums
{
public:
    static constexpr int numKits = 4;
    static const char* kitName (int kit) noexcept
    {
        static const char* names[] { "House 909", "Deep 808", "Disco Live", "Lo-Fi Soul" };
        return kit >= 0 && kit < numKits ? names[kit] : "";
    }

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        for (auto& v : voices)
            v.active = false;
        toneL = toneR = 0.0f;
    }

    void setKit (int k) noexcept     { kit = juce::jlimit (0, numKits - 1, k); }
    void setTone (float t) noexcept  { tone = juce::jlimit (0.0f, 1.0f, t); }

    void allNotesOff() noexcept
    {
        for (auto& v : voices)
            v.active = false;
    }

    /** Plays the note-ons of `midi` (notes found through `notes`, the lanes' MIDI notes, or the
        General MIDI drum map) and adds the sound to the buffer. */
    void render (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, const std::array<int, fgd::numLanes>& notes,
                 const std::array<float, fgd::numLanes>& laneVolume, const DrumSamples& samples) noexcept
    {
        currentSamples = &samples;
        const int numSamples = buffer.getNumSamples();
        int position = 0;

        for (const auto meta : midi)
        {
            const auto m = meta.getMessage();
            const int at = juce::jlimit (0, numSamples, meta.samplePosition);
            renderVoices (buffer, position, at - position);
            position = at;

            if (m.isNoteOn())
                trigger (m.getNoteNumber(), m.getFloatVelocity(), notes, laneVolume);
            else if (m.isAllNotesOff() || m.isAllSoundOff())
                allNotesOff();
        }

        renderVoices (buffer, position, numSamples - position);
        currentSamples = nullptr;
    }

private:
    //==============================================================================
    struct Svf
    {
        float ic1 = 0.0f, ic2 = 0.0f, g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;

        void set (float freq, float q, double sr) noexcept
        {
            g = std::tan (juce::MathConstants<float>::pi * juce::jlimit (20.0f, (float) sr * 0.45f, freq) / (float) sr);
            k = 1.0f / q;
            a1 = 1.0f / (1.0f + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        // Returns low, band, high.
        void process (float x, float& lp, float& bp, float& hp) noexcept
        {
            const float v3 = x - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;
            lp = v2; bp = v1; hp = x - k * v1 - v2;
        }
        float highPass (float x) noexcept { float l, b, h; process (x, l, b, h); return h; }
        float bandPass (float x) noexcept { float l, b, h; process (x, l, b, h); return b; }
    };

    struct Kit
    {
        float kickLow, kickHigh, kickPitchTime, kickDecay, kickClick, kickDrive;
        float snareTone, snareNoiseDecay, snareNoiseCut, snareToneMix;
        float hatCut, closedDecay, openDecay, metal;
        float clapDecay, darkness;   // darkness: a gentle low-pass on the whole kit
    };

    static const Kit& kitParams (int k) noexcept
    {
        static const Kit kits[] {
            // House 909: punchy kick with a click, bright noisy snare, crisp hats
            { 50.0f, 190.0f, 0.025f, 0.32f, 0.55f, 2.0f,  190.0f, 0.17f, 1800.0f, 0.45f,  8000.0f, 0.040f, 0.32f, 0.55f, 0.16f, 0.0f },
            // Deep 808: long round kick, tonal snare, metallic hats
            { 46.0f, 112.0f, 0.050f, 0.85f, 0.15f, 1.2f,  230.0f, 0.12f, 2400.0f, 0.65f,  7500.0f, 0.035f, 0.50f, 0.85f, 0.20f, 0.0f },
            // Disco Live: short beater kick, fat snare, airy hats
            { 58.0f, 125.0f, 0.020f, 0.22f, 0.80f, 1.5f,  200.0f, 0.22f, 1200.0f, 0.40f,  6000.0f, 0.060f, 0.45f, 0.30f, 0.18f, 0.15f },
            // Lo-Fi Soul: saturated, darker, dusty
            { 52.0f, 140.0f, 0.030f, 0.30f, 0.30f, 3.0f,  180.0f, 0.15f, 1500.0f, 0.55f,  5000.0f, 0.050f, 0.30f, 0.50f, 0.15f, 0.55f },
        };
        return kits[juce::jlimit (0, numKits - 1, k)];
    }

    struct Voice
    {
        bool active = false;
        int lane = 0;
        float time = 0.0f, gain = 0.0f, panL = 0.7f, panR = 0.7f, pitch = 1.0f, choke = 1.0f;
        bool choking = false;
        std::shared_ptr<const DrumSample> sample;   // the user's own sound, when the drum has one
        double samplePos = 0.0, sampleStep = 1.0;
        double phase1 = 0.0, phase2 = 0.0;
        std::array<double, 6> metalPhase {};
        std::uint32_t noiseState = 22222u;
        Svf f1, f2, f3;
        float maxTime = 1.0f;
        unsigned age = 0;

        float noise() noexcept
        {
            noiseState ^= noiseState << 13; noiseState ^= noiseState >> 17; noiseState ^= noiseState << 5;
            return (float) noiseState * (2.0f / 4294967295.0f) - 1.0f;
        }
    };

    static float laneGain (int lane) noexcept
    {
        static const float g[] { 0.95f, 0.6f, 0.55f, 0.4f, 0.28f, 0.28f, 0.2f, 0.22f, 0.25f, 0.45f, 0.3f, 0.55f, 0.24f };
        return g[juce::jlimit (0, fgd::numLanes - 1, lane)];
    }

    static float lanePan (int lane) noexcept
    {
        static const float p[] { 0.0f, 0.0f, 0.05f, -0.15f, 0.25f, 0.25f, -0.3f, -0.35f, 0.35f, -0.25f, 0.2f, 0.0f, -0.2f };
        return p[juce::jlimit (0, fgd::numLanes - 1, lane)];
    }

    void trigger (int note, float velocity, const std::array<int, fgd::numLanes>& notes,
                  const std::array<float, fgd::numLanes>& laneVolume) noexcept
    {
        // The lane playing this note: the lanes' own notes first, then the General MIDI map
        // (a dragged clip played back, a keyboard...).
        int lane = -1, offset = 0;
        for (int l = 0; l < fgd::numLanes && lane < 0; ++l)
            if (notes[(size_t) l] == note)
                lane = l;
        if (lane < 0)
        {
            // A fill's tom or conga a few notes away from its lane's note.
            for (int l : { (int) fgd::Lane::tom, (int) fgd::Lane::conga })
                if (std::abs (note - notes[(size_t) l]) <= 6)
                {
                    lane = l;
                    offset = note - notes[(size_t) l];
                    break;
                }
        }
        if (lane < 0)
        {
            lane = fgd::laneForGeneralMidiNote (note);
            if (lane < 0)
                return;
            offset = note - fgd::defaultNote (lane);
        }

        // The closed hat chokes the open hat, like on a real hi-hat; not one that has only just
        // started (a closed hat on the same step, a few ms later with the humanize, would silence it).
        if (lane == (int) fgd::Lane::closedHat)
            for (auto& v : voices)
                if (v.active && v.lane == (int) fgd::Lane::openHat && secondsPlayed (v) > 0.04)
                    v.choking = true;

        // A drum keeps at most a few voices of its own: a long sample on fast 16ths fades its own
        // oldest hit out, instead of taking a voice of another drum (and cutting it short).
        {
            Voice* oldest = nullptr;
            int sounding = 0;
            for (auto& v : voices)
                if (v.active && v.lane == lane && ! v.choking)
                {
                    ++sounding;
                    if (oldest == nullptr || v.age > oldest->age)
                        oldest = &v;
                }
            if (sounding >= maxVoicesPerDrum && oldest != nullptr)
                oldest->choking = true;
        }

        Voice* voice = nullptr;
        for (auto& v : voices)
            if (! v.active) { voice = &v; break; }
        if (voice == nullptr)   // all busy: one already fading out, else the oldest
            for (auto& v : voices)
                if (voice == nullptr || (v.choking && ! voice->choking) || (v.choking == voice->choking && v.age > voice->age))
                    voice = &v;

        for (auto& v : voices)
            ++v.age;

        const auto& k = kitParams (kit);
        auto& v = *voice;
        v = Voice();
        v.active = true;
        v.lane = lane;
        v.noiseState = 0x9E3779B9u ^ (std::uint32_t) (note * 7919 + (int) (velocity * 1000.0f) + (int) (++triggerCount));
        v.gain = std::pow (juce::jlimit (0.0f, 1.0f, velocity), 1.3f) * laneGain (lane) * juce::jlimit (0.0f, 4.0f, laneVolume[(size_t) lane]);
        const float pan = juce::jlimit (-1.0f, 1.0f, lanePan (lane) + (lane == (int) fgd::Lane::tom ? (float) offset * 0.05f : 0.0f));
        v.panL = std::cos ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
        v.panR = std::sin ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);

        // The user's own sample: played as it is (tuned for the fill's toms and congas).
        if (currentSamples != nullptr)
            if (auto s = (*currentSamples)[(size_t) lane]; s != nullptr && s->audio.getNumSamples() > 0)
            {
                v.sample = std::move (s);
                v.gain = std::pow (juce::jlimit (0.0f, 1.0f, velocity), 1.3f) * 0.9f * juce::jlimit (0.0f, 4.0f, laneVolume[(size_t) lane]);
                v.sampleStep = v.sample->sampleRate / sampleRate * std::pow (2.0, (double) offset / 12.0);
                v.maxTime = (float) (v.sample->audio.getNumSamples() / v.sample->sampleRate) + 0.05f;
                return;
            }

        const double sr = sampleRate;
        switch ((fgd::Lane) lane)
        {
            case fgd::Lane::kick:       v.maxTime = k.kickDecay * 7.0f; break;
            case fgd::Lane::snare:      v.maxTime = 0.6f; v.f1.set (k.snareNoiseCut, 0.7f, sr); v.f2.set (5200.0f, 0.8f, sr); break;
            case fgd::Lane::clap:       v.maxTime = k.clapDecay * 7.0f; v.f1.set (1150.0f, 1.6f, sr); v.f2.set (700.0f, 0.7f, sr); break;
            case fgd::Lane::rim:        v.maxTime = 0.12f; v.f1.set (3000.0f, 0.7f, sr); break;
            case fgd::Lane::closedHat:  v.maxTime = k.closedDecay * 8.0f; v.f1.set (k.hatCut, 0.7f, sr); v.f2.set (k.hatCut, 0.7f, sr); break;
            case fgd::Lane::openHat:    v.maxTime = k.openDecay * 7.0f; v.f1.set (k.hatCut, 0.7f, sr); v.f2.set (k.hatCut, 0.7f, sr); break;
            case fgd::Lane::ride:       v.maxTime = 4.0f; v.f1.set (5000.0f, 0.8f, sr); v.f2.set (3000.0f, 0.7f, sr); break;
            case fgd::Lane::shaker:     v.maxTime = 0.35f; v.f1.set (6500.0f, 0.7f, sr); v.f2.set (6500.0f, 0.7f, sr); break;
            case fgd::Lane::tambourine: v.maxTime = 0.6f; v.f1.set (9000.0f, 2.0f, sr); v.f2.set (6000.0f, 0.7f, sr); break;
            case fgd::Lane::conga:      v.maxTime = 0.9f; v.pitch = std::pow (2.0f, -4.0f * (float) offset / 12.0f); v.f1.set (2200.0f, 1.0f, sr); break;
            case fgd::Lane::cowbell:    v.maxTime = 1.0f; v.f1.set (1100.0f, 1.2f, sr); break;
            case fgd::Lane::tom:        v.maxTime = 1.6f; v.pitch = std::pow (2.0f, 1.5f * (float) offset / 12.0f); v.f1.set (3000.0f, 0.7f, sr); break;
            case fgd::Lane::crash:      v.maxTime = 5.0f; v.f1.set (4000.0f, 0.7f, sr); v.f2.set (4000.0f, 0.7f, sr); break;
        }
        v.f3.set (300.0f, 0.7f, sr);
    }

    float metal (Voice& v, float scale) noexcept
    {
        // Six square waves at inharmonic ratios: the classic drum machine cymbal.
        static constexpr double freqs[] { 205.3, 304.4, 369.6, 522.7, 540.0, 800.0 };
        float sum = 0.0f;
        for (size_t i = 0; i < 6; ++i)
        {
            v.metalPhase[i] += freqs[i] * scale / sampleRate;
            v.metalPhase[i] -= std::floor (v.metalPhase[i]);
            sum += v.metalPhase[i] < 0.5 ? 1.0f : -1.0f;
        }
        return sum * (1.0f / 6.0f);
    }

    float sine (double& phase, double freq) noexcept
    {
        phase += freq / sampleRate;
        phase -= std::floor (phase);
        return std::sin ((float) (phase * juce::MathConstants<double>::twoPi));
    }

    float renderVoice (Voice& v) noexcept
    {
        const auto& k = kitParams (kit);
        const float t = v.time;
        float out = 0.0f;

        switch ((fgd::Lane) v.lane)
        {
            case fgd::Lane::kick:
            {
                const double f = k.kickLow + (k.kickHigh - k.kickLow) * std::exp (-t / k.kickPitchTime);
                const float body = sine (v.phase1, f) * std::exp (-t / k.kickDecay);
                const float click = v.noise() * std::exp (-t / 0.0015f) * k.kickClick;
                out = std::tanh (k.kickDrive * (body + click)) / std::tanh (k.kickDrive);
                break;
            }
            case fgd::Lane::snare:
            {
                const double f = k.snareTone * (1.0 + 0.35 * std::exp (-t / 0.008));
                const float toneOut = (0.65f * sine (v.phase1, f) + 0.35f * sine (v.phase2, f * 1.78)) * std::exp (-t / 0.07f);
                const float n = v.f2.bandPass (v.f1.highPass (v.noise())) * 2.2f * std::exp (-t / k.snareNoiseDecay);
                out = k.snareToneMix * toneOut + (1.0f - k.snareToneMix * 0.5f) * n;
                break;
            }
            case fgd::Lane::clap:
            {
                // Three quick slaps, then the room.
                float env = 0.0f;
                for (int i = 0; i < 3; ++i)
                    if (const float d = t - 0.0105f * (float) i; d >= 0.0f)
                        env += std::exp (-d / 0.0035f);
                if (t > 0.021f)
                    env += 0.75f * std::exp (-(t - 0.021f) / k.clapDecay);
                out = v.f2.highPass (v.f1.bandPass (v.noise())) * 2.6f * env;
                break;
            }
            case fgd::Lane::rim:
            {
                const float body = (0.6f * sine (v.phase1, 1700.0) + 0.5f * sine (v.phase2, 480.0)) * std::exp (-t / 0.011f);
                out = body + v.f1.highPass (v.noise()) * std::exp (-t / 0.0025f) * 0.6f;
                break;
            }
            case fgd::Lane::closedHat:
            case fgd::Lane::openHat:
            {
                const float src = k.metal * metal (v, 1.0f) + (1.0f - k.metal * 0.6f) * v.noise();
                const float decay = v.lane == (int) fgd::Lane::closedHat ? k.closedDecay : k.openDecay;
                out = v.f2.highPass (v.f1.highPass (src)) * std::exp (-t / decay) * 1.6f;
                break;
            }
            case fgd::Lane::ride:
            {
                const float src = 0.8f * metal (v, 1.55f) + 0.25f * v.noise();
                const float wash = v.f2.highPass (v.f1.bandPass (src)) * std::exp (-t / 1.1f) * 1.6f;
                out = wash + 0.12f * sine (v.phase1, 3400.0) * std::exp (-t / 0.6f);
                break;
            }
            case fgd::Lane::shaker:
            {
                const float env = (1.0f - std::exp (-t / 0.007f)) * std::exp (-t / 0.045f);
                out = v.f2.highPass (v.f1.highPass (v.noise())) * env * 2.4f;
                break;
            }
            case fgd::Lane::tambourine:
            {
                float env = std::exp (-t / 0.055f);
                if (t > 0.018f) env += 0.55f * std::exp (-(t - 0.018f) / 0.05f);
                const float jingles = v.f1.bandPass (v.noise()) * 2.0f + 0.4f * v.f2.highPass (metal (v, 3.1f));
                out = jingles * env;
                break;
            }
            case fgd::Lane::conga:
            {
                const double f = 330.0 * v.pitch * (1.0 + 0.12 * std::exp (-t / 0.01));
                out = sine (v.phase1, f) * std::exp (-t / 0.17f) + v.f1.bandPass (v.noise()) * std::exp (-t / 0.006f) * 0.8f;
                break;
            }
            case fgd::Lane::cowbell:
            {
                v.phase1 += 540.0 / sampleRate; v.phase1 -= std::floor (v.phase1);
                v.phase2 += 800.0 / sampleRate; v.phase2 -= std::floor (v.phase2);
                const float sq = (v.phase1 < 0.5 ? 0.5f : -0.5f) + (v.phase2 < 0.5 ? 0.5f : -0.5f);
                out = v.f1.bandPass (sq) * 1.6f * (0.6f * std::exp (-t / 0.018f) + 0.4f * std::exp (-t / 0.22f));
                break;
            }
            case fgd::Lane::tom:
            {
                const double f = 105.0 * v.pitch * (1.0 + 0.45 * std::exp (-t / 0.03));
                out = sine (v.phase1, f) * std::exp (-t / 0.32f) + v.f1.bandPass (v.noise()) * std::exp (-t / 0.008f) * 0.3f;
                break;
            }
            case fgd::Lane::crash:
            {
                const float src = 0.5f * metal (v, 1.9f) + 0.65f * v.noise();
                const float env = (1.0f - std::exp (-t / 0.002f)) * std::exp (-t / 1.5f);
                out = v.f2.highPass (v.f1.highPass (src)) * env * 1.5f;
                break;
            }
        }

        if (v.choking)
        {
            v.choke *= chokeCoefficient;
            out *= v.choke;
            if (v.choke < 0.001f)
                v.active = false;
        }

        v.time += 1.0f / (float) sampleRate;
        if (v.time > v.maxTime)
            v.active = false;
        return out * v.gain;
    }

    static double secondsPlayed (const Voice& v) noexcept
    {
        if (v.sample != nullptr)
            return v.sample->sampleRate > 0.0 ? v.samplePos / v.sample->sampleRate : 0.0;
        return (double) v.time;
    }

    /** The user's sample, linearly interpolated; false when it has ended. */
    bool renderSample (Voice& v, float& l, float& r) noexcept
    {
        const auto& audio = v.sample->audio;
        const int n = audio.getNumSamples();
        const int i = (int) v.samplePos;
        if (i >= n - 1)
            return false;
        const float frac = (float) (v.samplePos - i);
        auto read = [&] (int ch) { const float* d = audio.getReadPointer (ch); return d[i] + (d[i + 1] - d[i]) * frac; };
        float a = read (0), b = audio.getNumChannels() > 1 ? read (1) : a;
        v.samplePos += v.sampleStep;

        float g = v.gain;
        if (v.choking)
        {
            v.choke *= chokeCoefficient;
            g *= v.choke;
            if (v.choke < 0.001f)
                return false;
        }
        // Mono samples are placed like the built-in drum; stereo ones keep their own image.
        if (audio.getNumChannels() > 1) { l += a * g; r += b * g; }
        else                            { l += a * g * v.panL * 1.41f; r += a * g * v.panR * 1.41f; }
        return true;
    }

    void renderVoices (juce::AudioBuffer<float>& buffer, int start, int count) noexcept
    {
        if (count <= 0)
            return;

        chokeCoefficient = std::exp (-1.0f / (0.008f * (float) sampleRate));
        const auto& k = kitParams (kit);

        // Tone: a gentle low-pass over the kit (the Lo-Fi kit is darker to start with).
        const float cutoff = 1500.0f * std::pow (13.0f, juce::jlimit (0.0f, 1.0f, tone * (1.0f - 0.45f * k.darkness)));
        const float toneCoeff = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * cutoff / (float) sampleRate);
        const bool toneOn = cutoff < 19000.0f;

        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

        for (int i = start; i < start + count; ++i)
        {
            float l = 0.0f, r = 0.0f;
            for (auto& v : voices)
                if (v.active && v.sample != nullptr)
                {
                    if (! renderSample (v, l, r))
                    {
                        v.active = false;
                        v.sample.reset();
                    }
                }
                else if (v.active)
                {
                    const float s = renderVoice (v);
                    l += s * v.panL;
                    r += s * v.panR;
                }

            if (toneOn)
            {
                toneL += (l - toneL) * toneCoeff;
                toneR += (r - toneR) * toneCoeff;
                l = toneL;
                r = toneR;
            }

            if (right != nullptr)
            {
                left[i] += l;
                right[i] += r;
            }
            else
                left[i] += 0.5f * (l + r);
        }
    }

    double sampleRate = 44100.0;
    int kit = 0;
    float tone = 0.85f, toneL = 0.0f, toneR = 0.0f, chokeCoefficient = 0.99f;
    unsigned triggerCount = 0;
    const DrumSamples* currentSamples = nullptr;
    static constexpr int maxVoicesPerDrum = 4;
    std::array<Voice, 32> voices {};
};

