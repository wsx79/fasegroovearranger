// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MainComponent.h"
#include "Localisation.h"
#include "StudioWindow.h"
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
void MainComponent::importAudioTakeAsync(const AudioTake& take,bool muted,bool analyse){
    if(loadingPlugin||editingRestricted())return;
    cancelSampleImport();auto next=project;const int target=addAudioTake(next,take,muted);
    if(target<0){message("Cannot import audio take: invalid audio or track limit.",true);return;}
    const auto settings=next.tracks[(size_t)target].sampler;const int root=next.tracks[(size_t)target].sampleRoot;
    sampleImportNextTrack=target;sampleImportProjectRevision=editRevision;sampleImportNext=std::move(next);sampleImportRestore=false;
    sampleImportJob=std::make_unique<SampleImportJob>(take.file,settings,root,false,audio.spectrumSampleRate(),sampleImportLimits(),false,false);sampleImportJob->analyse=analyse;
    if(!sampleImportJob->startThread()){sampleImportJob.reset();sampleImportNext.reset();message("Cannot start sample import worker.",true);}
}
void MainComponent::startSampleImport(int track,const juce::File& file,SamplerSettings settings,int root,bool automaticSlices,bool preset,bool restore){
    if(!juce::isPositiveAndBelow(track,(int)project.tracks.size()))return;
    cancelSampleImport();const auto& t=project.tracks[(size_t)track];
    sampleImportSession=t.sessionId;sampleImportInstrument=t.instrument;sampleImportPath=t.instrumentPath;
    sampleImportBefore=t.sampler;sampleImportRoot=t.sampleRoot;sampleImportRestore=restore;
    sampleImportJob=std::make_unique<SampleImportJob>(file,settings,root,isDrumTrack(track),audio.spectrumSampleRate(),sampleImportLimits(),automaticSlices,preset);sampleImportJob->detectTempo=!restore&&!preset;
    if(!sampleImportJob->startThread()){sampleImportJob.reset();lastSampleImportStatus="Cannot start sample import worker.";message(lastSampleImportStatus,true);}
}
void MainComponent::pollSampleImport(){
    if(!sampleImportJob)return;
    const bool appending=sampleImportNext.has_value();
    int track=appending?sampleImportNextTrack:-1;
    if(appending&&editRevision!=sampleImportProjectRevision){cancelSampleImport();return;}
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
        if(appending){project=std::move(*appended);project.tracks[(size_t)track].sampleHash=job.asset->hash.toStdString();
            if(job.analyse){auto& imported=project.tracks[(size_t)track];const bool embedded=job.embedded.bpm>0;imported.audioBpm=embedded?job.embedded.bpm:job.tempo.bpm;imported.audioBpmConfidence=embedded?1.:job.tempo.confidence;imported.audioKey=job.key.tonal?job.key.key:-1;imported.audioMinor=job.key.minor;imported.audioKeyConfidence=job.key.confidence;
                if(const double bpm=job.chosenBpm();bpm>0)imported.sampler.sourceBpm=bpm;}select(track,project.sectionAtBeat(project.clipStart(track,0)));changed(true,track);piano.fitNotes();updateSizes();
            if(job.analyse&&openStudioAfterImport){openStudioAfterImport=false;showStudio(0);}}
        else if(!restoring){const auto previous=t.sampler;t.sampler=job.settings;t.sampleRoot=job.root;
            // A new sample takes its own tempo: written in the file (ACID / Apple loop), in its name, or measured.
            if(job.detectTempo){if(const double bpm=job.chosenBpm();bpm>0){t.sampler.sourceBpm=bpm;t.audioBpm=bpm;t.audioBpmConfidence=job.embedded.bpm>0?1.:job.tempo.confidence;}}
            remapVocalChops(project,track,previous,true);t.instrument="sample";t.instrumentPath=job.file.getFullPathName().toStdString();
            t.sampleHash=job.asset->hash.toStdString();t.instrumentId.clear();t.instrumentState.clear();changed(false);sampleBrowser.refreshTarget();}
        lastSampleImportStatus="Sample loaded: "+job.file.getFileName();if(job.embedded.bpm>0)lastSampleImportStatus+="  ·  "+juce::String(job.embedded.bpm,job.embedded.bpm==std::round(job.embedded.bpm)?0:1)+" BPM ("+tr(job.embedded.source)+")";else if(!appending&&job.detectTempo&&job.chosenBpm()>0)lastSampleImportStatus+="  ·  "+juce::String(job.chosenBpm(),1)+" BPM ("+tr("measured")+")";if(appending&&job.analyse){if(job.embedded.bpm<=0)lastSampleImportStatus+="  ·  "+audioAnalysisSummary(project.tracks[(size_t)track]);message(lastSampleImportStatus);}else if(!appending&&!restoring&&job.detectTempo&&job.chosenBpm()>0)message(lastSampleImportStatus);
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
    {   // A dropped audio file becomes an audio clip at the drop beat, analysed for tempo and key.
        const auto loopFile=folder.getChildFile("dropped-124bpm.wav");{juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(loopFile.createOutputStream().release(),44100,2,24,{},0));
            juce::AudioBuffer<float> b(2,44100*12);for(int i=0;i<b.getNumSamples();++i){const double t=i/44100.,in=std::fmod(t,60./124.);const float v=(float)(.7*std::exp(-in*30)*std::sin(2*juce::MathConstants<double>::pi*60*in)+.2*std::sin(2*juce::MathConstants<double>::pi*220*t)*.3);b.setSample(0,i,v);b.setSample(1,i,v);}
            if(w)w->writeFromAudioSampleBuffer(b,0,b.getNumSamples());}
        const auto tracksBefore=project.tracks.size();importAudioClip(loopFile,selectedTrack,8,false);const bool done=waitForSampleImportForTests();
        const int added=(int)project.tracks.size()-1;const bool created=done&&project.tracks.size()==tracksBefore+1&&project.tracks[(size_t)added].clips.size()==1&&std::abs(project.clipStart(added,0)-8)<1e-9&&project.tracks[(size_t)added].clips[0].audioOffset>=0;
        check(created,"dropped audio becomes an audio clip at the drop beat on a new audio track");
        if(created){const auto& t=project.tracks[(size_t)added];check(std::abs(t.audioBpm-124)<.6&&t.audioBpmConfidence>.3,"imported audio tempo detected (124 BPM)");
            const double seconds=t.clips[0].audioDuration;adaptAudioTrack(added,0);check(project.tracks[(size_t)added].sampler.sync&&std::abs(project.clipLength(added,0)-seconds*project.tracks[(size_t)added].audioBpm/60.)<1e-3,"follow project tempo stretches the region to its musical length");
            adaptAudioTrack(added,2);check(!project.tracks[(size_t)added].sampler.sync&&std::abs(project.clipLength(added,0)-seconds*project.settings.bpm/60.)<1e-6,"original tempo restores the native length");
            undo(false);undo(false);}
        loopFile.deleteFile();
    }
    {   // A stem with silence before and after it: TRIM SILENCE moves the audio clip's source,
        // the clip keeps its place and shortens; Studio trims and gain behave the same way.
        const auto stemFile=folder.getChildFile("stem-with-silence.wav");{juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(stemFile.createOutputStream().release(),44100,2,24,{},0));
            juce::AudioBuffer<float> b(2,44100*5);b.clear();for(int i=44100;i<44100*4;++i){const float v=.5f*std::sin((float)i*.05f);b.setSample(0,i,v);b.setSample(1,i,v);}
            if(w)w->writeFromAudioSampleBuffer(b,0,b.getNumSamples());}
        importAudioClip(stemFile,selectedTrack,4,false);const bool done=waitForSampleImportForTests();const int added=(int)project.tracks.size()-1;
        const bool region=done&&!project.tracks[(size_t)added].clips.empty()&&project.tracks[(size_t)added].clips[0].audioOffset>=0;check(region,"stem with silence becomes an audio clip");
        if(region){const auto history=undoStack.size();const double position=project.clipStart(added,0);checkpoint();const auto before=project.tracks[(size_t)added].sampler;
            const bool trimmed=trimSampleSilence(added)&&syncAudioRegions(added,before);const auto& c=project.tracks[(size_t)added].clips[0];
            check(trimmed&&std::abs(c.audioOffset-(1.-.005))<.003&&std::abs(c.audioDuration-(3.+.005+.02))<.004&&std::abs(project.clipStart(added,0)-position)<1e-9
                  &&std::abs(project.clipLength(added,0)-c.audioDuration*project.settings.bpm/60.)<1e-6,"TRIM SILENCE starts the clip on the sound and ends it after the tail, in place");
            auto s=project.tracks[(size_t)added].sampler;const auto beforeEnd=s;s.end-=.1;project.tracks[(size_t)added].sampler=s;syncAudioRegions(added,beforeEnd);
            check(std::abs(project.tracks[(size_t)added].clips[0].audioDuration-(3.025-.5))<.004,"a Studio END trim shortens the audio clip by the same time");
            {auto fast=project.tracks[(size_t)added].sampler;const auto beforeFast=fast;const double length=project.clipLength(added,0);fast.transpose=12;project.tracks[(size_t)added].sampler=fast;syncAudioRegions(added,beforeFast);
             check(std::abs(project.clipLength(added,0)-length/2)<1e-6,"without Tempo Sync, +12 semitones plays twice as fast and the clip halves");
             const auto beforeSlow=project.tracks[(size_t)added].sampler;project.tracks[(size_t)added].sampler.transpose=0;syncAudioRegions(added,beforeSlow);}
            auto timing=project.tracks[(size_t)added].sampler;const auto beforeSync=timing;timing.sync=true;timing.sourceBpm=project.settings.bpm*2;project.tracks[(size_t)added].sampler=timing;syncAudioRegions(added,beforeSync);
            check(std::abs(project.clipLength(added,0)-project.tracks[(size_t)added].clips[0].audioDuration*timing.sourceBpm/60.)<1e-6,"Tempo Sync and SOURCE BPM set the audio clip length");
            SamplerSettings gain;gain.gainDb=-6.5;SamplerSettings read;check(readSampler(samplerJson(gain),read).wasOk()&&read.gainDb==-6.5&&!(read==SamplerSettings{}),"sample GAIN is saved and reloaded");
            undo(false);check(undoStack.size()==history&&std::abs(project.tracks[(size_t)added].clips[0].audioOffset-0)<1e-9,"Undo restores the untrimmed clip");
            undo(false);}
        stemFile.deleteFile();
    }
    {   // End to end through the interface: drop -> the Studio opens on the track; the track's
        // instrument button and a double-click on the clip open it again; the Studio's own knobs
        // edit the clip, and the rendered audio starts on the sound at the clip's start.
        const auto stemFile=folder.getChildFile("stem-late-start.wav");{juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(stemFile.createOutputStream().release(),44100,2,24,{},0));
            juce::AudioBuffer<float> b(2,44100*5);b.clear();for(int i=44100;i<44100*4;++i){const float v=.5f*std::sin((float)i*.031f);b.setSample(0,i,v);b.setSample(1,i,v);}
            if(w)w->writeFromAudioSampleBuffer(b,0,b.getNumSamples());}
        waitForSampleImportForTests();juce::MessageManager::getInstance()->runDispatchLoopUntil(50);   // let Undo's sample restores of the earlier blocks finish
        lastSampleImportStatus.clear();const auto editsBefore=editRevision;importAudioClip(stemFile,selectedTrack,4,true);select(0,0);   // selecting another track while it imports must not drop it
        const bool done=waitForSampleImportForTests();juce::MessageManager::getInstance()->runDispatchLoopUntil(80);const int added=(int)project.tracks.size()-1;
        auto studioOn=[&]{return studioWindow&&studioWindow->isVisible()&&studioWindow->editedTrack()==added;};auto closeStudio=[&]{if(studioWindow)studioWindow->setVisible(false);};
        check(done&&studioOn(),"dropping audio opens the Studio on its new track, even after selecting another track meanwhile");if(!(done&&studioOn()))report+="INFO done="+juce::String((int)done)+" tracks="+juce::String((int)project.tracks.size())+" last="+juce::String(project.tracks.back().name)+" active="+juce::String((int)sampleImportActive())+" restricted="+juce::String((int)editingRestricted())+" edits="+juce::String((int)(editRevision-editsBefore))+" size="+juce::String(stemFile.getSize())+" status="+lastSampleImportStatus+"\n";
        closeStudio();select(0,0);openInstrumentFor(added);check(studioOn(),"the audio track's instrument button opens the Studio on it");
        closeStudio();select(0,0);showArrangement();arrangementPort.setViewPosition(0,0);resized();
        {int row=-1;for(int r=0;r<(int)project.tracks.size();++r)if(trackAtRow(r)==added){row=r;break;}
         const float x=(float)(216+project.clipStart(added,0)*arrangement.pixelsPerBar/4.+3.),y=(float)(arrangement.rowY(row)+arrangement.rowHeight(row)-6);
         const auto source=juce::Desktop::getInstance().getMainMouseSource();const auto now=juce::Time::getCurrentTime();const juce::Point<float> at(x,y);
         if(row>=0)arrangement.mouseDoubleClick(juce::MouseEvent(source,at,juce::ModifierKeys::leftButtonModifier,1,0,0,0,0,&arrangement,&arrangement,now,at,now,2,false));
         check(row>=0&&studioOn()&&page==Page::arrangement,"double-clicking the audio clip opens the Studio on it (not the MIDI editor)");}
        if(studioOn()){const auto before=undoStack.size();const double position=project.clipStart(added,0);
            const bool moved=studioWindow->setControlForTests("START",1./5.);const auto& c=project.tracks[(size_t)added].clips[0];
            check(moved&&std::abs(c.audioOffset-1.)<.002&&std::abs(c.audioDuration-4.)<.002&&std::abs(project.clipStart(added,0)-position)<1e-9&&undoStack.size()==before+1,
                  "Studio START at 1 s: the clip keeps its place and starts on the sound (one Undo)");
            // Export only this track (the Export menu's track render) and find where the sound begins.
            const auto exported=folder.getChildFile("stem-late-start-export.wav");exported.deleteFile();
            const auto rendered=audio.exportAudio(project,exported,added,false,[]{return false;},[](double){});
            double onset=-1;{juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(exported));
                if(reader){juce::AudioBuffer<float> mix((int)reader->numChannels,(int)reader->lengthInSamples);reader->read(&mix,0,mix.getNumSamples(),0,true,true);
                    for(int i=0;i<mix.getNumSamples()&&onset<0;++i)for(int ch=0;ch<mix.getNumChannels();++ch)if(std::abs(mix.getSample(ch,i))>.01f){onset=i/reader->sampleRate;break;}}}
            const double expected=position*60./project.settings.bpm;exported.deleteFile();
            check(rendered.wasOk()&&onset>=0&&std::abs(onset-expected)<.03,"exported audio starts on the sound at the clip start");report+="MEASURED "+(rendered.wasOk()?juce::String():rendered.getErrorMessage()+" ")+"sound onset "+juce::String(onset,3)+" s, clip start "+juce::String(expected,3)+" s\n";
            const double length=project.clipLength(added,0);studioWindow->setControlForTests("TRANSPOSE",12);
            check(std::abs(project.clipLength(added,0)-length/2)<1e-6,"Studio TRANSPOSE +12 (no Tempo Sync): twice as fast, the clip on the track halves");
            studioWindow->setControlForTests("GAIN (dB)",-6);check(std::abs(project.tracks[(size_t)added].sampler.gainDb+6)<1e-9,"Studio GAIN sets the clip's level");
            studioWindow->trimSilenceForTests();check(std::abs(project.tracks[(size_t)added].clips[0].audioOffset-(1.-.005))<.003,"TRIM SILENCE button trims the clip on the track");
            while(undoStack.size()>before)undo(false);check(std::abs(project.tracks[(size_t)added].clips[0].audioOffset)<1e-9&&project.tracks[(size_t)added].sampler.gainDb==0,"Undo restores the clip as dropped");}
        closeStudio();undo(false);stemFile.deleteFile();
    }
    {   // A new sample takes its own tempo: the ACID chunk, else the file name, else a measured one.
        waitForSampleImportForTests();juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
        auto writeLoop=[&](const juce::File& f,const juce::StringPairArray& meta,bool pulses){juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(f.createOutputStream().release(),44100,2,24,meta,0));
            juce::AudioBuffer<float> b(2,44100*6);juce::Random random(7);for(int i=0;i<b.getNumSamples();++i){const double t=i/44100.,in=std::fmod(t,60./124.);const float v=pulses?(float)(.7*std::exp(-in*30)*std::sin(2*juce::MathConstants<double>::pi*60*t)):.2f*(random.nextFloat()*2-1);b.setSample(0,i,v);b.setSample(1,i,v);}
            if(w)w->writeFromAudioSampleBuffer(b,0,b.getNumSamples());};
        check(loopTempoFromName("Groove_124BPM")==124&&loopTempoFromName("deep chords 97 bpm")==97&&loopTempoFromName("Kick 01")==0&&loopTempoFromName("vox 12.5bpm")==0,"tempo in the file name (124BPM, 97 bpm; none in 'Kick 01')");
        auto loaded=[&](const juce::File& f){const double before=project.tracks[(size_t)track].sampler.sourceBpm;juce::ignoreUnused(before);loadSampleFile(track,f);waitForSampleImportForTests();return project.tracks[(size_t)track].sampler.sourceBpm;};
        juce::StringPairArray acid;acid.set(juce::WavAudioFormat::acidTempo,"128");acid.set(juce::WavAudioFormat::acidBeats,"8");acid.set(juce::WavAudioFormat::acidStretch,"1");
        const auto acidFile=folder.getChildFile("acidized-loop.wav");writeLoop(acidFile,acid,true);
        check(readLoopTempo(acidFile).bpm==128&&readLoopTempo(acidFile).source=="ACID","ACID tempo read from an acidized WAV");
        check(std::abs(loaded(acidFile)-128)<1e-9,"an acidized WAV loaded on a track takes its ACID tempo (128, not the project's)");
        const auto namedFile=folder.getChildFile("noise loop 97bpm.wav");writeLoop(namedFile,{},false);
        check(std::abs(loaded(namedFile)-97)<1e-9,"a loop with the tempo in its name takes it (97 BPM)");
        const auto measuredFile=folder.getChildFile("pulses.wav");writeLoop(measuredFile,{},true);const double measured=loaded(measuredFile);
        check(std::abs(measured-124)<.6,"a loop with no tempo data is measured (124 BPM)");report+="MEASURED loaded loop tempo "+juce::String(measured,2)+" BPM\n";
        // Dropped on the timeline: the ACID tempo wins over the measurement, and the front edge trims.
        importAudioClip(acidFile,selectedTrack,8,false);const bool dropped=waitForSampleImportForTests();const int added=(int)project.tracks.size()-1;
        check(dropped&&project.tracks[(size_t)added].audioBpm==128&&project.tracks[(size_t)added].sampler.sourceBpm==128,"a dropped acidized loop is marked 128 BPM");
        if(dropped&&project.tracks[(size_t)added].clips.size()==1){auto& t=project.tracks[(size_t)added];const double start=project.clipStart(added,0),end=start+project.clipLength(added,0),spb=t.clips[0].audioDuration/project.clipLength(added,0);
            {   // The real gesture: press on the clip's front edge and drag it right by two beats (Shift: free timing).
                showArrangement();arrangement.pixelsPerBar=40;resized();arrangementPort.setViewPosition(0,0);int row=-1;for(int r=0;r<(int)project.tracks.size();++r)if(trackAtRow(r)==added){row=r;break;}
                const auto history=undoStack.size();const float x=(float)(216+start*arrangement.pixelsPerBar/4.+2.),y=(float)(arrangement.rowY(row)+arrangement.rowHeight(row)-8);
                const auto source=juce::Desktop::getInstance().getMainMouseSource();auto mouse=[&](float px,juce::ModifierKeys mods){const auto now=juce::Time::getCurrentTime();const juce::Point<float> at(px,y);return juce::MouseEvent(source,at,mods,1,0,0,0,0,&arrangement,&arrangement,now,{x,y},now,1,px!=x);};
                const auto held=juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier|juce::ModifierKeys::shiftModifier);const float dx=(float)(2*arrangement.pixelsPerBar/4.);
                if(row>=0){arrangement.mouseDown(mouse(x,juce::ModifierKeys::leftButtonModifier));arrangement.mouseDrag(mouse(x+dx,held));arrangement.mouseUp(mouse(x+dx,held));}
                check(row>=0&&std::abs(project.clipStart(added,0)-(start+2))<.02&&std::abs(project.clipStart(added,0)+project.clipLength(added,0)-end)<1e-6&&undoStack.size()==history+1,"dragging the front edge with the mouse trims the clip start (one Undo)");
                if(undoStack.size()>history)undo(false);arrangement.pixelsPerBar=6;resized();}
            checkpoint();const bool front=project.trimClipStart(added,0,start+2);const auto& c=project.tracks[(size_t)added].clips[0];
            check(front&&std::abs(project.clipStart(added,0)-(start+2))<1e-9&&std::abs(project.clipStart(added,0)+project.clipLength(added,0)-end)<1e-9&&std::abs(c.audioOffset-2*spb)<1e-6,"dragging an audio clip's front edge trims its start, the end and the audio stay in place");
            project.trimClipStart(added,0,start-4);check(std::abs(project.clipStart(added,0)-start)<1e-9&&std::abs(project.tracks[(size_t)added].clips[0].audioOffset)<1e-9,"the front edge cannot go before the start of the file");
            undo(false);}
        {const int midi=project.addMidiTrack();if(midi>=0&&project.tracks[(size_t)midi].freeTimeline&&!project.tracks[(size_t)midi].clips.empty()){auto& c=project.tracks[(size_t)midi].clips[0];c.loopLength=c.loopPhase=0;c.loopSource.clear();c.notes={{1,1,60,100,0},{3,.5,62,100,0}};
            const double start=project.clipStart(midi,0);project.trimClipStart(midi,0,start+2);const auto& n=project.tracks[(size_t)midi].clips[0].notes;
            check(n.size()==1&&std::abs(n[0].start-1)<1e-9&&n[0].pitch==62,"a MIDI clip's front edge keeps the remaining notes where they were in time");}
         else check(false,"free MIDI track for the front-edge test");}
        {   // Deleting a handwritten track before an audio track keeps the audio loaded (no reload).
            waitForSampleImportForTests();const int midi=project.addMidiTrack();changed(false);importAudioClip(acidFile,selectedTrack,4,false);const bool done=waitForSampleImportForTests();const int audioTrack=(int)project.tracks.size()-1;
            const auto asset=audio.sampleAsset(audioTrack);const auto session=project.tracks[(size_t)audioTrack].sessionId;
            if(done&&midi>=0&&audioTrack==midi+1&&asset){select(midi,0);deleteMidiTrack(midi);
                check(project.tracks[(size_t)midi].sessionId==session&&audio.sampleAsset(midi)==asset&&!sampleImportActive()&&!restoringProject&&!loadingView.isVisible(),"deleting a MIDI track keeps the following audio track loaded, with no reload");
                {const auto out=folder.getChildFile("moved-audio-export.wav");const auto r=audio.exportAudio(project,out,midi,false,[]{return false;},[](double){});float peak=0;
                 {juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(out));if(reader){juce::AudioBuffer<float> b((int)reader->numChannels,(int)reader->lengthInSamples);reader->read(&b,0,b.getNumSamples(),0,true,true);peak=b.getMagnitude(0,b.getNumSamples());}}
                 out.deleteFile();check(r.wasOk()&&peak>.05f,"the moved audio track still plays (track export is not silent)");}}
            else check(false,"fixture for deleting a track before an audio track");
            // GENERATE SONG with audio on the timeline renews tempo, key and form; the audio clip
            // keeps matching its audio at the new tempo.
            const int at=done?midi:-1;if(at>=0){project.tracks[(size_t)at].sampler.sync=false;project.refitAudioClips();
                bool renewed=false;double mismatch=0;for(int attempt=0;attempt<4&&!renewed;++attempt){std::vector<int> bars;for(const auto& x:project.sections)bars.push_back(x.bars);const double bpm=project.settings.bpm;
                    generate(project,Scope::song,0,0,(std::uint32_t)(9001+attempt));std::vector<int> after;for(const auto& x:project.sections)after.push_back(x.bars);renewed=after!=bars&&project.settings.bpm!=bpm;}
                const auto& c=project.tracks[(size_t)at].clips[0];mismatch=std::abs(project.clipLength(at,0)-c.audioDuration*project.settings.bpm/60.);
                check(renewed&&mismatch<1e-6,"GENERATE SONG with an audio clip renews the form and tempo; the clip still matches its audio");
                // Studio-style SOURCE BPM edit: Tempo Sync follows, the clip length follows.
                const auto before=project.tracks[(size_t)at].sampler;project.tracks[(size_t)at].sampler.sourceBpm=100;project.tracks[(size_t)at].sampler.sync=true;syncAudioRegions(at,before);
                check(std::abs(project.clipLength(at,0)-project.tracks[(size_t)at].clips[0].audioDuration*100/60.)<1e-6,"SOURCE BPM 100 with Tempo Sync: the clip spans the audio at 100 BPM");}
        }
        for(auto f:{acidFile,namedFile,measuredFile})f.deleteFile();
    }
    folder.getChildFile("sample-import.txt").replaceWithText(report);return ok;
#endif
}
}

namespace fga {
juce::String MainComponent::audioAnalysisSummary(const Track& t)const{
    static const char* names[]{"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
    const auto trust=[](double c){return tr(c>=.7?"reliable":c>=.35?"likely":"uncertain");};
    juce::String s=t.audioBpm>0?juce::String(t.audioBpm,t.audioBpm==std::round(t.audioBpm)?0:1)+" BPM ("+trust(t.audioBpmConfidence)+")":tr("tempo not detected");
    s+="  ·  "+(t.audioKey>=0?juce::String(names[t.audioKey])+(t.audioMinor?"m":"")+" ("+trust(t.audioKeyConfidence)+")":tr("no reliable key (drums, voice or atonal)"));
    return s;
}
void MainComponent::importAudioClip(const juce::File& file,int track,double beat,bool openStudio){
    juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if(!reader||reader->sampleRate<=0||reader->lengthInSamples<=0){message(tr("Cannot read this audio file."),true);return;}
    AudioTake take;take.file=file;take.rate=reader->sampleRate;take.frames=reader->lengthInSamples;take.start=std::max(0.,beat);take.tempo=project.settings.bpm;
    reader.reset();juce::ignoreUnused(track);openStudioAfterImport=openStudio;importAudioTakeAsync(take,false,true);if(!sampleImportActive()&&openStudioAfterImport){openStudioAfterImport=false;}
}
}
