// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MixAssistantWindow.h"
#include "AssistantProject.h"
#include "MainComponent.h"
#include "MixAnalysis.h"
#include "AssistantUi.h"
#include "RenderProgressTask.h"
namespace fga {
class MixAssistantWindow::Panel:public juce::Component,private juce::Timer {
 struct Job:RenderProgressTask {
  std::vector<MixInput> inputs;MixReport report;const Localisation::Language language=Localisation::current();
  juce::Result result=juce::Result::fail(tr("Analysis did not complete."));std::function<void()> done;
  explicit Job(std::vector<MixInput> i):RenderProgressTask("Mix Assistant / WAV"),inputs(std::move(i)){}
  void run()override{
   try {result=analyseMixFiles(inputs,report,[this]{return threadShouldExit();},[this](double p){setProgress(p);});}
   catch(const std::exception& e){result=juce::Result::fail(tr("Analysis failed: ")+juce::String(e.what()));}
   catch(...){result=juce::Result::fail(tr("Analysis failed unexpectedly."));}
  }
  void threadComplete(bool cancelled)override{
   if(cancelled)result=juce::Result::fail(tr("Analysis cancelled."));
   if(failedToStart())result=juce::Result::fail(tr("Cannot start analysis worker."));
   if(done)done();
  }
 };

 MainComponent& owner;std::unique_ptr<Job> job;std::vector<MixInput> inputs;MixReport report;std::vector<bool> applied;
 juce::TextButton mix{"Mix WAV"},stem{"Add stem WAV"},clear{"Clear"},analyse{"Analyse"},a{"A: project"},b{"B: suggestion"},apply{"Apply selected suggestion"},eq{"Open existing EQ"};
 juce::Rectangle<int> metrics;int shownStep=-99;juce::ComboBox track,choice;juce::TextEditor text;juce::Label note;std::unique_ptr<juce::FileChooser> chooser;bool preview=false;std::uint64_t previewRevision=0,analysisRevision=0;
 Localisation::Language language=Localisation::current();
 int selected()const{return choice.getSelectedId()-1;}
 void restore(){if(preview){owner.audio.updateMix(owner.project);preview=false;}b.setToggleState(false,juce::dontSendNotification);}
 int target(std::uint64_t session)const{for(size_t i=0;i<owner.project.tracks.size();++i)if(owner.project.tracks[i].sessionId==session)return (int)i;return -1;}
 bool eligible()const{
  const int i=selected();
  if(job||i<0||i>=(int)report.suggestions.size()||i>=(int)applied.size()||applied[(size_t)i]||owner.editingRestricted()||owner.loadingPlugin||analysisRevision!=owner.controlRevision())return false;
  const auto& s=report.suggestions[(size_t)i];if(s.trimDb>=0)return false;
  const int ti=s.session==0?-1:target(s.session);if(s.session!=0&&ti<0)return false;
  return (ti<0?owner.project.masterGain:owner.project.tracks[(size_t)ti].gain)>0;
 }
 bool candidate(Project& p){
  if(!eligible())return false;const auto& s=report.suggestions[(size_t)selected()];const double factor=juce::Decibels::decibelsToGain(s.trimDb);
  if(s.session==0)p.masterGain*=factor;else p.tracks[(size_t)target(s.session)].gain*=factor;return true;
 }
 void choose(bool master){if(job)return;restore();const int ti=track.getSelectedId()-1;if(!master&&!juce::isPositiveAndBelow(ti,(int)owner.project.tracks.size()))return;const auto session=master?0:owner.project.tracks[(size_t)ti].sessionId;const int kind=master?-1:owner.project.tracks[(size_t)ti].kind;const int role=master?0:kind==4?1:kind==1?2:3;const auto revision=owner.controlRevision();
  chooser=std::make_unique<juce::FileChooser>(tr("Aligned WAV, same sample rate"),juce::File(),"*.wav");juce::Component::SafePointer<Panel> safe(this);chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe,master,session,role,revision](const juce::FileChooser& fc){if(!safe||fc.getResult()==juce::File())return;if(safe->owner.controlRevision()!=revision){safe->show(tr("Project changed; choose the WAV again."));return;}auto& v=safe->inputs;MixInput input{fc.getResult(),session,role};if(master){if(v.empty())v.push_back(input);else v[0]=input;}else{if(v.empty()){safe->show(tr("Choose mix WAV first."));return;}auto it=std::find_if(v.begin()+1,v.end(),[&](const MixInput& x){return x.session==session;});if(it!=v.end())*it=input;else if(v.size()<33)v.push_back(input);else{safe->show(tr("At most 32 stems per analysis; Clear to start again."));return;}}safe->report={};safe->choice.clear();safe->showFiles();});}
 void show(const juce::String& s){assistantui::showReport(text,s);repaint();}
 void showFiles(){juce::String s=tr("LOCAL WAV REFERENCE FILES / no project audio uploaded\n\n");for(const auto& i:inputs)s+=(i.role==0?"MIX":i.role==1?tr("KICK"):i.role==2?tr("BASS"):"STEM")+juce::String("  ")+i.file.getFullPathName()+"\n";show(s);}
 void analyseFiles(){if(job||inputs.empty())return;restore();analysisRevision=owner.controlRevision();report={};applied.clear();choice.clear();job=std::make_unique<Job>(inputs);juce::Component::SafePointer<Panel> safe(this);job->done=[safe]{if(!safe)return;auto& self=*safe;if(self.job->language!=Localisation::current()){self.show(tr("Language changed; analyse again."));self.report={};self.choice.clear();}else if(self.job->result.failed()){self.show(self.job->result.getErrorMessage());self.report={};self.choice.clear();}else{self.report=self.job->report;self.applied.assign(self.report.suggestions.size(),false);self.present();}self.owner.retiredJobs.retire(std::move(self.job));};job->launchThread();}
 juce::String label(const MixAdvice& s)const{
  if(s.trimDb==0)return s.text;
  const int ti=s.session==0?-1:target(s.session);
  const auto name=s.session==0?juce::String("MASTER"):ti>=0?juce::String(owner.project.tracks[(size_t)ti].name):juce::String(tr("Missing track"));
  return name+" / "+juce::String(s.trimDb,2)+juce::String::fromUTF8(" dB — ")+s.text;
 }
 void present(){choice.clear(juce::dontSendNotification);auto& m=report.mix;juce::String s=tr("REFERENCE WAV MEASUREMENTS (not live project analysis)\n");s+=tr("Sample peak: ")+juce::String(juce::Decibels::gainToDecibels(m.peak,-120.),2)+tr(" dBFS / headroom: ")+juce::String(-juce::Decibels::gainToDecibels(m.peak,-120.),2)+tr(" dB\nFull-scale samples: ")+juce::String((juce::int64)m.fullScale)+tr("\nStereo correlation: ")+(m.correlationValid?juce::String(m.correlation,3):tr("undefined / mono or silence"))+tr("\nIntegrated LUFS: ")+(m.lufs> -99?juce::String(m.lufs,1):tr("undefined / silence or insufficient duration"))+tr(juce::String::fromUTF8("\n\nSUGGESTIONS — listening hypotheses, not aesthetic judgments\n"));for(size_t i=0;i<report.suggestions.size();++i){choice.addItem(juce::String((int)i+1)+" / "+label(report.suggestions[i]),(int)i+1);s+=juce::String((int)i+1)+". "+label(report.suggestions[i])+"\n\n";}if(report.suggestions.empty())s+=tr("No threshold-based suggestion. This does not certify mix quality.");show(s);if(choice.getNumItems())choice.setSelectedId(1);}
 int step()const{return report.inputLevels.empty()?-1:preview?3:std::any_of(applied.begin(),applied.end(),[](bool x){return x;})?2:1;}
 void timerCallback()override{if(const int now=step()*2+(job!=nullptr);now!=shownStep){shownStep=now;repaint();}apply.setTooltip(analysisRevision!=owner.controlRevision()?tr("Project changed: analyse again before applying suggestions."):tr("Apply this suggestion only; Undo restores its previous gain."));if(preview&&(previewRevision!=owner.controlRevision()||owner.editingRestricted()))restore();const bool busy=job!=nullptr;mix.setEnabled(!busy);stem.setEnabled(!busy);clear.setEnabled(!busy);analyse.setEnabled(!busy&&!inputs.empty());const bool valid=eligible();b.setEnabled(valid);apply.setEnabled(valid);}
public:
 explicit Panel(MainComponent& m):owner(m){for(auto* c:std::initializer_list<juce::Component*>{&mix,&stem,&clear,&analyse,&a,&b,&apply,&eq,&track,&choice,&text,&note})addAndMakeVisible(c);assistantui::styleReport(text);assistantui::stylePrimary(analyse);text.setMultiLine(true);text.setReadOnly(true);text.setScrollbarsShown(true);note.setText("Choose original WAV mix + time-aligned stems (same rate, max 15 min). Kick/Bass roles follow the selected internal tracks. A/B is temporary; Apply creates one Undo. No automatic EQ.",juce::dontSendNotification);note.setFont(Daw::font(12));note.setMinimumHorizontalScale(1.f);note.setJustificationType(juce::Justification::topLeft);track.setScrollWheelEnabled(false);choice.setScrollWheelEnabled(false);
  mix.onClick=[this]{choose(true);};stem.onClick=[this]{choose(false);};clear.onClick=[this]{if(job)return;restore();inputs.clear();report={};applied.clear();choice.clear();showFiles();};analyse.onClick=[this]{analyseFiles();};choice.onChange=[this]{restore();};a.onClick=[this]{restore();};b.onClick=[this]{restore();auto p=owner.project;if(candidate(p)){owner.audio.updateMix(p);preview=true;previewRevision=owner.controlRevision();b.setToggleState(true,juce::dontSendNotification);}};
  apply.onClick=[this]{restore();auto p=owner.project;if(!candidate(p))return;const int i=selected();owner.checkpointMix();const auto& s=report.suggestions[(size_t)i];if(s.session==0)owner.project.masterGain=p.masterGain;else{const int ti=target(s.session);owner.project.tracks[(size_t)ti].gain=p.tracks[(size_t)ti].gain;}applied[(size_t)i]=true;owner.mixChanged();analysisRevision=owner.controlRevision();};eq.onClick=[this]{restore();owner.openGraphicEq(track.getSelectedId()-1);};
  for(auto* button:std::initializer_list<juce::Button*>{&mix,&stem,&clear,&analyse,&a,&b,&apply,&eq})Localisation::bindText(*button,button->getButtonText());
  Localisation::bindLabel(note,note.getText(),false);
  Localisation::bind(*this,[this]{if(language!=Localisation::current()){language=Localisation::current();restore();report={};applied.clear();choice.clear();if(job)job->requestCancel();showFiles();}repaint();});
  setSize(1000,660);showFiles();startTimer(100);
 }
 ~Panel()override{stopTimer();close();}
 void open(){track.clear(juce::dontSendNotification);for(size_t i=0;i<owner.project.tracks.size();++i)track.addItem(juce::String(owner.project.tracks[i].name),(int)i+1);track.setSelectedId(owner.selectedTrack+1);}
 void close(){restore();if(job){job->done=nullptr;job->requestCancel();owner.retiredJobs.retire(std::move(job));show(tr("Analysis cancelled."));}}
 void paint(juce::Graphics& g)override{g.fillAll(Daw::base);Daw::deviceHeader(g,{18,8,420,54},tr("MIX ASSISTANT"),tr("LOCAL / SUGGESTIONS"),Daw::returnA);
  const bool analysed=!report.inputLevels.empty();assistantui::drawSteps(g,{getWidth()-460,24,442,22},step(),job!=nullptr);
  const auto& m=report.mix;assistantui::drawMetrics(g,metrics,analysed?assistantui::mixMetrics(m.lufs,m.truePeak,m.peak,m.rms,m.correlationValid,m.correlation,-3.):std::vector<assistantui::Metric>{},tr("Choose a mix WAV and analyse: measured loudness, peaks and stereo appear here."));}
 void resized()override{auto r=getLocalBounds().reduced(18);r.removeFromTop(58);auto row=r.removeFromTop(30);mix.setBounds(row.removeFromLeft(110));row.removeFromLeft(8);track.setBounds(row.removeFromLeft(190));row.removeFromLeft(8);stem.setBounds(row.removeFromLeft(130));row.removeFromLeft(8);clear.setBounds(row.removeFromLeft(70));row.removeFromLeft(8);analyse.setBounds(row.removeFromLeft(110));r.removeFromTop(8);note.setBounds(r.removeFromTop(40));metrics=r.removeFromTop(78);r.removeFromTop(10);auto bottom=r.removeFromBottom(36);a.setBounds(bottom.removeFromLeft(110));bottom.removeFromLeft(8);b.setBounds(bottom.removeFromLeft(140));bottom.removeFromLeft(8);apply.setBounds(bottom.removeFromLeft(230));bottom.removeFromLeft(8);eq.setBounds(bottom.removeFromLeft(170));r.removeFromBottom(8);choice.setBounds(r.removeFromBottom(30));r.removeFromBottom(8);text.setBounds(r);}
 bool verify(const juce::File& folder){
#if !FGA_SELF_TESTS
  juce::ignoreUnused(folder);return false;
#else
  const auto languageBefore=Localisation::current();bool localized=true;
  Localisation::setCurrent(Localisation::Language::italian);
  {
   Panel initiallyItalian(owner);localized=initiallyItalian.analyse.getButtonText()=="Analizza"&&initiallyItalian.note.getText().startsWith("Seleziona il WAV");
   Localisation::setCurrent(Localisation::Language::english);localized=localized&&initiallyItalian.analyse.getButtonText()=="Analyse"&&initiallyItalian.note.getText().startsWith("Choose original WAV mix");
  }

  const auto saved=owner.project;const auto undos=owner.undoStack,redos=owner.redoStack;const auto dirty=owner.dirty;
  const auto lastControl=owner.lastCheckpointControl;const auto lastTime=owner.lastCheckpointTime;
  const auto before=juce::JSON::toString(projectJson(saved));
  report.suggestions={{"Test trim",0,-3}};applied={false};analysisRevision=owner.controlRevision();present();
  b.onClick();bool ok=preview&&before==juce::JSON::toString(projectJson(owner.project))&&owner.dirty==dirty&&owner.undoStack.size()==undos.size();
  a.onClick();ok=ok&&!preview;apply.onClick();ok=ok&&owner.undoStack.size()==std::min((size_t)40,undos.size()+1)&&owner.project.masterGain<saved.masterGain&&!eligible();
  owner.undo(false);ok=ok&&before==juce::JSON::toString(projectJson(owner.project));
  owner.undo(true);ok=ok&&std::abs(owner.project.masterGain-saved.masterGain*juce::Decibels::decibelsToGain(-3.))<1e-9;owner.undo(false);
  report.suggestions={{"Test track trim",owner.project.tracks[1].sessionId,-1.5}};applied={false};analysisRevision=owner.controlRevision();present();
  apply.onClick();ok=ok&&owner.project.tracks[1].gain<saved.tracks[1].gain&&owner.project.masterGain==saved.masterGain;owner.undo(false);
  ok=ok&&before==juce::JSON::toString(projectJson(owner.project));
  applied={false};analysisRevision=owner.controlRevision();b.onClick();owner.refreshControls();timerCallback();ok=ok&&!preview&&!eligible();
  owner.undoStack=undos;owner.redoStack=redos;owner.lastCheckpointControl=lastControl;owner.lastCheckpointTime=lastTime;owner.dirty=dirty;
  // Real asynchronous analysis and the actual cooperative Cancel button; no audio device needed.
  auto fixture=folder.getChildFile("mix-reference.wav");fixture.deleteFile();juce::WavAudioFormat format;
  {std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(fixture.createOutputStream().release(),48000,2,16,{},0));if(!writer)return false;juce::AudioBuffer<float> buffer(2,4096);for(int n=0;n<4096;++n){const float v=.8f*std::sin((float)n*.13f);buffer.setSample(0,n,v);buffer.setSample(1,n,v);}for(int n=0;n<256;++n)ok=writer->writeFromAudioSampleBuffer(buffer,0,4096)&&ok;}
  auto wait=[&]{const auto deadline=juce::Time::getMillisecondCounter()+10000;while(job&&juce::Time::getMillisecondCounter()<deadline)juce::MessageManager::getInstance()->runDispatchLoopUntil(10);return !job;};
  inputs={{fixture}};analyseFiles();ok=ok&&job!=nullptr;if(job)job->getAlertWindow()->getButton(0)->onClick();ok=wait()&&ok;ok=ok&&report.suggestions.empty();
  analyseFiles();ok=wait()&&ok;ok=ok&&report.mix.peak>.79&&!report.suggestions.empty();
  {auto image=createComponentSnapshot(getLocalBounds());if(auto stream=folder.getChildFile("mix-assistant.png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(image,*stream)&&ok;}}
  Localisation::setCurrent(Localisation::Language::italian);
  localized=localized&&analyse.getButtonText()=="Analizza"&&apply.getButtonText()=="Applica suggerimento scelto"&&getTopLevelComponent()->getName()=="Assistente mix"&&report.suggestions.empty();
  analyseFiles();localized=wait()&&localized;localized=localized&&text.getText().contains("MISURE DEI WAV")&&text.getText().contains("Riduzione del master")&&text.getText().contains("SUGGERIMENTI")&&!text.getText().contains("SUGGESTIONS");
  folder.getChildFile("mix-assistant-it-content.txt").replaceWithText(text.getText());
  {if(auto stream=folder.getChildFile("mix-assistant-it.png").createOutputStream())juce::PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()),*stream);}
  analyseFiles();Localisation::setCurrent(Localisation::Language::english);localized=wait()&&localized;localized=localized&&analyse.getButtonText()=="Analyse"&&report.suggestions.empty()&&text.getText().contains("Language changed; analyse again.");
  inputs={{folder.getChildFile("missing-mix.wav")}};analyseFiles();ok=wait()&&ok;ok=ok&&report.suggestions.empty()&&!eligible();inputs={{fixture}};
  // Closing an in-flight job severs its callback before retirement; reopening is safe.
  analyseFiles();close();ok=ok&&!job;inputs.clear();report={};applied.clear();choice.clear();showFiles();
  Localisation::setCurrent(languageBefore);
  return localized&&ok&&before==juce::JSON::toString(projectJson(owner.project));
#endif
 }

};
MixAssistantWindow::MixAssistantWindow(MainComponent& m):DocumentWindow(tr("Mix Assistant"),Daw::base,juce::DocumentWindow::closeButton),owner(m){setLookAndFeel(&m.theme);setUsingNativeTitleBar(true);panel=new Panel(m);projectPanel=new ProjectAssistantPanel(m,false);tabs=new juce::TabbedComponent(juce::TabbedButtonBar::TabsAtTop);tabs->addTab(tr("Project"),Daw::base,projectPanel,true);tabs->addTab(tr("WAV files"),Daw::base,panel,true);Localisation::bind(*tabs,[this]{tabs->setTabName(0,tr("Project"));tabs->setTabName(1,tr("WAV files"));});setContentOwned(tabs,true);Localisation::bind(*this,[this]{setName(tr("Mix Assistant"));});setResizable(true,false);setResizeLimits(1050,700,1800,1200);}
MixAssistantWindow::~MixAssistantWindow(){clearContentComponent();setLookAndFeel(nullptr);}
void MixAssistantWindow::open(){tabs->setCurrentTabIndex(0);projectPanel->open();panel->open();centreWithSize(1100,780);setVisible(true);toFront(true);}
void MixAssistantWindow::closeButtonPressed(){projectPanel->close();panel->close();setVisible(false);}
bool MixAssistantWindow::demo(const juce::File& f){tabs->setCurrentTabIndex(0);return projectPanel->demo(f);}
bool MixAssistantWindow::verify(const juce::File& folder){tabs->setCurrentTabIndex(1);const bool old=panel->verify(folder);tabs->setCurrentTabIndex(0);const bool ok=projectPanel->verify(folder.getChildFile("project"))&&old;folder.getChildFile("mix-assistant-ui.txt").replaceWithText(ok?"PASS preview A/B leaves project/Undo unchanged; explicit Apply and Undo/Redo; project-edit invalidation, real worker Cancel/retry/close":"FAIL Mix Assistant UI");return ok;}
void MainComponent::showMixAssistant(){if(editingRestricted()||loadingPlugin)return;if(!mixAssistantWindow)mixAssistantWindow=std::make_unique<MixAssistantWindow>(*this);mixAssistantWindow->open();}
}
