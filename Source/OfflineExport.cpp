// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "OfflineExport.h"
#include "LoudnessMeter.h"
#include <deque>
namespace fga {
namespace {
constexpr int chunk=4096;
using Cancel=std::function<bool()>;
using Progress=std::function<void(double)>;
juce::Result stopped(){return juce::Result::fail("Export cancelled");}
std::unique_ptr<juce::AudioFormatReader> openWav(const juce::File& file){
    juce::WavAudioFormat format;
    return std::unique_ptr<juce::AudioFormatReader>(format.createReaderFor(file.createInputStream().release(),true));
}
std::unique_ptr<juce::AudioFormatWriter> createWav(const juce::File& file,double rate,int bits,const juce::StringPairArray& metadata){
    auto stream=file.createOutputStream();
    if(!stream||!stream->setPosition(0)||stream->truncate().failed())return {};
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(),rate,2,(unsigned)bits,metadata,0));
    if(writer)stream.release();return writer;
}
// Shared with the historic guard: same wide-passband 16x estimator, fixed storage.
juce::Result scanPeak(juce::AudioFormatReader& reader,double& peak,const Cancel& cancelled,const Progress& progress){
    if(reader.numChannels!=2||reader.lengthInSamples<0)return juce::Result::fail("Expected stereo WAV");
    juce::dsp::Oversampling<float> os(2);os.clearOversamplingStages();
    for(int stage=0;stage<4;++stage)
        os.addOversamplingStage(juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,stage==0?.01f:.1f,-100.f,.1f,-100.f);
    os.initProcessing(chunk);juce::AudioBuffer<float> buffer(2,chunk);peak=0;
    const auto total=reader.lengthInSamples+chunk;
    for(juce::int64 at=0;at<total;at+=chunk){
        if(cancelled())return stopped();buffer.clear();
        if(at<reader.lengthInSamples&&!reader.read(&buffer,0,chunk,at,true,true))return juce::Result::fail("Cannot read export staging audio");
        for(int ch=0;ch<2;++ch)for(int s=0;s<chunk;++s){
            const float value=buffer.getSample(ch,s);if(!std::isfinite(value))return juce::Result::fail("Non-finite export staging audio");
            peak=std::max(peak,std::abs((double)value));
        }
        const auto high=os.processSamplesUp(juce::dsp::AudioBlock<float>(buffer));
        for(size_t ch=0;ch<2;++ch)for(size_t s=0;s<high.getNumSamples();++s){
            const float value=high.getChannelPointer(ch)[s];if(!std::isfinite(value))return juce::Result::fail("Non-finite reconstructed export peak");
            peak=std::max(peak,std::abs((double)value));
        }
        progress(std::min(1.,(double)(at+chunk)/total));
    }
    return juce::Result::ok();
}
juce::Result writeScaled(juce::AudioFormatReader& reader,const juce::File& target,int bits,double gain,const Cancel& cancelled,const Progress& progress){
    auto writer=createWav(target,reader.sampleRate,bits,reader.metadataValues);
    if(!writer)return juce::Result::fail("Cannot create encoded export WAV");
    juce::AudioBuffer<float> buffer(2,chunk);
    for(juce::int64 at=0;at<reader.lengthInSamples;at+=chunk){
        if(cancelled())return stopped();const int n=(int)std::min<juce::int64>(chunk,reader.lengthInSamples-at);
        if(!reader.read(&buffer,0,n,at,true,true))return juce::Result::fail("Cannot read export staging audio");
        for(int ch=0;ch<2;++ch)for(int s=0;s<n;++s){
            const double value=buffer.getSample(ch,s)*gain;
            if(!std::isfinite(value)||std::abs(value)>=1.)return juce::Result::fail("Export exceeds PCM range");
            buffer.setSample(ch,s,(float)value);
        }
        if(!writer->writeFromAudioSampleBuffer(buffer,0,n))return juce::Result::fail("Cannot write export WAV (disk space?)");
        progress((double)(at+n)/std::max<juce::int64>(1,reader.lengthInSamples));
    }
    return writer->flush()?juce::Result::ok():juce::Result::fail("Cannot finish export WAV");
}
// Offline-only candidate. No connection to playback/PDC. Admission is gated separately.
juce::Result renderLoud(juce::AudioFormatReader& reader,const juce::File& target,double ceilingDb,const Cancel& cancelled,const Progress& progress){
    constexpr int block=512,factor=4;
    juce::dsp::Oversampling<float> os(2,2,juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,true);os.initProcessing(block);
    const int ahead=(int)std::ceil(reader.sampleRate*.003),delay=ahead*factor;
    const int latency=ahead+(int)std::ceil(os.getLatencyInSamples());
    const double highRate=reader.sampleRate*factor,attack=std::exp(-1./(.0005*highRate)),release=std::exp(-1./(.100*highRate));
    const double ceiling=juce::Decibels::decibelsToGain(ceilingDb-.15);
    juce::AudioBuffer<float> ring(2,delay),buffer(2,block);ring.clear();int position=0;double gain=1;
    struct Peak {juce::int64 at;double value;};std::deque<Peak> peaks;juce::int64 clock=0;
    auto writer=createWav(target,reader.sampleRate,32,reader.metadataValues);
    if(!writer)return juce::Result::fail("Cannot create Loud staging WAV");
    const auto total=reader.lengthInSamples+latency;
    for(juce::int64 at=0;at<total;at+=block){
        if(cancelled())return stopped();const int n=(int)std::min<juce::int64>(block,total-at);buffer.clear();
        if(at<reader.lengthInSamples&&!reader.read(&buffer,0,(int)std::min<juce::int64>(n,reader.lengthInSamples-at),at,true,true))return juce::Result::fail("Cannot read Loud staging WAV");
        auto low=juce::dsp::AudioBlock<float>(buffer).getSubBlock(0,(size_t)n);auto high=os.processSamplesUp(low);
        for(size_t s=0;s<high.getNumSamples();++s,++clock){
            const double peak=std::max(std::abs((double)high.getSample(0,(int)s)),std::abs((double)high.getSample(1,(int)s)));
            while(!peaks.empty()&&peaks.back().value<=peak)peaks.pop_back();peaks.push_back({clock,peak});
            while(peaks.front().at<clock-delay)peaks.pop_front();
            const double required=std::min(1.,ceiling/std::max(1e-20,peaks.front().value));
            gain=required+(required<gain?attack:release)*(gain-required);
            for(int ch=0;ch<2;++ch){const float sample=ring.getSample(ch,position);ring.setSample(ch,position,high.getSample(ch,(int)s));high.setSample(ch,(int)s,(float)(sample*gain));}
            position=(position+1)%delay;
        }
        os.processSamplesDown(low);
        const int skip=(int)std::clamp<juce::int64>(latency-at,0,n);
        if(n>skip&&!writer->writeFromAudioSampleBuffer(buffer,skip,n-skip))return juce::Result::fail("Cannot write Loud staging WAV");
        progress((double)(at+n)/total);
    }
    return writer->flush()?juce::Result::ok():juce::Result::fail("Cannot finish Loud staging WAV");
}
}
juce::Result ExportPeakGuard::verifyFile(const juce::File& file,int bits,const Cancel& cancelled,double ceilingDb){
    auto reader=openWav(file);if(!reader)return juce::Result::fail("Cannot verify loudness guard");
    double peak=0;auto result=scanPeak(*reader,peak,cancelled,[](double){});if(result.failed())return result;
    const double ceiling=juce::Decibels::decibelsToGain(ceilingDb-.05);
    if(peak<=ceiling)return cancelled()?stopped():juce::Result::ok();
    juce::TemporaryFile temp(file);result=writeScaled(*reader,temp.getFile(),bits,ceiling/peak,cancelled,[](double){});
    reader.reset();if(result.failed())return result;if(cancelled())return stopped();
    return temp.overwriteTargetFileWithTemporary()?juce::Result::ok():juce::Result::fail("Cannot finish loudness guard");
}
juce::Result finishOfflineExport(const juce::File& source,const juce::File& output,int bits,AudioExportMode mode,double ceilingDb,const Cancel& cancelled,const Progress& progress,OfflineExportStats* stats){
    if(source==output||(bits!=16&&bits!=24)||!std::isfinite(ceilingDb)||ceilingDb<minExportCeilingDb||ceilingDb>maxExportCeilingDb||(mode!=AudioExportMode::transparent&&mode!=AudioExportMode::loud))return juce::Result::fail("Invalid offline export settings");
    juce::TemporaryFile limited(source);auto reader=openWav(source);
    if(!reader||!reader->usesFloatingPointData||reader->numChannels!=2||!std::isfinite(reader->sampleRate)||reader->sampleRate<8000||reader->sampleRate>192000)return juce::Result::fail("Expected stereo float export staging WAV at 8-192 kHz");
    const bool loud=mode==AudioExportMode::loud;
    auto scaled=[&](double from,double to){return [&,from,to](double value){progress(from+(to-from)*value);};};
    double inputPeak=0;auto result=scanPeak(*reader,inputPeak,cancelled,scaled(0,loud?.25:.5));if(result.failed())return result;
    double peak=inputPeak;
    if(loud){
        result=renderLoud(*reader,limited.getFile(),ceilingDb,cancelled,scaled(.25,.45));if(result.failed())return result;
        reader=openWav(limited.getFile());if(!reader)return juce::Result::fail("Cannot reopen Loud staging WAV");
        result=scanPeak(*reader,peak,cancelled,scaled(.45,.65));if(result.failed())return result;
    }
    const double ceiling=juce::Decibels::decibelsToGain(ceilingDb-.05),gain=peak>ceiling?ceiling/peak:1.;
    const auto frames=reader->lengthInSamples;
    result=writeScaled(*reader,output,bits,gain,cancelled,scaled(loud?.65:.5,.9));if(result.failed())return result;
    reader.reset();auto encoded=openWav(output);if(!encoded||encoded->lengthInSamples!=frames)return juce::Result::fail("Invalid encoded export WAV");
    double outputPeak=0;result=scanPeak(*encoded,outputPeak,cancelled,scaled(.9,1));if(result.failed())return result;
    if(outputPeak>juce::Decibels::decibelsToGain(ceilingDb))return juce::Result::fail("Encoded WAV exceeds True Peak ceiling");
    if(stats)*stats={inputPeak,outputPeak,gain,frames};
    return cancelled()?stopped():juce::Result::ok();
}
}
