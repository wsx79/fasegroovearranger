// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MainComponent.h"
#include "GuidePage.h"
#include <BinaryData.h>
namespace fga {
namespace {
juce::String u(const std::string& s){return juce::String::fromUTF8(s.c_str());}
void rename(const juce::String& title,const juce::String& value,std::function<void(juce::String)> cb){auto* dialog=new juce::AlertWindow(title,"",juce::MessageBoxIconType::NoIcon);dialog->addTextEditor("name",value,"Nome");dialog->addButton("OK",1,juce::KeyPress(juce::KeyPress::returnKey));dialog->addButton("Annulla",0,juce::KeyPress(juce::KeyPress::escapeKey));dialog->enterModalState(true,juce::ModalCallbackFunction::create([dialog,cb](int result){if(result==1)cb(dialog->getTextEditorContents("name"));}),true);}
class AboutPage:public juce::Component {
public:
    AboutPage(){setLookAndFeel(&theme);
        logo=juce::ImageCache::getFromMemory(BinaryData::fs_pitch_logo_png,BinaryData::fs_pitch_logo_pngSize);
        const juce::StringArray titles{"SOURCE CODE","Ko-fi","PayPal","Spotify"};const juce::StringArray urls{"https://github.com/wsx79/fasegroovearranger","https://ko-fi.com/fabios1979","https://paypal.me/fabios79","https://open.spotify.com/artist/29iQGuXML8LEw3xRp1dNRP"};
        for(int i=0;i<4;++i){auto* b=buttons.add(new juce::TextButton(titles[i]));auto url=urls[i];b->onClick=[url]{juce::URL(url).launchInDefaultBrowser();};addAndMakeVisible(b);}setSize(780,690);
    }
    void paint(juce::Graphics& g)override{
        g.fillAll(Palette::panel);auto line=[&](juce::String text,int y,int h,float size,juce::Colour colour=Palette::text){g.setColour(colour);g.setFont(FamilyTheme::font(size));g.drawText(text,20,y,getWidth()-40,h,juce::Justification::centred);};
        drawFamilyLogo(g,{210,18,360,64});line(juce::String::fromUTF8("Piano + Drums + Guitar · one ecosystem"),88,22,14,Palette::dim);line(juce::String::fromUTF8("Standalone 0.2.1 · Windows / macOS / Linux"),116,22,12,Palette::dim);
        line("CREATED BY",148,22,11,Palette::accent);if(logo.isValid())g.drawImageWithin(logo,100,176,getWidth()-200,100,juce::RectanglePlacement::centred);line(juce::String::fromUTF8("Fabio 'Pitch' Serra · DJ & Producer"),280,25,17);line("Listen to my productions on Spotify",310,22,12,juce::Colour(0xff1db954));
        line("DEVELOPMENT ASSISTANCE",366,22,11,Palette::accent);line("Claude (Anthropic)  /  Codex (OpenAI)",402,26,17);line("AI coding assistants",438,22,12,Palette::dim);
        line(juce::String::fromUTF8("Free & open source · GNU AGPL v3 or later"),543,24,13);line(juce::String::fromUTF8("Built with JUCE · Inter typeface: SIL Open Font License"),570,20,11,Palette::dim);line("VST is a registered trademark of Steinberg Media Technologies GmbH",594,20,10,Palette::dim);
    }
    ~AboutPage()override{setLookAndFeel(nullptr);}
    void resized()override{int width=(getWidth()-70)/4;for(int i=0;i<buttons.size();++i)buttons[i]->setBounds(20+i*(width+10),getHeight()-52,width,32);}
private:FamilyLookAndFeel theme;juce::Image logo;juce::OwnedArray<juce::TextButton> buttons;
};
class ExportTask:public juce::ThreadWithProgressWindow {
public:
    ExportTask(AudioEngine& e,Project p,juce::File path,std::vector<int> selected,bool mp3,std::function<void(juce::Result)> done)
    :ThreadWithProgressWindow(juce::String::fromUTF8("Fase Groove · Audio export"),true,true,15000),engine(e),project(std::move(p)),target(path),tracks(std::move(selected)),isMp3(mp3),callback(std::move(done)){}
    void run()override{
        for(size_t i=0;i<tracks.size();++i){int track=tracks[i];auto file=tracks.size()>1?target.getChildFile(juce::String(rowForTrack(track)+1).paddedLeft('0',2)+"-"+juce::File::createLegalFileName(u(project.tracks[(size_t)track].name))+(isMp3?".mp3":".wav")):target;
            setStatusMessage(file.getFileName());result=engine.exportAudio(project,file,track,isMp3,[this]{return threadShouldExit();},[this,i](double p){setProgress(((double)i+p)/(double)tracks.size());});if(result.failed())return;
        }
    }
    void threadComplete(bool cancelled)override{callback(cancelled?juce::Result::fail("Esportazione annullata. I file gia completati restano nella cartella."):result);}
private:AudioEngine& engine;Project project;juce::File target;std::vector<int> tracks;bool isMp3;std::function<void(juce::Result)> callback;juce::Result result=juce::Result::ok();
};
}
void MainComponent::showChordMenu(){
    juce::PopupMenu menu;menu.addSectionHeader(juce::String::fromUTF8("Progressione · un accordo per battuta"));
    auto names=chordNames(project.settings);
    for(int slot=0;slot<project.settings.chordCount;++slot){juce::PopupMenu chord;juce::PopupMenu degrees;
        for(int d=0;d<7;++d)degrees.addItem(1000+slot*100+d,"Grado "+juce::String(d+1),true,project.settings.freeChords[(size_t)slot]<0&&project.settings.degrees[(size_t)slot]==d);
        chord.addSubMenu("Nella scala",degrees);
        for(int quality=0;quality<3;++quality){juce::PopupMenu roots;for(int root=0;root<12;++root){int code=(root-project.settings.key+12)%12+quality*12;roots.addItem(1000+slot*100+10+code,fgg::pitchClassName(root),true,project.settings.freeChords[(size_t)slot]==code);}chord.addSubMenu(quality==0?"Maggiore":quality==1?"Minore":"Dominante",roots);}
        menu.addSubMenu(juce::String(slot+1)+juce::String::fromUTF8(" · ")+juce::String::fromUTF8(names[(size_t)slot].c_str()),chord);
    }
    juce::PopupMenu lengths;for(int n:{1,2,3,4,6,8})lengths.addItem(2000+n,juce::String(n)+" accordi",true,n==project.settings.chordCount);menu.addSubMenu("Lunghezza progressione",lengths);
    menu.addItem(3000,"Nuova progressione casuale");
    juce::Component::SafePointer<MainComponent> safe(this);menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&chordEditButton),[safe](int id){if(safe==nullptr||id==0)return;safe->checkpoint();auto& s=safe->project.settings;
        if(id>=1000&&id<1800){int slot=(id-1000)/100,value=(id-1000)%100;if(value<7){s.degrees[(size_t)slot]=value;s.freeChords[(size_t)slot]=-1;}else if(value>=10&&value<46)s.freeChords[(size_t)slot]=value-10;}
        if(id>2000&&id<=2008)s.chordCount=id-2000;if(id==3000)randomiseHarmony(safe->project,(std::uint32_t)juce::Random::getSystemRandom().nextInt());safe->changed(false);safe->message("Armonia aggiornata: GENERATE per scriverla nelle tracce sbloccate.");
    });
}
void MainComponent::showTrackMenu(int track){
    juce::PopupMenu menu;const auto& t=project.tracks[(size_t)track];menu.addSectionHeader(u(t.name));menu.addItem(1,"Rinomina");menu.addItem(2,"Muto",true,t.mute);menu.addItem(3,"Solo",true,t.solo);menu.addItem(4,"Blocca generazione",true,t.locked);menu.addSeparator();menu.addItem(5,"Suoni originali Fase Groove",!loadingPlugin);menu.addItem(6,"Rigenera traccia");
    if(t.instrument=="sample"){juce::PopupMenu roots;for(int n=24;n<=96;++n)roots.addItem(100+n,juce::MidiMessage::getMidiNoteName(n,true,true,3),true,n==t.sampleRoot);menu.addSubMenu("Radice campione",roots);}
    juce::Component::SafePointer<MainComponent> safe(this);menu.showMenuAsync(juce::PopupMenu::Options(),[safe,track](int id){if(safe==nullptr||id==0)return;safe->select(track,safe->selectedSection);if(id==1){rename("Rinomina traccia",u(safe->project.tracks[(size_t)track].name),[safe,track](juce::String name){if(safe!=nullptr&&name.isNotEmpty()){safe->checkpoint();safe->project.tracks[(size_t)track].name=name.toStdString();safe->changed(false);}});return;}safe->checkpoint();auto& t=safe->project.tracks[(size_t)track];if(id==2)t.mute=!t.mute;if(id==3)t.solo=!t.solo;if(id==4)t.locked=!t.locked;if(id==5){safe->closePluginEditor();safe->audio.useInternal(track);t.instrument="internal";t.instrumentPath.clear();t.instrumentId.clear();t.instrumentState.clear();}if(id==6){safe->regenerate(Scope::track);return;}if(id>=124&&id<=196){int root=id-100;auto r=safe->audio.loadSample(track,juce::File(u(t.instrumentPath)),root);if(r.failed())safe->message(r.getErrorMessage(),true);else t.sampleRoot=root;}safe->changed(false);});
}
void MainComponent::showSectionMenu(int section){
    juce::PopupMenu menu;menu.addSectionHeader(u(project.sections[(size_t)section].name));menu.addItem(1,"Rinomina");menu.addItem(2,"Duplica con MIDI");menu.addItem(3,"Elimina",project.sections.size()>1);menu.addItem(4,"Sposta a sinistra",section>0);menu.addItem(5,"Sposta a destra",section+1<(int)project.sections.size());menu.addItem(6,"Rigenera sezione");menu.addItem(7,"Loop sezione");
    juce::PopupMenu bars,types;for(int n:{1,2,4,8,12,16,24,32,48,64,96,128})bars.addItem(400+n,juce::String(n)+" bars",true,n==project.sections[(size_t)section].bars);for(int type=0;type<6;++type)types.addItem(200+type,sectionName((Section)type),true,(int)project.sections[(size_t)section].type==type);menu.addSubMenu("Durata",bars);menu.addSubMenu("Funzione musicale",types);
    juce::Component::SafePointer<MainComponent> safe(this);menu.showMenuAsync(juce::PopupMenu::Options(),[safe,section](int id){if(safe==nullptr||id==0)return;safe->select(safe->selectedTrack,section);if(id==1){rename("Rinomina sezione",u(safe->project.sections[(size_t)section].name),[safe,section](juce::String name){if(safe!=nullptr&&name.isNotEmpty()){safe->checkpoint();safe->project.sections[(size_t)section].name=name.toStdString();safe->changed();}});return;}safe->checkpoint();if(id==2){safe->project.insertSection(section+1,safe->project.sections[(size_t)section],section);safe->selectedSection++;}if(id==3){safe->project.removeSection(section);safe->selectedSection=std::min(section,(int)safe->project.sections.size()-1);}if(id==4){safe->project.moveSection(section,section-1);safe->selectedSection--;}if(id==5){safe->project.moveSection(section,section+1);safe->selectedSection++;}if(id==6){safe->regenerate(Scope::section);return;}if(id==7){safe->sectionLoopButton.triggerClick();return;}if(id>=401&&id<=528){safe->project.sections[(size_t)section].bars=id-400;safe->project.normalise();}else if(id>=200&&id<206){safe->project.sections[(size_t)section].type=(Section)(id-200);safe->project.sections[(size_t)section].name=sectionName((Section)(id-200));}
        safe->project.loopEnd=std::min(safe->project.loopEnd,safe->project.length());safe->piano.clearSelection();safe->changed();
    });
}
void MainComponent::showClipMenu(int track,int section){
    juce::PopupMenu m;m.addItem(1,"Rigenera solo questo clip");m.addItem(2,"Blocca generazione",true,project.tracks[(size_t)track].clips[(size_t)section].locked);m.addItem(3,"Copia MIDI");m.addItem(4,"Incolla MIDI",!clipboard.empty());m.addItem(5,"Svuota clip");m.addItem(6,"Loop sezione");juce::Component::SafePointer<MainComponent> safe(this);
    m.showMenuAsync(juce::PopupMenu::Options(),[safe,track,section](int id){if(safe==nullptr||id==0)return;safe->select(track,section);if(id==1)safe->regenerate(Scope::clip);if(id==2){safe->checkpoint();safe->clip().locked=!safe->clip().locked;safe->changed(false);}if(id==3)safe->copyButton.triggerClick();if(id==4)safe->pasteButton.triggerClick();if(id==5){safe->checkpoint();safe->clip().notes.clear();safe->changed();}if(id==6)safe->sectionLoopButton.triggerClick();});
}
void MainComponent::showGuide(bool english){juce::DialogWindow::LaunchOptions o;o.dialogTitle=english?"Fase Groove Arranger / User guide":"Fase Groove Arranger / Guida";o.dialogBackgroundColour=Palette::background;o.useNativeTitleBar=true;o.resizable=true;o.content.setOwned(createGuidePage(english).release());o.launchAsync();}
bool MainComponent::writeHelpEvidence(const juce::File& folder){AboutPage about;auto image=about.createComponentSnapshot(about.getLocalBounds());bool ok=false;if(auto out=folder.getChildFile("credits.png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(image,*out);}return verifyGuidePages(folder)&&ok;}
void MainComponent::showDocument(const juce::String& title,const juce::String& text){juce::DialogWindow::LaunchOptions o;o.dialogTitle=title;o.dialogBackgroundColour=Palette::panel;o.useNativeTitleBar=true;o.resizable=true;o.content.setOwned(createReferencePage(title,text).release());o.launchAsync();}
void MainComponent::showAbout(){juce::PopupMenu m;m.addItem(1,juce::String::fromUTF8("Informazioni e crediti"));m.addItem(2,"Privacy e permessi");m.addItem(3,"Licenze e attribuzioni");m.addItem(4,"Guida italiana");m.addItem(5,"English guide");juce::Component::SafePointer<MainComponent> safe(this);m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&aboutButton),[safe](int id){if(safe==nullptr)return;if(id==1){juce::DialogWindow::LaunchOptions o;o.dialogTitle=juce::String::fromUTF8("Fase Groove · Ecosystem");o.dialogBackgroundColour=Palette::panel;o.useNativeTitleBar=true;o.resizable=false;o.content.setOwned(new AboutPage);o.launchAsync();}if(id==2)safe->showDocument("Privacy e permessi",juce::String::fromUTF8(BinaryData::PRIVACY_it_md,BinaryData::PRIVACY_it_mdSize));if(id==3)safe->showDocument("Licenze",juce::String::fromUTF8(BinaryData::THIRD_PARTY_NOTICES_md,BinaryData::THIRD_PARTY_NOTICES_mdSize));if(id==4)safe->helpButton.triggerClick();if(id==5)safe->showGuide(true);});}
void MainComponent::showAudioSettings(){
    juce::PopupMenu menu;menu.addItem(1,"Dispositivo audio / buffer");menu.addItem(3,"Apri cartella autosave");menu.addItem(4,"Recupera autosave");juce::Component::SafePointer<MainComponent> safe(this);menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&settingsButton),[safe](int id){if(safe==nullptr)return;if(id==1){juce::DialogWindow::LaunchOptions o;o.dialogTitle=juce::String::fromUTF8("Fase Groove · Audio");o.dialogBackgroundColour=Palette::panel;o.useNativeTitleBar=true;o.resizable=false;auto* selector=new juce::AudioDeviceSelectorComponent(safe->audio.devices,0,0,0,2,false,false,true,false);selector->setSize(680,500);o.content.setOwned(selector);o.launchAsync();}
        if(id==3||id==4){auto folder=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("FS Pitch/Fase Groove Arranger");folder.createDirectory();if(id==3)folder.revealToUser();else safe->open(folder.getChildFile("autosave.fga"));}
    });
}
void MainComponent::exportMenu(){
    if(loadingPlugin){message("Attendi il caricamento dei plugin.");return;}juce::PopupMenu m;m.addSectionHeader(juce::String::fromUTF8("MIDI · note modificabili"));m.addItem(1,"Canzone multitraccia (.mid)");m.addItem(2,"Traccia selezionata (.mid)");m.addItem(3,"Tutte le tracce MIDI separate");m.addSectionHeader(juce::String::fromUTF8("WAV · 24 bit stereo / frequenza dispositivo"));m.addItem(4,"Mix completo WAV");m.addItem(5,"Traccia selezionata WAV");m.addItem(6,"Tutti gli stem WAV");m.addSectionHeader(juce::String::fromUTF8("MP3 · 320 kbps / 44.1 kHz / stereo"));m.addItem(7,"Mix completo MP3");m.addItem(8,"Traccia selezionata MP3");m.addItem(9,"Tutti gli stem MP3");juce::Component::SafePointer<MainComponent> safe(this);m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&exportButton),[safe](int id){if(safe!=nullptr&&id>0)safe->exportFile(id,safe->selectedTrack);});
}
void MainComponent::exportFile(int type,int track){
    const bool batch=type==3||type==6||type==9;const bool mp3=type>=7;const bool midi=type<=3;const bool single=type==2||type==5||type==8;
    int flags=batch?juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories:juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting;
    chooseFile(batch?"Cartella esportazione (crea una sottocartella dedicata)":"Esporta "+juce::String(midi?"MIDI":mp3?"MP3":"WAV"),batch?"*":midi?"*.mid":mp3?"*.mp3":"*.wav",flags,[this,type,track,batch,mp3,midi,single](juce::File target){
        if(batch){target=target.getChildFile(juce::File::createLegalFileName(u(project.name))+"-stems-"+juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S"));if(target.createDirectory().failed()){message("Impossibile creare la cartella stem.",true);return;}}
        else target=target.withFileExtension(midi?"mid":mp3?"mp3":"wav");
        if(midi){auto write=[this](const juce::File& file,int ti){auto bytes=midiFile(project,ti);juce::TemporaryFile temp(file);if(!temp.getFile().replaceWithData(bytes.data(),bytes.size())||!temp.overwriteTargetFileWithTemporary())return false;return true;};bool ok=true;if(batch){for(int ti=0;ti<trackCount;++ti)ok=write(target.getChildFile(juce::String(rowForTrack(ti)+1).paddedLeft('0',2)+"-"+juce::File::createLegalFileName(u(project.tracks[(size_t)ti].name))+".mid"),ti)&&ok;}else ok=write(target,single?track:-1);message(ok?"MIDI esportato: "+target.getFullPathName():"Esportazione MIDI non completata.",!ok);return;}
        audio.stop();closePluginEditor();std::vector<int> tracks;if(batch)for(int row=0;row<trackCount;++row)tracks.push_back(trackAtRow(row));else tracks.push_back(single?track:-1);
        juce::Component::SafePointer<MainComponent> safe(this);exportTask=std::make_unique<ExportTask>(audio,project,target,tracks,mp3,[safe,target](juce::Result r){if(safe!=nullptr)safe->message(r.wasOk()?"Audio esportato: "+target.getFullPathName():r.getErrorMessage(),r.failed());});exportTask->launchThread();
    });
}
}
