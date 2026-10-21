// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "AudioEngine.h"
#include "ProjectIO.h"
#include "FamilyTheme.h"
#include "InstrumentPanel.h"
#include "FamilyOptions.h"
#include "FamilyControls.h"
#include "Updater.h"
#include "DawWorkspace.h"
#include <set>
namespace fga {
class MainComponent;
bool verifyPianoTools(MainComponent&,const juce::File&);
class ArrangementView : public juce::Component, public juce::SettableTooltipClient {
public:
    explicit ArrangementView(MainComponent& m):owner(m){setWantsKeyboardFocus(true);}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    bool verifyAutomation(const juce::File&);
    int rowY(int row) const;
    int rowAt(int y) const;
    int totalHeight() const {return rowY(trackCount);}
    std::array<bool,trackCount> volumeVisible{};
    double pixelsPerBar=6;
private:
    friend class MainComponent;
    MainComponent& owner;
    int dragTrack=-1,dragPoint=-1;
    bool bending=false,loopDragging=false,copyingClip=false;
    double loopAnchor=0;
    int clipTrack=-1,clipSource=-1,clipTarget=-1;
    int sectionAt(float x) const;
    double originalCurve=0;
    float dragY=0;
    int pointAt(int track,juce::Point<float>) const;
    float volumeY(int track,double value) const;
    void editPoint(juce::Point<float>,bool snap);

};
class PianoRoll : public juce::Component,public juce::SettableTooltipClient {
public:
    explicit PianoRoll(MainComponent&);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void clearSelection(){selected.clear();}
    bool guitarNotes=false;
    int lowNote=48,highNote=72,brushVelocity=100;
    bool flamBrush=false;
    int tool=0;
    bool selecting=false;
    juce::Rectangle<float> marquee;
    void fitNotes();
    bool verifyEditing();
    float rowHeight()const{return laneView()?40.f:std::max(5.f,(getHeight()-148.f)/(highNote-lowNote+1));}
    int leftMargin()const{return laneView()?148:64;}
    float velocityTop()const{return ownerTrackIsDrum()?112.f:(float)getHeight()-96;}
    float velocityHeight()const{return (float)getHeight()-velocityTop()-18;}
    bool ownerTrackIsDrum()const;
    void editVelocity(juce::Point<float>);
    bool laneView() const;
    float noteY(const Note&) const;
    float noteHeight() const;
    float noteWidth(const Note&) const;
    int laneAt(float) const;
    double grid=.25,pixelsPerBeat=76;
    std::set<int> selected;
    void deleteSelected();
private:
    MainComponent& owner;
    std::vector<Note> before;
    juce::Point<float> anchor;
    bool resizing=false,velocityDrag=false,editingVelocity=false;
    int hit(juce::Point<float>) const;
    double beat(float x) const {return std::max(0.,(x-leftMargin())/pixelsPerBeat);}
    int pitch(float y) const;
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
    std::function<void(float)> onScaleChosen;
    Project project;
    int selectedTrack=0,selectedSection=0;
    AudioEngine audio;
    void checkpoint();
    void changed(bool schedule=true);
    void mixChanged();
    void select(int track,int section);
    void showTrackMenu(int);
    void showInstrument();
    void showEditor();
    void showArrangement();
    void showSectionMenu(int);
    void showClipMenu(int,int);
    Clip& clip(){return project.tracks[(size_t)selectedTrack].clips[(size_t)selectedSection];}
    double clipLength()const{return project.sections[(size_t)selectedSection].bars*4.;}
    void regenerate(Scope);
    bool requestClose();
    bool writeLanguageEvidence(const juce::File&);
    void applyLanguage();
    void showPianoTools(int);
    void fitSong();
    void setLoopRange(double from,double to);
    void randomPianoMaterial(bool velocities,bool bass);
    bool writeUiEvidence(const juce::File&);
    bool verifyDawWorkspace(const juce::File&);
private:
    FamilyLookAndFeel theme;
    OptionsOverlay options;
    UpdateOverlay updateOverlay;
    Updater::Checker updater;
    std::unique_ptr<juce::TooltipWindow> tooltips;
    ArrangementView arrangement{*this};PianoRoll piano{*this};
    juce::Viewport arrangementPort,pianoPort,instrumentPort,inspectorPort;
    TrackInspector trackInspector{*this};
    DawMixer mixer{*this};
    bool inspectorVisible=true,mixerVisible=true,followPlayhead=false;
    int mixerHeight=244;
    juce::TextButton inspectorToggle{"INSPECTOR"},mixerToggle{"MIXER"},fitSongButton{"FIT SONG"},followButton{"FOLLOW"};
    InstrumentPanel instrumentPanel{*this};
    enum class Page { arrangement,instrument,editor };
    Page page=Page::arrangement;
    juce::TextButton backButton{"< ARRANGEMENT"},instrumentButton{"INSTRUMENT PANEL"},editButton{"EDIT MIDI"},leadButton{"SOLISTA"},arrangeButton{"NEW ARRANGEMENT"},guitarNotesButton{"NOTES / LANES"};
    juce::TextButton automationButton{"AUTOMATION"};
    juce::Label durationLabel;
    juce::ComboBox sectionSelector,panelSections;
    juce::TextButton newButton{"NEW"},openButton{"OPEN"},saveButton{"SAVE"},saveAsButton{"SAVE AS"},exportButton{"EXPORT"},settingsButton{"AUDIO"},aboutButton{"OPTIONS"},helpButton{"GUIDE"};
    IconButton playButton{IconButton::Icon::play},stopButton{IconButton::Icon::stop},rewindButton{IconButton::Icon::rewind},undoButton{IconButton::Icon::undo},redoButton{IconButton::Icon::redo};
    juce::ToggleButton loopButton{"LOOP"};
    juce::TextButton songButton{"GENERATE SONG"},trackButton{"NEW TRACK"},sectionButton{"NEW SECTION"},clipButton{"NEW CLIP"},harmonyButton{"NEW CHORDS"},addSectionButton{"+ SECTION"};
    juce::ComboBox genreBox,keyBox,scaleBox,extensionBox,voicingBox,progressionBox,bassBox,stringsBox;
    juce::Slider tempoSlider,swingSlider,densitySlider,humanizeSlider,masterSlider,zoomSlider;
    juce::Label positionLabel,chordsLabel,statusLabel;
    juce::TextButton soundButton{"INSTRUMENT"},pluginEditorButton{"OPEN VST"},sampleButton{"LOAD SAMPLE"},loadPluginButton{"LOAD VST3 / AU"};
    juce::Slider gainSlider,panSlider,velocitySlider;
    juce::TextButton muteButton{"MUTE"},soloButton{"SOLO"};
    juce::ToggleButton lockButton{"LOCK TRACK"},lockClipButton{"LOCK CLIP"};
    juce::ComboBox soundBox,gridBox;
    juce::TextButton brushAccent{"Accent"},brushHit{"Hit"},brushGhost{"Ghost"},brushFlam{"Flam"};
    juce::TextButton fitNotesButton{"FIT"},drawTool{"Draw"},selectTool{"Select"},eraseTool{"Erase"};
    juce::TextButton quantiseButton{"QUANTISE"},transposeUp{"+12"},transposeDown{"-12"},deleteNotes{"DELETE"},importButton{"IMPORT MIDI"},copyButton{"COPY"},pasteButton{"PASTE"};
    juce::TextButton sectionLoopButton{"LOOP SECTION"};
    juce::TextButton chordEditButton{"EDIT CHORDS"};
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
    juce::Component::SafePointer<juce::DialogWindow> musicalToolWindow;
    int musicalToolKind=0;
    std::unique_ptr<juce::ThreadWithProgressWindow> exportTask;
    void refreshControls();
    void refreshTrack();
    void updateSizes();
    void undo(bool redo);
    void save(bool saveAs);
    void open(const juce::File&);
    void restoreInstruments();
    void loadPluginFile(int,const juce::File&,const juce::String& id={},const juce::String& state={});
    void loadPluginDescription(int,const juce::PluginDescription&,const juce::String& state={},bool restoring=false);
    void showPluginMenu(juce::Component* anchor=nullptr);
    void chooseFile(const juce::String&,const juce::String&,int,std::function<void(juce::File)>);
    void exportMenu();
    void exportFile(int type,int track);
    void loadSampleFile(int,const juce::File&);
    void showAbout();
    void showGuide(bool english=false);
    bool writeHelpEvidence(const juce::File&);
    void showDocument(const juce::String&,const juce::String&);
    void showAudioSettings();
    void closePluginEditor();
    void showChordMenu();
    void message(const juce::String&,bool error=false);
    void timerCallback() override;
    void importFile(const juce::File&);
    void modifySelected(std::function<void(Clip&)>);
    friend class ArrangementView;
    friend class PianoRoll;
    friend class InstrumentPanel;
    friend class TrackInspector;
    friend class MixerStrip;
    friend class DawMixer;
};
}
