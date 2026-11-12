// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MasteringAnalysis.h"
#include <numeric>
namespace fga {
namespace {
using Cancel=std::function<bool()>;using Progress=std::function<void(double)>;
std::unique_ptr<juce::AudioFormatReader> open(const juce::File& file){
    auto stream=file.createInputStream();if(!stream||!file.hasFileExtension("wav"))return {};
    juce::WavAudioFormat format;return std::unique_ptr<juce::AudioFormatReader>(format.createReaderFor(stream.release(),true));
}
bool unchanged(const MasteringFile& f){return f.file.existsAsFile()&&f.file.getSize()==f.bytes&&f.file.getLastModificationTime()==f.modified;}
// Reader adapter only; the existing export estimator performs all oversampling/peak measurement.
class BufferReader:public juce::AudioFormatReader {
    const juce::AudioBuffer<float>& audio;
public:
    BufferReader(const juce::AudioBuffer<float>& a,double rate):AudioFormatReader(nullptr,"Mastering excerpt"),audio(a){sampleRate=rate;numChannels=2;bitsPerSample=32;usesFloatingPointData=true;lengthInSamples=a.getNumSamples();}
    bool readSamples(int* const* dest,int channels,int offset,juce::int64 start,int n)override{
        for(int c=0;c<channels;++c)if(dest[c]){auto* out=reinterpret_cast<float*>(dest[c])+offset;std::fill(out,out+n,0.f);
            if(c<2&&start>=0&&start<lengthInSamples)std::copy_n(audio.getReadPointer(c)+(int)start,(int)std::min<juce::int64>(n,lengthInSamples-start),out);}
        return true;
    }
};
class MonoReader:public juce::AudioFormatReader {
    juce::AudioFormatReader& source;juce::AudioBuffer<float> scratch{1,4096};
public:
    explicit MonoReader(juce::AudioFormatReader& s):AudioFormatReader(nullptr,"Mono peak adapter"),source(s){sampleRate=s.sampleRate;numChannels=2;bitsPerSample=32;usesFloatingPointData=true;lengthInSamples=s.lengthInSamples;}
    bool readSamples(int* const* dest,int channels,int offset,juce::int64 start,int n)override{
        for(int at=0;at<n;at+=4096){const int used=std::min(4096,n-at);scratch.clear();
            if(!source.read(&scratch,0,used,start+at,true,false))return false;
            for(int c=0;c<channels;++c)if(dest[c])std::copy_n(scratch.getReadPointer(0),used,reinterpret_cast<float*>(dest[c])+offset+at);}
        return true;
    }
};
double excerptLufs(juce::AudioBuffer<float>& audio,double rate){
    LoudnessMeter meter;meter.prepare(rate);for(int i=0;i<audio.getNumSamples();i+=4096){float* p[]{audio.getWritePointer(0,i),audio.getWritePointer(1,i)};meter.process(p,2,std::min(4096,audio.getNumSamples()-i));}return meter.integrated.load();
}
juce::Result measure(const MasteringInput& input,MasteringFile& out,const Cancel& cancel,const Progress& progress){
    out={};out.file=input.file;out.vocal=input.vocal;out.bytes=input.file.getSize();out.modified=input.file.getLastModificationTime();
    MixReport mix;auto r=analyseMixFiles({{input.file}},mix,cancel,[&](double v){progress(v*.3);});if(r.failed())return r;
    auto reader=open(input.file);if(!reader)return juce::Result::fail("Cannot reopen mastering WAV: "+input.file.getFileName());
    out.mix=mix.mix;out.rate=reader->sampleRate;out.frames=reader->lengthInSamples;out.bits=(int)reader->bitsPerSample;out.channels=(int)reader->numChannels;
    if(reader->numChannels==1){MonoReader stereo(*reader);r=measureOfflineTruePeak(stereo,out.truePeak,cancel,[&](double v){progress(.3+.7*v);});}
    else r=measureOfflineTruePeak(*reader,out.truePeak,cancel,[&](double v){progress(.3+.7*v);});
    if(r.failed())return r;
    return unchanged(out)?juce::Result::ok():juce::Result::fail("WAV changed during mastering analysis.");
}
juce::Result excerpt(const MasteringFile& f,double start,juce::AudioBuffer<float>& audio,double& lufs,double& peak,const Cancel& cancel){
    auto reader=open(f.file);if(!reader||!unchanged(f))return juce::Result::fail("WAV changed; analyse again.");
    const auto at=(juce::int64)std::floor(start*f.rate);
    if(at>=f.frames)return juce::Result::fail("Excerpt starts after the end of the WAV.");
    const int count=(int)std::min<juce::int64>((juce::int64)(10*f.rate),f.frames-at);
    audio.setSize(2,count);LoudnessMeter meter;meter.prepare(f.rate);const int fade=std::max(1,(int)(f.rate*.005));
    for(int offset=0;offset<count;offset+=4096){
        if(cancel())return juce::Result::fail("Analysis cancelled.");const int n=std::min(4096,count-offset);
        if(!reader->read(&audio,offset,n,at+offset,true,true))return juce::Result::fail("Cannot read comparison excerpt.");
        for(int i=offset;i<offset+n;++i){const float envelope=(float)std::min({1.,(double)i/fade,(double)(count-1-i)/fade});
            for(int c=0;c<2;++c)audio.setSample(c,i,audio.getSample(c,i)*envelope);}
        float* p[]{audio.getWritePointer(0,offset),audio.getWritePointer(1,offset)};meter.process(p,2,n);
    }
    lufs=meter.integrated.load();BufferReader memory(audio,f.rate);
    return measureOfflineTruePeak(memory,peak,cancel,[](double){});
}
}
bool masteringFilesUnchanged(const MasteringReport& report){
    for(const auto& f:report.files)if(!unchanged(f))return false;
    return !report.hasReference||unchanged(report.reference);
}
juce::Result analyseMastering(const std::vector<MasteringInput>& inputs,const juce::File& reference,
    const MasteringTargets& targets,MasteringReport& result,const Cancel& cancel,const Progress& progress){
    result={};
    if(inputs.empty()||inputs.size()>33||inputs[0].vocal||!std::isfinite(targets.lufs)||targets.lufs< -30||targets.lufs> -5||
       !std::isfinite(targets.ceilingDb)||targets.ceilingDb<minExportCeilingDb||targets.ceilingDb>maxExportCeilingDb||
       !std::isfinite(targets.startA)||!std::isfinite(targets.startB)||targets.startA<0||targets.startB<0||targets.startA>900||targets.startB>900||
       (targets.expectedRate!=0&&targets.expectedRate!=44100&&targets.expectedRate!=48000&&targets.expectedRate!=96000)||
       (targets.expectedBits!=0&&targets.expectedBits!=16&&targets.expectedBits!=24))return juce::Result::fail("Invalid mastering targets or input count.");
    const int total=(int)inputs.size()+(reference!=juce::File()?1:0);
    for(size_t i=0;i<inputs.size();++i){
        for(size_t j=0;j<i;++j)if(inputs[i].file==inputs[j].file)return juce::Result::fail("Duplicate mix/stem WAV; bind each file once.");
        MasteringFile measured;auto r=measure(inputs[i],measured,cancel,[&](double v){progress(.8*(i+v)/total);});if(r.failed())return r;
        if(i>0&&(measured.rate!=result.files[0].rate||measured.frames>result.files[0].frames))return juce::Result::fail("Align stems to mix origin/rate; stems must not exceed mix duration.");
        result.files.push_back(std::move(measured));
    }
    auto fail=[&](const juce::String& text){result.qaPassed=false;result.qa.add("CHECK: "+text);};
    const auto& mix=result.files[0];const auto tpDb=juce::Decibels::gainToDecibels(mix.truePeak,-120.);
    if(mix.channels!=2)fail("Mix is mono; stereo mastering export is expected.");
    if(tpDb>targets.ceilingDb)fail("Mix exceeds selected True Peak ceiling: "+juce::String(tpDb,2)+" dBTP.");
    else result.qa.add("PASS: mix within selected True Peak ceiling (existing 16x estimator).");
    result.qa.add("MEASURED: True Peak margin to selected ceiling "+juce::String(targets.ceilingDb-tpDb,2)+" dB.");
    if(tpDb>targets.ceilingDb)result.suggestions.add("Reduce output gain or re-export with existing Transparent protection; attenuation cannot repair earlier clipping.");
    bool vocal=false;
    for(const auto& f:result.files){
        if(f.mix.fullScale||f.truePeak>=1)fail(f.file.getFileName()+": near-full-scale samples/intersample clipping risk; prior clipping cannot be inferred.");
        if(f.vocal&&f.mix.rms>1e-6)vocal=true;
        if(targets.expectedRate&&std::abs(f.rate-targets.expectedRate)>.1)fail(f.file.getFileName()+": sample rate differs from requested export rate.");
        if(targets.expectedBits&&f.bits!=targets.expectedBits)fail(f.file.getFileName()+": bit depth differs from requested export depth.");
    }
    if(targets.expectVocal&&!vocal)fail("Expected vocal stem missing or silent.");
    if(vocal)result.qa.add("MEASURED: explicitly tagged vocal stem contains audio; inclusion in the mix and singing/intelligibility are not detected.");
    else if(!targets.expectVocal)result.qa.add("NOT CHECKED: vocal presence was not required/tagged.");
    result.qa.add("MEASURED: "+juce::String((int)inputs.size()-1)+" supplied stems; missing stems and musical alignment cannot be inferred automatically.");
    if(mix.mix.rms>1e-12){
        const double crest=juce::Decibels::gainToDecibels(mix.mix.peak/mix.mix.rms);
        result.qa.add("MEASURED: sample crest factor "+juce::String(crest,2)+" dB; not loudness range (LRA).");
        if(crest<6)result.suggestions.add("Low sample crest factor: audition less reduction in the existing master compressor; this is material-dependent, not an error.");
    }else fail("Mix is silent; dynamics/loudness guidance unavailable.");
    if(targets.useLufs){
        if(mix.mix.lufs<= -99)fail("Integrated LUFS unavailable (silence/insufficient duration).");
        else{const double requested=targets.lufs-mix.mix.lufs,allowed=targets.ceilingDb-.05-tpDb;
            result.suggestions.add("Requested loudness delta "+juce::String(requested,2)+" dB; ceiling permits at most "+juce::String(allowed,2)+" dB uniform gain. Re-export and measure after any change; no limiter/loudness guarantee.");
            if(requested>allowed)result.suggestions.add("Requested LUFS cannot be reached by uniform gain within this ceiling. Choose a quieter target or manually review existing dynamics; Loud remains unavailable.");}
    }
    if(reference!=juce::File()){
        auto r=measure({reference,false},result.reference,cancel,[&](double v){progress(.8*(inputs.size()+v)/total);});if(r.failed())return r;result.hasReference=true;
        const auto& ref=result.reference;
        if(mix.mix.lufs> -99&&ref.mix.lufs> -99)result.qa.add("MEASURED: full-file mix minus reference loudness "+juce::String(mix.mix.lufs-ref.mix.lufs,2)+" LU; A/B matches selected excerpts separately.");
        if(mix.mix.rms>1e-12&&ref.mix.rms>1e-12){
            const double delta=juce::Decibels::gainToDecibels(mix.mix.peak/mix.mix.rms)-juce::Decibels::gainToDecibels(ref.mix.peak/ref.mix.rms);
            result.qa.add("MEASURED: sample crest factor difference mix minus reference "+juce::String(delta,2)+" dB.");
            if(delta< -3)result.suggestions.add("Mix has lower sample crest factor than this reference: audition existing dynamics/bypass at matched volume; material may justify the difference.");
        }
        const double sumA=std::accumulate(mix.mix.bandPower.begin(),mix.mix.bandPower.end(),0.),sumB=std::accumulate(ref.mix.bandPower.begin(),ref.mix.bandPower.end(),0.);
        constexpr const char* bands[]{"20-80 Hz","80-160 Hz","160-500 Hz","0.5-2 kHz","2-6 kHz","6-24 kHz"};
        if(sumA>1e-12&&sumB>1e-12)for(size_t k=0;k<6;++k){const double a=mix.mix.bandPower[k]/sumA,b=ref.mix.bandPower[k]/sumB;
            if(std::max(a,b)>1e-4){const double delta=10*std::log10(std::max(1e-8,a)/std::max(1e-8,b));if(std::abs(delta)>3)result.suggestions.add(juce::String(bands[k])+": relative spectral energy "+juce::String(delta,1)+" dB vs reference. Audition existing track EQ or an EQ plugin in master inserts; the reference is not a universal tonal target.");}}
        if(mix.channels!=2||ref.channels!=2)result.comparisonNote="A/B requires stereo mix and reference; mono stems are supported for QA.";
        else if(std::abs(mix.rate-ref.rate)>.1)result.comparisonNote="A/B unavailable: use files at the same rate; analysis does not resample.";
        else{
            double la=-100,lb=-100,pa=0,pb=0;
            r=excerpt(mix,targets.startA,result.a,la,pa,cancel);if(r.wasOk())r=excerpt(ref,targets.startB,result.b,lb,pb,cancel);
            if(cancel())return juce::Result::fail("Analysis cancelled.");
            if(r.failed())result.comparisonNote=r.getErrorMessage();
            else if(la<= -99||lb<= -99)result.comparisonNote="A/B unavailable: excerpt is silent or too short for integrated LUFS.";
            else{
                double common=std::min({la,lb,la+targets.ceilingDb-.05-juce::Decibels::gainToDecibels(pa,-120.),lb+targets.ceilingDb-.05-juce::Decibels::gainToDecibels(pb,-120.)});
                result.gainA=juce::Decibels::decibelsToGain(common-la);result.gainB=juce::Decibels::decibelsToGain(common-lb);
                result.a.applyGain((float)result.gainA);result.b.applyGain((float)result.gainB);
                BufferReader a(result.a,mix.rate),b(result.b,ref.rate);
                r=measureOfflineTruePeak(a,pa,cancel,[](double){});if(r.wasOk())r=measureOfflineTruePeak(b,pb,cancel,[](double){});if(r.failed())return r;
                if(std::max(pa,pb)>juce::Decibels::decibelsToGain(targets.ceilingDb))return juce::Result::fail("Normalized excerpt exceeds True Peak ceiling.");
                const double finalA=excerptLufs(result.a,mix.rate),finalB=excerptLufs(result.b,ref.rate);
                if(finalA< -60||finalB< -60||std::abs(finalA-finalB)>.1){result.a.setSize(0,0);result.b.setSize(0,0);result.comparisonNote="A/B unavailable: gated excerpt loudness cannot be matched reliably. Choose different offsets/reference.";}
                else {result.matchedLufs=(finalA+finalB)*.5;result.comparisonRate=mix.rate;result.comparisonReady=true;
                result.comparisonNote="A/B: at most 10-second excerpts, independent offsets, matched excerpt LUFS "+juce::String(common,2)+" before the common browser audition level; restarts, no seamless crossfade.";}
            }
        }
        if(!result.comparisonReady){result.a.setSize(0,0);result.b.setSize(0,0);}
    }else result.comparisonNote="Reference optional; choose one to enable loudness-normalized A/B.";
    progress(1);
    if(cancel())return juce::Result::fail("Analysis cancelled.");
    return masteringFilesUnchanged(result)?juce::Result::ok():juce::Result::fail("WAV changed; analyse again.");
}
}
