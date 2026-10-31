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
    ~CompositionJob()override{signalThreadShouldExit();stopThread(-1);}
    void run()override{Project result;juce::String summary;auto status=aiGenerate(base,options,result,summary,[this]{return threadShouldExit();});if(threadShouldExit())return;juce::MessageManager::callAsync([callback=complete,status,result=std::move(result),summary]()mutable{callback(status,std::move(result),summary);});}
private:Project base;AiOptions options;std::function<void(juce::Result,Project,juce::String)> complete;
};
class ModelListJob : public juce::Thread {
public:
    ModelListJob(AiOptions settings,std::function<void(juce::Result,juce::StringArray)> done):Thread("AI models"),options(std::move(settings)),complete(std::move(done)){}
    ~ModelListJob()override{signalThreadShouldExit();stopThread(-1);}
    void run()override{juce::StringArray models;auto result=aiModels(options,models,[this]{return threadShouldExit();});if(threadShouldExit())return;juce::MessageManager::callAsync([callback=complete,result,models]{callback(result,models);});}
private:AiOptions options;std::function<void(juce::Result,juce::StringArray)> complete;
};
}
class AiComposerPanel : public juce::Component {
public:
    explicit AiComposerPanel(MainComponent& m):owner(m),generate(IconButton::Icon::sparkles),apply(IconButton::Icon::play){
        for(auto* c:std::initializer_list<juce::Component*>{&provider,&model,&key,&endpoint,&mode,&duration,&prompt,&preview,&status,&generate,&cancel,&apply,&listen,&keyHelp,&checkServer,&guideHelp,&tracksPort,&progress})addAndMakeVisible(c);
        provider.addItemList({"OpenAI","Anthropic / Claude",text("Gemini · free tier","Gemini · quota gratuita"),text("Ollama · local, no API key","Ollama · locale, senza API"),"DeepSeek",text("Groq · free tier","Groq · quota gratuita"),"OpenRouter Free"},1);
        mode.addItem(text("New song","Nuovo brano"),1);mode.addItem(text("Edit selected tracks","Modifica tracce selezionate"),2);mode.setSelectedId(1,juce::dontSendNotification);
        duration.addItem(text("From the prompt","Dal prompt"),1);for(int n:{2,3,4,5,6,8,10,15,20,30})duration.addItem(juce::String(n)+" min",duration.getNumItems()+1);duration.setSelectedId(1,juce::dontSendNotification);
        model.setEditableText(true);key.setPasswordCharacter(0x2022);key.setTextToShowWhenEmpty(text("API key · this session only","Chiave API · solo questa sessione"),Daw::textFaint);endpoint.setText(Localisation::setting("aiOllamaEndpoint","http://127.0.0.1:11434"));
        prompt.setMultiLine(true);prompt.setReturnKeyStartsNewLine(true);prompt.setScrollbarsShown(true);prompt.setTextToShowWhenEmpty(text("A 6-minute techno song at 138 BPM, DJ intro, two drops and an evolving lead...","Una canzone techno di 6 minuti a 138 BPM, intro da DJ, due drop e un lead che evolve..."),Daw::textFaint);
        preview.setMultiLine(true);preview.setReadOnly(true);preview.setScrollbarsShown(true);preview.setText(text("New song replaces the MIDI arrangement; selected-track editing keeps the other parts. Review your composition here before applying it. Undo is available.","Nuovo brano sostituisce l’arrangiamento MIDI; la modifica mirata conserva le altre parti. Controlla qui la composizione prima di applicarla. Annulla è disponibile."));
        for(auto* editor:{&key,&endpoint,&prompt,&preview}){editor->setFont(Daw::font(Daw::body));editor->setColour(juce::TextEditor::backgroundColourId,Daw::well);editor->setColour(juce::TextEditor::outlineColourId,Daw::line);editor->setColour(juce::TextEditor::focusedOutlineColourId,Daw::accent);editor->setColour(juce::TextEditor::textColourId,Daw::text);editor->setIndents(10,8);}
        generate.primary=true;generate.setButtonText(text("COMPOSE","COMPONI"));generate.setTooltip(text("Ask the selected model to write MIDI notes and song sections.","Chiedi al modello scelto di scrivere note MIDI e sezioni del brano."));
        apply.primary=true;apply.setButtonText(text("APPLY","APPLICA"));apply.onColour=Daw::play;apply.setTooltip(text("Insert the preview into the project as one undoable action.","Inserisci l’anteprima nel progetto con una sola operazione annullabile."));apply.setEnabled(false);
        cancel.setButtonText(tr("Cancel"));cancel.setEnabled(false);listen.setButtonText(text("Listen when applied","Ascolta dopo Applica"));listen.setToggleState(true,juce::dontSendNotification);
        keyHelp.setButtonText(text("API keys / help","Chiavi API / aiuto"));keyHelp.setTooltip(text("Open the selected service's setup page in your browser.","Apri nel browser la pagina di configurazione del servizio scelto."));
        progress.setPercentageDisplay(false);progress.setColour(juce::ProgressBar::backgroundColourId,Daw::well);progress.setColour(juce::ProgressBar::foregroundColourId,Daw::accent);progress.setVisible(false);
        status.setFont(Daw::font(Daw::small));status.setColour(juce::Label::textColourId,Daw::textDim);
        tracksPort.setViewedComponent(&tracksBody,false);tracksPort.setScrollBarsShown(true,false);
        provider.onChange=[this]{
            ++modelRequestId;owner.retiredJobs.retire(std::move(modelsJob));modelsReady=false;setBusy(false);invalidatePreview();
            if(activeProvider>=0){keys[(size_t)activeProvider]=key.getText();if(model.getText().isNotEmpty())Localisation::setSetting(("aiModel"+juce::String(activeProvider)).toRawUTF8(),model.getText());}
            activeProvider=provider.getSelectedItemIndex();model.clear(juce::dontSendNotification);model.setEditableText(activeProvider!=3);model.setTextWhenNothingSelected(activeProvider==3?text("No local model detected","Nessun modello locale rilevato"):text("Choose or enter a model","Scegli o scrivi un modello"));
            const auto suggestions=aiSuggestedModels((AiProvider)activeProvider);
            model.addItemList(suggestions,1);auto chosen=Localisation::setting(("aiModel"+juce::String(activeProvider)).toRawUTF8(),"").trim();if(chosen.isEmpty()&&!suggestions.isEmpty())chosen=suggestions[0];model.setText(chosen,juce::dontSendNotification);
            key.setText(keys[(size_t)activeProvider],false);Localisation::setSetting("aiProvider",activeProvider);preview.setText(setupHelp());status.setText(text("Choose a model. REFRESH reads the service catalogue; availability and quotas depend on your account.","Scegli un modello. AGGIORNA legge il catalogo del servizio; accesso e quote dipendono dal tuo account."),juce::dontSendNotification);setBusy(false);resized();
            if(activeProvider==3)refreshModels();
        };
        model.onChange=[this]{invalidatePreview();};duration.onChange=[this]{invalidatePreview();};
        key.onTextChange=[this]{invalidatePreview();};
        endpoint.onTextChange=[this]{modelsReady=false;invalidatePreview();generate.setEnabled(false);};
        endpoint.onReturnKey=[this]{refreshModels();};
        checkServer.setButtonText(text("CHECK","VERIFICA"));checkServer.setTooltip(text("Read the selected service’s current text models. Cloud services need their API key; Ollama lists installed local models. This does not generate a song.","Leggi i modelli di testo aggiornati del servizio scelto. Il cloud richiede la chiave API; Ollama elenca quelli locali installati. Non genera un brano."));checkServer.onClick=[this]{refreshModels();};
        mode.onChange=[this]{duration.setEnabled(mode.getSelectedId()==1);invalidatePreview();};
        for(auto* box:{&provider,&model,&duration}){box->setTooltip(text("Choose the service, model and requested song duration.","Scegli servizio, modello e durata richiesta."));}
        prompt.onTextChange=[this]{invalidatePreview();};
        generate.onClick=[this]{start();};cancel.onClick=[this]{cancelJob();};apply.onClick=[this]{if(!ready)return;const auto result=owner.applyAiComposition(base,composition,listen.getToggleState());status.setText(tr(result.wasOk()?text("Composition applied. Undo restores the previous song.","Composizione applicata. Annulla ripristina il brano precedente."):result.getErrorMessage()),juce::dontSendNotification);if(result.wasOk()){ready=false;apply.setEnabled(false);}};
        keyHelp.onClick=[this]{invalidatePreview();preview.setText(setupHelp());juce::URL(aiSetupUrl((AiProvider)activeProvider)).launchInDefaultBrowser();};
        guideHelp.setButtonText(text("SETUP GUIDE","GUIDA IA"));guideHelp.setTooltip(text("Open the manual: AI services, free API quotas, setup and third-party credits.","Apri la guida: servizi IA, quote API gratuite, configurazione e crediti di terzi."));guideHelp.onClick=[this]{owner.showGuide(Localisation::current()!=Localisation::Language::italian);};
        // Start with local AI once when upgrading; subsequent explicit service choices are retained.
        const bool firstLocalSetup=Localisation::setting("aiLocalSetup","0")=="0";
        Localisation::setSetting("aiLocalSetup",1);
        provider.setSelectedItemIndex(firstLocalSetup?3:std::clamp(Localisation::setting("aiProvider","3").getIntValue(),0,(int)AiProvider::count-1),juce::sendNotificationSync);setSize(960,800);refreshTracks();
    }
    ~AiComposerPanel()override{cancelJob();key.clear();for(auto& k:keys)k.clear();}
    void refreshTracks(){
        if(job)return;std::set<std::uint64_t> chosen;for(size_t i=0;i<tracks.size();++i)if(tracks[i]->getToggleState())chosen.insert(trackIdentities[i]);const bool initial=tracks.empty();tracks.clear();trackIdentities.clear();
        for(size_t i=0;i<owner.project.tracks.size();++i){auto button=std::make_unique<juce::ToggleButton>();const auto& t=owner.project.tracks[i];button->setButtonText(juce::String::fromUTF8(t.name.c_str()));button->setEnabled(!t.locked);button->setToggleState(!t.locked&&(initial?!isManualTrack((int)i):chosen.count(t.sessionId)>0),juce::dontSendNotification);button->setTooltip(t.locked?text("Unlock this track in the inspector to let AI edit it.","Sblocca questa traccia nell’inspector per modificarla con l’IA."):text("Allow the composer to write MIDI for this instrument.","Consenti al compositore di scrivere MIDI per questo strumento."));button->onClick=[this]{invalidatePreview();};tracksBody.addAndMakeVisible(*button);tracks.push_back(std::move(button));trackIdentities.push_back(t.sessionId);}resized();
    }
    void cancelJob(){++requestId;++modelRequestId;owner.retiredJobs.retire(std::move(modelsJob));if(job){job->signalThreadShouldExit();owner.retiredJobs.retire(std::move(job));}setBusy(false);status.setText(text("Generation cancelled. The project is unchanged.","Generazione annullata. Il progetto è invariato."),juce::dontSendNotification);}
    void paint(juce::Graphics& g)override{
        g.fillAll(Daw::base);const auto w=getWidth();g.setGradientFill(juce::ColourGradient(Daw::accent.withAlpha(.12f),0.f,0.f,Daw::base,(float)w,100.f,false));g.fillRect(0,0,w,95);
        IconButton::drawIcon(g,IconButton::Icon::sparkles,{26,25,32,32},Daw::accent);g.setFont(Daw::font(Daw::display,true));g.setColour(Daw::text);g.drawText(text("CREATE WITH AI","CREA CON IA"),72,19,w-96,32,juce::Justification::centredLeft);
        g.setFont(Daw::font(Daw::body));g.setColour(Daw::textDim);g.drawText(text("Your idea. Your instruments. Editable MIDI.","La tua idea. I tuoi strumenti. MIDI modificabile."),73,53,w-100,22,juce::Justification::centredLeft);
        auto label=[&](const juce::String& title,int x,int y,int width){g.setColour(Daw::textDim);g.setFont(Daw::font(Daw::small,true));g.drawText(title,x,y,width,18,juce::Justification::centredLeft);};
        const int left=24,right=leftWidth+44;label(text("SERVICE","SERVIZIO"),left,103,200);label(text("MODEL","MODELLO"),left+274,103,260);label(provider.getSelectedItemIndex()==3?text("OLLAMA SERVER","SERVER OLLAMA"):text("API KEY · SESSION ONLY","CHIAVE API · SOLO SESSIONE"),left,167,leftWidth);
        label(text("WRITE","SCRIVI"),left,231,230);label(text("DURATION","DURATA"),left+300,231,180);label(text("DESCRIBE THE SONG","DESCRIVI IL BRANO"),left,295,leftWidth);label(text("INSTRUMENTS","STRUMENTI"),left,467,leftWidth);
        auto card=juce::Rectangle<float>((float)right-10,101.f,(float)getWidth()-right-14,(float)getHeight()-208);g.setColour(Daw::surface);g.fillRoundedRectangle(card,8);label(text("COMPOSITION PREVIEW","ANTEPRIMA COMPOSIZIONE"),right,111,getWidth()-right-20);
        g.setColour(Daw::textFaint);g.setFont(Daw::font(Daw::small));g.drawFittedText(text("Only your request, track names and musical context go to the selected service. Audio, files and plugin states stay here. Online API usage may incur charges. Keys are never saved.","Solo richiesta, nomi delle tracce e contesto musicale raggiungono il servizio scelto. Audio, file e stati dei plugin restano qui. Le API online possono avere costi a consumo. Le chiavi non vengono salvate."),24,getHeight()-44,getWidth()-48,36,juce::Justification::centredLeft,3);
    }
    void resized()override{
        leftWidth=std::min(548,getWidth()-350);const int left=24,right=leftWidth+44;
        progress.setBounds(left,87,getWidth()-48,3);guideHelp.setBounds(getWidth()-156,29,132,30);
        provider.setBounds(left,125,260,32);model.setBounds(left+274,125,leftWidth-274,32);
        const bool local=provider.getSelectedItemIndex()==3;key.setVisible(!local);endpoint.setVisible(local);checkServer.setVisible(true);checkServer.setButtonText(local?text("CHECK","VERIFICA"):text("REFRESH","AGGIORNA"));
        key.setBounds(left,189,leftWidth-268,32);endpoint.setBounds(left,189,leftWidth-268,32);checkServer.setBounds(left+leftWidth-258,189,108,32);keyHelp.setBounds(left+leftWidth-140,189,140,32);
        keyHelp.setButtonText(local?text("INSTALL / HELP","INSTALLA / AIUTO"):text("API keys / help","Chiavi API / aiuto"));
        mode.setBounds(left,253,286,32);duration.setBounds(left+300,253,leftWidth-300,32);
        prompt.setBounds(left,319,leftWidth,132);tracksPort.setBounds(left,491,leftWidth,std::max(72,getHeight()-605));
        tracksBody.setSize(leftWidth-14,((int)tracks.size()+3)/4*29);for(size_t i=0;i<tracks.size();++i)tracks[i]->setBounds((int)(i%4)*(leftWidth-14)/4,(int)(i/4)*29,(leftWidth-14)/4,27);
        preview.setBounds(right,141,getWidth()-right-24,std::max(170,getHeight()-260));
        status.setBounds(left,getHeight()-111,getWidth()-48,24);
        generate.setBounds(left,getHeight()-80,138,30);cancel.setBounds(left+148,getHeight()-80,96,30);listen.setBounds(right,getHeight()-80,180,30);apply.setBounds(getWidth()-144,getHeight()-80,120,30);repaint();
    }
    bool verify(const juce::File& folder){
        bool ok=tracks.size()==owner.project.tracks.size()&&generate.isVisible()&&generate.getTooltip().isNotEmpty()&&!apply.isEnabled()&&key.getPasswordCharacter()!=0;
        {
            const auto saved=owner.project;std::set<std::uint64_t> selected;
            for(size_t i=0;i<tracks.size();++i)if(tracks[i]->getToggleState())selected.insert(trackIdentities[i]);
            const int first=owner.project.addMidiTrack(),last=owner.project.addMidiTrack();
            if(first>=0&&last>=0){const auto identity=owner.project.tracks[(size_t)last].sessionId;refreshTracks();for(auto& button:tracks)button->setToggleState(false,juce::dontSendNotification);tracks[(size_t)last]->setToggleState(true,juce::dontSendNotification);owner.project.deleteMidiTrack(first);refreshTracks();
                const int moved=last-1;ok=ok&&trackIdentities[(size_t)moved]==identity&&tracks[(size_t)moved]->getToggleState();owner.project.tracks[(size_t)moved].locked=true;refreshTracks();ok=ok&&!tracks[(size_t)moved]->isEnabled()&&!tracks[(size_t)moved]->getToggleState();
            }else ok=false;
            owner.project=saved;refreshTracks();for(size_t i=0;i<tracks.size();++i)tracks[i]->setToggleState(selected.count(trackIdentities[i])>0,juce::dontSendNotification);
        }
        const auto old=provider.getSelectedItemIndex();provider.setSelectedItemIndex(3,juce::sendNotificationSync);ok=ok&&endpoint.isVisible()&&!key.isVisible();provider.setSelectedItemIndex(old,juce::sendNotificationSync);
        // Optional loopback fixture is used only by this native verification entry point.
        const auto fixture=juce::SystemStats::getEnvironmentVariable("FGA_AI_TEST_SERVER",{});
        if(fixture.isNotEmpty()){
            const auto savedEndpoint=endpoint.getText(),savedModel=Localisation::setting("aiModel3","");
            cancelJob();provider.setSelectedItemIndex(3,juce::sendNotificationSync);endpoint.setText(fixture,false);refreshModels();
            auto wait=[this]{const auto end=juce::Time::getMillisecondCounterHiRes()+5000;while((modelsJob||job)&&juce::Time::getMillisecondCounterHiRes()<end)juce::MessageManager::getInstance()->runDispatchLoopUntil(10);};
            wait();bool localOk=!modelsJob&&modelsReady&&model.getNumItems()==2&&!key.isVisible()&&checkServer.isVisible()&&generate.isEnabled();
            model.setText("qwen3:8b",juce::dontSendNotification);prompt.setText("techno 6 minutes",false);mode.setSelectedId(1,juce::sendNotificationSync);duration.setSelectedId(1,juce::sendNotificationSync);
            generate.onClick();wait();localOk=localOk&&!job&&ready&&apply.isEnabled()&&composition.settings.bpm==138&&std::abs(composition.length()*60/138-360)<.01;
            duration.setSelectedId(2,juce::sendNotificationSync);localOk=localOk&&!ready&&!apply.isEnabled();duration.setSelectedId(1,juce::sendNotificationSync);
            folder.getChildFile("ai-local-http.txt").replaceWithText(localOk?"PASS native model discovery, no API key field, installed-model selection, Compose callback, six-minute MIDI preview and stale-preview invalidation (HTTP fixture, no inference)":"FAIL native local AI flow");
            ok=localOk&&ok;cancelJob();endpoint.setText(savedEndpoint,false);provider.setSelectedItemIndex(old,juce::sendNotificationSync);Localisation::setSetting("aiOllamaEndpoint",savedEndpoint);Localisation::setSetting("aiModel3",savedModel);
            preview.setText(localHelp());
        }
        {
            const auto savedKeys=keys;const auto savedKey=key.getText();
            std::array<juce::String,(size_t)AiProvider::count> savedModels;
            for(size_t i=0;i<savedModels.size();++i)savedModels[i]=Localisation::setting(("aiModel"+juce::String((int)i)).toRawUTF8(),"");
            cancelJob();
            const auto originalSize=getBounds();prompt.setText(text("A six-minute techno song at 138 BPM, DJ intro, two drops and an evolving lead.","Una canzone techno di 6 minuti a 138 BPM, intro da DJ, due drop e un lead che evolve."),false);
            for(int index:{0,1,2,4,5,6}){
                provider.setSelectedItemIndex(index,juce::sendNotificationSync);key.clear();
                const auto selected=model.getText();refreshModels();
                ok=ok&&provider.getNumItems()==(int)AiProvider::count&&model.isTextEditable()&&model.getNumItems()>0&&model.getText().isNotEmpty()&&key.isVisible()&&!endpoint.isVisible()&&checkServer.isVisible()&&guideHelp.isVisible()&&!modelsJob&&model.getText()==selected&&generate.isEnabled();
                for(const auto size:{juce::Point<int>(920,700),juce::Point<int>(960,800)}){setSize(size.x,size.y);for(auto* child:getChildren())if(child->isVisible())ok=ok&&getLocalBounds().contains(child->getBounds());}
                if(auto stream=folder.getChildFile("ai-service-"+juce::String(index)+".png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(createComponentSnapshot(getLocalBounds()),*stream)&&ok;}else ok=false;
            }
            guideHelp.onClick();
            if(auto* dialog=dynamic_cast<juce::DialogWindow*>(juce::Component::getCurrentlyModalComponent())){ok=ok&&dialog->isVisible()&&dialog->isAlwaysOnTop();dialog->closeButtonPressed();}else ok=false;
            provider.setSelectedItemIndex(4,juce::sendNotificationSync);key.setText("fixture-deepseek-session",false);
            provider.setSelectedItemIndex(5,juce::sendNotificationSync);ok=ok&&key.getText().isEmpty();key.setText("fixture-groq-session",false);
            provider.setSelectedItemIndex(4,juce::sendNotificationSync);ok=ok&&key.getText()=="fixture-deepseek-session";
            provider.setSelectedItemIndex(old,juce::sendNotificationSync);keys=savedKeys;key.setText(savedKey,false);setBounds(originalSize);
            for(size_t i=0;i<savedModels.size();++i)Localisation::setSetting(("aiModel"+juce::String((int)i)).toRawUTF8(),savedModels[i]);
            folder.getChildFile("ai-providers.txt").replaceWithText(ok?"PASS seven service IDs, current model presets, editable cloud IDs, no-key catalogue errors, session-key isolation, model preservation and layout at 920x700 / 960x800 (no paid calls)":"FAIL AI provider UI");
        }
        if(activeProvider==3){const auto end=juce::Time::getMillisecondCounterHiRes()+9000;while(modelsJob&&juce::Time::getMillisecondCounterHiRes()<end)juce::MessageManager::getInstance()->runDispatchLoopUntil(10);}
        for(auto* c:getChildren())if(c->isVisible())ok=ok&&getLocalBounds().contains(c->getBounds());
        if(auto stream=folder.getChildFile("ai-composer.png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(createComponentSnapshot(getLocalBounds()),*stream)&&ok;}else ok=false;
        return ok;
    }
private:
    MainComponent& owner;double progressValue=-1;juce::ProgressBar progress{progressValue};juce::ComboBox provider,model,mode,duration;juce::TextEditor key,endpoint,prompt,preview;juce::Label status;IconButton generate,apply;juce::TextButton cancel,keyHelp,checkServer,guideHelp;juce::ToggleButton listen;juce::Viewport tracksPort;juce::Component tracksBody;std::vector<std::unique_ptr<juce::ToggleButton>> tracks;std::vector<std::uint64_t> trackIdentities;std::array<juce::String,(size_t)AiProvider::count> keys;int activeProvider=-1,leftWidth=548;bool ready=false,modelsReady=false;int requestId=0,modelRequestId=0;Project base,composition;std::unique_ptr<CompositionJob> job;std::unique_ptr<ModelListJob> modelsJob;
    void invalidatePreview(){if(job||modelsJob)return;ready=false;apply.setEnabled(false);}
    void setBusy(bool busy){progress.setVisible(busy);for(juce::Component* c:std::initializer_list<juce::Component*>{&provider,&model,&mode,&duration,&key,&endpoint,&prompt,&tracksPort,&generate,&keyHelp,&checkServer,&guideHelp})c->setEnabled(!busy);duration.setEnabled(!busy&&mode.getSelectedId()==1);cancel.setEnabled(busy);model.setEnabled(!busy&&(activeProvider!=3||modelsReady));generate.setEnabled(!busy&&(activeProvider!=3||modelsReady));apply.setEnabled(!busy&&ready);}
    juce::String localHelp()const{
        return text("LOCAL AI · NO API KEY\n\n1. Install Ollama from ollama.com and open it.\n2. Download a text model in Ollama. For example, run:\n   ollama pull qwen2.5:7b\n   (about 4.7 GB; allow sufficient RAM and disk space).\n3. Leave the server at http://127.0.0.1:11434 and press CHECK. Installed local models appear in MODEL.\n4. Describe your song, then COMPOSE and APPLY.\n\nOllama must be running on the same computer. If you use a different machine, enter its address. The first generation may take several minutes while the model loads. Smaller models are faster but may return invalid compositions. No subscription or API key is required for local models.",
                    "IA LOCALE · SENZA CHIAVE API\n\n1. Installa Ollama da ollama.com e aprilo.\n2. Scarica un modello di testo in Ollama. Ad esempio esegui:\n   ollama pull qwen2.5:7b\n   (circa 4,7 GB; servono RAM e spazio su disco sufficienti).\n3. Lascia il server su http://127.0.0.1:11434 e premi VERIFICA. I modelli locali installati compaiono in MODELLO.\n4. Descrivi il brano, poi COMPONI e APPLICA.\n\nOllama deve essere aperto sullo stesso computer. Se usi un altro computer, indica il suo indirizzo. La prima generazione può richiedere alcuni minuti per caricare il modello. I modelli piccoli sono più veloci ma possono produrre composizioni non valide. I modelli locali non richiedono abbonamenti né chiavi API.");
    }
    juce::String setupHelp()const{
        if(activeProvider==3)return localHelp();
        juce::String detail;
        switch((AiProvider)activeProvider){
            case AiProvider::gemini:detail=text("Gemini: create a Google AI Studio key. Some models have a free tier, subject to region/account quotas. Check the selected model's pricing before enabling billing; Gemini 4 Argon is paid. Free-tier data policies differ from paid use.","Gemini: crea una chiave in Google AI Studio. Alcuni modelli hanno quote gratuite, soggette a paese e account. Controlla il prezzo del modello prima di attivare la fatturazione; Gemini 4 Argon è a pagamento. Le regole sui dati del piano gratuito differiscono da quelle a pagamento.");break;
            case AiProvider::groq:detail=text("Groq: create a GroqCloud account and API key. The Free plan has model-specific token and request limits; start with openai/gpt-oss-20b. A complex full song can exceed the quota: enable fewer instruments or try again later. Do not upgrade billing to stay on the Free plan.","Groq: crea un account GroqCloud e una chiave API. Il piano Free ha limiti di token e richieste per modello; inizia con openai/gpt-oss-20b. Un brano complesso può superare la quota: abilita meno strumenti o riprova più tardi. Per restare nel piano Free non attivare un piano a pagamento.");break;
            case AiProvider::openrouter:detail=text("OpenRouter Free: create an OpenRouter API key. openrouter/free routes only to free models. REFRESH lists free text variants (:free); paid model IDs are rejected. Availability and daily quotas vary. Your request also reaches the selected upstream provider, whose data policies apply.","OpenRouter Free: crea una chiave API OpenRouter. openrouter/free usa solo modelli gratuiti. AGGIORNA elenca le varianti di testo gratuite (:free); gli ID a pagamento vengono rifiutati. Disponibilità e quote giornaliere variano. La richiesta raggiunge anche il fornitore del modello scelto, con le sue regole sui dati.");break;
            case AiProvider::deepseek:detail=text("DeepSeek: create a key on the DeepSeek API platform and check the API balance/pricing. deepseek-flash and deepseek-v4-pro are usage-billed APIs. Free access to the chat app does not make the API free.","DeepSeek: crea una chiave sulla piattaforma API DeepSeek e controlla saldo e prezzi API. deepseek-flash e deepseek-v4-pro sono API a consumo. L’accesso gratuito alla chat non rende gratuita l’API.");break;
            case AiProvider::openai:case AiProvider::anthropic:case AiProvider::ollama:case AiProvider::count:default:detail=text("OpenAI / Anthropic: create an API key in the selected developer console and check API billing and model access. A chat subscription does not automatically include API credit. API use may incur charges.","OpenAI / Anthropic: crea una chiave API nella console sviluppatori del servizio scelto e controlla fatturazione API e accesso ai modelli. L’abbonamento alla chat non include automaticamente credito API. L’uso delle API può avere costi.");break;
        }
        return detail+"\n\n"+text("1. API keys / help opens the official key page. Create a key and paste it in API KEY.\n2. Choose a suggested text model or press REFRESH for the live catalogue. You can also enter an exact compatible model ID. A listed model is not a guarantee of free access.\n3. Describe your song, select instruments and press COMPOSE. Review the preview, then APPLY.\n\nKeys stay in memory for this session. No audio or plugin state is uploaded. SETUP GUIDE contains detailed instructions, quota troubleshooting and service credits.","1. Chiavi API / aiuto apre la pagina ufficiale. Crea una chiave e incollala in CHIAVE API.\n2. Scegli un modello di testo suggerito o premi AGGIORNA per il catalogo attuale. Puoi anche scrivere un ID compatibile esatto. La presenza nell’elenco non garantisce l’accesso gratuito.\n3. Descrivi il brano, scegli gli strumenti e premi COMPONI. Controlla l’anteprima, poi APPLICA.\n\nLe chiavi restano in memoria per la sessione. Audio e stato dei plugin non vengono inviati. GUIDA IA contiene istruzioni dettagliate, problemi di quota e crediti dei servizi.");
    }
public:
    void refreshLocal(){if(activeProvider==3&&!job&&!modelsJob)refreshModels();}
private:
    void refreshModels(){
        if(job)return;
        ++modelRequestId;owner.retiredJobs.retire(std::move(modelsJob));modelsReady=false;ready=false;
        AiOptions options;options.provider=(AiProvider)activeProvider;options.key=key.getText().trim();options.endpoint=endpoint.getText();
        AiHttpRequest request;const auto valid=aiModelsRequest(options,{},request);
        if(valid.failed()){status.setText(tr(valid.getErrorMessage()),juce::dontSendNotification);preview.setText(setupHelp());setBusy(false);return;}
        if(activeProvider==3){juce::String address;aiOllamaEndpoint(options.endpoint,address);options.endpoint=address;endpoint.setText(address,false);Localisation::setSetting("aiOllamaEndpoint",address);}
        const auto previous=model.getText();if(activeProvider==3)model.clear(juce::dontSendNotification);setBusy(true);
        status.setText(text("Reading the current model catalogue…","Lettura del catalogo modelli aggiornato…"),juce::dontSendNotification);
        const int id=modelRequestId;juce::Component::SafePointer<AiComposerPanel> safe(this);
        modelsJob=std::make_unique<ModelListJob>(std::move(options),[safe,id,previous](juce::Result result,juce::StringArray names){
            if(!safe||id!=safe->modelRequestId)return;
            safe->owner.retiredJobs.retire(std::move(safe->modelsJob));
            if(result.wasOk()){
                safe->model.clear(juce::dontSendNotification);safe->model.addItemList(names,1);const auto chosen=previous.isNotEmpty()&&(safe->activeProvider!=3||names.contains(previous))?previous:names[0];
                safe->model.setText(chosen,juce::dontSendNotification);safe->modelsReady=true;safe->preview.setText(safe->setupHelp());
                Localisation::setSetting(("aiModel"+juce::String(safe->activeProvider)).toRawUTF8(),chosen);
                safe->status.setText(safe->activeProvider==3?text("Ollama connected. Choose a local model and describe your song. No API key needed.","Ollama collegato. Scegli un modello locale e descrivi il brano. Non serve una chiave API."):juce::String(names.size())+text(" text models loaded. Check your account quota; you can edit the model ID."," modelli di testo caricati. Controlla la quota del tuo account; puoi modificare l’ID modello."),juce::dontSendNotification);
            }else{safe->status.setText(tr(result.getErrorMessage()),juce::dontSendNotification);safe->preview.setText(tr(result.getErrorMessage())+"\n\n"+safe->setupHelp());}
            safe->setBusy(false);
        });modelsJob->startThread();
    }
    void start(){
        if(job||modelsJob)return;
        refreshTracks();
        if(activeProvider==3&&!modelsReady){refreshModels();return;}auto captured=owner.captureAiProject(base);if(captured.failed()){status.setText(tr(captured.getErrorMessage()),juce::dontSendNotification);return;}
        AiOptions options;options.provider=(AiProvider)provider.getSelectedItemIndex();options.model=model.getText().trim();options.key=key.getText().trim();options.endpoint=endpoint.getText().trim();options.prompt=prompt.getText().trim();options.wholeSong=mode.getSelectedId()==1;options.seconds=duration.getSelectedId()==1?0:duration.getText().getDoubleValue()*60;
        for(size_t i=0;i<tracks.size();++i)if(tracks[i]->getToggleState()&&tracks[i]->isEnabled())options.tracks.push_back((int)i);
        AiHttpRequest request;auto validation=aiHttpRequest(base,options,request);if(validation.failed()){status.setText(tr(validation.getErrorMessage()),juce::dontSendNotification);return;}
        Localisation::setSetting(("aiModel"+juce::String(provider.getSelectedItemIndex())).toRawUTF8(),options.model);Localisation::setSetting("aiOllamaEndpoint",options.endpoint);
        ready=false;setBusy(true);status.setText(text("Composing MIDI and arrangement… you can cancel at any time.","Composizione MIDI e arrangiamento… puoi annullare in qualsiasi momento."),juce::dontSendNotification);preview.setText(text("The model is writing your composition. Nothing has changed in the project yet.","Il modello sta scrivendo la composizione. Il progetto non è ancora stato modificato."));
        const int id=++requestId;juce::Component::SafePointer<AiComposerPanel> safe(this);job=std::make_unique<CompositionJob>(base,std::move(options),[safe,id](juce::Result result,Project p,juce::String summary){if(!safe||id!=safe->requestId)return;safe->owner.retiredJobs.retire(std::move(safe->job));safe->ready=result.wasOk();if(result.wasOk()){safe->composition=std::move(p);juce::String report=summary+"\n\n";for(const auto& section:safe->composition.sections)report+=juce::String::fromUTF8(section.name.c_str())+juce::String::fromUTF8("  ·  ")+juce::String(section.bars)+" "+tr("bars")+"\n";report+="\n";for(const auto& track:safe->composition.tracks){size_t count=0;for(const auto& c:track.clips)count+=c.notes.size();if(count)report+=juce::String::fromUTF8(track.name.c_str())+juce::String::fromUTF8("  ·  ")+juce::String((int)count)+" MIDI\n";}safe->preview.setText(report);safe->status.setText(text("Ready. Review the composition, then apply it.","Pronta. Controlla la composizione, poi applicala."),juce::dontSendNotification);}else{safe->preview.setText(tr(result.getErrorMessage()));safe->status.setText(text("Generation failed. Your project is unchanged.","Generazione non riuscita. Il progetto è invariato."),juce::dontSendNotification);}safe->setBusy(false);});job->startThread();
    }
};
AiComposerWindow::AiComposerWindow(MainComponent& m):DocumentWindow("Fase Groove Arranger / "+text("Create with AI","Crea con IA"),Daw::base,DocumentWindow::closeButton),owner(m),panel(std::make_unique<AiComposerPanel>(m)){
    setUsingNativeTitleBar(true);setAlwaysOnTop(true);setContentNonOwned(panel.get(),true);setResizable(true,false);setResizeLimits(920,700,1200,1000);const auto* display=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();centreWithSize(960,display?std::clamp(display->userArea.getHeight()-72,700,800):800);
}
AiComposerWindow::~AiComposerWindow(){clearContentComponent();panel.reset();}
void AiComposerWindow::closeButtonPressed(){panel->cancelJob();owner.dismissAuxiliaryWindow(*this);}
void AiComposerWindow::show(){panel->refreshTracks();panel->refreshLocal();if(!isVisible())centreWithSize(getWidth(),getHeight());setAlwaysOnTop(true);setVisible(true);toFront(true);}
bool AiComposerWindow::verify(const juce::File& f){return panel->verify(f)&&isAlwaysOnTop();}
}
