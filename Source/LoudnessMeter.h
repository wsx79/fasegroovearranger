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
    static float lufs(double energy){return std::isfinite(energy)&&energy>1e-15?(float)(-.691+10*std::log10(energy)):-100.f;}
public:
    std::atomic<float> shortTerm{-100},integrated{-100};
    void prepare(double rate){
        rate=std::max(8000.,rate);block=std::max(1,(int)std::lround(rate*.1));
        // BS.1770 Annex 1: bilinear adaptation of the reference 48 kHz filters.
        // A generic RBJ shelf changes the transition response; RLB's numerator
        // is deliberately [1, -2, 1], without the usual high-pass normalization.
        const double k=std::tan(juce::MathConstants<double>::pi*1681.974450955533/rate);
        constexpr double q=.7071752369554196,vh=1.584864701130855;
        const double vb=std::pow(vh,.4996667741545416),a0=1+k/q+k*k;
        const double h=std::tan(juce::MathConstants<double>::pi*38.13547087602444/rate),h0=1+h/.5003270373238773+h*h;
        using Coefficients=juce::dsp::IIR::Coefficients<double>;
        for(int ch=0;ch<2;++ch){
            shelf[(size_t)ch].coefficients=new Coefficients((vh+vb*k/q+k*k)/a0,2*(k*k-vh)/a0,(vh-vb*k/q+k*k)/a0,1.,2*(k*k-1)/a0,(1-k/q+k*k)/a0);
            hp[(size_t)ch].coefficients=new Coefficients(1.,-2.,1.,1.,2*(h*h-1)/h0,(1-h/.5003270373238773+h*h)/h0);
        }
        reset();
    }
    void reset(){for(auto& f:shelf)f.reset();for(auto& f:hp)f.reset();recent.fill(0);sums.fill(0);counts.fill(0);sum=absoluteEnergy=0;absoluteCount=0;frames=index=filled=0;shortTerm=-100;integrated=-100;}
    void process(float* const* data,int channels,int count){for(int n=0;n<count;++n){for(int ch=0;ch<std::min(channels,2);++ch)if(data[ch]){const double y=hp[(size_t)ch].processSample(shelf[(size_t)ch].processSample(std::isfinite(data[ch][n])?data[ch][n]:0.f));if(std::isfinite(y))sum+=y*y;else{hp[(size_t)ch].reset();shelf[(size_t)ch].reset();}}if(++frames<block)continue;recent[(size_t)index]=sum/block;sum=0;frames=0;index=(index+1)%30;filled=std::min(30,filled+1);double shortEnergy=0;for(int i=0;i<filled;++i)shortEnergy+=recent[(size_t)i];shortTerm=lufs(shortEnergy/std::max(1,filled));if(filled<4)continue;double energy=0;for(int i=1;i<=4;++i)energy+=recent[(size_t)((index-i+30)%30)];energy/=4;const float level=lufs(energy);if(!std::isfinite(level)||level< -70)continue;const int bin=(int)std::clamp(std::floor((level+70)*10.),0.,900.);sums[(size_t)bin]+=energy;++counts[(size_t)bin];absoluteEnergy+=energy;++absoluteCount;const float gate=lufs(absoluteEnergy/absoluteCount)-10;double total=0;std::uint64_t used=0;for(int i=(int)std::clamp(std::ceil((gate+70)*10.),0.,900.);i<=900;++i){total+=sums[(size_t)i];used+=counts[(size_t)i];}integrated=used?lufs(total/used):-100.f;}}
};
// Existing 4x linked guard; final PCM verification uses 16x and a 0.05 dB reserve.
class ExportPeakGuard {
    juce::dsp::Oversampling<float> oversampling{2,2,juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,true};
public:
    void prepare(int block){oversampling.initProcessing((size_t)block);oversampling.reset();}
    void process(juce::AudioBuffer<float>& buffer,int count){auto block=juce::dsp::AudioBlock<float>(buffer).getSubBlock(0,(size_t)count);auto high=oversampling.processSamplesUp(block);constexpr float ceiling=.875f;for(size_t i=0;i<high.getNumSamples();++i){const float peak=std::max(std::abs(high.getSample(0,(int)i)),std::abs(high.getSample(1,(int)i)));const float gain=peak>ceiling?ceiling/peak:1.f;for(int ch=0;ch<2;++ch)high.getChannelPointer((size_t)ch)[i]*=gain;}oversampling.processSamplesDown(block);}
    int latency()const{return (int)std::ceil(oversampling.getLatencyInSamples());}
    static juce::Result verifyFile(const juce::File&,int bits,const std::function<bool()>& cancelled,double ceilingDb=-1.);
};
}
