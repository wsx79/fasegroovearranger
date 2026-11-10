// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MainComponent.h"
#include "Localisation.h"
namespace fga {
bool MainComponent::verifyPianoRoll(const juce::File& folder){
#if !FGA_SELF_TESTS
    return false;
#else
    // This CLI runs with the isolated self-test settings file. Leave later native
    // tests a known editing view rather than the deliberately hidden final fixture.
    struct ResetPreferences{Localisation::Language language=Localisation::current();~ResetPreferences(){
        for(const auto& p:std::initializer_list<std::pair<const char*,const char*>>{{"pianoScaleGuide","0"},{"pianoActiveNotes","1"},{"pianoGhostTrack","-1"},{"pianoExpressionLane","0"},{"pianoGuitarNotes","0"},{"pianoGrid","3"}})Localisation::setSetting(p.first,p.second);
        Localisation::setCurrent(language);
    }} resetPreferences;
    folder.createDirectory();bool ok=true;juce::String report;
    auto check=[&](const char* name,bool value){report+=(value?"PASS ":"FAIL ")+juce::String(name)+"\n";ok=value&&ok;};
    auto capture=[&](const char* name){if(auto out=folder.getChildFile(name).createOutputStream()){juce::PNGImageFormat png;png.writeImageToStream(createComponentSnapshot(getLocalBounds()),*out);}};
    audio.stop();setSize(1440,980);project=newProject(0);project.settings.key=0;project.settings.scale=0;
    project.sections={{Section::main,2,"C major"},{Section::main,2,"F sharp major"}};
    auto& h=project.sections[1].harmony;h.enabled=true;h.key=6;h.scale=0;
    for(auto& t:project.tracks){t.freeTimeline=false;t.clips.assign(2,{});}project.normalise();
    project.tracks[0].clips[0].notes={{1,2,60,100,0},{1,2,64,100,0},{1,2,67,100,0}};
    project.tracks[1].clips[0].notes=project.tracks[0].clips[0].notes;
    Localisation::setCurrent(Localisation::Language::italian);applyLanguage();refreshControls();select(0,0);showArrangement();showEditor();
    check("Italian reference-view controls are translated",activeNotesButton.getButtonText()==juce::String::fromUTF8("NOTE TRACCIA"));
    check("Select is the default editor tool",piano.tool==1&&selectTool.getToggleState()&&!drawTool.getToggleState());
    drawTool.setToggleState(true,juce::dontSendNotification);drawTool.onClick();showArrangement();showEditor();
    check("Re-entering the roll selects the Select tool",piano.tool==1&&selectTool.getToggleState()&&!drawTool.getToggleState());
    if(!piano.showScaleGuide)scaleGuideButton.onClick();piano.ghostTrack.setSelectedId(3,juce::sendNotificationSync);gridBox.setSelectedId(4,juce::sendNotificationSync);
    piano.expressionLane.setSelectedId(1,juce::sendNotificationSync);piano.setActiveNotesVisible(true);piano.fitNotes();
    const auto notes=project.tracks[0].clips[0].notes;const auto json=juce::JSON::toString(projectJson(project),false,17);const auto history=undoStack.size();
    const float x=piano.leftMargin()+(float)(1.5*piano.pixelsPerBeat),y=piano.noteY(notes[0])+piano.noteHeight()/2;
    auto pixel=[&]{return piano.createComponentSnapshot(piano.getLocalBounds()).getPixelAt((int)x,(int)y);};
    const auto activePixel=pixel();capture("active-and-ghost.png");piano.selected={0};activeNotesButton.onClick();const auto ghostPixel=pixel();capture("ghost-only.png");
    check("Ghost notes remain visible under exactly overlapping active notes",!piano.showActiveNotes&&activePixel!=ghostPixel&&piano.selected.empty()&&!drawTool.isEnabled()&&!pasteButton.isEnabled());
    piano.ghostTrack.setSelectedId(1,juce::sendNotificationSync);const auto blankPixel=pixel();
    check("The revealed pixels belong to the reference notes",blankPixel!=ghostPixel);piano.ghostTrack.setSelectedId(3,juce::sendNotificationSync);
    const auto source=juce::Desktop::getInstance().getMainMouseSource();const auto time=juce::Time::getCurrentTime();const juce::Point<float> point{x,y};
    juce::MouseEvent event(source,point,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&piano,&piano,time,point,time,1,false);
    piano.mouseDown(event);piano.mouseDrag(event);piano.mouseUp(event);piano.selected={0};piano.deleteSelected();piano.editVelocity(point);piano.eraseAt(point);
    clipboard={{0,.25,72,100,0}};piano.pasteNotes();piano.keyPressed(juce::KeyPress(juce::KeyPress::upKey));keyPressed(juce::KeyPress('H'));
    check("Hiding notes neither edits MIDI nor creates Undo, even through editing gestures",json==juce::JSON::toString(projectJson(project),false,17)&&history==undoStack.size());piano.selected.clear();
    const auto eventsBefore=events(project,0);piano.setActiveNotesVisible(true);
    check("Show/hide keeps playback events and the project unchanged",eventsBefore.size()==events(project,0).size()&&json==juce::JSON::toString(projectJson(project),false,17)&&history==undoStack.size()&&drawTool.isEnabled());
    showArrangement();showEditor();check("Scale guide, reference track and grid survive closing the roll",piano.showScaleGuide&&scaleGuideButton.getToggleState()&&piano.ghost==1&&gridBox.getSelectedId()==4&&piano.grid==.125);
    piano.expressionLane.setSelectedId(3,juce::sendNotificationSync);piano.guitarNotes=true;Localisation::setSetting("pianoGuitarNotes","1");piano.setActiveNotesVisible(false);
    {PianoRoll restored(*this);check("Fresh roll restores visual settings from persistent preferences",restored.showScaleGuide&&!restored.showActiveNotes&&restored.ghost==1&&restored.expressionMode==2&&restored.guitarNotes&&restored.grid==.125&&restored.tool==1);}
    piano.expressionLane.setSelectedId(1,juce::sendNotificationSync);piano.setActiveNotesVisible(true);
    project.tracks[0].freeTimeline=true;project.tracks[0].clips.assign(1,{});project.tracks[0].clips[0].span=16;project.tracks[0].clips[0].notes=notes;
    select(0,0);showEditor();check("Scale guide follows independent section harmony inside a free clip",piano.noteInScale(60,0)&&!piano.noteInScale(60,8)&&piano.noteInScale(61,8));capture("section-scale-guide.png");
    project.tracks[1].freeTimeline=true;project.tracks[1].clips.assign(1,{});auto& reference=project.tracks[1].clips[0];reference.position=0;reference.span=32;reference.notes={{0,32,96,100,0}};
    project.tracks[0].clips[0].position=4;project.tracks[0].clips[0].span=8;piano.setActiveNotesVisible(false);piano.fitNotes();const auto ghosts=piano.ghostNotes();
    check("Reference notes clip to the visible take and fit their own pitch range",ghosts.size()==1&&ghosts[0].start==0&&ghosts[0].length==8&&ghosts[0].pitch==96&&piano.lowNote<=96&&piano.highNote>=96);
    folder.getChildFile("piano-roll.txt").replaceWithText(report);return ok;
#endif
}
}
