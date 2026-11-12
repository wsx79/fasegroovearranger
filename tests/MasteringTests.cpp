// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/MasteringAnalysis.h"
#include "../Source/AudioEngine.h"
#include <juce_cryptography/juce_cryptography.h>
#include "Localisation.h"
#include "../Source/ProductSettings.h"
#include <stdexcept>
#include <iostream>
namespace {
void requireMaster(bool value,const char* text){if(!value)throw std::runtime_error(text);}
double lufs(juce::AudioBuffer<float>& audio,double rate){fga::LoudnessMeter meter;meter.prepare(rate);meter.process(audio.getArrayOfWritePointers(),2,audio.getNumSamples());return meter.integrated.load();}
}
void runMasteringTests(){
 FaseGroove::useSelfTestPreferences();
    using namespace fga;const auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("mastering-phase08","",false);folder.createDirectory();struct Cleanup {juce::File file;~Cleanup(){file.deleteRecursively();}}cleanup{folder};
    auto write=[&](const char* name,double amplitude,double rate=48000,int channels=2,double hz=997,int blocks=24,double phase=0){
        const auto file=folder.getChildFile(name);file.deleteFile();auto stream=file.createOutputStream();juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(),rate,(unsigned)channels,24,{},0));requireMaster(writer!=nullptr,"mastering fixture writer");stream.release();
        juce::AudioBuffer<float> audio(channels,4096);for(int b=0;b<blocks;++b){for(int c=0;c<channels;++c)for(int i=0;i<4096;++i)audio.setSample(c,i,(float)(amplitude*std::sin((b*4096+i)*2*juce::MathConstants<double>::pi*hz/rate+phase)));requireMaster(writer->writeFromAudioSampleBuffer(audio,0,4096),"mastering fixture write");}requireMaster(writer->flush(),"fixture flush");return file;
    };
    const auto mix=write("mix.wav",.4),ref=write("reference.wav",.1),silent=write("silence.wav",0),mono=write("vocal-mono.wav",.2,48000,1),low=write("low.wav",.3,48000,2,60);
    const auto hash=juce::SHA256(mix).toHexString();MasteringTargets targets;MasteringReport report;
    auto run=[&](const std::vector<MasteringInput>& in,const juce::File& reference=juce::File()){return analyseMastering(in,reference,targets,report,[]{return false;},[](double){});};
    requireMaster(run({{mix}}).wasOk()&&report.qaPassed&&!report.comparisonReady&&!report.suggestions.joinIntoString(" ").contains("Requested loudness"),"no mandatory loudness target/reference");
    requireMaster(run({{mix}},ref).wasOk()&&report.comparisonReady,"normalized reference comparison ready");
    const double a=lufs(report.a,48000),b=lufs(report.b,48000);requireMaster(std::abs(a-b)<.01&&std::abs(a-report.matchedLufs)<.01,"excerpt loudness matched independently");
    double residual=0;for(int i=0;i<report.a.getNumSamples();++i)residual=std::max(residual,std::abs((double)report.a.getSample(0,i)-report.b.getSample(0,i)));requireMaster(residual<2e-6&&report.gainA<.251&&report.gainB<=1,"equal material matches after normalization without boosts");
    requireMaster(report.a.getSample(0,0)==0&&report.a.getSample(0,report.a.getNumSamples()-1)==0,"excerpt fade endpoints");
    requireMaster(juce::SHA256(mix).toHexString()==hash,"reference analysis never writes original");
    {
        const auto original=report;const auto language=Localisation::current();Localisation::setCurrent(Localisation::Language::italian);
        requireMaster(run({{mix}},ref).wasOk()&&report.qa.joinIntoString(" ").contains("SUPERATO: il mix")&&report.comparisonNote.startsWith("A/B: estratti"),"Italian QA and comparison guidance");
        bool identical=report.files[0].truePeak==original.files[0].truePeak&&report.matchedLufs==original.matchedLufs&&report.a.getNumSamples()==original.a.getNumSamples();
        for(int c=0;identical&&c<report.a.getNumChannels();++c)for(int i=0;i<report.a.getNumSamples();++i)identical=identical&&report.a.getSample(c,i)==original.a.getSample(c,i)&&report.b.getSample(c,i)==original.b.getSample(c,i);
        requireMaster(identical,"language preserves true peak, LUFS and normalized samples exactly");
        const auto missing=folder.getChildFile("Reference optional.wav");const auto error=run({{missing}});requireMaster(error.failed()&&error.getErrorMessage().contains("Impossibile")&&error.getErrorMessage().endsWith(missing.getFileName()),"Italian mastering errors preserve filename");
        Localisation::setCurrent(Localisation::Language::english);requireMaster(run({{mix}},ref).wasOk()&&report.qa.joinIntoString(" ").contains("PASS: mix within"),"English QA restored");Localisation::setCurrent(language);
    }
    targets.useLufs=true;targets.lufs=-5;targets.ceilingDb=-12;requireMaster(run({{mix}},ref).wasOk()&&!report.qaPassed&&report.comparisonReady&&report.suggestions.joinIntoString(" ").contains("cannot be reached"),"unreachable target explained, no duplicate limiter");
    requireMaster(std::max(report.a.getMagnitude(0,report.a.getNumSamples()),report.b.getMagnitude(0,report.b.getNumSamples()))<juce::Decibels::decibelsToGain(-12.f),"preview respects selected ceiling");
    targets={};targets.expectVocal=true;requireMaster(run({{mix}}).wasOk()&&!report.qaPassed,"expected missing vocal");requireMaster(run({{mix},{silent,true}}).wasOk()&&!report.qaPassed,"silent tagged vocal");
    requireMaster(run({{mix},{mono,true}}).wasOk()&&report.qaPassed,"mono rendered vocal stem accepted without pretending voice detection");
    targets={};targets.expectedBits=16;requireMaster(run({{mix}}).wasOk()&&!report.qaPassed,"export depth QA");targets={};targets.expectedRate=44100;requireMaster(run({{mix}}).wasOk()&&!report.qaPassed,"export rate QA");
    targets={};requireMaster(run({{mix}},low).wasOk()&&report.suggestions.joinIntoString(" ").contains("relative spectral"),"tonal differences produce qualified recommendations");
    requireMaster(run({{silent}},ref).wasOk()&&!report.qaPassed&&!report.comparisonReady,"silence disables normalization, does not become a good master");
    requireMaster(run({{mix}},write("other-rate.wav",.2,44100)).wasOk()&&!report.comparisonReady,"no implicit comparison resampling");
    targets.startA=899;requireMaster(run({{mix}},ref).wasOk()&&!report.comparisonReady,"out-of-range excerpt does not invalidate completed export QA");targets={};
    requireMaster(run({{mix},{mix}}).failed(),"duplicate mix/stem rejected");requireMaster(run({{mix},{write("long-stem.wav",.2,48000,2,997,25)}}).failed(),"longer stem rejected");
    requireMaster(run({{folder.getChildFile("missing.wav")}}).failed(),"missing WAV");int cancel=0;requireMaster(analyseMastering({{mix}},ref,targets,report,[&]{return ++cancel>3;},[](double){}).failed(),"cancel analysis");requireMaster(run({{mix}},ref).wasOk(),"retry analysis");
    auto originalTime=mix.getLastModificationTime();mix.setLastModificationTime(originalTime+juce::RelativeTime::seconds(3));requireMaster(!masteringFilesUnchanged(report),"changed WAV invalidates preview");mix.setLastModificationTime(originalTime);
    for(double rate:{44100.,96000.,192000.}){auto f=write("rate.wav",.4,rate);requireMaster(run({{f}},f).wasOk()&&report.comparisonReady,"44.1/96/192 kHz comparison");}
    {const auto longer=write("long-8k.wav",.2,8000);requireMaster(run({{longer}},longer).wasOk()&&report.comparisonReady&&report.a.getNumSamples()==80000&&report.b.getNumSamples()==80000,"comparison storage caps each side at 10 seconds");}
    std::vector<MasteringInput> oversized(34);requireMaster(run(oversized).failed(),"bounded WAV input count");
    // Same player/path as the UI: rate-matched data is unshifted and carries the historical .8 audition level.
    if(juce::SystemStats::getEnvironmentVariable("FGA_MASTERING_PROFILE",{})!="1"){const auto f=write("player.wav",.2,44100);requireMaster(run({{f}},f).wasOk()&&report.comparisonReady,"player preparation");
     AudioEngine engine(AudioEngine::Offline{});Project blank;blank.normalise();for(auto& t:blank.tracks)for(auto& clip:t.clips)clip.notes.clear();engine.publish(blank);
     const int latency=engine.latencySamples();juce::AudioBuffer<float> prepared;prepared.makeCopyOf(report.a);const auto serial=engine.previewSerial();engine.startPreview(std::move(prepared),44100);requireMaster(engine.previewSerial()!=serial,"preview ownership serial advances");
     juce::AudioBuffer<float> out(2,512);juce::AudioIODeviceCallbackContext context;const auto activeSerial=engine.previewSerial();engine.audioDeviceIOCallbackWithContext(nullptr,0,out.getArrayOfWritePointers(),2,512,context);
     double error=0;for(int i=0;i<512;++i)error=std::max(error,std::abs((double)out.getSample(0,i)-.8*report.a.getSample(0,i)));
     requireMaster(error<1e-7&&engine.latencySamples()==latency&&engine.previewSerial()==activeSerial,"no added sample offset/PDC or audio-side ownership mutation");engine.stopPreview();requireMaster(!engine.isPreviewing()&&engine.previewSerial()!=activeSerial,"preview stop invalidates ownership");}
    // Retain only explicitly requested QA files, outside the source tree; independent FFmpeg verification follows.
    const auto qaPath=juce::SystemStats::getEnvironmentVariable("FGA_MASTERING_QA_DIR",{});
    auto retain=[&](const juce::File& file,juce::AudioBuffer<float>& audio,double rate){auto stream=file.createOutputStream();juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(),rate,2,24,{},0));requireMaster(writer!=nullptr,"QA waveform writer");stream.release();requireMaster(writer->writeFromAudioSampleBuffer(audio,0,audio.getNumSamples())&&writer->flush(),"QA waveform write");};
    juce::String manifest="file,peak16,ceiling_db\n";
    for(double rate:{44100.,48000.,96000.}){
        const auto f=write("intersample.wav",.99,rate,2,rate/4,12,juce::MathConstants<double>::pi/4);
        requireMaster(run({{f}},f).wasOk()&&report.comparisonReady&&report.files[0].truePeak>report.files[0].mix.peak+.1&&!report.qaPassed,"intersample peak risk detected below full-scale samples");
        if(qaPath.isNotEmpty()){juce::File qa(qaPath);qa.createDirectory();const auto stem=juce::String((int)rate);auto input=qa.getChildFile(stem+"-input.wav");input.deleteFile();requireMaster(f.copyFileTo(input),"retain ISP input");manifest+=input.getFileName()+","+juce::String(report.files[0].truePeak,9)+",0\n";
            auto output=qa.getChildFile(stem+"-normalized.wav");output.deleteFile();retain(output,report.a,rate);manifest+=output.getFileName()+",0,"+juce::String(targets.ceilingDb,2)+"\n";}
    }
    if(qaPath.isNotEmpty())juce::File(qaPath).getChildFile("manifest.csv").replaceWithText(manifest);
    targets.ceilingDb=0;requireMaster(run({{mix}}).failed(),"historical ceiling upper bound preserved");targets={};targets.lufs=std::numeric_limits<double>::quiet_NaN();requireMaster(run({{mix}}).failed(),"NaN target rejected");targets={};
    const auto external=juce::SystemStats::getEnvironmentVariable("FGA_MIX_VOCAL_FIXTURE",{});if(external.isNotEmpty()){requireMaster(run({{juce::File(external)},{mono,true}}).wasOk(),"previously rendered guide plus explicit vocal stem");std::cout<<"PASS existing rendered guide WAV inspected, no synthesis/quality approval\n";}
    std::cout<<"PASS mastering QA / TP16 shared estimator, optional targets, normalized excerpts LUFS A="<<a<<" B="<<b<<" residual="<<residual<<", stereo/mono stems/vocal presence, tone, formats, silence, cancel/retry, original hash and changed-file guard\n";
}
