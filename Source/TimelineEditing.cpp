// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MainComponent.h"
#include "UiText.h"
#include <cmath>
namespace fga {
namespace {
std::vector<ClipId> chosenIds(const std::set<ClipId>& selected,int anchorTrack,int anchor){
    auto ids=selected;if(anchor>=0)ids.insert({anchorTrack,anchor});return {ids.begin(),ids.end()};
}
}
void MainComponent::adoptPlacements(int focusTrack,double focusBeat,const std::vector<ClipPlacement>& placed){
    const int focus=focusTrack>=0?project.clipAtBeat(focusTrack,focusBeat+1./1920.):-1;
    if(focus>=0)selectClip(focusTrack,focus);else if(!placed.empty()){const int index=project.clipAtBeat(placed.front().track,placed.front().beat+1./1920.);if(index>=0)selectClip(placed.front().track,index);}
    arrangement.selectedClips.clear();
    for(const auto& item:placed){const int index=project.clipAtBeat(item.track,item.beat+1./1920.);if(index>=0)arrangement.selectedClips.insert({item.track,index});}
}
void MainComponent::moveSelectedClips(int sourceTrack,int anchor,int targetTrack,double beat,bool copy){
    auto edited=project;std::vector<ClipPlacement> placed;double applied=0;
    const int rowDelta=rowForTrack(targetTrack)-rowForTrack(sourceTrack);
    if(!edited.translateClipGroup(chosenIds(arrangement.selectedClips,sourceTrack,anchor),beat-project.clipStart(sourceTrack,anchor),rowDelta,copy,applied,placed)){message("Clip edit exceeds the project limits.",true);return;}
    const double focus=project.clipStart(sourceTrack,anchor)+applied;const int focusTrack=trackAtRow(rowForTrack(sourceTrack)+rowDelta);
    checkpoint();project=std::move(edited);adoptPlacements(focusTrack,focus,placed);changed();
}
void MainComponent::seekTimeline(double beat){
    beat=std::clamp(beat,0.,131072.);editCursor=beat>project.length()?beat:-1;audio.seek(std::min(beat,project.length()));arrangement.repaint();
}
void MainComponent::togglePlayback(){
    piano.stopPreview();
    if(!audio.isPlaying()&&page==Page::editor&&!piano.selected.empty()){
        std::vector<Note> notes;for(int index:piano.selected)if(juce::isPositiveAndBelow(index,(int)clip().notes.size()))notes.push_back(clip().notes[(size_t)index]);
        if(!notes.empty()){editCursor=-1;audio.playSelectedNotes(selectedTrack,notes,clipStart(),project.loop);return;}
    }
    if(!audio.isPlaying()&&editCursor>=0){if(editCursor>project.length())return;audio.seek(editCursor);}editCursor=-1;audio.play(!audio.isPlaying());
}
// Live click on every beat (accented bar); never part of an export.
void MainComponent::toggleMetronome(){audio.setMetronome(!audio.metronomeOn());metronomeButton.setToggleState(audio.metronomeOn(),juce::dontSendNotification);message(audio.metronomeOn()?"Metronome on.":"Metronome off.");}
void MainComponent::selectClip(int track,int index,bool keepSelection,double at){
    if(audio.isClipPlaying()&&(track!=selectedTrack||index!=selectedClip))audio.play(false);
    track=std::clamp(track,0,(int)project.tracks.size()-1);const auto previous=arrangement.selectedClips;
    const int guide=index>=0?project.sectionAtBeat(project.clipStart(track,index)):project.sectionAtBeat(at>=0?at:timelinePosition());
    select(track,guide);selectedClip=index;noClipSelection=index<0;selectionBeat=index>=0?project.clipStart(track,index)+1./1920.:(at>=0?at:timelinePosition());
    arrangement.selectedClips=keepSelection?previous:std::set<ClipId>{};if(index>=0)arrangement.selectedClips.insert({track,index});
    refreshTrack();updateSizes();piano.fitNotes();trackInspector.refresh();arrangement.repaint();piano.repaint();
}
void MainComponent::ensureTimelineClip(bool addUndo){
    auto& t=project.tracks[(size_t)selectedTrack];if(!noClipSelection&&(!t.freeTimeline||(clipIndex()>=0&&clipIndex()<(int)t.clips.size())))return;
    Clip c;c.span=4;auto edited=project;const int placed=edited.placeClip(selectedTrack,std::move(c),arrangement.snapBeat(timelinePosition()));
    if(placed>=0){if(addUndo)checkpoint();project=std::move(edited);selectedClip=placed;noClipSelection=false;selectClip(selectedTrack,placed);changed();}
}
void MainComponent::timelineCommand(int command){
    if(loadingPlugin)return;
    auto ids=chosenIds(arrangement.selectedClips,selectedTrack,clipIndex());
    std::vector<ClipGroupPiece> pieces;double origin=0,endBeat=0;const bool have=project.collectClipGroup(ids,selectedTrack,pieces,origin,endBeat);
    if(command==1||command==2){if(!have)return;timelineClipboard=pieces;if(command==2){message("Clips copied. Paste places them at the playback cursor.");return;}}
    if(command==3&&timelineClipboard.empty())return;
    if(command!=3&&command!=8&&!have)return;
    auto edited=project;std::vector<ClipPlacement> placed;int splitIndex=-1;double splitBeat=timelinePosition();
    if(command==1||command==6){if(!edited.removeClipGroup(ids)){message("Clip edit exceeds the project limits.",true);return;}}
    if(command==4){const double beat=arrangement.snapBeat(timelinePosition());if(std::none_of(pieces.begin(),pieces.end(),[&](const ClipGroupPiece& piece){return piece.rowDelta==0&&beat>origin+piece.clip.position&&beat<origin+piece.clip.position+piece.clip.span;}))return;const int index=edited.clipAtBeat(selectedTrack,beat);if(index<0)return;splitIndex=edited.splitClip(selectedTrack,index,beat);if(splitIndex<0)return;splitBeat=beat;}
    if(command==3||command==5||command==8){auto copies=command==3?timelineClipboard:pieces;double destination=arrangement.snapBeat(timelinePosition());
        if(command==5)destination=endBeat;
        if(command==8){ClipGroupPiece empty;empty.clip.span=4;copies={empty};}
        if(!edited.placeClipGroup(selectedTrack,destination,copies,placed)){message("Clip edit exceeds the project limits.",true);return;}
    }
    checkpoint();project=std::move(edited);arrangement.selectedClips.clear();selectedClip=-1;noClipSelection=true;
    if(command==4)selectClip(selectedTrack,splitIndex);else if(!placed.empty())adoptPlacements(placed.front().track,placed.front().beat,placed);else{refreshTrack();updateSizes();trackInspector.refresh();}
    piano.clearSelection();changed();arrangement.grabKeyboardFocus();
    message("Timeline edited. Undo restores the previous clips.");
}
bool MainComponent::verifyTimelineEditing(const juce::File& folder){
#if !FGA_SELF_TESTS
return false;
#else
    const auto saved=project;const int savedTrack=selectedTrack,savedSection=selectedSection;const auto savedZoom=arrangement.pixelsPerBar;const auto board=timelineClipboard;
    audio.stop();editCursor=-1;project.sections={{Section::main,4,"Main"},{Section::breakdown,4,"Break"}};for(auto& t:project.tracks){t.freeTimeline=false;t.clips.assign(2,{});}project.normalise();project.tracks[4].clips[0].notes={{0,.25,36,100,0},{4,.25,36,90,0},{8,.25,36,80,0},{12,.25,36,70,0}};
    arrangement.pixelsPerBar=64;selectedClip=-1;arrangement.selectedClips.clear();showArrangement();selectClip(4,0);changed();arrangementPort.setViewPosition(0,0);bool ok=true;juce::String failed;
    auto stage=[&](const char* label){if(!ok&&failed.isEmpty())failed=label;};
    seekTimeline(4);timelineCommand(4);ok=ok&&project.tracks[4].clips.size()==3&&selectedClip==1&&clipStart()==4&&clipLength()==12;
    seekTimeline(8);keyPressed(juce::KeyPress('E',juce::ModifierKeys::commandModifier,0));ok=ok&&project.tracks[4].clips.size()==4&&selectedClip==2&&clipStart()==8&&clipLength()==8;stage("split button and shortcut");
    const auto splitState=project;selectClip(4,0);arrangement.selectedClips={{4,0},{4,1}};
    timelineCommand(2);seekTimeline(24);timelineCommand(3);ok=ok&&arrangement.selectedClips.size()==2&&project.clipAtBeat(4,24)>=0&&project.clipAtBeat(4,28)>=0;stage("paste retains complete clip group selection");
    timelineCommand(5);ok=ok&&arrangement.selectedClips.size()==2&&project.clipAtBeat(4,32)>=0&&project.clipAtBeat(4,36)>=0;stage("repeat duplicate works on the full pasted group");
    project=splitState;selectClip(4,0);arrangement.selectedClips={{4,0},{4,1}};changed();
    auto groupEvent=[&](float x){const juce::Point<float> p(x,76);return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&arrangement,&arrangement,juce::Time::getCurrentTime(),p,juce::Time::getCurrentTime(),1,false);};
    arrangement.mouseDown(groupEvent(232));arrangement.mouseDrag(groupEvent(360));arrangement.mouseUp(groupEvent(360));ok=ok&&project.clipAtBeat(4,0)<0&&project.clipAtBeat(4,4)<0&&project.clipAtBeat(4,8)>=0&&project.clipAtBeat(4,12)>=0&&arrangement.selectedClips.size()==2;stage("selected clip group movement");project=splitState;selectClip(4,2);changed();
    timelineCommand(2);const auto gridCursor=timelinePosition();arrangement.mouseDown(groupEvent(856));arrangement.mouseUp(groupEvent(856));ok=ok&&timelinePosition()==gridCursor;seekTimeline(40);ok=ok&&timelinePosition()==40&&audio.position()==32;togglePlayback();ok=ok&&!audio.isPlaying()&&timelinePosition()==40;timelineCommand(3);ok=ok&&selectedClip==4&&clipStart()==40&&clipLength()==8&&clip().notes.size()==2&&project.sections[0].bars==4;stage("copy paste at cursor beyond guide");togglePlayback();ok=ok&&audio.isPlaying()&&std::abs(audio.position()-40)<.1;togglePlayback();stage("play starts from the pasted clip cursor");
    showEditor();ok=ok&&page==Page::editor&&piano.verifyEditing()&&selectedClip==4&&clipStart()==40;showArrangement();stage("full MIDI editor for independent clip index beyond guide count");
    timelineCommand(1);ok=ok&&project.clipAtBeat(4,40)<0;seekTimeline(48);timelineCommand(3);ok=ok&&clipStart()==48&&clipLength()==8;
    timelineCommand(5);ok=ok&&clipStart()==56&&project.clipAtBeat(4,48)>=0;stage("cut paste and adjacent duplicate");
    Project loaded;ok=ok&&projectFromJson(projectJson(project),loaded).wasOk()&&loaded.tracks[4].freeTimeline&&loaded.tracks[4].clips==project.tracks[4].clips;stage("independent clip project round trip");
    auto event=[&](float x,float y,int mods=0){const juce::Point<float> p(x,y);return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier|mods),1,0,0,0,0,&arrangement,&arrangement,juce::Time::getCurrentTime(),p,juce::Time::getCurrentTime(),1,false);};
    arrangement.pixelsPerBar=16;updateSizes();arrangementPort.setViewPosition(0,0);
    arrangement.mouseDown(event(442,76));arrangement.mouseDrag(event(458,116));arrangement.mouseUp(event(458,116));
    ok=ok&&selectedTrack==5&&clipStart()==60&&project.clipAtBeat(4,56)<0&&clip().notes[0].pitch==38&&clip().notes[0].lane==1;stage("cross-track drag and destination drum mapping");
    undo(false);ok=ok&&project.clipAtBeat(4,56)>=0&&project.clipAtBeat(5,60)<0&&!project.tracks[5].freeTimeline;stage("undo cross-track drag");
    std::vector<double> guideBeats;for(const auto& eventBeat:events(project,4))if(eventBeat.velocity>0&&eventBeat.controller<0)guideBeats.push_back(eventBeat.beat);const double guidePlay=audio.position();arrangement.mouseDown(event(278,16));arrangement.mouseDrag(event(310,16));arrangement.mouseUp(event(310,16));
    std::vector<double> movedBeats;for(const auto& eventBeat:events(project,4))if(eventBeat.velocity>0&&eventBeat.controller<0)movedBeats.push_back(eventBeat.beat);
    bool followed=guideBeats.size()==movedBeats.size();for(size_t i=0;followed&&i<guideBeats.size();++i)followed=std::abs(movedBeats[i]-(guideBeats[i]+(guideBeats[i]>=16?8:0)))<1e-6;
    ok=ok&&project.sections[0].bars==6&&project.sectionStart(1)==24&&followed&&audio.position()==guidePlay;stage("guide edge drag moves later music with the section and leaves the playhead");
    selectClip(4,project.clipAtBeat(4,48));arrangement.pixelsPerBar=32;updateSizes();arrangementPort.setViewPosition(0,0);
    if(auto out=folder.getChildFile("timeline-editing.png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(createComponentSnapshot(getLocalBounds()),*out)&&ok;}else ok=false;
    keyPressed(juce::KeyPress('A',juce::ModifierKeys::commandModifier,0));timelineCommand(6);ok=ok&&project.tracks[4].clips.empty();project.normalise();ok=ok&&project.tracks[4].clips.empty();stage("select all and delete all blocks without resurrecting placeholders");
    showEditor();ok=ok&&clipIndex()>=0&&clipLength()==4&&clip().notes.empty();stage("create editable empty clip on an empty track");
    project.sections={{Section::main,4,"Main"},{Section::drop,4,"Drop"},{Section::outro,4,"Outro"}};for(auto& t:project.tracks){t.freeTimeline=false;t.clips.assign(3,{});}project.normalise();project.tracks[4].clips[0].notes={{0,.25,36,100,0}};project.tracks[4].clips[2].notes={{0,.25,36,55,0}};project.resizeClipLoop(4,0,32);selectClip(4,2);project.resizeGuide(0,6);changed();ok=ok&&selectedClip==2&&std::abs(clipStart()-40)<1e-6&&clip().notes.size()==1&&clip().notes[0].velocity==55&&project.sectionStart(1)==24;stage("selected clip moves with its section when the guide is resized");
    project=saved;editCursor=-1;selectedClip=-1;noClipSelection=false;arrangement.selectedClips.clear();timelineClipboard=board;arrangement.pixelsPerBar=savedZoom;select(savedTrack,savedSection);showArrangement();changed();undoStack.clear();redoStack.clear();refreshControls();
    folder.getChildFile("timeline-editing.txt").replaceWithText(ok?"PASS clip split, copy/cut/paste, duplicate/delete, cross-track drag, drum remapping, Undo, editable guide, independent MIDI editor, project round trip and empty-track editing":"FAIL timeline editing: "+failed);return ok;
#endif
}

}
