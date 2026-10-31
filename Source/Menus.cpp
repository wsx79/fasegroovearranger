// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MainComponent.h"
#include "GuidePage.h"
#include "UiText.h"
#include "ExportSupport.h"
#include <BinaryData.h>
namespace fga {
namespace {
juce::String u(const std::string& s){return juce::String::fromUTF8(s.c_str());}
AudioExportOptions exportSettings(){const auto requested=Localisation::setting("exportSampleRate","48000").getIntValue();const int rates[]{44100,48000,88200,96000,192000};const bool known=std::find(std::begin(rates),std::end(rates),requested)!=std::end(rates);return {known?(double)requested:48000.,Localisation::setting("exportBitDepth","24")=="16"?16:24};}
void rename(const juce::String& title,const juce::String& value,std::function<void(juce::String)> cb){auto* dialog=new juce::AlertWindow(tr(title),"",juce::MessageBoxIconType::NoIcon);dialog->addTextEditor("name",value,"Nome");dialog->addButton("OK",1,juce::KeyPress(juce::KeyPress::returnKey));dialog->addButton(tr("Cancel"),0,juce::KeyPress(juce::KeyPress::escapeKey));dialog->enterModalState(true,juce::ModalCallbackFunction::create([dialog,cb](int result){if(result==1)cb(dialog->getTextEditorContents("name"));}),true);}
class ExportTask:public juce::ThreadWithProgressWindow {
public:
    ExportTask(AudioEngine& e,Project p,juce::File path,std::vector<int> selected,bool mp3,bool batch,AudioExportOptions settings,std::function<void(juce::Result)> done)
    :ThreadWithProgressWindow(juce::String::fromUTF8("Fase Groove · Audio export"),true,true,15000),engine(e),project(std::move(p)),target(path),tracks(std::move(selected)),isMp3(mp3),stems(batch),options(settings),callback(std::move(done)){}
    void run()override{
        // Write the arrangement map first, so it also accompanies any stems completed
        // before cancellation. Audio files themselves use atomic temporary-file writes.
        if(stems&&!writeExportArrangement(target,project)){result=juce::Result::fail("Cannot write the arrangement text file.");return;}
        for(size_t i=0;i<tracks.size();++i){const int track=tracks[i];const auto file=stems?exportStemFile(target,project,track,isMp3?"mp3":"wav"):target;
            setStatusMessage(file.getFileName());double blockPeak=0;result=engine.exportAudio(project,file,track,isMp3,[this]{return threadShouldExit();},[this,i](double p){setProgress(((double)i+p)/(double)tracks.size());},&blockPeak,options);peak=std::max(peak,blockPeak);if(result.failed())return;
        }
    }
    void threadComplete(bool cancelled)override{if(!cancelled&&result.wasOk()&&peak>1.)result=juce::Result::fail("Exported, but the mix peaked at "+juce::String(juce::Decibels::gainToDecibels((float)peak),1)+" dB and was limited to -0.01 dBFS.");callback(cancelled?juce::Result::fail("Esportazione annullata. I file gia completati restano nella cartella."):result);}
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
    project.insertSection(at,{type,8,sectionName(type)},-1);selectedSection=at;
    generate(project,Scope::section,selectedTrack,at);
    changed();select(selectedTrack,at);
    message(tr("Section added and generated. REGENERATE rewrites it; Undo removes it."));
}
void MainComponent::showTrackMenu(int track){
    juce::PopupMenu menu;const auto& t=project.tracks[(size_t)track];menu.addSectionHeader(u(t.name));menu.addItem(1,tr("Rinomina"));menu.addItem(2,tr("Muto"),true,t.mute);menu.addItem(3,tr("Solo"),true,t.solo);menu.addItem(4,tr("Blocca generazione"),true,t.locked);menu.addSeparator();menu.addItem(5,tr("Suoni originali Fase Groove"),!loadingPlugin);menu.addItem(6,tr("Rigenera traccia"),hasGenerator(t));menu.addSeparator();menu.addItem(7,tr("Add MIDI track"),project.tracks.size()<maxTracks);menu.addItem(9,tr("VOCAL CHOPS"),project.tracks.size()<maxTracks);if(isManualTrack(track))menu.addItem(8,tr("Delete MIDI track"),!loadingPlugin);
    if(t.instrument=="sample"){juce::PopupMenu roots;for(int n=24;n<=96;++n)roots.addItem(100+n,juce::MidiMessage::getMidiNoteName(n,true,true,3),true,n==t.sampleRoot);menu.addSubMenu(tr("Radice campione"),roots);}
    juce::Component::SafePointer<MainComponent> safe(this);menu.showMenuAsync(juce::PopupMenu::Options(),[safe,track](int id){if(safe==nullptr||id==0)return;if(id==7){safe->addMidiTrack();return;}if(id==9){safe->addMidiTrack(true);return;}if(id==8){safe->deleteMidiTrack(track);return;}if(track>=(int)safe->project.tracks.size())return;safe->select(track,safe->selectedSection);if(id==1){rename("Rinomina traccia",u(safe->project.tracks[(size_t)track].name),[safe,track](juce::String name){if(safe!=nullptr&&name.trim().isNotEmpty()&&track<(int)safe->project.tracks.size()){safe->checkpoint();safe->project.tracks[(size_t)track].name=name.trim().substring(0,128).toStdString();safe->changed(false);}});return;}if(id==6){safe->regenerate(Scope::track);return;}safe->checkpoint();auto& t=safe->project.tracks[(size_t)track];if(id==2)t.mute=!t.mute;if(id==3)t.solo=!t.solo;if(id==4)t.locked=!t.locked;if(id==5){safe->closePluginEditor();safe->audio.useInternal(track);t.instrument="internal";t.instrumentPath.clear();t.instrumentId.clear();t.instrumentState.clear();}if(id>=124&&id<=196){int root=id-100;t.sampleRoot=root;}safe->changed(false);});
}
void MainComponent::showSectionMenu(int section){
    juce::PopupMenu menu;menu.addSectionHeader(u(project.sections[(size_t)section].name));menu.addItem(1,tr("Rinomina"));menu.addItem(2,tr("Duplicate section with notes"));menu.addItem(3,tr("Elimina"),project.sections.size()>1);menu.addItem(4,tr("Sposta a sinistra"),section>0);menu.addItem(5,tr("Sposta a destra"),section+1<(int)project.sections.size());menu.addItem(6,tr("Rigenera sezione"));menu.addItem(7,tr("Loop sezione"),true,project.loop&&project.loopMatches(project.sectionStart(section),project.sectionStart(section+1)));menu.addItem(8,tr("Clear loop"),project.loop||project.hasLoopRegion());
    juce::PopupMenu bars,types;for(int n:{1,2,4,8,12,16,24,32,48,64,96,128})bars.addItem(400+n,juce::String(n)+" bars",true,n==project.sections[(size_t)section].bars);for(int type=0;type<6;++type)types.addItem(200+type,sectionName((Section)type),true,(int)project.sections[(size_t)section].type==type);menu.addSubMenu(tr("Durata"),bars);menu.addSubMenu(tr("Funzione musicale"),types);
    juce::Component::SafePointer<MainComponent> safe(this);menu.showMenuAsync(juce::PopupMenu::Options(),[safe,section](int id){if(safe==nullptr||id==0)return;safe->select(safe->selectedTrack,section);if(id==1){rename("Rinomina sezione",u(safe->project.sections[(size_t)section].name),[safe,section](juce::String name){if(safe!=nullptr&&name.isNotEmpty()){safe->checkpoint();safe->project.sections[(size_t)section].name=name.toStdString();safe->changed();}});return;}safe->checkpoint();if(id==2){if(safe->project.duplicateSection(section))safe->selectedSection++;}if(id==3){safe->project.removeGuide(section);safe->selectedSection=std::min(section,(int)safe->project.sections.size()-1);}if(id==4){safe->project.moveGuide(section,section-1);safe->selectedSection--;}if(id==5){safe->project.moveGuide(section,section+1);safe->selectedSection++;}if(id==6){safe->regenerate(Scope::section);return;}if(id==7){safe->project.toggleLoopRegion(safe->project.sectionStart(section),safe->project.sectionStart(section+1));safe->changed(false);return;}if(id==8){safe->project.clearLoop();safe->changed(false);return;}if(id>=401&&id<=528){safe->project.resizeGuide(section,id-400);}else if(id>=200&&id<206){for(int track=0;track<(int)safe->project.tracks.size();++track)safe->project.detachTrack(track);safe->project.sections[(size_t)section].type=(Section)(id-200);safe->project.sections[(size_t)section].name=sectionName((Section)(id-200));}
        safe->project.loopEnd=std::min(safe->project.loopEnd,safe->project.length());safe->piano.clearSelection();safe->changed();
    });
}
void MainComponent::showClipMenu(int track,int index){
    const bool hasClip=index>=0&&index<(int)project.tracks[(size_t)track].clips.size();juce::PopupMenu menu;
    menu.addItem(11,tr("Cut clip"),hasClip);menu.addItem(12,tr("Copy clip"),hasClip);menu.addItem(13,tr("Paste clips at cursor"),!timelineClipboard.empty());menu.addItem(14,tr("Split at cursor"),hasClip);menu.addItem(15,tr("Duplicate clip"),hasClip);menu.addItem(16,tr("Delete clip"),hasClip);menu.addItem(18,tr("Create empty MIDI clip"));menu.addItem(4,tr("Humanize notes"),hasClip);menu.addSeparator();
    menu.addItem(1,tr("Rigenera solo questo clip"),hasClip);menu.addItem(2,tr("Blocca generazione"),hasClip,hasClip&&project.tracks[(size_t)track].clips[(size_t)index].locked);menu.addItem(6,tr("LOOP CLIP"),hasClip,hasClip&&project.loop&&project.loopMatches(project.clipStart(track,index),project.clipStart(track,index)+project.clipLength(track,index)));menu.addItem(7,tr("Clear loop"),project.loop||project.hasLoopRegion());
    juce::Component::SafePointer<MainComponent> safe(this);menu.showMenuAsync(juce::PopupMenu::Options(),[safe,track,index](int id){if(!safe||!id)return;safe->selectClip(track,index,true);if(id==4){safe->checkpoint();humanize(safe->clip(),.65,(std::uint32_t)juce::Random::getSystemRandom().nextInt(),safe->clipLength());tidyNotes(safe->clip(),safe->clipLength());safe->changed();return;}if(id>=11){safe->timelineCommand(id-10);return;}if(id==1)safe->regenerate(Scope::clip);if(id==2){safe->checkpoint();safe->clip().locked=!safe->clip().locked;safe->changed(false);}if(id==6)safe->toggleClipLoop();if(id==7)safe->clearLoop();});
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
void MainComponent::showDocument(const juce::String& title,const juce::String& text){juce::DialogWindow::LaunchOptions o;o.dialogTitle=title;o.dialogBackgroundColour=Palette::panel;o.useNativeTitleBar=true;o.resizable=true;o.content.setOwned(createReferencePage(title,text).release());o.launchAsync();}
void MainComponent::showAbout(){
    float scale=1.f;if(auto* p=getParentComponent())scale=p->getWidth()/1440.f;
    options.checkUpdatesButton.setEnabled(!updater.isBusy());options.checkUpdatesButton.setButtonText(tr(updater.isBusy()?"CHECKING...":"CHECK NOW"));
    options.show(false,scale);
}
void MainComponent::showAudioSettings(){
    juce::PopupMenu menu;menu.addSectionHeader(tr("AUDIO / MIDI"));menu.addItem(1,tr("Dispositivo audio / buffer"));menu.addItem(3,tr("Apri cartella autosave"));menu.addItem(4,tr("Recupera autosave"));menu.addSeparator();menu.addItem(5,tr("Metronome (C)"),true,audio.metronomeOn());juce::Component::SafePointer<MainComponent> safe(this);menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&settingsButton).withMinimumWidth(240).withStandardItemHeight(30),[safe](int id){if(safe==nullptr)return;if(id==5){safe->toggleMetronome();return;}if(id==1){juce::DialogWindow::LaunchOptions o;o.dialogTitle=juce::String::fromUTF8("Fase Groove · Audio");o.dialogBackgroundColour=Palette::panel;o.useNativeTitleBar=true;o.resizable=false;auto* selector=new juce::AudioDeviceSelectorComponent(safe->audio.devices,0,2,0,2,false,false,true,false);selector->setSize(920,640);o.content.setOwned(selector);o.componentToCentreAround=safe.getComponent();o.launchAsync();}
        if(id==3||id==4){auto folder=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("FS Pitch/Fase Groove Arranger");folder.createDirectory();if(id==3)folder.revealToUser();else{const auto autosave=folder.getChildFile("autosave.fga");safe->open(autosave,true);}}
    });
}
void MainComponent::exportMenu(){
    if(loadingPlugin){message("Attendi il caricamento dei plugin.");return;}juce::PopupMenu m;m.addItem(20,"STUDIO - SAMPLER / MIDI / MEDIA");m.addSeparator();m.addSectionHeader(juce::String::fromUTF8("MIDI · note modificabili"));m.addItem(1,tr("Canzone multitraccia (.mid)"));m.addItem(2,tr("Traccia selezionata (.mid)"));m.addItem(3,tr("Tutte le tracce MIDI separate"));m.addSectionHeader(juce::String::fromUTF8("WAV · ")+tr("Sample rate / bit depth"));m.addItem(4,tr("Mix completo WAV"));m.addItem(5,tr("Traccia selezionata WAV"));m.addItem(6,tr("Tutti gli stem WAV"));m.addSectionHeader(juce::String::fromUTF8("MP3 · 320 kbps / 44.1 kHz / stereo"));m.addItem(7,tr("Mix completo MP3"));m.addItem(8,tr("Traccia selezionata MP3"));m.addItem(9,tr("Tutti gli stem MP3"));juce::Component::SafePointer<MainComponent> safe(this);m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&exportButton),[safe](int id){if(safe!=nullptr&&id==20)safe->showStudio();else if(safe!=nullptr&&id>0)safe->exportFile(id,safe->selectedTrack);});
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
        if(batch){target=target.getChildFile(juce::File::createLegalFileName(u(project.name))+exportSuffix(project)+"-stems-"+juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S"));target=target.getNonexistentSibling();if(target.createDirectory().failed()){message("Impossibile creare la cartella stem.",true);return;}}
        else target=exportNamedFile(target,project,midi?"mid":mp3?"mp3":"wav");
        if(midi){auto write=[this](const juce::File& file,int ti){auto bytes=midiFile(project,ti);juce::TemporaryFile temp(file);return temp.getFile().replaceWithData(bytes.data(),bytes.size())&&temp.overwriteTargetFileWithTemporary();};bool ok=true;if(batch){ok=writeExportArrangement(target,project);for(int row=0;row<(int)project.tracks.size();++row){const int ti=trackAtRow(row);ok=write(exportStemFile(target,project,ti,"mid"),ti)&&ok;}}else ok=write(target,single?track:-1);message(ok?"MIDI esportato: "+target.getFullPathName():"Esportazione MIDI non completata.",!ok);return;}
        instrumentPanel.flushRender();audio.stop();audio.stopPreview();audio.stopSampleAudition();mediaBusy=true;updateEditingAvailability();
        // Stop the live callback and render the existing plugin instances. This avoids
        // reloading libraries/licences and frees the device callback during the export.
        audio.devices.removeAudioCallback(&audio);
        std::vector<int> tracks;if(batch)for(int row=0;row<(int)project.tracks.size();++row)tracks.push_back(trackAtRow(row));else tracks.push_back(single?track:-1);
        juce::Component::SafePointer<MainComponent> safe(this);exportTask=std::make_unique<ExportTask>(audio,project,target,tracks,mp3,batch,encoding,[safe,target](juce::Result r){if(safe){safe->audio.devices.addAudioCallback(&safe->audio);safe->mediaBusy=false;safe->updateEditingAvailability();safe->message(r.wasOk()?"Audio esportato: "+target.getFullPathName():r.getErrorMessage(),r.failed());}});if(auto* alert=exportTask->getAlertWindow())alert->setAlwaysOnTop(true);exportTask->launchThread();
    };
    if(destination!=juce::File()){render(destination);return;}
    auto folder=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);const auto remembered=Localisation::setting("lastExportFolder");if(juce::File::isAbsolutePath(remembered)&&juce::File(remembered).isDirectory())folder=juce::File(remembered);
    const auto extension=midi?"mid":mp3?"mp3":"wav";const auto suggested=batch?folder:folder.getChildFile(juce::File::createLegalFileName(single?u(project.tracks[(size_t)track].name):u(project.name))+exportSuffix(project)+"."+extension);
    chooseFile(batch?"Cartella esportazione (crea una sottocartella dedicata)":"Esporta "+juce::String(midi?"MIDI":mp3?"MP3":"WAV"),batch?"*":"*."+juce::String(extension),flags,std::move(render),{},suggested);
}
bool MainComponent::verifyExports(const juce::File& folder){
#if !FGA_SELF_TESTS
    juce::ignoreUnused(folder);return false;
#else
    if(!folder.createDirectory())return false;
    const auto saved=project;const auto savedFile=projectFile;const bool wasDirty=dirty;const auto oldRate=Localisation::setting("exportSampleRate","48000"),oldBits=Localisation::setting("exportBitDepth","24"),oldFolder=Localisation::setting("lastExportFolder"),oldSave=Localisation::setting("lastSaveFolder");
    auto* original=audio.plugin(0);juce::String report;bool ok=true;auto check=[&](bool pass,const char* label){ok=pass&&ok;report+=juce::String(pass?"PASS ":"FAIL ")+label+"\n";};
    project=newProject(7);project.settings.bpm=120;project.settings.key=1;project.settings.scale=0;project.sections={{Section::intro,1,"Intro"},{Section::main,2,"Jackin groove"}};project.normalise();project.masterGain=.5;project.masterAutomation.enabled=false;
    for(auto& track:project.tracks){track.mute=true;track.volume.enabled=false;track.panAutomation.enabled=false;track.parameters.clear();for(auto& clip:track.clips)clip.notes.clear();}auto& pianoTrack=project.tracks[0];pianoTrack.mute=false;pianoTrack.gain=.5;pianoTrack.pan=0;pianoTrack.instrument=original?"plugin":"internal";pianoTrack.clips[0].notes={{0,2,69,100,0}};audio.publish(project);
    auto settle=[&]{const auto deadline=juce::Time::getMillisecondCounterHiRes()+30000.;while(mediaBusy&&juce::Time::getMillisecondCounterHiRes()<deadline)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);return !mediaBusy&&!audio.isExporting()&&!audio.isPlaying()&&!editingRestricted();};
    exportFile(5,0);auto* encodingDialog=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());const bool hasEncoding=encodingDialog&&encodingDialog->getComboBoxComponent("rate")&&encodingDialog->getComboBoxComponent("depth");check(hasEncoding&&encodingDialog->getComboBoxComponent("rate")->getNumItems()==5&&encodingDialog->getComboBoxComponent("depth")->getNumItems()==2,"WAV export presents working sample-rate and bit-depth choices");
    if(encodingDialog){if(auto out=folder.getChildFile("wav-options.png").createOutputStream()){juce::PNGImageFormat png;png.writeImageToStream(encodingDialog->createComponentSnapshot(encodingDialog->getLocalBounds()),*out);}encodingDialog->exitModalState(0);juce::MessageManager::getInstance()->runDispatchLoopUntil(20);}check(!mediaBusy&&!editingRestricted(),"cancelling encoding choices leaves the DAW available");
    const auto chosen=folder.getChildFile("Piano.wav");bool pathRequested=false,encodingDestroyed=false,validChoice=false;juce::Component::SafePointer<juce::AlertWindow> finishedDialog;
    fileChoiceForTest=[&](const juce::File& initial,int mode,std::function<void(juce::File)> accepted,std::function<void()>){pathRequested=true;encodingDestroyed=finishedDialog==nullptr;validChoice=initial.hasFileExtension("wav")&&(mode&juce::FileBrowserComponent::saveMode)!=0;accepted(chosen);};
    audio.play(true);exportFile(5,0);auto* acceptedDialog=dynamic_cast<juce::AlertWindow*>(juce::Component::getCurrentlyModalComponent());finishedDialog=acceptedDialog;
    if(acceptedDialog){acceptedDialog->getComboBoxComponent("rate")->setSelectedItemIndex(1);acceptedDialog->getComboBoxComponent("depth")->setSelectedItemIndex(0);acceptedDialog->triggerButtonClick(tr("EXPORT"));}
    const auto choiceDeadline=juce::Time::getMillisecondCounterHiRes()+5000.;while(!pathRequested&&juce::Time::getMillisecondCounterHiRes()<choiceDeadline)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
    fileChoiceForTest={};check(pathRequested&&encodingDestroyed&&validChoice,"confirming WAV encoding opens the file chooser after destroying the previous modal window");
    check(mediaBusy&&!audio.isPlaying(),"confirming export suspends live playback and editing");check(settle(),"successful export restores audio device availability and editing");
    juce::WavAudioFormat wav;{const auto file=exportNamedFile(chosen,project,"wav");std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(file.createInputStream().release(),true));juce::AudioBuffer<float> samples(2,8192);const bool audible=reader&&reader->read(&samples,0,8192,4096,true,true)&&samples.getMagnitude(0,8192)>.0001f;check(audible&&reader->sampleRate==48000&&reader->bitsPerSample==16,"WAV export through the actual UI contains music at 48 kHz / 16 bit");}
    check(audio.plugin(0)==original,"export retains the already loaded instrument instance");
    exportFile(1,0,folder.getChildFile("Song.mid"));const auto midi=exportNamedFile(folder.getChildFile("Song.mid"),project,"mid");auto stream=midi.createInputStream();juce::MidiFile parsed;check(stream&&parsed.readFrom(*stream),"multitrack MIDI filename includes BPM and key and contains readable MIDI");
    Localisation::setSetting("exportSampleRate","44100");Localisation::setSetting("exportBitDepth","24");exportFile(6,0,folder);check(settle(),"stem export completes and releases restrictions");const auto stems=folder.findChildFiles(juce::File::findDirectories,false,"*-stems-*");bool stemOk=stems.size()==1;if(stemOk){const auto files=stems[0].findChildFiles(juce::File::findFiles,false,"*.wav");stemOk=files.size()==(int)project.tracks.size();for(const auto& file:files){std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(file.createInputStream().release(),true));stemOk=reader&&reader->sampleRate==44100&&reader->bitsPerSample==24&&file.getFileName().contains("_120bpm_Db")&&stemOk;}const auto map=stems[0].getChildFile("Arrangiamento_120bpm_Db.txt").loadFileAsString();stemOk=stemOk&&map.contains("00:00.000 - 00:02.000  Intro")&&map.contains("00:02.000 - 00:06.000  Jackin groove");}check(stemOk,"all stems have the selected encoding, musical names and an exact section timeline TXT");
    exportFile(5,0,folder.getChildFile("cancel.wav"));if(exportTask)exportTask->signalThreadShouldExit();check(settle(),"cancelling export restores audio and editing");
    exportFile(5,0,folder.getChildFile("missing-parent/fail.wav"));check(settle()&&!exportNamedFile(folder.getChildFile("missing-parent/fail.wav"),project,"wav").existsAsFile(),"write failure restores audio and editing without a partial WAV");
    check(writeProjectFile(folder.getChildFile("Saved.fga"))&&Localisation::setting("lastSaveFolder")==folder.getFullPathName(),"successful Save remembers its directory");check(Localisation::setting("lastExportFolder")==folder.getFullPathName(),"export remembers its directory and retains it after an invalid destination");
    project=saved;projectFile=savedFile;dirty=wasDirty;audio.publish(project);Localisation::setSetting("exportSampleRate",oldRate);Localisation::setSetting("exportBitDepth",oldBits);Localisation::setSetting("lastExportFolder",oldFolder);Localisation::setSetting("lastSaveFolder",oldSave);refreshControls();folder.getChildFile("export-ui.txt").replaceWithText(report);return ok;
#endif
}
}
