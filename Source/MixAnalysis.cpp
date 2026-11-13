// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MixAnalysis.h"
#include "OfflineExport.h"
#include "Localisation.h"
namespace fga {
namespace {
class MonoPeakReader:public juce::AudioFormatReader{
    juce::AudioFormatReader& source;juce::AudioBuffer<float> scratch{1,4096};
public:
    explicit MonoPeakReader(juce::AudioFormatReader& s):AudioFormatReader(nullptr,"mono true peak"),source(s){sampleRate=s.sampleRate;numChannels=2;bitsPerSample=32;usesFloatingPointData=true;lengthInSamples=s.lengthInSamples;}
    bool readSamples(int* const* dest,int channels,int offset,juce::int64 start,int n)override{
        for(int at=0;at<n;at+=4096){const int used=std::min(4096,n-at);scratch.clear();if(!source.read(&scratch,0,used,start+at,true,false))return false;
            for(int c=0;c<channels;++c)if(dest[c])std::copy_n(scratch.getReadPointer(0),used,reinterpret_cast<float*>(dest[c])+offset+at);}
        return true;
    }
};
}

juce::Result analyseMixFiles(const std::vector<MixInput>& inputs,MixReport& result,
                            const std::function<bool()>& cancelled,const std::function<void(double)>& progress){
    result={};
    if(inputs.empty()||inputs.size()>33||inputs[0].role!=0)
        return juce::Result::fail(tr("Choose one mix WAV and at most 32 aligned stems."));
    juce::WavAudioFormat format;
    std::vector<std::unique_ptr<juce::AudioFormatReader>> readers;
    std::vector<std::pair<juce::int64,juce::Time>> stamps;
    std::vector<std::uint64_t> sessions;
    for(size_t i=0;i<inputs.size();++i){
        const auto& input=inputs[i];
        if((i==0&&input.session!=0)||(i>0&&(input.role<1||input.role>3||input.session==0||
            std::find(sessions.begin(),sessions.end(),input.session)!=sessions.end())))
            return juce::Result::fail(tr("Invalid or repeated stem track binding."));
        sessions.push_back(input.session);
        stamps.push_back({input.file.getSize(),input.file.getLastModificationTime()});
        if(cancelled())return juce::Result::fail(tr("Analysis cancelled."));
        auto stream=input.file.createInputStream();
        if(!stream||!input.file.hasFileExtension("wav"))
            return juce::Result::fail(tr("Cannot open WAV: ")+input.file.getFileName());
        auto reader=std::unique_ptr<juce::AudioFormatReader>(format.createReaderFor(stream.release(),true));
        if(!reader||reader->numChannels<1||reader->numChannels>2||reader->lengthInSamples<1)
            return juce::Result::fail(tr("Expected nonempty mono/stereo WAV: ")+input.file.getFileName());
        if(!std::isfinite(reader->sampleRate)||reader->sampleRate<8000||reader->sampleRate>192000)
            return juce::Result::fail(tr(juce::String::fromUTF8("WAV sample rate must be 8–192 kHz: "))+input.file.getFileName());
        if(reader->lengthInSamples>reader->sampleRate*900)
            return juce::Result::fail(tr("WAV exceeds the 15-minute analysis limit: ")+input.file.getFileName());
        if(!readers.empty()&&reader->sampleRate!=readers[0]->sampleRate)
            return juce::Result::fail(tr("Use aligned files with the same sample rate; no implicit resampling."));
        if(!readers.empty()&&reader->lengthInSamples>readers[0]->lengthInSamples)
            return juce::Result::fail(tr("Stem exceeds mix duration; align files before analysis."));
        readers.push_back(std::move(reader));
    }
    result.inputLevels.resize(inputs.size());
    constexpr int block=EqAnalyzer::size;
    std::vector<juce::AudioBuffer<float>> buffers;
    for(size_t i=0;i<inputs.size();++i)buffers.emplace_back(2,block);
    std::vector<EqAnalyzer> analyzers(inputs.size());
    std::vector<std::array<double,6>> bands(inputs.size());
    std::vector<double> energies(inputs.size());
    struct Pair {size_t a,b;double score=0;int active=0;bool low=false;std::array<double,2> shared{};};
    std::vector<Pair> pairs;
    for(size_t a=1;a<inputs.size();++a)for(size_t b=a+1;b<inputs.size();++b)
        pairs.push_back({a,b,0,0,(inputs[a].role==1&&inputs[b].role==2)||(inputs[a].role==2&&inputs[b].role==1),{}});
    LoudnessMeter meter;meter.prepare(readers[0]->sampleRate);
    bool previewFound=false;
    double ll=0,rr=0,lr=0,sum=0;juce::int64 frames=0;
    for(juce::int64 at=0;at<readers[0]->lengthInSamples;at+=block){
        if(cancelled())return juce::Result::fail(tr("Analysis cancelled."));
        const int n=(int)std::min<juce::int64>(block,readers[0]->lengthInSamples-at);
        for(size_t f=0;f<inputs.size();++f){
            auto& audio=buffers[f];audio.clear();
            if(at<readers[f]->lengthInSamples&&!readers[f]->read(&audio,0,
                (int)std::min<juce::int64>(n,readers[f]->lengthInSamples-at),at,true,true))
                return juce::Result::fail(tr("Cannot read WAV: ")+inputs[f].file.getFileName());
            if(readers[f]->numChannels==1)audio.copyFrom(1,0,audio,0,0,n);
            energies[f]=0;
            for(int c=0;c<2;++c)for(int i=0;i<n;++i){
                const double v=audio.getSample(c,i);
                if(!std::isfinite(v))return juce::Result::fail(tr("Non-finite audio: ")+inputs[f].file.getFileName());
                energies[f]+=v*v;
                result.inputLevels[f].peak=std::max(result.inputLevels[f].peak,std::abs(v));
                if(f==0){
                    result.mix.peak=std::max(result.mix.peak,std::abs(v));
                    if(c<(int)readers[f]->numChannels&&std::abs(v)>=.9999)++result.mix.fullScale;
                }
            }
            result.inputLevels[f].rms+=energies[f];
            bands[f].fill(0);
            const auto power=analyzers[f].framePower(audio.getReadPointer(0),audio.getReadPointer(1),n);
            for(float v:power)if(!std::isfinite(v))return juce::Result::fail(tr("Audio amplitude exceeds analyzer range."));
            constexpr double edges[]{20,80,160,500,2000,6000,24000};
            for(int bin=1;bin<block/2;++bin){
                const double hz=bin*readers[0]->sampleRate/block;
                for(int k=0;k<6;++k)if(hz>=edges[k]&&hz<edges[k+1])bands[f][(size_t)k]+=power[(size_t)bin];
            }
        }
        if(!previewFound&&energies[0]/(2*n)>1e-6){result.previewStart=std::clamp(at/readers[0]->sampleRate-.05,0.,899.);previewFound=true;}
        for(size_t k=0;k<6;++k)result.mix.bandPower[k]+=bands[0][k]*n;
        auto& audio=buffers[0];
        meter.process(audio.getArrayOfWritePointers(),(int)readers[0]->numChannels,n);
        for(int i=0;i<n;++i){const double l=audio.getSample(0,i),r=audio.getSample(1,i);ll+=l*l;rr+=r*r;lr+=l*r;}
        sum+=energies[0];frames+=n;
        if(progress)progress((double)(at+n)/readers[0]->lengthInSamples);
        for(auto& p:pairs){
            if(energies[p.a]/(2*n)<1e-6||energies[p.b]/(2*n)<1e-6)continue;
            double dot=0,aa=0,bb=0;
            for(int k=0;k<(p.low?2:6);++k){
                const double a=bands[p.a][(size_t)k],b=bands[p.b][(size_t)k];dot+=a*b;aa+=a*a;bb+=b*b;if(p.low)p.shared[(size_t)k]+=std::sqrt(a*b);
            }
            if(aa>1e-14&&bb>1e-14){p.score+=dot/std::sqrt(aa*bb);++p.active;}
        }
    }
    for(auto& energy:result.mix.bandPower)energy/=std::max<juce::int64>(1,frames);
    result.mix.rms=std::sqrt(sum/std::max(1.,2.*frames));
    for(auto& level:result.inputLevels)level.rms=std::sqrt(level.rms/std::max(1.,2.*frames));
    result.mix.correlationValid=readers[0]->numChannels==2&&ll>1e-15&&rr>1e-15;
    if(result.mix.correlationValid)result.mix.correlation=std::clamp(lr/std::sqrt(ll*rr),-1.,1.);
    result.mix.lufs=meter.integrated.load();
    double truePeak=0;juce::Result peakResult=juce::Result::ok();
    if(readers[0]->numChannels==2)peakResult=measureOfflineTruePeak(*readers[0],truePeak,cancelled,[](double){});
    else{MonoPeakReader mono(*readers[0]);peakResult=measureOfflineTruePeak(mono,truePeak,cancelled,[](double){});}
    if(peakResult.failed())return peakResult;
    if(cancelled())return juce::Result::fail(tr("Analysis cancelled."));
    result.mix.truePeak=truePeak;
    if(result.mix.truePeak>juce::Decibels::decibelsToGain(-3.))
        result.suggestions.push_back({tr("Suggested master trim for 3 dB true-peak headroom (verify by listening)."),0,-3.-juce::Decibels::gainToDecibels(result.mix.truePeak)});
    if(result.mix.fullScale)
        result.suggestions.push_back({tr("Near-full-scale samples detected. A PCM file cannot prove whether clipping occurred upstream."),0,0});
    if(result.mix.correlationValid&&result.mix.correlation<-.2)
        result.suggestions.push_back({tr("Negative stereo correlation: check mono compatibility; no automatic stereo correction."),0,0});
    const int minimum=std::max(1,(int)std::ceil((double)frames/block*.1));
    for(const auto& p:pairs)if(p.active>=minimum&&p.score/p.active>.85){
        const auto target=p.low?(inputs[p.a].role==2?p.a:p.b):p.b;
        result.suggestions.push_back({(p.low?tr("Possible kick/bass low-band overlap: "):tr("Possible spectral masking: "))+
            inputs[p.a].file.getFileName()+" / "+inputs[p.b].file.getFileName()+
            tr(". Try a conservative -1.5 dB trim; spectral similarity is not proof of a bad mix."),inputs[target].session,-1.5,
            // The EQ alternative goes where the two parts actually share energy: kick fundamentals (20-80 Hz) or the bass body (80-160 Hz).
            p.low?(p.shared[0]>p.shared[1]?60.:115.):0.});
    }
    for(size_t i=0;i<inputs.size();++i)
        if(inputs[i].file.getSize()!=stamps[i].first||inputs[i].file.getLastModificationTime()!=stamps[i].second)
            return juce::Result::fail(tr("WAV changed during analysis; choose stable files and retry."));
    return cancelled()?juce::Result::fail(tr("Analysis cancelled.")):juce::Result::ok();
}
}
