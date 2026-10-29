// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include "RetiredJobs.h"
#include "AudioEngine.h"
#include "ProjectIO.h"
#include "DawTheme.h"
#include "InstrumentPanel.h"
#include "FamilyOptions.h"
#include "FamilyControls.h"
#include "Updater.h"
#include "DawWorkspace.h"
#include "PluginSettings.h"
#include "PluginBrowser.h"
#include "SampleBrowser.h"
#include "StudioWindow.h"
#include "MixerRoutingWindow.h"
#include <set>
#include <deque>
namespace fga {
class MainComponent;
bool verifyPianoTools(MainComponent&,const juce::File&);

// The narrow half of the GENERATE split button: it opens the scope menu (song, track,
// section, clip) so the four generation scopes share one slot instead of four buttons.
class GenerateMenuButton : public juce::Button {
public:
    GenerateMenuButton():juce::Button("generate-scope"){setWantsKeyboardFocus(false);setMouseCursor(juce::MouseCursor::PointingHandCursor);}
    void paintButton(juce::Graphics& g,bool over,bool down)override{
        const auto r=getLocalBounds().toFloat().reduced(.5f);
        // Same disabled fade as the labelled action next to it: during a take the whole
        // generation block dims together instead of leaving an active-looking caret.
        g.beginTransparencyLayer(isEnabled()?1.f:.38f);
        g.setColour(down?Daw::accent.darker(.12f):over&&isEnabled()?Daw::accent.brighter(.06f):Daw::accent);g.fillRoundedRectangle(r,Daw::radius);
        g.setColour(juce::Colours::black.withAlpha(.35f));g.drawRoundedRectangle(r,Daw::radius,1.f);
        const float cx=r.getCentreX(),cy=r.getCentreY();juce::Path caret;
        caret.startNewSubPath(cx-3.5f,cy-1.5f);caret.lineTo(cx,cy+2.f);caret.lineTo(cx+3.5f,cy-1.5f);
        g.setColour(Daw::onAccent);g.strokePath(caret,juce::PathStrokeType(1.6f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        g.endTransparencyLayer();
    }
};
class ArrangementView : public juce::Component, public juce::SettableTooltipClient {
public:
    explicit ArrangementView(MainComponent&);
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
    bool verifyAutomation(const juce::File&);
    bool keyPressed(const juce::KeyPress&) override;
    int rowHeight(int row) const {return trackHeights[(size_t)trackAtRow(row)];}
    int rowY(int row) const;
    int rowAt(int y) const;
    int totalHeight() const;
    std::array<bool,maxTracks> volumeVisible{};
    // What each track's automation lane shows and edits: 0 volume, 1 pan, 2 master volume,
    // 3 + k the track's plugin parameter lane k.
    enum LaneTarget { laneVolume=0, lanePan, laneMaster, lanePlugin };
    std::array<int,maxTracks> laneTarget{};
    VolumeAutomation& envelope(int track) const;
    double envelopeMax(int track) const;       // 2 for a gain, 1 for pan and plugin parameters
    double envelopeDefault(int track) const;   // a new lane's flat value
    juce::String envelopeTitle(int track) const;
    void showLaneTargetMenu(int track);
    double pixelsPerBar=6;
    std::set<int> selectedClips;
    int dropTrack=-1; // a sample dragged from the browser over this track
    double snapBeat(double beat,bool bypass=false) const;
private:
    friend class MainComponent;
    MainComponent& owner;
    std::array<int,maxTracks> trackHeights{};
    int heightTrack=-1,heightBefore=40;
    int hoverTrack=-1,hoverClip=-1;   // the clip under the pointer, outlined
    void setHover(int track,int clip);
    float heightAnchorY=0;
    int heightEdgeAt(const juce::MouseEvent&) const;
    void saveTrackHeights();
    int dragTrack=-1,dragPoint=-1;
    bool bending=false,loopDragging=false,copyingClip=false;
    double loopAnchor=0;
    // Loop brace gestures on the lower ruler: move an edge, move the region or draw a new one.
    enum LoopGesture { loopNone=0,loopLeftEdge,loopRightEdge,loopMove,loopDraw };
    int loopGesture=loopNone;bool loopGestureActive=false,loopWasOn=false;float loopPressX=0;double loopPressBeat=0,loopFrom=0,loopTo=0;
    int loopEdgeAt(float x) const;   // loopLeftEdge, loopRightEdge or loopNone
    void dragLoop(const juce::MouseEvent&);
public:
    bool verifyLoopEditing();
private:
    int clipTrack=-1,clipSource=-1,clipTarget=-1;
    double clipAnchorX=0,clipDropBeat=0;
    bool clipMoving=false;
    bool panning=false;
    juce::Point<int> panStart;
    juce::Point<float> panAnchor;
    int sectionAt(float x) const;
    int clipAt(int track,float x) const;
    int edgeAt(int track,float x) const;
    void beginResize(int section,int track,const juce::MouseEvent&);
    void cancelResize();
    int resizeSectionIndex=-1,resizeTrack=-1;
    float resizeAnchorX=0;
    double resizeOriginalLength=0,resizeLastLength=0;
    std::unique_ptr<Project> resizeBefore;
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
    void mouseMove(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&,const juce::MouseWheelDetails&) override;
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
    void eraseAt(juce::Point<float>);
    bool laneView() const;
    float noteY(const Note&) const;
    float noteHeight() const;
    float noteWidth(const Note&) const;
    int laneAt(float) const;
    double grid=.25,pixelsPerBeat=76;
    std::set<int> selected;
    void deleteSelected();
    bool editKeys(const juce::KeyPress&);
    void pasteNotes();
private:
    friend class MainComponent;
    MainComponent& owner;
    std::vector<Note> before;
    std::vector<Note> gestureOriginal;
    juce::Point<float> anchor;
    bool resizing=false,velocityDrag=false,editingVelocity=false,velocityCheckpoint=false,erasing=false,eraseCheckpoint=false;
    int hit(juce::Point<float>) const;
    void retainSelection(const std::vector<Note>&);
    double beat(float x) const {return std::max(0.,(x-leftMargin())/pixelsPerBeat);}
    int pitch(float y) const;
};
class AiComposerWindow;
class MainComponent : public juce::Component,private juce::Timer,public juce::FileDragAndDropTarget,public juce::DragAndDropContainer,public juce::DragAndDropTarget {
public:
    MainComponent();
    ~MainComponent() override;
    // Opens the first .fga in a command line (double-click, file association, second instance).
    bool openFromCommandLine(const juce::String& commandLine);
    static juce::File projectInCommandLine(const juce::String& commandLine);
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&,int,int) override;
    // Samples dragged from the browser onto a track.
    bool isInterestedInDragSource(const SourceDetails&) override;
    void itemDragMove(const SourceDetails&) override;
    void itemDragExit(const SourceDetails&) override;
    void itemDropped(const SourceDetails&) override;
    void showSampleBrowser(bool);
    bool verifySampleBrowser(const juce::File&);
    std::function<void(float)> onScaleChosen;
    Project project;
    int selectedTrack=0,selectedSection=0,selectedClip=-1;
    AudioEngine audio;
    RetiredJobs retiredJobs;
    PluginCatalog pluginCatalog;
    bool editingRestricted()const;
    void updateEditingAvailability();
    void stopPlaybackAndRecording();
    void checkpoint();
    void checkpointMix(const void* control=nullptr);
    // onlyTrack: just that track's notes changed (its schedule alone is rebuilt).
    void changed(bool schedule=true,int onlyTrack=-1);
    void mixChanged();
    void select(int track,int section);
    void showTrackMenu(int);
    void addMidiTrack();
    void deleteMidiTrack(int);
    void showAddSectionMenu();
    void addSongSection(Section);
    void fitGeneratedView();
    void showInstrument();
    void showEditor();
    void showArrangement();
    void showSectionMenu(int);
    void showClipMenu(int,int);
    int clipIndex() const {if(noClipSelection)return -1;return project.tracks[(size_t)selectedTrack].freeTimeline?selectedClip:selectedSection;}
    Clip& clip(){auto& clips=project.tracks[(size_t)selectedTrack].clips;const int i=clipIndex();return i>=0&&i<(int)clips.size()?clips[(size_t)i]:emptyClip;}
    const Clip& clip()const{const auto& clips=project.tracks[(size_t)selectedTrack].clips;const int i=clipIndex();return i>=0&&i<(int)clips.size()?clips[(size_t)i]:emptyClip;}
    double clipLength()const{return project.clipLength(selectedTrack,clipIndex());}
    double clipStart()const{return clipIndex()<0?timelinePosition():project.clipStart(selectedTrack,clipIndex());}
    void selectClip(int track,int clip,bool keepSelection=false,double at=-1);
    void timelineCommand(int command);
    void moveSelectedClips(int sourceTrack,int anchor,int targetTrack,double beat,bool copy);
    void ensureTimelineClip(bool addUndo=true);
    double timelinePosition()const{return editCursor>=0&&!audio.isPlaying()?editCursor:audio.position();}
    void seekTimeline(double beat);
    void togglePlayback();
    void toggleMetronome();
    void regenerate(Scope,bool newTake=true);
    void showGenerateMenu();
    // Cached views of derived song data: chord names and length are asked for from
    // paint() and the 20 Hz timer, and both were rebuilt from scratch every time.
    const std::vector<std::string>& cachedChordNames();
    double songLength();
    // The guide gear: the VST editor when the track hosts one, the instrument panel otherwise.
    void openInstrumentFor(int track);
    void requestClose(std::function<void()> accepted);
    bool writeLanguageEvidence(const juce::File&);
    void applyLanguage();
    void showPianoTools(int);
    void fitSong();
    void zoomAt(bool editor,double factor,double viewportX);
    void wheelNavigate(bool editor,const juce::MouseEvent&,const juce::MouseWheelDetails&);
    void zoomSelection();
    void loopSelection();
    void suspendFollow();
    bool verifyNavigation(const juce::File&);
    void setLoopRange(double from,double to);
    void toggleLoop();
    void clearLoop();
    void toggleClipLoop();
    bool clipLoopActive() const;
    // What the track actually plays: the plugin's name, the sample's name or the guide sound.
    juce::String soundSourceName(int track);
    // The sound source plus the insert effect's name, as the inspectors show it.
    juce::String sourceWithEffect(int track);
    const juce::String& cachedSourceName(int track);
    // Drag start/end of a slider: one undo step per gesture.
    void trackGesture(juce::Slider&,std::function<void()> onEnd={},bool mix=false);
    static bool inGesture(const juce::Component& c){return (bool)c.getProperties()["fgaGesture"];}
    void checkpoint(const void* control);
    void repaintHeader();
    void useBuiltInSound(int track);
    void browseSamplesFor(int track);
    void toggleTrackLock();
    void toggleClipLock();
    void regenerateEditedClip();
    void applyInstrumentSettings(int track,bool harmony);
    void randomPianoMaterial(bool velocities,bool bass);
    bool writeUiEvidence(const juce::File&);
    juce::StringArray iconButtonsWithoutTips();
    bool verifyDawWorkspace(const juce::File&);
    bool verifyPluginWindow(const juce::File&);
    void dismissAuxiliaryWindow(juce::Component&);
    bool verifyWindowFocus(const juce::File&);
    bool verifyAuditRepairs(const juce::File&);
    void showStudio(int tab=0);
    void showMixerRouting(int track=-1);
    void showAiComposer();
    juce::Result captureAiProject(Project&);
    juce::Result applyAiComposition(const Project&,const Project&,bool listen);
    bool verifyAiComposer(const juce::File&);
    bool verifyTimelineEditing(const juce::File&);
    bool verifyPresentation(const juce::File&);
private:
    friend bool verifyPianoTools(MainComponent&,const juce::File&);
    friend class StudioWindow;
    friend class MixerRoutingWindow;
    std::unique_ptr<StudioWindow> studioWindow;
    std::unique_ptr<MixerRoutingWindow> mixerRoutingWindow;
    std::unique_ptr<AiComposerWindow> aiWindow;
    ActionButton studioButton{"STUDIO"};
    IconButton aiComposeButton{IconButton::Icon::sparkles};
    DawLookAndFeel theme;
    OptionsOverlay options;
    PluginSettings pluginSettings{pluginCatalog};
    PluginBrowser pluginBrowser{pluginCatalog};
    UpdateOverlay updateOverlay;
    Updater::Checker updater;
    std::unique_ptr<juce::TooltipWindow> tooltips;
    ArrangementView arrangement{*this};PianoRoll piano{*this};
    juce::Viewport arrangementPort,pianoPort,instrumentPort,inspectorPort;
    IconButton inspectorCollapse{IconButton::Icon::close};
    TrackInspector trackInspector{*this};
    DawMixer mixer{*this};
    InspectorResizer inspectorResizer{*this},browserResizer{*this,true};
    bool inspectorVisible=true,mixerVisible=true,followPlayhead=false;
    bool timeDisplay=false;
    juce::String positionLegend;
    DisplayButton positionModeButton;
    IconButton timelineZoomOut{IconButton::Icon::zoomOut},timelineZoomIn{IconButton::Icon::zoomIn};
    ActionButton timelineFitButton{"FIT SONG"};
    juce::Slider timelineZoom;
    static juce::String timeText(double seconds,int decimals=0);
    juce::String rulerText(double beat)const;
    void toggleTimeDisplay();
    int mixerHeight=244,inspectorWidth=248;
    int sampleBrowserWidth=0;   // 0: automatic (a quarter of the window); else dragged by its left edge (saved)
    std::vector<std::string> chordNameCache;
    std::uint32_t chordNameCacheKey=0;
    bool chordNameCacheReady=false;
    double songLengthCache=-1;
    int clampBrowserWidth(int width)const{return std::clamp(width,260,std::max(260,std::min(640,getWidth()/2)));}
    IconButton inspectorToggle{IconButton::Icon::inspector},mixerToggle{IconButton::Icon::mixer},browserToggle{IconButton::Icon::browser},fitSongButton{IconButton::Icon::fitWidth},followButton{IconButton::Icon::follow};
    // Full screen drops the native title bar, so the arranger reaches the screen edges.
    IconButton fullScreenButton{IconButton::Icon::fullScreen};
    void applyFullScreen(bool on);
    bool appFullScreen=false,fullScreenApplying=false;juce::Rectangle<int> fullScreenRestore;
    SampleBrowser sampleBrowser{audio};
    bool browserVisible=false;
    InstrumentPanel instrumentPanel{*this};
    enum class Page { arrangement,instrument,editor };
    Page page=Page::arrangement;
    // Window layout: the control bar, the toolbar rows below it (two, three in the MIDI editor)
    // and the status bar.
    static constexpr int controlBarHeight=56,toolRowHeight=36,statusHeight=28;
    int toolbarBottom()const{return controlBarHeight+(page==Page::editor?3:2)*toolRowHeight+(page==Page::arrangement?16:0);}
    juce::Rectangle<int> lcdArea;std::vector<int> lcdDividers;std::vector<juce::Rectangle<int>> toolbarDividers;
    ActionButton backButton{"< ARRANGEMENT"},instrumentButton{"INSTRUMENT PANEL"},editButton{"EDIT MIDI"},leadButton{"GUITAR LEADS"},arrangeButton{"NEW ARRANGEMENT"},guitarNotesButton{"NOTES / LANES"};
    IconButton automationButton{IconButton::Icon::automation};
    juce::Label durationLabel;
    juce::ComboBox sectionSelector,panelSections;
    IconButton newButton{IconButton::Icon::newFile},openButton{IconButton::Icon::folder},saveButton{IconButton::Icon::save},saveAsButton{IconButton::Icon::saveAs},exportButton{IconButton::Icon::exportFile},settingsButton{IconButton::Icon::speaker},aboutButton{IconButton::Icon::gear},helpButton{IconButton::Icon::help};
    IconButton playButton{IconButton::Icon::play},stopButton{IconButton::Icon::stop},rewindButton{IconButton::Icon::rewind},undoButton{IconButton::Icon::undo},redoButton{IconButton::Icon::redo};
    IconButton loopButton{IconButton::Icon::loop},metronomeButton{IconButton::Icon::metronome};
    ActionButton songButton{"GENERATE SONG"},trackButton{"NEW TRACK"},sectionButton{"NEW SECTION"},clipButton{"NEW CLIP"},harmonyButton{"NEW CHORDS"},addSectionButton{"+ SECTION"};
    GenerateMenuButton generateMenuButton;
    ActionButton addTrackButton { "+ MIDI" };
    juce::ComboBox genreBox,keyBox,scaleBox,extensionBox,voicingBox,progressionBox,bassBox,stringsBox;
    juce::Slider tempoSlider,swingSlider,densitySlider,humanizeSlider,masterSlider,zoomSlider;
    juce::Label positionLabel,chordsLabel,statusLabel;
    ActionButton soundButton{"INSTRUMENT"},pluginEditorButton{"OPEN VST"},sampleButton{"LOAD SAMPLE"},loadPluginButton{"LOAD VST3 / AU"};
    juce::Slider gainSlider,panSlider,velocitySlider;
    ActionButton muteButton{"MUTE"},soloButton{"SOLO"};
    juce::ToggleButton lockButton{"LOCK TRACK"},lockClipButton{"LOCK CLIP"};
    juce::ComboBox soundBox,gridBox;
    ActionButton brushAccent{"ACCENT"},brushHit{"HIT"},brushGhost{"GHOST"},brushFlam{"FLAM"};
    ActionButton fitNotesButton{"FIT"};
    IconButton drawTool{IconButton::Icon::pencil},selectTool{IconButton::Icon::pointer},eraseTool{IconButton::Icon::eraser},regenerateClipButton{IconButton::Icon::dice};
    ActionButton quantiseButton{"QUANTISE"},transposeUp{"+12"},transposeDown{"-12"},deleteNotes{"DELETE"},importButton{"IMPORT MIDI"},copyButton{"COPY"},pasteButton{"PASTE"};
    ActionButton sectionLoopButton{"LOOP CLIP"};
    ActionButton chordEditButton{"EDIT CHORDS"};
    juce::Label inspectorLabel,editorLabel;
    std::deque<Project> undoStack,redoStack;
    const void* lastCheckpointControl=nullptr;juce::uint32 lastCheckpointTime=0;
    double lastTimerBeat=-1;int lastPlayX=-1,soundBoxTrack=-1;
    juce::StringArray sectionSelectorNames;
    std::array<juce::String,maxTracks> sourceNames;std::array<bool,maxTracks> sourceNameCached{};
    std::vector<Note> clipboard;
    std::vector<Clip> timelineClipboard;
    Clip emptyClip;
    bool noClipSelection=false;
    double selectionBeat=0,editCursor=-1;
    double timelineSnap=1;
    IconButton zoomSelectionButton{IconButton::Icon::zoomSelection},loopSelectionButton{IconButton::Icon::loopSelection};
    juce::ComboBox timelineGrid;
    juce::File projectFile;
    bool dirty=false,refreshing=false;
    int loadingPlugin=0;
    int ticks=0;
    std::unique_ptr<juce::FileChooser> chooser;
    class InstrumentWindow;
    std::unique_ptr<InstrumentWindow> instrumentWindow;
    bool instrumentIsOpen() const;
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
    void createNewProject();
    bool writeProjectFile(const juce::File&);
    void completeCloseChoice(int,std::function<void()>);
    void finishPendingSave(const juce::File&,std::function<void()>);
    bool closePending=false,mediaBusy=false,availabilityUpdating=false;
    std::vector<std::pair<juce::Component::SafePointer<juce::Component>,bool>> restrictedComponents;
    class AutosaveJob;
    std::unique_ptr<AutosaveJob> autosaveJob;
    bool autosaveDue=false;
    int autosaveRequestId=0;
    void autosaveIfIdle(const juce::File& target={});
    void open(const juce::File&,bool recovered=false);
    void restoreInstruments();
    void loadPluginFile(int,const juce::File&,const juce::String& id={},const juce::String& state={});
    void loadPluginDescription(int,const juce::PluginDescription&,const juce::String& state={},bool restoring=false);
    void loadEffectFile(int,const juce::File&,const juce::String& id={},const juce::String& state={});
    void loadEffectDescription(int,const juce::PluginDescription&,const juce::String& state={},bool restoring=false);
    void chooseBrowserRow(const PluginCatalog::BrowserRow&);
    void showPluginMenu(juce::Component* anchor=nullptr);
    void chooseFile(const juce::String&,const juce::String&,int,std::function<void(juce::File)>,std::function<void()> cancelled={});
    void exportMenu();
    void exportFile(int type,int track,const juce::File& destination={});
    void loadSampleFile(int,const juce::File&);
    void showAbout();
    void showGuide(bool english=false);
    bool writeHelpEvidence(const juce::File&);
    void showDocument(const juce::String&,const juce::String&);
    void showAudioSettings();
    void closePluginEditor();
    void showChordMenu();
    void message(const juce::String&,bool error=false);
    void showPosition(double beat);
    void timerCallback() override;
    void importFile(const juce::File&,double placeAt=-1);
    void modifySelected(std::function<void(Clip&)>);
    friend class ArrangementView;
    friend class PianoRoll;
    friend class InstrumentPanel;
    friend class TrackInspector;
    friend class InspectorResizer;
    friend class MixerStrip;
    friend class DawMixer;
};
}
