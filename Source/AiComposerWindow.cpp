// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AiComposerWindow.h"
#include "AiComposer.h"
#include "MainComponent.h"
#include "UiText.h"
namespace fga {
namespace {
juce::String text(const char* en,const char* it){return Localisation::current()==Localisation::Language::italian?juce::String::fromUTF8(it):tr(juce::String::fromUTF8(en));}
class CompositionJob : public juce::Thread {
public:
    CompositionJob(Project p,AiOptions o,std::function<void(juce::Result,Project,juce::String)> done):Thread("AI composer"),base(std::move(p)),options(std::move(o)),complete(std::move(done)){}
    ~CompositionJob()override{signalThreadShouldExit();stopThread(5000);}
    void run()override{Project result;juce::String summary;auto status=aiGenerate(base,options,result,summary,[this]{return threadShouldExit();});if(threadShouldExit())return;juce::MessageManager::callAsync([callback=complete,status,result=std::move(result),summary]()mutable{callback(status,std::move(result),summary);});}
private:Project base;AiOptions options;std::function<void(juce::Result,Project,juce::String)> complete;
};
}
class AiComposerPanel : public juce::Component {
public:
    explicit AiComposerPanel(MainComponent& m):owner(m),generate(IconButton::Icon::sparkles),apply(IconButton::Icon::play){
        for(auto* c:std::initializer_list<juce::Component*>{&provider,&model,&key,&endpoint,&mode,&duration,&prompt,&preview,&status,&generate,&cancel,&apply,&listen,&keyHelp,&tracksPort,&progress})addAndMakeVisible(c);
        provider.addItemList({"OpenAI","Anthropic / Claude","Google / Gemini","Ollama"},1);
        mode.addItem(text("New song","Nuovo brano"),1);mode.addItem(text("Edit selected tracks","Modifica tracce selezionate"),2);mode.setSelectedId(1,juce::dontSendNotification);
        duration.addItem(text("From the prompt","Dal prompt"),1);for(int n:{2,3,4,5,6,8,10,15,20,30})duration.addItem(juce::String(n)+" min",duration.getNumItems()+1);duration.setSelectedId(1,juce::dontSendNotification);
        model.setEditableText(true);key.setPasswordCharacter(0x2022);key.setTextToShowWhenEmpty(text("API key · this session only","Chiave API · solo questa sessione"),Daw::textFaint);endpoint.setText(Localisation::setting("aiOllamaEndpoint","http://127.0.0.1:11434"));
        prompt.setMultiLine(true);prompt.setReturnKeyStartsNewLine(true);prompt.setScrollbarsShown(true);prompt.setTextToShowWhenEmpty(text("A 6-minute techno song at 138 BPM, DJ intro, two drops and an evolving lead...","Una canzone techno di 6 minuti a 138 BPM, intro da DJ, due drop e un lead che evolve..."),Daw::textFaint);
        preview.setMultiLine(true);preview.setReadOnly(true);preview.setScrollbarsShown(true);preview.setText(text("New song replaces the MIDI arrangement; selected-track editing keeps the other parts. Review your composition here before applying it. Undo is available.","Nuovo brano sostituisce l’arrangiamento MIDI; la modifica mirata conserva le altre parti. Controlla qui la composizione prima di applicarla. Annulla è disponibile."));
        for(auto* editor:{&key,&endpoint,&prompt,&preview}){editor->setFont(Daw::font(13));editor->setColour(juce::TextEditor::backgroundColourId,Daw::well);editor->setColour(juce::TextEditor::outlineColourId,Daw::line);editor->setColour(juce::TextEditor::focusedOutlineColourId,Daw::accent);editor->setColour(juce::TextEditor::textColourId,Daw::text);editor->setIndents(10,8);}
        generate.primary=true;generate.setButtonText(text("COMPOSE","COMPONI"));generate.setTooltip(text("Ask the selected model to write MIDI notes and song sections.","Chiedi al modello scelto di scrivere note MIDI e sezioni del brano."));
        apply.primary=true;apply.setButtonText(text("APPLY","APPLICA"));apply.onColour=Daw::play;apply.setTooltip(text("Insert the preview into the project as one undoable action.","Inserisci l’anteprima nel progetto con una sola operazione annullabile."));apply.setEnabled(false);
        cancel.setButtonText(tr("Cancel"));cancel.setEnabled(false);listen.setButtonText(text("Listen when applied","Ascolta dopo Applica"));listen.setToggleState(true,juce::dontSendNotification);
        keyHelp.setButtonText(text("API keys / help","Chiavi API / aiuto"));keyHelp.setTooltip(text("Open the selected service's setup page in your browser.","Apri nel browser la pagina di configurazione del servizio scelto."));
        progress.setPercentageDisplay(false);progress.setColour(juce::ProgressBar::backgroundColourId,Daw::well);progress.setColour(juce::ProgressBar::foregroundColourId,Daw::accent);progress.setVisible(false);
        status.setFont(Daw::font(11));status.setColour(juce::Label::textColourId,Daw::textDim);
        tracksPort.setViewedComponent(&tracksBody,false);tracksPort.setScrollBarsShown(true,false);
        provider.onChange=[this]{if(activeProvider>=0){keys[(size_t)activeProvider]=key.getText();Localisation::setSetting(("aiModel"+juce::String(activeProvider)).toRawUTF8(),model.getText());}activeProvider=provider.getSelectedItemIndex();model.clear(juce::dontSendNotification);const juce::StringArray suggestions=activeProvider==0?juce::StringArray{"gpt-4.1","gpt-4.1-mini"}:activeProvider==1?juce::StringArray{"claude-sonnet-4-5","claude-haiku-4-5"}:activeProvider==2?juce::StringArray{"gemini-2.5-flash","gemini-2.5-pro"}:juce::StringArray{"qwen2.5:14b","qwen3:14b","llama3.3:70b"};model.addItemList(suggestions,1);model.setText(Localisation::setting(("aiModel"+juce::String(activeProvider)).toRawUTF8(),suggestions[0]),juce::dontSendNotification);key.setText(keys[(size_t)activeProvider],false);Localisation::setSetting("aiProvider",activeProvider);resized();};
        mode.onChange=[this]{duration.setEnabled(mode.getSelectedId()==1);invalidatePreview();};
        for(auto* box:{&provider,&model,&duration}){box->setTooltip(text("Choose the service, model and requested song duration.","Scegli servizio, modello e durata richiesta."));}
        prompt.onTextChange=[this]{invalidatePreview();};
        generate.onClick=[this]{start();};cancel.onClick=[this]{cancelJob();};apply.onClick=[this]{if(!ready)return;const auto result=owner.applyAiComposition(base,composition,listen.getToggleState());status.setText(tr(result.wasOk()?text("Composition applied. Undo restores the previous song.","Composizione applicata. Annulla ripristina il brano precedente."):result.getErrorMessage()),juce::dontSendNotification);if(result.wasOk()){ready=false;apply.setEnabled(false);}};
        keyHelp.onClick=[this]{const juce::StringArray links{"https://platform.openai.com/api-keys","https://console.anthropic.com/settings/keys","https://aistudio.google.com/apikey","https://ollama.com/download"};juce::URL(links[provider.getSelectedItemIndex()]).launchInDefaultBrowser();};
        provider.setSelectedItemIndex(std::clamp(Localisation::setting("aiProvider","0").getIntValue(),0,3),juce::sendNotificationSync);setSize(960,800);refreshTracks();
    }
    ~AiComposerPanel()override{cancelJob();key.clear();for(auto& k:keys)k.clear();}
    void refreshTracks(){
        if(job)return;std::set<int> chosen;for(size_t i=0;i<tracks.size();++i)if(tracks[i]->getToggleState())chosen.insert((int)i);const bool initial=tracks.empty();tracks.clear();
        for(size_t i=0;i<owner.project.tracks.size();++i){auto button=std::make_unique<juce::ToggleButton>();const auto& t=owner.project.tracks[i];button->setButtonText(juce::String::fromUTF8(t.name.c_str()));button->setEnabled(!t.locked);button->setToggleState(!t.locked&&(initial?!isManualTrack((int)i):chosen.count((int)i)>0),juce::dontSendNotification);button->setTooltip(t.locked?text("Unlock this track in the inspector to let AI edit it.","Sblocca questa traccia nell’inspector per modificarla con l’IA."):text("Allow the composer to write MIDI for this instrument.","Consenti al compositore di scrivere MIDI per questo strumento."));button->onClick=[this]{invalidatePreview();};tracksBody.addAndMakeVisible(*button);tracks.push_back(std::move(button));}resized();
    }
    void cancelJob(){++requestId;if(job){job->signalThreadShouldExit();job.reset();}setBusy(false);status.setText(text("Generation cancelled. The project is unchanged.","Generazione annullata. Il progetto è invariato."),juce::dontSendNotification);}
    void paint(juce::Graphics& g)override{
        g.fillAll(Daw::base);const auto w=getWidth();g.setGradientFill(juce::ColourGradient(Daw::accent.withAlpha(.12f),0.f,0.f,Daw::base,(float)w,100.f,false));g.fillRect(0,0,w,95);
        IconButton::drawIcon(g,IconButton::Icon::sparkles,{26,25,32,32},Daw::accent);g.setFont(Daw::font(23,true));g.setColour(Daw::text);g.drawText(text("CREATE WITH AI","CREA CON IA"),72,19,w-96,32,juce::Justification::centredLeft);
        g.setFont(Daw::font(12));g.setColour(Daw::textDim);g.drawText(text("Your idea. Your instruments. Editable MIDI.","La tua idea. I tuoi strumenti. MIDI modificabile."),73,53,w-100,22,juce::Justification::centredLeft);
        auto label=[&](const juce::String& title,int x,int y,int width){g.setColour(Daw::textDim);g.setFont(Daw::font(11,true));g.drawText(title,x,y,width,18,juce::Justification::centredLeft);};
        const int left=24,right=leftWidth+44;label(text("SERVICE","SERVIZIO"),left,103,200);label(text("MODEL","MODELLO"),left+224,103,260);label(provider.getSelectedItemIndex()==3?text("OLLAMA SERVER","SERVER OLLAMA"):text("API KEY · SESSION ONLY","CHIAVE API · SOLO SESSIONE"),left,167,leftWidth);
        label(text("WRITE","SCRIVI"),left,231,230);label(text("DURATION","DURATA"),left+300,231,180);label(text("DESCRIBE THE SONG","DESCRIVI IL BRANO"),left,295,leftWidth);label(text("INSTRUMENTS","STRUMENTI"),left,467,leftWidth);
        auto card=juce::Rectangle<float>((float)right-10,101.f,(float)getWidth()-right-14,(float)getHeight()-208);g.setColour(Daw::surface);g.fillRoundedRectangle(card,8);label(text("COMPOSITION PREVIEW","ANTEPRIMA COMPOSIZIONE"),right,111,getWidth()-right-20);
        g.setColour(Daw::textFaint);g.setFont(Daw::font(10.5f));g.drawFittedText(text("Only your request, track names and musical context go to the selected service. Audio, files and plugin states stay here. Online API usage may incur charges. Keys are never saved.","Solo richiesta, nomi delle tracce e contesto musicale raggiungono il servizio scelto. Audio, file e stati dei plugin restano qui. Le API online possono avere costi a consumo. Le chiavi non vengono salvate."),24,getHeight()-44,getWidth()-48,36,juce::Justification::centredLeft,3);
    }
    void resized()override{
        leftWidth=std::min(548,getWidth()-350);const int left=24,right=leftWidth+44;
        progress.setBounds(left,87,getWidth()-48,3);
        provider.setBounds(left,125,210,32);model.setBounds(left+224,125,leftWidth-224,32);
        const bool local=provider.getSelectedItemIndex()==3;key.setVisible(!local);endpoint.setVisible(local);key.setBounds(left,189,leftWidth-150,32);endpoint.setBounds(key.getBounds());keyHelp.setBounds(left+leftWidth-140,189,140,32);
        mode.setBounds(left,253,286,32);duration.setBounds(left+300,253,leftWidth-300,32);
        prompt.setBounds(left,319,leftWidth,132);tracksPort.setBounds(left,491,leftWidth,std::max(72,getHeight()-605));
        tracksBody.setSize(leftWidth-14,((int)tracks.size()+3)/4*29);for(size_t i=0;i<tracks.size();++i)tracks[i]->setBounds((int)(i%4)*(leftWidth-14)/4,(int)(i/4)*29,(leftWidth-14)/4,27);
        preview.setBounds(right,141,getWidth()-right-24,std::max(170,getHeight()-260));
        status.setBounds(left,getHeight()-111,getWidth()-48,24);
        generate.setBounds(left,getHeight()-80,138,30);cancel.setBounds(left+148,getHeight()-80,96,30);listen.setBounds(right,getHeight()-80,180,30);apply.setBounds(getWidth()-144,getHeight()-80,120,30);repaint();
    }
    bool verify(const juce::File& folder){
        bool ok=generate.isVisible()&&generate.getTooltip().isNotEmpty()&&!apply.isEnabled()&&key.getPasswordCharacter()!=0;
        const auto old=provider.getSelectedItemIndex();provider.setSelectedItemIndex(3,juce::sendNotificationSync);ok=ok&&endpoint.isVisible()&&!key.isVisible();provider.setSelectedItemIndex(old,juce::sendNotificationSync);
        for(auto* c:getChildren())if(c->isVisible())ok=ok&&getLocalBounds().contains(c->getBounds());
        if(auto stream=folder.getChildFile("ai-composer.png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(createComponentSnapshot(getLocalBounds()),*stream)&&ok;}else ok=false;
        return ok;
    }
private:
    MainComponent& owner;double progressValue=-1;juce::ProgressBar progress{progressValue};juce::ComboBox provider,model,mode,duration;juce::TextEditor key,endpoint,prompt,preview;juce::Label status;IconButton generate,apply;juce::TextButton cancel,keyHelp;juce::ToggleButton listen;juce::Viewport tracksPort;juce::Component tracksBody;std::vector<std::unique_ptr<juce::ToggleButton>> tracks;std::array<juce::String,4> keys;int activeProvider=-1,leftWidth=548;bool ready=false;int requestId=0;Project base,composition;std::unique_ptr<CompositionJob> job;
    void invalidatePreview(){if(job)return;ready=false;apply.setEnabled(false);}
    void setBusy(bool busy){progress.setVisible(busy);for(juce::Component* c:std::initializer_list<juce::Component*>{&provider,&model,&mode,&duration,&key,&endpoint,&prompt,&tracksPort,&generate,&keyHelp})c->setEnabled(!busy);duration.setEnabled(!busy&&mode.getSelectedId()==1);cancel.setEnabled(busy);apply.setEnabled(!busy&&ready);}
    void start(){
        if(job)return;auto captured=owner.captureAiProject(base);if(captured.failed()){status.setText(tr(captured.getErrorMessage()),juce::dontSendNotification);return;}
        AiOptions options;options.provider=(AiProvider)provider.getSelectedItemIndex();options.model=model.getText().trim();options.key=key.getText().trim();options.endpoint=endpoint.getText().trim();options.prompt=prompt.getText().trim();options.wholeSong=mode.getSelectedId()==1;options.seconds=duration.getSelectedId()==1?0:duration.getText().getDoubleValue()*60;
        for(size_t i=0;i<tracks.size();++i)if(tracks[i]->getToggleState()&&tracks[i]->isEnabled())options.tracks.push_back((int)i);
        AiHttpRequest request;auto validation=aiHttpRequest(base,options,request);if(validation.failed()){status.setText(tr(validation.getErrorMessage()),juce::dontSendNotification);return;}
        Localisation::setSetting(("aiModel"+juce::String(provider.getSelectedItemIndex())).toRawUTF8(),options.model);Localisation::setSetting("aiOllamaEndpoint",options.endpoint);
        ready=false;setBusy(true);status.setText(text("Composing MIDI and arrangement… you can cancel at any time.","Composizione MIDI e arrangiamento… puoi annullare in qualsiasi momento."),juce::dontSendNotification);preview.setText(text("The model is writing your composition. Nothing has changed in the project yet.","Il modello sta scrivendo la composizione. Il progetto non è ancora stato modificato."));
        const int id=++requestId;juce::Component::SafePointer<AiComposerPanel> safe(this);job=std::make_unique<CompositionJob>(base,std::move(options),[safe,id](juce::Result result,Project p,juce::String summary){if(!safe||id!=safe->requestId)return;safe->job.reset();safe->ready=result.wasOk();if(result.wasOk()){safe->composition=std::move(p);juce::String report=summary+"\n\n";for(const auto& section:safe->composition.sections)report+=juce::String::fromUTF8(section.name.c_str())+juce::String::fromUTF8("  ·  ")+juce::String(section.bars)+" "+tr("bars")+"\n";report+="\n";for(const auto& track:safe->composition.tracks){size_t count=0;for(const auto& c:track.clips)count+=c.notes.size();if(count)report+=juce::String::fromUTF8(track.name.c_str())+juce::String::fromUTF8("  ·  ")+juce::String((int)count)+" MIDI\n";}safe->preview.setText(report);safe->status.setText(text("Ready. Review the composition, then apply it.","Pronta. Controlla la composizione, poi applicala."),juce::dontSendNotification);}else{safe->preview.setText(tr(result.getErrorMessage()));safe->status.setText(text("Generation failed. Your project is unchanged.","Generazione non riuscita. Il progetto è invariato."),juce::dontSendNotification);}safe->setBusy(false);});job->startThread();
    }
};
AiComposerWindow::AiComposerWindow(MainComponent& owner):DocumentWindow("Fase Groove Arranger / "+text("Create with AI","Crea con IA"),Daw::base,DocumentWindow::closeButton),panel(std::make_unique<AiComposerPanel>(owner)){
    setUsingNativeTitleBar(true);setAlwaysOnTop(true);setContentNonOwned(panel.get(),true);setResizable(true,false);setResizeLimits(920,700,1200,1000);const auto* display=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();centreWithSize(960,display?std::clamp(display->userArea.getHeight()-72,700,800):800);
}
AiComposerWindow::~AiComposerWindow(){clearContentComponent();panel.reset();}
void AiComposerWindow::closeButtonPressed(){panel->cancelJob();setVisible(false);}
void AiComposerWindow::show(){panel->refreshTracks();if(!isVisible())centreWithSize(getWidth(),getHeight());setVisible(true);toFront(true);}
bool AiComposerWindow::verify(const juce::File& f){return panel->verify(f)&&isAlwaysOnTop();}
}
