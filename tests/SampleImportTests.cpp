// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/SampleImport.h"
#include "../Source/AudioEngine.h"
#include "../Source/StudioIO.h"
#include <stdexcept>
#include <iostream>
namespace {
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
void writeFixture(const juce::File& file,int seconds){
    juce::WavAudioFormat wav;std::unique_ptr<juce::OutputStream> stream=file.createOutputStream();
    auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(32));
    check(writer!=nullptr,"fixture writer");juce::AudioBuffer<float> block(2,4800);
    for(int i=0;i<4800;++i){block.setSample(0,i,.3f*std::sin(i*.03f));block.setSample(1,i,-.2f*std::sin(i*.03f));}
    for(int n=0;n<seconds*10;++n)check(writer->writeFromAudioSampleBuffer(block,0,4800),"fixture write");
}
struct JobScope {
    fga::SampleImportJob job;
    JobScope(const juce::File& f,fga::SamplerSettings s={},double rate=44100,bool preset=false,fga::SampleLoadLimits limits={}):job(f,s,60,false,rate,limits,false,preset){check(job.startThread(),"worker start");}
    ~JobScope(){job.signalThreadShouldExit();job.finish.signal();job.waitForThreadToExit(10000);}
    void wait(){const auto deadline=juce::Time::getMillisecondCounterHiRes()+10000;while(!job.ready.load(std::memory_order_acquire)&&juce::Time::getMillisecondCounterHiRes()<deadline)juce::Thread::sleep(1);check(job.ready.load(std::memory_order_acquire),"worker completed bounded fixture");}
};
}
void runSampleImportTests(){
    using namespace fga;const auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("sample-import-test","",false);folder.createDirectory();
    struct Cleanup{juce::File dir;~Cleanup(){dir.deleteRecursively();}} cleanup{folder};
    const auto file=folder.getChildFile("input.wav");writeFixture(file,8);const auto initial=sampleMemoryInUse();
    std::shared_ptr<const SampleAsset> asset;check(readSampleAsset(file,asset).wasOk(),"default policy accepts existing sample");auto held=asset;
    SampleLoadLimits limits;limits.fileBytes=1024*1024;
    auto error=readSampleAsset(file,asset,limits);check(error.failed()&&error.getErrorMessage().contains("file limit")&&asset==held,"file cap rejects before replacement");
    limits={};limits.decodedBytes=1024*1024;error=readSampleAsset(file,asset,limits);check(error.failed()&&error.getErrorMessage().contains("decoded-memory"),"decoded cap checked before buffer allocation");
    limits={};limits.seconds=1;check(readSampleAsset(file,asset,limits).failed(),"duration limit");
    check(readSampleAsset(file,asset,{},[]{return true;}).failed()&&asset==held,"immediate cancellation preserves previous asset");
    int checks=0;error=readSampleAsset(file,asset,{},[&]{return ++checks>4;});check(error.failed()&&asset==held,"cancel chunked decode");
    const auto budgetBefore=sampleMemoryInUse();std::shared_ptr<const SampleAsset> cancelled;
    bool stop=false;error=readSampleAsset(file,cancelled,{},[&]{return stop;},[&](double p){if(p>=.6)stop=true;});
    check(error.failed()&&!cancelled&&sampleMemoryInUse()==budgetBefore,"cancel hash releases reservation and partial PCM");
    {
        const auto large=folder.getChildFile("budget.wav");writeFixture(large,120);SampleLoadLimits small;small.totalBytes=64ll*1024*1024;
        std::shared_ptr<const SampleAsset> first,second;check(readSampleAsset(large,first,small).wasOk(),"first asset fits total budget");
        const auto reserved=sampleMemoryInUse();error=readSampleAsset(large,second,small);
        check(error.failed()&&!second&&sampleMemoryInUse()==reserved&&error.getErrorMessage().contains("total decoded-memory"),"aggregate budget includes old and pending samples");
        first.reset();
        JobScope a(large,{},44100,false,small),b(large,{},44100,false,small);a.wait();b.wait();
        check((int)a.job.result.wasOk()+(int)b.job.result.wasOk()==1&&sampleMemoryInUse()<=small.totalBytes,"concurrent reservations cannot oversubscribe total budget");
    }
    AudioEngine engine(AudioEngine::Offline{});SamplerSettings settings;settings.mode=2;settings.sliceCount=2;settings.slices[1]=.5;settings.polyphony=1;
    {
        JobScope scope(file,settings);scope.wait();check(scope.job.result.wasOk()&&scope.job.prepared,"background decode/hash/peaks/DSP preparation");
        check(engine.installPreparedSample(0,scope.job.prepared,scope.job.retired).wasOk()&&!scope.job.prepared,"prepared source transaction transfers ownership");
        check(engine.sampleAsset(0)->hash==held->hash,"async and synchronous PCM fingerprint agree");
    }
    auto source=engine.sampleAsset(0);
    {
        JobScope wrongRate(file,settings,48000);wrongRate.wait();int checkpoints=0;
        check(engine.installPreparedSample(0,wrongRate.job.prepared,wrongRate.job.retired,[&]{++checkpoints;}).failed()&&checkpoints==0&&engine.sampleAsset(0)==source,"device-rate mismatch preserves source and Undo boundary");
    }
    {
        JobScope cancel(file);cancel.job.signalThreadShouldExit();cancel.wait();check(cancel.job.result.failed(),"cancel worker before publication");
    }
    {
        SamplerPreset preset{settings,60,file,held->hash};const auto path=folder.getChildFile("voice.fgs");check(saveSamplerPreset(preset,path).wasOk(),"save preset fixture");
        JobScope loaded(path,{},44100,true);loaded.wait();check(loaded.job.result.wasOk()&&loaded.job.settings==settings,"preset read/hash validation and preparation off-thread");
    }
    SampleAsset grid;grid.rate=48000;grid.frames=48000*8;SamplerSettings s;s.snap=false;s.sliceSnapBeats=1;
    const double a=snapSlicePoint(grid,.073,s,125);check(std::abs(a-.06)<1e-10,"beat snap uses project BPM");
    check(std::abs(snapSlicePoint(grid,.073,s,150)-.05)<1e-10,"beat snap follows changed project tempo");
    s.sync=true;s.sourceBpm=100;check(std::abs(snapSlicePoint(grid,.09,s,125)-.075)<1e-10&&std::abs(snapSlicePoint(grid,.09,s,150)-.075)<1e-10,"sync source grid stays aligned at changing output BPM");
    s.sync=false;s.transpose=12;check(std::abs(snapSlicePoint(grid,.13,s,125)-.12)<1e-10,"musical grid accounts for resampling pitch");
    s.transpose=0;s.sliceSnapBeats=4;check(std::abs(snapSlicePoint(grid,.22,s,125)-.24)<1e-10,"bar snap");
    s.sliceSnapBeats=1./3.;check(std::abs(snapSlicePoint(grid,.027,s,125)-.02)<1e-10,"triplet snap");
    s.sliceSnapBeats=.25;s.start=.1;check(std::abs(snapSlicePoint(grid,.126,s,125)-.13)<1e-10,"grid origin follows trim start");
    s.sliceSnapBeats=0;check(snapSlicePoint(grid,.126,s,125)==.126,"snap off retains exact manual marker");
    s.sliceSnapBeats=.5;s.normalise();SamplerSettings loaded;check(readSampler(samplerJson(s),loaded).wasOk()&&loaded==s,"musical grid round trip");
    auto legacy=samplerJson(s);legacy.getDynamicObject()->removeProperty("sliceSnapBeats");check(readSampler(legacy,loaded).wasOk()&&loaded.sliceSnapBeats==0,"legacy projects default to no musical snapping");
    engine.useInternal(0);source.reset();asset.reset();held.reset();check(sampleMemoryInUse()==initial,"reservations are released after the last asset owner");
    std::cout<<"PASS asynchronous sample preparation, cancellation, budgets, preset validation and musical grid\n";
}
