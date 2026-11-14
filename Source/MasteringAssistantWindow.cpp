// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MasteringAssistantWindow.h"
#include "MixAssistantWindow.h"
#include "AssistantProject.h"
#include "MainComponent.h"
#include "MasteringAnalysis.h"
#include "AssistantUi.h"
#include "RenderProgressTask.h"
#include "EffectsUi.h"
#include "GraphicEqWindow.h"
namespace fga {
class MasteringAssistantWindow::Panel:public juce::Component,private juce::Timer {
    struct Job:RenderProgressTask {
        std::vector<MasteringInput> inputs;juce::File reference;MasteringTargets targets;const Localisation::Language language=Localisation::current();
        std::shared_ptr<MasteringReport> report=std::make_shared<MasteringReport>(),basis;
        juce::AudioBuffer<float> preview;int side=-1;std::function<void()> done;
        juce::Result result=juce::Result::fail(tr("Analysis did not complete."));
        Job():RenderProgressTask("Mastering Assistant / local WAV"){}
        void run()override{
            try {
                if(side<0)result=analyseMastering(inputs,reference,targets,*report,[this]{return threadShouldExit();},[this](double p){setProgress(p);});
                else if(!basis||!masteringFilesUnchanged(*basis))result=juce::Result::fail(tr("WAV changed; analyse again."));
                else{if(threadShouldExit())return;preview.makeCopyOf(side==0?basis->a:basis->b);result=threadShouldExit()?juce::Result::fail(tr("Preview cancelled.")):juce::Result::ok();setProgress(1);}
            }catch(const std::exception& e){result=juce::Result::fail(tr("Mastering analysis failed: ")+juce::String(e.what()));}
            catch(...){result=juce::Result::fail(tr("Mastering analysis failed."));}
        }
        void threadComplete(bool cancelled)override{if(cancelled)result=juce::Result::fail(tr("Operation cancelled."));if(failedToStart())result=juce::Result::fail(tr("Cannot start worker."));if(done)done();}
    };
    MainComponent& owner;std::unique_ptr<Job> job;std::shared_ptr<MasteringReport> report;
    std::vector<MasteringInput> inputs;juce::File reference;std::unique_ptr<juce::FileChooser> chooser;
    juce::TextButton mix{"Mix / exported WAV"},ref{"Optional reference"},stems{"Add stems"},vocal{"Add vocal stem"},clear{"Clear"},analyse{"Analyse / export QA"};
    juce::TextButton a{"A: mix excerpt"},b{"B: reference excerpt"},stop{"Stop preview"},eq{"Track EQ"},fx{"Master inserts"},exportWav{"Export..."};
    juce::ToggleButton useLufs{"Optional LUFS target"},expectVocal{"Expect vocal stem"};
    juce::Slider lufs,ceiling,startA,startB;juce::ComboBox rate,bits;juce::TextEditor text;juce::Label note;juce::Rectangle<int> metrics;int shownStep=-99;
    std::uint64_t revision=0,ownedPreview=0;bool audition=false;Localisation::Language language=Localisation::current();
    void show(const juce::String& value){assistantui::showReport(text,value);repaint();}
    int step()const{return !report||report->files.empty()?-1:audition?3:1;}
    bool ours()const{return audition&&owner.audio.previewSerial()==ownedPreview;}
    void stopOurs(){if(ours())owner.audio.stopPreview();audition=false;a.setToggleState(false,juce::dontSendNotification);b.setToggleState(false,juce::dontSendNotification);}
    void invalidate(){stopOurs();report.reset();showFiles();}
    MasteringTargets targets()const{MasteringTargets t;t.useLufs=useLufs.getToggleState();t.expectVocal=expectVocal.getToggleState();t.lufs=lufs.getValue();t.ceilingDb=ceiling.getValue();t.startA=startA.getValue();t.startB=startB.getValue();constexpr int rates[]{0,44100,48000,96000};constexpr int depths[]{0,16,24};t.expectedRate=rates[std::clamp(rate.getSelectedId()-1,0,3)];t.expectedBits=depths[std::clamp(bits.getSelectedId()-1,0,2)];return t;}
    void showFiles(){juce::String s=tr("LOCAL RENDERED FILES - not live project analysis\n");for(size_t i=0;i<inputs.size();++i)s+=(i==0?tr("MIX: "):inputs[i].vocal?tr("VOCAL STEM: "):"STEM: ")+inputs[i].file.getFullPathName()+"\n";if(reference!=juce::File())s+=tr("REFERENCE: ")+reference.getFullPathName();show(s);}
    void choose(int kind){
        if(job||owner.editingRestricted()||owner.loadingPlugin)return;stopOurs();chooser=std::make_unique<juce::FileChooser>(tr("Choose local rendered WAV"),juce::File(),"*.wav");
        juce::Component::SafePointer<Panel> safe(this);
        chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles|(kind>=2?juce::FileBrowserComponent::canSelectMultipleItems:0),[safe,kind](const juce::FileChooser& fc){
            if(!safe||safe->job||fc.getResults().isEmpty())return;auto& p=*safe;
            if(kind>=2&&p.inputs.empty()){p.show(tr("Choose mix WAV first."));return;}
            for(const auto& f:fc.getResults()){
                if(kind==0){if(p.inputs.empty())p.inputs.push_back({f});else p.inputs[0]={f};}
                else if(kind==1)p.reference=f;
                else{auto it=std::find_if(p.inputs.begin(),p.inputs.end(),[&](const MasteringInput& i){return i.file==f;});if(it!=p.inputs.end()){if(it!=p.inputs.begin()&&kind==3)it->vocal=true;}
                    else if(p.inputs.size()<33)p.inputs.push_back({f,kind==3});else{p.invalidate();p.show(tr("At most 32 stems. Clear to start again."));return;}}
            }p.invalidate();
        });
    }
    void complete(){
        const bool previewJob=job->side>=0;
        if(job->language!=Localisation::current()){invalidate();show(tr("Language changed; analyse again."));}
        else if(job->result.failed()){show(job->result.getErrorMessage());report.reset();}
        else if(revision!=owner.controlRevision()){show(tr("Project changed while working; analyse again."));report.reset();}
        else if(previewJob){
            if(canAudition()&&std::abs(owner.audio.spectrumSampleRate()-report->comparisonRate)<.1){
                owner.sampleBrowser.stopPreview();owner.audio.startPreview(std::move(job->preview),report->comparisonRate);
                ownedPreview=owner.audio.previewSerial();audition=true;(job->side==0?a:b).setToggleState(true,juce::dontSendNotification);
            }else show(tr("Stop playback/monitoring and wait for tails; device rate must match the comparison WAVs."));
        }else{report=job->report;present();}
        owner.retiredJobs.retire(std::move(job));
    }
    void launch(){juce::Component::SafePointer<Panel> safe(this);job->done=[safe]{if(safe)safe->complete();};job->launchThread();}
    void analyseFiles(){if(job||inputs.empty()||owner.editingRestricted()||owner.loadingPlugin)return;stopOurs();report.reset();revision=owner.controlRevision();job=std::make_unique<Job>();job->inputs=inputs;job->reference=reference;job->targets=targets();launch();}
    bool canAudition()const{return !owner.editingRestricted()&&!owner.loadingPlugin&&!owner.audio.isPlaying()&&!owner.audio.isMonitoring()&&!owner.audio.isSampleAuditioning()&&owner.audio.masterLevel()<1e-4f;}
    void play(int side){
        if(job||!report||!report->comparisonReady||revision!=owner.controlRevision())return;
        const bool own=ours();if(!own&&!canAudition()){show(tr("Stop playback/monitoring and wait for tails before A/B."));return;}
        if(std::abs(owner.audio.spectrumSampleRate()-report->comparisonRate)>.1){show(tr("Set the audio device to ")+juce::String(report->comparisonRate,0)+tr(" Hz before A/B; no implicit resampling."));return;}
        stopOurs();owner.sampleBrowser.stopPreview();job=std::make_unique<Job>();job->side=side;job->basis=report;launch();
    }
    void present(){
        juce::String s=tr("MEASUREMENTS OF SUPPLIED WAV FILES - not a quality certificate or live project analysis\n\n");
        auto append=[&](const MasteringFile& f,const juce::String& label){s+=label+": "+f.file.getFileName()+" / "+juce::String(f.rate,0)+" Hz / "+juce::String(f.bits)+" bit / "+juce::String(f.channels)+tr(" channels / ")+juce::String(f.frames/f.rate,2)+" s\n  LUFS "+(f.mix.lufs> -99?juce::String(f.mix.lufs,2):tr("undefined"))+" | TP16 "+juce::String(juce::Decibels::gainToDecibels(f.truePeak,-120.),2)+tr(" dBTP | sample peak ")+juce::String(juce::Decibels::gainToDecibels(f.mix.peak,-120.),2)+" dBFS\n";};
        for(size_t i=0;i<report->files.size();++i)append(report->files[i],i==0?"MIX":report->files[i].vocal?tr("VOCAL STEM"):"STEM");if(report->hasReference)append(report->reference,tr("REFERENCE"));
        s+=tr("\nEXPORT QA: ")+juce::String(report->qaPassed?tr("selected numeric checks passed"):tr("review required"))+"\n"+report->qa.joinIntoString("\n")+tr("\n\nSUBJECTIVE SUGGESTIONS - approve any edit yourself in the existing track EQ/master inserts\n");
        s+=report->suggestions.isEmpty()?tr("No threshold-based suggestion.\n"):report->suggestions.joinIntoString("\n")+"\n";s+="\n"+report->comparisonNote;show(s);
    }
    void timerCallback()override{
        if(const int now=step()*2+(job!=nullptr);now!=shownStep){shownStep=now;repaint();}
        if(audition&&(!ours()||!owner.audio.isPreviewing())){audition=false;a.setToggleState(false,juce::dontSendNotification);b.setToggleState(false,juce::dontSendNotification);}
        if(ours()&&(revision!=owner.controlRevision()||owner.editingRestricted()||owner.audio.isPlaying()||owner.audio.isMonitoring()))stopOurs();
        const bool busy=job!=nullptr||owner.editingRestricted()||owner.loadingPlugin;for(auto* c:std::initializer_list<juce::Component*>{&mix,&ref,&stems,&vocal,&clear,&analyse,&useLufs,&expectVocal,&lufs,&ceiling,&startA,&startB,&rate,&bits,&eq,&fx,&exportWav})c->setEnabled(!busy);
        analyse.setEnabled(!busy&&!inputs.empty());lufs.setEnabled(!busy&&useLufs.getToggleState());const bool ready=!busy&&report&&report->comparisonReady&&revision==owner.controlRevision();a.setEnabled(ready);b.setEnabled(ready);
    }
public:
    explicit Panel(MainComponent& m):owner(m){
        for(auto* c:std::initializer_list<juce::Component*>{&mix,&ref,&stems,&vocal,&clear,&analyse,&a,&b,&stop,&eq,&fx,&useLufs,&expectVocal,&lufs,&ceiling,&startA,&startB,&rate,&bits,&text,&note,&exportWav})addAndMakeVisible(c);assistantui::styleReport(text);assistantui::stylePrimary(analyse);
        text.setMultiLine(true);text.setReadOnly(true);text.setScrollbarsShown(true);note.setText("Original/rendered WAVs only. Tag a vocal stem to check audio presence. Targets are optional/user chosen. A/B: 10-second excerpts, transport stopped, monitoring off, matching device rate. Edits use existing track EQ/master inserts and Undo.",juce::dontSendNotification);note.setFont(Daw::font(12));note.setMinimumHorizontalScale(1.f);note.setJustificationType(juce::Justification::topLeft);
        auto slider=[&](juce::Slider& s,double lo,double hi,double value,const juce::String& name){s.setSliderStyle(juce::Slider::LinearBar);s.setRange(lo,hi,.1);s.setValue(value,juce::dontSendNotification);Localisation::bind(s,[&s,name]{s.setTextValueSuffix(" "+tr(name));s.setTooltip(tr(name));});s.onValueChange=[this]{if(!job)invalidate();};};
        slider(lufs,-30,-5,-14,"LUFS");slider(ceiling,minExportCeilingDb,maxExportCeilingDb,-1,"dBTP ceiling");slider(startA,0,899,0,"s / A offset");slider(startB,0,899,0,"s / B offset");
        rate.addItemList({"Any rate","44100 Hz","48000 Hz","96000 Hz"},1);bits.addItemList({"Any depth","16 bit","24 bit"},1);Localisation::bindItems(rate,{"Any rate","44100 Hz","48000 Hz","96000 Hz"});Localisation::bindItems(bits,{"Any depth","16 bit","24 bit"});rate.setSelectedId(1,juce::dontSendNotification);bits.setSelectedId(1,juce::dontSendNotification);
        for(auto* box:{&rate,&bits})box->onChange=[this]{if(!job)invalidate();};useLufs.onClick=[this]{invalidate();};expectVocal.onClick=[this]{invalidate();};
        mix.onClick=[this]{choose(0);};ref.onClick=[this]{choose(1);};stems.onClick=[this]{choose(2);};vocal.onClick=[this]{choose(3);};clear.onClick=[this]{if(job)return;inputs.clear();reference=juce::File();invalidate();};analyse.onClick=[this]{analyseFiles();};
        a.onClick=[this]{play(0);};b.onClick=[this]{play(1);};stop.onClick=[this]{stopOurs();if(job&&job->side>=0)job->requestCancel();};
        eq.onClick=[this]{stopOurs();owner.openGraphicEq();};fx.onClick=[this]{stopOurs();owner.showEffectChain(masterEffectTrack);};exportWav.onClick=[this]{stopOurs();owner.exportMenu();};
        for(auto* button:std::initializer_list<juce::Button*>{&mix,&ref,&stems,&vocal,&clear,&analyse,&a,&b,&stop,&eq,&fx,&exportWav,&useLufs,&expectVocal})Localisation::bindText(*button,button->getButtonText());
        Localisation::bindLabel(note,note.getText(),false);
        Localisation::bind(*this,[this]{if(language!=Localisation::current()){language=Localisation::current();if(job)job->requestCancel();invalidate();}repaint();});
        showFiles();setSize(1100,740);
    }
    ~Panel()override{close();}
    void open(){startTimer(100);}
    void close(){stopTimer();stopOurs();if(job){job->done=nullptr;job->requestCancel();owner.retiredJobs.retire(std::move(job));}}
    void paint(juce::Graphics& g)override{g.fillAll(Daw::base);Daw::deviceHeader(g,{18,8,460,54},tr("MASTERING ASSISTANT"),tr("LOCAL / MEASURE / COMPARE / VERIFY"),Daw::returnB);
        assistantui::drawSteps(g,{getWidth()-460,24,442,22},step(),job!=nullptr);
        std::vector<assistantui::Metric> m;
        if(step()>=0){const auto& f=report->files[0];m=assistantui::mixMetrics(f.mix.lufs,f.truePeak,f.mix.peak,f.mix.rms,f.mix.correlationValid,f.mix.correlation,ceiling.getValue());
            if(report->hasReference&&report->reference.mix.lufs> -99&&f.mix.lufs> -99){const double d=f.mix.lufs-report->reference.mix.lufs;m.push_back({tr("VS REFERENCE"),(d>=0?"+":"")+juce::String(d,1),"LU",std::abs(d)>3?assistantui::Level::warn:assistantui::Level::neutral,d,-12,12,0.});}
            m.push_back({tr("EXPORT QA"),report->qaPassed?tr("PASS"):tr("CHECK"),"",report->qaPassed?assistantui::Level::good:assistantui::Level::warn});}
        assistantui::drawMetrics(g,metrics,m,tr("Choose a mix or exported WAV and analyse: measured loudness, true peak and QA appear here."));}
    void resized()override{
        auto r=getLocalBounds().reduced(18);r.removeFromTop(60);auto row=r.removeFromTop(30);
        for(auto* button:{&mix,&ref,&stems,&vocal,&clear,&analyse}){button->setBounds(row.removeFromLeft(button==&analyse?160:button==&clear?65:150));row.removeFromLeft(6);}r.removeFromTop(8);note.setBounds(r.removeFromTop(40));
        row=r.removeFromTop(30);useLufs.setBounds(row.removeFromLeft(155));lufs.setBounds(row.removeFromLeft(110));row.removeFromLeft(10);ceiling.setBounds(row.removeFromLeft(170));row.removeFromLeft(10);rate.setBounds(row.removeFromLeft(125));row.removeFromLeft(8);bits.setBounds(row.removeFromLeft(110));expectVocal.setBounds(row.reduced(6,0));r.removeFromTop(8);
        row=r.removeFromTop(28);startA.setBounds(row.removeFromLeft(200));row.removeFromLeft(10);startB.setBounds(row.removeFromLeft(200));r.removeFromTop(10);metrics=r.removeFromTop(78);r.removeFromTop(10);
        auto bottom=r.removeFromBottom(34);for(auto* button:{&a,&b,&stop,&eq,&fx,&exportWav}){button->setBounds(bottom.removeFromLeft(button==&b?180:button==&exportWav?130:150));bottom.removeFromLeft(8);}r.removeFromBottom(8);text.setBounds(r);
    }
    bool verify(const juce::File&);
};
bool MasteringAssistantWindow::Panel::verify(const juce::File& folder){
#if !FGA_SELF_TESTS
    juce::ignoreUnused(folder);return false;
#else
    juce::String checks;bool ok=true;auto check=[&](bool condition,const char* label){checks+=(condition?"PASS ":"FAIL ")+juce::String(label)+"\n";ok=condition&&ok;};
    const auto languageBefore=Localisation::current();
    Localisation::setCurrent(Localisation::Language::italian);
    {
        Panel initiallyItalian(owner);
        check(initiallyItalian.analyse.getButtonText()=="Analizza / verifica"&&initiallyItalian.note.getText().startsWith("Solo WAV"),"window constructed in Italian follows saved language");
        Localisation::setCurrent(Localisation::Language::english);
        check(initiallyItalian.analyse.getButtonText()=="Analyse / export QA"&&initiallyItalian.note.getText().startsWith("Original/rendered WAVs only."),"Italian-created controls and instructions restore canonical English keys");
    }

    const auto before=juce::JSON::toString(projectJson(owner.project));const bool dirty=owner.dirty;const auto history=owner.undoStack.size(),redo=owner.redoStack.size();const int pdc=owner.audio.latencySamples();
    const double sampleRate=owner.audio.spectrumSampleRate();const auto mixFile=folder.getChildFile("master-fixture.wav"),refFile=folder.getChildFile("reference-fixture.wav");
    auto write=[&](const juce::File& file,float amplitude){file.deleteFile();juce::WavAudioFormat format;auto stream=file.createOutputStream();if(!stream)return false;std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(),sampleRate,2,24,{},0));if(!writer)return false;stream.release();juce::AudioBuffer<float> audio(2,4096);for(int block=0;block<24;++block){for(int i=0;i<4096;++i){const float value=amplitude*(float)std::sin((block*4096+i)*2*juce::MathConstants<double>::pi*997/sampleRate);audio.setSample(0,i,value);audio.setSample(1,i,value);}if(!writer->writeFromAudioSampleBuffer(audio,0,4096))return false;}return writer->flush();};
    check(write(mixFile,.4f)&&write(refFile,.1f),"write stereo comparison fixtures");
    auto wait=[&]{const auto deadline=juce::Time::getMillisecondCounter()+20000;juce::AudioBuffer<float> buffer(2,512);juce::AudioIODeviceCallbackContext context;
        while(job&&juce::Time::getMillisecondCounter()<deadline){owner.audio.audioDeviceIOCallbackWithContext(nullptr,0,buffer.getArrayOfWritePointers(),2,512,context);juce::MessageManager::getInstance()->runDispatchLoopUntil(10);}return !job;};
    owner.audio.stop();inputs={{mixFile}};reference=refFile;analyseFiles();check(wait()&&report&&report->comparisonReady,"real asynchronous analysis produces normalized comparison");
    if(report){timerCallback();present();if(auto out=folder.getChildFile("mastering-assistant.png").createOutputStream()){juce::PNGImageFormat png;check(png.writeImageToStream(createComponentSnapshot(getLocalBounds()),*out),"capture interface");}}
    Localisation::setCurrent(Localisation::Language::italian);
    check(analyse.getButtonText()=="Analizza / verifica"&&rate.getText()=="Qualsiasi Hz"&&getTopLevelComponent()->getName()=="Assistente mastering"&&!report,"Italian controls/title and cached-language invalidation");
    analyseFiles();check(wait()&&report&&text.getText().contains("VERIFICA EXPORT")&&text.getText().contains("SUPERATO: il mix"),"Italian asynchronous QA and advice");
    if(auto stream=folder.getChildFile("mastering-assistant-it.png").createOutputStream())juce::PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()),*stream);
    analyseFiles();Localisation::setCurrent(Localisation::Language::english);check(wait()&&analyse.getButtonText()=="Analyse / export QA"&&!report,"language change cancels active worker and restores English without stale analysis");
    analyseFiles();check(wait()&&report,"analysis after language change");
    a.onClick();check(wait()&&ours()&&owner.audio.isPreviewing(),"A uses existing browser preview");
    const auto token=ownedPreview;b.onClick();check(wait()&&ours()&&ownedPreview!=token,"B switches normalized source through existing player");
    stop.onClick();check(!owner.audio.isPreviewing(),"Stop reverses file audition");
    a.onClick();check(wait()&&ours(),"preview can restart");owner.refreshControls();timerCallback();check(!owner.audio.isPreviewing(),"project revision invalidates audition");
    const int track=owner.selectedTrack;eq.onClick();check(owner.graphicEqWindow&&owner.graphicEqWindow->isVisible()&&owner.selectedTrack==track,"Track EQ reuses existing editor without changing selected track");if(owner.graphicEqWindow)owner.graphicEqWindow->closeButtonPressed();
    fx.onClick();check(owner.effectsWindow&&owner.effectsWindow->isVisible()&&owner.effectsWindow->getName().contains("Master"),"Master inserts open the actual existing master chain");if(owner.effectsWindow)owner.effectsWindow->closeButtonPressed();
    // Another preview owner must survive this window closing.
    juce::AudioBuffer<float> other(2,2048);other.clear();owner.audio.startPreview(std::move(other),sampleRate);const auto external=owner.audio.previewSerial();close();check(owner.audio.isPreviewing()&&external==owner.audio.previewSerial(),"closing does not stop another preview owner");owner.audio.stopPreview();open();
    inputs={{mixFile}};reference=refFile;analyseFiles();if(job)job->getAlertWindow()->getButton(0)->onClick();check(wait()&&!report,"real Cancel handler is cooperative");
    analyseFiles();check(wait()&&report,"analysis retries after Cancel");
    if(report){rate.setSelectedId(2,juce::dontSendNotification);invalidate();check(!report,"target edits invalidate cached analysis");rate.setSelectedId(1,juce::dontSendNotification);}
    inputs={{folder.getChildFile("missing.wav")}};analyseFiles();check(wait()&&!report,"missing file returns an error");
    inputs={{mixFile}};analyseFiles();close();check(!job,"close retires running job without joining UI");
    check(before==juce::JSON::toString(projectJson(owner.project))&&owner.dirty==dirty&&owner.undoStack.size()==history&&owner.redoStack.size()==redo&&owner.audio.latencySamples()==pdc,"project, dirty, Undo/Redo and PDC preserved");
    Localisation::setCurrent(languageBefore);
    folder.getChildFile("mastering-ui.txt").replaceWithText(checks);return ok;
#endif
}
MasteringAssistantWindow::MasteringAssistantWindow(MainComponent& m):DocumentWindow(tr("Mastering Assistant"),Daw::base,juce::DocumentWindow::closeButton){setLookAndFeel(&m.theme);setUsingNativeTitleBar(true);panel=new Panel(m);projectPanel=new ProjectAssistantPanel(m,true);tabs=new juce::TabbedComponent(juce::TabbedButtonBar::TabsAtTop);tabs->addTab(tr("Project"),Daw::base,projectPanel,true);tabs->addTab(tr("WAV files"),Daw::base,panel,true);Localisation::bind(*tabs,[this]{tabs->setTabName(0,tr("Project"));tabs->setTabName(1,tr("WAV files"));});setContentOwned(tabs,true);Localisation::bind(*this,[this]{setName(tr("Mastering Assistant"));});setResizable(true,false);setResizeLimits(1050,700,1800,1200);}
MasteringAssistantWindow::~MasteringAssistantWindow(){clearContentComponent();setLookAndFeel(nullptr);}
void MasteringAssistantWindow::open(){tabs->setCurrentTabIndex(0);projectPanel->open();panel->open();centreWithSize(1100,780);setVisible(true);toFront(true);}
void MasteringAssistantWindow::closeButtonPressed(){projectPanel->close();panel->close();setVisible(false);}
bool MasteringAssistantWindow::demo(const juce::File& f){tabs->setCurrentTabIndex(0);return projectPanel->demo(f);}
bool MasteringAssistantWindow::verify(const juce::File& f){tabs->setCurrentTabIndex(1);const bool old=panel->verify(f);tabs->setCurrentTabIndex(0);return projectPanel->verify(f.getChildFile("project"))&&old;}
void MainComponent::showMasteringAssistant(){if(editingRestricted()||loadingPlugin)return;if(!masteringAssistantWindow)masteringAssistantWindow=std::make_unique<MasteringAssistantWindow>(*this);masteringAssistantWindow->open();}
// --assistant-demo: a generated song, the Mix then the Mastering suggestions all applied, with the
// before/after MP3s, the screenshots and the lists of changes (for a listening review).
bool MainComponent::assistantDemo(const juce::File& folder){
    folder.createDirectory();auto wait=[&]{const auto until=juce::Time::getMillisecondCounterHiRes()+120000;while(pluginsBusy()&&juce::Time::getMillisecondCounterHiRes()<until)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);return !pluginsBusy();};
    project=newProject(0);generate(project,Scope::song,0,0,4242);project.normalise();restoreInstruments();if(!wait())return false;refreshControls();
    auto render=[&](const char* name){audio.capturePluginStates(project);return audio.exportAudio(project,folder.getChildFile(name),-1,true,[]{return false;},[](double){}).wasOk();};
    bool ok=render("before.mp3");
    showMixAssistant();ok=mixAssistantWindow&&mixAssistantWindow->demo(folder.getChildFile("mix"))&&ok;if(mixAssistantWindow)mixAssistantWindow->closeButtonPressed();
    ok=render("after-mix.mp3")&&ok;
    showMasteringAssistant();ok=masteringAssistantWindow&&masteringAssistantWindow->demo(folder.getChildFile("master"))&&ok;if(masteringAssistantWindow)masteringAssistantWindow->closeButtonPressed();
    ok=render("after-master.mp3")&&ok;return ok;
}
// --ui-preview: the screens of this release (editor path bar, AUTO/TRIM badges, the assistants' columns).
bool MainComponent::uiPreview(const juce::File& folder){
    folder.createDirectory();auto wait=[&]{const auto until=juce::Time::getMillisecondCounterHiRes()+120000;while(pluginsBusy()&&juce::Time::getMillisecondCounterHiRes()<until)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);return !pluginsBusy();};
    auto shot=[&](juce::Component& c,const char* name){if(auto stream=folder.getChildFile(name).createOutputStream())juce::PNGImageFormat().writeImageToStream(c.createComponentSnapshot(c.getLocalBounds()),*stream);};
    project=newProject(0);generate(project,Scope::song,0,0,4242);project.normalise();
    auto& piano=project.tracks[0];piano.gain=juce::Decibels::decibelsToGain(1.2);piano.volume.enabled=true;piano.volume.points={{0,.6,0},{64,1.,0}};piano.panAutomation.enabled=true;piano.panAutomation.points={{0,.3,0},{64,.7,0}};
    for(auto& t:project.tracks)if(t.kind==2||t.kind==0)t.sends={.25,.1};
    restoreInstruments();if(!wait())return false;mixerVisible=true;resized();refreshControls();audio.seek(16);audio.play(true);for(int i=0;i<40;++i)juce::MessageManager::getInstance()->runDispatchLoopUntil(10);audio.play(false);
    addNativeCompressor(0);if(!wait())return false;closePluginEditor();openHostedEditor(0,1);juce::MessageManager::getInstance()->runDispatchLoopUntil(200);
    snapshotPluginWindow(folder.getChildFile("editor-piano-insert.png"));closePluginEditor();
    project.enableReturnChains();restoreInstruments();if(!wait())return false;openHostedEditor(returnEffectTrack,project.returnEffects[0].effects.empty()?1:project.returnEffects[0].effects[0].slot);juce::MessageManager::getInstance()->runDispatchLoopUntil(200);
    snapshotPluginWindow(folder.getChildFile("editor-return-a.png"));closePluginEditor();
    refreshControls();shot(*this,"main-mixer.png");
    showMixAssistant();const bool ok=mixAssistantWindow&&mixAssistantWindow->demo(folder.getChildFile("mix"));if(mixAssistantWindow)mixAssistantWindow->closeButtonPressed();
    return ok;
}
bool MainComponent::verifyMasteringAssistant(const juce::File& f){f.createDirectory();showMasteringAssistant();return masteringAssistantWindow&&masteringAssistantWindow->verify(f);}
}
