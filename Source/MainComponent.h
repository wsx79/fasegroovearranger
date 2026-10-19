// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "AudioEngine.h"
#include "ProjectIO.h"
#include "FamilyTheme.h"
#include <set>
namespace fga {
class MainComponent;
class ArrangementView : public juce::Component {
public:
    explicit ArrangementView(MainComponent& m):owner(m){}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    double pixelsPerBar=48;
private: MainComponent& owner;
};
class PianoRoll : public juce::Component {
public:
    explicit PianoRoll(MainComponent&);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void clearSelection(){selected.clear();}
    double grid=.25,pixelsPerBeat=76;
    std::set<int> selected;
    void deleteSelected();
private:
    MainComponent& owner;
    std::vector<Note> before;
    juce::Point<float> anchor;
    bool resizing=false,velocityDrag=false;
    int hit(juce::Point<float>) const;
    double beat(float x) const {return std::max(0.,(x-64.)/pixelsPerBeat);}
    int pitch(float y) const {return std::clamp(127-(int)((y-24)/12),0,127);}
};
class MainComponent : public juce::Component,private juce::Timer,public juce::FileDragAndDropTarget {
public:
    MainComponent();
    ~MainComponent() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&,int,int) override;
    Project project;
    int selectedTrack=0,selectedSection=0;
    AudioEngine audio;
    void checkpoint();
    void changed(bool schedule=true);
    void select(int track,int section);
    void showTrackMenu(int);
    void showSectionMenu(int);
    void showClipMenu(int,int);
    Clip& clip(){return project.tracks[(size_t)selectedTrack].clips[(size_t)selectedSection];}
    double clipLength()const{return project.sections[(size_t)selectedSection].bars*4.;}
    void regenerate(Scope);
    bool requestClose();
private:
    FamilyLookAndFeel theme;
    juce::TooltipWindow tooltips{this,650};
    ArrangementView arrangement{*this};PianoRoll piano{*this};
    juce::Viewport arrangementPort,pianoPort;
    juce::TextButton newButton{"NEW"},openButton{"OPEN"},saveButton{"SAVE"},saveAsButton{"SAVE AS"},exportButton{"EXPORT"},settingsButton{"AUDIO"},aboutButton{"ECOSYSTEM"},helpButton{"GUIDA"};
    juce::TextButton playButton{"PLAY"},stopButton{"STOP"},rewindButton{"|<"},undoButton{"UNDO"},redoButton{"REDO"};
    juce::ToggleButton loopButton{"LOOP"};
    juce::TextButton songButton{"GENERATE SONG"},trackButton{"NEW TRACK"},sectionButton{"NEW SECTION"},clipButton{"NEW CLIP"},harmonyButton{"NEW CHORDS"},addSectionButton{"+ SECTION"};
    juce::ComboBox genreBox,keyBox,scaleBox,extensionBox,voicingBox,progressionBox,bassBox,stringsBox;
    juce::Slider tempoSlider,swingSlider,densitySlider,humanizeSlider,masterSlider,zoomSlider;
    juce::Label positionLabel,chordsLabel,statusLabel;
    juce::TextButton soundButton{"INSTRUMENT"},pluginEditorButton{"OPEN VST"},sampleButton{"LOAD SAMPLE"},loadPluginButton{"LOAD VST3 / AU"};
    juce::Slider gainSlider,panSlider,velocitySlider;
    juce::ToggleButton muteButton{"MUTE"},soloButton{"SOLO"},lockButton{"LOCK TRACK"},lockClipButton{"LOCK CLIP"};
    juce::ComboBox soundBox,gridBox;
    juce::TextButton quantiseButton{"QUANTISE"},transposeUp{"+12"},transposeDown{"-12"},deleteNotes{"DELETE"},importButton{"IMPORT MIDI"},copyButton{"COPY"},pasteButton{"PASTE"};
    juce::TextButton sectionLoopButton{"LOOP SECTION"};
    juce::Label inspectorLabel,editorLabel;
    std::vector<Project> undoStack,redoStack;
    std::vector<Note> clipboard;
    juce::File projectFile;
    bool dirty=false,refreshing=false;
    int loadingPlugin=0;
    int ticks=0;
    std::unique_ptr<juce::FileChooser> chooser;
    class PluginWindow;
    std::unique_ptr<PluginWindow> pluginWindow;
    std::unique_ptr<juce::ThreadWithProgressWindow> exportTask;
    void refreshControls();
    void refreshTrack();
    void updateSizes();
    void undo(bool redo);
    void save(bool saveAs);
    void open(const juce::File&);
    void restoreInstruments();
    void loadPluginFile(int,const juce::File&,const juce::String& id={},const juce::String& state={});
    void chooseFile(const juce::String&,const juce::String&,int,std::function<void(juce::File)>);
    void exportMenu();
    void exportFile(int type,int track);
    void loadSampleFile(int,const juce::File&);
    void showAbout();
    void showDocument(const juce::String&,const juce::String&);
    void showAudioSettings();
    void closePluginEditor();
    void message(const juce::String&,bool error=false);
    void timerCallback() override;
    void importFile(const juce::File&);
    void modifySelected(std::function<void(Clip&)>);
    friend class ArrangementView;
    friend class PianoRoll;
};
}
