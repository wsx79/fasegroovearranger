// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MainComponent.h"
#include "AssistantProject.h"
#include "RenderProgressTask.h"
#include <future>
#include "GuidePage.h"
#include "UiText.h"
#include "ExportSupport.h"
#include <BinaryData.h>
namespace fga {
namespace {
// JUCE dereferences the stream it is given: a file that cannot be opened yields no reader.
std::unique_ptr<juce::AudioFormatReader> readWavFile(const juce::File& file){auto stream=file.createInputStream();if(!stream)return {};juce::WavAudioFormat wav;return std::unique_ptr<juce::AudioFormatReader>(wav.createReaderFor(stream.release(),true));}
}
namespace {
juce::String u(const std::string& s){return juce::String::fromUTF8(s.c_str());}
AudioExportOptions exportSettings(){
    const auto requested=Localisation::setting("exportSampleRate","48000").getIntValue();
    const int rates[]{44100,48000,88200,96000,192000};
    const bool known=std::find(std::begin(rates),std::end(rates),requested)!=std::end(rates);
    const auto saved=Localisation::setting("exportPeakMode");
    const auto historic=Localisation::setting("exportLoudnessGuard");
    const auto mode=saved=="legacy"||(saved.isEmpty()&&historic.isNotEmpty())?AudioExportMode::legacy:
        saved=="loud"&&offlineLoudReady?AudioExportMode::loud:AudioExportMode::transparent;
    double ceiling=Localisation::setting("exportCeilingDb","-1").getDoubleValue();
    if(!std::isfinite(ceiling))ceiling=-1;
    AudioExportOptions options{known?(double)requested:48000.,Localisation::setting("exportBitDepth","24")=="16"?16:24,mode,std::clamp(ceiling,minExportCeilingDb,maxExportCeilingDb)};
    options.loudnessGuard=historic=="1";return options;
}
void rename(const juce::String& title,const juce::String& value,std::function<void(juce::String)> cb){auto* dialog=new juce::AlertWindow(tr(title),"",juce::MessageBoxIconType::NoIcon);dialog->addTextEditor("name",value,"Nome");dialog->addButton("OK",1,juce::KeyPress(juce::KeyPress::returnKey));dialog->addButton(tr("Cancel"),0,juce::KeyPress(juce::KeyPress::escapeKey));dialog->enterModalState(true,juce::ModalCallbackFunction::create([dialog,cb](int result){if(result==1)cb(dialog->getTextEditorContents("name"));}),true);}
class ExportTask:public RenderProgressTask {
public:
    ExportTask(AudioEngine& e,Project p,juce::File path,std::vector<int> selected,bool mp3,bool batch,AudioExportOptions settings,std::function<void(juce::Result)> done)
    :RenderProgressTask(juce::String::fromUTF8("Fase Groove · Audio export")),engine(e),project(std::move(p)),target(path),tracks(std::move(selected)),isMp3(mp3),stems(batch),options(settings),callback(std::move(done)){}
#if FGA_SELF_TESTS
    std::function<void(double)> progressForTest;
#endif
    void reportProgress(double p){
        setProgress(p);
#if FGA_SELF_TESTS
        if(progressForTest)progressForTest(p);
#endif
    }
    void run()override{
        reportProgress(0);
        if(threadShouldExit()){result=juce::Result::fail("Export cancelled");return;}
        // Write the arrangement map first, so it also accompanies any stems completed
        // before cancellation. Audio files themselves use atomic temporary-file writes.
        if(stems&&!writeExportArrangement(target,project)){result=juce::Result::fail("Cannot write the arrangement text file.");return;}
        const bool parallel=stems&&tracks.size()>1&&std::all_of(tracks.begin(),tracks.end(),[this](int ti){return ti>=0&&(project.tracks[(size_t)ti].instrument=="internal"||project.tracks[(size_t)ti].instrument=="sample")&&project.tracks[(size_t)ti].effects.empty();});
        if(parallel){for(size_t i=0;i<tracks.size();i+=2){if(threadShouldExit()){result=juce::Result::fail("Export cancelled");return;}std::vector<std::future<std::pair<juce::Result,double>>> jobs;for(size_t j=i;j<std::min(i+2,tracks.size());++j){const int track=tracks[j];auto copy=engine.offlineCopy(track);if(!copy){result=juce::Result::fail("Cannot create private stem engine");break;}const auto file=exportStemFile(target,project,track,isMp3?"mp3":"wav");jobs.push_back(std::async(std::launch::async,[this,track,file,copy=std::move(copy)]()mutable{double p=0;auto r=copy->exportAudio(project,file,track,isMp3,[this]{return threadShouldExit();},[](double){},&p,options);return std::make_pair(r,p);}));}for(auto& job:jobs){auto value=job.get();peak=std::max(peak,value.second);if(value.first.failed())result=value.first;}if(result.failed())return;reportProgress((double)std::min(i+2,tracks.size())/tracks.size());}return;}
        for(size_t i=0;i<tracks.size();++i){const int track=tracks[i];const auto file=stems?exportStemFile(target,project,track,isMp3?"mp3":"wav"):target;
            setStatusMessage(file.getFileName());double blockPeak=0;result=engine.exportAudio(project,file,track,isMp3,[this]{return threadShouldExit();},[this,i](double p){reportProgress(((double)i+p)/(double)tracks.size());},&blockPeak,options);peak=std::max(peak,blockPeak);if(result.failed())return;
        }
    }
    void threadComplete(bool cancelled)override{if(failedToStart())result=juce::Result::fail("Cannot start export worker.");if(!cancelled&&result.wasOk()&&options.mode==AudioExportMode::legacy&&peak>1.)result=juce::Result::fail("Exported, but the mix peaked at "+juce::String(juce::Decibels::gainToDecibels((float)peak),1)+" dB and was limited to -0.01 dBFS.");callback(cancelled?juce::Result::fail("Esportazione annullata. I file gia completati restano nella cartella."):result);}
private:AudioEngine& engine;Project project;juce::File target;std::vector<int> tracks;bool isMp3,stems;AudioExportOptions options;std::function<void(juce::Result)> callback;juce::Result result=juce::Result::ok();double peak=0;
};
}
juce::File MainComponent::projectInCommandLine(const juce::String& commandLine){
    {const auto whole=commandLine.trim().unquoted();if(whole.endsWithIgnoreCase(".fga")&&juce::File::isAbsolutePath(whole)&&juce::File(whole).existsAsFile())return juce::File(whole);} // unquoted path with spaces
    for(auto token:juce::StringArray::fromTokens(commandLine,true)){token=token.unquoted().trim();
        if(!token.endsWithIgnoreCase(".fga"))continue;
        if(juce::File::isAbsolutePath(token)&&juce::File(token).existsAsFile())return juce::File(token);
        const auto relative=juce::File::getCurrentWorkingDirectory().getChildFile(token);if(relative.existsAsFile())return relative;}
    return {};
}
bool MainComponent::openFromCommandLine(const juce::String& commandLine){
    const auto file=projectInCommandLine(commandLine);if(file==juce::File())return false;
    open(file);return loadingView.isVisible()||projectFile==file;
}
void MainComponent::showChordMenu(){
    juce::PopupMenu menu;menu.addSectionHeader(tr("EDIT CHORDS"));
    menu.addItem(4000,tr("Circle of Fifths interactive editor..."));menu.addSeparator();
    const auto harmony=activeHarmony(project,selectedSection);auto names=chordNames(harmony);
    for(int slot=0;slot<harmony.chordCount;++slot){juce::PopupMenu chord;juce::PopupMenu degrees;
        for(int d=0;d<7;++d)degrees.addItem(1000+slot*100+d,tr("Degree")+" "+juce::String(d+1),true,harmony.freeChords[(size_t)slot]<0&&harmony.degrees[(size_t)slot]==d);
        chord.addSubMenu(tr("Nella scala"),degrees);
        for(int quality=0;quality<3;++quality){juce::PopupMenu roots;for(int root=0;root<12;++root){int code=(root-harmony.key+12)%12+quality*12;roots.addItem(1000+slot*100+10+code,fgg::pitchClassName(root),true,harmony.freeChords[(size_t)slot]==code);}chord.addSubMenu(quality==0?tr("Major"):quality==1?tr("Minor"):tr("Dominant"),roots);}
        menu.addSubMenu(juce::String(slot+1)+juce::String::fromUTF8(" · ")+juce::String::fromUTF8(names[(size_t)slot].c_str()),chord);
    }
    juce::PopupMenu lengths;for(int n:{1,2,3,4,6,8})lengths.addItem(2000+n,juce::String(n)+" "+tr("chords"),true,n==harmony.chordCount);menu.addSubMenu(tr("Lunghezza progressione"),lengths);
    juce::PopupMenu durations;const int steps[]{8,16,32,64};for(int i=0;i<4;++i)durations.addItem(2100+steps[i],tr(i==0?"1/2 bar":i==1?"1 bar":i==2?"2 bars":"4 bars"),true,harmony.chordSteps==steps[i]);menu.addSubMenu(tr("Each chord"),durations);
    menu.addItem(3000,tr("Nuova progressione casuale"));
    juce::Component::SafePointer<MainComponent> safe(this);menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&chordEditButton),[safe](int id){if(safe)safe->applyChordMenuChoice(id);});
}
void MainComponent::applyChordMenuChoice(int id){
    if(id==0||loadingPlugin||editingRestricted())return;
    if(id==4000){showChordCirclePopover(0);return;}
    const auto harmony=activeHarmony(project,selectedSection);
    const bool chord=id>=1000&&id<1800;const int slot=(id-1000)/100,value=(id-1000)%100;
    if(chord&&(slot>=harmony.chordCount||(value>=7&&value<10)||value>=46))return;
    const bool count=id>2000&&id<=2008,duration=id==2108||id==2116||id==2132||id==2164;
    if(!chord&&!count&&!duration&&id!=3000)return;
    flushGlobalFeel();instrumentPanel.flushRender();checkpoint();
    auto edited=harmony;
    if(chord){if(value<7){edited.degrees[(size_t)slot]=value;edited.freeChords[(size_t)slot]=-1;edited.manualChords[(size_t)slot]=false;}else{edited.freeChords[(size_t)slot]=value-10;edited.manualChords[(size_t)slot]=true;}}
    if(count)edited.chordCount=id-2000;if(duration)edited.chordSteps=id-2100;
    if(id==3000)randomiseHarmony(edited,(std::uint32_t)juce::Random::getSystemRandom().nextInt());
    const bool local=sectionHarmonyEnabled(project,selectedSection);
    if(local)storeHarmony(project,selectedSection,edited);else{project.settings.key=edited.key;project.settings.scale=edited.scale;project.settings.chordCount=edited.chordCount;project.settings.chordSteps=edited.chordSteps;project.settings.degrees=edited.degrees;project.settings.freeChords=edited.freeChords;project.settings.manualChords=edited.manualChords;if(id==3000){project.settings.extension=edited.extension;project.settings.voicing=edited.voicing;}}
    if(local){generate(project,Scope::section,selectedTrack,selectedSection,0);changed();}else applyInstrumentSettings(0,true);
    if(instrumentIsOpen())instrumentPanel.rebuild();
    message(local?"Section chords written into unlocked tracks in this section.":"New chords written into unlocked tracks. GENERATE SONG draws another harmony.");
}

void MainComponent::showAddSectionMenu(){
    if(loadingPlugin||project.sections.size()>=64)return;
    juce::PopupMenu menu;for(int type=0;type<6;++type)menu.addItem(type+1,tr(sectionName((Section)type)));
    juce::Component::SafePointer<MainComponent> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&addSectionButton),[safe](int id){if(safe&&id>=1&&id<=6)safe->addSongSection((Section)(id-1));});
}
void MainComponent::addSongSection(Section type){
    if(loadingPlugin||project.sections.size()>=64)return;
    instrumentPanel.flushRender();checkpoint();const int at=std::clamp(selectedSection+1,0,(int)project.sections.size());
    // Insert a real clip per track (empty ones) and fill the new section in the same
    // undo step: detaching every track left the section silent.
    if(!project.insertSection(at,{type,8,sectionName(type)},-1)){if(!undoStack.empty())undoStack.pop_back();message("Section change exceeds the project limits.",true);refreshControls();return;}selectedSection=at;
    generate(project,Scope::section,selectedTrack,at);
    changed();select(selectedTrack,at);
    message(tr("Section added and generated. REGENERATE rewrites it; Undo removes it."));
}
void MainComponent::freezeTrack(int track){
    if(track<0||track>=(int)project.tracks.size()||loadingPlugin||exportTask)return;
    if(audio.trackFrozen(track)){audio.unfreezeTrack(track);refreshControls();return;}
    audio.capturePluginStates(project);auto snapshot=project;auto copy=audio.offlineCopy(track);if(!copy){message(tr("Cannot create a private freeze render."),true);return;}
    auto& target=snapshot.tracks[(size_t)track];const auto identity=target.sessionId;target.eq.enabled=false;target.gain=1;target.pan=0;target.volume={};target.panAutomation={};target.group=-1;target.sends={};snapshot.masterGain=1;snapshot.masterAutomation={};snapshot.buses=defaultBuses();snapshot.masterEffects.effects.clear();snapshot.loop=false;
    class FreezeTask final:public RenderProgressTask{public:std::unique_ptr<AudioEngine> engine;Project project;juce::File file;int track;juce::Result result=juce::Result::ok();std::function<void(bool,juce::Result)> done;FreezeTask(std::unique_ptr<AudioEngine> e,Project p,int t,juce::File f,std::function<void(bool,juce::Result)> d):RenderProgressTask("Freeze Track"),engine(std::move(e)),project(std::move(p)),file(f),track(t),done(std::move(d)){}void run()override{result=engine->exportAudio(project,file,track,false,[this]{return threadShouldExit();},[this](double p){setProgress(p);});}void threadComplete(bool cancelled)override{if(failedToStart())result=juce::Result::fail("Cannot start freeze worker.");done(cancelled,result);}};
    const auto file=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fga-freeze",".wav",false);const double bpm=project.settings.bpm;juce::Component::SafePointer<MainComponent> safe(this);
    exportTask=std::make_unique<FreezeTask>(std::move(copy),std::move(snapshot),track,file,[safe,track,identity,bpm,file](bool cancelled,juce::Result result){if(safe&&!cancelled&&result.wasOk()&&track<(int)safe->project.tracks.size()&&safe->project.tracks[(size_t)track].sessionId==identity){safe->audio.freezeTrack(track,file,bpm,identity);safe->message(tr(safe->audio.trackFrozen(track)?"Track frozen. Unfreeze restores the live instrument.":"Freeze cache exceeds sampler memory limits."),!safe->audio.trackFrozen(track));}else if(safe&&!cancelled&&result.failed())safe->message(result.getErrorMessage(),true);file.deleteFile();if(safe){safe->retiredJobs.retire(std::move(safe->exportTask));safe->refreshControls();safe->updateEditingAvailability();}});exportTask->launchThread();updateEditingAvailability();
}
void MainComponent::showTrackMenu(int track){
    juce::PopupMenu menu;const auto& t=project.tracks[(size_t)track];menu.addSectionHeader(u(t.name));menu.addItem(10,tr(audio.trackFrozen(track)?"Unfreeze Track":"Freeze Track"),!loadingPlugin&&t.instrument=="plugin");menu.addItem(1,tr("Rinomina"));menu.addItem(2,tr("Muto"),true,t.mute);menu.addItem(3,tr("Solo"),true,t.solo);menu.addItem(4,tr("Blocca generazione"),true,t.locked);menu.addSeparator();menu.addItem(5,tr("Suoni originali Fase Groove"),!loadingPlugin);menu.addItem(6,tr("Rigenera traccia"),hasGenerator(t));menu.addSeparator();menu.addItem(7,tr("Add MIDI track"),project.tracks.size()<maxTracks);menu.addItem(9,tr("VOCAL CHOPS"),project.tracks.size()<maxTracks);if(isManualTrack(track))menu.addItem(8,tr("Delete MIDI track"),!loadingPlugin);
    if(t.instrument=="sample"){juce::PopupMenu roots;for(int n=24;n<=96;++n)roots.addItem(100+n,juce::MidiMessage::getMidiNoteName(n,true,true,3),true,n==t.sampleRoot);menu.addSubMenu(tr("Radice campione"),roots);}
    juce::Component::SafePointer<MainComponent> safe(this);menu.showMenuAsync(juce::PopupMenu::Options(),[safe,track](int id){if(safe==nullptr||id==0)return;if(id==10){safe->freezeTrack(track);return;}if(id==7){safe->addMidiTrack();return;}if(id==9){safe->addMidiTrack(true);return;}if(id==8){safe->deleteMidiTrack(track);return;}if(track>=(int)safe->project.tracks.size())return;safe->select(track,safe->selectedSection);if(id==1){rename("Rinomina traccia",u(safe->project.tracks[(size_t)track].name),[safe,track](juce::String name){if(safe!=nullptr&&name.trim().isNotEmpty()&&track<(int)safe->project.tracks.size()){safe->checkpoint();safe->project.tracks[(size_t)track].name=name.trim().substring(0,128).toStdString();safe->changed(false);}});return;}if(id==6){safe->regenerate(Scope::track);return;}safe->checkpoint();auto& t=safe->project.tracks[(size_t)track];if(id==2)t.mute=!t.mute;if(id==3)t.solo=!t.solo;if(id==4)t.locked=!t.locked;if(id==5){safe->closePluginEditor();safe->audio.useInternal(track);t.instrument="internal";t.instrumentPath.clear();t.instrumentId.clear();t.instrumentState.clear();}if(id>=124&&id<=196){int root=id-100;t.sampleRoot=root;}safe->changed(false);});
}
void MainComponent::showSectionMenu(int section){
    juce::PopupMenu menu;menu.addSectionHeader(u(project.sections[(size_t)section].name));menu.addItem(1,tr("Rinomina"));menu.addItem(2,tr("Duplicate section with notes"));menu.addItem(3,tr("Elimina"),project.sections.size()>1);menu.addItem(4,tr("Sposta a sinistra"),section>0);menu.addItem(5,tr("Sposta a destra"),section+1<(int)project.sections.size());menu.addItem(6,tr("Rigenera sezione"));menu.addItem(7,tr("Loop sezione"),true,project.loop&&project.loopMatches(project.sectionStart(section),project.sectionStart(section+1)));menu.addItem(8,tr("Clear loop"),project.loop||project.hasLoopRegion());
    menu.addItem(50,tr("Independent section harmony"),true,project.sections[(size_t)section].harmony.enabled);menu.addItem(51,tr("Rotate section chords"),project.sections[(size_t)section].harmony.enabled);menu.addItem(52,tr("Pre-drop silence"),true,project.sections[(size_t)section].preDropSilence);menu.addItem(53,tr("Accelerating snare roll"),true,project.sections[(size_t)section].snareRoll);menu.addItem(54,tr("Reverse crash"),project.tracks[16].instrument=="sample",project.sections[(size_t)section].reverseCrash);{juce::PopupMenu energy;const int current=project.sections[(size_t)section].energy;const char* names[]{"Auto","Low","Medium","High","Peak"};for(int i=0;i<5;++i)energy.addItem(60+i,tr(names[i]),true,current==i-1);menu.addSubMenu(tr("Energy"),energy);}
    juce::PopupMenu bars,types;for(int n:{1,2,4,8,12,16,24,32,48,64,96,128})bars.addItem(400+n,juce::String(n)+" bars",true,n==project.sections[(size_t)section].bars);for(int type=0;type<6;++type)types.addItem(200+type,sectionName((Section)type),true,(int)project.sections[(size_t)section].type==type);menu.addSubMenu(tr("Durata"),bars);menu.addSubMenu(tr("Funzione musicale"),types);
    juce::Component::SafePointer<MainComponent> safe(this);menu.showMenuAsync(juce::PopupMenu::Options(),[safe,section](int id){if(safe==nullptr||id==0)return;safe->select(safe->selectedTrack,section);if(id==1){rename("Rinomina sezione",u(safe->project.sections[(size_t)section].name),[safe,section](juce::String name){if(safe!=nullptr&&name.isNotEmpty()){safe->checkpoint();safe->project.sections[(size_t)section].name=name.toStdString();safe->changed();}});return;}safe->checkpoint();if(id>=60&&id<=64){safe->checkpoint();safe->project.sections[(size_t)section].energy=id-61;generate(safe->project,Scope::section,safe->selectedTrack,section,0);safe->changed();return;}if(id>=50&&id<=54){auto& sectionData=safe->project.sections[(size_t)section];auto& h=sectionData.harmony;const auto& s=safe->project.settings;if(id==50){h.enabled=!h.enabled;if(h.enabled){h.key=s.key;h.scale=s.scale;h.chordCount=s.chordCount;h.chordSteps=s.chordSteps;h.degrees=s.degrees;h.freeChords=s.freeChords;h.manualChords=s.manualChords;}}if(id==51){std::rotate(h.degrees.begin(),h.degrees.begin()+1,h.degrees.begin()+h.chordCount);std::rotate(h.freeChords.begin(),h.freeChords.begin()+1,h.freeChords.begin()+h.chordCount);std::rotate(h.manualChords.begin(),h.manualChords.begin()+1,h.manualChords.begin()+h.chordCount);}if(id==52)sectionData.preDropSilence=!sectionData.preDropSilence;if(id==53)sectionData.snareRoll=!sectionData.snareRoll;if(id==54)sectionData.reverseCrash=!sectionData.reverseCrash;generate(safe->project,Scope::section,safe->selectedTrack,section,0);safe->changed();return;}if(id==2){if(safe->project.duplicateSection(section))safe->selectedSection++;}if(id==3){safe->project.removeGuide(section);safe->selectedSection=std::min(section,(int)safe->project.sections.size()-1);}if(id==4){safe->project.moveGuide(section,section-1);safe->selectedSection--;}if(id==5){safe->project.moveGuide(section,section+1);safe->selectedSection++;}if(id==6){safe->regenerate(Scope::section);return;}if(id==7){safe->project.toggleLoopRegion(safe->project.sectionStart(section),safe->project.sectionStart(section+1));safe->changed(false);return;}if(id==8){safe->project.clearLoop();safe->changed(false);return;}if(id>=401&&id<=528){safe->project.resizeGuide(section,id-400);}else if(id>=200&&id<206){for(int track=0;track<(int)safe->project.tracks.size();++track)safe->project.detachTrack(track);safe->project.sections[(size_t)section].type=(Section)(id-200);safe->project.sections[(size_t)section].name=sectionName((Section)(id-200));}
        safe->project.loopEnd=std::min(safe->project.loopEnd,safe->project.length());safe->piano.clearSelection();safe->changed();
    });
}
void MainComponent::renameClip(int track,int index){
    if(track<0||track>=(int)project.tracks.size())return;auto& clips=project.tracks[(size_t)track].clips;if(index<0||index>=(int)clips.size()||project.sections.empty())return;
    const int guide=std::clamp(project.sectionAtBeat(project.clipStart(track,index)),0,(int)project.sections.size()-1);const auto fallback=u(project.sections[(size_t)guide].name);
    const auto initial=clips[(size_t)index].name.empty()?fallback:u(clips[(size_t)index].name);juce::Component::SafePointer<MainComponent> safe(this);
    rename("Rename clip",initial,[safe,track,index,fallback](juce::String name){if(safe==nullptr||track>=(int)safe->project.tracks.size())return;auto& live=safe->project.tracks[(size_t)track].clips;if(index<0||index>=(int)live.size())return;auto trimmed=name.trim().substring(0,128);std::string stored=trimmed.isEmpty()||trimmed==fallback?std::string():trimmed.toStdString();if(live[(size_t)index].name==stored)return;safe->checkpoint();live[(size_t)index].name=std::move(stored);safe->changed(false);});
}
void MainComponent::showClipMenu(int track,int index){
    const bool hasClip=index>=0&&index<(int)project.tracks[(size_t)track].clips.size();juce::PopupMenu menu;
    menu.addItem(11,tr("Cut clip"),hasClip);menu.addItem(12,tr("Copy clip"),hasClip);menu.addItem(13,tr("Paste clips at cursor"),!timelineClipboard.empty());menu.addItem(14,tr("Split at cursor"),hasClip);menu.addItem(15,tr("Duplicate clip"),hasClip);menu.addItem(16,tr("Delete clip"),hasClip);menu.addItem(18,tr("Create empty MIDI clip"));menu.addItem(19,tr("Rename clip"),hasClip);menu.addItem(4,tr("Humanize notes"),hasClip);menu.addSeparator();
    const auto& source=project.tracks[(size_t)track];
    const auto* candidate=hasClip?&source.clips[(size_t)index]:nullptr;
    const bool convertible=candidate&&candidate->audioOffset<0&&source.instrument=="sample"&&(source.sampler.mode==1||(source.sampler.mode<0&&isDrumTrack(track)))&&candidate->notes.size()==1&&candidate->notes[0].pitch==source.sampleRoot&&std::abs(candidate->notes[0].start)<1e-9&&audio.sampleAsset(track);
    menu.addItem(30,tr("Convert to Audio Region"),convertible);
    // Imported audio: adapt to the project's tempo and key from the analysis made at import.
    if(candidate&&candidate->audioOffset>=0&&source.instrument=="sample"){static const char* keys[]{"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
        juce::PopupMenu adapt;adapt.addSectionHeader(audioAnalysisSummary(source));
        adapt.addItem(40,tr("Follow project tempo")+(source.audioBpm>0?"  ("+juce::String(source.audioBpm,1)+" BPM)":juce::String()),source.audioBpm>0,source.sampler.sync&&source.audioBpm>0&&std::abs(source.sampler.sourceBpm-source.audioBpm)<1e-6);
        adapt.addItem(41,tr("Transpose to project key")+(source.audioKey>=0?"  ("+juce::String(keys[source.audioKey])+(source.audioMinor?"m":"")+")":juce::String()),source.audioKey>=0);
        adapt.addItem(42,tr("Original tempo and key"),source.sampler.sync||source.sampler.transpose!=0||source.sampler.fine!=0);adapt.addSeparator();
        // The other direction: the generated parts follow the imported audio.
        adapt.addItem(43,tr("Set project tempo from this audio"),source.audioBpm>0&&std::abs(source.audioBpm-project.settings.bpm)>.05);
        adapt.addItem(44,tr("Set project key from this audio"),source.audioKey>=0);
        menu.addSubMenu(tr("Audio: tempo and key"),adapt);menu.addItem(33,tr("Edit in Studio"));}
    menu.addItem(32,tr("Clear Extracted Groove"),project.settings.grooveEnabled);menu.addItem(31,tr("Reset Clip Feel to Global"),candidate&&std::any_of(candidate->feel.begin(),candidate->feel.end(),[](double v){return v>=0;}));
    menu.addItem(1,tr("Rigenera solo questo clip"),hasClip);menu.addItem(2,tr("Blocca generazione"),hasClip,hasClip&&project.tracks[(size_t)track].clips[(size_t)index].locked);menu.addItem(6,tr("LOOP CLIP"),hasClip,hasClip&&project.loop&&project.loopMatches(project.clipStart(track,index),project.clipStart(track,index)+project.clipLength(track,index)));menu.addItem(7,tr("Clear loop"),project.loop||project.hasLoopRegion());
    juce::Component::SafePointer<MainComponent> safe(this);menu.showMenuAsync(juce::PopupMenu::Options(),[safe,track,index](int id){if(!safe||!id)return;safe->selectClip(track,index,true);
        if(id==30){const auto asset=safe->audio.sampleAsset(track);if(!asset)return;safe->checkpoint();auto& t=safe->project.tracks[(size_t)track];auto& c=safe->clip();c.span=safe->clipLength();c.audioOffset=t.sampler.start*asset->seconds();c.audioDuration=std::min(c.span*60./safe->project.settings.bpm,std::max(0.,t.sampler.end*asset->seconds()-c.audioOffset));if(c.audioDuration<=0){safe->undo(false);return;}c.loopLength=c.loopPhase=0;c.loopSource.clear();c.notes[0].length=c.span;safe->changed();return;}
        if(id>=40&&id<=44){safe->adaptAudioTrack(track,id-40);return;}
        if(id==33){safe->showStudio(0);return;}
        if(id==32){safe->checkpoint();safe->project.settings.grooveEnabled=false;generate(safe->project);safe->changed();return;}
        if(id==31){safe->checkpoint();safe->clip().feel={-1,-1,-1};generate(safe->project,Scope::clip,track,safe->clipIndex(),0);safe->changed();return;}
        if(id==4){safe->checkpoint();humanize(safe->clip(),.65,(std::uint32_t)juce::Random::getSystemRandom().nextInt(),safe->clipLength());tidyNotes(safe->clip(),safe->clipLength());safe->changed();return;}if(id==19){safe->renameClip(track,index);return;}if(id>=11&&id<=18){safe->timelineCommand(id-10);return;}if(id==1)safe->regenerate(Scope::clip);if(id==2){safe->checkpoint();safe->clip().locked=!safe->clip().locked;safe->changed(false);}if(id==6)safe->toggleClipLoop();if(id==7)safe->clearLoop();});
}
void MainComponent::showGuide(bool english){juce::DialogWindow::LaunchOptions o;o.dialogTitle=english?"Fase Groove Arranger / User guide":"Fase Groove Arranger / Guida";o.dialogBackgroundColour=Palette::background;o.useNativeTitleBar=true;o.resizable=true;o.content.setOwned(createGuidePage(english).release());if(auto* window=o.launchAsync())window->setAlwaysOnTop(true);}
bool MainComponent::writeHelpEvidence(const juce::File& folder){
    bool ok=Updater::isNewer("v0.4.2","0.4.1")&&!Updater::isNewer("0.4.1","0.4.1")&&!Updater::isNewer("v0.3.9","0.4.1")&&Updater::isNewer("0.10.0","0.9.9")&&Updater::isNewer("0.11.0","0.10.0")&&Updater::isNewer("0.12.0","0.11.0")&&Updater::isNewer("0.12.1","0.12.0")&&Updater::isNewer("0.13.0","0.12.1");
    for(bool about:{true,false}){
        options.show(about,1.f);auto image=createComponentSnapshot(getLocalBounds());
        if(auto out=folder.getChildFile(about?"credits.png":"settings.png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(image,*out)&&ok;}else ok=false;
        ok=options.verifyLayout()&&ok;
    }
    options.setVisible(false);
    Updater::Result release;release.status=Updater::Result::Status::ready;release.version="0.4.2";
    release.notes="## Fase Groove Arranger\n\n- **"+tr("New version")+"**\n- "+tr("What's new:");
    updateOverlay.show(release);ok=updateOverlay.verifyLayout()&&ok;
    if(auto out=folder.getChildFile("update.png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(createComponentSnapshot(getLocalBounds()),*out)&&ok;}else ok=false;
    updateOverlay.setVisible(false);
    folder.getChildFile("family-options.txt").replaceWithText(ok?"PASS family SETTINGS/ABOUT tabs, settings, credits and link geometry":"FAIL family options");
    return verifyGuidePages(folder)&&ok;
}
void MainComponent::showDocument(const juce::String& title,const juce::String& text){juce::DialogWindow::LaunchOptions o;o.dialogTitle=title;o.dialogBackgroundColour=Palette::panel;o.useNativeTitleBar=true;o.resizable=true;o.content.setOwned(createReferencePage(title,text).release());if(auto* window=o.launchAsync())window->setAlwaysOnTop(true);}
void MainComponent::showAbout(){
    const float scale=juce::Desktop::getInstance().getGlobalScaleFactor();
    options.checkUpdatesButton.setEnabled(!updater.isBusy());options.checkUpdatesButton.setButtonText(tr(updater.isBusy()?"CHECKING...":"CHECK NOW"));
    options.show(true,scale);
}
namespace {
// Audio device dialog. On Windows it can enable ASIO, so the Steinberg ASIO Usage
// Guidelines (SDK 2.3.4, section 1f) require the unmodified ASIO Compatible Logo and the
// trademark attribution in this dialog.
class AudioSettingsPanel final:public juce::Component{
public:
    explicit AudioSettingsPanel(juce::AudioDeviceManager& devices):selector(devices,0,2,0,2,false,false,true,false){
        addAndMakeVisible(selector);
#if JUCE_ASIO
        logo=juce::ImageCache::getFromMemory(BinaryData::asiocompatiblelogo_png,BinaryData::asiocompatiblelogo_pngSize);
#endif
    }
    void paint(juce::Graphics& g)override{
        if(!logo.isValid())return;
        auto strip=getLocalBounds().removeFromBottom(footer).reduced(16,10);
        g.setColour(juce::Colours::white.withAlpha(.08f));g.drawHorizontalLine(getHeight()-footer,16.f,(float)getWidth()-16.f);
        const float h=(float)strip.getHeight(),w=h*(float)logo.getWidth()/(float)logo.getHeight();
        g.drawImage(logo,juce::Rectangle<float>((float)strip.getX(),(float)strip.getY(),w,h),juce::RectanglePlacement::centred);
        g.setColour(juce::Colours::white.withAlpha(.7f));g.setFont(FamilyTheme::font(12.f));
        g.drawFittedText("ASIO is a registered trademark of Steinberg Media Technologies GmbH.",strip.withTrimmedLeft((int)w+14),juce::Justification::centredLeft,2);
    }
    void resized()override{selector.setBounds(getLocalBounds().withTrimmedBottom(logo.isValid()?footer:0));}
private:
    juce::AudioDeviceSelectorComponent selector;juce::Image logo;static constexpr int footer=64;
};
}
void MainComponent::showAudioSettings(){
    juce::PopupMenu menu;menu.addSectionHeader(tr("AUDIO / MIDI"));menu.addItem(1,tr("Dispositivo audio / buffer"));menu.addItem(3,tr("Apri cartella autosave"));menu.addItem(4,tr("Recupera autosave"));menu.addItem(6,tr("Collect portable project"));menu.addSeparator();menu.addItem(5,tr("Metronome (C)"),true,audio.metronomeOn());juce::Component::SafePointer<MainComponent> safe(this);menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&settingsButton).withMinimumWidth(240).withStandardItemHeight(30),[safe](int id){if(safe==nullptr)return;if(id==5){safe->toggleMetronome();return;}if(id==1){juce::DialogWindow::LaunchOptions o;o.dialogTitle=juce::String::fromUTF8("Fase Groove · Audio");o.dialogBackgroundColour=Palette::panel;o.useNativeTitleBar=true;o.resizable=false;auto* selector=new AudioSettingsPanel(safe->audio.devices);selector->setSize(920,704);o.content.setOwned(selector);o.componentToCentreAround=safe.getComponent();o.launchAsync();}
        if(id==6){safe->collectPortable();return;}
        if(id==3||id==4){auto folder=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("FS Pitch/Fase Groove Arranger");folder.createDirectory();if(id==3)folder.revealToUser();else{RecoveryOffer offer;auto snap=folder.getChildFile("autosave.fga");if(pendingRecovery(recoveryDirectory(),offer)){snap=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fga-recovery",".fga",false);if(!offer.snapshot.copyFileTo(snap)){safe->message(tr("The recovery file is damaged and was not applied. The original project was not changed."),true);return;}}safe->open(snap,true);}}
    });
}
void MainComponent::exportMenu(){
    if(loadingPlugin){message("Attendi il caricamento dei plugin.");return;}
    const auto settings=exportSettings();juce::PopupMenu m,processing,ceilings;
    processing.addItem(21,"Transparent",true,settings.mode==AudioExportMode::transparent);
    if(offlineLoudReady)processing.addItem(22,"Loud",true,settings.mode==AudioExportMode::loud);
    if(settings.mode==AudioExportMode::legacy||Localisation::setting("exportLoudnessGuard").isNotEmpty()){
        processing.addItem(23,"Legacy",true,settings.mode==AudioExportMode::legacy);
        processing.addItem(24,"Legacy guard",settings.mode==AudioExportMode::legacy,settings.loudnessGuard);
    }
    const int values[]{-1,-2,-3,-6,-12};
    for(int i=0;i<5;++i)ceilings.addItem(30+i,juce::String(values[i])+" dBTP",true,std::abs(settings.ceilingDb-values[i])<.01);
    m.addSubMenu("True Peak: "+juce::String(settings.mode==AudioExportMode::transparent?"Transparent":settings.mode==AudioExportMode::loud?"Loud":"Legacy"),processing);
    m.addSubMenu("Ceiling: "+juce::String(settings.ceilingDb,1)+" dBTP",ceilings,settings.mode!=AudioExportMode::legacy||settings.loudnessGuard);
    m.addItem(20,"STUDIO - SAMPLER / MIDI / MEDIA");m.addSeparator();
    m.addSectionHeader(juce::String::fromUTF8("MIDI · note modificabili"));m.addItem(1,tr("Canzone multitraccia (.mid)"));m.addItem(2,tr("Traccia selezionata (.mid)"));m.addItem(3,tr("Tutte le tracce MIDI separate"));
    m.addSectionHeader(juce::String::fromUTF8("WAV · ")+tr("Sample rate / bit depth"));m.addItem(4,tr("Mix completo WAV"));m.addItem(5,tr("Traccia selezionata WAV"));m.addItem(6,tr("Tutti gli stem WAV"));
    m.addSectionHeader(juce::String::fromUTF8("MP3 · 320 kbps / 44.1 kHz / stereo"));m.addItem(7,tr("Mix completo MP3"));m.addItem(8,tr("Traccia selezionata MP3"));m.addItem(9,tr("Tutti gli stem MP3"));
    juce::Component::SafePointer<MainComponent> safe(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&exportButton),[safe](int id){
        if(!safe)return;
        if(id==21||id==23||(id==22&&offlineLoudReady))Localisation::setSetting("exportPeakMode",id==21?"transparent":id==22?"loud":"legacy");
        else if(id==24)Localisation::setSetting("exportLoudnessGuard",Localisation::setting("exportLoudnessGuard","0")=="1"?"0":"1");
        else if(id>=30&&id<35){const int values[]{-1,-2,-3,-6,-12};Localisation::setSetting("exportCeilingDb",juce::String(values[id-30]));}
        else if(id==20)safe->showStudio();else if(id>0)safe->exportFile(id,safe->selectedTrack);
    });
}
void MainComponent::exportFile(int type,int track,const juce::File& destination,bool configured){
    if(type<1||type>9||loadingPlugin||editingRestricted())return;
    const bool batch=type==3||type==6||type==9;const bool mp3=type>=7;const bool midi=type<=3;const bool single=type==2||type==5||type==8;
    if(single&&!juce::isPositiveAndBelow(track,(int)project.tracks.size()))return;
    if(!midi&&!mp3&&!configured&&destination==juce::File()){
        auto* dialog=new juce::AlertWindow(tr("WAV export"),tr("Choose the encoding for all exported WAV files."),juce::MessageBoxIconType::NoIcon);
        dialog->setLookAndFeel(&theme);dialog->addComboBox("rate",{"44.1 kHz","48 kHz","88.2 kHz","96 kHz","192 kHz"},tr("Sample rate"));dialog->addComboBox("depth",{"16 bit PCM","24 bit PCM"},tr("Bit depth"));
        const int rates[]{44100,48000,88200,96000,192000};const int savedRate=Localisation::setting("exportSampleRate","48000").getIntValue();int selected=1;for(int i=0;i<5;++i)if(rates[i]==savedRate)selected=i;
        dialog->getComboBoxComponent("rate")->setSelectedItemIndex(selected);dialog->getComboBoxComponent("depth")->setSelectedItemIndex(Localisation::setting("exportBitDepth","24")=="16"?0:1);
        dialog->addButton(tr("EXPORT"),1,juce::KeyPress(juce::KeyPress::returnKey));dialog->addButton(tr("Cancel"),0,juce::KeyPress(juce::KeyPress::escapeKey));dialog->setAlwaysOnTop(true);
        juce::Component::SafePointer<MainComponent> safe(this);
        dialog->enterModalState(true,juce::ModalCallbackFunction::create([safe,dialog,type,track](int result){
            if(safe&&result==1){
                const int values[]{44100,48000,88200,96000,192000};
                Localisation::setSetting("exportSampleRate",juce::String(values[std::clamp(dialog->getComboBoxComponent("rate")->getSelectedItemIndex(),0,4)]));
                Localisation::setSetting("exportBitDepth",dialog->getComboBoxComponent("depth")->getSelectedItemIndex()==0?"16":"24");
                // The native chooser must outlive the encoding window's modal teardown.
                dialog->setVisible(false);dialog->setAlwaysOnTop(false);
                juce::MessageManager::callAsync([safe,type,track]{if(safe)safe->exportFile(type,track,{},true);});
            }
            dialog->setLookAndFeel(nullptr);
        }),true);return;
    }
    const auto encoding=exportSettings();
    const int flags=batch?juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories:juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting;
    auto render=[this,track,batch,mp3,midi,single,encoding](juce::File target){
        if(loadingPlugin||editingRestricted()||(single&&!juce::isPositiveAndBelow(track,(int)project.tracks.size())))return;
        const auto chosenFolder=batch?target:target.getParentDirectory();if(chosenFolder.isDirectory())Localisation::setSetting("lastExportFolder",chosenFolder.getFullPathName());
        const auto named=batch?target:exportNamedFile(target,project,midi?"mid":mp3?"mp3":"wav");
        if(named!=target&&named.existsAsFile()){
            juce::Component::SafePointer<MainComponent> safe(this);const int retry=batch?(midi?3:mp3?9:6):single?(midi?2:mp3?8:5):midi?1:mp3?7:4;
            juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon,tr("EXPORT"),named.getFileName()+"\n"+tr("Replace the existing file?"),tr("Replace"),tr("Cancel"),getTopLevelComponent(),juce::ModalCallbackFunction::create([safe,retry,track,named](int result){if(safe&&result)safe->exportFile(retry,track,named,true);}));return;
        }
        if(batch){target=target.getChildFile(juce::File::createLegalFileName(u(project.name))+exportSuffix(project)+"-stems-"+juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S"));target=target.getNonexistentSibling();if(target.createDirectory().failed()){message(tr("Cannot create the stem folder."),true);return;}}
        else target=exportNamedFile(target,project,midi?"mid":mp3?"mp3":"wav");
        if(midi){auto write=[this](const juce::File& file,int ti){auto bytes=midiFile(project,ti);juce::TemporaryFile temp(file);return temp.getFile().replaceWithData(bytes.data(),bytes.size())&&temp.overwriteTargetFileWithTemporary();};bool ok=true;if(batch){ok=writeExportArrangement(target,project);for(int row=0;row<(int)project.tracks.size();++row){const int ti=trackAtRow(row);ok=write(exportStemFile(target,project,ti,"mid"),ti)&&ok;}}else ok=write(target,single?track:-1);message(ok?"MIDI esportato: "+target.getFullPathName():"Esportazione MIDI non completata.",!ok);return;}
        instrumentPanel.flushRender();audio.stop();audio.stopPreview();audio.stopSampleAudition();mediaBusy=true;updateEditingAvailability();
        // Stop the live callback and render the existing plugin instances. This avoids
        // reloading libraries/licences and frees the device callback during the export.
        audio.devices.removeAudioCallback(&audio);
        std::vector<int> tracks;if(batch)for(int row=0;row<(int)project.tracks.size();++row)tracks.push_back(trackAtRow(row));else tracks.push_back(single?track:-1);
        juce::Component::SafePointer<MainComponent> safe(this);exportTask=std::make_unique<ExportTask>(audio,project,target,tracks,mp3,batch,encoding,[safe,target](juce::Result r){if(safe){safe->audio.devices.addAudioCallback(&safe->audio);safe->mediaBusy=false;safe->retiredJobs.retire(std::move(safe->exportTask));safe->updateEditingAvailability();safe->message(r.wasOk()?"Audio esportato: "+target.getFullPathName():r.getErrorMessage(),r.failed());}});
#if FGA_SELF_TESTS
        static_cast<ExportTask*>(exportTask.get())->progressForTest=exportProgressForTest;
#endif
        if(auto* alert=exportTask->getAlertWindow())alert->setAlwaysOnTop(true);exportTask->launchThread();
    };
    if(destination!=juce::File()){render(destination);return;}
    auto folder=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);const auto remembered=Localisation::setting("lastExportFolder");if(juce::File::isAbsolutePath(remembered)&&juce::File(remembered).isDirectory())folder=juce::File(remembered);
    const auto extension=midi?"mid":mp3?"mp3":"wav";const auto suggested=batch?folder:folder.getChildFile(juce::File::createLegalFileName(single?u(project.tracks[(size_t)track].name):u(project.name))+exportSuffix(project)+"."+extension);
    chooseFile(batch?"Cartella esportazione (crea una sottocartella dedicata)":"Esporta "+juce::String(midi?"MIDI":mp3?"MP3":"WAV"),batch?"*":"*."+juce::String(extension),flags,std::move(render),{},suggested);
}
bool MainComponent::verifyExports(const juce::File& folder,const juce::PluginDescription* effect){
#if !FGA_SELF_TESTS
    juce::ignoreUnused(folder,effect);return false;
#else
    if(!folder.createDirectory())return false;
    const auto saved=project;const auto savedFile=projectFile;const bool wasDirty=dirty;const auto oldRate=Localisation::setting("exportSampleRate","48000"),oldBits=Localisation::setting("exportBitDepth","24"),oldFolder=Localisation::setting("lastExportFolder"),oldSave=Localisation::setting("lastSaveFolder");
    auto* original=audio.plugin(0);juce::String report,cancelDetails;bool ok=true;auto check=[&](bool pass,const char* label){ok=pass&&ok;report+=juce::String(pass?"PASS ":"FAIL ")+label+"\n";};
    // Press the real Cancel button while cleanup needs the message thread.
    // A finite worker-side wait makes the old UI-join failure reproducible
    // without forcibly killing a test thread or hanging the test process.
    struct CancelProbeState {std::atomic<bool> started{false},serviced{false};juce::WaitableEvent message;};
    struct CancelProbe final:ExportTask {
        std::shared_ptr<CancelProbeState> state;
        CancelProbe(AudioEngine& e,Project p,std::shared_ptr<CancelProbeState> s,std::function<void(juce::Result)> done):ExportTask(e,std::move(p),{}, {-1},false,false,{},std::move(done)),state(std::move(s)){}
        void run()override{state->started=true;while(!threadShouldExit())wait(1);auto keep=state;juce::MessageManager::callAsync([keep]{keep->message.signal();});state->serviced=state->message.wait(800);}
    };
    // Finite reproduction of the previous JUCE wrapper on the native runner.
    // The worker times out safely, so this self-test never kills a thread.
    struct LegacyCancelProbe final:juce::ThreadWithProgressWindow {
        std::shared_ptr<CancelProbeState> state;bool& done;
        LegacyCancelProbe(std::shared_ptr<CancelProbeState> s,bool& d):ThreadWithProgressWindow("Legacy Cancel regression probe",true,true,15000),state(std::move(s)),done(d){}
        void run()override{state->started=true;while(!threadShouldExit())wait(1);auto keep=state;juce::MessageManager::callAsync([keep]{keep->message.signal();});state->serviced=state->message.wait(800);}
        void threadComplete(bool)override{done=true;}
    };
    auto legacyState=std::make_shared<CancelProbeState>();bool legacyDone=false;
    {
        LegacyCancelProbe legacy(legacyState,legacyDone);legacy.launchThread();
        const auto until=juce::Time::getMillisecondCounterHiRes()+3000;
        while(!legacyState->started&&juce::Time::getMillisecondCounterHiRes()<until)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
        legacy.getAlertWindow()->getButton(0)->triggerClick();
        while(!legacyDone&&juce::Time::getMillisecondCounterHiRes()<until)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
        check(legacyDone&&!legacyState->serviced,"legacy Cancel reproduces blocked UI cleanup handshake (finite test probe)");
    }
    auto probeState=std::make_shared<CancelProbeState>();bool probeDone=false;
    CancelProbe probe(audio,project,probeState,[&](juce::Result){probeDone=true;});probe.launchThread();
    auto probeDeadline=juce::Time::getMillisecondCounterHiRes()+3000;
    while(!probeState->started&&juce::Time::getMillisecondCounterHiRes()<probeDeadline)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
    const auto cancelProbeStart=juce::Time::getMillisecondCounterHiRes();probe.getAlertWindow()->getButton(0)->triggerClick();
    while(!probeDone&&juce::Time::getMillisecondCounterHiRes()<probeDeadline)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
    check(probeDone&&probeState->serviced,"real Cancel keeps message dispatch alive for worker cleanup (no synchronous UI join)");
    cancelDetails+="INFO cancel-to-worker-completion "+juce::String(juce::Time::getMillisecondCounterHiRes()-cancelProbeStart,2)+" ms (local UI probe, not a hardware latency claim)\n";

    const auto oldMode=Localisation::setting("exportPeakMode"),oldCeiling=Localisation::setting("exportCeilingDb"),oldGuard=Localisation::setting("exportLoudnessGuard");
    Localisation::setSetting("exportPeakMode",{});Localisation::setSetting("exportLoudnessGuard",{});
    check(exportSettings().mode==AudioExportMode::transparent,"new export settings default to Transparent");
    Localisation::setSetting("exportLoudnessGuard","1");check(exportSettings().mode==AudioExportMode::legacy&&exportSettings().loudnessGuard,"historic guard preference is preserved");
    Localisation::setSetting("exportPeakMode","transparent");Localisation::setSetting("exportCeilingDb","-3");
    check(exportSettings().mode==AudioExportMode::transparent&&exportSettings().ceilingDb==-3,"Transparent selection and ceiling are retained");
    Localisation::setSetting("exportCeilingDb","-1");
    project=newProject(7);project.settings.bpm=120;project.settings.key=1;project.settings.scale=0;project.sections={{Section::intro,1,"Intro"},{Section::main,2,"Jackin groove"}};project.normalise();project.masterGain=.5;project.masterAutomation.enabled=false;
    for(auto& track:project.tracks){track.mute=true;track.volume.enabled=false;track.panAutomation.enabled=false;track.parameters.clear();for(auto& clip:track.clips)clip.notes.clear();}auto& pianoTrack=project.tracks[0];pianoTrack.mute=false;pianoTrack.gain=.5;pianoTrack.pan=0;pianoTrack.instrument=original?"plugin":"internal";pianoTrack.clips[0].notes={{0,2,69,100,0}};audio.publish(project);
    // Reuse the same button/cleanup matrix with an actual native VST3 effect.
    // Only the effects fixture supplies this description; no production path changes.
    juce::AudioPluginInstance* originalEffect=nullptr;
    if(effect){
        // A load still in flight (the instrument fixture on a slow runner) makes
        // loadEffectDescription return without loading: wait for it first.
        const auto idle=juce::Time::getMillisecondCounterHiRes()+15000;
        while((pluginsBusy()||editingRestricted())&&juce::Time::getMillisecondCounterHiRes()<idle)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
        loadEffectDescription(0,*effect,{},false,1);
        const auto until=juce::Time::getMillisecondCounterHiRes()+15000;
        while(pluginsBusy()&&juce::Time::getMillisecondCounterHiRes()<until)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
        originalEffect=audio.effectPlugin(0,1);closePluginEditor();
        check(originalEffect&&!pluginsBusy(),"cancellation fixture loads a real VST3 effect");
    }
    // Debug synthesis of all 18 stems is deliberately slower; keep the same completion/audio assertions.
   #if JUCE_DEBUG
    constexpr double exportTestTimeout=600000.;
   #else
    constexpr double exportTestTimeout=30000.;
   #endif
    auto settle=[&]{const auto deadline=juce::Time::getMillisecondCounterHiRes()+exportTestTimeout;while(mediaBusy&&juce::Time::getMillisecondCounterHiRes()<deadline)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);return !mediaBusy&&!audio.isExporting()&&!audio.isPlaying()&&!editingRestricted();};
    exportFile(5,0);auto* encodingDialog=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());const bool hasEncoding=encodingDialog&&encodingDialog->getComboBoxComponent("rate")&&encodingDialog->getComboBoxComponent("depth");check(hasEncoding&&encodingDialog->getComboBoxComponent("rate")->getNumItems()==5&&encodingDialog->getComboBoxComponent("depth")->getNumItems()==2,"WAV export presents working sample-rate and bit-depth choices");
    if(encodingDialog){if(auto out=folder.getChildFile("wav-options.png").createOutputStream()){juce::PNGImageFormat png;png.writeImageToStream(encodingDialog->createComponentSnapshot(encodingDialog->getLocalBounds()),*out);}encodingDialog->exitModalState(0);juce::MessageManager::getInstance()->runDispatchLoopUntil(20);}check(!mediaBusy&&!editingRestricted(),"cancelling encoding choices leaves the DAW available");
    const auto chosen=folder.getChildFile("Piano.wav");bool pathRequested=false,encodingDestroyed=false,validChoice=false,exportSuspended=false;juce::Component::SafePointer<juce::AlertWindow> finishedDialog;
    fileChoiceForTest=[&](const juce::File& initial,int mode,std::function<void(juce::File)> accepted,std::function<void()>){pathRequested=true;encodingDestroyed=finishedDialog==nullptr;validChoice=initial.hasFileExtension("wav")&&(mode&juce::FileBrowserComponent::saveMode)!=0;accepted(chosen);exportSuspended=mediaBusy&&!audio.isPlaying()&&editingRestricted();};
    audio.play(true);exportFile(5,0);auto* acceptedDialog=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());finishedDialog=acceptedDialog;
    if(acceptedDialog){acceptedDialog->getComboBoxComponent("rate")->setSelectedItemIndex(1);acceptedDialog->getComboBoxComponent("depth")->setSelectedItemIndex(0);acceptedDialog->triggerButtonClick(tr("EXPORT"));}
    const auto choiceDeadline=juce::Time::getMillisecondCounterHiRes()+5000.;while(!pathRequested&&juce::Time::getMillisecondCounterHiRes()<choiceDeadline)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
    fileChoiceForTest={};check(pathRequested&&encodingDestroyed&&validChoice,"confirming WAV encoding opens the file chooser after destroying the previous modal window");
    // Observe suspension in the acceptance callback: a short export can finish
    // while the outer message-loop wait is still dispatching other UI events.
    check(exportSuspended,"confirming export suspends live playback and editing");check(settle(),"successful export restores audio device availability and editing");
    juce::WavAudioFormat wav;{const auto file=exportNamedFile(chosen,project,"wav");std::unique_ptr<juce::AudioFormatReader> reader(readWavFile(file).release());juce::AudioBuffer<float> samples(2,8192);const bool audible=reader&&reader->read(&samples,0,8192,4096,true,true)&&samples.getMagnitude(0,8192)>.0001f;check(audible&&reader->sampleRate==48000&&reader->bitsPerSample==16,"WAV export through the actual UI contains music at 48 kHz / 16 bit");}
    check(audio.plugin(0)==original,"export retains the already loaded instrument instance");
    exportFile(1,0,folder.getChildFile("Song.mid"));const auto midi=exportNamedFile(folder.getChildFile("Song.mid"),project,"mid");auto stream=midi.createInputStream();juce::MidiFile parsed;check(stream&&parsed.readFrom(*stream),"multitrack MIDI filename includes BPM and key and contains readable MIDI");
    Localisation::setSetting("exportSampleRate","44100");Localisation::setSetting("exportBitDepth","24");exportFile(6,0,folder);check(settle(),"stem export completes and releases restrictions");const auto stems=folder.findChildFiles(juce::File::findDirectories,false,"*-stems-*");bool stemOk=stems.size()==1;if(stemOk){const auto files=stems[0].findChildFiles(juce::File::findFiles,false,"*.wav");stemOk=files.size()==(int)project.tracks.size();for(const auto& file:files){std::unique_ptr<juce::AudioFormatReader> reader(readWavFile(file).release());stemOk=reader&&reader->sampleRate==44100&&reader->bitsPerSample==24&&file.getFileName().contains("_120bpm_Db")&&stemOk;}const auto map=stems[0].getChildFile("Arrangiamento_120bpm_Db.txt").loadFileAsString();stemOk=stemOk&&map.contains("00:00.000 - 00:02.000  Intro")&&map.contains("00:02.000 - 00:06.000  Jackin groove");}check(stemOk,"all stems have the selected encoding, musical names and an exact section timeline TXT");
    exportFile(5,0,folder.getChildFile("cancel.wav"));if(exportTask)exportTask->signalThreadShouldExit();check(settle(),"cancelling export restores audio and editing");
    // Gate actual export progress so cancellation cannot accidentally arrive
    // after a short job has already completed. Only this self-test owns the gate.
    struct CancelGate {std::atomic<bool> reached{false},expired{false},pulse{false},closeAccepted{false};juce::WaitableEvent release;};
    auto filesIn=[&]{juce::StringArray names;for(const auto& f:folder.findChildFiles(juce::File::findFiles,false))names.add(f.getFileName());names.sort(false);return names;};
    auto cancelAt=[&](const char* name,double threshold,int type,bool repeated){
        const auto gate=std::make_shared<CancelGate>();
        exportProgressForTest=[gate,threshold](double value){if(value>=threshold&&!gate->reached.exchange(true)){if(!gate->release.wait(2000))gate->expired=true;}};
        const auto previous=exportNamedFile(folder.getChildFile(juce::File::createLegalFileName(juce::String(name))+".wav"),project,"wav");
        const juce::String marker="Existing file survives cancellation: "+juce::String(name);if(!previous.replaceWithText(marker)){check(false,"cannot create existing-file cancellation fixture");exportProgressForTest={};return;}
        const auto before=filesIn();const auto latency=audio.latencySamples();
        exportFile(type,0,type==6?folder:previous);
        const auto until=juce::Time::getMillisecondCounterHiRes()+10000;
        while(mediaBusy&&!gate->reached&&juce::Time::getMillisecondCounterHiRes()<until)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
        const bool active=mediaBusy&&exportTask&&gate->reached;
        if(active){
            auto* button=exportTask->getAlertWindow()->getButton(0);button->triggerClick();if(repeated){button->triggerClick();button->triggerClick();requestClose([gate]{gate->closeAccepted=true;});}
            // Delayed UI work must still run while worker cleanup is pending.
            juce::Timer::callAfterDelay(80,[gate]{gate->pulse=true;gate->release.signal();});
        }else gate->release.signal();
        const bool complete=settle();exportProgressForTest={};
        check(active&&complete&&gate->pulse&&!gate->expired&&!gate->closeAccepted&&!closePending,name);
        check(previous.loadFileAsString()==marker&&filesIn()==before,"cancel preserves existing bytes and leaves no root staging files");
        check(audio.latencySamples()==latency&&audio.plugin(0)==original,"cancel restores PDC and retains original plugin instance");
        if(effect)check(originalEffect&&audio.effectPlugin(0,1)==originalEffect,"cancel retains the real VST3 effect instance");
        if(type==6){bool clean=true;for(const auto& dir:folder.findChildFiles(juce::File::findDirectories,false,"*-stems-*")){juce::StringArray allowed;allowed.add("Arrangiamento"+exportSuffix(project)+".txt");for(int ti=0;ti<(int)project.tracks.size();++ti)allowed.add(exportStemFile(dir,project,ti,"wav").getFileName());for(const auto& f:dir.findChildFiles(juce::File::findFiles,true))clean=clean&&allowed.contains(f.getFileName());}check(clean,"cancelled stems retain only completed WAV/map files, not temporary files");}
    };
    Localisation::setSetting("exportSampleRate","48000");
    cancelAt("Cancel immediately after launch",0,5,false);
    cancelAt("Cancel during float rendering and writing",.20,5,false);
    cancelAt("Cancel during offline scaling and PCM writing",.85,5,false);
    cancelAt("Cancel near final verification before atomic publication",.999,5,false);
    cancelAt("Repeated Cancel and close keep UI responsive and engine alive",.15,5,true);
    cancelAt("Cancel stem worker without joining futures on the UI",.01,6,false);
    // Reuse the real native return host; do not replace a loaded VST3 source.
    const auto oldBuses=project.buses;const auto oldSends=project.tracks[0].sends;const int oldGroup=project.tracks[0].group;
    project.returnChains=true;project.tracks[0].group=0;project.tracks[0].sends={1,1};project.buses[0].sidechainAmount=.3;
    auto settlePlugin=[&]{const auto until=juce::Time::getMillisecondCounterHiRes()+10000;while(pluginsBusy()&&juce::Time::getMillisecondCounterHiRes()<until)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);return !pluginsBusy();};
    addNativeReturn(returnEffectTrack,false);const bool loadedA=settlePlugin()&&audio.effectPlugin(returnEffectTrack,1);closePluginEditor();addNativeReturn(returnEffectTrack+1,true);const bool loadedB=settlePlugin()&&audio.effectPlugin(returnEffectTrack+1,1);closePluginEditor();
    check(loadedA&&loadedB,"cancellation fixture loads actual editable return effects");
    if(loadedA&&loadedB)cancelAt("Cancel with group sidechain and shared A/B return effects",.2,4,false);
    if(loadedA&&loadedB){
        auto captured=std::make_shared<AssistantAudio>();captured->snapshot=project;captured->tracks={0};bool done=false,valid=false;
        auto* active=renderAssistantProject(captured,[](AssistantAudio& data,const auto&,const auto&){juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> mix(readWavFile(data.mix()).release()),stem(format.createReaderFor(data.stem(0).createInputStream().release(),true));return mix&&stem&&mix->usesFloatingPointData&&stem->lengthInSamples==mix->lengthInSamples?juce::Result::ok():juce::Result::fail("Invalid captured project audio");},[&](juce::Result result){done=true;valid=result.wasOk();},{});
        check(active&&settle()&&done&&valid&&audio.plugin(0)==original&&(!effect||audio.effectPlugin(0,1)==originalEffect),"project float analysis retains real hosted processors, group/returns and restores editing");
        auto cancelled=std::make_shared<AssistantAudio>();cancelled->snapshot=project;cancelled->tracks={0};done=false;valid=true;
        active=renderAssistantProject(cancelled,[](AssistantAudio&,const auto&,const auto&){return juce::Result::ok();},[&](juce::Result result){done=true;valid=result.wasOk();},{});
        if(active)active->getAlertWindow()->getButton(0)->onClick();
        check(active&&settle()&&done&&!valid&&audio.plugin(0)==original&&cancelled->directory.findChildFiles(juce::File::findFilesAndDirectories,false).isEmpty(),"project analysis Cancel returns worker ownership and removes partial audio with hosted processors");
    }

    audio.clearEffect(returnEffectTrack);audio.clearEffect(returnEffectTrack+1);project.returnChains=false;for(auto& rack:project.returnEffects)rack.effects.clear();project.buses=oldBuses;project.tracks[0].sends=oldSends;project.tracks[0].group=oldGroup;audio.publish(project);
    const auto retry=exportNamedFile(folder.getChildFile("after-repeated-cancel.wav"),project,"wav");exportFile(5,0,retry);check(settle(),"new export succeeds after all repeated/button cancellations");
    {std::unique_ptr<juce::AudioFormatReader> reader(readWavFile(retry).release());check(reader&&reader->sampleRate==48000&&reader->bitsPerSample==24,"retry produces complete WAV at requested rate/depth");}
    if(effect)cancelDetails+=originalEffect?"PASS cancellation matrix exercised a real loaded VST3 effect\n":"FAIL VST3 effect cancellation fixture missing\n";
    cancelDetails+=original?"PASS cancellation matrix exercised a real loaded VST3 instrument\n":"NOT RUN real VST3 cancellation in this invocation; run native host fixture separately\n";folder.getChildFile("export-cancel-details.txt").replaceWithText(cancelDetails);
    exportFile(5,0,folder.getChildFile("missing-parent/fail.wav"));check(settle()&&!exportNamedFile(folder.getChildFile("missing-parent/fail.wav"),project,"wav").existsAsFile(),"write failure restores audio and editing without a partial WAV");
    check(writeProjectFile(folder.getChildFile("Saved.fga"))&&Localisation::setting("lastSaveFolder")==folder.getFullPathName(),"successful Save remembers its directory");check(Localisation::setting("lastExportFolder")==folder.getFullPathName(),"export remembers its directory and retains it after an invalid destination");
    Localisation::setSetting("exportPeakMode",oldMode);Localisation::setSetting("exportCeilingDb",oldCeiling);Localisation::setSetting("exportLoudnessGuard",oldGuard);
    if(effect){closePluginEditor();audio.clearEffect(0,1);}
    project=saved;projectFile=savedFile;dirty=wasDirty;audio.publish(project);Localisation::setSetting("exportSampleRate",oldRate);Localisation::setSetting("exportBitDepth",oldBits);Localisation::setSetting("lastExportFolder",oldFolder);Localisation::setSetting("lastSaveFolder",oldSave);refreshControls();folder.getChildFile("export-ui.txt").replaceWithText(report);return ok;
#endif
}
}

namespace fga {
// 0: follow the project tempo (pitch-preserving stretch from the detected BPM); 1: transpose to
// the project key (also through the stretcher, so the tempo is unchanged); 2: original.
// The region keeps its source seconds; its length in beats follows how it now plays.
bool MainComponent::syncAudioRegions(int track,const SamplerSettings& before){
    if(!juce::isPositiveAndBelow(track,(int)project.tracks.size()))return false;auto& t=project.tracks[(size_t)track];const auto asset=audio.sampleAsset(track);
    if(t.instrument!="sample"||!asset||asset->seconds()<=0)return false;const auto& s=t.sampler;const double total=asset->seconds();
    const bool trimmed=s.start!=before.start||s.end!=before.end,timing=s.sync!=before.sync||s.sourceBpm!=before.sourceBpm||s.transpose!=before.transpose||s.fine!=before.fine;if(!trimmed&&!timing)return false;
    bool any=false;
    for(auto& c:t.clips){if(c.audioOffset<0||c.audioDuration<=0)continue;any=true;
        if(trimmed){// The region keeps its place on the timeline; its source moves with the trim.
            double from=c.audioOffset+(s.start-before.start)*total,to=c.audioOffset+c.audioDuration+(s.end-before.end)*total;
            from=std::clamp(from,s.start*total,s.end*total-.01);to=std::clamp(to,from+.01,s.end*total);c.audioOffset=from;c.audioDuration=to-from;}
        // Tempo Sync stretches to the project tempo; without it the audio plays at its own speed,
        // faster or slower with TRANSPOSE / FINE (varispeed), and the clip length follows.
        const double beats=c.audioDuration*(s.sync?s.sourceBpm:project.settings.bpm/std::pow(2.,(s.transpose+s.fine*.01)/12.))/60.;c.span=beats;c.loopLength=c.loopPhase=0;c.loopSource.clear();
        if(c.notes.empty())c.notes.push_back({0,beats,t.sampleRoot,127,0});c.notes.resize(1);c.notes[0].start=0;c.notes[0].length=beats;}
    if(any)project.normalise();return any;
}
bool MainComponent::trimSampleSilence(int track){
    if(!juce::isPositiveAndBelow(track,(int)project.tracks.size()))return false;const auto asset=audio.sampleAsset(track);if(!asset||asset->frames<2)return false;
    const auto& a=asset->audio;const int frames=std::min(asset->frames,a.getNumSamples()),channels=a.getNumChannels();float peak=0;
    for(int ch=0;ch<channels;++ch)peak=std::max(peak,a.getMagnitude(ch,0,frames));if(peak<=0)return false;
    // Silence: 48 dB under the file's peak (never above -60 dBFS), so tails and room noise stay.
    const float threshold=std::max(peak*.004f,.001f);auto loud=[&](int i){for(int ch=0;ch<channels;++ch)if(std::abs(a.getSample(ch,i))>threshold)return true;return false;};
    int first=0;while(first<frames&&!loud(first))++first;int last=frames-1;while(last>first&&!loud(last))--last;if(first>=last)return false;
    first=std::max(0,first-(int)(.005*asset->rate));last=std::min(frames-1,last+(int)(.02*asset->rate));   // 5 ms before the onset, 20 ms of tail
    auto& s=project.tracks[(size_t)track].sampler;const double start=(double)first/frames,end=(double)(last+1)/frames;
    if(std::abs(start-s.start)<1e-6&&std::abs(end-s.end)<1e-6)return false;
    s.start=start;s.end=end;s.loopStart=std::clamp(s.loopStart,start,end);s.loopEnd=std::clamp(s.loopEnd,s.loopStart,end);s.normalise();return true;   // the caller syncs the audio clips
}
void MainComponent::adaptAudioTrack(int track,int action){
    if(!juce::isPositiveAndBelow(track,(int)project.tracks.size())||editingRestricted())return;checkpoint();auto& t=project.tracks[(size_t)track];auto& s=t.sampler;
    if(action==0&&t.audioBpm>0){s.sync=true;s.sourceBpm=t.audioBpm;}
    if(action==1&&t.audioKey>=0){const auto h=activeHarmony(project,selectedSection);const bool projectMinor=sgk::scaleIsMinor(h.scale);
        const int tonic=(h.key+(t.audioMinor&&!projectMinor?9:!t.audioMinor&&projectMinor?3:0))%12;s.transpose=((tonic-t.audioKey)%12+18)%12-6;s.fine=0;
        if(!s.sync){s.sync=true;s.sourceBpm=project.settings.bpm;}}
    if(action==2){s.sync=false;s.transpose=0;s.fine=0;}
    if(action==3&&t.audioBpm>0){project.settings.bpm=std::clamp(t.audioBpm,20.,300.);s.sync=false;s.sourceBpm=t.audioBpm;s.normalise();project.refitAudioClips();}
    if(action==4&&t.audioKey>=0){const bool minorNow=sgk::scaleIsMinor(project.settings.scale);project.settings.key=t.audioKey;if(minorNow!=t.audioMinor)project.settings.scale=t.audioMinor?2:0;s.transpose=0;s.fine=0;
        s.normalise();applyInstrumentSettings(0,true);refreshControls();message(tr("Project key set from the audio; harmonic parts follow it. Undo restores it."));return;}
    s.normalise();
    for(auto& c:t.clips)if(c.audioOffset>=0&&c.audioDuration>0){const double beats=c.audioDuration*(s.sync?s.sourceBpm:project.settings.bpm/std::pow(2.,(s.transpose+s.fine*.01)/12.))/60.;c.span=beats;for(auto& n:c.notes){n.start=0;n.length=beats;}}
    project.normalise();changed();
    if(action==3){refreshControls();message(tr("Project tempo set from the audio. Undo restores it."));return;}
    message(action==0?tr("Audio follows the project tempo; pitch is preserved. Undo restores it."):action==1?tr("Audio transposed to the project key")+" ("+juce::String(s.transpose>0?"+":"")+juce::String(s.transpose)+" st). "+tr("Undo restores it."):tr("Original tempo and key restored."));
}
}
