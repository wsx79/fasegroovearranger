// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "AudioEngine.h"
#include "ProjectIO.h"
#include <cmath>
#include <iostream>

namespace fga {
// Full-song integration evidence, separate from the short routing smoke test.
// Only original built-in instruments are used; no installed plugins are needed.
inline bool writeClubAudioAudit(const juce::File& folder){
    if(folder.createDirectory().failed())return false;
    AudioEngine audio;audio.devices.closeAudioDevice();
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    juce::String report="genre\tbpm\tbars\tseconds\tpeak\trms\tmin_phrase_rms\tfirst_phrase_rms\tlast_phrase_rms\n";
    bool ok=true;
    for(int genre=0;genre<13;++genre){
        auto p=newProject(genre);
        const auto name=juce::String("club-")+juce::String(genre);
        std::cout<<"Rendering complete club arrangement "<<genre<<" at "<<p.settings.bpm<<" BPM"<<std::endl;
        const auto midi=midiFile(p);
        ok=folder.getChildFile(name+".mid").replaceWithData(midi.data(),midi.size())&&ok;
        ok=saveProject(p,folder.getChildFile(name+".fga")).wasOk()&&ok;
        const auto wav=folder.getChildFile("render-check.wav");
        const auto result=audio.exportAudio(p,wav,-1,false,[]{return false;},[](double){});
        if(result.failed()){folder.getChildFile("error.txt").appendText(name+": "+result.getErrorMessage()+"\n");return false;}
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(wav));
        if(!reader||reader->numChannels!=2)return false;
        const double seconds=p.length()*60/p.settings.bpm;
        const auto musicFrames=(juce::int64)std::ceil(seconds*reader->sampleRate);
        ok=reader->lengthInSamples==(juce::int64)std::ceil((seconds+3)*reader->sampleRate)&&ok;
        const int phraseCount=(int)(p.length()/32);
        std::vector<double> energies((size_t)phraseCount,0);
        std::vector<juce::int64> counts((size_t)phraseCount,0);
        double energy=0,peak=0;juce::int64 count=0;
        juce::AudioBuffer<float> block(2,8192);
        for(juce::int64 at=0;at<reader->lengthInSamples;at+=8192){
            const int n=(int)std::min<juce::int64>(8192,reader->lengthInSamples-at);
            if(!reader->read(&block,0,n,at,true,true))return false;
            for(int sample=0;sample<n;++sample){
                const int phrase=std::min(phraseCount-1,(int)((at+sample)/reader->sampleRate*p.settings.bpm/60/32));
                for(int ch=0;ch<2;++ch){
                    const double v=block.getSample(ch,sample);
                    if(!std::isfinite(v))return false;
                    peak=std::max(peak,std::abs(v));
                    if(at+sample<musicFrames){energy+=v*v;++count;energies[(size_t)phrase]+=v*v;++counts[(size_t)phrase];}
                }
            }
        }
        reader.reset();if(!wav.deleteFile())return false;
        double minimum=1;for(int i=0;i<phraseCount;++i)minimum=std::min(minimum,std::sqrt(energies[(size_t)i]/std::max<juce::int64>(1,counts[(size_t)i])));
        const double rms=std::sqrt(energy/std::max<juce::int64>(1,count));
        const double first=std::sqrt(energies.front()/std::max<juce::int64>(1,counts.front()));
        const double last=std::sqrt(energies.back()/std::max<juce::int64>(1,counts.back()));
        // Detect clipped guide mixes and accidental whole-phrase silence.
        ok=peak<.9985&&rms>.001&&minimum>.00001&&first>.0001&&last>.0001&&ok;
        report+=juce::String(genres()[(size_t)genre])+"\t"+juce::String(p.settings.bpm)+"\t"+juce::String(p.length()/4)+"\t"+juce::String(seconds,4)+"\t"+juce::String(peak,6)+"\t"+juce::String(rms,6)+"\t"+juce::String(minimum,6)+"\t"+juce::String(first,6)+"\t"+juce::String(last,6)+"\n";
        folder.getChildFile("full-song-audio.tsv").replaceWithText(report);
        if(genre==1){
            const auto mp3=audio.exportAudio(p,folder.getChildFile("Soulful-House-full-song.mp3"),-1,true,[]{return false;},[](double){});
            ok=mp3.wasOk()&&ok;
        }
    }
    folder.getChildFile("audio-audit.txt").replaceWithText(ok?"PASS all 13 complete songs, finite audio, phrase continuity, genre tempo and headroom":"FAIL full-song rendering");
    return ok;
}
}
