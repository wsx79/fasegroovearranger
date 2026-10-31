// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "DawWorkspace.h"
#include "MainComponent.h"
#include "UiText.h"
namespace fga {
namespace {
juce::String text(const std::string& s){return juce::String::fromUTF8(s.c_str());}
juce::Colour ink(int track){return Daw::trackColour(track);}
float db(double gain){return gain<=.001?-60.f:juce::Decibels::gainToDecibels((float)gain,-60.f);}
double amplitude(double value){return value<=-59.99?0.:juce::Decibels::decibelsToGain(value);}
void caption(juce::Graphics& g,const juce::String& s,juce::Rectangle<int> bounds){Daw::drawCaps(g,tr(s),bounds,Daw::textFaint);}
void slider(juce::Slider& s,double low,double high,const char* suffix){s.setRange(low,high,.1);s.setSliderStyle(juce::Slider::LinearHorizontal);s.setTextBoxStyle(juce::Slider::TextBoxRight,false,60,20);s.setTextValueSuffix(suffix);s.setDoubleClickReturnValue(true,0);}
void tip(juce::Component& c,const char* s){c.getProperties().set("fgaTip",s);}
}
// Readback only: the UI never feeds an automation sample back into the project.
void MainComponent::updateGainDisplay(juce::Slider& control,double base,const VolumeAutomation& lane,bool gesture){
    if(gesture||(bool)control.getProperties().getWithDefault("fgaGesture",false))return;
    for(auto* child:control.getChildren())if(auto* label=dynamic_cast<juce::Label*>(child);label&&label->isBeingEdited())return;
    const bool following=audio.isPlaying()&&lane.active();
    const double baseDb=db(base),value=db(following?base*lane.valueAt(audio.position()):base);
    // The base control stops at +6 dB, but a 200% envelope can add another 6 dB.
    // Expand only the readback range; manual edits retain the original base range.
    const double upper=std::max(6.,std::ceil(value*10.)/10.);
    if(control.getMaximum()!=upper)control.setRange(-60,upper,.1);
    const double shown=std::clamp(std::round(value*10.)/10.,-60.,upper);
    control.getProperties().set("automationOffsetDb",following?shown-baseDb:0.);
    control.setValue(shown,juce::dontSendNotification);
}
double MainComponent::gainFromControl(const juce::Slider& control)const{
    // A mouse/key/wheel/text edit trims the base by the change from the displayed
    // value. Its offset stays fixed for a drag, even while the playhead advances.
    return amplitude(std::clamp(control.getValue()-(double)control.getProperties().getWithDefault("automationOffsetDb",0.),-60.,6.));
}
// Absolute parameter envelopes: trim the stored value by the user's movement,
// and never write the evaluated envelope into the project from a timer.
void MainComponent::updatePanDisplay(juce::Slider& control,double base,const VolumeAutomation& lane,bool gesture){
    if(gesture||inGesture(control))return;
    for(auto* child:control.getChildren())if(auto* label=dynamic_cast<juce::Label*>(child);label&&label->isBeingEdited())return;
    const double shown=audio.isPlaying()&&lane.active()?panFromAutomation(lane.valueAt(audio.position()))*100.:base*100.;
    control.setValue(shown,juce::dontSendNotification);
    control.getProperties().set("automationOffset",control.getValue()-base*100.);
}
void MainComponent::updateParameterDisplay(juce::Slider& control,double base,int index,const Track& track,bool cutoff){
    if(inGesture(control))return;
    for(auto* child:control.getChildren())if(auto* label=dynamic_cast<juce::Label*>(child);label&&label->isBeingEdited())return;
    double shown=base;
    if(audio.isPlaying())for(const auto& lane:track.parameters)if(lane.slot==-1&&lane.index==index&&lane.curve.active()){
        const auto value=std::clamp(lane.curve.valueAt(audio.position()),0.,1.);shown=cutoff?20.*std::pow(1000.,value):value*(control.getMaximum()>1?100.:1.);
    }
    control.setValue(shown,juce::dontSendNotification);
    control.getProperties().set("automationOffset",control.getValue()-base);
}
double MainComponent::valueFromControl(const juce::Slider& control)const{
    return std::clamp(control.getValue()-(double)control.getProperties().getWithDefault("automationOffset",0.),control.getMinimum(),control.getMaximum());
}
InspectorResizer::InspectorResizer(MainComponent& m,bool forBrowser):owner(m),browser(forBrowser){setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);tip(*this,forBrowser?"Drag the sample browser's left edge to resize its width; double-click to restore the default.":"Drag the inspector's right edge to resize its width; double-click to restore the default.");}
void InspectorResizer::paint(juce::Graphics& g){const int middle=getHeight()/2;g.setColour((isMouseOverOrDragging()?Daw::accent:Daw::textFaint).withAlpha(isMouseOverOrDragging()?.9f:.5f));for(int dot=-2;dot<=2;++dot)g.fillEllipse(getWidth()*.5f-1.5f,(float)middle+dot*7-1.5f,3.f,3.f);}
void InspectorResizer::mouseDown(const juce::MouseEvent& e){dragOrigin=e.getEventRelativeTo(&owner).position.x;initialWidth=browser?owner.sampleBrowser.getWidth():owner.inspectorWidth;}
void InspectorResizer::mouseDrag(const juce::MouseEvent& e){
    // The browser sits on the right: dragging its left edge to the left makes it wider.
    if(browser){owner.sampleBrowserWidth=owner.clampBrowserWidth(initialWidth-juce::roundToInt(e.getEventRelativeTo(&owner).position.x-dragOrigin));owner.resized();return;}
    owner.inspectorWidth=std::clamp(initialWidth+juce::roundToInt(e.getEventRelativeTo(&owner).position.x-dragOrigin),224,std::max(224,std::min(480,owner.getWidth()/3)));owner.resized();}
void InspectorResizer::mouseUp(const juce::MouseEvent&){if(browser)Localisation::setSetting("sampleBrowserWidth",owner.sampleBrowserWidth);else Localisation::setSetting("inspectorWidth",owner.inspectorWidth);}
void InspectorResizer::mouseDoubleClick(const juce::MouseEvent&){if(browser){owner.sampleBrowserWidth=0;owner.resized();Localisation::setSetting("sampleBrowserWidth",0);return;}owner.inspectorWidth=248;owner.resized();Localisation::setSetting("inspectorWidth",owner.inspectorWidth);}

TrackInspector::TrackInspector(MainComponent& m):owner(m),effects(m){
    addAndMakeVisible(effects);effects.setComponentID("effect-chain");
    for(auto* c:std::initializer_list<juce::Component*>{&name,&source,&section,&gain,&pan,&instrument,&editor,&mute,&solo,&lockTrack,&lockClip,&internal,&sample,&plugin,&pluginEditor,&samplerEditor,&deleteTrack,&regenerate,&loop})addAndMakeVisible(c);
    name.setComponentID("track-name");name.setFont(Daw::font(Daw::display,true));name.setMinimumHorizontalScale(.55f);name.setEditable(false,true,false);name.onTextChange=[this]{if(updating||!validTrack())return;auto value=name.getText().trim().substring(0,128);if(value.isEmpty()){refresh();return;}auto& t=owner.project.tracks[(size_t)owner.selectedTrack];if(t.name!=value.toStdString()){owner.checkpoint();t.name=value.toStdString();owner.changed(false);}};tip(name,"Double-click to rename this track.");
    source.setFont(Daw::font(Daw::small));source.setColour(juce::Label::textColourId,Daw::textDim);source.setMinimumHorizontalScale(.5f);source.setInterceptsMouseClicks(false,false);source.setComponentID("track-source");
    section.setFont(Daw::font(Daw::body));section.setColour(juce::Label::textColourId,Daw::textDim);section.setMinimumHorizontalScale(.55f);section.setInterceptsMouseClicks(false,false);
    gain.setComponentID("track-gain");pan.setComponentID("track-pan");sample.setComponentID("inspector-sample");loop.setComponentID("inspector-loop");
    slider(gain,-60,6.0," dB");slider(pan,-100,100,"%");
    for(auto* knob:{&gain,&pan}){knob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);knob->setTextBoxStyle(juce::Slider::TextBoxBelow,false,78,19);knob->setMouseDragSensitivity(170);}
    instrument.setIcon(IconButton::Icon::gear);editor.setIcon(IconButton::Icon::pencil);regenerate.setIcon(IconButton::Icon::dice);loop.setIcon(IconButton::Icon::loop);
    lockTrack.setIcon(IconButton::Icon::lock);lockClip.setIcon(IconButton::Icon::lock);pluginEditor.setIcon(IconButton::Icon::piano);samplerEditor.setIcon(IconButton::Icon::studio);deleteTrack.setIcon(IconButton::Icon::close);
    for(auto* b:{&mute,&solo,&lockTrack,&lockClip,&internal,&sample,&plugin,&loop})b->setClickingTogglesState(false);
    mute.setColour(juce::TextButton::buttonOnColourId,Daw::mute);solo.setColour(juce::TextButton::buttonOnColourId,Daw::solo);
    // Actions read the selection when clicked; nothing is cached between refreshes.
    gain.onDragStart=[this]{gainGesture=true;owner.checkpointMix();};gain.onDragEnd=[this]{gainGesture=false;refresh();};
    gain.onValueChange=[this]{if(updating||!validTrack())return;if(!gainGesture)owner.checkpointMix(&gain);owner.project.tracks[(size_t)owner.selectedTrack].gain=owner.gainFromControl(gain);owner.mixChanged();};
    pan.onDragStart=[this]{panGesture=true;owner.checkpointMix();};pan.onDragEnd=[this]{panGesture=false;refresh();};
    pan.onValueChange=[this]{if(updating||!validTrack())return;if(!panGesture)owner.checkpointMix(&pan);owner.project.tracks[(size_t)owner.selectedTrack].pan=owner.valueFromControl(pan)/100.;owner.mixChanged();};
    deleteTrack.setComponentID("delete-midi-track");deleteTrack.onClick=[this]{owner.deleteMidiTrack(owner.selectedTrack);};tip(deleteTrack,"Delete this handwritten MIDI track. Undo restores its notes and sound.");
    instrument.onClick=[this]{owner.showInstrument();};editor.onClick=[this]{owner.showEditor();};
    mute.onClick=[this]{if(!validTrack())return;owner.checkpoint();auto& t=owner.project.tracks[(size_t)owner.selectedTrack];t.mute=!t.mute;owner.mixChanged();};
    solo.onClick=[this]{if(!validTrack())return;owner.checkpoint();auto& t=owner.project.tracks[(size_t)owner.selectedTrack];t.solo=!t.solo;owner.mixChanged();};
    lockTrack.onClick=[this]{if(validTrack())owner.toggleTrackLock();};lockClip.onClick=[this]{if(validTrack())owner.toggleClipLock();};
    internal.onClick=[this]{owner.useBuiltInSound(owner.selectedTrack);};sample.onClick=[this]{owner.browseSamplesFor(owner.selectedTrack);};
    plugin.onClick=[this]{owner.showPluginMenu(&plugin);};pluginEditor.setComponentID("inspector-plugin-editor");pluginEditor.onClick=[this]{owner.openHostedEditor(owner.selectedTrack,0);};
    samplerEditor.setComponentID("inspector-sampler");samplerEditor.onClick=[this]{owner.showStudio(0);};Localisation::bindText(samplerEditor,"OPEN SAMPLER");tip(samplerEditor,"Open the waveform, sample settings and playable slices for this track.");
    regenerate.onClick=[this]{owner.regenerate(Scope::clip);};loop.onClick=[this]{owner.toggleClipLoop();};
    tip(gain,"Track volume in dB. This changes audio, not MIDI velocities. Double-click for 0 dB.");tip(pan,"Stereo pan. Double-click to centre.");tip(mute,"Mute this track.");tip(solo,"Listen to soloed tracks.");tip(editor,"Open the selected clip in the full MIDI editor. Shortcut: E.");tip(instrument,"Open the original Fase Groove instrument controls.");
    tip(sample,"Open the integrated sample browser for this track: double-click a sample or drag it onto the track.");tip(loop,"Loop the selected clip; click again to remove the loop.");
    internal.setButtonText(tr("Built-in"));sample.setButtonText(tr("Sample"));plugin.setButtonText("VST / AU");
    editor.setButtonText(tr("EDIT MIDI"));regenerate.setButtonText(tr("REGENERATE"));loop.setButtonText(tr("LOOP"));pluginEditor.setButtonText(tr("OPEN VST"));
    for(auto* b:{&editor,&regenerate,&loop,&pluginEditor,&instrument}){b->getProperties().set("fgaIconOnly",true);b->setTitle(b->getButtonText());}
    tip(plugin,"Choose an instrument or effect from the plugin browser.");tip(regenerate,"Regenerate the selected clip, keeping track and clip locks.");
    // No refresh here: the owner is still being constructed; select() fills the inspector.
    contentHeight=layout(232);setSize(232,contentHeight);
}
bool TrackInspector::validTrack()const{return owner.selectedTrack>=0&&owner.selectedTrack<(int)owner.project.tracks.size();}
void TrackInspector::refresh(){
    if(!validTrack())return;
    updating=true;regenerate.setEnabled(!isManualTrack(owner.selectedTrack));const int ti=owner.selectedTrack;const auto& t=owner.project.tracks[(size_t)ti];
    if(!name.isBeingEdited())name.setText(text(t.name),juce::dontSendNotification);name.setColour(juce::Label::textColourId,Daw::text);
    const auto sound=owner.sourceWithEffect(ti);
    source.setText(sound,juce::dontSendNotification);source.setTooltip(sound);
    const int clipIndex=owner.clipIndex();const bool hasClip=clipIndex>=0&&clipIndex<(int)t.clips.size();
    if(hasClip){const double bars=owner.clipLength()/4.;const int guide=std::clamp(owner.project.sectionAtBeat(owner.clipStart()),0,(int)owner.project.sections.size()-1);section.setText(text(owner.project.sections[(size_t)guide].name)+" / "+juce::String(bars,bars==(int)bars?0:2)+" "+tr("bars"),juce::dontSendNotification);}
    else section.setText(juce::String::fromUTF8("—"),juce::dontSendNotification);
    // A fader the user is dragging keeps its own value; the model follows it.
    owner.updateGainDisplay(gain,t.gain,t.volume,gainGesture);
    owner.updatePanDisplay(pan,t.pan,t.panAutomation,panGesture);
    // An automated volume or pan reads in blue.
    for(auto [slider,lane]:{std::pair<juce::Slider*,const VolumeAutomation*>{&gain,&t.volume},{&pan,&t.panAutomation}}){if(lane->moves()){slider->setColour(juce::Slider::textBoxTextColourId,Daw::automation);slider->setColour(juce::Slider::thumbColourId,Daw::automation);slider->setColour(juce::Slider::rotarySliderFillColourId,Daw::automation);}else{slider->removeColour(juce::Slider::textBoxTextColourId);slider->removeColour(juce::Slider::thumbColourId);slider->removeColour(juce::Slider::rotarySliderFillColourId);}}
    const bool busy=owner.loadingPlugin>0;deleteTrack.setEnabled(!busy);const bool hasPlugin=owner.audio.plugin(ti)!=nullptr;
    mute.setToggleState(t.mute,juce::dontSendNotification);solo.setToggleState(t.solo,juce::dontSendNotification);
    lockTrack.setToggleState(t.locked,juce::dontSendNotification);lockClip.setToggleState(hasClip&&t.clips[(size_t)clipIndex].locked,juce::dontSendNotification);lockClip.setEnabled(hasClip);
    internal.setToggleState(t.instrument=="internal",juce::dontSendNotification);sample.setToggleState(t.instrument=="sample",juce::dontSendNotification);plugin.setToggleState(t.instrument=="plugin",juce::dontSendNotification);
    internal.setEnabled(!busy);sample.setEnabled(!busy);plugin.setEnabled(!busy);pluginEditor.setEnabled(hasPlugin&&!busy);if(auto* vst=owner.audio.plugin(ti)){const auto action=tr("Open %plugin%").replace("%plugin%",vst->getName());pluginEditor.setTooltip(action);pluginEditor.setTitle(action);}samplerEditor.setEnabled(t.instrument=="sample"&&!busy);
    regenerate.setEnabled(!isManualTrack(ti)&&hasClip&&!t.locked&&!t.clips[(size_t)clipIndex].locked&&!busy);loop.setEnabled(hasClip);loop.setToggleState(owner.clipLoopActive(),juce::dontSendNotification);
    // Only these texts change here; the rest is translated on creation and language change.
    effects.refresh();updating=false;for(auto* c:std::initializer_list<juce::Component*>{&name,&source,&section})translateControls(*c);resized();repaint();
}
void TrackInspector::paint(juce::Graphics& g){
    g.fillAll(Daw::base);if(getWidth()<=40)return;
    for(const auto& bounds:cards){
        g.setColour(Daw::surface);g.fillRoundedRectangle(bounds.toFloat(),Daw::panelRadius);
        g.setColour(Daw::line);g.drawRoundedRectangle(bounds.toFloat().reduced(.5f),Daw::panelRadius,1.f);
    }
    if(validTrack()&&!cards.empty()){
        const auto colour=ink(owner.selectedTrack);
        auto badge=juce::Rectangle<float>(20.f,(float)name.getY()-1.f,28.f,24.f);
        g.setColour(colour.withAlpha(.16f));g.fillRoundedRectangle(badge,4.f);
        g.setColour(colour);g.setFont(Daw::font(Daw::body,true));
        g.drawText(juce::String(rowForTrack(owner.selectedTrack)+1).paddedLeft('0',2),badge,juce::Justification::centred);
        g.setColour(colour);g.fillRoundedRectangle(cards.front().toFloat().withHeight(2.f).reduced(10.f,0),1.f);
    }
    for(const auto& title:captions)Daw::drawCaps(g,tr(title.first),title.second,Daw::textDim,title.first=="Volume"||title.first=="Pan"?juce::Justification::centred:juce::Justification::centredLeft);
}
// Four compact cards share the workstation's type, spacing and colours. Actions
// retain their component IDs and always operate on the current selection.
int TrackInspector::layout(int width){
    cards.clear();captions.clear();const int margin=8,inset=20,w=std::max(120,width-inset*2),gap=6;
    const int half=(w-gap)/2;int y=8,cardStart=y;
    auto title=[&](const char* text){captions.push_back({text,{inset,y+6,w,14}});y+=22;};
    auto row=[&](juce::Component& c,int height=24){c.setBounds(inset,y,w,height);y+=height+4;};
    auto endCard=[&]{y+=6;cards.push_back({margin,cardStart,width-margin*2,y-cardStart});y+=6;cardStart=y;};
    const int close=24;captions.push_back({"TRACK INSPECTOR",{inset,y+(close-14)/2,std::max(40,w-close-8),14}});owner.inspectorCollapse.setBounds(width-margin-8-close,y,close,close);y+=close+6;
    name.setBounds(inset+36,y,w-36,22);source.setBounds(inset,y+22,w,16);y+=40;
    {auto tools=juce::Rectangle<int>(inset,y,w,Daw::controlHeight);mute.setBounds(tools.removeFromLeft(8*Daw::unit));tools.removeFromLeft(Daw::unit);solo.setBounds(tools.removeFromLeft(8*Daw::unit));tools.removeFromLeft(Daw::unit);lockTrack.setBounds(tools);}y+=7*Daw::unit;const bool manual=validTrack()&&isManualTrack(owner.selectedTrack);deleteTrack.setVisible(manual);if(manual)row(deleteTrack);else deleteTrack.setBounds({});endCard();
    title("SOUND ASSIGNMENT");
    // Width follows the translated label, so Italian Suono guida is not crushed to "SUONO G…".
    {juce::Button* buttons[]{&internal,&sample,&plugin};const int n=3,avail=w-gap*(n-1);int sum=0;
        auto need=[&](const juce::Button& b){return std::max(40,(int)std::ceil(juce::GlyphArrangement::getStringWidth(Daw::font(Daw::small,true),b.getButtonText()))+16);};
        for(auto* b:buttons)sum+=need(*b);int x=inset,used=0;
        for(int i=0;i<n;++i){const int room=std::max(40,avail-used-40*(n-1-i));const int width=i==n-1?std::max(1,avail-used):std::clamp(need(*buttons[i])*avail/std::max(1,sum),40,room);buttons[i]->setBounds(x,y,width,24);x+=width+gap;used+=width;}
        y+=28;}
    row(instrument);
    const bool external=validTrack()&&owner.audio.plugin(owner.selectedTrack)!=nullptr;
    pluginEditor.setVisible(external);if(external)row(pluginEditor);else pluginEditor.setBounds({});
    const bool sampled=validTrack()&&owner.project.tracks[(size_t)owner.selectedTrack].instrument=="sample";samplerEditor.setVisible(sampled);if(sampled)row(samplerEditor);else samplerEditor.setBounds({});
    endCard();
    row(effects,effects.preferredHeight());endCard();
    title("VOLUME / PAN");
    gain.setBounds(inset,y,half,76);pan.setBounds(inset+half+gap,y,w-half-gap,76);
    captions.push_back({"Volume",{inset,y+79,half,14}});captions.push_back({"Pan",{inset+half+gap,y+79,w-half-gap,14}});y+=98;endCard();
    title("SELECTED CLIP");
    row(section,18);
    auto iconPair=[&](juce::Button& a,juce::Button& b){auto line=juce::Rectangle<int>(inset,y,w,Daw::controlHeight);const int aw=(bool)a.getProperties().getWithDefault("fgaIconOnly",false)?Daw::controlHeight:half;a.setBounds(line.removeFromLeft(aw));line.removeFromLeft(gap);const int bw=(bool)b.getProperties().getWithDefault("fgaIconOnly",false)?Daw::controlHeight:line.getWidth();b.setBounds(line.removeFromLeft(bw));y+=7*Daw::unit;};
    iconPair(editor,regenerate);iconPair(loop,lockClip);endCard();
    return y;
}
void TrackInspector::tick(){
    if(!validTrack()||!isShowing())return;
    const auto& track=owner.project.tracks[(size_t)owner.selectedTrack];
    owner.updateGainDisplay(gain,track.gain,track.volume,gainGesture);
    owner.updatePanDisplay(pan,track.pan,track.panAutomation,panGesture);
}
void TrackInspector::resized(){
    const bool collapsed=getWidth()<=40;for(auto* child:getChildren())child->setVisible(!collapsed);if(collapsed)return;
    contentHeight=layout(getWidth());if(getHeight()!=contentHeight)setSize(getWidth(),contentHeight);
}

MixerStrip::MixerStrip(MainComponent& m,int ti):owner(m),track(ti){
    for(auto* c:std::initializer_list<juce::Component*>{&name,&gain,&pan,&mute,&solo,&instrument,&clip,&sendA,&sendB,&output,&effects})addAndMakeVisible(c);
    gain.setComponentID("mixer-gain");pan.setComponentID("mixer-pan");effects.setComponentID("mixer-effects");effects.getProperties().set("fgaRawText",true);effects.onClick=[this]{if(track>=0)owner.showEffectChain(track);};tip(effects,"EFFECTS");
    name.getProperties().set("fgaRawText",true);
    setComponentID("mixer-track-"+juce::String(track));sendA.setComponentID("mixer-send-a");sendB.setComponentID("mixer-send-b");output.setComponentID("mixer-output");
    for(int i=0;i<2;++i){auto& button=i==0?sendA:sendB;button.setColour(juce::TextButton::buttonOnColourId,Daw::accent);button.onClick=[this,i]{if(track<0||track>=(int)owner.project.tracks.size())return;owner.checkpoint();auto& value=owner.project.tracks[(size_t)track].sends[(size_t)i];if(value>0){sendMemory[(size_t)i]=value;value=0;}else value=sendMemory[(size_t)i];owner.mixChanged();};tip(button,i==0?"Toggle send A to shared reverb. Adjust its amount in Bus & Send.":"Toggle send B to shared delay. Adjust its amount in Bus & Send.");}
    tip(output,"Output: M is master, GA is Group A, GB is Group B. Click to change routing.");output.onClick=[this]{if(track<0)return;juce::PopupMenu menu;const int group=owner.project.tracks[(size_t)track].group;menu.addItem(1,tr("Direct to master"),true,group<0);menu.addItem(2,tr("GROUP A"),true,group==0);menu.addItem(3,tr("GROUP B"),true,group==1);juce::Component::SafePointer<MainComponent> safe(&owner);const int ti=track;menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&output),[safe,ti](int id){if(safe&&id>0&&ti<(int)safe->project.tracks.size()){safe->checkpoint();safe->project.tracks[(size_t)ti].group=id-2;safe->mixChanged();}});};
    name.setEditable(false,track>=0,false);name.onEditorShow=[this]{if(track>=0&&track<(int)owner.project.tracks.size())if(auto* editor=name.getCurrentTextEditor())editor->setText(text(owner.project.tracks[(size_t)track].name),false);};name.onTextChange=[this]{if(updating||track<0||track>=(int)owner.project.tracks.size())return;auto value=name.getText().trim().substring(0,128);if(value.isEmpty()){refresh();return;}auto& t=owner.project.tracks[(size_t)track];if(t.name!=value.toStdString()){owner.checkpoint();t.name=value.toStdString();owner.changed(false);}};tip(name,"Double-click to rename this track.");name.setFont(Daw::font(Daw::small,true));name.setJustificationType(juce::Justification::centred);name.setMinimumHorizontalScale(.5f);
    // A mixer fader: -60 to +6 dB with 0 dB at three quarters of its travel, its value shown
    // below it; the pan is a knob.
    slider(gain,-60,6," dB");gain.setSliderStyle(juce::Slider::LinearVertical);gain.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);gain.setSkewFactor(2.2);
    slider(pan,-100,100,"%");pan.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);pan.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);pan.setMouseDragSensitivity(160);
    instrument.setColour(juce::TextButton::buttonColourId,Daw::surfaceHi);mute.setColour(juce::TextButton::buttonOnColourId,Daw::mute);solo.setColour(juce::TextButton::buttonOnColourId,Daw::solo);clip.setColour(juce::TextButton::buttonOnColourId,Daw::meterHigh);
    gain.onDragStart=[this]{gainGesture=true;owner.checkpointMix();};gain.onDragEnd=[this]{gainGesture=false;};
    gain.onValueChange=[this]{if(updating)return;if(!gainGesture)owner.checkpointMix(&gain);if(track<0)owner.project.masterGain=owner.gainFromControl(gain);else owner.project.tracks[(size_t)track].gain=owner.gainFromControl(gain);owner.mixChanged();};
    pan.onDragStart=[this]{panGesture=true;owner.checkpointMix();};pan.onDragEnd=[this]{panGesture=false;};
    pan.onValueChange=[this]{if(updating||track<0)return;if(!panGesture)owner.checkpointMix(&pan);owner.project.tracks[(size_t)track].pan=owner.valueFromControl(pan)/100.;owner.mixChanged();};
    mute.onClick=[this]{owner.checkpoint();auto& t=owner.project.tracks[(size_t)track];t.mute=!t.mute;owner.mixChanged();};solo.onClick=[this]{owner.checkpoint();auto& t=owner.project.tracks[(size_t)track];t.solo=!t.solo;owner.mixChanged();};
    instrument.onClick=[this]{if(track>=0){owner.select(track,owner.selectedSection);owner.showInstrument();}};clip.onClick=[this]{owner.audio.clearClipping();repaint();};
    tip(gain,"Track volume in dB. This changes audio, not MIDI velocities. Double-click for 0 dB.");tip(pan,"Stereo pan. Double-click to centre.");tip(mute,"Mute this track.");tip(solo,"Listen to soloed tracks.");tip(clip,"Master peak overload. Click to clear the indicator.");tip(instrument,"Open the original Fase Groove instrument controls.");refresh();
}
void MixerStrip::refresh(){if(track>=0&&owner.project.tracks.empty())return;effects.setVisible(track>=0);if(track>=0){const auto& chain=owner.project.tracks[(size_t)track].effects;int active=0;for(const auto& fx:chain)if(!fx.bypassed)++active;effects.setButtonText("FX"+(active>0?" "+juce::String(active):juce::String()));effects.setToggleState(active>0,juce::dontSendNotification);effects.setEnabled(!owner.loadingPlugin);}updating=true;const bool master=track<0;const auto full=master?tr("MASTER"):text(owner.project.tracks[(size_t)track].name);name.setText(master?full:Localisation::upper(full),juce::dontSendNotification);name.setTooltip(full);name.setColour(juce::Label::textColourId,Daw::text);owner.updateGainDisplay(gain,master?owner.project.masterGain:owner.project.tracks[(size_t)track].gain,master?owner.project.masterAutomation:owner.project.tracks[(size_t)track].volume,gainGesture);pan.setVisible(!master&&getHeight()>=180);mute.setVisible(!master);solo.setVisible(!master);instrument.setVisible(!master&&getHeight()>=120);clip.setVisible(master);sendA.setVisible(!master);sendB.setVisible(!master);output.setVisible(!master);if(!master){auto& t=owner.project.tracks[(size_t)track];output.setButtonText(t.group==0?"GA":t.group==1?"GB":"M");for(int i=0;i<2;++i){auto& b=i==0?sendA:sendB;const auto value=t.sends[(size_t)i];if(value>0)sendMemory[(size_t)i]=value;b.setToggleState(value>0,juce::dontSendNotification);}owner.updatePanDisplay(pan,t.pan,t.panAutomation,panGesture);mute.setToggleState(t.mute,juce::dontSendNotification);solo.setToggleState(t.solo,juce::dontSendNotification);instrument.setButtonText(t.instrument=="internal"?"BUILT-IN":t.instrument=="sample"?"SAMPLE":"VST3 / AU");}updating=false;translateControls(*this);repaint();}
void MixerStrip::tick(){if(track>=0&&track<(int)owner.project.tracks.size()){const auto& t=owner.project.tracks[(size_t)track];owner.updatePanDisplay(pan,t.pan,t.panAutomation,panGesture);}

    const double previousValue=gain.getValue(),previousMaximum=gain.getMaximum();
    if(track<0||track<(int)owner.project.tracks.size())owner.updateGainDisplay(gain,track<0?owner.project.masterGain:owner.project.tracks[(size_t)track].gain,track<0?owner.project.masterAutomation:owner.project.tracks[(size_t)track].volume,gainGesture);
    if(gain.getMaximum()!=previousMaximum)repaint();
    else if(gain.getValue()!=previousValue)repaint(4,gain.getBottom()+2,getWidth()-8,16);
    peak=std::max(track<0?owner.audio.masterLevel():owner.audio.level(track),peak*.82f);clip.setToggleState(owner.audio.hasClipped(),juce::dontSendNotification);
    // Each side falls smoothly; its peak holds for 1.5 s, then falls too.
    for(int side=0;side<2;++side){const float now=track<0?owner.audio.masterLevel(side):owner.audio.level(track,side);sides[(size_t)side]=std::max(now,sides[(size_t)side]*.82f);
        if(sides[(size_t)side]>=holds[(size_t)side]){holds[(size_t)side]=sides[(size_t)side];holdTicks[(size_t)side]=30;}else if(--holdTicks[(size_t)side]<=0)holds[(size_t)side]=std::max(sides[(size_t)side],holds[(size_t)side]*.9f);}
    // Repaint only when the drawn meter moves: an idle arranger must not redraw 18 strips 20 times a second.
    const float level=Daw::meterPosition(peak);const int colour=peak>=1?2:peak>.5f?1:0;bool moved=std::abs(level-drawnLevel)>.002f||colour!=drawnColour;
    for(size_t side=0;side<2;++side){const float shown=Daw::meterPosition(sides[side]),held=Daw::meterPosition(holds[side]);if(std::abs(shown-drawnSides[side])>.002f||std::abs(held-drawnHolds[side])>.002f){moved=true;drawnSides[side]=shown;drawnHolds[side]=held;}}
    if(moved){drawnLevel=level;drawnColour=colour;repaint(meterBounds());}}
float MixerStrip::faderY(double db)const{return (float)gain.getY()+(float)gain.getPositionOfValue(db);}
juce::Rectangle<int> MixerStrip::meterBounds()const{const bool master=track<0;const int width=master?14:9;const int top=(int)std::floor(faderY(6)),bottom=(int)std::ceil(faderY(-60));return {getWidth()-width-8,top,width,std::max(4,bottom-top)};}
void MixerStrip::paint(juce::Graphics& g){
    const bool master=track<0,current=track==owner.selectedTrack;const auto colour=master?Daw::accent:ink(track);const int w=getWidth(),h=getHeight();
    const auto body=getLocalBounds().toFloat().reduced(.5f);g.setColour(master||current?Daw::surfaceHi:Daw::surface);g.fillRoundedRectangle(body,Daw::radius);
    g.setColour(colour.withAlpha(master?.0f:.9f));g.fillRect(2.f,1.f,(float)w-4.f,4.f);
    // dB scale beside the fader, meters on the right (two sides with peak hold).
    if(gain.isVisible()&&gain.getHeight()>40){
        const int left=gain.getX()+gain.getWidth()/2-14;g.setFont(Daw::font(Daw::small));
        for(double db:{6.,0.,-6.,-12.,-24.,-48.}){const float y=faderY(db);g.setColour(db==0?Daw::textDim:Daw::textFaint);g.fillRect((float)left+11.f,y,4.f,1.f);
            // A 4 px gap keeps the tick from reading as a minus sign after the number.
            if(gain.getHeight()>90||db==0||db==-24)g.drawText(juce::String(db>0?"+":"")+juce::String((int)db),juce::Rectangle<float>((float)left-19.f,y-6.f,24.f,12.f),juce::Justification::centredRight);}
        const auto meter=meterBounds().toFloat();g.setColour(juce::Colours::black.withAlpha(.6f));g.fillRoundedRectangle(meter.expanded(1.f),1.5f);
        const float sideWidth=(meter.getWidth()-1.f)*.5f;const float top=faderY(6),bottom=faderY(-60);
        juce::ColourGradient gradient=juce::ColourGradient::vertical(Daw::meterHigh,top,Daw::meterLow,bottom);auto at=[&](double db){return std::clamp((faderY(db)-top)/std::max(1.f,bottom-top),0.f,1.f);};gradient.addColour(at(0),Daw::meterHigh);gradient.addColour(at(-6),Daw::meterMid);gradient.addColour(at(-14),Daw::meterLow);
        auto levelY=[&](float linear){if(linear<=.001f)return bottom;return faderY(std::clamp((double)juce::Decibels::gainToDecibels(linear,-60.f),-60.,6.));};
        for(int side=0;side<2;++side){const float x=meter.getX()+side*(sideWidth+1.f);const float y=levelY(sides[(size_t)side]);
            if(y<bottom){g.setGradientFill(gradient);g.fillRect(juce::Rectangle<float>(x,y,sideWidth,bottom-y));}
            const float hold=holds[(size_t)side];if(hold>.001f){g.setColour(Daw::meterColour(hold));g.fillRect(x,levelY(hold)-1.f,sideWidth,1.5f);}}
        g.setColour(juce::Colours::black.withAlpha(.5f));g.fillRect(meter.getX(),faderY(0),meter.getWidth(),1.f);
        // The fader's value under it.
        const auto value=juce::Rectangle<float>(4.f,(float)gain.getBottom()+2.f,(float)w-8.f,15.f);g.setColour(Daw::well);g.fillRoundedRectangle(value,2.f);
        const float db=(float)gain.getValue();g.setColour(gain.isMouseOverOrDragging()?Daw::text:Daw::textDim);g.setFont(Daw::font(Daw::small,true));g.drawText(db<=-59.9f?juce::String::fromUTF8("-\xe2\x88\x9e"):juce::String(db,1),value,juce::Justification::centred);}
    if(pan.isVisible()){const double value=pan.getValue();g.setColour(Daw::textFaint);g.setFont(Daw::font(Daw::small));g.drawText(std::abs(value)<.5?juce::String("C"):(value<0?"L":"R")+juce::String((int)std::round(std::abs(value))),juce::Rectangle<int>(pan.getX()-6,pan.getBottom(),pan.getWidth()+12,12),juce::Justification::centred);}
    // Name at the foot, on a band of the track colour.
    const auto band=juce::Rectangle<float>(2.f,6.f,(float)w-4.f,22.f);g.setColour(master?Daw::control:colour.withAlpha(current?.55f:.2f));g.fillRoundedRectangle(band,2.f);
    if(current){g.setColour(Daw::accent);g.drawRoundedRectangle(body.reduced(.75f),Daw::radius,1.5f);}
    else{g.setColour(juce::Colours::black.withAlpha(.6f));g.drawRoundedRectangle(body,Daw::radius,1.f);}
}
void MixerStrip::resized(){const int w=getWidth(),h=getHeight();const bool master=track<0,compact=h<180;name.setBounds(3,8,w-(master?6:27),18);output.setBounds(w-23,8,20,18);output.setVisible(!master);
    const int meterWidth=master?14:9,faderRight=w-meterWidth-10;const bool sourceVisible=!master&&h>=120;instrument.setVisible(sourceVisible);instrument.setBounds(5,h-22,w-10,18);clip.setBounds(6,32,w-12,18);
    if(compact){pan.setVisible(false);const int cell=(w-13)/4;mute.setBounds(4,32,cell,18);solo.setBounds(7+cell,32,cell,18);sendA.setBounds(10+cell*2,32,cell,18);sendB.setBounds(13+cell*3,32,cell,18);effects.setBounds(5,54,w-10,18);const int top=master?60:76;gain.setBounds(10,top,faderRight-10,std::max(18,h-top-(sourceVisible?40:22)));}
    else{pan.setVisible(!master);pan.setBounds(6,32,28,28);mute.setBounds(38,32,w-43,15);solo.setBounds(38,49,w-43,15);sendA.setBounds(5,73,(w-13)/2,18);sendB.setBounds(8+(w-13)/2,73,(w-13)/2,18);effects.setBounds(5,95,w-10,18);const int top=master?60:118;gain.setBounds(10,top,faderRight-10,std::max(20,h-top-40));}}
void MixerStrip::mouseDown(const juce::MouseEvent& e){if(track>=0){owner.select(track,owner.selectedSection);if(e.mods.isPopupMenu())owner.showTrackMenu(track);}}
DawMixer::DawMixer(MainComponent& m):owner(m),master(m,-1){getProperties().set("fgaTip","Drag the mixer header to resize its height; double-click to restore the default.");port.setViewedComponent(&channels,false);port.setScrollBarsShown(false,true);addAndMakeVisible(port);addAndMakeVisible(master);addAndMakeVisible(collapseButton);addAndMakeVisible(routingButton);routingButton.setIcon(IconButton::Icon::automation);collapseButton.setIcon(IconButton::Icon::mixer);routingButton.setComponentID("mixer-routing");routingButton.onClick=[this]{owner.showMixerRouting();};tip(routingButton,"Open mixer output groups and shared reverb/delay sends and returns.");collapseButton.setComponentID("mixer-collapse");
    tip(collapseButton,"Collapse the mixer to make room for tracks; click again to restore it. Shortcut: F3.");
    collapseButton.onClick=[this]{owner.mixerVisible=!owner.mixerVisible;owner.mixerToggle.setToggleState(owner.mixerVisible,juce::dontSendNotification);owner.resized();};
    for(int row=0;row<trackCount;++row){auto strip=std::make_unique<MixerStrip>(m,trackAtRow(row));channels.addAndMakeVisible(*strip);strips.push_back(std::move(strip));}}
void DawMixer::refresh(){
    const int count=(int)owner.project.tracks.size();
    while((int)strips.size()>count)strips.pop_back();
    while((int)strips.size()<count){auto strip=std::make_unique<MixerStrip>(owner,trackAtRow((int)strips.size()));channels.addAndMakeVisible(*strip);strips.push_back(std::move(strip));}
    resized();for(auto& s:strips)s->refresh();master.refresh();}
void DawMixer::tick(){
    for(auto& s:strips)s->tick();master.tick();bool moved=false;
    for(size_t side=0;side<2;++side){const float before=compactPeaks[side],held=compactHolds[side];compactPeaks[side]=std::max(owner.audio.masterLevel((int)side),before*.82f);
        if(compactPeaks[side]>=held){compactHolds[side]=compactPeaks[side];compactHoldTicks[side]=24;}else if(--compactHoldTicks[side]<=0)compactHolds[side]=std::max(compactPeaks[side],held*.9f);
        moved=moved||std::abs(Daw::meterPosition(before)-Daw::meterPosition(compactPeaks[side]))>.001f||std::abs(Daw::meterPosition(held)-Daw::meterPosition(compactHolds[side]))>.001f;}
    if(!owner.mixerVisible&&moved)repaint(0,0,std::max(0,getWidth()-320),24);
}
bool DawMixer::verifySends(const juce::File& folder){const auto saved=owner.project;auto undos=owner.undoStack,redos=owner.redoStack;const bool dirty=owner.dirty;const auto midi=midiFile(owner.project);bool ok=!strips.empty();if(ok){const int ti=trackAtRow(0);for(int i=0;i<2;++i){auto* button=dynamic_cast<juce::Button*>(strips[0]->findChildWithID(i==0?"mixer-send-a":"mixer-send-b"));if(!button){ok=false;continue;}const double old=owner.project.tracks[(size_t)ti].sends[(size_t)i];const auto count=owner.undoStack.size();button->onClick();const double value=owner.project.tracks[(size_t)ti].sends[(size_t)i];ok=ok&&(old>0?value==0:value>0)&&button->getToggleState()==(value>0)&&owner.undoStack.size()==count+1&&midiFile(owner.project)==midi;owner.undo(false);ok=ok&&owner.project.tracks[(size_t)ti].sends[(size_t)i]==old;}for(int h:{100,140,210,320}){strips[0]->setSize(74,h);for(auto* c:strips[0]->getChildren())if(c->isVisible())ok=ok&&strips[0]->getLocalBounds().contains(c->getBounds())&&!c->getBounds().isEmpty();}}
    owner.project=saved;owner.undoStack=std::move(undos);owner.redoStack=std::move(redos);owner.dirty=dirty;owner.audio.updateMix(owner.project);owner.refreshControls();resized();folder.getChildFile("mixer-send-buttons.txt").replaceWithText(ok?"PASS mixer A/reverb and B/delay toggles update real sends, restore amounts with Undo, preserve MIDI and fit compact strips":"FAIL mixer send buttons");return ok;}
bool DawMixer::verifyAutomation(const juce::File& folder){
#if !FGA_SELF_TESTS
    juce::ignoreUnused(folder);return false;
#else
    const auto saved=owner.project;const auto undos=owner.undoStack,redos=owner.redoStack;const bool dirty=owner.dirty,wasPlaying=owner.audio.isPlaying();
    const int selected=owner.selectedTrack,section=owner.selectedSection,clip=owner.selectedClip;const bool noClip=owner.noClipSelection;const double cursor=owner.editCursor,position=owner.audio.position();
    bool ok=strips.size()>1;juce::String failure;auto check=[&](bool pass,const char* step){if(!pass&&failure.isEmpty())failure=step;ok=pass&&ok;};
    owner.audio.stop();const int ti=trackAtRow(0),other=trackAtRow(1);auto& p=owner.project;p.loop=false;p.masterGain=.8;p.masterAutomation.enabled=true;p.masterAutomation.points={{0,.2,-.4},{4,1.8,0}};
    p.tracks[(size_t)ti].gain=.5;p.tracks[(size_t)ti].volume.enabled=true;p.tracks[(size_t)ti].volume.points={{0,.25,.7},{2,1.5,-.5},{4,.5,0}};
    p.tracks[(size_t)other].gain=.75;p.tracks[(size_t)other].volume.enabled=false;p.tracks[(size_t)other].volume.points={{0,0,0},{4,2,0}};
    owner.selectedTrack=ti;owner.selectedSection=0;owner.selectedClip=0;owner.noClipSelection=false;owner.editCursor=-1;owner.audio.publish(p);owner.refreshControls();
    auto* inspector=dynamic_cast<juce::Slider*>(owner.trackInspector.findChildWithID("track-gain"));
    auto* channel=dynamic_cast<juce::Slider*>(strips[0]->findChildWithID("mixer-gain"));auto* fixed=dynamic_cast<juce::Slider*>(strips[1]->findChildWithID("mixer-gain"));auto* output=dynamic_cast<juce::Slider*>(master.findChildWithID("mixer-gain"));
    check(inspector&&channel&&fixed&&output,"volume controls");
    if(inspector&&channel&&fixed&&output){
        auto* inspectorPan=dynamic_cast<juce::Slider*>(owner.trackInspector.findChildWithID("track-pan"));auto* mixerPan=dynamic_cast<juce::Slider*>(strips[0]->findChildWithID("mixer-pan"));
        check(inspectorPan&&mixerPan,"pan controls");
        p.tracks[(size_t)ti].pan=.2;p.tracks[(size_t)ti].panAutomation.enabled=true;p.tracks[(size_t)ti].panAutomation.points={{0,.1,.5},{4,.9,0}};owner.audio.updateMix(p);
        auto near=[](double a,double b){return std::abs(a-b)<.11;};
        auto verify=[&]{const double beat=owner.audio.position(),expected=db(p.tracks[(size_t)ti].gain*p.tracks[(size_t)ti].volume.valueAt(beat)),masterValue=db(p.masterGain*p.masterAutomation.valueAt(beat));
            check(near(inspector->getValue(),expected)&&near(channel->getValue(),expected)&&near(owner.gainSlider.getValue(),expected),"track automation readback");
            check(near(output->getValue(),masterValue)&&near(owner.masterSlider.getValue(),masterValue),"master automation readback");check(near(fixed->getValue(),db(.75)),"disabled automation");
            const double panValue=panFromAutomation(p.tracks[(size_t)ti].panAutomation.valueAt(beat))*100.;
            check(inspectorPan&&mixerPan&&near(inspectorPan->getValue(),panValue)&&near(mixerPan->getValue(),panValue)&&std::abs(owner.panSlider.getValue()-panValue)<.51,"pan automation readback");};
        const auto unchanged=juce::JSON::toString(projectJson(p),false,17);owner.audio.seek(0);owner.audio.play(true);owner.timerCallback();verify();const double first=inspector->getValue();
        juce::AudioBuffer<float> block(2,512);juce::AudioIODeviceCallbackContext context;
        for(int i=0;i<12;++i){owner.audio.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);owner.timerCallback();verify();}
        check(inspector->getValue()>first+.1,"motion follows audio callback");
        check(unchanged==juce::JSON::toString(projectJson(p),false,17)&&owner.undoStack.size()==undos.size()&&owner.redoStack.size()==redos.size()&&owner.dirty==dirty,"readback is non-destructive");
        owner.audio.seek(3.25);owner.timerCallback();verify();
        inspector->showTextBox();const double editing=inspector->getValue();owner.audio.seek(1.25);owner.timerCallback();check(near(inspector->getValue(),editing),"text entry is not overwritten");inspector->hideTextBox(true);owner.timerCallback();verify();
        for(auto* control:{inspector,channel,&owner.gainSlider,&owner.masterSlider,output}){
            const bool isMaster=control==&owner.masterSlider||control==output;const auto before=p;const double base=isMaster?p.masterGain:p.tracks[(size_t)ti].gain;const auto undoCount=owner.undoStack.size();const double requested=control->getValue()+1;
            control->onDragStart();control->setValue(requested,juce::sendNotificationSync);owner.audio.seek(owner.audio.position()<2?3.:1.);owner.timerCallback();
            check(near(control->getValue(),requested),"automation does not fight a drag");const double edited=isMaster?p.masterGain:p.tracks[(size_t)ti].gain;check(std::abs(db(edited)-db(base)-1)<.02,"manual edit trims the base once");
            auto expected=before;if(isMaster)expected.masterGain=edited;else expected.tracks[(size_t)ti].gain=edited;
            check(juce::JSON::toString(projectJson(expected),false,17)==juce::JSON::toString(projectJson(p),false,17)&&owner.undoStack.size()==undoCount+1,"manual edit keeps curves and one undo gesture");
            control->onDragEnd();owner.timerCallback();verify();
        }
        if(inspectorPan&&mixerPan)for(auto* control:{inspectorPan,mixerPan,&owner.panSlider}){
            owner.timerCallback();const double before=p.tracks[(size_t)ti].pan,shown=control->getValue();
            control->onDragStart();control->setValue(shown+5,juce::sendNotificationSync);
            check(std::abs(p.tracks[(size_t)ti].pan-before-.05)<.006,"pan gesture trims stored value once");control->onDragEnd();owner.timerCallback();verify();
        }
        {juce::Slider native;native.setRange(0,100,.1);Track t;ParameterAutomation lane;lane.slot=-1;lane.index=0;lane.curve.enabled=true;lane.curve.points={{0,.15,0},{4,.85,0}};t.parameters={lane};
            owner.audio.seek(2);owner.updateParameterDisplay(native,30,0,t);check(std::abs(native.getValue()-50)<.11,"native parameter readback");
            native.setValue(55,juce::dontSendNotification);check(std::abs(owner.valueFromControl(native)-35)<.11,"native manual trim");
            t.parameters[0].curve.enabled=false;owner.updateParameterDisplay(native,30,0,t);check(native.getValue()==30,"native bypass readback");
            t.parameters[0].index=4;t.parameters[0].curve.enabled=true;native.setRange(20,20000,1);owner.updateParameterDisplay(native,12000,4,t,true);check(std::abs(native.getValue()-20*std::sqrt(1000.))<1,"sampler logarithmic filter readback");}
        {auto& t=p.tracks[(size_t)ti];const auto parameters=t.parameters;const auto source=t.instrument;t.instrument="internal";const double tone=t.engine.tone;ParameterAutomation lane;lane.slot=-1;lane.index=0;lane.curve.enabled=true;lane.curve.points={{0,.67,0}};t.parameters={lane};
            owner.instrumentPanel.rebuild();juce::Slider* knob=nullptr;for(auto* child:owner.instrumentPanel.getChildren())if(child->getProperties()["fgaControl"].toString()=="TONE")knob=dynamic_cast<juce::Slider*>(child);
            check(knob!=nullptr,"native tone control exists");if(knob){const auto json=juce::JSON::toString(projectJson(p),false,17);owner.instrumentPanel.repaintMeters(false);check(std::abs(knob->getValue()-67)<.51,"native tone knob follows curve");owner.audio.play(false);owner.instrumentPanel.repaintMeters(false);check(std::abs(knob->getValue()-tone*100)<.51&&json==juce::JSON::toString(projectJson(p),false,17),"idle native knob restores base without edits");owner.audio.play(true);}t.parameters=parameters;t.instrument=source;owner.instrumentPanel.rebuild();}
        p.loop=true;p.loopStart=2;p.loopEnd=3;owner.audio.play(false);owner.audio.publish(p);owner.audio.seek(2.995);owner.audio.play(true);owner.audio.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);owner.timerCallback();
        check(owner.audio.position()>=2&&owner.audio.position()<2.1,"loop wraps");verify();
        p.tracks[(size_t)ti].gain=1.6;p.tracks[(size_t)ti].volume.points={{0,2,0}};p.masterGain=1.6;p.masterAutomation.points={{0,2,0}};owner.audio.updateMix(p);owner.refreshControls();owner.timerCallback();verify();check(inspector->getValue()>6&&output->getValue()>6&&owner.masterSlider.getValue()>6,"readback includes gain above base range");
        owner.selectedTrack=other;owner.refreshTrack();owner.timerCallback();check(near(inspector->getValue(),db(.75)),"selection changes followed track");owner.selectedTrack=ti;owner.refreshTrack();owner.timerCallback();verify();
        owner.audio.play(false);owner.timerCallback();check(inspectorPan&&mixerPan&&near(inspectorPan->getValue(),p.tracks[(size_t)ti].pan*100)&&near(mixerPan->getValue(),p.tracks[(size_t)ti].pan*100),"pan stop restores base");check(near(inspector->getValue(),db(1.6))&&near(channel->getValue(),db(1.6))&&near(output->getValue(),db(1.6))&&near(owner.masterSlider.getValue(),db(1.6))&&inspector->getMaximum()==6,"stop restores base controls");
        p.tracks[(size_t)ti].volume.enabled=false;p.masterAutomation.enabled=false;owner.audio.play(true);owner.refreshControls();owner.timerCallback();check(near(inspector->getValue(),db(1.6))&&near(channel->getValue(),db(1.6))&&near(output->getValue(),db(1.6))&&near(owner.masterSlider.getValue(),db(1.6)),"bypass restores base controls");
    }
    owner.audio.stop();owner.project=saved;owner.undoStack=undos;owner.redoStack=redos;owner.dirty=dirty;owner.selectedTrack=selected;owner.selectedSection=section;owner.selectedClip=clip;owner.noClipSelection=noClip;owner.editCursor=cursor;owner.audio.publish(saved);owner.audio.seek(position);if(wasPlaying)owner.audio.play(true);owner.refreshControls();
    folder.getChildFile("automation-controls.txt").replaceWithText(ok?"PASS volume/pan/native/filter readback: inspector, mixer, master and instrument toolbar; real audio advancement, curves, seek/loop, bypass/stop, selection, text editing, manual trim and unchanged project/Undo on readback":"FAIL automation controls: "+failure);return ok;
#endif
}
void DawMixer::reveal(int track){const int x=rowForTrack(track)*channelWidth;if(x<port.getViewPositionX())port.setViewPosition(x,0);else if(x+channelWidth-4>port.getViewPositionX()+port.getViewWidth())port.setViewPosition(x+channelWidth-4-port.getViewWidth(),0);}
void DawMixer::paint(juce::Graphics& g){g.fillAll(Daw::base);g.setColour(Daw::chrome);g.fillRoundedRectangle(0,0,(float)getWidth(),24,Daw::radius);g.setColour(juce::Colours::black.withAlpha(.5f));g.drawRoundedRectangle(juce::Rectangle<float>(0,0,(float)getWidth(),24).reduced(.5f),Daw::radius,1);
    if(owner.mixerVisible){Daw::drawCaps(g,tr("MIXER"),{10,0,120,24},Daw::textDim);caption(g,"Audio levels / MIDI velocities unchanged",{90,0,getWidth()-430,24});if(strips.empty()){g.setColour(Daw::textDim);g.setFont(Daw::font(Daw::body));IconButton::drawIcon(g,IconButton::Icon::mixer,{24.f,40.f,16.f,16.f},Daw::textDim);g.drawFittedText(tr("No tracks in the mixer"),48,36,getWidth()-80,24,juce::Justification::centredLeft,1,.7f);}}
    else{
        Daw::drawCaps(g,"MASTER",{12,0,54,24},Daw::textDim);const float left=18.f*Daw::unit,dbW=17.f*Daw::unit;const int reserve=std::min(routingButton.getX(),collapseButton.getX())-Daw::unit;const float width=std::max(10.f*Daw::unit,(float)reserve-left-2.f*Daw::unit-dbW);const int segments=std::clamp((int)(width/7),24,160);const float cell=width/segments;
        for(size_t side=0;side<2;++side){const float y=side==0?6.f:14.f;const float level=Daw::meterPosition(compactPeaks[side]);
            for(int part=0;part<segments;++part){const float value=(part+.5f)/segments;auto colour=value>.91f?Daw::meterHigh:value>.82f?Daw::meterMid:Daw::meterLow;g.setColour(value<=level?colour:colour.withAlpha(.09f));g.fillRoundedRectangle(left+part*cell,y,std::max(1.f,cell-1.f),4.f,1.f);}
            if(compactHolds[side]>.001f){g.setColour(Daw::meterColour(compactHolds[side]));g.fillRect(left+std::min(width-1.f,width*Daw::meterPosition(compactHolds[side])),y,1.5f,4.f);}}
        const float peak=std::max(compactPeaks[0],compactPeaks[1]);g.setColour(peak>=1?Daw::meterHigh:Daw::textDim);g.setFont(Daw::font(Daw::small,true));g.drawText(peak>.001f?juce::String(juce::Decibels::gainToDecibels(peak),1)+" dB":juce::String::fromUTF8("−∞ dB"),(int)(left+width+12),0,64,24,juce::Justification::centredLeft);
    }
    if(owner.mixerVisible){g.setColour(Daw::textFaint.withAlpha(.6f));for(int dot=-2;dot<=2;++dot)g.fillEllipse(getWidth()*.5f+dot*7-1.5f,10.5f,3.f,3.f);}}
void DawMixer::resized(){auto bar=getLocalBounds().removeFromTop(Daw::toolbarHeight).reduced(0,2);auto collapse=bar.removeFromRight(36*Daw::unit);bar.removeFromRight(Daw::unit);routingButton.getProperties().set("fgaIconOnly",true);routingButton.setTitle(tr("BUS & SEND"));auto routing=bar.removeFromRight(Daw::controlHeight);routingButton.setBounds(routing);routingButton.setButtonText("BUS & SEND");translateControls(routingButton);collapseButton.setBounds(collapse);collapseButton.setButtonText(owner.mixerVisible?"COLLAPSE MIXER":"SHOW MIXER");translateControls(collapseButton);collapseButton.setTitle(collapseButton.getButtonText());port.setVisible(owner.mixerVisible);master.setVisible(owner.mixerVisible);if(!owner.mixerVisible)return;auto body=getLocalBounds().withTrimmedTop(Daw::toolbarHeight+Daw::unit);auto masterArea=body.removeFromRight(26*Daw::unit);port.setBounds(body);const int height=std::max(100,port.getHeight()-16);channelWidth=std::clamp(port.getWidth()/std::max(1,(int)strips.size()),74,100);channels.setSize((int)strips.size()*channelWidth,height);for(int row=0;row<(int)strips.size();++row)strips[(size_t)row]->setBounds(row*channelWidth,0,channelWidth-Daw::unit+1,height);master.setBounds(masterArea.withHeight(height));}
void DawMixer::mouseDown(const juce::MouseEvent& e){if(e.y<25&&owner.mixerVisible){resizingMixer=true;resizeOrigin=e.getEventRelativeTo(&owner).position.y;resizeHeight=getHeight();}}
void DawMixer::mouseDrag(const juce::MouseEvent& e){if(resizingMixer){owner.mixerHeight=std::clamp(resizeHeight+juce::roundToInt(resizeOrigin-e.getEventRelativeTo(&owner).position.y),160,std::max(160,owner.getHeight()-476));owner.resized();}}
void DawMixer::mouseUp(const juce::MouseEvent&){if(resizingMixer)Localisation::setSetting("mixerHeight",owner.mixerHeight);resizingMixer=false;}
void DawMixer::mouseDoubleClick(const juce::MouseEvent& e){if(e.y<25&&owner.mixerVisible){resizingMixer=false;owner.mixerHeight=244;owner.resized();Localisation::setSetting("mixerHeight",owner.mixerHeight);}}
void DawMixer::mouseMove(const juce::MouseEvent& e){setMouseCursor(e.y<25&&owner.mixerVisible?juce::MouseCursor::UpDownResizeCursor:juce::MouseCursor::NormalCursor);}

bool MainComponent::verifyDawWorkspace(const juce::File& folder){
#if !FGA_SELF_TESTS
return false;
#else
    const auto saved=project;const int track=selectedTrack,section=selectedSection;const auto zoom=arrangement.pixelsPerBar;
    showArrangement();audio.stop();bool ok=inspectorPort.isVisible()&&mixer.isVisible()&&!arrangementPort.getBounds().intersects(inspectorPort.getBounds())&&!arrangementPort.getBounds().intersects(mixer.getBounds());
    juce::String firstFailure;auto stage=[&](const char* name){if(!ok&&firstFailure.isEmpty())firstFailure=name;};stage("initial dock layout");
    const int expandedHeight=mixer.getHeight(),trackAreaHeight=arrangementPort.getHeight();
    const auto unchangedMidi=midiFile(project);const double unchangedPosition=audio.position();
    if(auto* collapse=dynamic_cast<juce::Button*>(mixer.findChildWithID("mixer-collapse"))){
        collapse->onClick();ok=ok&&!mixerVisible&&mixer.isVisible()&&mixer.getHeight()==28&&arrangementPort.getHeight()>trackAreaHeight&&!mixerToggle.getToggleState();
        if(auto output=folder.getChildFile("mixer-collapsed.png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(createComponentSnapshot(getLocalBounds()),*output)&&ok;}else ok=false;
        const int trackAreaWidth=arrangementPort.getWidth();
        inspectorCollapse.onClick();ok=ok&&!inspectorVisible&&!inspectorPort.isVisible()&&inspectorPort.getWidth()==0&&arrangementPort.getWidth()>trackAreaWidth&&!inspectorToggle.getToggleState()&&!inspectorCollapse.isVisible()&&arrangementPort.getX()==12;
        if(auto output=folder.getChildFile("workspace-collapsed.png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(createComponentSnapshot(getLocalBounds()),*output)&&ok;}else ok=false;
        inspectorToggle.setToggleState(true,juce::dontSendNotification);inspectorToggle.onClick();ok=ok&&inspectorVisible&&inspectorPort.getWidth()==inspectorWidth&&arrangementPort.getWidth()==trackAreaWidth&&inspectorToggle.getToggleState();
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
    inspectorResizer.mouseDoubleClick(resizeEvent(&inspectorResizer,2,20));mixer.mouseDoubleClick(resizeEvent(&mixer,600,10));ok=ok&&inspectorWidth==248&&mixerHeight==244;
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
    {   // Compact inspector cards stay readable at the minimum, default and expanded widths.
        const int previous=selectedTrack;const int width=trackInspector.getWidth();
        for(int track:{0,4,leadTrack}){select(track,1);for(int size:{208,232,400}){
            trackInspector.setSize(size,trackInspector.preferredHeight());trackInspector.refresh();
            const auto children=trackInspector.getChildren();
            for(int a=0;a<children.size();++a)if(children[a]->isVisible()){
                ok=ok&&trackInspector.getLocalBounds().contains(children[a]->getBounds())&&!children[a]->getBounds().isEmpty();
                for(int b=a+1;b<children.size();++b)if(children[b]->isVisible())ok=ok&&!children[a]->getBounds().intersects(children[b]->getBounds());
            }
        }}trackInspector.setSize(width,trackInspector.preferredHeight());select(previous,1);resized();
        stage("inspector cards fit every supported width and Lead selection");
    }

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
    ok=ok&&page==Page::arrangement&&instrumentIsOpen()&&selectedTrack==4;stage("explicit header engine gear");showArrangement();
    const auto beforeLead=project;const auto oldLead=project.settings.guitarLead;
    ok=ok&&leadButton.isVisible()&&!leadButton.getBounds().intersects(loopSelectionButton.getBounds());
    if(leadButton.onClick)leadButton.onClick();
    ok=ok&&page==Page::arrangement&&project.settings.guitarLead!=oldLead;
    undo(false);ok=ok&&project.settings.guitarLead==oldLead;
    for(int ti=0;ti<trackCount;++ti)ok=ok&&project.tracks[(size_t)ti].clips==beforeLead.tracks[(size_t)ti].clips;
    stage("header selection, explicit engine gear and main guitar lead toggle with undo");
    const double gridPosition=audio.position();arrangement.mouseDown(event(232,76));arrangement.mouseUp(event(232,76));ok=ok&&page==Page::arrangement&&selectedTrack==4&&selectedSection==0&&audio.position()==gridPosition&&!audio.isPlaying();
    arrangement.mouseDown(event(232,76));arrangement.mouseDrag(event(360,76));arrangement.mouseUp(event(360,76));ok=ok&&project.tracks[4].freeTimeline&&project.tracks[4].clips.size()==1&&project.clipStart(4,0)==16&&project.tracks[4].clips[0].notes[0].start==0;
    undo(false);ok=ok&&project.tracks[4].clips[0].notes[0].start==0&&project.tracks[4].clips[1].notes[0].start==1;
    arrangement.mouseDown(event(365,20));arrangement.mouseUp(event(365,20));
    ok=ok&&selectedSection==1&&std::abs(audio.position()-18.625)<1e-9&&audio.isPlaying();
    audio.play(true);ok=ok&&std::abs(audio.position()-18.625)<1e-9;
    arrangement.mouseDown(event(291,116));arrangement.mouseUp(event(291,116));
    ok=ok&&selectedTrack==5&&audio.isPlaying()&&std::abs(audio.position()-18.625)<1e-9;audio.play(false);
    arrangement.pixelsPerBar=240;updateSizes();arrangementPort.setViewPosition(320,0);
    const int scroll=arrangementPort.getViewPositionX();
    arrangement.mouseDown(event((float)(scroll+253),76));arrangement.mouseUp(event((float)(scroll+253),76));
    ok=ok&&scroll>0&&std::abs(audio.position()-18.625)<1e-9;
    arrangement.pixelsPerBar=32;updateSizes();arrangementPort.setViewPosition(0,0);
    arrangement.mouseDown(event(283,46));arrangement.mouseUp(event(283,46));ok=ok&&std::abs(audio.position()-8.375)<1e-9;
    stage("grid selection leaves transport untouched; section header starts audition and ruler seeks precisely");
    arrangement.mouseDown(event(280,46));arrangement.mouseUp(event(280,46));ok=ok&&std::abs(audio.position()-8)<1e-9;
    arrangement.mouseDown(event(248,46,juce::ModifierKeys::shiftModifier));arrangement.mouseDrag(event(312,46,juce::ModifierKeys::shiftModifier));arrangement.mouseUp(event(312,46));ok=ok&&project.loop&&project.loopStart==4&&project.loopEnd==16&&dirty;
    arrangement.mouseDoubleClick(event(232,76));ok=ok&&page==Page::editor&&pianoPort.isVisible();
    stage("ruler, loop and full MIDI editor");
    showArrangement();ok=ok&&arrangement.verifyLoopEditing();stage("loop brace: edge resize, move, draw on the ruler, grid snap, one-beat minimum, click to seek and Undo");
    showArrangement();project.loop=false;changed();arrangementPort.setViewPosition(0,0);
    const double beforeEdgeResize=audio.position();
    arrangement.mouseDown(event(342,76));arrangement.mouseDrag(event(278,76));arrangement.mouseUp(event(278,76));
    ok=ok&&project.clipLength(4,0)==8&&project.clipLength(5,0)==16&&project.sections[0].bars==4&&audio.position()==beforeEdgeResize;
    arrangement.mouseDown(event(278,76));arrangement.mouseDrag(event(406,76));arrangement.mouseUp(event(406,76));
    ok=ok&&project.clipLength(4,0)==24&&project.tracks[4].clips[0].notes.size()==2&&project.tracks[4].clips[0].notes[1].start==16;
    if(auto output=folder.getChildFile("clip-loop-resize.png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(createComponentSnapshot(getLocalBounds()),*output)&&ok;}else ok=false;
    {juce::AlertWindow dialog(tr("Unsaved changes"),tr("The project has unsaved changes. Save them before closing?"),juce::MessageBoxIconType::QuestionIcon,this);dialog.addButton(tr("SAVE"),1);dialog.addButton(tr("Discard"),2);dialog.addButton(tr("Cancel"),0);   // the themed close dialog
        if(auto output=folder.getChildFile("close-dialog.png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(dialog.createComponentSnapshot(dialog.getLocalBounds()),*output)&&ok;}else ok=false;}
    Project loaded;ok=ok&&projectFromJson(projectJson(project),loaded).wasOk()&&loaded.tracks[4].clips[0]==project.tracks[4].clips[0]&&events(loaded,4).size()==events(project,4).size();
    undo(false);ok=ok&&project.clipLength(4,0)==8;undo(false);ok=ok&&project.clipLength(4,0)==16&&project.tracks[4].clips[0].notes.size()==1;
    arrangement.mouseDown(event(342,16));arrangement.mouseDrag(event(406,16));arrangement.mouseUp(event(406,16));
    ok=ok&&project.sections[0].bars==6&&project.sectionStart(1)==24&&project.tracks[4].clips[0].notes.size()==1;
    for(int ti=0;ti<trackCount;++ti)ok=ok&&project.clipLength(ti,0)==16&&project.clipStart(ti,1)==16;
    if(auto output=folder.getChildFile("section-loop-resize.png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(createComponentSnapshot(getLocalBounds()),*output)&&ok;}else ok=false;
    const auto beforeCancel=midiFile(project);const auto historySize=undoStack.size();arrangement.mouseDown(event(406,16));arrangement.mouseDrag(event(470,16));arrangement.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey));arrangement.mouseUp(event(470,16));
    ok=ok&&project.sections[0].bars==6&&midiFile(project)==beforeCancel&&undoStack.size()==historySize;
    audio.seek(2);arrangement.mouseDown(event(406,76));arrangement.mouseUp(event(406,76));ok=ok&&std::abs(audio.position()-2)<1e-9;
    stage("individual loops and independent guide resizing, persistence, Undo and Escape");

    {   // User-reported problems: loop removal, inspector sample browser and sound names,
        // editor REGENERATE and instrument-panel changes, each with Undo.
        const auto savedProject=project;showArrangement();arrangementPort.setViewPosition(0,0);select(4,1);project.clearLoop();changed(false);
        sectionLoopButton.onClick();ok=ok&&project.loop&&clipLoopActive()&&sectionLoopButton.getToggleState();stage("clip loop enabled");
        sectionLoopButton.onClick();ok=ok&&!project.loop&&!project.hasLoopRegion()&&!sectionLoopButton.getToggleState();stage("clip loop removed");
        undo(false);ok=ok&&project.loop&&clipLoopActive();undo(true);ok=ok&&!project.loop&&!project.hasLoopRegion();stage("clip loop Undo/Redo");
        keyPressed(juce::KeyPress('L'));ok=ok&&project.loop&&project.loopStart==0&&project.loopEnd==project.length();keyPressed(juce::KeyPress('L'));ok=ok&&!project.loop;stage("global loop shortcut");
        checkpoint();setLoopRange(4,16);arrangementPort.setViewPosition(0,0);arrangement.mouseDoubleClick(event(280,46));ok=ok&&!project.loop&&!project.hasLoopRegion();stage("loop double-click removal");
        undo(false);ok=ok&&project.loop&&project.loopStart==4&&project.loopEnd==16;
        stage("loop on/off, removal, double-click clear and Undo");
        showSampleBrowser(false);select(1,0);auto* chooserBefore=chooser.get();
        if(auto* button=dynamic_cast<juce::Button*>(trackInspector.findChildWithID("inspector-sample")))button->onClick();else ok=false;
        ok=ok&&browserVisible&&sampleBrowser.isVisible()&&chooser.get()==chooserBefore&&page==Page::arrangement&&selectedTrack==1;
        if(auto* button=dynamic_cast<juce::Button*>(trackInspector.findChildWithID("inspector-sample")))button->onClick();else ok=false;
        ok=ok&&!browserVisible&&!sampleBrowser.isVisible()&&selectedTrack==1;showSampleBrowser(false);
        stage("inspector LOAD SAMPLE opens the integrated browser and closes it again");
        auto* sourceLabel=dynamic_cast<juce::Label*>(trackInspector.findChildWithID("track-source"));
        ok=ok&&sourceLabel!=nullptr&&sourceLabel->getText()==soundSourceName(1)&&soundSourceName(1)==juce::String(guideSoundName(std::clamp(project.tracks[1].sound,0,6)))&&soundSourceName(4)==juce::String(GuideDrums::kitName(std::clamp(project.tracks[4].sound,0,3)));
        project.tracks[5].instrument="sample";project.tracks[5].instrumentPath=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("My Snare.wav").getFullPathName().toStdString();ok=ok&&soundSourceName(5)=="My Snare";
        project.tracks[6].instrument="plugin";project.tracks[6].instrumentPath="/missing/Big Synth.vst3";ok=ok&&soundSourceName(6).contains("Big Synth");
        project=savedProject;changed();
        stage("track headers and inspector name the actual sound");
        select(0,1);showEditor();const auto beforeEdit=clip();clip().notes.clear();regenerateClipButton.onClick();
        ok=ok&&regenerateClipButton.isVisible()&&!clip().notes.empty()&&clip().seed!=beforeEdit.seed;undo(false);ok=ok&&clip().notes.empty();
        clip().locked=true;const auto lockedClip=clip();regenerateClipButton.onClick();ok=ok&&clip()==lockedClip;clip().locked=false;
        ok=ok&&drawTool.getButtonText().isEmpty()&&drawTool.getTooltip().isNotEmpty()&&eraseTool.getTooltip().isNotEmpty()&&selectTool.getTooltip().isNotEmpty();
        stage("editor REGENERATE with locks and Undo; tool icons with tips");
        project=savedProject;changed();select(0,1);showInstrument();const auto pianoBefore=project.tracks[0].clips;
        juce::ComboBox* groove=nullptr;for(auto* child:instrumentPanel.getChildren())if(child->getProperties()["fgaControl"].toString()=="GROOVE")groove=dynamic_cast<juce::ComboBox*>(child);
        if(groove){groove->setSelectedItemIndex(groove->getSelectedItemIndex()==2?3:2,juce::sendNotificationSync);}
        ok=ok&&groove!=nullptr&&project.tracks[0].clips!=pianoBefore;undo(false);ok=ok&&project.tracks[0].clips==pianoBefore;
        stage("instrument panel groove re-renders the piano, Undo restores it");
        project=savedProject;changed();showArrangement();
    }

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
    {
        const auto before=project;const auto count=project.tracks.size();
        addMidiTrack();const int added=selectedTrack;
        ok=ok&&added==(int)count&&project.tracks.size()==count+1&&page==Page::editor&&!piano.laneView()&&clip().notes.empty();stage("add manual MIDI track opens a blank piano roll");
        undo(false);ok=ok&&project.tracks.size()==count&&selectedTrack<(int)count;
        undo(true);ok=ok&&project.tracks.size()==count+1;select(added,0);
        if(auto* label=dynamic_cast<juce::Label*>(trackInspector.findChildWithID("track-name"))){label->setText("La mia melodia",juce::sendNotificationSync);ok=ok&&project.tracks[(size_t)added].name=="La mia melodia";if(instrumentIsOpen())ok=ok&&instrumentPort.getTopLevelComponent()->getName().contains("La mia melodia");}else ok=false;
        ok=ok&&piano.verifyEditing();clip().notes={{0,1,72,100,0},{2,.5,76,91,0}};changed();
        const auto written=project.tracks[(size_t)added].clips;
        regenerate(Scope::song);ok=ok&&project.tracks[(size_t)added].clips==written;
        Project restored;ok=ok&&projectFromJson(projectJson(project),restored).wasOk()&&restored.tracks[(size_t)added].clips==written&&restored.tracks[(size_t)added].name=="La mia melodia";
        showArrangement();const auto mainPage=page;showInstrument();ok=ok&&page==mainPage&&instrumentIsOpen()&&instrumentPanel.layoutProblems().isEmpty();
        stage("manual MIDI editing, rename, generation isolation, save/load, add undo/redo and detached panel");
        const int following=project.addMidiTrack();project.tracks[(size_t)following].name="Keep this track";project.tracks[(size_t)following].sound=3;project.tracks[(size_t)following].clips[0].notes={{1,.5,79,97,0}};const auto followingNotes=project.tracks[(size_t)following].clips;
        select(added,0);changed();
        if(auto* remove=dynamic_cast<juce::Button*>(trackInspector.findChildWithID("delete-midi-track")))remove->onClick();else ok=false;
        ok=ok&&project.tracks.size()==count+1&&project.tracks[(size_t)added].name=="Keep this track"&&project.tracks[(size_t)added].clips==followingNotes&&project.tracks[(size_t)added].sound==3;
        undo(false);ok=ok&&project.tracks.size()==count+2&&project.tracks[(size_t)added].clips==written;
        undo(true);ok=ok&&project.tracks.size()==count+1&&project.tracks[(size_t)added].clips==followingNotes;
        const auto generatedCount=project.tracks.size();deleteMidiTrack(0);ok=ok&&project.tracks.size()==generatedCount;
        stage("delete manual track with later-track routing and undo/redo; generated tracks protected");
        const auto capacityBefore=project;while(project.tracks.size()<maxTracks)project.addMidiTrack();changed();resized();ok=ok&&!addTrackButton.isEnabled();
        deleteMidiTrack(maxTracks-1);ok=ok&&addTrackButton.isEnabled();addMidiTrack();ok=ok&&project.tracks.size()==maxTracks&&!addTrackButton.isEnabled();undo(false);ok=ok&&addTrackButton.isEnabled();
        project=capacityBefore;selectedTrack=0;selectedClip=-1;changed();resized();stage("delete frees track capacity and re-enables add MIDI, including after undo");
        for(int type=0;type<6;++type){const auto formBefore=project;const int at=selectedSection+1;addSongSection((Section)type);ok=ok&&project.sections[(size_t)at].type==(Section)type&&project.sections.size()==formBefore.sections.size()+1;undo(false);ok=ok&&project.sections.size()==formBefore.sections.size();}
        stage("all six section menu choices insert the chosen role and undo");
        select(0,0);showEditor();piano.pixelsPerBeat=160;pianoPort.setViewPosition(200,0);regenerateEditedClip();
        ok=ok&&pianoPort.getViewPositionX()==0&&piano.pixelsPerBeat<160&&piano.leftMargin()+clipLength()*piano.pixelsPerBeat<=pianoPort.getWidth();
        piano.pixelsPerBeat=160;regenerate(Scope::clip,false);ok=ok&&piano.pixelsPerBeat<160&&pianoPort.getViewPositionX()==0;
        int lowest=127,highest=0;for(const auto& n:clip().notes){lowest=std::min(lowest,n.pitch);highest=std::max(highest,n.pitch);}ok=ok&&piano.lowNote<=lowest&&piano.highNote>=highest;
        showArrangement();arrangement.pixelsPerBar=160;regenerate(Scope::song);ok=ok&&arrangement.pixelsPerBar<160&&arrangementPort.getViewPositionX()==0;
        stage("regeneration fits horizontal clip/song zoom and the generated pitch range");
        project=newProject(1);project.sections={{Section::main,4,"Main"}};project.normalise();selectedTrack=0;selectedSection=0;selectedClip=-1;changed();showInstrument();
        juce::ComboBox* extensions=nullptr;for(auto* child:instrumentPanel.getChildren())if(child->getProperties()["fgaControl"].toString()=="Extensions")extensions=dynamic_cast<juce::ComboBox*>(child);
        if(extensions){extensions->setSelectedItemIndex(1,juce::sendNotificationSync);extensions->setSelectedItemIndex(0,juce::sendNotificationSync);const auto triads=project.tracks[0].clips[0].notes;ok=ok&&project.settings.extension==0;extensions->setSelectedItemIndex(2,juce::sendNotificationSync);ok=ok&&project.settings.extension==2&&project.tracks[0].clips[0].notes!=triads;extensionBox.setSelectedItemIndex(0,juce::sendNotificationSync);ok=ok&&project.settings.extension==0&&project.tracks[0].clips[0].notes==triads;}else ok=false;
        stage("piano Extensions changes real MIDI immediately from both panel and main harmony controls");
        // Earlier regeneration fixtures queue panel rebuilds. Drain those callbacks
        // before retaining a control across real mouse notifications.
        bool controlsReady=false;juce::MessageManager::callAsync([&controlsReady]{controlsReady=true;});
        while(!controlsReady)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
        juce::Slider* range=nullptr;for(auto* child:instrumentPanel.getChildren())if(child->getProperties()["fgaControl"].toString()=="Range")range=dynamic_cast<juce::Slider*>(child);
        juce::String rangeEvidence;
        auto rangeCheck=[&](const char* name,bool passed){rangeEvidence+=juce::String(passed?"PASS ":"FAIL ")+name+"\n";ok=passed&&ok;};
        if(range){
            // Pixel checks catch the old stationary thumb; value-only tests miss it.
            const auto hasThumb=[&](double note){const auto image=range->createComponentSnapshot(range->getLocalBounds());return image.getPixelAt(juce::roundToInt(range->getPositionOfValue(note)),range->getHeight()/2-3).getBrightness()>.6f;};
            rangeCheck("two-value slider style",range->getSliderStyle()==juce::Slider::TwoValueHorizontal);rangeCheck("initial low thumb visible",hasThumb(range->getMinValue()));rangeCheck("initial high thumb visible",hasThumb(range->getMaxValue()));
            const auto drag=[&](double from,double to){
                const juce::Point<float> start(range->getPositionOfValue(from),range->getHeight()*.5f),end(range->getPositionOfValue(to),start.y);
                const auto event=[&](juce::Point<float> point,bool dragged){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,range,range,juce::Time::getCurrentTime(),start,juce::Time::getCurrentTime(),1,dragged);};
                rangeCheck("range is the hit-test target",instrumentPanel.getComponentAt(range->getPosition()+start.toInt())==range);
                range->mouseDown(event(start,false));range->mouseDrag(event(end,true));
                // A quick release must commit before the queued JUCE notification.
                range->mouseUp(event(end,true));instrumentPanel.flushRender();
            };
            const auto beforeRange=project.tracks[0].clips[0].notes;
            drag(range->getMinValue(),72);rangeCheck("low slider moved to 72",range->getMinValue()==72);rangeCheck("low MIDI setting committed at release",project.tracks[0].engine.rangeLow==72);rangeCheck("low thumb visible at 72",hasThumb(72));
            drag(range->getMaxValue(),84);rangeCheck("high slider moved to 84",range->getMaxValue()==84);rangeCheck("high MIDI setting committed at release",project.tracks[0].engine.rangeHigh==84);rangeCheck("both moved thumbs visible",hasThumb(72)&&hasThumb(84));rangeCheck("old thumb positions cleared",!hasThumb(24)&&!hasThumb(108));
            rangeCheck("range changes real MIDI",project.tracks[0].clips[0].notes!=beforeRange);
            if(auto output=folder.getChildFile("piano-range.png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(instrumentPort.createComponentSnapshot(instrumentPort.getLocalBounds()),*output)&&ok;}else ok=false;
            undo(false);rangeCheck("one Undo restores high bound",project.tracks[0].engine.rangeLow==72&&project.tracks[0].engine.rangeHigh==108);
            undo(false);rangeCheck("second Undo restores both bounds and MIDI",project.tracks[0].engine.rangeLow==24&&project.tracks[0].engine.rangeHigh==108&&project.tracks[0].clips[0].notes==beforeRange);
        }else rangeCheck("range control exists",false);
        folder.getChildFile("piano-range-gesture.txt").replaceWithText(rangeEvidence);
        stage("piano note range draws both real thumbs, follows mouse drags and updates MIDI");
        if(auto* floating=dynamic_cast<juce::DocumentWindow*>(instrumentPort.getTopLevelComponent())){
            ok=ok&&floating->isAlwaysOnTop();floating->setTopLeftPosition(37,46);floating->closeButtonPressed();ok=ok&&!floating->isVisible();showInstrument();
            if(auto* display=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())ok=ok&&floating->getBounds().getCentre().getDistanceFrom(display->userArea.getCentre())<3;
            ok=ok&&floating->isVisible()&&floating->isAlwaysOnTop();
        }else ok=false;
        stage("instrument window stays on top, closes manually and reopens centred");
        select(16,0);showEditor();ok=ok&&piano.verifyEditing();stage("handwritten Crash has factory gate, and drum notes can be resized");
        project=before;selectedTrack=0;selectedClip=-1;changed();
    }
    project=saved;arrangement.pixelsPerBar=zoom;select(track,section);changed();showArrangement();undoStack.clear();redoStack.clear();refreshControls();
    folder.getChildFile("daw-workspace.txt").replaceWithText(ok?"PASS inspector/mixer layout, mixer and inspector collapse/restore, drag resize with saved dimensions, MIDI/transport unchanged, audio-only fader with undo, clip select/move/undo, individual loop and independent guide edge resizing with source recovery/Undo/Escape/project round-trip, section-header audition, grid selection without transport jumps, precise ruler seek, loop drag persisted, full MIDI editor, visible Piano range thumbs with mouse dragging, MIDI update and Undo":"FAIL DAW workspace regression: "+firstFailure);return ok;
#endif
}


// Sample browser end to end: saved folders and favourites, navigation and history, search,
// filters and sorting, info, preview, unsupported and missing files, double-click import into
// the selected track, drag onto another track, layout and persistence. Headless (smoke test).
bool MainComponent::verifySampleBrowser(const juce::File& folder){
#if !FGA_SELF_TESTS
return false;
#else
    bool ok=true;juce::String firstFailure;auto stage=[&](const char* name,bool value){if(!value&&firstFailure.isEmpty())firstFailure=name;ok=ok&&value;};
    const auto savedSetting=Localisation::setting("sampleBrowser");const bool savedVisible=browserVisible;const auto savedProject=project;const int savedTrack=selectedTrack;
    auto library=folder.getChildFile("browser-library");library.deleteRecursively();auto kicks=library.getChildFile("Drums").getChildFile("Kicks");kicks.createDirectory();
    auto wav=[](const juce::File& f,double rate,int channels,int frames){juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> w(format.createWriterFor(f.createOutputStream().release(),rate,(unsigned)channels,16,{},0));
        juce::AudioBuffer<float> b(channels,frames);for(int ch=0;ch<channels;++ch)for(int i=0;i<frames;++i)b.setSample(ch,i,.5f*(float)std::sin(i*.03)*(1.f-(float)i/(float)frames));w->writeFromAudioSampleBuffer(b,0,frames);};
    wav(kicks.getChildFile("Kick Deep.wav"),44100,1,22050);wav(kicks.getChildFile("Kick Short.wav"),48000,2,4800);wav(library.getChildFile("Clap.wav"),44100,2,44100);
    library.getChildFile("Loop.mp3").replaceWithText("mp3");library.getChildFile("readme.txt").replaceWithText("text");
    showSampleBrowser(true);stage("browser shown",sampleBrowser.isVisible()&&!sampleBrowser.getBounds().intersects(arrangementPort.getBounds())&&!sampleBrowser.getBounds().intersects(mixer.getBounds())&&sampleBrowser.verifyLayout());
    {
        // The browser's left edge: dragged wider, remembered, and a double-click goes back to the automatic width.
        auto edgeEvent=[this](float x){const juce::Point<float> p(x,20.f);return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&browserResizer,&browserResizer,juce::Time::getCurrentTime(),p,juce::Time::getCurrentTime(),1,false);};
        const int before=sampleBrowser.getWidth(),savedWidth=sampleBrowserWidth;
        browserResizer.mouseDown(edgeEvent(2.f));browserResizer.mouseDrag(edgeEvent(-38.f));browserResizer.mouseUp(edgeEvent(-38.f));
        bool edge=browserResizer.isVisible()&&sampleBrowser.getWidth()==before+40&&Localisation::setting("sampleBrowserWidth","").getIntValue()==before+40
                  &&!sampleBrowser.getBounds().intersects(arrangementPort.getBounds())&&browserResizer.getRight()==sampleBrowser.getX();
        browserResizer.mouseDoubleClick(edgeEvent(2.f));edge=edge&&sampleBrowserWidth==0;
        sampleBrowserWidth=savedWidth;Localisation::setSetting("sampleBrowserWidth",savedWidth);resized();
        stage("sample browser resized by its edge and remembered",edge);
    }
    if(samples::Preferences::fromJson(savedSetting).folders.empty())if(auto out=folder.getChildFile("sample-browser-empty.png").createOutputStream()){juce::PNGImageFormat png;png.writeImageToStream(createComponentSnapshot(getLocalBounds()),*out);}
    sampleBrowser.addFolder(library);sampleBrowser.toggleFavourite(library.getFullPathName());sampleBrowser.navigate(library);stage("listing",sampleBrowser.waitForListing(5000));
    stage("folders first, audio only",sampleBrowser.visibleRows()==3&&sampleBrowser.fileAt(0).getFileName()=="Drums"&&sampleBrowser.fileAt(1).getFileName()=="Clap.wav"&&sampleBrowser.fileAt(2).getFileName()=="Loop.mp3");
    sampleBrowser.setFilter(samples::allFiles);stage("all files filter",sampleBrowser.visibleRows()==4);sampleBrowser.setFilter(samples::allAudio);
    sampleBrowser.navigate(kicks);sampleBrowser.waitForListing(5000);stage("breadcrumb from the saved folder",sampleBrowser.crumbNames().joinIntoString("/")=="browser-library/Drums/Kicks");
    sampleBrowser.setSort(samples::byLength,true);sampleBrowser.waitForListing(500);for(int i=0;i<40&&sampleBrowser.fileAt(0).getFileName()!="Kick Short.wav";++i)juce::MessageManager::getInstance()->runDispatchLoopUntil(25);
    stage("sort by length",sampleBrowser.fileAt(0).getFileName()=="Kick Short.wav"&&sampleBrowser.fileAt(1).getFileName()=="Kick Deep.wav");
    sampleBrowser.setSort(samples::byName,false);stage("sort by name descending",sampleBrowser.fileAt(0).getFileName()=="Kick Short.wav");sampleBrowser.setSort(samples::byName,true);
    sampleBrowser.setSearch("deep");stage("search",sampleBrowser.visibleRows()==1&&sampleBrowser.fileAt(0).getFileName()=="Kick Deep.wav");
    sampleBrowser.selectRow(0);juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
    stage("file info",sampleBrowser.infoText().contains("0:00.50")&&sampleBrowser.infoText().contains("WAV")&&sampleBrowser.infoText().contains("44.1 kHz")&&!sampleBrowser.infoIsError());
    sampleBrowser.preview(sampleBrowser.fileAt(0));for(int i=0;i<100&&!audio.isPreviewing();++i)juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    stage("preview starts",audio.isPreviewing());sampleBrowser.stopPreview();stage("preview stops",!audio.isPreviewing());
    // Double-click / Enter / LOAD: into the selected track's sampler, with Undo.
    select(6,selectedSection);const auto undoBefore=undoStack.size();
    stage("import into the selected track",sampleBrowser.loadSelected()&&project.tracks[6].instrument=="sample"&&juce::File(juce::String::fromUTF8(project.tracks[6].instrumentPath.c_str())).getFileName()=="Kick Deep.wav"&&undoStack.size()==undoBefore+1);
    sampleBrowser.setSearch("");
    // Drag onto another track's row in the arrangement.
    arrangementPort.setViewPosition(0,0);const int row=rowForTrack(7);const auto target=getLocalPoint(&arrangement,juce::Point<int>(216+40,arrangement.rowY(row)+arrangement.rowHeight(row)/2));
    const SourceDetails drag(juce::String(SampleBrowser::dragPrefix)+kicks.getChildFile("Kick Short.wav").getFullPathName(),&sampleBrowser,target);
    stage("drag source accepted",isInterestedInDragSource(drag));itemDragMove(drag);stage("drop target highlighted",arrangement.dropTrack==7);
    if(auto out=folder.getChildFile("sample-browser.png").createOutputStream()){juce::PNGImageFormat png;stage("browser screenshot",png.writeImageToStream(createComponentSnapshot(getLocalBounds()),*out));}
    // Other window shapes (the content scales with the width, so only the height changes).
    {const int height=getHeight();for(int h:{760,1100}){setSize(1440,h);const bool fits=sampleBrowser.verifyLayout()&&!sampleBrowser.getBounds().intersects(arrangementPort.getBounds())&&!sampleBrowser.getBounds().intersects(mixer.getBounds())&&sampleBrowser.getBottom()<=getHeight()-31;
        stage(h==760?"browser layout 760":"browser layout 1100",fits);if(auto out=folder.getChildFile("sample-browser-"+juce::String(h)+".png").createOutputStream()){juce::PNGImageFormat png;png.writeImageToStream(createComponentSnapshot(getLocalBounds()),*out);}}
        setSize(1440,height);}
    itemDropped(drag);stage("drop loads the hovered track",project.tracks[7].instrument=="sample"&&selectedTrack==7&&juce::String::fromUTF8(project.tracks[7].instrumentPath.c_str()).endsWith("Kick Short.wav")&&arrangement.dropTrack<0);
    // Unsupported and unavailable.
    sampleBrowser.navigate(library);sampleBrowser.waitForListing(5000);sampleBrowser.selectRow(2);
    stage("unsupported file",!sampleBrowser.loadSelected()&&sampleBrowser.infoIsError()&&project.tracks[(size_t)selectedTrack].instrumentPath.find("Loop.mp3")==std::string::npos);
    stage("history",sampleBrowser.canBack());sampleBrowser.back();sampleBrowser.waitForListing(5000);stage("back",sampleBrowser.currentFolder()==kicks&&sampleBrowser.canForward());
    sampleBrowser.forward();sampleBrowser.waitForListing(5000);stage("forward",sampleBrowser.currentFolder()==library);
    sampleBrowser.navigate(kicks);sampleBrowser.waitForListing(5000);sampleBrowser.up();sampleBrowser.waitForListing(5000);stage("up",sampleBrowser.currentFolder()==kicks.getParentDirectory());
    sampleBrowser.navigate(library.getChildFile("Missing"));sampleBrowser.waitForListing(5000);stage("unavailable folder",sampleBrowser.visibleRows()==0);
    // Saved for the next session.
    sampleBrowser.navigate(kicks);sampleBrowser.waitForListing(5000);
    const auto saved=samples::Preferences::fromJson(Localisation::setting("sampleBrowser"));
    stage("preferences saved",saved.lastFolder==kicks.getFullPathName()&&saved.folders.size()>=1&&saved.isFavourite(library.getFullPathName())&&Localisation::setting("sampleBrowserVisible")=="1");
    sampleBrowser.removeFolder(library.getFullPathName());stage("folder removed",!samples::Preferences::fromJson(Localisation::setting("sampleBrowser")).isFavourite(library.getFullPathName()));
    // Restore everything the check touched.
    sampleBrowser.stopPreview();audio.useInternal(6);audio.useInternal(7);project=savedProject;select(savedTrack,selectedSection);changed();undoStack.clear();redoStack.clear();
    Localisation::setSetting("sampleBrowser",savedSetting);showSampleBrowser(savedVisible);library.deleteRecursively();message(tr("Ready"));
    folder.getChildFile("sample-browser.txt").replaceWithText(ok?"PASS sample browser: saved folders/favourites, folders-first listing, type filter, search, sort by length/name, breadcrumb, back/forward/up, file info, preview start/stop, unsupported and unavailable files, Enter/double-click import with Undo, drag onto a track with highlight, saved preferences":"FAIL sample browser: "+firstFailure);
    return ok;
#endif
}
}
