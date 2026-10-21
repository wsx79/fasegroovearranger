// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MainComponent.h"
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
    const bool ruler=e.position.y<view.getY()+(editor?40:rulerHeight)&&std::abs(wheel.deltaY)>std::abs(wheel.deltaX);
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
    if(project.loop&&project.loopStart==from&&project.loopEnd==to)return;
    checkpoint();setLoopRange(from,to);
}
void PianoRoll::retainSelection(const std::vector<Note>& wanted){
    selected.clear();const auto& notes=owner.clip().notes;
    for(size_t i=0;i<notes.size();++i)for(const auto& n:wanted)if(notes[i].pitch==n.pitch&&std::abs(notes[i].start-n.start)<1./960.){selected.insert((int)i);break;}
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
    if(owner.selectedTrack>=4)for(auto& n:copied)n.pitch=owner.project.tracks[(size_t)owner.selectedTrack].engine.midiNote>=0?owner.project.tracks[(size_t)owner.selectedTrack].engine.midiNote:fgd::defaultNote(owner.selectedTrack-4);
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
    seekTimeline(8);pasteButton.onClick();
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
    regenerate(Scope::clip);undo(false);stage("one undo for generate into an empty track",project.tracks[4].clips.empty()&&project.tracks[4].freeTimeline==empty.tracks[4].freeTimeline);
    project=saved;clipboard=board;editCursor=-1;arrangement.pixelsPerBar=oldZoom;piano.pixelsPerBeat=oldPianoZoom;
    selectedClip=-1;noClipSelection=false;arrangement.selectedClips.clear();select(track,section);showArrangement();changed();
    undoStack=savedUndo;redoStack=savedRedo;dirty=savedDirty;refreshControls();
    folder.getChildFile("daw-navigation.txt").replaceWithText(ok?"PASS anchored wheel zoom, scrolling, selection zoom/loop, MIDI nudge/transpose/duplicate/cut/paste, drum isolation, gesture safety and atomic undo":"FAIL "+failed);
    return ok;
}
}
