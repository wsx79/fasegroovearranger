// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <juce_dsp/juce_dsp.h>
#include <atomic>
namespace fga {
// BS.1770 stereo K weighting, 400 ms blocks at 75% overlap, -70 LUFS / -10 LU gates.
class LoudnessMeter {
    std::array<juce::dsp::IIR::Filter<double>,2> shelf,hp;
    std::array<double,30> recent{};std::array<double,901> sums{};std::array<std::uint64_t,901> counts{};
    double sum=0,absoluteEnergy=0;std::uint64_t absoluteCount=0;int frames=0,block=4800,index=0,filled=0;
    static float lufs(double energy){return energy>1e-15?(float)(-.691+10*std::log10(energy)):-100.f;}
public:
    std::atomic<float> shortTerm{-100},integrated{-100};
    void prepare(double rate){rate=std::max(8000.,rate);block=std::max(1,(int)std::lround(rate*.1));for(int ch=0;ch<2;++ch){shelf[(size_t)ch].coefficients=juce::dsp::IIR::Coefficients<double>::makeHighShelf(rate,1681.9744509555,.7071752369554196,1.584864701130855);hp[(size_t)ch].coefficients=juce::dsp::IIR::Coefficients<double>::makeHighPass(rate,38.13547087602444,.5003270373238773);}reset();}
    void reset(){for(auto& f:shelf)f.reset();for(auto& f:hp)f.reset();recent.fill(0);sums.fill(0);counts.fill(0);sum=absoluteEnergy=0;absoluteCount=0;frames=index=filled=0;shortTerm=-100;integrated=-100;}
    void process(float* const* data,int channels,int count){for(int n=0;n<count;++n){for(int ch=0;ch<std::min(channels,2);++ch)if(data[ch]){const double y=hp[(size_t)ch].processSample(shelf[(size_t)ch].processSample(data[ch][n]));sum+=y*y;}if(++frames<block)continue;recent[(size_t)index]=sum/block;sum=0;frames=0;index=(index+1)%30;filled=std::min(30,filled+1);double shortEnergy=0;for(int i=0;i<filled;++i)shortEnergy+=recent[(size_t)i];shortTerm=lufs(shortEnergy/std::max(1,filled));if(filled<4)continue;double energy=0;for(int i=1;i<=4;++i)energy+=recent[(size_t)((index-i+30)%30)];energy/=4;const float level=lufs(energy);if(level< -70)continue;const int bin=std::clamp((int)std::floor((level+70)*10),0,900);sums[(size_t)bin]+=energy;++counts[(size_t)bin];absoluteEnergy+=energy;++absoluteCount;const float gate=lufs(absoluteEnergy/absoluteCount)-10;double total=0;std::uint64_t used=0;for(int i=std::clamp((int)std::ceil((gate+70)*10),0,900);i<=900;++i){total+=sums[(size_t)i];used+=counts[(size_t)i];}integrated=used?lufs(total/used):-100.f;}}
};
// 4x FIR oversampling places the ceiling on reconstructed peaks, not just PCM samples.
class ExportPeakGuard {
    juce::dsp::Oversampling<float> oversampling{2,2,juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,true};
public:
    void prepare(int block){oversampling.initProcessing((size_t)block);oversampling.reset();}
    void process(juce::AudioBuffer<float>& buffer,int count){auto block=juce::dsp::AudioBlock<float>(buffer).getSubBlock(0,(size_t)count);auto high=oversampling.processSamplesUp(block);constexpr float ceiling=.875f;for(size_t i=0;i<high.getNumSamples();++i){const float peak=std::max(std::abs(high.getSample(0,(int)i)),std::abs(high.getSample(1,(int)i)));const float gain=peak>ceiling?ceiling/peak:1.f;for(int ch=0;ch<2;++ch)high.getChannelPointer((size_t)ch)[i]*=gain;}oversampling.processSamplesDown(block);}
    int latency()const{return (int)std::ceil(oversampling.getLatencyInSamples());}
    static juce::Result verifyFile(const juce::File& file,int bits,const std::function<bool()>& cancelled){juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(file.createInputStream().release(),true));if(!reader)return juce::Result::fail("Cannot verify loudness guard");juce::dsp::Oversampling<float> os(2,2,juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,true);os.initProcessing(4096);juce::AudioBuffer<float> buffer(2,4096);float peak=0;for(juce::int64 at=0;at<reader->lengthInSamples+4096;at+=4096){if(cancelled())return juce::Result::fail("Export cancelled");buffer.clear();if(at<reader->lengthInSamples)reader->read(&buffer,0,4096,at,true,true);const auto high=os.processSamplesUp(juce::dsp::AudioBlock<float>(buffer));for(size_t ch=0;ch<high.getNumChannels();++ch)for(size_t i=0;i<high.getNumSamples();++i)peak=std::max(peak,std::abs(high.getChannelPointer(ch)[i]));}constexpr float ceiling=.89115f;if(peak<=ceiling)return juce::Result::ok();const float gain=ceiling/peak;juce::TemporaryFile temporary(file);auto stream=temporary.getFile().createOutputStream();std::unique_ptr<juce::AudioFormatWriter> writer(stream?format.createWriterFor(stream.get(),reader->sampleRate,2,(unsigned)bits,reader->metadataValues,0):nullptr);if(!writer)return juce::Result::fail("Cannot write loudness guard");stream.release();for(juce::int64 at=0;at<reader->lengthInSamples;at+=4096){if(cancelled())return juce::Result::fail("Export cancelled");const int n=(int)std::min<juce::int64>(4096,reader->lengthInSamples-at);reader->read(&buffer,0,n,at,true,true);buffer.applyGain(gain);if(!writer->writeFromAudioSampleBuffer(buffer,0,n))return juce::Result::fail("Cannot write guarded audio");}writer.reset();reader.reset();return temporary.overwriteTargetFileWithTemporary()?juce::Result::ok():juce::Result::fail("Cannot finish loudness guard");}
};
}
