// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MainComponent.h"
#include "UiText.h"
namespace fga {
namespace {
std::vector<Clip> chosenClips(const Project& project,int track,const std::set<int>& indices){
    auto visible=project;visible.detachTrack(track);std::vector<Clip> chosen;
    for(int index:indices)if(index>=0&&index<(int)project.tracks[(size_t)track].clips.size()){
        const double end=project.clipStart(track,index)+project.clipLength(track,index);const int mapped=visible.clipAtBeat(track,end-1./1920.);
        if(mapped>=0){const auto& c=visible.tracks[(size_t)track].clips[(size_t)mapped];if(chosen.empty()||chosen.back().position!=c.position)chosen.push_back(c);}}
    return chosen;
}
}
void MainComponent::moveSelectedClips(int sourceTrack,int anchor,int targetTrack,double beat,bool copy){
    auto indices=arrangement.selectedClips;indices.insert(anchor);auto chosen=chosenClips(project,sourceTrack,indices);if(chosen.empty())return;
    const double delta=std::clamp(beat-project.clipStart(sourceTrack,anchor),-chosen.front().position,131072.-chosen.back().position);
    const double sourceEnd=project.clipStart(sourceTrack,anchor)+project.clipLength(sourceTrack,anchor);double focus=chosen.front().position+delta;
    for(const auto& c:chosen)if(sourceEnd>c.position&&sourceEnd<=c.position+c.span+1./1920.)focus=c.position+delta;
    auto edited=project;if(!copy){edited.detachTrack(sourceTrack);for(const auto& c:chosen){const int index=edited.clipAtBeat(sourceTrack,c.position+1./1920.);if(index>=0)edited.deleteClip(sourceTrack,index);}}
    std::vector<double> starts;for(auto c:chosen){const double at=c.position+delta;starts.push_back(at);if(edited.placeClip(targetTrack,std::move(c),at)<0){message("Clip edit exceeds the project limits.",true);return;}}
    checkpoint();project=std::move(edited);selectClip(targetTrack,project.clipAtBeat(targetTrack,focus+1./1920.));arrangement.selectedClips.clear();for(double at:starts){const int index=project.clipAtBeat(targetTrack,at+1./1920.);if(index>=0)arrangement.selectedClips.insert(index);}changed();
}
void MainComponent::seekTimeline(double beat){
    beat=std::clamp(beat,0.,131072.);editCursor=beat>project.length()?beat:-1;audio.seek(std::min(beat,project.length()));arrangement.repaint();
}
void MainComponent::togglePlayback(){
    if(!audio.isPlaying()&&editCursor>=0){if(editCursor>project.length())return;audio.seek(editCursor);}editCursor=-1;audio.play(!audio.isPlaying());
}
// Live click on every beat (accented bar); never part of an export.
void MainComponent::toggleMetronome(){audio.setMetronome(!audio.metronomeOn());metronomeButton.setToggleState(audio.metronomeOn(),juce::dontSendNotification);message(audio.metronomeOn()?"Metronome on.":"Metronome off.");}
void MainComponent::selectClip(int track,int index,bool keepSelection,double at){
    track=std::clamp(track,0,(int)project.tracks.size()-1);const bool same=selectedTrack==track;const auto previous=arrangement.selectedClips;
    const int guide=index>=0?project.sectionAtBeat(project.clipStart(track,index)):project.sectionAtBeat(at>=0?at:timelinePosition());
    select(track,guide);selectedClip=index;noClipSelection=index<0;selectionBeat=index>=0?project.clipStart(track,index)+1./1920.:(at>=0?at:timelinePosition());
    arrangement.selectedClips=keepSelection&&same?previous:std::set<int>{};if(index>=0)arrangement.selectedClips.insert(index);
    refreshTrack();updateSizes();piano.fitNotes();trackInspector.refresh();arrangement.repaint();piano.repaint();
}
void MainComponent::ensureTimelineClip(bool addUndo){
    auto& t=project.tracks[(size_t)selectedTrack];if(!noClipSelection&&(!t.freeTimeline||(clipIndex()>=0&&clipIndex()<(int)t.clips.size())))return;
    Clip c;c.span=4;auto edited=project;const int placed=edited.placeClip(selectedTrack,std::move(c),arrangement.snapBeat(timelinePosition()));
    if(placed>=0){if(addUndo)checkpoint();project=std::move(edited);selectedClip=placed;noClipSelection=false;selectClip(selectedTrack,placed);changed();}
}
void MainComponent::timelineCommand(int command){
    if(loadingPlugin)return;
    auto indices=arrangement.selectedClips;if(indices.empty()&&clipIndex()>=0)indices.insert(clipIndex());
    auto chosen=chosenClips(project,selectedTrack,indices);
    if(command==1||command==2){if(chosen.empty())return;timelineClipboard=chosen;const double origin=chosen.front().position;for(auto& c:timelineClipboard)c.position-=origin;if(command==2){message("Clips copied. Paste places them at the playback cursor.");return;}}
    if(command==3&&timelineClipboard.empty())return;
    if(command!=3&&command!=8&&chosen.empty())return;
    auto edited=project;int selected=-1;double selectedBeat=timelinePosition();std::vector<double> placedStarts;
    if(command==1||command==6){edited.detachTrack(selectedTrack);for(const auto& c:chosen){const int index=edited.clipAtBeat(selectedTrack,c.position+1./1920.);if(index>=0)edited.deleteClip(selectedTrack,index);}}
    if(command==4){const double beat=arrangement.snapBeat(timelinePosition());if(std::none_of(chosen.begin(),chosen.end(),[beat](const Clip& c){return beat>c.position&&beat<c.position+c.span;}))return;const int index=edited.clipAtBeat(selectedTrack,beat);if(index<0)return;selected=edited.splitClip(selectedTrack,index,beat);if(selected<0)return;selectedBeat=beat;}
    if(command==3||command==5||command==8){std::vector<Clip> copies=command==3?timelineClipboard:chosen;double destination=arrangement.snapBeat(timelinePosition());
        if(command==5){destination=0;for(const auto& c:chosen)destination=std::max(destination,c.position+c.span);const double origin=copies.front().position;for(auto& c:copies)c.position-=origin;}
        if(command==8){Clip c;c.span=4;copies={c};}
        for(auto c:copies){const double at=destination+c.position;const int placed=edited.placeClip(selectedTrack,std::move(c),at);placedStarts.push_back(at);if(placed<0){message("Clip edit exceeds the project limits.",true);return;}if(selected<0){selected=placed;selectedBeat=at;}}
        selected=edited.clipAtBeat(selectedTrack,selectedBeat+1./1920.);
    }
    checkpoint();project=std::move(edited);arrangement.selectedClips.clear();selectedClip=-1;noClipSelection=true;
    if(selected>=0){selectClip(selectedTrack,selected);for(double at:placedStarts){const int index=project.clipAtBeat(selectedTrack,at+1./1920.);if(index>=0)arrangement.selectedClips.insert(index);}}else{refreshTrack();updateSizes();trackInspector.refresh();}piano.clearSelection();changed();arrangement.grabKeyboardFocus();
    message("Timeline edited. Undo restores the previous clips.");
}
bool MainComponent::verifyTimelineEditing(const juce::File& folder){
    const auto saved=project;const int savedTrack=selectedTrack,savedSection=selectedSection;const auto savedZoom=arrangement.pixelsPerBar;const auto board=timelineClipboard;
    audio.stop();editCursor=-1;project.sections={{Section::main,4,"Main"},{Section::breakdown,4,"Break"}};for(auto& t:project.tracks){t.freeTimeline=false;t.clips.assign(2,{});}project.normalise();project.tracks[4].clips[0].notes={{0,.25,36,100,0},{4,.25,36,90,0},{8,.25,36,80,0},{12,.25,36,70,0}};
    arrangement.pixelsPerBar=64;selectedClip=-1;arrangement.selectedClips.clear();showArrangement();selectClip(4,0);changed();arrangementPort.setViewPosition(0,0);bool ok=true;juce::String failed;
    auto stage=[&](const char* label){if(!ok&&failed.isEmpty())failed=label;};
    seekTimeline(4);timelineCommand(4);ok=ok&&project.tracks[4].clips.size()==3&&selectedClip==1&&clipStart()==4&&clipLength()==12;
    seekTimeline(8);keyPressed(juce::KeyPress('E',juce::ModifierKeys::commandModifier,0));ok=ok&&project.tracks[4].clips.size()==4&&selectedClip==2&&clipStart()==8&&clipLength()==8;stage("split button and shortcut");
    const auto splitState=project;selectClip(4,0);arrangement.selectedClips={0,1};
    timelineCommand(2);seekTimeline(24);timelineCommand(3);ok=ok&&arrangement.selectedClips.size()==2&&project.clipAtBeat(4,24)>=0&&project.clipAtBeat(4,28)>=0;stage("paste retains complete clip group selection");
    timelineCommand(5);ok=ok&&arrangement.selectedClips.size()==2&&project.clipAtBeat(4,32)>=0&&project.clipAtBeat(4,36)>=0;stage("repeat duplicate works on the full pasted group");
    project=splitState;selectClip(4,0);arrangement.selectedClips={0,1};changed();
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
    const auto beforeGuide=project.tracks[4].clips;const double guidePlay=audio.position();arrangement.mouseDown(event(278,16));arrangement.mouseDrag(event(310,16));arrangement.mouseUp(event(310,16));
    ok=ok&&project.sections[0].bars==6&&project.tracks[4].clips==beforeGuide&&audio.position()==guidePlay;stage("guide edge drag leaves clips and playhead unchanged");
    selectClip(4,project.clipAtBeat(4,48));arrangement.pixelsPerBar=32;updateSizes();arrangementPort.setViewPosition(0,0);
    if(auto out=folder.getChildFile("timeline-editing.png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(createComponentSnapshot(getLocalBounds()),*out)&&ok;}else ok=false;
    keyPressed(juce::KeyPress('A',juce::ModifierKeys::commandModifier,0));timelineCommand(6);ok=ok&&project.tracks[4].clips.empty();project.normalise();ok=ok&&project.tracks[4].clips.empty();stage("select all and delete all blocks without resurrecting placeholders");
    showEditor();ok=ok&&clipIndex()>=0&&clipLength()==4&&clip().notes.empty();stage("create editable empty clip on an empty track");
    project.sections={{Section::main,4,"Main"},{Section::drop,4,"Drop"},{Section::outro,4,"Outro"}};for(auto& t:project.tracks){t.freeTimeline=false;t.clips.assign(3,{});}project.normalise();project.tracks[4].clips[0].notes={{0,.25,36,100,0}};project.tracks[4].clips[2].notes={{0,.25,36,55,0}};project.resizeClipLoop(4,0,32);selectClip(4,2);project.resizeGuide(0,6);changed();ok=ok&&selectedClip==1&&clipStart()==32&&clip().notes.size()==1&&clip().notes[0].velocity==55;stage("selected clip survives materialisation of fully covered legacy blocks");
    project=saved;editCursor=-1;selectedClip=-1;noClipSelection=false;arrangement.selectedClips.clear();timelineClipboard=board;arrangement.pixelsPerBar=savedZoom;select(savedTrack,savedSection);showArrangement();changed();undoStack.clear();redoStack.clear();refreshControls();
    folder.getChildFile("timeline-editing.txt").replaceWithText(ok?"PASS clip split, copy/cut/paste, duplicate/delete, cross-track drag, drum remapping, Undo, editable guide, independent MIDI editor, project round trip and empty-track editing":"FAIL timeline editing: "+failed);return ok;
}

}
