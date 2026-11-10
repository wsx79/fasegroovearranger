// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/GuideVocalRender.h"
#include "../Source/GuideProcess.h"
#include "../Source/AudioEngine.h"
#include "../Source/ProjectIO.h"
#include <stdexcept>
#include <iostream>
void runGuideVocalTests(){
    using namespace fga;
    auto check=[](bool ok,const char* text){if(!ok)throw std::runtime_error(text);};
    Project p;p.sections={{Section::intro,16,"DJ intro"},{Section::main,8,"Verse"},{Section::outro,16,"DJ outro"}};p.normalise();p.settings.bpm=125;
    const int t=p.addMidiTrack();auto& c=p.tracks[(size_t)t].clips[0];c.span=160;c.notes={{0,1,60,90,0},{64,1,60,90,0},{66,2,64,100,0},{96,1,60,90,0}};enableVocalClip(c,160);c.vocal->voice="ecantorix-en";
    for(const auto& n:c.notes)c.vocal->syllables.push_back({c.vocal->nextSyllableId++,"hello","",{n.vocalId}});
    GuideVocalPlan plan;check(guideVocalPlan(p,t,0,plan).wasOk()&&plan.start==64&&std::abs(plan.duration-1.92)<1e-6&&plan.audible.size()==2,"guide region and intro/outro exclusion");
    auto overlap=p;overlap.tracks[(size_t)t].clips[0].notes[2].start=64.5;check(guideVocalPlan(overlap,t,0,plan).failed(),"guide overlapping notes rejected");
    auto override=p;override.tracks[(size_t)t].clips[0].vocal->allowIntroOutro=true;
    check(guideVocalPlan(override,t,0,plan).wasOk()&&plan.start==0&&plan.audible.size()==4,"explicit intro/outro singing override");
    auto eight=p;eight.tracks[(size_t)t].clips[0].notes[2].length=30;
    check(guideVocalPlan(eight,t,0,plan).wasOk()&&std::abs(plan.duration-15.36)<1e-6,"eight bars at 125 BPM");
    check(guideVocalPlan(p,t,0,plan).wasOk(),"restore test plan");
    GuideVocalPlan plan2;auto unsafe=p;unsafe.tracks[(size_t)t].clips[0].vocal->syllables[1].text="[[phonemes]]";
    check(guideVocalPlan(unsafe,t,0,plan2).failed(),"raw phoneme/markup input rejected");
    std::cout<<"PASS guide MIDI timing, regions, intro/outro override and invalid input\n";
    const auto script=juce::SystemStats::getEnvironmentVariable("FGA_ECANTORIX","");if(script.isEmpty()){std::cout<<"NOT RUN real eCantorix render (FGA_ECANTORIX unset)\n";return;}
    const auto cache=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fga-guide-test","",false);AudioTake take;auto progress=[](double){};
    auto r=renderGuideVocal(p,t,0,juce::File(script),cache,take,[]{return false;},progress);if(r.failed())throw std::runtime_error(r.getErrorMessage().toStdString());
    check(take.start==64&&take.frames==(juce::int64)std::ceil(1.92*22050),"actual guide duration/start");
    juce::WavAudioFormat f;auto reader=std::unique_ptr<juce::AudioFormatReader>(f.createReaderFor(take.file.createInputStream().release(),true));juce::AudioBuffer<float> audio(1,(int)take.frames);reader->read(&audio,0,(int)take.frames,0,true,false);reader.reset();
    check(audio.getMagnitude(0,1000,(int)(.4*22050))>0.001f&&audio.getMagnitude(0,(int)(.5*22050),(int)(.4*22050))==0,"actual singing and exact rest");
    // Upstream defaults to -24 semitones: verify our compensation on a real C4.
    int bestLag=0;double bestCorrelation=0;
    for(int lag=30;lag<200;++lag){double sum=0,left=0,right=0;for(int i=4000;i<8000-lag;++i){const double a=audio.getSample(0,i),b=audio.getSample(0,i+lag);sum+=a*b;left+=a*a;right+=b*b;}const double correlation=sum/std::sqrt(std::max(1e-20,left*right));if(correlation>bestCorrelation){bestCorrelation=correlation;bestLag=lag;}}
    check(bestLag>=80&&bestLag<=89&&bestCorrelation>.8,"guide follows C4 without upstream octave transposition");
    // Backdate the file: timestamps alone can hide rewrites within one second.
    const auto example=juce::SystemStats::getEnvironmentVariable("FGA_GUIDE_TEST_OUTPUT","");
    if(juce::File::isAbsolutePath(example))check(take.file.copyFileTo(juce::File(example)),"copy real guide example WAV");
    AudioTake eightTake;check(renderGuideVocal(eight,t,0,juce::File(script),cache,eightTake,[]{return false;},progress).wasOk()&&eightTake.frames==(juce::int64)std::ceil(15.36*22050),"real eight-bar 125 BPM guide duration");
    const auto time=juce::Time::getCurrentTime()-juce::RelativeTime::days(1);
    take.file.setLastModificationTime(time);const auto storedTime=take.file.getLastModificationTime();
    for(int retry=0;retry<10;++retry){
        AudioTake cached;bool hit=false;
        const auto reuse=renderGuideVocal(p,t,0,juce::File(script),cache,cached,[]{return false;},progress,&hit);
        if(reuse.failed())throw std::runtime_error(reuse.getErrorMessage().toStdString());
        check(hit&&cached.file==take.file&&cached.file.getLastModificationTime()==storedTime,"cached guide reused without synthesis or rewrite");
    }
    // Old-key collision: identical rounded MIDI, different exact duration/mask.
    auto precise=p;precise.tracks[(size_t)t].clips[0].notes[2].length+=.0001;
    GuideVocalPlan other;check(guideVocalPlan(precise,t,0,other).wasOk()&&other.midi==plan.midi&&other.duration!=plan.duration,"sub-tick cache collision fixture");
    AudioTake exact;bool hit=false;
    check(renderGuideVocal(precise,t,0,juce::File(script),cache,exact,[]{return false;},progress,&hit).wasOk()&&!hit&&exact.file!=take.file,"exact durations have independent cache keys");
    AudioTake original;check(renderGuideVocal(p,t,0,juce::File(script),cache,original,[]{return false;},progress,&hit).wasOk()&&hit&&original.file==take.file&&original.file.getLastModificationTime()==storedTime,"sub-tick edit preserves unrelated cached render");
    check(take.file.appendData("x",1),"mutate cache without changing WAV header");
    AudioTake repaired;check(renderGuideVocal(p,t,0,juce::File(script),cache,repaired,[]{return false;},progress,&hit).wasOk()&&!hit&&repaired.file==take.file,"checksum rejects corrupt cache and repairs it");
    p.tracks[(size_t)t].clips[0].vocal->syllables[1].text="world";AudioTake changed;check(renderGuideVocal(p,t,0,juce::File(script),cache,changed,[]{return false;},progress).wasOk()&&changed.file!=take.file,"word edit invalidates cached audio");
    check(renderGuideVocal(p,t,0,juce::File(script),cache,changed,[]{return true;},progress).failed(),"cancelled guide not delivered");
    const int target=addAudioTake(p,take,false);check(target>=0&&p.tracks[(size_t)target].clips[0].position==64,"guide uses existing AudioTake timeline");
    AudioEngine engine(AudioEngine::Offline{});auto& audioTrack=p.tracks[(size_t)target];
    check(engine.loadSample(target,take.file,60,audioTrack.sampler).wasOk(),"guide sample load");
    const auto stem=cache.getChildFile("guide-stem.wav");
    check(engine.exportAudio(p,stem,target,false,[]{return false;},progress).wasOk(),"existing separate WAV stem export");
    auto stemReader=std::unique_ptr<juce::AudioFormatReader>(f.createReaderFor(stem.createInputStream().release(),true));
    juce::AudioBuffer<float> intro(2,22050);check(stemReader&&stemReader->read(&intro,0,22050,0,true,true)&&intro.getMagnitude(0,22050)==0,"exported stem intro silence");
    const int outroAt=(int)std::round(96*60./125*stemReader->sampleRate);
    check(stemReader->read(&intro,0,22050,outroAt,true,true)&&intro.getMagnitude(0,22050)==0,"exported stem outro silence");stemReader.reset();
    Project reopened;auto saved=cache.getChildFile("guide.fga");
    check(saveProject(p,saved).wasOk()&&loadProject(saved,reopened).wasOk()&&reopened.tracks[(size_t)target].instrumentPath==audioTrack.instrumentPath,"guide WAV reference save/reopen");
    const auto noisy=runGuideProcess({"perl","-e","print q(x) x (2*1024*1024)"},cache.getChildFile("large.log"),2000,[]{return false;});
    check(noisy.failed()&&noisy.getErrorMessage().contains("output limit"),"fast process exit still enforces diagnostic output bound");
    const auto failed=runGuideProcess({"perl","-e","print q(expected diagnostic); exit 7"},cache.getChildFile("failed.log"),2000,[]{return false;});
    check(failed.failed()&&failed.getErrorMessage().contains("exit 7")&&failed.getErrorMessage().contains("expected diagnostic"),"process exit code and bounded error text preserved");
    check(runGuideProcess({"perl","-e","sleep 30"},cache.getChildFile("timeout.log"),60,[]{return false;}).failed(),"process timeout is enforced");
    auto child=cache.getChildFile("slow.pl");child.replaceWithText("sleep 30;\n");
    const auto began=juce::Time::getMillisecondCounterHiRes();
    auto stopped=runGuideProcess({"perl",child.getFullPathName()},cache.getChildFile("cancel.log"),5000,[&]{return juce::Time::getMillisecondCounterHiRes()-began>100;});
    check(stopped.failed()&&stopped.getErrorMessage().contains("cancelled")&&juce::Time::getMillisecondCounterHiRes()-began<2000,"active child cancellation bounded");
    check(runGuideProcess({"perl","-e","exit 0"},cache.getChildFile("retry.log"),2000,[]{return false;}).wasOk(),"retry after cancellation");
#if !JUCE_WINDOWS
    const auto marker=cache.getChildFile("orphan-marker");
    child.replaceWithText("system $^X, '-e', 'sleep 1; open my $f, q(>), $ARGV[0] or die $!; print $f q(alive)', $ARGV[0];\n");
    const auto treeBegan=juce::Time::getMillisecondCounterHiRes();
    check(runGuideProcess({"perl",child.getFullPathName(),marker.getFullPathName()},cache.getChildFile("tree.log"),5000,[&]{return juce::Time::getMillisecondCounterHiRes()-treeBegan>100;}).failed(),"cancel nested process tree");
    juce::Thread::sleep(1100);check(!marker.existsAsFile(),"cancel leaves no active descendant");
#endif
    for(int attempt=0;attempt<2;++attempt){
        double synthesisStarted=0;AudioTake cancelledTake;
        const auto cancelCache=cache.getChildFile("cancel-"+juce::String(attempt));
        auto cancelledRender=renderGuideVocal(p,t,0,juce::File(script),cancelCache,cancelledTake,[&]{return synthesisStarted>0&&juce::Time::getMillisecondCounterHiRes()-synthesisStarted>150;},[&](double value){if(value==.1)synthesisStarted=juce::Time::getMillisecondCounterHiRes();});
        check(cancelledRender.failed()&&cancelledRender.getErrorMessage().contains("cancelled")&&!cancelledTake.file.existsAsFile()&&cancelCache.getNumberOfChildFiles(juce::File::findFilesAndDirectories)==0,"active renderer cancellation cleans partial workspace");
    }
    cache.deleteRecursively();std::cout<<"PASS real offline eCantorix rendering, cache, rests, edit invalidation and AudioTake\n";
}
