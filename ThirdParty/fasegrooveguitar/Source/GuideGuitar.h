// Fase Groove Guitar - Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

// The guide guitar: plucked strings, synthesized (Karplus-Strong: a burst of noise in a tuned
// delay line that loses its highs as it rings), so the groove can be heard before it is
// dragged into the DAW and played with your own guitar instrument. Each lane has its touch:
// a muted scratch is a dead, percussive click, a swell fades in, a stab is short.
// Four tones: Clean Funk, Warm Jazz, Disco Chorus, Nylon.

#include "engine/Guitar.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>
#include <vector>

class GuideGuitar
{
public:
    static constexpr int numTones = 4;
    static const char* toneName (int tone) noexcept
    {
        static const char* names[] { "Clean Funk", "Warm Jazz", "Disco Chorus", "Nylon" };
        return tone >= 0 && tone < numTones ? names[tone] : "";
    }

    /** A note to play or stop, at a sample of the block. */
    struct Event
    {
        int sample = 0;
        int note = 60;
        float velocity = 0.0f;   // 0: note off
        int lane = 0;
    };

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        for (auto& v : voices)
        {
            v.line.assign ((size_t) lineSize, 0.0f);
            v.active = false;
        }
        chorusLine.assign ((size_t) chorusSize, 0.0f);
        chorusWrite = 0;
        chorusPhase = 0.0;
        toneL = toneR = dcInL = dcInR = dcOutL = dcOutR = 0.0f;
    }

    void setSound (int t) noexcept   { sound = juce::jlimit (0, numTones - 1, t); }
    void setTone (float t) noexcept  { tone = juce::jlimit (0.0f, 1.0f, t); }

    void allNotesOff() noexcept
    {
        for (auto& v : voices)
            v.active = false;
    }

    /** Plays the events (sorted by sample) and adds the sound to the buffer. */
    void render (juce::AudioBuffer<float>& buffer, const std::vector<Event>& events,
                 const std::array<float, fgg::numLanes>& laneVolume) noexcept
    {
        if (voices[0].line.empty())
            return;   // not prepared
        const int numSamples = buffer.getNumSamples();
        int position = 0;
        for (const auto& e : events)
        {
            const int at = juce::jlimit (0, numSamples, e.sample);
            renderVoices (buffer, position, at - position);
            position = at;
            if (e.note < 0)
                allNotesOff();
            else if (e.velocity > 0.0f)
                noteOn (e.note, e.velocity, e.lane, laneVolume[(size_t) juce::jlimit (0, fgg::numLanes - 1, e.lane)]);
            else
                noteOff (e.note);
        }
        renderVoices (buffer, position, numSamples - position);
    }

private:
    static constexpr int lineSize = 8192;    // E2 at 192 kHz fits (2335 samples)
    static constexpr int chorusSize = 4096;

    struct Sound
    {
        float sustain;      // seconds an open low string rings (to -60 dB)
        float brightness;   // the pick's attack, 0 dark .. 1 bright
        float damping;      // how fast the highs die in the string, 0 .. 1
        float pickPoint;    // where the string is plucked (fraction of its length)
        float chorus;       // stereo chorus amount
    };

    static const Sound& soundParams (int s) noexcept
    {
        static const Sound sounds[] {
            { 2.2f, 0.9f,  0.35f, 0.12f, 0.0f },    // Clean Funk: single coils, bright and snappy
            { 3.2f, 0.45f, 0.65f, 0.2f,  0.0f },    // Warm Jazz: a hollow body, the tone rolled off
            { 2.6f, 0.8f,  0.4f,  0.13f, 0.55f },   // Disco Chorus: bright, wide, shimmering
            { 2.4f, 0.35f, 0.55f, 0.25f, 0.1f },    // Nylon: soft fingers, round
        };
        return sounds[juce::jlimit (0, numTones - 1, s)];
    }

    struct Voice
    {
        std::vector<float> line;
        bool active = false, releasing = false;
        int note = 0, lane = 0, write = 0;
        double delay = 100.0;              // in samples
        float feedback = 0.99f, smooth = 0.5f, last = 0.0f;
        float gain = 1.0f, envelope = 1.0f, attack = 1.0f, attackStep = 0.0f, release = 0.999f;
        float panL = 0.7f, panR = 0.7f;
        unsigned age = 0;
    };

    void noteOn (int note, float velocity, int lane, float volume) noexcept
    {
        // The same string again: it restarts. Otherwise a free voice, or the oldest one.
        Voice* v = nullptr;
        for (auto& candidate : voices)
            if (candidate.active && candidate.note == note)
                v = &candidate;
        if (v == nullptr)
            for (auto& candidate : voices)
                if (! candidate.active)
                {
                    v = &candidate;
                    break;
                }
        if (v == nullptr)
        {
            v = &voices[0];
            for (auto& candidate : voices)
                if (candidate.age < v->age)
                    v = &candidate;
        }

        const auto& s = soundParams (sound);
        const auto l = (fgg::Lane) lane;
        const bool muted = l == fgg::Lane::scratch;
        const bool short_ = l == fgg::Lane::stab || l == fgg::Lane::chank;
        const double freq = 440.0 * std::pow (2.0, (note - 69) / 12.0);

        v->active = true;
        v->releasing = false;
        v->note = note;
        v->lane = lane;
        v->age = ++triggerCount;
        v->delay = juce::jlimit (2.0, (double) lineSize - 4.0, sampleRate / freq - 0.5);

        // How long it rings: the low strings longer; a muted string almost not at all.
        const float sustain = muted ? 0.045f : s.sustain * (short_ ? 0.35f : 1.0f) * (float) std::pow (110.0 / freq, 0.35);
        v->feedback = (float) std::pow (10.0, -3.0 / (juce::jmax (0.02f, sustain) * freq));
        v->smooth = juce::jlimit (0.05f, 0.95f, s.damping * 0.6f + (muted ? 0.35f : 0.0f) + (1.0f - tone) * 0.25f);
        v->last = 0.0f;
        v->gain = velocity * velocity * volume * (muted ? 0.7f : 0.45f);
        v->envelope = 1.0f;
        v->release = std::exp (-1.0f / (0.012f * (float) sampleRate));
        // A swell fades in, as with the volume knob.
        const bool swell = l == fgg::Lane::swell;
        v->attack = swell ? 0.0f : 1.0f;
        v->attackStep = swell ? 1.0f / (0.35f * (float) sampleRate) : 0.0f;

        // Low strings in the middle, high ones a little to the side; the chorus widens it.
        const float pan = juce::jlimit (-1.0f, 1.0f, (note - 58) / 40.0f) * 0.4f;
        v->panL = std::cos ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
        v->panR = std::sin ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);

        // The pluck: a burst of noise, darker when softer, with the notch of the pick's position.
        const int length = (int) std::ceil (v->delay) + 2;
        const float bright = juce::jlimit (0.05f, 1.0f, s.brightness * (0.5f + 0.5f * velocity) * (0.55f + 0.45f * tone) * (muted ? 0.6f : 1.0f));
        float lp = 0.0f;
        std::array<float, 16> history {};
        const int pick = juce::jlimit (1, 15, (int) std::round (s.pickPoint * 16.0f));
        for (int i = 0; i < length; ++i)
        {
            const float white = random.nextFloat() * 2.0f - 1.0f;
            lp += (white - lp) * bright;
            const float shaped = lp - 0.6f * history[(size_t) ((i + 16 - pick) % 16)];
            history[(size_t) (i % 16)] = lp;
            v->line[(size_t) ((v->write - length + i + lineSize * 2) % lineSize)] = shaped;
        }
    }

    void noteOff (int note) noexcept
    {
        for (auto& v : voices)
            if (v.active && v.note == note && ! v.releasing)
                v.releasing = true;   // the finger lifts: the string stops in a few ms
    }

    float renderVoice (Voice& v) noexcept
    {
        // Read the string one period back (fractional), smooth it (the loss of highs), write it on.
        double readPos = (double) v.write - v.delay;
        while (readPos < 0.0)
            readPos += lineSize;
        const int i0 = (int) readPos;
        const float frac = (float) (readPos - i0);
        const float a = v.line[(size_t) (i0 & (lineSize - 1))], b = v.line[(size_t) ((i0 + 1) & (lineSize - 1))];
        const float x = a + (b - a) * frac;
        const float y = (1.0f - v.smooth) * x + v.smooth * v.last;
        v.last = y;
        v.line[(size_t) v.write] = y * v.feedback;
        v.write = (v.write + 1) & (lineSize - 1);

        if (v.attack < 1.0f)
            v.attack = juce::jmin (1.0f, v.attack + v.attackStep);
        if (v.releasing)
        {
            v.envelope *= v.release;
            if (v.envelope < 0.0005f)
                v.active = false;
        }
        return x * v.gain * v.envelope * v.attack;
    }

    void renderVoices (juce::AudioBuffer<float>& buffer, int start, int count) noexcept
    {
        if (count <= 0)
            return;

        const auto& s = soundParams (sound);
        // Tone: the guitar's tone knob, a gentle low-pass over everything.
        const float cutoff = 1200.0f * std::pow (14.0f, tone);
        const float toneCoeff = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * cutoff / (float) sampleRate);
        const float dcCoeff = 1.0f - (float) (juce::MathConstants<double>::twoPi * 60.0 / sampleRate);
        const double chorusRate = 0.7 / sampleRate;

        auto* left = buffer.getWritePointer (0);
        auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

        for (int i = start; i < start + count; ++i)
        {
            float l = 0.0f, r = 0.0f;
            for (auto& v : voices)
                if (v.active)
                {
                    const float out = renderVoice (v);
                    l += out * v.panL;
                    r += out * v.panR;
                }

            toneL += (l - toneL) * toneCoeff;
            toneR += (r - toneR) * toneCoeff;
            // The DC a plucked string can leave, out.
            dcOutL = toneL - dcInL + dcCoeff * dcOutL;  dcInL = toneL;
            dcOutR = toneR - dcInR + dcCoeff * dcOutR;  dcInR = toneR;
            l = dcOutL;
            r = dcOutR;

            if (s.chorus > 0.0f)
            {
                // A chorus: the sound again a few ms later, the delay swaying, opposite on each side.
                chorusLine[(size_t) chorusWrite] = 0.5f * (l + r);
                chorusPhase += chorusRate;
                if (chorusPhase >= 1.0)
                    chorusPhase -= 1.0;
                const double sway = std::sin (chorusPhase * juce::MathConstants<double>::twoPi);
                auto tap = [this] (double ms)
                {
                    double pos = (double) chorusWrite - ms * 0.001 * sampleRate;
                    while (pos < 0.0)
                        pos += chorusSize;
                    const int i0 = (int) pos;
                    const float f = (float) (pos - i0);
                    const float a = chorusLine[(size_t) (i0 % chorusSize)], b = chorusLine[(size_t) ((i0 + 1) % chorusSize)];
                    return a + (b - a) * f;
                };
                l += s.chorus * tap (7.0 + 2.5 * sway);
                r += s.chorus * tap (7.0 - 2.5 * sway);
                chorusWrite = (chorusWrite + 1) % chorusSize;
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
    int sound = 0;
    float tone = 0.7f, toneL = 0.0f, toneR = 0.0f;
    float dcInL = 0.0f, dcInR = 0.0f, dcOutL = 0.0f, dcOutR = 0.0f;
    std::vector<float> chorusLine;
    int chorusWrite = 0;
    double chorusPhase = 0.0;
    unsigned triggerCount = 0;
    juce::Random random { 1979 };
    std::array<Voice, 40> voices {};
};

