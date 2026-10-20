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
    juce::String report="genre\tbpm\tbars\tseconds\tpeak\traw_peak\trms\tmin_phrase_rms\tfirst_phrase_rms\tlast_phrase_rms\n";
    bool ok=true;
    for(int genre=0;genre<13;++genre){
        auto p=newProject(genre);
        const auto name=juce::String("club-")+juce::String(genre);
        std::cout<<"Rendering complete club arrangement "<<genre<<" at "<<p.settings.bpm<<" BPM"<<std::endl;
        const auto midi=midiFile(p);
        ok=folder.getChildFile(name+".mid").replaceWithData(midi.data(),midi.size())&&ok;
        ok=saveProject(p,folder.getChildFile(name+".fga")).wasOk()&&ok;
        const auto wav=folder.getChildFile("render-check.wav");
        double rawPeak=0;
        const auto result=audio.exportAudio(p,wav,-1,false,[]{return false;},[](double){},&rawPeak);
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
        ok=rawPeak<.9985&&peak<.9985&&rms>.001&&minimum>.00001&&first>.0001&&last>.0001&&ok;
        report+=juce::String(genres()[(size_t)genre])+"\t"+juce::String(p.settings.bpm)+"\t"+juce::String(p.length()/4)+"\t"+juce::String(seconds,4)+"\t"+juce::String(peak,6)+"\t"+juce::String(rawPeak,6)+"\t"+juce::String(rms,6)+"\t"+juce::String(minimum,6)+"\t"+juce::String(first,6)+"\t"+juce::String(last,6)+"\n";
        folder.getChildFile("full-song-audio.tsv").replaceWithText(report);
        // Listen to the same eight-bar chorus window in every style, with
        // actual stored MIDI and guide presets (not an unrelated demo synth).
        {
            int section=0;for(size_t si=0;si<p.sections.size();++si)if(p.sections[si].type==Section::drop){section=(int)si;break;}
            auto preview=p;preview.sections={p.sections[(size_t)section]};preview.sections[0].bars=8;
            for(int ti=0;ti<trackCount;++ti)preview.tracks[(size_t)ti].clips={p.tracks[(size_t)ti].clips[(size_t)section]};
            preview.loop=false;preview.normalise();
            auto result=audio.exportAudio(preview,folder.getChildFile("preview-"+juce::String(genre)+".mp3"),-1,true,[]{return false;},[](double){});
            ok=result.wasOk()&&ok;
            if(genre==4){
                auto drums=preview;for(int ti=0;ti<4;++ti)drums.tracks[(size_t)ti].mute=true;
                ok=audio.exportAudio(drums,folder.getChildFile("Jackin-drums.mp3"),-1,true,[]{return false;},[](double){}).wasOk()&&ok;
                ok=audio.exportAudio(preview,folder.getChildFile("Jackin-bass.mp3"),1,true,[]{return false;},[](double){}).wasOk()&&ok;
                ok=audio.exportAudio(preview,folder.getChildFile("Jackin-keys.mp3"),0,true,[]{return false;},[](double){}).wasOk()&&ok;
            }
            if(genre==1||genre==2||genre==6||genre==8){
                const auto bassFile=folder.getChildFile("bass-check.wav");
                ok=audio.exportAudio(preview,bassFile,1,false,[]{return false;},[](double){}).wasOk()&&ok;
                auto bassReader=std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(bassFile));
                double side=0,mid=0;if(!bassReader)ok=false;else{juce::AudioBuffer<float> b(2,8192);for(juce::int64 pos=0;pos<bassReader->lengthInSamples;pos+=8192){int n=(int)std::min<juce::int64>(8192,bassReader->lengthInSamples-pos);bassReader->read(&b,0,n,pos,true,true);for(int i=0;i<n;++i){const double l=b.getSample(0,i),r=b.getSample(1,i);side+=(l-r)*(l-r);mid+=(l+r)*(l+r);}}ok=mid>0&&side<mid*1e-10&&ok;}
                bassReader.reset();bassFile.deleteFile();
            }
        }
        if(genre==1||genre==4){
            const auto mp3=audio.exportAudio(p,folder.getChildFile(genre==1?"Soulful-House-full-song.mp3":"Jackin-full-song.mp3"),-1,true,[]{return false;},[](double){});
            ok=mp3.wasOk()&&ok;
        }
    }
    folder.getChildFile("audio-audit.txt").replaceWithText(ok?"PASS all 13 complete songs, finite audio, phrase continuity, genre tempo and headroom":"FAIL full-song rendering");
    return ok;
}
}
