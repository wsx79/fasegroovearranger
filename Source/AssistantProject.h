// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "MixAnalysis.h"
#include "Localisation.h"
#include "MasteringAnalysis.h"
#include "core/Project.h"
#include <juce_gui_extra/juce_gui_extra.h>
namespace fga {
class MainComponent;class AudioEngine;class RenderProgressTask;
struct AssistantAudio {
    Project snapshot;
    juce::File directory=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("fga-assistant-"+juce::Uuid().toString());
    std::vector<int> tracks;
    ~AssistantAudio(){directory.deleteRecursively();}
    juce::File mix()const{return directory.getChildFile("mix.wav");}
    juce::File stem(int track)const{return directory.getChildFile("track-"+juce::String(track)+".wav");}
    std::vector<MixInput> mixInputs()const;
};
struct AssistantChange {
    enum Kind {gain,eq,compressor};Kind kind=gain;
    std::uint64_t session=0;int slot=0,parameter=0;double before=0,after=0,frequency=100,q=.707;
    EqSettings originalEq;
    juce::String reason;
    bool selected=true;
};
std::vector<AssistantChange> assistantGainChanges(const Project&,const MixReport&);
void assistantEqChanges(const Project&,const MixReport&,std::vector<AssistantChange>&);
void assistantCompressorChanges(AudioEngine&,const Project&,int track,const MixMeasure&,std::vector<AssistantChange>&);
juce::Result assistantCandidate(const Project&,AudioEngine&,const std::vector<AssistantChange>&,Project&);
void assistantParameters(AudioEngine&,const Project&,const std::vector<AssistantChange>&,bool after);
class ProjectAssistantPanel final:public juce::Component,private juce::Timer,private juce::ListBoxModel {
public:
    ProjectAssistantPanel(MainComponent&,bool mastering);~ProjectAssistantPanel()override;
    void open();void close();bool verify(const juce::File&);
    void resized()override;void paint(juce::Graphics&)override;
private:
    MainComponent& owner;bool master;
    juce::TextButton analyse{"Analyse project"},reference{"Optional reference"},apply{"Apply suggestions"},a{"A: original"},b{"B: suggestions"},r{"Compare reference"},stop{"Stop preview"};
    juce::ToggleButton useLufs{"Optional LUFS target"};juce::Slider lufs,ceiling;
    juce::Label note;juce::TextEditor text;juce::ListBox list;
    std::unique_ptr<juce::FileChooser> chooser;
    std::shared_ptr<AssistantAudio> audio,beforeAfter;
    std::shared_ptr<MixReport> mixReport;std::shared_ptr<MasteringReport> masterReport,comparison;
    std::vector<AssistantChange> changes;
    juce::File refFile;MasteringTargets targets;
    RenderProgressTask* task=nullptr;
    std::uint64_t revision=0,preview=0,generation=0;
    Localisation::Language language=Localisation::current();
    bool stale=false,applied=false,referenceMode=false;juce::Rectangle<int> metrics;int shownStep=-99;int step()const;
    void prepareComparison(int side,bool applyAfter);
    void renderProject();void previewSide(int);void applySelected();void present();void invalidate();void stopPreview();
    void release(std::shared_ptr<AssistantAudio>&);void timerCallback()override;
    int getNumRows()override{return (int)changes.size();}
    void paintListBoxItem(int,juce::Graphics&,int,int,bool)override;
    void listBoxItemClicked(int,const juce::MouseEvent&)override;
};
}
