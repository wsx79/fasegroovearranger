// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MixAssistantWindow.h"
#include "MainComponent.h"
#include "MixAnalysis.h"
#include "RenderProgressTask.h"
namespace fga {
class MixAssistantWindow::Panel:public juce::Component,private juce::Timer {
 struct Job:RenderProgressTask {
  std::vector<MixInput> inputs;MixReport report;
  juce::Result result=juce::Result::fail("Analysis did not complete.");std::function<void()> done;
  explicit Job(std::vector<MixInput> i):RenderProgressTask("Mix Assistant / WAV"),inputs(std::move(i)){}
  void run()override{
   try {result=analyseMixFiles(inputs,report,[this]{return threadShouldExit();},[this](double p){setProgress(p);});}
   catch(const std::exception& e){result=juce::Result::fail("Analysis failed: "+juce::String(e.what()));}
   catch(...){result=juce::Result::fail("Analysis failed unexpectedly.");}
  }
  void threadComplete(bool cancelled)override{
   if(cancelled)result=juce::Result::fail("Analysis cancelled.");
   if(failedToStart())result=juce::Result::fail("Cannot start analysis worker.");
   if(done)done();
  }
 };

 MainComponent& owner;std::unique_ptr<Job> job;std::vector<MixInput> inputs;MixReport report;std::vector<bool> applied;
 juce::TextButton mix{"Mix WAV"},stem{"Add stem WAV"},clear{"Clear"},analyse{"Analyse"},a{"A: project"},b{"B: suggestion"},apply{"Apply selected suggestion"},eq{"Open existing EQ"};
 juce::ComboBox track,choice;juce::TextEditor text;juce::Label note;std::unique_ptr<juce::FileChooser> chooser;bool preview=false;std::uint64_t previewRevision=0,analysisRevision=0;
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
  chooser=std::make_unique<juce::FileChooser>("Aligned WAV, same sample rate",juce::File(),"*.wav");juce::Component::SafePointer<Panel> safe(this);chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe,master,session,role,revision](const juce::FileChooser& fc){if(!safe||fc.getResult()==juce::File())return;if(safe->owner.controlRevision()!=revision){safe->show("Project changed; choose the WAV again.");return;}auto& v=safe->inputs;MixInput input{fc.getResult(),session,role};if(master){if(v.empty())v.push_back(input);else v[0]=input;}else{if(v.empty()){safe->show("Choose mix WAV first.");return;}auto it=std::find_if(v.begin()+1,v.end(),[&](const MixInput& x){return x.session==session;});if(it!=v.end())*it=input;else if(v.size()<33)v.push_back(input);else{safe->show("At most 32 stems per analysis; Clear to start again.");return;}}safe->report={};safe->choice.clear();safe->showFiles();});}
 void show(const juce::String& s){text.setText(s,false);}
 void showFiles(){juce::String s="LOCAL WAV REFERENCE FILES / no project audio uploaded\n\n";for(const auto& i:inputs)s+=(i.role==0?"MIX":i.role==1?"KICK":i.role==2?"BASS":"STEM")+juce::String("  ")+i.file.getFullPathName()+"\n";show(s);}
 void analyseFiles(){if(job||inputs.empty())return;restore();analysisRevision=owner.controlRevision();report={};applied.clear();choice.clear();job=std::make_unique<Job>(inputs);juce::Component::SafePointer<Panel> safe(this);job->done=[safe]{if(!safe)return;auto& self=*safe;if(self.job->result.failed()){self.show(self.job->result.getErrorMessage());self.report={};self.choice.clear();}else{self.report=self.job->report;self.applied.assign(self.report.suggestions.size(),false);self.present();}self.owner.retiredJobs.retire(std::move(self.job));};job->launchThread();}
 juce::String label(const MixAdvice& s)const{
  if(s.trimDb==0)return s.text;
  const int ti=s.session==0?-1:target(s.session);
  const auto name=s.session==0?juce::String("MASTER"):ti>=0?juce::String(owner.project.tracks[(size_t)ti].name):juce::String("Missing track");
  return name+" / "+juce::String(s.trimDb,2)+" dB — "+s.text;
 }
 void present(){choice.clear(juce::dontSendNotification);auto& m=report.mix;juce::String s="REFERENCE WAV MEASUREMENTS (not live project analysis)\n";s+="Sample peak: "+juce::String(juce::Decibels::gainToDecibels(m.peak,-120.),2)+" dBFS / headroom: "+juce::String(-juce::Decibels::gainToDecibels(m.peak,-120.),2)+" dB\nFull-scale samples: "+juce::String((juce::int64)m.fullScale)+"\nStereo correlation: "+(m.correlationValid?juce::String(m.correlation,3):"undefined / mono or silence")+"\nIntegrated LUFS: "+(m.lufs> -99?juce::String(m.lufs,1):"undefined / silence or insufficient duration")+"\n\nSUGGESTIONS — listening hypotheses, not aesthetic judgments\n";for(size_t i=0;i<report.suggestions.size();++i){choice.addItem(juce::String((int)i+1)+" / "+label(report.suggestions[i]),(int)i+1);s+=juce::String((int)i+1)+". "+label(report.suggestions[i])+"\n\n";}if(report.suggestions.empty())s+="No threshold-based suggestion. This does not certify mix quality.";show(s);if(choice.getNumItems())choice.setSelectedId(1);}
 void timerCallback()override{apply.setTooltip(analysisRevision!=owner.controlRevision()?"Project changed: analyse again before applying suggestions.":"Apply this suggestion only; Undo restores its previous gain.");if(preview&&(previewRevision!=owner.controlRevision()||owner.editingRestricted()))restore();const bool busy=job!=nullptr;mix.setEnabled(!busy);stem.setEnabled(!busy);clear.setEnabled(!busy);analyse.setEnabled(!busy&&!inputs.empty());const bool valid=eligible();b.setEnabled(valid);apply.setEnabled(valid);}
public:
 explicit Panel(MainComponent& m):owner(m){for(auto* c:std::initializer_list<juce::Component*>{&mix,&stem,&clear,&analyse,&a,&b,&apply,&eq,&track,&choice,&text,&note})addAndMakeVisible(c);text.setMultiLine(true);text.setReadOnly(true);text.setScrollbarsShown(true);note.setText("Choose original WAV mix + time-aligned stems (same rate, max 15 min). Kick/Bass roles follow the selected internal tracks. A/B is temporary; Apply creates one Undo. No automatic EQ.",juce::dontSendNotification);note.setFont(Daw::font(12));track.setScrollWheelEnabled(false);choice.setScrollWheelEnabled(false);
  mix.onClick=[this]{choose(true);};stem.onClick=[this]{choose(false);};clear.onClick=[this]{if(job)return;restore();inputs.clear();report={};applied.clear();choice.clear();showFiles();};analyse.onClick=[this]{analyseFiles();};choice.onChange=[this]{restore();};a.onClick=[this]{restore();};b.onClick=[this]{restore();auto p=owner.project;if(candidate(p)){owner.audio.updateMix(p);preview=true;previewRevision=owner.controlRevision();b.setToggleState(true,juce::dontSendNotification);}};
  apply.onClick=[this]{restore();auto p=owner.project;if(!candidate(p))return;const int i=selected();owner.checkpointMix();const auto& s=report.suggestions[(size_t)i];if(s.session==0)owner.project.masterGain=p.masterGain;else{const int ti=target(s.session);owner.project.tracks[(size_t)ti].gain=p.tracks[(size_t)ti].gain;}applied[(size_t)i]=true;owner.mixChanged();analysisRevision=owner.controlRevision();};eq.onClick=[this]{restore();owner.openGraphicEq(track.getSelectedId()-1);};setSize(1000,660);showFiles();startTimer(100);
 }
 ~Panel()override{stopTimer();close();}
 void open(){track.clear(juce::dontSendNotification);for(size_t i=0;i<owner.project.tracks.size();++i)track.addItem(juce::String(owner.project.tracks[i].name),(int)i+1);track.setSelectedId(owner.selectedTrack+1);}
 void close(){restore();if(job){job->done=nullptr;job->requestCancel();owner.retiredJobs.retire(std::move(job));show("Analysis cancelled.");}}
 void paint(juce::Graphics& g)override{g.fillAll(Daw::base);Daw::deviceHeader(g,{18,8,700,54},"MIX ASSISTANT","LOCAL / SUGGESTIONS",Daw::returnA);}
 void resized()override{auto r=getLocalBounds().reduced(18);r.removeFromTop(58);auto row=r.removeFromTop(30);mix.setBounds(row.removeFromLeft(110));row.removeFromLeft(8);track.setBounds(row.removeFromLeft(190));row.removeFromLeft(8);stem.setBounds(row.removeFromLeft(130));row.removeFromLeft(8);clear.setBounds(row.removeFromLeft(70));row.removeFromLeft(8);analyse.setBounds(row.removeFromLeft(110));r.removeFromTop(8);note.setBounds(r.removeFromTop(48));auto bottom=r.removeFromBottom(36);a.setBounds(bottom.removeFromLeft(110));bottom.removeFromLeft(8);b.setBounds(bottom.removeFromLeft(140));bottom.removeFromLeft(8);apply.setBounds(bottom.removeFromLeft(230));bottom.removeFromLeft(8);eq.setBounds(bottom.removeFromLeft(170));r.removeFromBottom(8);choice.setBounds(r.removeFromBottom(30));r.removeFromBottom(8);text.setBounds(r);}
 bool verify(const juce::File& folder){
#if !FGA_SELF_TESTS
  juce::ignoreUnused(folder);return false;
#else
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
  inputs={{folder.getChildFile("missing-mix.wav")}};analyseFiles();ok=wait()&&ok;ok=ok&&report.suggestions.empty()&&!eligible();inputs={{fixture}};
  // Closing an in-flight job severs its callback before retirement; reopening is safe.
  analyseFiles();close();ok=ok&&!job;inputs.clear();report={};applied.clear();choice.clear();showFiles();
  return ok&&before==juce::JSON::toString(projectJson(owner.project));
#endif
 }

};
MixAssistantWindow::MixAssistantWindow(MainComponent& m):DocumentWindow("Mix Assistant",Daw::base,juce::DocumentWindow::closeButton),owner(m){setLookAndFeel(&m.theme);setUsingNativeTitleBar(true);panel=new Panel(m);setContentOwned(panel,true);setResizable(true,false);setResizeLimits(900,600,1800,1200);}
MixAssistantWindow::~MixAssistantWindow(){clearContentComponent();setLookAndFeel(nullptr);}
void MixAssistantWindow::open(){panel->open();centreWithSize(1000,660);setVisible(true);toFront(true);}
void MixAssistantWindow::closeButtonPressed(){panel->close();setVisible(false);}
bool MixAssistantWindow::verify(const juce::File& folder){const bool ok=panel->verify(folder);folder.getChildFile("mix-assistant-ui.txt").replaceWithText(ok?"PASS preview A/B leaves project/Undo unchanged; explicit Apply and Undo/Redo; project-edit invalidation, real worker Cancel/retry/close":"FAIL Mix Assistant UI");return ok;}
void MainComponent::showMixAssistant(){if(editingRestricted()||loadingPlugin)return;if(!mixAssistantWindow)mixAssistantWindow=std::make_unique<MixAssistantWindow>(*this);mixAssistantWindow->open();}
}
