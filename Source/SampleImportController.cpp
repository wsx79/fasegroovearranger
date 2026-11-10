// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MainComponent.h"
#include "Localisation.h"
namespace fga {
SampleLoadLimits MainComponent::sampleImportLimits()const{
    SampleLoadLimits limits;
    constexpr std::int64_t mib=1024*1024;
    limits.fileBytes=(std::int64_t)bounded(Localisation::setting("sample.maxFileMiB","512").getDoubleValue(),512,1,512)*mib;
    limits.decodedBytes=(std::int64_t)bounded(Localisation::setting("sample.maxDecodedMiB","512").getDoubleValue(),512,1,512)*mib;
    limits.totalBytes=(std::int64_t)bounded(Localisation::setting("sample.totalMiB","2048").getDoubleValue(),2048,64,2048)*mib;
    limits.seconds=Localisation::setting("sample.maxSeconds","1800").getDoubleValue();limits.normalise();return limits;
}
void MainComponent::cancelSampleImport(){
    if(!sampleImportJob)return;
    if(sampleImportRestore)for(size_t i=0;i<project.tracks.size();++i){const auto& t=project.tracks[i];if(t.sessionId==sampleImportSession&&t.instrument==sampleImportInstrument&&t.instrumentPath==sampleImportPath)audio.markMissing((int)i);}
    sampleImportNext.reset();
    sampleImportJob->signalThreadShouldExit();sampleImportJob->finish.signal();
    retiredJobs.retire(std::move(sampleImportJob));lastSampleImportStatus="Sample import cancelled.";
}
juce::String MainComponent::sampleImportMessage()const{
    if(sampleImportJob)return "Loading sample... "+juce::String((int)(sampleImportJob->progress.load()*100))+"%";
    return lastSampleImportStatus;
}
void MainComponent::loadSampleFile(int track,const juce::File& file){
    if(!juce::isPositiveAndBelow(track,(int)project.tracks.size())||loadingPlugin||editingRestricted())return;
    const auto& t=project.tracks[(size_t)track];SamplerSettings settings=t.vocalChops?vocalSamplerSettings():SamplerSettings{};
    if(!t.vocalChops){settings.mode=isDrumTrack(track)?1:0;settings.polyphony=8;}
    const int root=isDrumTrack(track)?(t.engine.midiNote>=0?t.engine.midiNote:fgd::defaultNote(track-4)):60;
    startSampleImport(track,file,settings,root,t.vocalChops,false);
}
void MainComponent::loadSamplePreset(int track,const juce::File& file){
    if(!juce::isPositiveAndBelow(track,(int)project.tracks.size())||loadingPlugin||editingRestricted())return;
    startSampleImport(track,file,{},60,false,true);
}
void MainComponent::importAudioTakeAsync(const AudioTake& take,bool muted){
    if(loadingPlugin||editingRestricted())return;
    cancelSampleImport();auto next=project;const int target=addAudioTake(next,take,muted);
    if(target<0){message("Cannot import audio take: invalid audio or track limit.",true);return;}
    const auto settings=next.tracks[(size_t)target].sampler;const int root=next.tracks[(size_t)target].sampleRoot;
    sampleImportNextTrack=target;sampleImportProjectRevision=uiRevision;sampleImportNext=std::move(next);sampleImportRestore=false;
    sampleImportJob=std::make_unique<SampleImportJob>(take.file,settings,root,false,audio.spectrumSampleRate(),sampleImportLimits(),false,false);
    if(!sampleImportJob->startThread()){sampleImportJob.reset();sampleImportNext.reset();message("Cannot start sample import worker.",true);}
}
void MainComponent::startSampleImport(int track,const juce::File& file,SamplerSettings settings,int root,bool automaticSlices,bool preset,bool restore){
    if(!juce::isPositiveAndBelow(track,(int)project.tracks.size()))return;
    cancelSampleImport();const auto& t=project.tracks[(size_t)track];
    sampleImportSession=t.sessionId;sampleImportInstrument=t.instrument;sampleImportPath=t.instrumentPath;
    sampleImportBefore=t.sampler;sampleImportRoot=t.sampleRoot;sampleImportRestore=restore;
    sampleImportJob=std::make_unique<SampleImportJob>(file,settings,root,isDrumTrack(track),audio.spectrumSampleRate(),sampleImportLimits(),automaticSlices,preset);
    if(!sampleImportJob->startThread()){sampleImportJob.reset();lastSampleImportStatus="Cannot start sample import worker.";message(lastSampleImportStatus,true);}
}
void MainComponent::pollSampleImport(){
    if(!sampleImportJob)return;
    const bool appending=sampleImportNext.has_value();
    int track=appending?sampleImportNextTrack:-1;
    if(appending&&uiRevision!=sampleImportProjectRevision){cancelSampleImport();return;}
    if(!appending)for(size_t i=0;i<project.tracks.size();++i)if(project.tracks[i].sessionId==sampleImportSession){track=(int)i;break;}
    if(track<0){cancelSampleImport();return;}
    auto& t=appending?sampleImportNext->tracks[(size_t)track]:project.tracks[(size_t)track];
    if(!appending&&(t.instrument!=sampleImportInstrument||t.instrumentPath!=sampleImportPath||!(t.sampler==sampleImportBefore)||t.sampleRoot!=sampleImportRoot)){cancelSampleImport();return;}
    if(!sampleImportJob->ready.load(std::memory_order_acquire))return;
    if(!sampleImportRestore&&(editingRestricted()||loadingPlugin))return;
    const bool restoring=sampleImportRestore;
    auto completed=std::move(sampleImportJob);auto& job=*completed;
    auto appended=std::move(sampleImportNext);sampleImportNext.reset();
    auto result=job.result;
    if(result.wasOk())result=audio.installPreparedSample(track,job.prepared,job.retired,[&]{
        if(!restoring){closePluginEditor(track,0);audio.invalidatePluginState(track,0);checkpoint();}
    });
    if(result.wasOk()){
        if(appending){project=std::move(*appended);project.tracks[(size_t)track].sampleHash=job.asset->hash.toStdString();select(track,project.sectionAtBeat(project.clipStart(track,0)));changed(true,track);piano.fitNotes();updateSizes();}
        else if(!restoring){const auto previous=t.sampler;t.sampler=job.settings;t.sampleRoot=job.root;
            remapVocalChops(project,track,previous,true);t.instrument="sample";t.instrumentPath=job.file.getFullPathName().toStdString();
            t.sampleHash=job.asset->hash.toStdString();t.instrumentId.clear();t.instrumentState.clear();changed(false);sampleBrowser.refreshTarget();}
        lastSampleImportStatus="Sample loaded: "+job.file.getFileName();
    }else{
        lastSampleImportStatus=result.getErrorMessage();
        if(restoring){audio.markMissing(track);reportExternal(lastSampleImportStatus+" · "+juce::String(t.name));}
        else message(lastSampleImportStatus,true);
    }
    if(studioWindow)studioWindow->refreshSampleState();
    job.finish.signal();retiredJobs.retire(std::move(completed));
    if(restoring)audio.publish(project);
}
bool MainComponent::waitForSampleImportForTests(int timeoutMs){
#if FGA_SELF_TESTS
    const auto deadline=juce::Time::getMillisecondCounterHiRes()+timeoutMs;
    while((sampleImportActive()||restoringProject)&&juce::Time::getMillisecondCounterHiRes()<deadline){
        juce::MessageManager::getInstance()->runDispatchLoopUntil(5);pollSampleImport();updateLoadingProgress();
    }
    return !sampleImportActive()&&!restoringProject;
#else
    juce::ignoreUnused(timeoutMs);return false;
#endif
}
void MainComponent::showSampleImportLimits(){
    auto* dialog=new juce::AlertWindow("Sample import limits","Limits apply to new file imports. Total includes prepared and retained file PCM/peaks; DSP and codec overhead are additional.",juce::MessageBoxIconType::NoIcon);
    const auto limits=sampleImportLimits();
    dialog->addTextEditor("file",juce::String(limits.fileBytes/(1024*1024)),"File size MiB (1–512)");
    dialog->addTextEditor("decoded",juce::String(limits.decodedBytes/(1024*1024)),"Decoded sample MiB (1–512)");
    dialog->addTextEditor("total",juce::String(limits.totalBytes/(1024*1024)),"Total decoded MiB (64–2048)");
    dialog->addTextEditor("seconds",juce::String(limits.seconds,0),"Duration seconds (1–1800)");
    dialog->addButton("Apply",1);dialog->addButton("Cancel",0);dialog->setAlwaysOnTop(true);
    dialog->enterModalState(true,juce::ModalCallbackFunction::create([dialog](int result){if(result==1){
        SampleLoadLimits next;constexpr std::int64_t mib=1024*1024;
        auto numeric=[dialog](const char* name,double fallback,double lo,double hi){const auto text=dialog->getTextEditorContents(name).trim();if(text.isEmpty()||text.containsAnyOf("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"))return fallback;return bounded(text.getDoubleValue(),fallback,lo,hi);};
        next.fileBytes=(std::int64_t)numeric("file",512,1,512)*mib;next.decodedBytes=(std::int64_t)numeric("decoded",512,1,512)*mib;
        next.totalBytes=(std::int64_t)numeric("total",2048,64,2048)*mib;next.seconds=numeric("seconds",1800,1,1800);next.normalise();
        Localisation::setSetting("sample.maxFileMiB",juce::String(next.fileBytes/mib));Localisation::setSetting("sample.maxDecodedMiB",juce::String(next.decodedBytes/mib));
        Localisation::setSetting("sample.totalMiB",juce::String(next.totalBytes/mib));Localisation::setSetting("sample.maxSeconds",juce::String(next.seconds));
    }}),true);
}
}
namespace fga {
bool MainComponent::verifySampleImport(const juce::File& folder){
#if !FGA_SELF_TESTS
    juce::ignoreUnused(folder);return false;
#else
    folder.createDirectory();bool ok=true;juce::String report;
    auto check=[&](bool passed,const char* name){ok&=passed;report+=(passed?"PASS ":"FAIL ")+juce::String(name)+"\n";};
    struct Preferences{std::array<const char*,4> keys{{"sample.maxFileMiB","sample.maxDecodedMiB","sample.totalMiB","sample.maxSeconds"}};std::array<juce::String,4> values;
        Preferences(){for(size_t i=0;i<keys.size();++i)values[i]=Localisation::setting(keys[i],i==2?"2048":i==3?"1800":"512");}
        ~Preferences(){for(size_t i=0;i<keys.size();++i)Localisation::setSetting(keys[i],values[i]);}} prefs;
    Localisation::setSetting("sample.maxFileMiB","512");Localisation::setSetting("sample.maxDecodedMiB","512");Localisation::setSetting("sample.totalMiB","2048");Localisation::setSetting("sample.maxSeconds","1800");
    audio.stop();project=Project{};project.sections={{Section::main,8,"Import"}};project.normalise();
    const int track=project.addMidiTrack();project.tracks[(size_t)track].clips[0].notes={{0,.5,60,100,0}};select(track,0);changed(false);
    const auto file=folder.getChildFile("import.wav");juce::WavAudioFormat wav;
    {std::unique_ptr<juce::OutputStream> stream=file.createOutputStream();auto writer=wav.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(48000).withNumChannels(2).withBitsPerSample(32));
        if(!writer)return false;juce::AudioBuffer<float> block(2,4800);for(int c=0;c<2;++c)for(int i=0;i<4800;++i)block.setSample(c,i,.3f*std::sin(i*.03f));for(int i=0;i<80;++i)writer->writeFromAudioSampleBuffer(block,0,4800);}
    // Same decoder/DSP preparation on the caller thread is the local synchronous reference.
    const auto referenceStart=juce::Time::getMillisecondCounterHiRes();
    {std::shared_ptr<const SampleAsset> asset;if(readSampleAsset(file,asset).failed())return false;SampleSynth reference(asset,{},60,false);reference.setCurrentPlaybackSampleRate(audio.spectrumSampleRate());}
    const double synchronousMs=juce::Time::getMillisecondCounterHiRes()-referenceStart;
    const auto notes=project.tracks[(size_t)track].clips;const auto history=undoStack.size();
    auto heartbeat=std::make_shared<bool>(false);juce::MessageManager::callAsync([heartbeat]{*heartbeat=true;});const auto began=juce::Time::getMillisecondCounterHiRes();
    loadSampleFile(track,file);const double dispatchMs=juce::Time::getMillisecondCounterHiRes()-began;
    check(sampleImportActive()&&!audio.sampleAsset(track)&&undoStack.size()==history,"load returns asynchronously without changing source or Undo");
    check(waitForSampleImportForTests()&&*heartbeat,"event loop remains responsive during import");
    const double completionMs=juce::Time::getMillisecondCounterHiRes()-began;
    check(dispatchMs<250,"sample request dispatch remains bounded on local fixture");
    check(audio.sampleAsset(track)&&project.tracks[(size_t)track].clips==notes&&undoStack.size()==history+1,"successful import preserves MIDI and creates one checkpoint");
    if(!audio.sampleAsset(track)){folder.getChildFile("sample-import.txt").replaceWithText(report);return false;}
    undo(false);waitForSampleImportForTests();check(project.tracks[(size_t)track].instrument=="internal"&&!audio.sampleAsset(track),"Undo restores previous source");
    undo(true);check(!audio.offlineCopy(track),"pending sample restore cannot render an internal fallback instrument");
    check(waitForSampleImportForTests()&&audio.sampleAsset(track),"Redo asynchronously restores saved sample");
    const auto retained=audio.sampleAsset(track);const auto previousHistory=undoStack.size();
    Localisation::setSetting("sample.maxFileMiB","1");loadSampleFile(track,file);waitForSampleImportForTests();
    check(audio.sampleAsset(track)==retained&&undoStack.size()==previousHistory&&lastSampleImportStatus.contains("file limit"),"configured limit preserves source/MIDI/Undo with clear error");Localisation::setSetting("sample.maxFileMiB","512");
    loadSampleFile(track,file);sampleButton.onClick();waitForSampleImportForTests();check(audio.sampleAsset(track)==retained&&undoStack.size()==previousHistory,"cancel preserves old sample and history");
    loadSampleFile(track,file);project.tracks[(size_t)track].sampler.transpose=7;pollSampleImport();check(!sampleImportActive()&&audio.sampleAsset(track)==retained,"edited sampler rejects stale completion");project.tracks[(size_t)track].sampler.transpose=0;
    const int obsolete=project.addMidiTrack();loadSampleFile(obsolete,file);project.tracks.erase(project.tracks.begin()+obsolete);pollSampleImport();check(!sampleImportActive(),"deleted target cannot receive late sample data");
    report+="Local synchronous preparation ms="+juce::String(synchronousMs,3)+"; async dispatch ms="+juce::String(dispatchMs,3)+"; total import ms="+juce::String(completionMs,3)+"\n";
    folder.getChildFile("sample-import.txt").replaceWithText(report);return ok;
#endif
}
}
