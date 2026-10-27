// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MainComponent.h"
#include "Localisation.h"
#include <cmath>
#include <limits>

namespace fga {
namespace {
constexpr int headerWidth=216,rulerHeight=60;
bool selectionRange(const MainComponent& owner,const std::set<int>& indices,bool editor,double& from,double& to){
    from=std::numeric_limits<double>::max();to=0;
    if(editor){
        const auto& notes=owner.clip().notes;
        for(int i:indices)if(i>=0&&i<(int)notes.size()){from=std::min(from,notes[(size_t)i].start);to=std::max(to,notes[(size_t)i].start+notes[(size_t)i].length);}
        if(to<=from){from=0;to=owner.clipLength();}
    }else{
        auto chosen=indices;if(chosen.empty()&&owner.clipIndex()>=0)chosen.insert(owner.clipIndex());
        for(int i:chosen)if(i>=0&&i<(int)owner.project.tracks[(size_t)owner.selectedTrack].clips.size()){
            const double at=owner.project.clipStart(owner.selectedTrack,i);from=std::min(from,at);to=std::max(to,at+owner.project.clipLength(owner.selectedTrack,i));}
    }
    return to>from;
}
}

juce::String MainComponent::timeText(double seconds,int decimals){
    if(!std::isfinite(seconds))seconds=0;seconds=std::max(0.,seconds);decimals=std::clamp(decimals,0,3);
    const auto factor=decimals==3?1000:decimals==2?100:decimals==1?10:1;
    const auto ticks=(juce::int64)std::floor(seconds*factor+1e-6),whole=ticks/factor;
    juce::String text=whole>=3600?juce::String(whole/3600)+":"+juce::String((whole/60)%60).paddedLeft('0',2)+":"+juce::String(whole%60).paddedLeft('0',2):juce::String(whole/60)+":"+juce::String(whole%60).paddedLeft('0',2);
    if(decimals)text+="."+juce::String(ticks%factor).paddedLeft('0',decimals);return text;
}
juce::String MainComponent::rulerText(double beat)const{return timeDisplay?timeText(beat*60/project.settings.bpm,2):juce::String((int)std::llround(beat/4)+1);}
void MainComponent::toggleTimeDisplay(){
    timeDisplay=!timeDisplay;Localisation::setSetting("timelineTimeDisplay",timeDisplay?"1":"0");showPosition(timelinePosition());arrangement.repaint();repaint(lcdArea);
}
void MainComponent::suspendFollow(){followPlayhead=false;followButton.setToggleState(false,juce::dontSendNotification);}
void MainComponent::zoomAt(bool editor,double factor,double viewportX){
    if(!std::isfinite(factor)||factor<=0)return;
    auto& port=editor?pianoPort:arrangementPort;auto& scale=editor?piano.pixelsPerBeat:arrangement.pixelsPerBar;
    const int margin=editor?piano.leftMargin():headerWidth;const auto view=port.getViewArea();
    viewportX=std::clamp(viewportX,(double)margin,(double)std::max(margin,view.getWidth()-20));
    const double time=(view.getX()+viewportX-margin)/scale;
    scale=std::clamp(scale*factor,editor?2.:.05,160.);
    zoomSlider.setValue(scale,juce::dontSendNotification);updateSizes();
    port.setViewPosition(juce::roundToInt(margin+time*scale-viewportX),view.getY());
    suspendFollow();port.repaint();
}
void MainComponent::wheelNavigate(bool editor,const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel){
    if(e.mods.isAnyMouseButtonDown())return;
    auto& port=editor?pianoPort:arrangementPort;const auto view=port.getViewArea();
    const double localX=e.position.x-view.getX();
    // The ruler offers direct wheel zoom; modifiers provide it over the note/clip area.
    const bool ruler=!e.mods.isShiftDown()&&e.position.y<view.getY()+(editor?40:rulerHeight)&&std::abs(wheel.deltaY)>std::abs(wheel.deltaX);
    if(e.mods.isCtrlDown()||e.mods.isCommandDown()||ruler){
        const double delta=std::abs(wheel.deltaY)>0?wheel.deltaY:wheel.deltaX;
        zoomAt(editor,std::exp(std::clamp(delta*2.5,-1.,1.)),localX);return;
    }
    const double x=e.mods.isShiftDown()?(wheel.deltaY+wheel.deltaX):wheel.deltaX;
    const double y=e.mods.isShiftDown()?0:wheel.deltaY;
    port.setViewPosition(view.getX()-juce::roundToInt(x*300),view.getY()-juce::roundToInt(y*300));
    suspendFollow();
}
void ArrangementView::mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel){
    if(resizeBefore||heightTrack>=0||clipTrack>=0||panning||loopDragging||dragTrack>=0)return;
    owner.wheelNavigate(false,e,wheel);
}
void PianoRoll::mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel){
    if(!before.empty()||selecting||editingVelocity)return;
    owner.wheelNavigate(true,e,wheel);
}
void MainComponent::zoomSelection(){
    const bool editor=page==Page::editor;double from=0,to=0;
    if(!selectionRange(*this,editor?piano.selected:arrangement.selectedClips,editor,from,to))return;
    auto& port=editor?pianoPort:arrangementPort;const int margin=editor?piano.leftMargin():headerWidth;
    auto& scale=editor?piano.pixelsPerBeat:arrangement.pixelsPerBar;
    scale=std::clamp((std::max(40,port.getWidth()-margin-48))/(to-from)*(editor?1.:4.),editor?2.:.05,160.);
    zoomSlider.setValue(scale,juce::dontSendNotification);updateSizes();
    port.setViewPosition(std::max(0,juce::roundToInt(from*(editor?scale:scale/4)-12)),port.getViewPositionY());
    suspendFollow();port.repaint();
}
void MainComponent::loopSelection(){
    const bool editor=page==Page::editor;double from=0,to=0;
    if(!selectionRange(*this,editor?piano.selected:arrangement.selectedClips,editor,from,to))return;
    if(editor){from+=clipStart();to+=clipStart();}
    // The same selection again removes the loop.
    checkpoint();project.toggleLoopRegion(from,to);changed(false);
}
void PianoRoll::retainSelection(const std::vector<Note>& wanted){
    selected.clear();const auto& notes=owner.clip().notes;
    std::array<std::vector<double>,128> times;
    for(const auto& n:wanted)if(n.pitch>=0&&n.pitch<128)times[(size_t)n.pitch].push_back(n.start);
    for(auto& starts:times)std::sort(starts.begin(),starts.end());
    for(size_t i=0;i<notes.size();++i){const auto& n=notes[i];if(n.pitch<0||n.pitch>127)continue;
        const auto& starts=times[(size_t)n.pitch];auto at=std::lower_bound(starts.begin(),starts.end(),n.start-1./960.);
        if(at!=starts.end()&&std::abs(*at-n.start)<1./960.)selected.insert((int)i);}
    repaint();
}
void PianoRoll::pasteNotes(){
    if(owner.clipIndex()<0||owner.clipLength()<=0||owner.clipboard.empty())return;
    auto copied=owner.clipboard;double origin=copied.front().start;for(const auto& n:copied)origin=std::min(origin,n.start);
    const double cursor=owner.timelinePosition()-owner.clipStart();const double at=cursor>=0&&cursor<owner.clipLength()?std::floor(cursor/grid)*grid:0;
    double end=0;for(auto& n:copied){n.start+=at-origin;end=std::max(end,n.start+n.length);}
    // Preserve the complete phrase rather than silently truncating notes at the clip end.
    if(end>owner.clipLength()+1./960.){owner.message("The notes do not fit inside this clip. Extend the clip first.",true);return;}
    size_t count=copied.size();for(const auto& t:owner.project.tracks)for(const auto& c:t.clips)count+=c.notes.size()+c.loopSource.size();
    if(count>2000000)return;
    owner.checkpoint();auto& notes=owner.clip().notes;notes.insert(notes.end(),copied.begin(),copied.end());owner.changed();
    // Normalisation adapts notes to the destination drum lane.
    if(ownerTrackIsDrum())for(auto& n:copied)n.pitch=owner.project.tracks[(size_t)owner.selectedTrack].engine.midiNote>=0?owner.project.tracks[(size_t)owner.selectedTrack].engine.midiNote:fgd::defaultNote(owner.selectedTrack-4);
    retainSelection(copied);
}
bool PianoRoll::editKeys(const juce::KeyPress& key){
    // Never let a shortcut change the project underneath an unfinished gesture.
    if(!before.empty()||selecting||editingVelocity)return true;
    const int code=key.getKeyCode();const bool command=key.getModifiers().isCommandDown();
    if(command&&code=='X'){if(!selected.empty()){owner.copyButton.onClick();deleteSelected();}return true;}
    const bool duplicate=command&&code=='D';
    const bool horizontal=code==juce::KeyPress::leftKey||code==juce::KeyPress::rightKey;
    const bool vertical=code==juce::KeyPress::upKey||code==juce::KeyPress::downKey;
    if(!duplicate&&!horizontal&&!vertical)return false;
    if((command&&!duplicate)||key.getModifiers().isAltDown())return false;
    auto edited=owner.clip().notes;std::vector<Note> wanted;
    double first=owner.clipLength(),last=0;int low=127,high=0,minLane=11,maxLane=0;
    for(int i:selected)if(i>=0&&i<(int)edited.size()){
        const auto& n=edited[(size_t)i];wanted.push_back(n);first=std::min(first,n.start);last=std::max(last,n.start+n.length);low=std::min(low,n.pitch);high=std::max(high,n.pitch);minLane=std::min(minLane,n.lane);maxLane=std::max(maxLane,n.lane);}
    if(wanted.empty())return true;
    double delta=horizontal?(key.getModifiers().isShiftDown()?4.:grid)*(code==juce::KeyPress::leftKey?-1:1):0;
    if(duplicate){delta=std::ceil((last-first)/grid-1e-9)*grid;if(last+delta>owner.clipLength()+1./960.)return true;}
    else delta=std::clamp(delta,-first,std::max(-first,owner.clipLength()-last));
    int semitones=vertical?(code==juce::KeyPress::upKey?1:-1)*(key.getModifiers().isShiftDown()?12:1):0;
    int lanes=0;if(laneView()){if(owner.selectedTrack==3&&vertical)lanes=std::clamp(code==juce::KeyPress::upKey?-1:1,-minLane,11-maxLane);semitones=0;}
    else semitones=std::clamp(semitones,-low,127-high);
    if(!duplicate&&delta==0&&semitones==0&&lanes==0)return true;
    if(duplicate){size_t count=wanted.size();for(const auto& t:owner.project.tracks)for(const auto& c:t.clips)count+=c.notes.size()+c.loopSource.size();if(count>2000000)return true;}
    for(auto& n:wanted){n.start+=delta;n.pitch+=semitones;n.lane+=lanes;}
    if(duplicate)edited.insert(edited.end(),wanted.begin(),wanted.end());else{size_t k=0;for(int i:selected)if(i>=0&&i<(int)edited.size())edited[(size_t)i]=wanted[k++];}
    owner.checkpoint();owner.clip().notes=std::move(edited);owner.changed();retainSelection(wanted);return true;
}

bool MainComponent::verifyNavigation(const juce::File& folder){
    const auto saved=project;const auto savedUndo=undoStack,savedRedo=redoStack;const auto board=clipboard;
    const int track=selectedTrack,section=selectedSection;const double oldZoom=arrangement.pixelsPerBar,oldPianoZoom=piano.pixelsPerBeat;
    const bool savedDirty=dirty;audio.stop();editCursor=-1;
    project.sections={{Section::main,16,"Main"},{Section::breakdown,8,"Break"}};
    for(auto& t:project.tracks){t.freeTimeline=false;t.clips.assign(2,{});}project.normalise();select(0,0);showArrangement();
    arrangement.pixelsPerBar=80;updateSizes();arrangementPort.setViewPosition(260,90);
    bool ok=true;juce::String failed;auto stage=[&](const char* name,bool pass){if(!pass){ok=false;failed+=juce::String(name)+"; ";}};
    const auto source=juce::Desktop::getInstance().getMainMouseSource();const auto time=juce::Time::getCurrentTime();
    auto event=[&](juce::Component& c,float x,float y,int mods){const juce::Point<float> p(x,y);return juce::MouseEvent(source,p,juce::ModifierKeys(mods),1,0,0,0,0,&c,&c,time,p,time,1,false);};
    const auto json=juce::JSON::toString(projectJson(project),false,17);const auto midi=midiFile(project);
    const double pointer=430;double anchor=(arrangementPort.getViewPositionX()+pointer-headerWidth)/arrangement.pixelsPerBar;
    followPlayhead=true;juce::MouseWheelDetails wheel{};wheel.deltaY=.2f;
    arrangement.mouseWheelMove(event(arrangement,arrangementPort.getViewPositionX()+(float)pointer,arrangementPort.getViewPositionY()+110,juce::ModifierKeys::ctrlModifier),wheel);
    stage("pointer anchored timeline wheel zoom",std::abs((arrangementPort.getViewPositionX()+pointer-headerWidth)/arrangement.pixelsPerBar-anchor)<1./arrangement.pixelsPerBar&&!followPlayhead);
    const auto navigationUndo=undoStack.size();zoomSlider.setValue(arrangement.pixelsPerBar*1.1,juce::sendNotificationSync);
    stage("zoom slider leaves undo untouched",undoStack.size()==navigationUndo);
    const int x=arrangementPort.getViewPositionX();wheel.deltaY=-.2f;
    arrangement.mouseWheelMove(event(arrangement,(float)x+430,arrangementPort.getViewPositionY()+110,juce::ModifierKeys::shiftModifier),wheel);
    stage("horizontal wheel scroll",arrangementPort.getViewPositionX()>x);
    arrangement.mouseWheelMove(event(arrangement,arrangementPort.getViewPositionX()+430,arrangementPort.getViewPositionY()+16,0),wheel);
    stage("navigation never edits musical data",json==juce::JSON::toString(projectJson(project),false,17)&&midi==midiFile(project));
    arrangement.selectedClips={0,1};zoomSelectionButton.onClick();
    stage("fit selected timeline span",arrangementPort.getViewPositionX()==0&&project.length()/4*arrangement.pixelsPerBar<arrangementPort.getWidth()-headerWidth);
    loopSelectionButton.onClick();stage("loop complete clip selection",project.loop&&project.loopStart==0&&project.loopEnd==96);undo(false);
    stage("undo loop locators",json==juce::JSON::toString(projectJson(project),false,17));
    select(0,0);showEditor();clip().notes={{1,.5,60,100,0},{2,.5,64,90,0}};changed();piano.selected={0,1};
    piano.pixelsPerBeat=76;updateSizes();pianoPort.setViewPosition(200,0);anchor=(pianoPort.getViewPositionX()+pointer-piano.leftMargin())/piano.pixelsPerBeat;
    wheel.deltaY=.1f;piano.mouseWheelMove(event(piano,pianoPort.getViewPositionX()+(float)pointer,80,juce::ModifierKeys::ctrlModifier),wheel);
    stage("pointer anchored MIDI wheel zoom",std::abs((pianoPort.getViewPositionX()+pointer-piano.leftMargin())/piano.pixelsPerBeat-anchor)<1./piano.pixelsPerBeat);
    piano.keyPressed(juce::KeyPress(juce::KeyPress::leftKey,juce::ModifierKeys::shiftModifier,0));
    stage("note nudge preserves group at boundary",clip().notes[0].start==0&&clip().notes[1].start==1&&piano.selected.size()==2);
    piano.keyPressed(juce::KeyPress(juce::KeyPress::upKey,juce::ModifierKeys::shiftModifier,0));
    stage("octave transpose",clip().notes[0].pitch==72&&clip().notes[1].pitch==76);
    piano.keyPressed(juce::KeyPress('D',juce::ModifierKeys::commandModifier,0));
    stage("duplicate notes selects complete copy",clip().notes.size()==4&&piano.selected.size()==2&&clip().notes[2].start==1.5&&clip().notes[3].start==2.5);
    piano.keyPressed(juce::KeyPress('X',juce::ModifierKeys::commandModifier,0));
    stage("cut selected MIDI only",clip().notes.size()==2&&clipboard.size()==2);
    seekTimeline(8);piano.keyPressed(juce::KeyPress('V',juce::ModifierKeys::commandModifier,0));
    stage("paste relative phrase at cursor",clip().notes.size()==4&&clip().notes[2].start==8&&clip().notes[3].start==9&&piano.selected.size()==2);
    loopSelection();stage("loop selected MIDI",project.loop&&project.loopStart==8&&project.loopEnd==9.5);
    const auto notes=clip().notes;
    piano.fitNotes();pianoPort.setViewPosition(0,0);const auto selectionUndoSize=undoStack.size();const auto n=clip().notes.front();const float nx=piano.leftMargin()+(float)(n.start*piano.pixelsPerBeat)+2,ny=piano.noteY(n)+piano.noteHeight()/2;
    piano.mouseDown(event(piano,nx,ny,juce::ModifierKeys::leftButtonModifier));piano.mouseUp(event(piano,nx,ny,0));
    stage("selection click creates no undo step",undoStack.size()==selectionUndoSize&&clip().notes==notes);
    seekTimeline(clipLength()-.25);pasteButton.onClick();stage("reject truncated MIDI phrase",clip().notes==notes);
    select(4,0);showEditor();clip().notes={{1,.25,36,100,0}};changed();piano.selected={0};const auto drum=clip().notes;
    piano.keyPressed(juce::KeyPress(juce::KeyPress::upKey));stage("drum pitch stays isolated",clip().notes==drum);
    // A key during an active clip gesture must not undo the source underneath mouseUp.
    showArrangement();arrangement.clipTrack=4;arrangement.clipSource=0;arrangement.clipTarget=4;arrangement.clipMoving=true;
    const auto undoSize=undoStack.size();arrangement.keyPressed(juce::KeyPress('Z',juce::ModifierKeys::commandModifier,0));
    stage("protect active clip gesture",undoStack.size()==undoSize);arrangement.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));
    stage("cancel active clip gesture",!arrangement.clipMoving&&arrangement.clipTrack<0);
    // Creation plus generation is one user operation, including on a previously empty track.
    project.tracks[4].freeTimeline=true;project.tracks[4].clips.clear();noClipSelection=true;selectedClip=-1;seekTimeline(4);const auto empty=project;
    regenerate(Scope::clip);stage("generate empty track at the chosen cursor",project.tracks[4].clips.size()==1&&project.clipStart(4,0)==4&&timelinePosition()==4);
    undo(false);stage("one undo for generate into an empty track",project.tracks[4].clips.empty()&&project.tracks[4].freeTimeline==empty.tracks[4].freeTimeline);
    project=saved;clipboard=board;editCursor=-1;arrangement.pixelsPerBar=oldZoom;piano.pixelsPerBeat=oldPianoZoom;
    selectedClip=-1;noClipSelection=false;arrangement.selectedClips.clear();select(track,section);showArrangement();changed();
    undoStack=savedUndo;redoStack=savedRedo;dirty=savedDirty;refreshControls();
    folder.getChildFile("daw-navigation.txt").replaceWithText(ok?"PASS anchored wheel zoom, scrolling, selection zoom/loop, MIDI nudge/transpose/duplicate/cut/paste, drum isolation, gesture safety and atomic undo":"FAIL "+failed);
    return ok;
}
// Native presentation regressions use the real controls. Navigation must not edit
// musical data; the small master meter must be driven by actual callback audio.
bool MainComponent::verifyPresentation(const juce::File& folder){
    const auto saved=project;const auto undos=undoStack,redos=redoStack;const auto midi=midiFile(project);
    const bool wasDirty=dirty,wasInspector=inspectorVisible,wasMixer=mixerVisible,wasTime=timeDisplay;
    const int track=selectedTrack,section=selectedSection;const double zoom=arrangement.pixelsPerBar,cursor=editCursor,at=audio.position();const auto scroll=arrangementPort.getViewPosition();
    const auto timeSetting=Localisation::setting("timelineTimeDisplay","0");
    bool ok=true;juce::String failures;
    auto stage=[&](const char* name,bool pass){if(!pass){ok=false;failures+=juce::String(name)+"; ";}};
    auto snapshot=[&](const char* name){if(auto out=folder.getChildFile(name).createOutputStream())return juce::PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()),*out);return false;};
    auto json=[&]{return juce::JSON::toString(projectJson(project),false,17);};const auto original=json();
    audio.stop();showArrangement();inspectorVisible=false;resized();arrangementPort.setViewPosition(0,0);
    const auto source=juce::Desktop::getInstance().getMainMouseSource();const auto now=juce::Time::getCurrentTime();const juce::Point<float> pos(64.f,80.f);
    arrangement.mouseDoubleClick(juce::MouseEvent(source,pos,juce::ModifierKeys::leftButtonModifier,1,0,0,0,0,&arrangement,&arrangement,now,pos,now,2,false));
    stage("double-click track header reveals inspector",inspectorVisible&&selectedTrack==trackAtRow(0)&&page==Page::arrangement&&inspectorPort.getWidth()>0);
    auto* gain=dynamic_cast<juce::Slider*>(trackInspector.findChildWithID("track-gain"));auto* pan=dynamic_cast<juce::Slider*>(trackInspector.findChildWithID("track-pan"));
    stage("inspector rotaries have visible editable values",gain&&pan&&gain->getSliderStyle()==juce::Slider::RotaryHorizontalVerticalDrag&&pan->getSliderStyle()==juce::Slider::RotaryHorizontalVerticalDrag&&gain->getTextBoxPosition()==juce::Slider::TextBoxBelow&&gain->getHeight()>60);
    if(gain){const auto before=project.tracks[(size_t)selectedTrack].gain;gain->setValue(3,juce::sendNotificationSync);stage("rotary gain changes audio without changing MIDI",std::abs(project.tracks[(size_t)selectedTrack].gain-juce::Decibels::decibelsToGain(3.))<1e-6&&midiFile(project)==midi);undo(false);stage("one undo restores rotary gain",std::abs(project.tracks[(size_t)selectedTrack].gain-before)<1e-9);}
    if(pan){const double before=project.tracks[(size_t)selectedTrack].pan;pan->setValue(-25,juce::sendNotificationSync);stage("rotary pan uses the existing stereo range",std::abs(project.tracks[(size_t)selectedTrack].pan+.25)<1e-9);undo(false);stage("one undo restores rotary pan",std::abs(project.tracks[(size_t)selectedTrack].pan-before)<1e-9);}
    for(auto* knob:{&masterSlider,&densitySlider,&swingSlider,&humanizeSlider})stage("home controls remain functional rotaries",knob->getSliderStyle()==juce::Slider::RotaryHorizontalVerticalDrag&&knob->getBounds().getHeight()>=40);
    // Show the song at a nonzero time. Only the display preference changes.
    seekTimeline(std::min(32.,project.length()));timeDisplay=false;showPosition(timelinePosition());const auto beforeMode=json();const auto count=undoStack.size();const auto position=audio.position();const auto grid=timelineSnap;
    positionModeButton.onClick();stage("clock mode preserves project, grid, undo and playhead",timeDisplay&&json()==beforeMode&&midiFile(project)==midi&&undoStack.size()==count&&audio.position()==position&&timelineSnap==grid&&positionLabel.getText()==timeText(timelinePosition()*60/project.settings.bpm,3));
    stage("clock and ruler convert actual beat durations",timeText(61.234,3)=="1:01.234"&&timeText(3661.2)=="1:01:01"&&rulerText(4)==timeText(240/project.settings.bpm,2)&&positionLegend.startsWith("-"));
    stage("elapsed-time workspace screenshot",snapshot("workspace-time.png"));
    positionModeButton.onClick();stage("bar display is restored without edits",!timeDisplay&&json()==beforeMode&&undoStack.size()==count&&rulerText(4)=="2");
    const double beforeZoom=arrangement.pixelsPerBar;timelineZoomIn.onClick();stage("footer zoom-in works",arrangement.pixelsPerBar>beforeZoom);timelineZoomOut.onClick();stage("footer zoom-out reverses scale",std::abs(arrangement.pixelsPerBar-beforeZoom)<1e-6);
    timelineZoom.setValue(12,juce::sendNotificationSync);stage("footer zoom slider controls the timeline",std::abs(arrangement.pixelsPerBar-12)<1e-6);timelineFitButton.onClick();
    stage("fit covers the complete song and keeps footer controls separate",216+project.length()/4*arrangement.pixelsPerBar<arrangementPort.getWidth()&&arrangementPort.getViewPositionX()==0&&chordsLabel.getRight()<timelineZoomOut.getX()&&timelineFitButton.getRight()==mixer.getRight());
    stage("view controls leave music and undo unchanged",json()==beforeMode&&midiFile(project)==midi&&undoStack.size()==count&&audio.position()==position);
    stage("new workspace screenshot",snapshot("workspace-refined.png"));
    mixerVisible=false;mixerToggle.setToggleState(false,juce::dontSendNotification);resized();mixer.tick();
    bool hiddenPort=false;for(auto* child:mixer.getChildren())if(auto* port=dynamic_cast<juce::Viewport*>(child))hiddenPort=!port->isVisible();stage("collapsed mixer retains compact master display",mixer.isVisible()&&mixer.getHeight()==28&&hiddenPort);
    // A real live guide note, through the main callback, drives the compact meter.
    for(auto& t:project.tracks)t.solo=false;project.tracks[0].mute=false;project.tracks[0].gain=1;project.masterGain=.8;audio.collectGarbage();audio.updateMix(project);refreshControls();
    audio.liveMidi(0,juce::MidiMessage::noteOn(1,69,.8f),false);juce::AudioBuffer<float> block(2,512);juce::AudioIODeviceCallbackContext context;
    for(int i=0;i<12;++i){audio.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);mixer.tick();}
    const auto image=mixer.createComponentSnapshot(mixer.getLocalBounds());const auto pixel=image.getPixelAt(80,7);
    stage("compact meter follows real stereo master audio",audio.masterLevel()>.001f&&pixel.getGreen()>pixel.getRed()*1.2f);
    stage("collapsed live master screenshot",snapshot("workspace-master-meter.png"));
    audio.liveMidi(0,juce::MidiMessage::noteOff(1,69),false);audio.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);audio.stop();
    mixerVisible=true;mixerToggle.setToggleState(true,juce::dontSendNotification);resized();bool shownPort=false;for(auto* child:mixer.getChildren())if(auto* port=dynamic_cast<juce::Viewport*>(child))shownPort=port->isVisible();stage("reopening mixer restores faders",shownPort&&mixer.getHeight()>28);
    project=saved;timeDisplay=wasTime;Localisation::setSetting("timelineTimeDisplay",timeSetting);inspectorVisible=wasInspector;mixerVisible=wasMixer;inspectorToggle.setToggleState(wasInspector,juce::dontSendNotification);mixerToggle.setToggleState(wasMixer,juce::dontSendNotification);arrangement.pixelsPerBar=zoom;editCursor=cursor;
    changed();audio.seek(at);undoStack=undos;redoStack=redos;dirty=wasDirty;select(track,section);resized();arrangementPort.setViewPosition(scroll);showPosition(timelinePosition());stage("test restores musical project",json()==original&&midiFile(project)==midi);
    folder.getChildFile("presentation.txt").replaceWithText(ok?"PASS header double-click/inspector; rotary gain/pan Undo; home knobs; real master meter and mixer reopening; footer zoom/fit; elapsed/remaining time and beat-aligned ruler; unchanged MIDI, project, grid and Undo":"FAIL "+failures);
    return ok;
}

}
