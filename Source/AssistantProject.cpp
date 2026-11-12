// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AssistantProject.h"
#include "MainComponent.h"
#include "RenderProgressTask.h"
#include "NativeCompressor.h"
#include "CompressorDSP.h"
#include "ProjectIO.h"
namespace fga {
namespace {
class FileCleanup final:public juce::Thread {
    std::shared_ptr<AssistantAudio> files;
public:
    explicit FileCleanup(std::shared_ptr<AssistantAudio> f):Thread("Assistant file cleanup"),files(std::move(f)){}
    void run()override{files.reset();}
};
int indexOf(const Project& p,std::uint64_t session){if(!session)return masterEffectTrack;for(size_t i=0;i<p.tracks.size();++i)if(p.tracks[i].sessionId==session)return (int)i;return -1;}
juce::String nameOf(const Project& p,const AssistantChange& c){const int ti=indexOf(p,c.session);return ti==masterEffectTrack?"MASTER":ti>=0?juce::String(p.tracks[(size_t)ti].name):tr("Missing track");}
juce::String description(const Project& p,const AssistantChange& c){
    juce::String value;
    if(c.kind==AssistantChange::gain)value=tr("Volume")+": "+juce::String(juce::Decibels::gainToDecibels(c.before,-120.),2)+" -> "+juce::String(juce::Decibels::gainToDecibels(c.after,-120.),2)+" dB";
    else if(c.kind==AssistantChange::eq)value="EQ "+juce::String(c.frequency,0)+" Hz / 0 -> "+juce::String(c.after,1)+" dB / Q "+juce::String(c.q,2);
    else{const char* names[]{"Threshold","Ratio","Knee","Attack","Release","Makeup"};const double range=CompressorDSP::maximum[(size_t)c.parameter]-CompressorDSP::minimum[(size_t)c.parameter],base=CompressorDSP::minimum[(size_t)c.parameter];value=tr("Native Compressor")+" / "+tr(names[c.parameter])+": "+juce::String(base+c.before*range,1)+" -> "+juce::String(base+c.after*range,1)+(c.parameter==1?" :1":" ms");}
    return nameOf(p,c)+" / "+value+" / "+c.reason;
}
}
RenderProgressTask* MainComponent::renderAssistantProject(std::shared_ptr<AssistantAudio> files,
    std::function<juce::Result(AssistantAudio&,const std::function<bool()>&,const std::function<void(double)>&)> work,
    std::function<void(juce::Result)> done,std::vector<AssistantChange> changes){
    if(editingRestricted()||loadingPlugin||exportTask)return nullptr;
    instrumentPanel.flushRender();if(changes.empty())files->snapshot=project;
    Project validated;if(assistantCandidate(project,audio,changes,validated).failed())return nullptr;
    struct Task final:RenderProgressTask {
        AudioEngine& engine;Project original;std::shared_ptr<AssistantAudio> files;
        using Work=decltype(work);using Done=decltype(done);
        Work operation;Done completion;std::vector<AssistantChange> changes;juce::Result result=juce::Result::fail("Analysis did not complete.");
        Task(AudioEngine& e,Project p,std::shared_ptr<AssistantAudio> f,Work w,Done d,std::vector<AssistantChange> c):RenderProgressTask(tr("Analyse project")),engine(e),original(std::move(p)),files(std::move(f)),operation(std::move(w)),completion(std::move(d)),changes(std::move(c)){}
        void run()override{
            // Move file-owning captures out of the task: their final release is on a
            // worker, even if the panel closes before the UI completion callback.
            auto data=std::move(files);auto analyse=std::move(operation);
            struct Restore {AudioEngine& engine;const Project& original;const std::vector<AssistantChange>& changes;~Restore(){assistantParameters(engine,original,changes,false);engine.publish(original);}}restore{engine,original,changes};
            try{
                if(threadShouldExit()){result=juce::Result::fail(tr("Analysis cancelled."));return;}
                for(int track=0;track<engineTrackCount;++track){if(isReturnTrack(track)&&!data->snapshot.returnChains)continue;if(track>= (int)data->snapshot.tracks.size()&&track<maxTracks)continue;
                    for(const auto& slot:data->snapshot.effectTarget(track).effects)if(!slot.bypassed&&!engine.effectPlugin(track,slot.slot)){result=juce::Result::fail(tr("Reconnect missing project effects before analysis."));return;}}
                if(!data->directory.createDirectory()){result=juce::Result::fail(tr("Cannot create analysis temporary directory."));return;}
                assistantParameters(engine,original,changes,true);
                result=engine.renderAnalysis(data->snapshot,data->directory,data->tracks,[this]{return threadShouldExit();},[this](double value){setProgress(value*.7);});
                if(result.wasOk())result=analyse(*data,[this]{return threadShouldExit();},[this](double value){setProgress(.7+value*.3);});
            }catch(const std::exception& e){result=juce::Result::fail(tr("Analysis failed: ")+juce::String(e.what()));}catch(...){result=juce::Result::fail(tr("Analysis failed unexpectedly."));}
        }
        void threadComplete(bool cancelled)override{if(failedToStart())result=juce::Result::fail(tr("Cannot start analysis worker."));completion(cancelled?juce::Result::fail(tr("Analysis cancelled.")):result);}
    };
    const double beat=audio.position();instrumentPanel.flushRender();audio.stop();audio.stopPreview();audio.stopSampleAudition();mediaBusy=true;updateEditingAvailability();audio.devices.removeAudioCallback(&audio);
    juce::Component::SafePointer<MainComponent> safe(this);
    exportTask=std::make_unique<Task>(audio,project,std::move(files),std::move(work),[safe,beat,done=std::move(done)](juce::Result result){if(!safe)return;safe->audio.seek(beat);safe->audio.devices.addAudioCallback(&safe->audio);safe->mediaBusy=false;safe->retiredJobs.retire(std::move(safe->exportTask));safe->updateEditingAvailability();done(result);},std::move(changes));
    auto* task=exportTask.get();task->launchThread();return task;
}
void MainComponent::cancelAssistantProject(RenderProgressTask* expected){if(expected&&exportTask.get()==expected)expected->requestCancel();}
ProjectAssistantPanel::ProjectAssistantPanel(MainComponent& m,bool mastering):owner(m),master(mastering){
    for(auto* component:std::initializer_list<juce::Component*>{&analyse,&reference,&apply,&a,&b,&r,&stop,&useLufs,&lufs,&ceiling,&note,&text,&list})addAndMakeVisible(component);
    for(auto* button:std::initializer_list<juce::Button*>{&analyse,&reference,&apply,&a,&b,&r,&stop,&useLufs})Localisation::bindText(*button,button->getButtonText());
    Localisation::bindLabel(note,"Analyse the open project. Track taps are post-insert/fader, before groups/returns/master; the full mix includes all routing. Select changes, compare, then Apply with one Undo. External reference is optional.",false);note.setFont(Daw::font(12));note.setMinimumHorizontalScale(1.f);note.setJustificationType(juce::Justification::topLeft);
    text.setMultiLine(true);text.setReadOnly(true);text.setScrollbarsShown(true);list.setModel(this);list.setRowHeight(46);list.setColour(juce::ListBox::backgroundColourId,Daw::base);list.setOutlineThickness(1);list.setColour(juce::ListBox::outlineColourId,Daw::line);
    auto slider=[&](juce::Slider& slider,double low,double high,double value,const char* unit){slider.setSliderStyle(juce::Slider::LinearBar);slider.setRange(low,high,.1);slider.setValue(value,juce::dontSendNotification);Localisation::bind(slider,[&slider,unit]{slider.setTextValueSuffix(" "+tr(unit));});slider.onValueChange=[this]{invalidate();};};
    slider(lufs,-30,-5,-14,"LUFS");slider(ceiling,minExportCeilingDb,maxExportCeilingDb,-1,"dBTP ceiling");useLufs.onClick=[this]{invalidate();};useLufs.setVisible(master);lufs.setVisible(master);
    analyse.onClick=[this]{renderProject();};apply.onClick=[this]{applySelected();};a.onClick=[this]{previewSide(0);};b.onClick=[this]{previewSide(1);};r.onClick=[this]{stopPreview();referenceMode=!referenceMode;present();previewSide(0);};stop.onClick=[this]{stopPreview();};
    reference.onClick=[this]{
        if(task)return;juce::PopupMenu menu;menu.addItem(1,tr("Choose local rendered WAV"));menu.addItem(2,tr("Remove reference"),refFile!=juce::File());
        juce::Component::SafePointer<ProjectAssistantPanel> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&reference),[safe](int result){
            if(!safe||safe->task||!result)return;
            if(result==2){safe->refFile=juce::File();safe->invalidate();return;}
            safe->chooser=std::make_unique<juce::FileChooser>(tr("Choose local rendered WAV"),juce::File(),"*.wav");
            safe->chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe](const juce::FileChooser& fc){if(safe&&fc.getResult()!=juce::File()){safe->refFile=fc.getResult();safe->invalidate();}});
        });
    };
    Localisation::bind(*this,[this]{if(language!=Localisation::current()){language=Localisation::current();owner.cancelAssistantProject(task);invalidate();}present();repaint();});setSize(1060,700);startTimer(100);
}
ProjectAssistantPanel::~ProjectAssistantPanel(){stopTimer();close();list.setModel(nullptr);}
void ProjectAssistantPanel::release(std::shared_ptr<AssistantAudio>& files){if(!files)return;auto cleanup=std::make_unique<FileCleanup>(std::move(files));cleanup->startThread();owner.retiredJobs.retire(std::move(cleanup));}
void ProjectAssistantPanel::open(){startTimer(100);present();timerCallback();}
void ProjectAssistantPanel::close(){referenceMode=false;++generation;owner.cancelAssistantProject(task);stopPreview();release(audio);release(beforeAfter);mixReport.reset();masterReport.reset();comparison.reset();changes.clear();list.updateContent();}
void ProjectAssistantPanel::stopPreview(){if(preview&&owner.audio.previewSerial()==preview)owner.audio.stopPreview();preview=0;a.setToggleState(false,juce::dontSendNotification);b.setToggleState(false,juce::dontSendNotification);r.setToggleState(referenceMode,juce::dontSendNotification);}
void ProjectAssistantPanel::invalidate(){referenceMode=false;++generation;stale=true;applied=false;stopPreview();owner.cancelAssistantProject(task);release(audio);release(beforeAfter);mixReport.reset();masterReport.reset();comparison.reset();changes.clear();list.updateContent();present();}
void ProjectAssistantPanel::renderProject(){
    if(task||owner.editingRestricted()||owner.pluginsBusy())return;owner.instrumentPanel.flushRender();close();stale=false;applied=false;
    audio=std::make_shared<AssistantAudio>();audio->snapshot=owner.project;for(size_t i=0;i<audio->snapshot.tracks.size();++i)audio->tracks.push_back((int)i);
    targets={};targets.refineComparison=true;targets.ceilingDb=ceiling.getValue();targets.useLufs=master&&useLufs.getToggleState();targets.lufs=lufs.getValue();
    mixReport=std::make_shared<MixReport>();masterReport=std::make_shared<MasteringReport>();const auto mixResult=mixReport;const auto masterResult=masterReport;const auto target=targets;const auto ref=refFile;const auto token=++generation;
    juce::Component::SafePointer<ProjectAssistantPanel> safe(this);
    task=owner.renderAssistantProject(audio,[mixResult,masterResult,target,ref](AssistantAudio& data,const auto& cancel,const auto& progress){auto result=analyseMixFiles(data.mixInputs(),*mixResult,cancel,[&](double p){progress(p*.5);});if(result.wasOk()){auto actual=target;actual.startA=mixResult->previewStart;result=analyseMastering({{data.mix()}},ref,actual,*masterResult,cancel,[&](double p){progress(.5+p*.5);});}return result;},[safe,token](juce::Result result){
        if(!safe)return;safe->task=nullptr;if(token!=safe->generation)return;safe->revision=safe->owner.controlRevision();
        if(result.failed()){safe->text.setText(tr(result.getErrorMessage()),false);safe->release(safe->audio);safe->mixReport.reset();safe->masterReport.reset();return;}
        safe->targets.startA=safe->mixReport->previewStart;safe->revision=safe->owner.controlRevision();safe->changes=assistantGainChanges(safe->owner.project,*safe->mixReport);
        if(safe->master){auto& out=safe->changes;out.erase(std::remove_if(out.begin(),out.end(),[](const auto& c){return c.session==0;}),out.end());const auto& measured=safe->masterReport->files[0].mix;
            double delta=std::min(0.,safe->targets.ceilingDb-.05-juce::Decibels::gainToDecibels(safe->masterReport->files[0].truePeak,-120.));if(safe->targets.useLufs&&measured.lufs> -99)delta=std::min(delta,safe->targets.lufs-measured.lufs);
            if(delta< -.05&&safe->owner.project.masterGain>0){AssistantChange change;change.before=safe->owner.project.masterGain;change.after=change.before*juce::Decibels::decibelsToGain(delta);change.reason=tr("Suggested master attenuation; remeasure after applying.");out.insert(out.begin(),change);}}
        assistantEqChanges(safe->owner.project,*safe->mixReport,safe->changes);assistantCompressorChanges(safe->owner.audio,safe->owner.project,masterEffectTrack,safe->mixReport->mix,safe->changes);
        if(!safe->master)for(size_t i=0;i<safe->audio->tracks.size()&&i+1<safe->mixReport->inputLevels.size();++i)assistantCompressorChanges(safe->owner.audio,safe->owner.project,safe->audio->tracks[i],safe->mixReport->inputLevels[i+1],safe->changes);
        safe->list.updateContent();safe->present();safe->timerCallback();
    },{});
    if(!task){release(audio);text.setText(tr("Wait for the current operation to finish."),false);}
}
void ProjectAssistantPanel::prepareComparison(int side,bool applyAfter){
    if(task||owner.editingRestricted()||owner.pluginsBusy())return;owner.instrumentPanel.flushRender();
    if(!audio||stale||applied||owner.editingRestricted()||revision!=owner.controlRevision())return;
    Project candidate;auto result=assistantCandidate(owner.project,owner.audio,changes,candidate);if(result.failed()){text.setText(result.getErrorMessage(),false);return;}
    beforeAfter=std::make_shared<AssistantAudio>();beforeAfter->snapshot=std::move(candidate);const auto original=audio;const auto pair=std::make_shared<MasteringReport>();auto target=targets;target.startB=target.startA;const auto token=++generation;juce::Component::SafePointer<ProjectAssistantPanel> safe(this);
    task=owner.renderAssistantProject(beforeAfter,[original,pair,target](AssistantAudio& changed,const auto& cancel,const auto& progress){return analyseMastering({{original->mix()}},changed.mix(),target,*pair,cancel,progress);},[safe,token,pair,side,applyAfter](juce::Result result){
        if(!safe)return;safe->task=nullptr;if(token!=safe->generation)return;safe->revision=safe->owner.controlRevision();
        if(result.failed()||!pair->hasReference){safe->release(safe->beforeAfter);safe->text.setText(result.failed()?tr(result.getErrorMessage()):pair->comparisonNote,false);return;}
        safe->comparison=pair;safe->revision=safe->owner.controlRevision();safe->present();if(applyAfter)safe->applySelected();else safe->previewSide(side);
    },changes);
    if(!task){release(beforeAfter);text.setText(tr("Project changed; analyse again."),false);}
}
void ProjectAssistantPanel::previewSide(int side){
    if(task||stale||!masterReport||owner.editingRestricted()||revision!=owner.controlRevision())return;
    if(!referenceMode&&!comparison){prepareComparison(side,false);return;}
    const auto report=referenceMode?masterReport:comparison;
    if(!report||!report->comparisonReady)return;
    if(owner.audio.isPlaying()||owner.audio.isMonitoring()||std::abs(owner.audio.spectrumSampleRate()-report->comparisonRate)>.1){text.setText(tr("Stop playback/monitoring; device rate must match the project analysis."),false);return;}
    stopPreview();juce::AudioBuffer<float> clip;clip.makeCopyOf(side==0?report->a:report->b);owner.sampleBrowser.stopPreview();owner.audio.startPreview(std::move(clip),report->comparisonRate);preview=owner.audio.previewSerial();(side==0?a:b).setToggleState(true,juce::dontSendNotification);
}
void ProjectAssistantPanel::applySelected(){
    if(task||owner.editingRestricted()||owner.pluginsBusy())return;owner.instrumentPanel.flushRender();
    if(stale||applied||!audio||owner.editingRestricted()||revision!=owner.controlRevision()||std::none_of(changes.begin(),changes.end(),[](const auto& c){return c.selected;}))return;
    if(!comparison){prepareComparison(-1,true);return;}
    stopPreview();owner.audio.capturePluginStates(owner.project);Project candidate;auto result=assistantCandidate(owner.project,owner.audio,changes,candidate);if(result.failed()){text.setText(result.getErrorMessage(),false);return;}
    owner.checkpoint();owner.project=std::move(candidate);assistantParameters(owner.audio,owner.project,changes,true);owner.audio.invalidatePluginStates();owner.audio.capturePluginStates(owner.project);owner.mixChanged();revision=owner.controlRevision();applied=true;present();timerCallback();
}
void ProjectAssistantPanel::present(){
    b.setButtonText(tr(referenceMode?"B: reference":"B: suggestions"));r.setToggleState(referenceMode,juce::dontSendNotification);
    if(task){text.setText(tr("Project analysis is running; Cancel remains available in the progress window."),false);return;}
    juce::String value=tr("Project analysis uses the current instruments, effects, automation and routing.")+"\n"+tr("Reference optional; no WAV import is required.")+"\n";
    if(refFile!=juce::File())value+=tr("REFERENCE: ")+refFile.getFileName()+"\n";
    value+="\n"+tr(referenceMode?"A/B compares the project with the reference at matched volume.":"A/B compares original and selected settings at matched volume.")+"\n";
    if(stale)value+="\n"+tr("Project changed; analyse again.")+"\n";
    if(masterReport&&!masterReport->files.empty()){
        const auto& file=masterReport->files[0];value+="\n"+tr("Sample peak: ")+juce::String(juce::Decibels::gainToDecibels(file.mix.peak,-120.),2)+" dBFS / TP16 "+juce::String(juce::Decibels::gainToDecibels(file.truePeak,-120.),2)+" dBTP / LUFS "+juce::String(file.mix.lufs,2)+"\n";
        const auto externalStemInfo=tr("MEASURED: ")+"0"+tr(" supplied stems; missing stems and musical alignment cannot be inferred automatically.");
        for(const auto& line:masterReport->qa)if(line!=externalStemInfo)value+=line+"\n";
        value+="\n"+masterReport->suggestions.joinIntoString("\n")+"\n"+masterReport->comparisonNote+"\n";
        if(audio&&mixReport){value+="\n"+tr("Tracks analysed: ")+juce::String((int)audio->tracks.size())+" / "+tr("post-insert/fader, before groups/returns/master")+"\n";
            for(size_t i=0;i<audio->tracks.size()&&i+1<mixReport->inputLevels.size();++i){const auto& level=mixReport->inputLevels[i+1];value+=juce::String(audio->snapshot.tracks[(size_t)audio->tracks[i]].name)+" / "+tr("Sample peak: ")+juce::String(juce::Decibels::gainToDecibels(level.peak,-120.),1)+" dBFS / RMS "+juce::String(juce::Decibels::gainToDecibels(level.rms,-120.),1)+" dBFS\n";}}

    }
    if(comparison)value+="\n"+comparison->comparisonNote+"\n";
    if(comparison&&comparison->hasReference)value+="\n"+tr("Rendered proposal True Peak: ")+juce::String(juce::Decibels::gainToDecibels(comparison->reference.truePeak,-120.),2)+" dBTP / LUFS "+juce::String(comparison->reference.mix.lufs,2)+"\n";
    if(comparison&&comparison->hasReference&&comparison->reference.truePeak>juce::Decibels::decibelsToGain(targets.ceilingDb))value+=tr("Proposal remains above the selected ceiling; remeasure after Apply or retain Transparent export protection.")+"\n";

    value+="\n"+tr("Select numeric changes below. EQ/compression are listening proposals; automatic compressor edits require an existing non-automated native compressor.");
    if(applied)value+="\n\n"+tr("Suggestions applied. One Undo restores the previous settings.");text.setText(value,false);list.repaint();
}
void ProjectAssistantPanel::timerCallback(){
    if(!task&&audio&&revision!=owner.controlRevision()&&!stale){invalidate();}
    if(preview&&owner.audio.previewSerial()!=preview)preview=0;
    const bool busy=task||owner.editingRestricted()||owner.pluginsBusy();analyse.setEnabled(!busy);reference.setEnabled(!busy);useLufs.setEnabled(!busy);lufs.setEnabled(!busy&&useLufs.getToggleState());ceiling.setEnabled(!busy);list.setEnabled(!busy&&!stale&&!applied);
    const bool ready=!busy&&!stale&&audio&&masterReport;const bool selected=std::any_of(changes.begin(),changes.end(),[](const auto& c){return c.selected;});apply.setEnabled(ready&&!applied&&selected);a.setEnabled(ready&&(referenceMode?masterReport->comparisonReady:!comparison||comparison->comparisonReady));b.setEnabled(ready&&(referenceMode?masterReport->comparisonReady:(!comparison||comparison->comparisonReady)&&(selected||comparison)));r.setEnabled(ready&&masterReport->comparisonReady);stop.setEnabled(preview!=0);
}
void ProjectAssistantPanel::paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool){if(!juce::isPositiveAndBelow(row,(int)changes.size()))return;const auto& c=changes[(size_t)row];g.setColour(c.selected?Daw::returnA.withAlpha(.16f):Daw::surface);g.fillRect(0,0,width,height);g.setColour(Daw::text);g.drawRect(8,height/2-7,14,14);if(c.selected)g.fillRect(11,height/2-4,8,8);g.setFont(Daw::font(12));g.drawFittedText(description(owner.project,c),{30,2,width-36,height-4},juce::Justification::centredLeft,2,1.f);}
void ProjectAssistantPanel::listBoxItemClicked(int row,const juce::MouseEvent&){if(task||stale||applied||!juce::isPositiveAndBelow(row,(int)changes.size()))return;stopPreview();changes[(size_t)row].selected=!changes[(size_t)row].selected;comparison.reset();release(beforeAfter);list.repaint();timerCallback();}
void ProjectAssistantPanel::paint(juce::Graphics& g){g.fillAll(Daw::base);Daw::deviceHeader(g,{18,8,950,54},tr(master?"MASTERING ASSISTANT":"MIX ASSISTANT"),tr("CURRENT PROJECT / MEASURE / SELECT / APPLY"),master?Daw::returnB:Daw::returnA);}
void ProjectAssistantPanel::resized(){auto area=getLocalBounds().reduced(18);area.removeFromTop(60);auto row=area.removeFromTop(32);analyse.setBounds(row.removeFromLeft(180));row.removeFromLeft(8);reference.setBounds(row.removeFromLeft(165));row.removeFromLeft(12);if(master){useLufs.setBounds(row.removeFromLeft(140));lufs.setBounds(row.removeFromLeft(110));row.removeFromLeft(10);}ceiling.setBounds(row.removeFromLeft(170));area.removeFromTop(8);note.setBounds(area.removeFromTop(48));area.removeFromTop(8);auto bottom=area.removeFromBottom(34);for(auto* button:{&a,&b,&r,&stop,&apply}){button->setBounds(bottom.removeFromLeft(button==&apply?200:165));bottom.removeFromLeft(8);}area.removeFromBottom(8);list.setBounds(area.removeFromBottom(std::min(190,area.getHeight()/2)));area.removeFromBottom(8);text.setBounds(area);}
bool ProjectAssistantPanel::verify(const juce::File& folder){
#if !FGA_SELF_TESTS
    juce::ignoreUnused(folder);return false;
#else
    folder.createDirectory();close();owner.audio.capturePluginStates(owner.project);const auto languageBefore=Localisation::current();Localisation::setCurrent(Localisation::Language::italian);
    const auto saved=owner.project;const auto undos=owner.undoStack,redos=owner.redoStack;const bool dirty=owner.dirty;const int selected=owner.selectedTrack,section=owner.selectedSection,clip=owner.selectedClip;const double beat=owner.audio.position();
    juce::String checks;bool ok=true;auto check=[&](bool condition,const char* label){checks+=(condition?"PASS ":"FAIL ")+juce::String(label)+"\n";ok=condition&&ok;};
    auto wait=[&]{const auto until=juce::Time::getMillisecondCounterHiRes()+60000;while((task||owner.pluginsBusy())&&juce::Time::getMillisecondCounterHiRes()<until)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);return !task&&!owner.pluginsBusy()&&!owner.editingRestricted()&&!owner.audio.isExporting();};
    owner.project=newProject(731);owner.project.sections={{Section::main,1,"Assistant fixture"}};owner.project.settings.bpm=120;owner.project.normalise();owner.project.masterGain=2;
    for(auto& t:owner.project.tracks){t.mute=true;t.group=-1;t.sends={};t.parameters.clear();for(auto& c:t.clips)c.notes.clear();}
    for(int ti:{0,1,4}){auto& t=owner.project.tracks[(size_t)ti];t.mute=false;t.gain=2;t.clips[0].notes={{0,1.8,ti==1?36:ti==4?36:69,127,0}};}owner.project.tracks[1].group=0;owner.project.tracks[1].sends={.2,.1};owner.project.buses[0].sidechainAmount=.3;
    owner.restoreInstruments();check(wait(),"prepare authored project without external WAV input");owner.addNativeCompressor(masterEffectTrack);check(wait(),"reuse existing native compressor insert");owner.closePluginEditor();owner.audio.capturePluginStates(owner.project);owner.refreshControls();
    const auto before=juce::JSON::toString(projectJson(owner.project));const auto history=owner.undoStack.size();auto* native=owner.audio.effectPlugin(masterEffectTrack,1);const double ratio=native?native->getParameters()[1]->getValue():0;
    renderProject();check(task&&task->getAlertWindow()->getName()=="Analizza progetto","Italian project progress window title");check(wait()&&audio&&masterReport&&!changes.empty(),"analyse project directly with routing and concrete suggestions");
    check(before==juce::JSON::toString(projectJson(owner.project))&&owner.undoStack.size()==history,"analysis preserves project and Undo");
    check(analyse.getButtonText()=="Analizza progetto"&&apply.getButtonText()=="Applica suggerimenti"&&text.getText().startsWith(juce::String::fromUTF8("L’analisi usa strumenti")),"Italian project controls and actual analysis report");
    const auto referenceFile=folder.getChildFile("reference.wav");referenceFile.deleteFile();check(audio&&audio->mix().copyFileTo(referenceFile),"prepare optional reference from captured fixture");refFile=referenceFile;renderProject();check(wait()&&masterReport&&masterReport->comparisonReady,"project analysis supports optional reference comparison");
    bool hasCompressor=false;for(auto& change:changes){change.selected=true;hasCompressor=hasCompressor||change.kind==AssistantChange::compressor;}
    check(hasCompressor,"existing native compressor has selectable numeric settings");
    prepareComparison(1,false);check(wait()&&comparison&&comparison->comparisonReady,"render selected proposal for normalized A/B");
    folder.getChildFile("proposal-diagnostics.txt").replaceWithText(text.getText()+"\nINFO stale="+juce::String((int)stale)+" revision="+juce::String((juce::int64)revision)+" owner="+juce::String((juce::int64)owner.controlRevision()));
    check(before==juce::JSON::toString(projectJson(owner.project))&&owner.undoStack.size()==history&&native&&std::abs(native->getParameters()[1]->getValue()-ratio)<1e-6,"proposal render restores native parameters and leaves project/Undo untouched");
    if(comparison&&comparison->comparisonReady){
        bool written=true;for(int side=0;side<2;++side){const auto file=folder.getChildFile(side?"proposal-B.wav":"proposal-A.wav");file.deleteFile();auto output=file.createOutputStream();juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(output.get(),comparison->comparisonRate,2,24,{},0));if(!writer){written=false;break;}output.release();const auto& signal=side?comparison->b:comparison->a;written=writer->writeFromAudioSampleBuffer(signal,0,signal.getNumSamples())&&writer->flush()&&written;}
        check(written,"retain actual normalized proposal audio for independent QA");
    }
    previewSide(0);check(preview&&owner.audio.isPreviewing(),"A auditions original using existing player");previewSide(1);check(preview&&owner.audio.isPreviewing(),"B auditions proposed settings using existing player");r.onClick();b.onClick();check(referenceMode&&preview&&owner.audio.isPreviewing(),"reference mode auditions its own matched A/B pair");r.onClick();stopPreview();
    applySelected();const auto after=juce::JSON::toString(projectJson(owner.project));check(applied&&owner.undoStack.size()==std::min(size_t(40),history+1)&&after!=before,"Apply selected settings creates exactly one Undo");
    check(native&&std::abs(native->getParameters()[1]->getValue()-ratio)>1e-5,"Apply changes actual native compressor parameters");
    owner.undo(false);check(juce::JSON::toString(projectJson(owner.project))==before&&std::abs(owner.audio.effectPlugin(masterEffectTrack,1)->getParameters()[1]->getValue()-ratio)<1e-6,"one Undo restores project and actual native settings");
    owner.undo(true);check(juce::JSON::toString(projectJson(owner.project))==after,"Redo restores complete settings batch");
    renderProject();const auto cancelled=audio?audio->directory:juce::File();if(task)task->getAlertWindow()->getButton(0)->onClick();check(wait()&&!audio,"actual Cancel restores editing and audio ownership");
    const auto until=juce::Time::getMillisecondCounterHiRes()+3000;while(cancelled.exists()&&juce::Time::getMillisecondCounterHiRes()<until)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);check(!cancelled.exists(),"cancelled analysis temporary files are removed");
    renderProject();check(wait()&&masterReport,"analysis retries after cancellation");
    if(auto stream=folder.getChildFile(master?"master-project.png":"mix-project.png").createOutputStream())juce::PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()),*stream);
    owner.refreshControls();timerCallback();check(stale&&changes.empty()&&!preview,"project edit invalidates plans and temporary audition");
    renderProject();close();check(wait(),"close cancels running project worker cooperatively");
    owner.project=saved;owner.restoreInstruments();check(wait(),"restore original instruments/project after fixture");owner.undoStack=undos;owner.redoStack=redos;owner.dirty=dirty;owner.selectedTrack=selected;owner.selectedSection=section;owner.selectedClip=clip;owner.audio.seek(beat);owner.refreshControls();
    Localisation::setCurrent(languageBefore);
    folder.getChildFile("project-assistant.txt").replaceWithText(checks);return ok;
#endif
}

}
