// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
// Tempo a loop carries with it: the ACID chunk of a WAV (tempo, or beats over its length),
// the beat count of an AIFF Apple loop, or a tempo in the file name ("Groove 124bpm.wav").
// Read on the import worker; analysis (AudioAnalysis.h) is only used when none is present.
#include <juce_audio_formats/juce_audio_formats.h>
#include <regex>
namespace fga {
struct LoopTempoInfo {double bpm=0;juce::String source;};
inline double loopTempoFromName(const juce::String& name){
    const std::regex pattern(R"((\d{2,3}(?:[.,]\d{1,2})?)\s*[-_ ]?\s*bpm)",std::regex::icase);std::smatch m;const auto text=name.toStdString();
    if(std::regex_search(text,m,pattern)){const double bpm=juce::String(m[1].str()).replaceCharacter(',','.').getDoubleValue();if(bpm>=40&&bpm<=250)return bpm;}
    return 0;
}
inline LoopTempoInfo readLoopTempo(const juce::File& file){
    LoopTempoInfo info;auto valid=[](double bpm){return std::isfinite(bpm)&&bpm>=40&&bpm<=250;};
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    if(std::unique_ptr<juce::AudioFormatReader> reader{formats.createReaderFor(file)}){
        const auto& m=reader->metadataValues;const double seconds=reader->sampleRate>0?(double)reader->lengthInSamples/reader->sampleRate:0;
        const bool oneShot=m.getValue(juce::WavAudioFormat::acidOneShot,"0").getIntValue()!=0||m.getValue(juce::AiffAudioFormat::appleOneShot,"0").getIntValue()!=0;
        if(!oneShot){
            const double acid=m.getValue(juce::WavAudioFormat::acidTempo,"0").getDoubleValue();
            const int beats=std::max(m.getValue(juce::WavAudioFormat::acidBeats,"0").getIntValue(),m.getValue(juce::AiffAudioFormat::appleBeats,"0").getIntValue());
            if(valid(acid)){info.bpm=acid;info.source="ACID";}
            else if(beats>0&&seconds>0&&valid(beats*60./seconds)){info.bpm=beats*60./seconds;info.source=m.containsKey(juce::WavAudioFormat::acidBeats)?"ACID":"Apple Loop";}
        }
    }
    if(info.bpm<=0){if(const double named=loopTempoFromName(file.getFileNameWithoutExtension());named>0){info.bpm=named;info.source="file name";}}
    if(info.bpm>0&&std::abs(info.bpm-std::round(info.bpm))<.02)info.bpm=std::round(info.bpm);
    return info;
}
}
