// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "DawWorkspace.h"
#include "MainComponent.h"
#include "UiText.h"
namespace fga {
namespace {
juce::String text(const std::string& s){return juce::String::fromUTF8(s.c_str());}
juce::Colour ink(int track){if(track==0||track==3)return Palette::accent;if(track==1)return Palette::accent2;if(track==2)return Palette::strings;const int lane=track-4;return lane==0?Palette::accent:lane<=3?Palette::accent2:lane<=6?Palette::cymbals:lane<=10?Palette::percussion:Palette::toms;}
float db(double gain){return gain<=.001?-60.f:juce::Decibels::gainToDecibels((float)gain,-60.f);}
double amplitude(double value){return value<=-59.99?0.:juce::Decibels::decibelsToGain(value);}
void caption(juce::Graphics& g,const juce::String& s,juce::Rectangle<int> bounds){g.setColour(Palette::dim);g.setFont(FamilyTheme::font(10,true));g.drawText(tr(s),bounds,juce::Justification::centredLeft);}
void slider(juce::Slider& s,double low,double high,const char* suffix){s.setRange(low,high,.1);s.setSliderStyle(juce::Slider::LinearHorizontal);s.setTextBoxStyle(juce::Slider::TextBoxRight,false,60,20);s.setTextValueSuffix(suffix);s.setDoubleClickReturnValue(true,0);}
void tip(juce::Component& c,const char* s){c.getProperties().set("fgaTip",s);}
}
InspectorResizer::InspectorResizer(MainComponent& m):owner(m){setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);tip(*this,"Drag the inspector's right edge to resize its width; double-click to restore the default.");}
void InspectorResizer::paint(juce::Graphics& g){g.setColour(Palette::dim.withAlpha(isMouseOverOrDragging()?.85f:.4f));const int middle=getHeight()/2;g.fillRoundedRectangle(2.f,(float)middle-18,2.f,36.f,1.f);g.fillRoundedRectangle(6.f,(float)middle-18,1.f,36.f,.5f);}
void InspectorResizer::mouseDown(const juce::MouseEvent& e){dragOrigin=e.getEventRelativeTo(&owner).position.x;initialWidth=owner.inspectorWidth;}
void InspectorResizer::mouseDrag(const juce::MouseEvent& e){owner.inspectorWidth=std::clamp(initialWidth+juce::roundToInt(e.getEventRelativeTo(&owner).position.x-dragOrigin),184,std::max(184,std::min(480,owner.getWidth()/3)));owner.resized();}
void InspectorResizer::mouseUp(const juce::MouseEvent&){Localisation::setSetting("inspectorWidth",owner.inspectorWidth);}
void InspectorResizer::mouseDoubleClick(const juce::MouseEvent&){owner.inspectorWidth=216;owner.resized();Localisation::setSetting("inspectorWidth",owner.inspectorWidth);}

TrackInspector::TrackInspector(MainComponent& m):owner(m){
    for(auto* c:std::initializer_list<juce::Component*>{&name,&source,&section,&gain,&pan,&instrument,&editor,&mute,&solo,&lockTrack,&lockClip,&internal,&sample,&plugin,&pluginEditor,&regenerate,&loop})addAndMakeVisible(c);
    name.setFont(FamilyTheme::font(17,true));source.setFont(FamilyTheme::font(11));source.setColour(juce::Label::textColourId,Palette::dim);source.setMinimumHorizontalScale(.75f);section.setFont(FamilyTheme::font(12));
    gain.setComponentID("track-gain");pan.setComponentID("track-pan");
    slider(gain,-60,6.0," dB");slider(pan,-100,100,"%");
    gain.onDragStart=[this]{gainGesture=true;owner.checkpoint();};gain.onDragEnd=[this]{gainGesture=false;};
    gain.onValueChange=[this]{if(updating)return;if(!gainGesture)owner.checkpoint();owner.project.tracks[(size_t)owner.selectedTrack].gain=amplitude(gain.getValue());owner.mixChanged();};
    pan.onDragStart=[this]{panGesture=true;owner.checkpoint();};pan.onDragEnd=[this]{panGesture=false;};
    pan.onValueChange=[this]{if(updating)return;if(!panGesture)owner.checkpoint();owner.project.tracks[(size_t)owner.selectedTrack].pan=pan.getValue()/100.;owner.mixChanged();};
    instrument.onClick=[this]{owner.showInstrument();};editor.onClick=[this]{owner.showEditor();};
    mute.onClick=[this]{owner.checkpoint();auto& t=owner.project.tracks[(size_t)owner.selectedTrack];t.mute=!t.mute;owner.mixChanged();};
    solo.onClick=[this]{owner.checkpoint();auto& t=owner.project.tracks[(size_t)owner.selectedTrack];t.solo=!t.solo;owner.mixChanged();};
    lockTrack.onClick=[this]{owner.lockButton.triggerClick();};lockClip.onClick=[this]{owner.lockClipButton.triggerClick();};
    internal.onClick=[this]{owner.soundButton.triggerClick();};sample.onClick=[this]{owner.sampleButton.triggerClick();};plugin.onClick=[this]{owner.showPluginMenu(&plugin);};pluginEditor.onClick=[this]{owner.pluginEditorButton.triggerClick();};
    regenerate.onClick=[this]{owner.regenerate(Scope::clip);};loop.onClick=[this]{owner.sectionLoopButton.triggerClick();};
    tip(gain,"Track volume in dB. This changes audio, not MIDI velocities. Double-click for 0 dB.");tip(pan,"Stereo pan. Double-click to centre.");tip(mute,"Mute this track.");tip(solo,"Listen to soloed tracks.");tip(editor,"Open the selected clip in the full MIDI editor. Shortcut: E.");tip(instrument,"Open the original Fase Groove instrument controls.");
    refresh();setSize(208,626);
}
void TrackInspector::refresh(){
    if(owner.project.tracks.empty())return;updating=true;const auto& t=owner.project.tracks[(size_t)owner.selectedTrack];name.setText(text(t.name),juce::dontSendNotification);name.setColour(juce::Label::textColourId,ink(owner.selectedTrack));
    source.setText(t.instrument=="internal"?tr("Fase Groove engine"):t.instrument=="sample"?juce::File(text(t.instrumentPath)).getFileName():owner.audio.plugin(owner.selectedTrack)?owner.audio.plugin(owner.selectedTrack)->getName():tr("Missing instrument"),juce::dontSendNotification);
    const double bars=owner.clipLength()/4.;section.setText(text(owner.project.sections[(size_t)owner.selectedSection].name)+" / "+juce::String(bars,bars==(int)bars?0:2)+" "+tr("bars"),juce::dontSendNotification);
    gain.setValue(db(t.gain),juce::dontSendNotification);pan.setValue(t.pan*100,juce::dontSendNotification);mute.setToggleState(t.mute,juce::dontSendNotification);solo.setToggleState(t.solo,juce::dontSendNotification);lockTrack.setToggleState(t.locked,juce::dontSendNotification);lockClip.setToggleState(owner.clip().locked,juce::dontSendNotification);internal.setToggleState(t.instrument=="internal",juce::dontSendNotification);pluginEditor.setEnabled(owner.audio.plugin(owner.selectedTrack)!=nullptr&&!owner.loadingPlugin);sample.setEnabled(!owner.loadingPlugin);plugin.setEnabled(!owner.loadingPlugin);internal.setEnabled(!owner.loadingPlugin);updating=false;translateControls(*this);
}
void TrackInspector::paint(juce::Graphics& g){g.fillAll(Palette::panel);g.setColour(Palette::panelEdge);g.drawRect(getLocalBounds());if(getWidth()<=40)return;caption(g,"TRACK INSPECTOR",{12,6,getWidth()-48,18});caption(g,"VOLUME / PAN",{12,190,getWidth()-24,18});caption(g,"SOUND ASSIGNMENT",{12,278,getWidth()-24,20});caption(g,"SELECTED CLIP",{12,480,getWidth()-24,20});}
void TrackInspector::resized(){
    const bool collapsed=getWidth()<=40;for(auto* child:getChildren())child->setVisible(!collapsed);if(collapsed)return;
    const int w=getWidth()-24;
    name.setBounds(12,30,w,26);source.setBounds(12,60,w,24);instrument.setBounds(12,94,w,28);editor.setBounds(12,128,w,28);
    mute.setBounds(12,163,w/2-3,24);solo.setBounds(15+w/2,163,w/2-3,24);gain.setBounds(12,215,w,28);pan.setBounds(12,247,w,28);
    internal.setBounds(12,304,w,26);sample.setBounds(12,334,w,26);plugin.setBounds(12,364,w,26);pluginEditor.setBounds(12,394,w,26);
    lockTrack.setBounds(12,436,w/2-3,26);lockClip.setBounds(15+w/2,436,w/2-3,26);
    section.setBounds(12,504,w,26);regenerate.setBounds(12,538,w,26);loop.setBounds(12,570,w,26);
}

MixerStrip::MixerStrip(MainComponent& m,int ti):owner(m),track(ti){
    for(auto* c:std::initializer_list<juce::Component*>{&name,&gain,&pan,&mute,&solo,&instrument,&clip})addAndMakeVisible(c);
    name.setInterceptsMouseClicks(false,false);name.setFont(FamilyTheme::font(11,true));name.setJustificationType(juce::Justification::centred);name.setMinimumHorizontalScale(.65f);
    slider(gain,-60,6," dB");gain.setSliderStyle(juce::Slider::LinearVertical);gain.setTextBoxStyle(juce::Slider::TextBoxBelow,false,78,20);slider(pan,-100,100,"%");pan.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
    gain.onDragStart=[this]{gainGesture=true;owner.checkpoint();};gain.onDragEnd=[this]{gainGesture=false;};
    gain.onValueChange=[this]{if(updating)return;if(!gainGesture)owner.checkpoint();if(track<0)owner.project.masterGain=amplitude(gain.getValue());else owner.project.tracks[(size_t)track].gain=amplitude(gain.getValue());owner.mixChanged();};
    pan.onDragStart=[this]{panGesture=true;owner.checkpoint();};pan.onDragEnd=[this]{panGesture=false;};
    pan.onValueChange=[this]{if(updating||track<0)return;if(!panGesture)owner.checkpoint();owner.project.tracks[(size_t)track].pan=pan.getValue()/100.;owner.mixChanged();};
    mute.onClick=[this]{owner.checkpoint();auto& t=owner.project.tracks[(size_t)track];t.mute=!t.mute;owner.mixChanged();};solo.onClick=[this]{owner.checkpoint();auto& t=owner.project.tracks[(size_t)track];t.solo=!t.solo;owner.mixChanged();};
    instrument.onClick=[this]{if(track>=0){owner.select(track,owner.selectedSection);owner.showInstrument();}};clip.onClick=[this]{owner.audio.clearClipping();repaint();};
    tip(gain,"Track volume in dB. This changes audio, not MIDI velocities. Double-click for 0 dB.");tip(pan,"Stereo pan. Double-click to centre.");tip(mute,"Mute this track.");tip(solo,"Listen to soloed tracks.");tip(clip,"Master peak overload. Click to clear the indicator.");tip(instrument,"Open the original Fase Groove instrument controls.");refresh();
}
void MixerStrip::refresh(){if(track>=0&&owner.project.tracks.empty())return;updating=true;const bool master=track<0;name.setText(master?tr("MASTER"):text(owner.project.tracks[(size_t)track].name),juce::dontSendNotification);name.setColour(juce::Label::textColourId,master?Palette::text:ink(track));gain.setValue(db(master?owner.project.masterGain:owner.project.tracks[(size_t)track].gain),juce::dontSendNotification);pan.setVisible(!master&&getHeight()>=170);mute.setVisible(!master);solo.setVisible(!master);instrument.setVisible(!master&&getHeight()>=170);clip.setVisible(master);if(!master){auto& t=owner.project.tracks[(size_t)track];pan.setValue(t.pan*100,juce::dontSendNotification);mute.setToggleState(t.mute,juce::dontSendNotification);solo.setToggleState(t.solo,juce::dontSendNotification);instrument.setButtonText(t.instrument=="internal"?"BUILT-IN":t.instrument=="sample"?"SAMPLE":"VST3 / AU");}updating=false;translateControls(*this);repaint();}
void MixerStrip::tick(){peak=std::max(track<0?owner.audio.masterLevel():owner.audio.level(track),peak*.82f);clip.setToggleState(owner.audio.hasClipped(),juce::dontSendNotification);clip.setColour(juce::TextButton::buttonOnColourId,owner.audio.hasClipped()?Palette::toms:Palette::control);repaint();}
void MixerStrip::paint(juce::Graphics& g){g.fillAll(Palette::panel);g.setColour(track==owner.selectedTrack?Palette::accent:Palette::panelEdge);g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f),4,track==owner.selectedTrack?1.5f:1.f);const int top=getHeight()<170?56:99;auto meter=juce::Rectangle<float>((float)getWidth()-18,(float)top,7,(float)std::max(24,getHeight()-top-34));g.setColour(Palette::field);g.fillRoundedRectangle(meter,2);const float level=std::clamp((db(peak)+60)/60.f,0.f,1.f);g.setColour(peak>=1?Palette::toms:peak>.7f?Palette::accent:Palette::accent2);g.fillRect(meter.removeFromBottom(meter.getHeight()*level));}
void MixerStrip::resized(){int w=getWidth();name.setBounds(4,5,w-8,22);instrument.setBounds(6,30,w-12,22);pan.setBounds(8,55,w-16,20);mute.setBounds(6,77,(w-16)/2,20);solo.setBounds(10+(w-16)/2,77,(w-16)/2,20);clip.setBounds(8,48,w-16,24);gain.setBounds(4,99,w-8,getHeight()-106);
    if(getHeight()<170){pan.setVisible(false);instrument.setVisible(false);mute.setBounds(6,30,(w-16)/2,20);solo.setBounds(10+(w-16)/2,30,(w-16)/2,20);clip.setBounds(8,30,w-16,22);gain.setBounds(4,56,w-8,getHeight()-63);}
    else{pan.setVisible(track>=0);instrument.setVisible(track>=0);}}
void MixerStrip::mouseDown(const juce::MouseEvent& e){if(track>=0){owner.select(track,owner.selectedSection);if(e.mods.isPopupMenu())owner.showTrackMenu(track);}}
DawMixer::DawMixer(MainComponent& m):owner(m),master(m,-1){getProperties().set("fgaTip","Drag the mixer header to resize its height; double-click to restore the default.");port.setViewedComponent(&channels,false);port.setScrollBarsShown(false,true);addAndMakeVisible(port);addAndMakeVisible(master);addAndMakeVisible(collapseButton);collapseButton.setComponentID("mixer-collapse");
    tip(collapseButton,"Collapse the mixer to make room for tracks; click again to restore it. Shortcut: F3.");
    collapseButton.onClick=[this]{owner.mixerVisible=!owner.mixerVisible;owner.mixerToggle.setToggleState(owner.mixerVisible,juce::dontSendNotification);owner.resized();};
    for(int row=0;row<trackCount;++row){auto strip=std::make_unique<MixerStrip>(m,trackAtRow(row));channels.addAndMakeVisible(*strip);strips.push_back(std::move(strip));}}
void DawMixer::refresh(){for(auto& s:strips)s->refresh();master.refresh();}
void DawMixer::tick(){for(auto& s:strips)s->tick();master.tick();}
void DawMixer::reveal(int track){const int x=rowForTrack(track)*channelWidth;if(x<port.getViewPositionX())port.setViewPosition(x,0);else if(x+channelWidth-4>port.getViewPositionX()+port.getViewWidth())port.setViewPosition(x+channelWidth-4-port.getViewWidth(),0);}
void DawMixer::paint(juce::Graphics& g){g.fillAll(Palette::background);caption(g,"MIXER",{8,0,120,22});caption(g,"Audio levels / MIDI velocities unchanged",{130,0,getWidth()-300,22});if(owner.mixerVisible){g.setColour(Palette::dim.withAlpha(.4f));g.fillRoundedRectangle(getWidth()*.5f-18,7,36,2,1);g.fillRoundedRectangle(getWidth()*.5f-18,12,36,1,.5f);}}
void DawMixer::resized(){collapseButton.setBounds(getWidth()-156,1,152,22);collapseButton.setButtonText(owner.mixerVisible?"COLLAPSE MIXER":"SHOW MIXER");translateControls(collapseButton);port.setVisible(owner.mixerVisible);master.setVisible(owner.mixerVisible);if(!owner.mixerVisible)return;port.setBounds(0,25,getWidth()-110,getHeight()-25);const int height=std::max(100,port.getHeight()-16);channelWidth=std::clamp(port.getWidth()/trackCount,74,100);channels.setSize(trackCount*channelWidth,height);for(int row=0;row<trackCount;++row)strips[(size_t)row]->setBounds(row*channelWidth,0,channelWidth-4,height);master.setBounds(getWidth()-104,25,104,height);}
void DawMixer::mouseDown(const juce::MouseEvent& e){if(e.y<25&&owner.mixerVisible){resizingMixer=true;resizeOrigin=e.getEventRelativeTo(&owner).position.y;resizeHeight=getHeight();}}
void DawMixer::mouseDrag(const juce::MouseEvent& e){if(resizingMixer){owner.mixerHeight=std::clamp(resizeHeight+juce::roundToInt(resizeOrigin-e.getEventRelativeTo(&owner).position.y),160,std::max(160,owner.getHeight()-476));owner.resized();}}
void DawMixer::mouseUp(const juce::MouseEvent&){if(resizingMixer)Localisation::setSetting("mixerHeight",owner.mixerHeight);resizingMixer=false;}
void DawMixer::mouseDoubleClick(const juce::MouseEvent& e){if(e.y<25&&owner.mixerVisible){resizingMixer=false;owner.mixerHeight=244;owner.resized();Localisation::setSetting("mixerHeight",owner.mixerHeight);}}
void DawMixer::mouseMove(const juce::MouseEvent& e){setMouseCursor(e.y<25&&owner.mixerVisible?juce::MouseCursor::UpDownResizeCursor:juce::MouseCursor::NormalCursor);}

bool MainComponent::verifyDawWorkspace(const juce::File& folder){
    const auto saved=project;const int track=selectedTrack,section=selectedSection;const auto zoom=arrangement.pixelsPerBar;
    showArrangement();audio.stop();bool ok=inspectorPort.isVisible()&&mixer.isVisible()&&!arrangementPort.getBounds().intersects(inspectorPort.getBounds())&&!arrangementPort.getBounds().intersects(mixer.getBounds());
    juce::String firstFailure;auto stage=[&](const char* name){if(!ok&&firstFailure.isEmpty())firstFailure=name;};stage("initial dock layout");
    const int expandedHeight=mixer.getHeight(),trackAreaHeight=arrangementPort.getHeight();
    const auto unchangedMidi=midiFile(project);const double unchangedPosition=audio.position();
    if(auto* collapse=dynamic_cast<juce::Button*>(mixer.findChildWithID("mixer-collapse"))){
        collapse->onClick();ok=ok&&!mixerVisible&&mixer.isVisible()&&mixer.getHeight()==28&&arrangementPort.getHeight()>trackAreaHeight&&!mixerToggle.getToggleState();
        if(auto output=folder.getChildFile("mixer-collapsed.png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(createComponentSnapshot(getLocalBounds()),*output)&&ok;}else ok=false;
        const int trackAreaWidth=arrangementPort.getWidth();
        inspectorCollapse.onClick();ok=ok&&!inspectorVisible&&inspectorPort.isVisible()&&inspectorPort.getWidth()==32&&arrangementPort.getWidth()>trackAreaWidth&&!inspectorToggle.getToggleState()&&inspectorCollapse.isVisible();
        if(auto output=folder.getChildFile("workspace-collapsed.png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(createComponentSnapshot(getLocalBounds()),*output)&&ok;}else ok=false;
        inspectorCollapse.onClick();ok=ok&&inspectorVisible&&inspectorPort.getWidth()==inspectorWidth&&arrangementPort.getWidth()==trackAreaWidth&&inspectorToggle.getToggleState();
        collapse->onClick();ok=ok&&mixerVisible&&mixer.getHeight()==expandedHeight&&mixerToggle.getToggleState()&&midiFile(project)==unchangedMidi&&audio.position()==unchangedPosition;
    }else ok=false;
    stage("dock collapse and restore");
    const int originalWidth=inspectorWidth,originalMixerHeight=mixerHeight;
    auto resizeEvent=[](juce::Component* target,float x,float y){const juce::Point<float> p(x,y);return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,target,target,juce::Time::getCurrentTime(),p,juce::Time::getCurrentTime(),1,false);};
    inspectorResizer.mouseDown(resizeEvent(&inspectorResizer,2,20));inspectorResizer.mouseDrag(resizeEvent(&inspectorResizer,42,20));inspectorResizer.mouseUp(resizeEvent(&inspectorResizer,42,20));
    ok=ok&&inspectorWidth==originalWidth+40&&inspectorPort.getWidth()==inspectorWidth&&Localisation::setting("inspectorWidth","").getIntValue()==inspectorWidth;
    mixer.mouseDown(resizeEvent(&mixer,600,10));mixer.mouseDrag(resizeEvent(&mixer,600,-30));mixer.mouseUp(resizeEvent(&mixer,600,-30));
    ok=ok&&mixerHeight==originalMixerHeight+40&&mixer.getHeight()==mixerHeight&&Localisation::setting("mixerHeight","").getIntValue()==mixerHeight&&midiFile(project)==unchangedMidi&&audio.position()==unchangedPosition;
    stage("scaled drag resizing and saved dimensions");
    if(auto output=folder.getChildFile("workspace-resized.png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(createComponentSnapshot(getLocalBounds()),*output)&&ok;}else ok=false;
    inspectorResizer.mouseDoubleClick(resizeEvent(&inspectorResizer,2,20));mixer.mouseDoubleClick(resizeEvent(&mixer,600,10));ok=ok&&inspectorWidth==216&&mixerHeight==244;
    stage("default dock size reset");
    inspectorWidth=originalWidth;mixerHeight=originalMixerHeight;Localisation::setSetting("inspectorWidth",inspectorWidth);Localisation::setSetting("mixerHeight",mixerHeight);resized();
    const int originalHeight=getHeight();
    for(int height:{720,588}){setSize(1440,height);ok=ok&&!arrangementPort.getBounds().intersects(mixer.getBounds())&&mixer.getBottom()<=getHeight()-31;}
    setSize(1440,originalHeight);
    stage("small window dock layout");
    const auto midi=midiFile(project);const double originalGain=project.tracks[(size_t)selectedTrack].gain;
    if(auto* fader=dynamic_cast<juce::Slider*>(trackInspector.findChildWithID("track-gain"))){
        fader->setValue(-12,juce::sendNotificationSync);ok=ok&&std::abs(project.tracks[(size_t)selectedTrack].gain-juce::Decibels::decibelsToGain(-12.))<1e-6&&midiFile(project)==midi;
        undo(false);ok=ok&&std::abs(project.tracks[(size_t)selectedTrack].gain-originalGain)<1e-9;
    }else ok=false;
    stage("inspector fader and undo");
    project.sections={{Section::main,4,"First"},{Section::drop,4,"Second"}};project.normalise();for(auto& t:project.tracks)for(auto& c:t.clips)c.notes.clear();
    project.tracks[4].clips[0].notes={{0,.25,36,100,0}};project.tracks[4].clips[1].notes={{1,.25,36,75,0}};
    project.loop=false;project.loopStart=0;project.loopEnd=32;selectedTrack=4;selectedSection=0;arrangement.pixelsPerBar=32;changed();arrangementPort.setViewPosition(0,0);
    auto event=[&](float x,float y,int mods=0){const juce::Point<float> p(x,y);return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier|mods),1,0,0,0,0,&arrangement,&arrangement,juce::Time::getCurrentTime(),p,juce::Time::getCurrentTime(),1,false);};
    // Header selection must not enter the engine. Only its gear opens it.
    select(0,0);arrangementPort.setViewPosition(0,0);const auto headerMidi=midiFile(project);const auto headerPosition=audio.position();
    arrangement.mouseDown(event(78,76));arrangement.mouseUp(event(78,76));
    ok=ok&&page==Page::arrangement&&selectedTrack==4&&midiFile(project)==headerMidi&&audio.position()==headerPosition;stage("header selection without engine or playback changes");
    arrangement.mouseDown(event(180,76));arrangement.mouseUp(event(180,76));
    ok=ok&&page==Page::arrangement&&project.tracks[4].solo;stage("header Solo without opening the engine");undo(false);
    arrangement.mouseDown(event(202,76));arrangement.mouseUp(event(202,76));
    ok=ok&&page==Page::instrument&&selectedTrack==4;stage("explicit header engine gear");showArrangement();
    const auto beforeLead=project;const auto oldLead=project.settings.guitarLead;
    ok=ok&&leadButton.isVisible()&&!leadButton.getBounds().intersects(loopSelectionButton.getBounds());
    if(leadButton.onClick)leadButton.onClick();
    ok=ok&&page==Page::arrangement&&project.settings.guitarLead!=oldLead;
    undo(false);ok=ok&&project.settings.guitarLead==oldLead;
    for(int ti=0;ti<trackCount;++ti)ok=ok&&project.tracks[(size_t)ti].clips==beforeLead.tracks[(size_t)ti].clips;
    stage("header selection, explicit engine gear and main guitar lead toggle with undo");
    arrangement.mouseDown(event(232,76));arrangement.mouseUp(event(232,76));ok=ok&&page==Page::arrangement&&selectedTrack==4&&selectedSection==0&&std::abs(audio.position()-2)<1e-9&&!audio.isPlaying();
    arrangement.mouseDown(event(232,76));arrangement.mouseDrag(event(360,76));arrangement.mouseUp(event(360,76));ok=ok&&project.tracks[4].freeTimeline&&project.tracks[4].clips.size()==1&&project.clipStart(4,0)==16&&project.tracks[4].clips[0].notes[0].start==0;
    undo(false);ok=ok&&project.tracks[4].clips[0].notes[0].start==0&&project.tracks[4].clips[1].notes[0].start==1;
    arrangement.mouseDown(event(365,76));arrangement.mouseUp(event(365,76));
    ok=ok&&selectedSection==1&&std::abs(audio.position()-18.625)<1e-9&&!audio.isPlaying();
    audio.play(true);ok=ok&&std::abs(audio.position()-18.625)<1e-9;
    arrangement.mouseDown(event(291,116));arrangement.mouseUp(event(291,116));
    ok=ok&&selectedTrack==5&&audio.isPlaying()&&std::abs(audio.position()-9.375)<1e-9;audio.play(false);
    arrangement.pixelsPerBar=240;updateSizes();arrangementPort.setViewPosition(320,0);
    const int scroll=arrangementPort.getViewPositionX();
    arrangement.mouseDown(event((float)(scroll+253),76));arrangement.mouseUp(event((float)(scroll+253),76));
    ok=ok&&scroll>0&&std::abs(audio.position()-(scroll+37)*4./240)<1e-9;
    arrangement.pixelsPerBar=32;updateSizes();arrangementPort.setViewPosition(0,0);
    arrangement.mouseDown(event(283,46));arrangement.mouseUp(event(283,46));ok=ok&&std::abs(audio.position()-8.375)<1e-9;
    stage("precise clip seek and scrolling");
    arrangement.mouseDown(event(280,46));arrangement.mouseUp(event(280,46));ok=ok&&std::abs(audio.position()-8)<1e-9;
    arrangement.mouseDown(event(248,46,juce::ModifierKeys::shiftModifier));arrangement.mouseDrag(event(312,46,juce::ModifierKeys::shiftModifier));arrangement.mouseUp(event(312,46));ok=ok&&project.loop&&project.loopStart==4&&project.loopEnd==16&&dirty;
    arrangement.mouseDoubleClick(event(232,76));ok=ok&&page==Page::editor&&pianoPort.isVisible();
    stage("ruler, loop and full MIDI editor");
    showArrangement();project.loop=false;changed();arrangementPort.setViewPosition(0,0);
    const double beforeEdgeResize=audio.position();
    arrangement.mouseDown(event(342,76));arrangement.mouseDrag(event(278,76));arrangement.mouseUp(event(278,76));
    ok=ok&&project.clipLength(4,0)==8&&project.clipLength(5,0)==16&&project.sections[0].bars==4&&audio.position()==beforeEdgeResize;
    arrangement.mouseDown(event(278,76));arrangement.mouseDrag(event(406,76));arrangement.mouseUp(event(406,76));
    ok=ok&&project.clipLength(4,0)==24&&project.tracks[4].clips[0].notes.size()==2&&project.tracks[4].clips[0].notes[1].start==16;
    if(auto output=folder.getChildFile("clip-loop-resize.png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(createComponentSnapshot(getLocalBounds()),*output)&&ok;}else ok=false;
    Project loaded;ok=ok&&projectFromJson(projectJson(project),loaded).wasOk()&&loaded.tracks[4].clips[0]==project.tracks[4].clips[0]&&events(loaded,4).size()==events(project,4).size();
    undo(false);ok=ok&&project.clipLength(4,0)==8;undo(false);ok=ok&&project.clipLength(4,0)==16&&project.tracks[4].clips[0].notes.size()==1;
    arrangement.mouseDown(event(342,16));arrangement.mouseDrag(event(406,16));arrangement.mouseUp(event(406,16));
    ok=ok&&project.sections[0].bars==6&&project.sectionStart(1)==24&&project.tracks[4].clips[0].notes.size()==1;
    for(int ti=0;ti<trackCount;++ti)ok=ok&&project.clipLength(ti,0)==16&&project.clipStart(ti,1)==16;
    if(auto output=folder.getChildFile("section-loop-resize.png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(createComponentSnapshot(getLocalBounds()),*output)&&ok;}else ok=false;
    const auto beforeCancel=midiFile(project);const auto historySize=undoStack.size();arrangement.mouseDown(event(406,16));arrangement.mouseDrag(event(470,16));arrangement.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));arrangement.mouseUp(event(470,16));
    ok=ok&&project.sections[0].bars==6&&midiFile(project)==beforeCancel&&undoStack.size()==historySize;
    audio.seek(2);arrangement.mouseDown(event(406,76));arrangement.mouseUp(event(406,76));ok=ok&&std::abs(audio.position()-23.75)<1e-9;
    stage("individual loops and independent guide resizing, persistence, Undo and Escape");

    const auto savedHeights=arrangement.trackHeights;const auto savedLanes=arrangement.volumeVisible;
    arrangement.trackHeights.fill(40);arrangement.volumeVisible.fill(false);updateSizes();arrangementPort.setViewPosition(0,0);
    const auto beforeHeightMidi=midiFile(project);const double beforeHeightPosition=audio.position();const auto beforeHeightUndo=undoStack.size();
    arrangement.mouseDown(event(100,100));arrangement.mouseDrag(event(100,180));arrangement.mouseUp(event(100,180));
    ok=ok&&arrangement.rowHeight(0)==120&&arrangement.rowHeight(1)==40&&arrangement.rowY(1)==180&&arrangement.rowAt(179)==0&&arrangement.rowAt(180)==1&&Localisation::setting("trackHeight4").getIntValue()==120;
    arrangement.volumeVisible[4]=true;updateSizes();ok=ok&&arrangement.rowY(1)==260&&arrangement.volumeY(4,1)==220;
    arrangement.mouseDown(event(100,180));arrangement.mouseDrag(event(100,220));arrangement.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));arrangement.mouseUp(event(100,220));
    ok=ok&&arrangement.rowHeight(0)==120&&arrangement.rowY(1)==260;
    if(auto output=folder.getChildFile("track-height-resize.png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(createComponentSnapshot(getLocalBounds()),*output)&&ok;}else ok=false;
    arrangement.mouseDoubleClick(event(100,180));ok=ok&&arrangement.rowHeight(0)==40&&Localisation::setting("trackHeight4").getIntValue()==40;
    arrangement.volumeVisible.fill(false);updateSizes();arrangementPort.setViewPosition(0,320);
    const int bassY=arrangement.rowY(13)+40;
    arrangement.mouseDown(event(100,(float)bassY));arrangement.mouseDrag(event(100,(float)bassY+500));arrangement.mouseUp(event(100,(float)bassY+500));
    ok=ok&&arrangement.rowHeight(13)==240&&arrangement.rowHeight(14)==40&&arrangement.rowHeight(0)==40;
    const int bassBottom=arrangement.rowY(13)+240;
    arrangement.mouseDown(event(100,(float)bassBottom));arrangement.mouseDrag(event(100,(float)bassBottom-500));arrangement.mouseUp(event(100,(float)bassBottom-500));
    ok=ok&&arrangement.rowHeight(13)==40&&midiFile(project)==beforeHeightMidi&&audio.position()==beforeHeightPosition&&undoStack.size()==beforeHeightUndo;
    stage("individual track height drag, automation alignment, saved size, Escape, reset and scrolled lower track");
    arrangement.trackHeights=savedHeights;arrangement.volumeVisible=savedLanes;arrangement.saveTrackHeights();updateSizes();
    project=saved;arrangement.pixelsPerBar=zoom;select(track,section);changed();showArrangement();undoStack.clear();redoStack.clear();refreshControls();
    folder.getChildFile("daw-workspace.txt").replaceWithText(ok?"PASS inspector/mixer layout, mixer and inspector collapse/restore, drag resize with saved dimensions, MIDI/transport unchanged, audio-only fader with undo, clip select/move/undo, individual loop and independent guide edge resizing with source recovery/Undo/Escape/project round-trip, precise clip/ruler seek, paused start, playing jump, scrolled seek, loop drag persisted, full MIDI editor":"FAIL DAW workspace regression: "+firstFailure);return ok;
}

}
