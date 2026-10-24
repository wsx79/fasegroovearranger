// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MainComponent.h"
#include "GuidePage.h"
#include "UiText.h"
#include <BinaryData.h>
namespace fga {
namespace {
juce::String u(const std::string& s){return juce::String::fromUTF8(s.c_str());}
void rename(const juce::String& title,const juce::String& value,std::function<void(juce::String)> cb){auto* dialog=new juce::AlertWindow(tr(title),"",juce::MessageBoxIconType::NoIcon);dialog->addTextEditor("name",value,"Nome");dialog->addButton("OK",1,juce::KeyPress(juce::KeyPress::returnKey));dialog->addButton(tr("Cancel"),0,juce::KeyPress(juce::KeyPress::escapeKey));dialog->enterModalState(true,juce::ModalCallbackFunction::create([dialog,cb](int result){if(result==1)cb(dialog->getTextEditorContents("name"));}),true);}
class ExportTask:public juce::ThreadWithProgressWindow {
public:
    // `copy`, when given, is a private engine: the export renders there and playback goes on.
    ExportTask(AudioEngine& e,std::unique_ptr<AudioEngine> copy,Project p,juce::File path,std::vector<int> selected,bool mp3,std::function<void(juce::Result)> done)
    :ThreadWithProgressWindow(juce::String::fromUTF8("Fase Groove · Audio export"),true,true,15000),owned(std::move(copy)),engine(owned?*owned:e),project(std::move(p)),target(path),tracks(std::move(selected)),isMp3(mp3),callback(std::move(done)){}
    void run()override{
        for(size_t i=0;i<tracks.size();++i){int track=tracks[i];auto file=tracks.size()>1?target.getChildFile(juce::String(rowForTrack(track)+1).paddedLeft('0',2)+"-"+juce::File::createLegalFileName(u(project.tracks[(size_t)track].name))+(isMp3?".mp3":".wav")):target;
            setStatusMessage(file.getFileName());result=engine.exportAudio(project,file,track,isMp3,[this]{return threadShouldExit();},[this,i](double p){setProgress(((double)i+p)/(double)tracks.size());});if(result.failed())return;
        }
    }
    void threadComplete(bool cancelled)override{owned.reset();callback(cancelled?juce::Result::fail("Esportazione annullata. I file gia completati restano nella cartella."):result);}
private:std::unique_ptr<AudioEngine> owned;AudioEngine& engine;Project project;juce::File target;std::vector<int> tracks;bool isMp3;std::function<void(juce::Result)> callback;juce::Result result=juce::Result::ok();
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
    open(file);return projectFile==file;
}
void MainComponent::showChordMenu(){
    juce::PopupMenu menu;menu.addSectionHeader(juce::String::fromUTF8("Progressione · un accordo per battuta"));
    auto names=chordNames(project.settings);
    for(int slot=0;slot<project.settings.chordCount;++slot){juce::PopupMenu chord;juce::PopupMenu degrees;
        for(int d=0;d<7;++d)degrees.addItem(1000+slot*100+d,"Grado "+juce::String(d+1),true,project.settings.freeChords[(size_t)slot]<0&&project.settings.degrees[(size_t)slot]==d);
        chord.addSubMenu(tr("Nella scala"),degrees);
        for(int quality=0;quality<3;++quality){juce::PopupMenu roots;for(int root=0;root<12;++root){int code=(root-project.settings.key+12)%12+quality*12;roots.addItem(1000+slot*100+10+code,fgg::pitchClassName(root),true,project.settings.freeChords[(size_t)slot]==code);}chord.addSubMenu(quality==0?"Maggiore":quality==1?"Minore":"Dominante",roots);}
        menu.addSubMenu(juce::String(slot+1)+juce::String::fromUTF8(" · ")+juce::String::fromUTF8(names[(size_t)slot].c_str()),chord);
    }
    juce::PopupMenu lengths;for(int n:{1,2,3,4,6,8})lengths.addItem(2000+n,juce::String(n)+" accordi",true,n==project.settings.chordCount);menu.addSubMenu(tr("Lunghezza progressione"),lengths);
    juce::PopupMenu durations;const int steps[]{8,16,32,64};for(int i=0;i<4;++i)durations.addItem(2100+steps[i],tr(i==0?"1/2 bar":i==1?"1 bar":i==2?"2 bars":"4 bars"),true,project.settings.chordSteps==steps[i]);menu.addSubMenu(tr("Each chord"),durations);
    menu.addItem(3000,tr("Nuova progressione casuale"));
    juce::Component::SafePointer<MainComponent> safe(this);menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&chordEditButton),[safe](int id){if(safe==nullptr||id==0)return;safe->checkpoint();auto& s=safe->project.settings;
        if(id>=1000&&id<1800){int slot=(id-1000)/100,value=(id-1000)%100;if(value<7){s.degrees[(size_t)slot]=value;s.freeChords[(size_t)slot]=-1;}else if(value>=10&&value<46)s.freeChords[(size_t)slot]=value-10;}
        if(id>2000&&id<=2008)s.chordCount=id-2000;if(id>=2108&&id<=2164)s.chordSteps=id-2100;if(id==3000){randomiseHarmony(safe->project,(std::uint32_t)juce::Random::getSystemRandom().nextInt());generate(safe->project);}safe->changed();if(safe->page==Page::instrument)safe->showInstrument();safe->message("New chords written into unlocked tracks. GENERATE SONG draws another harmony.");
    });
}
void MainComponent::showTrackMenu(int track){
    juce::PopupMenu menu;const auto& t=project.tracks[(size_t)track];menu.addSectionHeader(u(t.name));menu.addItem(1,tr("Rinomina"));menu.addItem(2,tr("Muto"),true,t.mute);menu.addItem(3,tr("Solo"),true,t.solo);menu.addItem(4,tr("Blocca generazione"),true,t.locked);menu.addSeparator();menu.addItem(5,tr("Suoni originali Fase Groove"),!loadingPlugin);menu.addItem(6,tr("Rigenera traccia"));
    if(t.instrument=="sample"){juce::PopupMenu roots;for(int n=24;n<=96;++n)roots.addItem(100+n,juce::MidiMessage::getMidiNoteName(n,true,true,3),true,n==t.sampleRoot);menu.addSubMenu(tr("Radice campione"),roots);}
    juce::Component::SafePointer<MainComponent> safe(this);menu.showMenuAsync(juce::PopupMenu::Options(),[safe,track](int id){if(safe==nullptr||id==0)return;safe->select(track,safe->selectedSection);if(id==1){rename("Rinomina traccia",u(safe->project.tracks[(size_t)track].name),[safe,track](juce::String name){if(safe!=nullptr&&name.isNotEmpty()){safe->checkpoint();safe->project.tracks[(size_t)track].name=name.toStdString();safe->changed(false);}});return;}safe->checkpoint();auto& t=safe->project.tracks[(size_t)track];if(id==2)t.mute=!t.mute;if(id==3)t.solo=!t.solo;if(id==4)t.locked=!t.locked;if(id==5){safe->closePluginEditor();safe->audio.useInternal(track);t.instrument="internal";t.instrumentPath.clear();t.instrumentId.clear();t.instrumentState.clear();}if(id==6){safe->regenerate(Scope::track);return;}if(id>=124&&id<=196){int root=id-100;auto r=safe->audio.loadSample(track,juce::File(u(t.instrumentPath)),root);if(r.failed())safe->message(r.getErrorMessage(),true);else t.sampleRoot=root;}safe->changed(false);});
}
void MainComponent::showSectionMenu(int section){
    juce::PopupMenu menu;menu.addSectionHeader(u(project.sections[(size_t)section].name));menu.addItem(1,tr("Rinomina"));menu.addItem(2,tr("Duplicate section with notes"));menu.addItem(3,tr("Elimina"),project.sections.size()>1);menu.addItem(4,tr("Sposta a sinistra"),section>0);menu.addItem(5,tr("Sposta a destra"),section+1<(int)project.sections.size());menu.addItem(6,tr("Rigenera sezione"));menu.addItem(7,tr("Loop sezione"),true,project.loop&&project.loopMatches(project.sectionStart(section),project.sectionStart(section+1)));menu.addItem(8,tr("Clear loop"),project.loop||project.hasLoopRegion());
    juce::PopupMenu bars,types;for(int n:{1,2,4,8,12,16,24,32,48,64,96,128})bars.addItem(400+n,juce::String(n)+" bars",true,n==project.sections[(size_t)section].bars);for(int type=0;type<6;++type)types.addItem(200+type,sectionName((Section)type),true,(int)project.sections[(size_t)section].type==type);menu.addSubMenu(tr("Durata"),bars);menu.addSubMenu(tr("Funzione musicale"),types);
    juce::Component::SafePointer<MainComponent> safe(this);menu.showMenuAsync(juce::PopupMenu::Options(),[safe,section](int id){if(safe==nullptr||id==0)return;safe->select(safe->selectedTrack,section);if(id==1){rename("Rinomina sezione",u(safe->project.sections[(size_t)section].name),[safe,section](juce::String name){if(safe!=nullptr&&name.isNotEmpty()){safe->checkpoint();safe->project.sections[(size_t)section].name=name.toStdString();safe->changed();}});return;}safe->checkpoint();if(id==2){if(safe->project.duplicateSection(section))safe->selectedSection++;}if(id==3){safe->project.removeGuide(section);safe->selectedSection=std::min(section,(int)safe->project.sections.size()-1);}if(id==4){safe->project.moveGuide(section,section-1);safe->selectedSection--;}if(id==5){safe->project.moveGuide(section,section+1);safe->selectedSection++;}if(id==6){safe->regenerate(Scope::section);return;}if(id==7){safe->project.toggleLoopRegion(safe->project.sectionStart(section),safe->project.sectionStart(section+1));safe->changed(false);return;}if(id==8){safe->project.clearLoop();safe->changed(false);return;}if(id>=401&&id<=528){safe->project.resizeGuide(section,id-400);}else if(id>=200&&id<206){for(int track=0;track<trackCount;++track)safe->project.detachTrack(track);safe->project.sections[(size_t)section].type=(Section)(id-200);safe->project.sections[(size_t)section].name=sectionName((Section)(id-200));}
        safe->project.loopEnd=std::min(safe->project.loopEnd,safe->project.length());safe->piano.clearSelection();safe->changed();
    });
}
void MainComponent::showClipMenu(int track,int index){
    const bool hasClip=index>=0&&index<(int)project.tracks[(size_t)track].clips.size();juce::PopupMenu menu;
    menu.addItem(11,tr("Cut clip"),hasClip);menu.addItem(12,tr("Copy clip"),hasClip);menu.addItem(13,tr("Paste clips at cursor"),!timelineClipboard.empty());menu.addItem(14,tr("Split at cursor"),hasClip);menu.addItem(15,tr("Duplicate clip"),hasClip);menu.addItem(16,tr("Delete clip"),hasClip);menu.addItem(18,tr("Create empty MIDI clip"));menu.addItem(4,tr("Humanize notes"),hasClip);menu.addSeparator();
    menu.addItem(1,tr("Rigenera solo questo clip"),hasClip);menu.addItem(2,tr("Blocca generazione"),hasClip,hasClip&&project.tracks[(size_t)track].clips[(size_t)index].locked);menu.addItem(6,tr("LOOP CLIP"),hasClip,hasClip&&project.loop&&project.loopMatches(project.clipStart(track,index),project.clipStart(track,index)+project.clipLength(track,index)));menu.addItem(7,tr("Clear loop"),project.loop||project.hasLoopRegion());
    juce::Component::SafePointer<MainComponent> safe(this);menu.showMenuAsync(juce::PopupMenu::Options(),[safe,track,index](int id){if(!safe||!id)return;safe->selectClip(track,index,true);if(id==4){safe->checkpoint();humanize(safe->clip(),.65,(std::uint32_t)juce::Random::getSystemRandom().nextInt(),safe->clipLength());tidyNotes(safe->clip(),safe->clipLength());safe->changed();return;}if(id>=11){safe->timelineCommand(id-10);return;}if(id==1)safe->regenerate(Scope::clip);if(id==2){safe->checkpoint();safe->clip().locked=!safe->clip().locked;safe->changed(false);}if(id==6)safe->toggleClipLoop();if(id==7)safe->clearLoop();});
}
void MainComponent::showGuide(bool english){juce::DialogWindow::LaunchOptions o;o.dialogTitle=english?"Fase Groove Arranger / User guide":"Fase Groove Arranger / Guida";o.dialogBackgroundColour=Palette::background;o.useNativeTitleBar=true;o.resizable=true;o.content.setOwned(createGuidePage(english).release());o.launchAsync();}
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
void MainComponent::showDocument(const juce::String& title,const juce::String& text){juce::DialogWindow::LaunchOptions o;o.dialogTitle=title;o.dialogBackgroundColour=Palette::panel;o.useNativeTitleBar=true;o.resizable=true;o.content.setOwned(createReferencePage(title,text).release());o.launchAsync();}
void MainComponent::showAbout(){
    float scale=1.f;if(auto* p=getParentComponent())scale=p->getWidth()/1440.f;
    options.checkUpdatesButton.setEnabled(!updater.isBusy());options.checkUpdatesButton.setButtonText(tr(updater.isBusy()?"CHECKING...":"CHECK NOW"));
    options.show(false,scale);
}
void MainComponent::showAudioSettings(){
    juce::PopupMenu menu;menu.addItem(1,tr("Dispositivo audio / buffer"));menu.addItem(3,tr("Apri cartella autosave"));menu.addItem(4,tr("Recupera autosave"));menu.addSeparator();menu.addItem(5,tr("Metronome (C)"),true,audio.metronomeOn());juce::Component::SafePointer<MainComponent> safe(this);menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&settingsButton),[safe](int id){if(safe==nullptr)return;if(id==5){safe->toggleMetronome();return;}if(id==1){juce::DialogWindow::LaunchOptions o;o.dialogTitle=juce::String::fromUTF8("Fase Groove · Audio");o.dialogBackgroundColour=Palette::panel;o.useNativeTitleBar=true;o.resizable=false;auto* selector=new juce::AudioDeviceSelectorComponent(safe->audio.devices,0,0,0,2,false,false,true,false);selector->setSize(680,500);o.content.setOwned(selector);o.launchAsync();}
        if(id==3||id==4){auto folder=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("FS Pitch/Fase Groove Arranger");folder.createDirectory();if(id==3)folder.revealToUser();else{const auto autosave=folder.getChildFile("autosave.fga");safe->open(autosave);
            // A recovered song is unsaved work: SAVE must ask where to keep it, never overwrite the autosave slot.
            if(safe!=nullptr&&safe->projectFile==autosave){safe->projectFile=juce::File();safe->dirty=true;safe->repaint();}}}
    });
}
void MainComponent::exportMenu(){
    if(loadingPlugin){message("Attendi il caricamento dei plugin.");return;}juce::PopupMenu m;m.addSectionHeader(juce::String::fromUTF8("MIDI · note modificabili"));m.addItem(1,tr("Canzone multitraccia (.mid)"));m.addItem(2,tr("Traccia selezionata (.mid)"));m.addItem(3,tr("Tutte le tracce MIDI separate"));m.addSectionHeader(juce::String::fromUTF8("WAV · 24 bit stereo / frequenza dispositivo"));m.addItem(4,tr("Mix completo WAV"));m.addItem(5,tr("Traccia selezionata WAV"));m.addItem(6,tr("Tutti gli stem WAV"));m.addSectionHeader(juce::String::fromUTF8("MP3 · 320 kbps / 44.1 kHz / stereo"));m.addItem(7,tr("Mix completo MP3"));m.addItem(8,tr("Traccia selezionata MP3"));m.addItem(9,tr("Tutti gli stem MP3"));juce::Component::SafePointer<MainComponent> safe(this);m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&exportButton),[safe](int id){if(safe!=nullptr&&id>0)safe->exportFile(id,safe->selectedTrack);});
}
void MainComponent::exportFile(int type,int track){
    const bool batch=type==3||type==6||type==9;const bool mp3=type>=7;const bool midi=type<=3;const bool single=type==2||type==5||type==8;
    int flags=batch?juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories:juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting;
    chooseFile(batch?"Cartella esportazione (crea una sottocartella dedicata)":"Esporta "+juce::String(midi?"MIDI":mp3?"MP3":"WAV"),batch?"*":midi?"*.mid":mp3?"*.mp3":"*.wav",flags,[this,type,track,batch,mp3,midi,single](juce::File target){
        if(batch){target=target.getChildFile(juce::File::createLegalFileName(u(project.name))+"-stems-"+juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S"));if(target.createDirectory().failed()){message("Impossibile creare la cartella stem.",true);return;}}
        else target=target.withFileExtension(midi?"mid":mp3?"mp3":"wav");
        if(midi){auto write=[this](const juce::File& file,int ti){auto bytes=midiFile(project,ti);juce::TemporaryFile temp(file);if(!temp.getFile().replaceWithData(bytes.data(),bytes.size())||!temp.overwriteTargetFileWithTemporary())return false;return true;};bool ok=true;if(batch){for(int ti=0;ti<trackCount;++ti)ok=write(target.getChildFile(juce::String(rowForTrack(ti)+1).paddedLeft('0',2)+"-"+juce::File::createLegalFileName(u(project.tracks[(size_t)ti].name))+".mid"),ti)&&ok;}else ok=write(target,single?track:-1);message(ok?"MIDI esportato: "+target.getFullPathName():"Esportazione MIDI non completata.",!ok);return;}
        // Render on a private copy of the instruments so playback keeps running; only when a
        // plugin cannot be duplicated does the export take over the live engine (and stop it).
        auto copy=audio.offlineCopy();if(!copy){audio.stop();closePluginEditor();}std::vector<int> tracks;if(batch)for(int row=0;row<trackCount;++row)tracks.push_back(trackAtRow(row));else tracks.push_back(single?track:-1);
        juce::Component::SafePointer<MainComponent> safe(this);exportTask=std::make_unique<ExportTask>(audio,std::move(copy),project,target,tracks,mp3,[safe,target](juce::Result r){if(safe!=nullptr)safe->message(r.wasOk()?"Audio esportato: "+target.getFullPathName():r.getErrorMessage(),r.failed());});exportTask->launchThread();
    });
}
}
