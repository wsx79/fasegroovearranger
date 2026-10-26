// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "StudioWindow.h"
#include "MainComponent.h"
#include "StudioIO.h"
#include "Localisation.h"
namespace fga {
namespace {
juce::String studioTip(const juce::String& name){
    static const std::pair<const char*,const char*> tips[]{
        {"LOAD SAMPLE","Load an audio file into this track without changing its MIDI."},
        {"OPEN PRESET","Open a sampler preset with its sample and playback settings."},
        {"SAVE PRESET","Save the sample and sampler settings as a portable preset."},
        {"RESET SAMPLE","Restore sampler defaults, keeping the sample file, MIDI and track mix. Undo restores your edits."},
        {"AUTO SLICE","Cut the sample at attacks or into equal pieces and map the slices to consecutive keyboard notes."},
        {"WRITE SLICE MIDI","Replace the selected clip notes with slice triggers in their original timing. Undo restores the clip."},
        {"REVERSE","Play the sample or each slice backwards."},
        {"LOOP","Repeat the loop region while a key is held in gate mode."},
        {"ZERO CROSSING","Snap trim edges and slice markers to nearby zero crossings to reduce clicks."},
        {"TEMPO SYNC","Follow the song tempo using Source BPM, independently of note pitch."},
        {"FILTER","Enable the low-pass filter. Cutoff and resonance shape the sample tone."},
        {"START","Sample trim start, as a fraction of the full file: 0 is the beginning."},
        {"END","Sample trim end, as a fraction of the full file: 1 is the end."},
        {"LOOP START","Beginning of the repeated region inside the sample trim."},
        {"LOOP END","End of the repeated region inside the sample trim."},
        {"CROSSFADE (s)","Blend the loop edges over this many seconds to soften the seam."},
        {"FADE IN (s)","Fade in each sample or slice over this many seconds."},
        {"FADE OUT (s)","Fade out near the end of each sample or slice."},
        {"ROOT NOTE","MIDI note that plays the sample at its original pitch."},
        {"TRANSPOSE","Transpose the sample and all slices by semitones."},
        {"FINE (cents)","Fine-tune pitch in cents. 100 cents equals one semitone."},
        {"VOICES","Maximum simultaneous sample voices. Older voices are replaced at this limit."},
        {"ATTACK (s)","Time for the amplitude envelope to reach full level."},
        {"DECAY (s)","Time for the amplitude envelope to fall to the sustain level."},
        {"SUSTAIN","Level held after the attack and decay stages."},
        {"RELEASE (s)","Fade time after releasing a key in gate mode."},
        {"VELOCITY","How much MIDI velocity changes sample loudness."},
        {"CUTOFF (Hz)","Low-pass cutoff frequency: lower values remove more high frequencies."},
        {"RESONANCE","Emphasize frequencies around the filter cutoff."},
        {"SOURCE BPM","Original sample tempo, used by tempo sync and slice MIDI timing."},
        {"SLICE SENSITIVITY","Higher sensitivity detects more attacks when slicing by transients."},
        {"FIRST SLICE NOTE","MIDI note assigned to the first slice; following slices use consecutive notes."},
        {"COLLECT & SAVE","Copy used samples beside the project and save a portable project with relative paths."},
        {"RELINK SAMPLES","Find missing sample files by content or by a unique matching filename."},
    };
    for(const auto& entry:tips)if(name==entry.first)return entry.second;return name;
}
class StudioTabsTheme : public DawLookAndFeel {
public:
    int getTabButtonBestWidth(juce::TabBarButton& b,int)override{return std::max(116,(int)juce::GlyphArrangement::getStringWidth(Daw::caps(11),b.getButtonText())+32);}
    void drawTabButton(juce::TabBarButton& b,juce::Graphics& g,bool hover,bool)override{auto area=b.getLocalBounds();g.fillAll(b.isFrontTab()?Daw::surface:hover?Daw::surfaceHi:Daw::chrome);g.setFont(Daw::caps(11));g.setColour(b.isFrontTab()?Daw::text:Daw::textDim);g.drawText(b.getButtonText(),area.reduced(8),juce::Justification::centred);if(b.isFrontTab()){g.setColour(Daw::accent);g.fillRect(area.removeFromBottom(2));}}
    void drawTabAreaBehindFrontButton(juce::TabbedButtonBar&,juce::Graphics& g,int w,int h)override{g.setColour(Daw::line);g.drawHorizontalLine(h-1,0,(float)w);}
};
class SamplerSurface : public juce::Component {
public:
    std::vector<std::pair<juce::String,juce::Rectangle<int>>> cards;
    void paint(juce::Graphics& g)override{
        g.fillAll(Daw::base);
        for(const auto& card:cards){auto area=card.second.toFloat();g.setColour(Daw::surface);g.fillRoundedRectangle(area,5);g.setColour(Daw::line);g.drawRoundedRectangle(area.reduced(.5f),5,1);Daw::drawCaps(g,tr(card.first),card.second.reduced(14,8).removeFromTop(18),Daw::textDim);}
    }
};
class StudioKeyboard : public juce::MidiKeyboardComponent,public juce::SettableTooltipClient {
public:
    explicit StudioKeyboard(juce::MidiKeyboardState& state):MidiKeyboardComponent(state,horizontalKeyboard){setColour(keyDownOverlayColourId,Daw::accent.withAlpha(.8f));setColour(mouseOverKeyOverlayColourId,Daw::accent.withAlpha(.22f));setWantsKeyboardFocus(true);}
    std::array<bool,128> playing{};int firstMapped=0,lastMapped=127;
    void mappedRange(int first,int last){if(first!=firstMapped||last!=lastMapped){firstMapped=first;lastMapped=last;repaint();}}
    bool mouseDownOnKey(int note,const juce::MouseEvent&)override{return note>=firstMapped&&note<=lastMapped;}
    bool mouseDraggedToKey(int note,const juce::MouseEvent&)override{return note>=firstMapped&&note<=lastMapped;}
    void showNotes(const std::array<bool,128>& notes){if(notes!=playing){playing=notes;repaint();}}
    void drawWhiteNote(int note,juce::Graphics& g,juce::Rectangle<float> area,bool down,bool over,juce::Colour line,juce::Colour text)override{MidiKeyboardComponent::drawWhiteNote(note,g,area,down||playing[(size_t)note],over,line,text);if(note<firstMapped||note>lastMapped){g.setColour(Daw::base.withAlpha(.7f));g.fillRect(area);}}
    void drawBlackNote(int note,juce::Graphics& g,juce::Rectangle<float> area,bool down,bool over,juce::Colour fill)override{MidiKeyboardComponent::drawBlackNote(note,g,area,down||playing[(size_t)note],over,fill);if(note<firstMapped||note>lastMapped){g.setColour(Daw::base.withAlpha(.5f));g.fillRect(area);}}
};
class StudioMeter : public juce::Component,public juce::SettableTooltipClient {
public:
    void setLevel(float value){if(level!=value){level=value;repaint();}}
    void paint(juce::Graphics& g)override{auto area=getLocalBounds().toFloat().reduced(0,7);auto bar=area.withTrimmedRight(65);g.setColour(Daw::well);g.fillRoundedRectangle(bar,2);const float db=juce::Decibels::gainToDecibels(level,-60.f),amount=std::clamp((db+60)/66,0.f,1.f);g.setColour(level>1?Daw::meterHigh:level>.8?Daw::meterMid:Daw::meterLow);g.fillRoundedRectangle(bar.withWidth(bar.getWidth()*amount),2);g.setColour(Daw::textDim);g.setFont(Daw::font(11));g.drawText(level>.00001f?juce::String(db,1)+" dB":juce::String::fromUTF8("−∞ dB"),getLocalBounds(),juce::Justification::centredRight);}
private:float level=0;
};
class Waveform : public juce::Component,public juce::SettableTooltipClient {
public:
    std::shared_ptr<const SampleAsset> asset;
    SamplerSettings settings;
    double zoom=1,scroll=0;
    std::array<bool,128> playing{};
    std::function<void(double,double)> onRegion;
    std::function<void()> onGesture,onView;
    std::function<void(double,bool)> onMarker;
    void paint(juce::Graphics& g)override{
        g.fillAll(Daw::well);
        if(!asset||asset->peaks.empty()){g.setColour(Daw::textDim);g.setFont(Daw::font(13));g.drawText(tr("Load a sample to edit its waveform."),getLocalBounds(),juce::Justification::centred);return;}
        auto x=[&](double p){return float((p-scroll)*zoom*getWidth());};
        const int top=26,bottom=getHeight()-18,height=bottom-top;const float centre=(top+bottom)*.5f;
        g.setColour(Daw::chrome);g.fillRect(0,0,getWidth(),top);
        g.setColour(Daw::accent.withAlpha(.10f));g.fillRect(x(settings.start),(float)top,x(settings.end)-x(settings.start),(float)height);
        for(int i=0;i<=8;++i){const int px=i*getWidth()/8;const double sec=(scroll+(double)i/(8*zoom))*asset->seconds();g.setColour(Daw::line);g.drawVerticalLine(px,(float)top,(float)bottom);g.setColour(Daw::textDim);g.setFont(Daw::font(10));g.drawText(juce::String(sec,sec<10?2:1)+" s",px+5,3,80,18,juce::Justification::centredLeft);}
        g.setColour(Daw::lineStrong);g.drawHorizontalLine((int)centre,0,(float)getWidth());
        if(settings.mode==2)for(int i=0;i<settings.sliceCount;++i)if(playing[(size_t)(settings.sliceBase+i)]){g.setColour(Daw::accent.withAlpha(.25f));const double end=i+1<settings.sliceCount?settings.slices[(size_t)i+1]:settings.end;g.fillRect(x(settings.slices[(size_t)i]),(float)top,x(end)-x(settings.slices[(size_t)i]),(float)height);}
        g.setColour(Daw::accent);for(int pixel=0;pixel<getWidth();++pixel){const size_t i=std::min(asset->peaks.size()-1,(size_t)(std::clamp(scroll+(double)pixel/(zoom*std::max(1,getWidth())),0.,1.)*(asset->peaks.size()-1)));const auto peak=asset->peaks[i];g.drawVerticalLine(pixel,centre-.44f*height*peak[1],centre-.44f*height*peak[0]);}
        if(settings.loop){g.setColour(Daw::solo.withAlpha(.8f));g.drawRect(x(settings.loopStart),(float)top+5,x(settings.loopEnd)-x(settings.loopStart),(float)height-10,1.5f);}
        g.setFont(Daw::font(10,true));for(int i=0;i<settings.sliceCount;++i){const float px=x(settings.slices[(size_t)i]);g.setColour(Daw::automation);g.drawVerticalLine((int)px,(float)top,(float)bottom);if(settings.sliceCount/zoom<32)g.drawText(juce::String(i+1),(int)px+4,top+4,30,14,juce::Justification::centredLeft);}
        for(double point:{settings.start,settings.end}){const float px=x(point);g.setColour(Daw::text);g.drawVerticalLine((int)px,(float)top,(float)bottom);g.fillRect(px-3,(float)top,6.f,6.f);}
        g.setColour(Daw::textDim);g.setFont(Daw::font(10));g.drawText(asset->file.getFileName()+"  ·  "+juce::String(asset->seconds(),2)+" s  ·  "+juce::String(asset->rate/1000.,1)+" kHz",8,bottom,getWidth()-16,18,juce::Justification::centredLeft);
    }
    void mouseDown(const juce::MouseEvent& e)override{if(!asset)return;anchor=position(e.x);if((e.mods.isShiftDown()||e.mods.isRightButtonDown())&&onMarker){onMarker(anchor,e.mods.isRightButtonDown());return;}trimming=false;dragEnd=std::abs(anchor-settings.end)<std::abs(anchor-settings.start);}
    void mouseDrag(const juce::MouseEvent& e)override{if(!asset||!onRegion||e.mods.isShiftDown()||e.mods.isRightButtonDown()||!e.mouseWasDraggedSinceMouseDown())return;if(!trimming){trimming=true;if(onGesture)onGesture();}double p=position(e.x);if(settings.snap)p=snapSamplePoint(*asset,p);onRegion(dragEnd?settings.start:p,dragEnd?p:settings.end);}
    void zoomAt(double factor,float pointer){const double fraction=std::clamp((double)pointer/std::max(1,getWidth()),0.,1.),anchor=scroll+fraction/zoom;zoom=std::clamp(zoom*factor,1.,32.);scroll=std::clamp(anchor-fraction/zoom,0.,1.-1./zoom);if(onView)onView();repaint();}
    void mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel)override{
        if(!asset)return;
        if(e.mods.isShiftDown()||std::abs(wheel.deltaX)>std::abs(wheel.deltaY)){scroll=std::clamp(scroll-(wheel.deltaX+wheel.deltaY)*.35/zoom,0.,1.-1./zoom);if(onView)onView();repaint();}
        else zoomAt(std::exp(std::clamp(wheel.deltaY*2.5f,-1.f,1.f)),e.position.x);
    }
    void mouseMagnify(const juce::MouseEvent& e,float scale)override{if(asset&&std::isfinite(scale)&&scale>0)zoomAt(scale,e.position.x);}
    void mouseDoubleClick(const juce::MouseEvent&)override{zoom=1;scroll=0;if(onView)onView();repaint();}
private:
    double anchor=0;bool dragEnd=false,trimming=false;
    double position(int x)const{return std::clamp(scroll+(double)x/(zoom*std::max(1,getWidth())),0.,1.);}
};
struct MediaTask : juce::ThreadWithProgressWindow {
    Project source,result;juce::File destination;bool collect;RelinkResult relink;
    juce::Result status=juce::Result::ok();std::function<void(juce::Result,Project,RelinkResult)> done;
    MediaTask(Project p,juce::File f,bool c):ThreadWithProgressWindow(tr(c?"COLLECT & SAVE":"RELINK SAMPLES"),true,true),source(std::move(p)),destination(f),collect(c){}
    void run()override{auto cancel=[this]{return threadShouldExit();};result=source;status=collect?collectProjectMedia(source,destination,result,cancel,[this](double p){setProgress(p);}):relinkProjectMedia(result,destination,relink,cancel);}
    void threadComplete(bool cancelled)override{if(done)done(cancelled?juce::Result::fail("Cancelled"):status,std::move(result),relink);}
};
}
class StudioWindow::Panel : public juce::Component,private juce::Timer,private juce::MidiKeyboardState::Listener {
public:
    explicit Panel(MainComponent& o):owner(o),tabs(juce::TabbedButtonBar::TabsAtTop),keyboard(keys),sampleKeyboard(keys){
        addAndMakeVisible(target);addAndMakeVisible(tabs);tabs.setLookAndFeel(&tabTheme);tabs.setOutline(0);tabs.setTabBarDepth(36);
        samplerTab.addAndMakeVisible(samplerPort);samplerTab.addAndMakeVisible(performance);samplerPort.setViewedComponent(&sampler,false);tabs.addTab("SAMPLER",Daw::surface,&samplerTab,false);tabs.addTab("MIDI REC",Daw::surface,&record,false);tabs.addTab("BUS & SEND",Daw::surface,&buses,false);tabs.addTab("MEDIA",Daw::surface,&media,false);
        target.onChange=[this]{if(owner.audio.isRecording())finish();releasePads();keys.allNotesOff(0);track=target.getSelectedId()-1;owner.select(track,owner.selectedSection);owner.audio.setMidiMonitor(track,monitor.getToggleState());refresh();};
        performance.addAndMakeVisible(sampleKeyboard);performance.addAndMakeVisible(sampleNote);performance.addAndMakeVisible(sampleInput);performance.addAndMakeVisible(sampleMonitor);sampler.addAndMakeVisible(wave);Localisation::bindTip(wave,"Wheel or pinch to zoom at the pointer; swipe horizontally or Shift-wheel to scroll. Double-click fits the sample. Drag trims; Shift-click adds a slice, right-click removes one.");wave.onMarker=[this](double point,bool remove){if(!wave.asset)return;owner.checkpoint();auto& settings=selected().sampler;if(remove&&settings.sliceCount>0){auto first=settings.slices.begin(),last=first+settings.sliceCount;auto it=std::min_element(first,last,[point](double a,double b){return std::abs(a-point)<std::abs(b-point);});std::move(it+1,last,it);--settings.sliceCount;}else if(!remove&&settings.sliceCount<128-settings.sliceBase){settings.slices[(size_t)settings.sliceCount++]=settings.snap?snapSamplePoint(*wave.asset,point):point;}settings.mode=2;commit();};wave.onGesture=[this]{owner.checkpoint();};wave.onRegion=[this](double a,double b){auto& s=selected().sampler;s.start=std::min(a,b-.000001);s.end=std::max(b,a+.000001);s.normalise();commit();};
        button(sampler,"LOAD SAMPLE",[this]{choose("LOAD SAMPLE","*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3",false,[this](juce::File f){owner.loadSampleFile(track,f);refresh();});});
        button(sampler,"OPEN PRESET",[this]{choose("OPEN PRESET","*.fgs",false,[this](juce::File f){SamplerPreset p;auto r=loadSamplerPreset(f,p);if(r.wasOk())r=owner.audio.loadSample(track,p.file,p.root,p.settings);if(r.failed()){owner.message(r.getErrorMessage(),true);return;}owner.checkpoint();auto& t=selected();t.instrument="sample";t.instrumentPath=p.file.getFullPathName().toStdString();t.sampleHash=p.hash.toStdString();t.sampleRoot=p.root;t.sampler=p.settings;t.instrumentId.clear();t.instrumentState.clear();commit();});});
        button(sampler,"SAVE PRESET",[this]{if(!owner.audio.sampleAsset(track))return;choose("SAVE PRESET","*.fgs",true,[this](juce::File f){SamplerPreset p{selected().sampler,selected().sampleRoot,sampleFile(selected()),juce::String(selected().sampleHash)};auto r=saveSamplerPreset(p,f.withFileExtension("fgs"));owner.message(r.wasOk()?"Preset saved":r.getErrorMessage(),r.failed());});});
        button(sampler,"RESET SAMPLE",[this]{resetSample();});
        sampler.addAndMakeVisible(sliceMethod);const juce::StringArray sliceMethods{"Transients","4 equal slices","8 equal slices","16 equal slices","32 equal slices","64 equal slices"};sliceMethod.addItemList(sliceMethods,1);Localisation::bindItems(sliceMethod,sliceMethods);sliceMethod.setSelectedId(1,juce::dontSendNotification);sliceMethod.onChange=[this]{refresh();};
        mode.addItemList({"Classic / gate","One-shot","Slices"},1);sampler.addAndMakeVisible(mode);mode.onChange=[this]{if(updating)return;owner.checkpoint();selected().sampler.mode=mode.getSelectedId()-1;if(mode.getSelectedId()==3&&selected().sampler.sliceCount==0){sliceSample(false);return;}commit();};
        toggle(sampler,"REVERSE",&SamplerSettings::reverse);toggle(sampler,"LOOP",&SamplerSettings::loop);toggle(sampler,"ZERO CROSSING",&SamplerSettings::snap);toggle(sampler,"TEMPO SYNC",&SamplerSettings::sync);toggle(sampler,"FILTER",&SamplerSettings::filter);
        scalar("START",&SamplerSettings::start,0,1,.00001);scalar("END",&SamplerSettings::end,.00001,1,.00001);scalar("LOOP START",&SamplerSettings::loopStart,0,1,.00001);scalar("LOOP END",&SamplerSettings::loopEnd,.00001,1,.00001);
        scalar("CROSSFADE (s)",&SamplerSettings::crossfade,0,.25,.001);scalar("FADE IN (s)",&SamplerSettings::fadeIn,0,2,.001);scalar("FADE OUT (s)",&SamplerSettings::fadeOut,0,2,.001);
        integer("ROOT NOTE",nullptr,0,127);integer("TRANSPOSE",&SamplerSettings::transpose,-48,48);scalar("FINE (cents)",&SamplerSettings::fine,-100,100,.1);integer("VOICES",&SamplerSettings::polyphony,1,32);
        scalar("ATTACK (s)",&SamplerSettings::attack,0,10,.001);scalar("DECAY (s)",&SamplerSettings::decay,0,10,.001);scalar("SUSTAIN",&SamplerSettings::sustain,0,1,.01);scalar("RELEASE (s)",&SamplerSettings::release,.001,20,.001);
        scalar("VELOCITY",&SamplerSettings::velocity,0,1,.01);scalar("CUTOFF (Hz)",&SamplerSettings::cutoff,20,20000,1);scalar("RESONANCE",&SamplerSettings::resonance,0,1,.01);scalar("SOURCE BPM",&SamplerSettings::sourceBpm,30,300,.1);scalar("SLICE SENSITIVITY",&SamplerSettings::sensitivity,0,1,.01);integer("FIRST SLICE NOTE",&SamplerSettings::sliceBase,0,127);
        button(sampler,"AUTO SLICE",[this]{sliceSample(true);});samplerButtons.back()->setColour(juce::TextButton::buttonColourId,Daw::accent);
        button(sampler,"WRITE SLICE MIDI",[this]{auto asset=owner.audio.sampleAsset(track);if(!asset||selected().locked)return;auto& s=selected().sampler;if(!s.sliceCount)return;owner.select(track,owner.selectedSection);owner.ensureTimelineClip();if(owner.clip().locked){owner.message(tr("Unlock the clip before writing slice MIDI."),true);return;}owner.checkpoint();auto& c=owner.clip();c.notes.clear();c.loopSource.clear();c.loopLength=0;s.mode=2;for(int i=0;i<s.sliceCount;++i){double at=(s.slices[(size_t)i]-s.start)*asset->seconds()*s.sourceBpm/60.;if(at>=owner.clipLength())break;c.notes.push_back({at,std::min(.125,owner.clipLength()-at),s.sliceBase+i,100,0});}owner.changed(true,track);owner.piano.fitNotes();owner.updateSizes();refresh();});
        sampler.addAndMakeVisible(zoomCaption);sampler.addAndMakeVisible(scrollCaption);Localisation::bindLabel(zoomCaption,"ZOOM");Localisation::bindLabel(scrollCaption,"POSITION");zoomCaption.setFont(Daw::font(10));scrollCaption.setFont(Daw::font(10));
        sampler.addAndMakeVisible(zoom);zoom.setRange(1,32,.01);zoom.setValue(1);zoom.onValueChange=[this]{if(updating)return;wave.zoomAt(zoom.getValue()/wave.zoom,wave.getWidth()*.5f);};
        sampler.addAndMakeVisible(scroll);scroll.setRange(0,1,.0001);scroll.onValueChange=[this]{wave.scroll=scroll.getValue()*(1.-1./wave.zoom);wave.repaint();};
        for(auto* slider:{&zoom,&scroll}){slider->setSliderStyle(juce::Slider::LinearHorizontal);slider->setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);}
        wave.onView=[this]{zoom.setValue(wave.zoom,juce::dontSendNotification);scroll.setValue(wave.zoom>1?wave.scroll/(1.-1./wave.zoom):0,juce::dontSendNotification);scroll.setEnabled(wave.asset&&wave.zoom>1);};
        Localisation::bindTip(zoom,"Zoom the waveform. The mouse wheel and trackpad pinch zoom at the pointer.");Localisation::bindTip(scroll,"Move through the zoomed waveform without changing the trim points.");
        performance.addAndMakeVisible(sliceBank);sliceBank.onChange=[this]{releasePads();refresh();};
        for(int i=0;i<16;++i){auto* pad=control<juce::TextButton>(performance);pads[(size_t)i]=pad;pad->onStateChange=[this,pad,i]{const bool down=pad->isDown()&&pad->isEnabled()&&pad->isVisible();if(down==padHeld[(size_t)i])return;padHeld[(size_t)i]=down;if(down){padTracks[(size_t)i]=track;padNotes[(size_t)i]=selected().sampler.sliceBase+std::max(0,sliceBank.getSelectedId()-1)*16+i;}owner.audio.liveMidi(padTracks[(size_t)i],down?juce::MidiMessage::noteOn(1,padNotes[(size_t)i],.85f):juce::MidiMessage::noteOff(1,padNotes[(size_t)i]));};}
        setupRecording();setupBuses();
        Localisation::bindText(sampleMonitor,"MIDI INPUT");sampleMonitor.onClick=[this]{monitor.setToggleState(sampleMonitor.getToggleState(),juce::dontSendNotification);owner.audio.setMidiMonitor(track,monitor.getToggleState());};
        sampleInput.onChange=[this]{device.setSelectedId(sampleInput.getSelectedId(),juce::sendNotificationSync);};
        sampleNote.setFont(Daw::font(13,true));sampleNote.setColour(juce::Label::textColourId,Daw::accent);sampleNote.setJustificationType(juce::Justification::centredRight);
        Localisation::bindTip(sampleInput,"Choose the MIDI keyboard that plays this sample.");Localisation::bindTip(sampleMonitor,"Enable the selected MIDI input for this sample. The on-screen keyboard always works.");
        Localisation::bindTip(target,"Choose a sample track. Recording and bus pages also accept instrument and MIDI tracks.");Localisation::bindTip(mode,"Gate follows key length; one-shot plays to the end; slices maps each detected piece to a separate key.");Localisation::bindTip(sliceMethod,"Detect attacks or divide the trimmed region into equal slices, then press Auto slice.");Localisation::bindTip(sliceBank,"Choose the bank of sixteen slice pads. The keyboard can play every slice.");
        Localisation::bindTip(sampleKeyboard,"Play the sample with the mouse or computer keys A W S E D F T G Y H U J K. In Slices, consecutive notes trigger consecutive pieces.");Localisation::bindTip(keyboard,"Play and record MIDI with the mouse or computer keys A W S E D F T G Y H U J K.");Localisation::bindTip(sampleNote,"Notes currently sounding, including one-shot sample tails and MIDI playback.");

        button(media,"COLLECT & SAVE",[this]{choose("COLLECT & SAVE","*.fga",true,[this](juce::File f){runMedia(f.withFileExtension("fga"),true);});});
        button(media,"RELINK SAMPLES",[this]{choose("RELINK SAMPLES","*",false,[this](juce::File f){runMedia(f,false);},true);});
        media.addAndMakeVisible(mediaInfo);Localisation::bindLabel(mediaInfo,"Collect copies samples beside the project and saves relative paths.\nRelink searches by content hash, or by a unique filename for older projects.\nOriginal samples are preserved.",false);
        Localisation::bind(tabs,[this]{const char* names[]{"SAMPLER","MIDI REC","BUS & SEND","MEDIA"};for(int i=0;i<4;++i)tabs.setTabName(i,tr(names[i]));});
        Localisation::bindItems(mode,{"Classic / gate","One-shot","Slices"});Localisation::bindItems(countIn,{"No count-in","1 bar count-in","2 bars count-in"});Localisation::bindItems(quantise,{"Quantise off","1/4 notes","1/8 notes","1/16 notes","1/32 notes"});Localisation::bindText(stopButton,"STOP");Localisation::bindText(monitor,"MONITOR");Localisation::bindText(overdub,"OVERDUB");
        if(Localisation::tipsEnabled())tooltips=std::make_unique<juce::TooltipWindow>(this,500);keys.addListener(this);startTimerHz(15);refresh();restoreRecordingSettings();
    }
    ~Panel()override{tabs.setLookAndFeel(nullptr);stopTimer();saveRecordingSettings();finish();releasePads();owner.setEnabled(true);owner.audio.setMidiMonitor(track,false);keys.removeListener(this);if(task)task->stopThread(10000);}
    bool verify(const juce::File& folder){
        auto before=owner.project;auto undos=owner.undoStack,redos=owner.redoStack;const int selectedTrack=owner.selectedTrack;const bool wasDirty=owner.dirty;owner.audio.stop();
        folder.createDirectory();auto file=folder.getChildFile("Percussion-loop.wav");
        juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(file.createOutputStream().release(),48000,1,16,{},0));if(!writer)return false;
        juce::AudioBuffer<float> source(1,48000);for(int i=0;i<48000;++i)source.setSample(0,i,.35f*std::sin((float)(i*.0576))*(i%12000<9000?1.f:.03f));writer->writeFromAudioSampleBuffer(source,0,48000);writer.reset();
        tabs.setCurrentTabIndex(1);const bool previousMonitor=monitor.getToggleState();track=maxTracks-1;owner.selectedTrack=4;monitor.setToggleState(true,juce::dontSendNotification);owner.audio.setMidiMonitor(track,true);refresh();owner.audio.liveMidi(-1,juce::MidiMessage::noteOn(1,36,.9f),false);juce::AudioBuffer<float> input(2,512);juce::AudioIODeviceCallbackContext context;owner.audio.audioDeviceIOCallbackWithContext(nullptr,0,input.getArrayOfWritePointers(),2,512,context);const bool targetOk=track==4&&owner.audio.level(4)>.00001f;owner.audio.liveMidi(-1,juce::MidiMessage::noteOff(1,36),false);owner.audio.setMidiMonitor(4,false);monitor.setToggleState(previousMonitor,juce::dontSendNotification);owner.audio.stop();
        track=0;owner.selectedTrack=0;owner.loadSampleFile(0,file);tabs.setCurrentTabIndex(0);refresh();
        bool ok=targetOk&&wave.asset&&selected().instrument=="sample";auto old=selected().sampler;fields[0].slider->setValue(.125,juce::sendNotificationSync);ok=ok&&selected().sampler.start==.125;owner.undo(false);refresh();ok=ok&&selected().sampler==old;
        bool detailsOk=target.getNumItems()==1&&target.getItemId(0)==1;juce::String firstFailure;auto stage=[&](const char* name){if(!detailsOk&&firstFailure.isEmpty())firstFailure=name;};stage("sample target list");
        owner.trackInspector.refresh();auto* inspectorSampler=dynamic_cast<juce::TextButton*>(owner.trackInspector.findChildWithID("inspector-sampler"));detailsOk=detailsOk&&inspectorSampler&&inspectorSampler->isVisible();if(inspectorSampler)inspectorSampler->onClick();stage("inspector entry");
        const auto mouse=juce::Desktop::getInstance().getMainMouseSource();const auto time=juce::Time::getCurrentTime();const juce::Point<float> point(wave.getWidth()*.5f,70);
        juce::MouseEvent event(mouse,point,{},1,0,0,0,0,&wave,&wave,time,point,time,1,false);juce::MouseWheelDetails wheel{};wheel.deltaY=.2f;
        wave.mouseWheelMove(event,wheel);const double zoomed=wave.zoom,anchored=wave.scroll+.5/wave.zoom;detailsOk=detailsOk&&zoomed>1&&std::abs(anchored-.5)<1e-6;stage("wheel anchor");
        wave.mouseMagnify(event,1.5f);detailsOk=detailsOk&&wave.zoom>zoomed&&std::abs(wave.scroll+.5/wave.zoom-.5)<1e-6;stage("pinch anchor");
        wheel.deltaX=-.15f;wheel.deltaY=0;const double beforePan=wave.scroll;wave.mouseWheelMove(event,wheel);detailsOk=detailsOk&&wave.scroll>beforePan;stage("horizontal pan");
        const auto beforeFit=selected().sampler;const auto fitUndo=owner.undoStack.size();wave.mouseDown(event);wave.mouseDown(event);wave.mouseDoubleClick(event);detailsOk=selected().sampler==beforeFit&&owner.undoStack.size()==fitUndo&&detailsOk&&wave.zoom==1&&wave.scroll==0&&zoom.getValue()==1;stage("double-click fit");
        sliceMethod.setSelectedId(4,juce::dontSendNotification);samplerButtons[4]->onClick();detailsOk=detailsOk&&selected().sampler.mode==2&&selected().sampler.sliceCount==16&&sampleKeyboard.isVisible()&&sampleKeyboard.getHeight()>50&&pads[0]->isVisible();stage("equal slices and keyboard");
        owner.audio.liveMidi(0,juce::MidiMessage::noteOn(1,36,.8f));owner.audio.audioDeviceIOCallbackWithContext(nullptr,0,input.getArrayOfWritePointers(),2,512,context);updateNotes();detailsOk=detailsOk&&sampleKeyboard.playing[36]&&sampleNote.getText().contains(juce::MidiMessage::getMidiNoteName(36,true,true,3));stage("sounding-note display");
        const auto slicedSettings=selected().sampler;const auto sourcePath=selected().instrumentPath;const auto midi=selected().clips;const auto undoCount=owner.undoStack.size();samplerButtons[3]->onClick();detailsOk=detailsOk&&selected().sampler.mode==0&&selected().sampler.sliceCount==0&&selected().sampler.polyphony==8&&selected().sampler.start==0&&selected().instrumentPath==sourcePath&&selected().clips==midi&&owner.undoStack.size()==undoCount+1;stage("default reset and data preservation");
        owner.undo(false);refresh();detailsOk=detailsOk&&selected().sampler==slicedSettings;stage("reset undo");
        auto points=detectSampleSlices(*wave.asset,0,1,.7);sliceMethod.setSelectedId(1,juce::dontSendNotification);samplerButtons[4]->onClick();detailsOk=detailsOk&&selected().sampler.sliceCount==(int)points.size();stage("transient detection");
        sliceMethod.setSelectedId(4,juce::dontSendNotification);samplerButtons[4]->onClick();
        bool tipsOk=(!Localisation::tipsEnabled()||tooltips!=nullptr)&&!sampleKeyboard.getTooltip().isEmpty();if(tooltips){tooltips->displayTip(samplerButtons[4]->getScreenBounds().getCentre(),samplerButtons[4]->getTooltip());tipsOk=tipsOk&&tooltips->isShowing()&&tooltips->getParentComponent()==this;tooltips->hideTip();}for(auto* flag:flags)tipsOk=tipsOk&&!flag->getTooltip().isEmpty();for(auto& field:fields)tipsOk=tipsOk&&field.slider->getTooltip().length()>field.caption->getText().length();for(auto* button:samplerButtons)tipsOk=tipsOk&&!button->getTooltip().isEmpty();
        owner.select(1,owner.selectedSection);owner.trackInspector.refresh();detailsOk=detailsOk&&inspectorSampler&&!inspectorSampler->isVisible();showTab(1);detailsOk=detailsOk&&target.getNumItems()==(int)owner.project.tracks.size();const auto recordingTarget=owner.audio.startRecording(1,0);showTab(0);detailsOk=detailsOk&&recordingTarget.wasOk()&&track==1;stage("recording destination");if(owner.audio.isRecording())owner.audio.finishRecording();owner.audio.stop();owner.select(0,owner.selectedSection);showTab(0);
        const juce::Point<int> normalSize(getWidth(),getHeight());setSize(940,630);resized();bool layoutOk=samplerTab.getLocalBounds().contains(performance.getBounds())&&sampleKeyboard.getHeight()>50;for(auto* button:samplerButtons)layoutOk=layoutOk&&button->getWidth()>60&&sampler.getLocalBounds().contains(button->getBounds());setSize(normalSize.x,normalSize.y);resized();
        folder.getChildFile("studio-sampler-controls.txt").replaceWithText(detailsOk&&tipsOk&&layoutOk?"PASS inspector sampler entry; sample-only target IDs; wheel/pinch anchor and pan; transient/equal auto slicing and playable keyboard; sounding-note display; default reset preserves file/MIDI and supports Undo; detached tooltips; compact layout":"FAIL sampler controls: actions="+juce::String((int)detailsOk)+" tips="+juce::String((int)tipsOk)+" layout="+juce::String((int)layoutOk)+" first="+firstFailure);ok=ok&&detailsOk&&tipsOk&&layoutOk;
        auto snapshot=[&](int tab,const char* name){showTab(tab);resized();auto image=createComponentSnapshot(getLocalBounds());auto out=folder.getChildFile(name).createOutputStream();return out&&juce::PNGImageFormat().writeImageToStream(image,*out);};
        owner.audio.liveMidi(0,juce::MidiMessage::noteOn(1,37,.8f));owner.audio.audioDeviceIOCallbackWithContext(nullptr,0,input.getArrayOfWritePointers(),2,512,context);updateNotes();
        ok=snapshot(0,"studio-sampler.png")&&snapshot(1,"studio-recording.png")&&snapshot(2,"studio-buses.png")&&ok;
        owner.audio.stop();owner.dirty=false;const auto takeUndo=owner.undoStack.size();bool recorded=owner.audio.startRecording(0,0).wasOk();owner.audio.liveMidi(0,juce::MidiMessage::noteOn(1,37,.8f));for(int n=0;n<20;++n)owner.audio.audioDeviceIOCallbackWithContext(nullptr,0,input.getArrayOfWritePointers(),2,512,context);owner.audio.liveMidi(0,juce::MidiMessage::noteOff(1,37));owner.audio.audioDeviceIOCallbackWithContext(nullptr,0,input.getArrayOfWritePointers(),2,512,context);owner.studioWindow->closeButtonPressed();recorded=recorded&&!owner.audio.isRecording()&&owner.dirty&&owner.undoStack.size()==takeUndo+1&&std::any_of(owner.project.tracks[0].clips[0].notes.begin(),owner.project.tracks[0].clips[0].notes.end(),[](const Note& note){return note.pitch==37;});owner.audio.stop();owner.dirty=false;recorded=owner.audio.startRecording(0,0).wasOk()&&recorded;const bool closing=owner.requestClose();recorded=closing&&!owner.audio.isRecording()&&recorded;if(owner.audio.isRecording())owner.audio.finishRecording();owner.audio.stop();ok=recorded&&ok;
        owner.loadSampleFile(4,file);showTab(0);refresh();ok=target.getNumItems()==2&&target.getItemId(0)==1&&target.getItemId(1)==5&&ok;auto& drum=owner.project.tracks[4];drum.sampler.mode=2;drum.sampler.sliceCount=2;drum.sampler.slices[0]=0;drum.sampler.slices[1]=.5;drum.clips[0].notes={{0,.25,36,100,0},{.5,.25,37,100,0}};owner.changed(true,4);owner.select(4,0);owner.showEditor();owner.piano.fitNotes();owner.updateSizes();ok=!owner.piano.laneView()&&!owner.piano.ownerTrackIsDrum()&&drum.clips[0].notes[1].pitch==37&&ok;
        showTab(0);track=4;refresh();const auto drumMidi=owner.project.tracks[4].clips;samplerButtons[3]->onClick();ok=owner.project.tracks[4].clips==drumMidi&&owner.project.tracks[4].sampler.mode==1&&ok;owner.undo(false);refresh();ok=owner.project.tracks[4].clips==drumMidi&&owner.project.tracks[4].sampler.mode==2&&ok;
        owner.project=std::move(before);owner.undoStack=std::move(undos);owner.redoStack=std::move(redos);owner.selectedTrack=selectedTrack;owner.dirty=wasDirty;owner.restoreInstruments();owner.refreshControls();owner.showArrangement();track=selectedTrack;refresh();
        folder.getChildFile("studio.txt").replaceWithText(ok?"PASS native Studio panels, sampler load, trim and undo":"FAIL Studio panels");return ok;
    }
    void endSession(){finish();releasePads();keys.allNotesOff(0);owner.setEnabled(!busy);}
    void showTab(int n){if(!owner.audio.isRecording())track=std::clamp(owner.selectedTrack,0,(int)owner.project.tracks.size()-1);tabs.setCurrentTabIndex(std::clamp(n,0,3));resized();refresh();if(!owner.audio.isRecording())owner.audio.setMidiMonitor(track,monitor.getToggleState());}
    void paint(juce::Graphics& g)override{g.fillAll(Daw::base);g.setColour(Daw::accent);g.setFont(Daw::font(18,true));g.drawText("STUDIO",18,16,160,30,juce::Justification::centredLeft);}
    void resized()override{
        auto r=getLocalBounds().reduced(16);target.setBounds(r.removeFromTop(30).withTrimmedLeft(180));r.removeFromTop(10);tabs.setBounds(r);
        auto tab=samplerTab.getLocalBounds();const bool sliced=track>=0&&track<(int)owner.project.tracks.size()&&selected().sampler.mode==2&&selected().sampler.sliceCount>0;
        const int padCount=sliced?std::min(16,std::max(0,selected().sampler.sliceCount-std::max(0,sliceBank.getSelectedId()-1)*16)):0;const int padColumns=std::max(1,std::min(8,padCount)),padRows=(padCount+padColumns-1)/padColumns;
        performance.setBounds(tab.removeFromBottom(124+padRows*36).reduced(10,4));samplerPort.setBounds(tab);
        sampler.setSize(std::max(880,samplerPort.getWidth()-16),std::max(594,samplerPort.getHeight()));
        auto s=sampler.getLocalBounds().reduced(12);auto toolbar=s.removeFromTop(32);
        for(int i=0;i<4;++i)samplerButtons[(size_t)i]->setBounds(toolbar.removeFromLeft((sampler.getWidth()-24)/4).reduced(3,1));
        s.removeFromTop(8);wave.setBounds(s.removeFromTop(134+sampler.getHeight()-594));auto navigation=s.removeFromTop(28);auto zoomArea=navigation.removeFromLeft(navigation.getWidth()/2);zoomCaption.setBounds(zoomArea.removeFromLeft(55));scrollCaption.setBounds(navigation.removeFromLeft(75));zoom.setBounds(zoomArea.reduced(5,2));scroll.setBounds(navigation.reduced(5,2));
        auto row=s.removeFromTop(60);mode.setBounds(row.removeFromLeft(180).withSizeKeepingCentre(174,30));const int flagWidth=row.getWidth()/3;for(int i=0;i<5;++i)flags[(size_t)i]->setBounds(row.getX()+(i%3)*flagWidth,row.getY()+(i/3)*30,flagWidth,30);s.removeFromTop(8);
        sampler.cards.clear();const int cardWidth=(s.getWidth()-16)/3;
        const char* titles[]{"REGION & LOOP","PITCH & ENVELOPE","FILTER & SLICES"};const int first[]{0,7,15},last[]{7,15,21};
        for(int col=0;col<3;++col){auto card=s.removeFromLeft(cardWidth);sampler.cards.push_back({titles[col],card});auto content=card.reduced(12,8);content.removeFromTop(25);
            for(int i=first[col];i<last[col];++i){auto cell=content.removeFromTop(col==2?29:32);fields[(size_t)i].caption->setBounds(cell.removeFromLeft(std::min(116,cell.getWidth()/2)));fields[(size_t)i].slider->setBounds(cell.reduced(1,2));}
            if(col==2){sliceMethod.setBounds(content.removeFromTop(27).reduced(0,2));samplerButtons[4]->setBounds(content.removeFromTop(29).reduced(0,2));samplerButtons[5]->setBounds(content.removeFromTop(29).reduced(0,2));}s.removeFromLeft(8);
        }
        auto foot=performance.getLocalBounds();auto info=foot.removeFromTop(30);sampleNote.setBounds(info.removeFromRight(std::min(300,info.getWidth()/3)));sampleInput.setBounds(info.removeFromLeft(220).reduced(0,2));sampleMonitor.setBounds(info.removeFromLeft(140));sliceBank.setBounds(info.reduced(4,2));
        if(sliced){auto padsArea=foot.removeFromTop(padRows*36);for(int i=0;i<16;++i)pads[(size_t)i]->setBounds(padsArea.getX()+(i%padColumns)*padsArea.getWidth()/padColumns,padsArea.getY()+(i/padColumns)*36,padsArea.getWidth()/padColumns-5,31);}
        sampleKeyboard.setBounds(foot.reduced(0,4));sampler.repaint();
        auto rec=record.getLocalBounds().reduced(12);record.cards={{"MIDI RECORDING",rec.withHeight(226)},{"PLAY & RECORD",rec.withTrimmedTop(238).withHeight(160)}};rec.reduce(12,8);rec.removeFromTop(28);
        auto inputRow=rec.removeFromTop(30);refreshInput.setBounds(inputRow.removeFromRight(110).reduced(2,0));device.setBounds(inputRow);rec.removeFromTop(12);auto rr=rec.removeFromTop(30);monitor.setBounds(rr.removeFromLeft(150));countIn.setBounds(rr.removeFromLeft(150));quantise.setBounds(rr.removeFromLeft(170));overdub.setBounds(rr.removeFromLeft(150));rec.removeFromTop(10);rr=rec.removeFromTop(36);recordButton.setBounds(rr.removeFromLeft(170));stopButton.setBounds(rr.removeFromLeft(100));offset.setBounds(rr.removeFromLeft(220));recordStatus.setBounds(rec.removeFromTop(52));keyboard.setBounds(record.cards[1].second.reduced(12,8).withTrimmedTop(28));
        auto br=buses.getLocalBounds().reduced(12);buses.cards.clear();auto routing=br.removeFromTop(132);buses.cards.push_back({"TRACK ROUTING",routing});routing.reduce(12,8);routing.removeFromTop(25);route.setBounds(routing.removeFromTop(28));for(int i=0;i<2;++i)busControls[(size_t)i]->setBounds(routing.removeFromTop(30));br.removeFromTop(12);
        size_t k=2;const int counts[]{5,5,7,9},busWidth=(br.getWidth()-24)/4;for(int i=0;i<4;++i){auto column=br.removeFromLeft(busWidth);buses.cards.push_back({"",column});column.reduce(10,8);for(int n=0;n<counts[i];++n)busControls[k++]->setBounds(column.removeFromTop(32).reduced(0,2));busMeters[(size_t)i].setBounds(column.removeFromTop(26));br.removeFromLeft(8);}
        auto mr=media.getLocalBounds().reduced(12);media.cards={{"PROJECT SAMPLES",mr.withHeight(268)}};mr.reduce(12,8);mr.removeFromTop(28);auto actions=mr.removeFromTop(36);for(auto* button:mediaButtons)button->setBounds(actions.removeFromLeft(std::min(270,actions.getWidth())).reduced(3,0));mr.removeFromTop(18);mediaInfo.setBounds(mr.removeFromTop(150));

    }
private:
    juce::StringArray trackNames;std::vector<int> trackIds;MainComponent& owner;int track=-1;bool updating=false;int lastTab=-1,lastKeyboardBase=-1,inputPoll=0;
    juce::ComboBox target,mode,device,countIn,quantise,route,sliceBank,sliceMethod,sampleInput;std::array<juce::TextButton*,16> pads{};std::array<bool,16> padHeld{};std::array<int,16> padTracks{},padNotes{};
    StudioTabsTheme tabTheme;juce::TabbedComponent tabs;SamplerSurface sampler,record,buses,media;juce::Component samplerTab,performance;juce::Viewport samplerPort;
    Waveform wave;juce::Slider zoom,scroll,offset;juce::ToggleButton sampleMonitor{"MIDI INPUT"},monitor{"MONITOR"},overdub{"OVERDUB"};
    juce::TextButton recordButton{"RECORD"},stopButton{"STOP"},refreshInput{"REFRESH"};juce::Array<juce::MidiDeviceInfo> midiDevices;juce::Label recordStatus,mediaInfo,sampleNote,zoomCaption,scrollCaption;std::unique_ptr<juce::TooltipWindow> tooltips;
    juce::MidiKeyboardState keys;StudioKeyboard keyboard,sampleKeyboard;bool busy=false;
    std::vector<std::unique_ptr<juce::Component>> controls;std::vector<juce::TextButton*> samplerButtons,mediaButtons;
    std::vector<juce::ToggleButton*> flags;std::vector<bool SamplerSettings::*> flagMembers;
    struct Field {juce::Label* caption;juce::Slider* slider;double SamplerSettings::*real=nullptr;int SamplerSettings::*integer=nullptr;bool root=false;};std::vector<Field> fields;
    std::array<StudioMeter,4> busMeters;std::vector<juce::Component*> busControls;std::vector<std::function<void()>> busRefresh;
    std::vector<CapturedMidi> pending;std::unique_ptr<juce::FileChooser> chooser;std::unique_ptr<MediaTask> task;
    void updateNotes(){
        if(track<0||track>=(int)owner.project.tracks.size()){sampleKeyboard.showNotes({});keyboard.showNotes({});return;}
        const auto notes=tabs.getCurrentTabIndex()==0&&selected().instrument!="sample"?std::array<bool,128>{}:owner.audio.activeNotes(track);sampleKeyboard.showNotes(notes);keyboard.showNotes(notes);if(wave.playing!=notes){wave.playing=notes;wave.repaint();}
        juce::StringArray names,details;int count=0;for(int n=0;n<128;++n)if(notes[(size_t)n]){++count;auto name=juce::MidiMessage::getMidiNoteName(n,true,true,3);if(names.size()<4)names.add(name);details.add(name+" · MIDI "+juce::String(n));}
        sampleNote.setText(names.isEmpty()?tr("No active notes"):tr("PLAYING")+"  "+names.joinIntoString(" · ")+(count>4?" +"+juce::String(count-4):juce::String()),juce::dontSendNotification);
        sampleNote.setTooltip(count?details.joinIntoString("\n"):tr("Notes currently sounding, including one-shot sample tails and MIDI playback."));
        for(int i=0;i<16;++i){const int note=selected().sampler.sliceBase+std::max(0,sliceBank.getSelectedId()-1)*16+i;pads[(size_t)i]->setToggleState(note<128&&notes[(size_t)note],juce::dontSendNotification);}
    }
    void sliceSample(bool checkpoint){
        const auto asset=owner.audio.sampleAsset(track);if(!asset)return;if(checkpoint)owner.checkpoint();releasePads();keys.allNotesOff(0);auto& settings=selected().sampler;
        std::vector<double> points;const int limit=128-settings.sliceBase;
        if(sliceMethod.getSelectedId()<=1)points=detectSampleSlices(*asset,settings.start,settings.end,settings.sensitivity,limit);
        else{const int count=std::min(limit,1<<sliceMethod.getSelectedId());for(int i=0;i<count;++i)points.push_back(settings.start+(settings.end-settings.start)*i/count);}
        settings.slices.fill(0);settings.sliceCount=(int)points.size();std::copy(points.begin(),points.end(),settings.slices.begin());settings.mode=2;sliceBank.setSelectedId(1,juce::dontSendNotification);commit();
    }
    void resetSample(){
        if(!owner.audio.sampleAsset(track))return;owner.checkpoint();releasePads();keys.allNotesOff(0);owner.audio.liveMidi(track,juce::MidiMessage::allSoundOff(1),false);
        auto& target=selected();target.sampler={};target.sampler.mode=isDrumTrack(track)?1:0;target.sampler.polyphony=8;target.sampleRoot=isDrumTrack(track)?(target.engine.midiNote>=0?target.engine.midiNote:fgd::defaultNote(track-4)):60;
        wave.zoom=1;wave.scroll=0;sliceMethod.setSelectedId(1,juce::dontSendNotification);commit();
    }
    Track& selected(){return owner.project.tracks[(size_t)track];}
    template<class T> T* control(juce::Component& page){auto p=std::make_unique<T>();auto* raw=p.get();page.addAndMakeVisible(raw);controls.push_back(std::move(p));return raw;}
    void button(juce::Component& page,juce::String title,std::function<void()> fn){auto* b=control<juce::TextButton>(page);Localisation::bindText(*b,title);b->onClick=std::move(fn);Localisation::bindTip(*b,studioTip(title));(&page==&sampler?samplerButtons:mediaButtons).push_back(b);}
    void toggle(juce::Component& page,juce::String name,bool SamplerSettings::*member){auto* b=control<juce::ToggleButton>(page);Localisation::bindText(*b,name);b->onClick=[this,b,member]{if(updating)return;owner.checkpoint();selected().sampler.*member=b->getToggleState();commit();};Localisation::bindTip(*b,studioTip(name));flags.push_back(b);flagMembers.push_back(member);}
    Field makeField(juce::String title,double lo,double hi,double step){Field f;f.caption=control<juce::Label>(sampler);Localisation::bindLabel(*f.caption,title);f.slider=control<juce::Slider>(sampler);f.slider->setSliderStyle(juce::Slider::LinearHorizontal);f.slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,78,22);f.slider->setRange(lo,hi,step);Localisation::bindTip(*f.slider,studioTip(title));Localisation::bindTip(*f.caption,studioTip(title));owner.trackGesture(*f.slider);return f;}
    void scalar(juce::String name,double SamplerSettings::*member,double lo,double hi,double step){auto f=makeField(name,lo,hi,step);f.real=member;auto* slider=f.slider;slider->onValueChange=[this,slider,member]{if(updating)return;owner.checkpoint(slider);selected().sampler.*member=slider->getValue();commit();};fields.push_back(f);}
    void integer(juce::String name,int SamplerSettings::*member,int lo,int hi){auto f=makeField(name,lo,hi,1);f.integer=member;f.root=!member;auto* slider=f.slider;slider->onValueChange=[this,slider,member]{if(updating)return;owner.checkpoint(slider);if(member)selected().sampler.*member=(int)slider->getValue();else selected().sampleRoot=(int)slider->getValue();commit();};fields.push_back(f);}
    void releasePads(){for(int i=0;i<16;++i)if(padHeld[(size_t)i]){padHeld[(size_t)i]=false;owner.audio.liveMidi(padTracks[(size_t)i],juce::MidiMessage::noteOff(1,padNotes[(size_t)i]));}}
    void commit(){selected().sampler.normalise();owner.changed(false);refresh();}
    void choose(juce::String title,juce::String filter,bool save,std::function<void(juce::File)> fn,bool directory=false){chooser=std::make_unique<juce::FileChooser>(tr(title),juce::File(),filter,true,false,this);juce::Component::SafePointer<Panel> safe(this);chooser->launchAsync((save?juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting:juce::FileBrowserComponent::openMode)|(directory?juce::FileBrowserComponent::canSelectDirectories:juce::FileBrowserComponent::canSelectFiles),[safe,fn](const juce::FileChooser& c){if(safe&&c.getResult()!=juce::File())fn(c.getResult());});}
    void refresh(){
        if(owner.project.tracks.empty())return;updating=true;
        if(track<0||track>=(int)owner.project.tracks.size()){track=std::clamp(owner.selectedTrack,0,(int)owner.project.tracks.size()-1);owner.audio.setMidiMonitor(track,monitor.getToggleState());}
        juce::StringArray names;std::vector<int> ids;const bool samplesOnly=tabs.getCurrentTabIndex()==0;
        for(size_t i=0;i<owner.project.tracks.size();++i)if(!samplesOnly||owner.project.tracks[i].instrument=="sample"){names.add(juce::String(owner.project.tracks[i].name));ids.push_back((int)i);}
        if(names!=trackNames||ids!=trackIds){trackNames=names;trackIds=ids;target.clear(juce::dontSendNotification);for(size_t i=0;i<ids.size();++i)target.addItem(names[(int)i],ids[i]+1);}
        if(samplesOnly&&!owner.audio.isRecording()&&!ids.empty()&&std::find(ids.begin(),ids.end(),track)==ids.end()){releasePads();keys.allNotesOff(0);track=ids.front();owner.select(track,owner.selectedSection);owner.audio.setMidiMonitor(track,monitor.getToggleState());}
        if(samplesOnly&&!owner.audio.isRecording()&&ids.empty()){const int next=std::clamp(owner.selectedTrack,0,(int)owner.project.tracks.size()-1);if(next!=track){releasePads();keys.allNotesOff(0);track=next;owner.audio.setMidiMonitor(track,monitor.getToggleState());}}
        target.setTextWhenNothingSelected(tr("No sample tracks"));target.setSelectedId(std::find(ids.begin(),ids.end(),track)!=ids.end()?track+1:0,juce::dontSendNotification);target.setEnabled(!ids.empty()&&!owner.audio.isRecording());
        auto& s=selected().sampler;for(int id=2;id<=6;++id)sliceMethod.setItemEnabled(id,(1<<id)<=128-s.sliceBase);if(sliceMethod.getSelectedId()>1&&(1<<sliceMethod.getSelectedId())>128-s.sliceBase)sliceMethod.setSelectedId(1,juce::dontSendNotification);auto asset=owner.audio.sampleAsset(track);if(asset!=wave.asset){releasePads();keys.allNotesOff(0);wave.zoom=1;wave.scroll=0;lastKeyboardBase=-1;}wave.asset=std::move(asset);const bool hasSample=wave.asset!=nullptr;zoom.setEnabled(hasSample);wave.onView();mode.setEnabled(hasSample);for(auto* flag:flags)flag->setEnabled(hasSample);for(auto& field:fields)field.slider->setEnabled(hasSample);for(size_t i=2;i<samplerButtons.size();++i)samplerButtons[i]->setEnabled(hasSample);wave.settings=s;wave.repaint();mode.setSelectedId((s.mode<0?(isDrumTrack(track)?1:0):s.mode)+1,juce::dontSendNotification);
        for(size_t i=0;i<flags.size();++i)flags[i]->setToggleState(s.*flagMembers[i],juce::dontSendNotification);
        for(auto& f:fields){f.slider->setValue(f.real?s.*f.real:f.root?selected().sampleRoot:s.*f.integer,juce::dontSendNotification);const bool loopControl=f.real==&SamplerSettings::loopStart||f.real==&SamplerSettings::loopEnd||f.real==&SamplerSettings::crossfade;f.slider->setEnabled(hasSample&&(!loopControl||(s.loop&&mode.getSelectedId()==1))&&(!f.root||mode.getSelectedId()!=3)&&(f.real!=&SamplerSettings::fadeOut||!(s.loop&&mode.getSelectedId()==1))&&(f.real!=&SamplerSettings::release||mode.getSelectedId()==1)&&(f.real!=&SamplerSettings::sensitivity||sliceMethod.getSelectedId()<=1)&&((f.real!=&SamplerSettings::cutoff&&f.real!=&SamplerSettings::resonance)||s.filter));}
        flags[1]->setEnabled(hasSample&&mode.getSelectedId()==1);sampleKeyboard.setVisible(true);sampleKeyboard.setEnabled(hasSample&&(mode.getSelectedId()!=3||s.sliceCount>0));sampleKeyboard.setAvailableRange(0,127);sampleKeyboard.setKeyWidth(30);sampleKeyboard.mappedRange(mode.getSelectedId()==3?s.sliceBase:0,mode.getSelectedId()==3?s.sliceBase+s.sliceCount-1:127);const int banks=std::max(1,(s.sliceCount+15)/16);if(sliceBank.getNumItems()!=banks){sliceBank.clear(juce::dontSendNotification);for(int b=0;b<banks;++b)sliceBank.addItem(juce::String(b*16+1)+" – "+juce::String(std::min(s.sliceCount,(b+1)*16)),b+1);sliceBank.setSelectedId(1,juce::dontSendNotification);}for(int b=0;b<banks;++b)sliceBank.changeItemText(b+1,juce::String(b*16+1)+" – "+juce::String(std::min(s.sliceCount,(b+1)*16)));sliceBank.setVisible(hasSample&&mode.getSelectedId()==3);sliceBank.setEnabled(banks>1);for(int i=0;i<16;++i){int index=std::max(0,sliceBank.getSelectedId()-1)*16+i;auto* pad=pads[(size_t)i];pad->setVisible(hasSample&&mode.getSelectedId()==3&&index<s.sliceCount);pad->setButtonText(juce::MidiMessage::getMidiNoteName(s.sliceBase+index,true,true,3)+" · "+juce::String(index+1));pad->setTooltip(tr("Play this slice. Its MIDI note is shown on the pad.")+" "+juce::String(index+1));pad->setEnabled(hasSample);}sampleMonitor.setToggleState(monitor.getToggleState(),juce::dontSendNotification);sampleMonitor.setEnabled(hasSample&&sampleInput.getSelectedId()>1);sampleInput.setEnabled(!owner.audio.isRecording());
        sliceMethod.setEnabled(hasSample);samplerButtons[5]->setEnabled(hasSample&&s.mode==2&&s.sliceCount>0&&!selected().locked);
        const int base=mode.getSelectedId()==3?s.sliceBase+std::max(0,sliceBank.getSelectedId()-1)*16:std::max(0,selected().sampleRoot-12);if(base!=lastKeyboardBase){lastKeyboardBase=base;sampleKeyboard.setLowestVisibleKey(base);sampleKeyboard.clearKeyMappings();sampleKeyboard.setKeyPressBaseOctave(base/12);const char mapping[]{"awsedftgyhujkolp"};for(int i=0;i<16&&base+i<128;++i)sampleKeyboard.setKeyPressForNote(juce::KeyPress(mapping[i]),base%12+i);}
        route.changeItemText(2,juce::String(owner.project.buses[0].name));route.changeItemText(3,juce::String(owner.project.buses[1].name));
        for(auto& fn:busRefresh)fn();route.setSelectedId(selected().group+2,juce::dontSendNotification);updating=false;resized();
    }
    void saveRecordingSettings(){Localisation::setSetting("studio.midiInput",owner.audio.midiInput());Localisation::setSetting("studio.countIn",countIn.getSelectedId());Localisation::setSetting("studio.quantise",quantise.getSelectedId());Localisation::setSetting("studio.overdub",overdub.getToggleState()?1:0);Localisation::setSetting("studio.monitor",monitor.getToggleState()?1:0);Localisation::setSetting("studio.offset",offset.getValue());}
    void restoreRecordingSettings(){countIn.setSelectedId(std::clamp(Localisation::setting("studio.countIn","2").getIntValue(),1,3));quantise.setSelectedId(std::clamp(Localisation::setting("studio.quantise","1").getIntValue(),1,5));overdub.setToggleState(Localisation::setting("studio.overdub","1")=="1",juce::dontSendNotification);monitor.setToggleState(Localisation::setting("studio.monitor","0")=="1",juce::dontSendNotification);offset.setValue(Localisation::setting("studio.offset","0").getDoubleValue());auto id=Localisation::setting("studio.midiInput");auto devices=juce::MidiInput::getAvailableDevices();for(int i=0;i<devices.size();++i)if(devices[i].identifier==id)device.setSelectedId(i+2);owner.audio.setMidiMonitor(track,monitor.getToggleState());}
    void refreshMidiInputs(){const auto current=owner.audio.midiInput();midiDevices=juce::MidiInput::getAvailableDevices();device.clear(juce::dontSendNotification);device.addItem(tr("On-screen keyboard"),1);int selected=1;for(int i=0;i<midiDevices.size();++i){device.addItem(midiDevices[i].name,i+2);if(midiDevices[i].identifier==current)selected=i+2;}device.setSelectedId(selected,juce::dontSendNotification);if(selected==1&&current.isNotEmpty())owner.audio.setMidiInput({});sampleInput.clear(juce::dontSendNotification);sampleInput.addItem(tr("On-screen keyboard"),1);for(int i=0;i<midiDevices.size();++i)sampleInput.addItem(midiDevices[i].name,i+2);sampleInput.setSelectedId(selected,juce::dontSendNotification);}
    void setupRecording(){
        for(auto* c:std::initializer_list<juce::Component*>{&device,&refreshInput,&countIn,&quantise,&monitor,&overdub,&offset,&recordButton,&stopButton,&recordStatus,&keyboard})record.addAndMakeVisible(c);
        Localisation::bindTip(device,"Choose the MIDI device to monitor or record; no device is needed for the on-screen keyboard.");Localisation::bindTip(refreshInput,"Refresh the list of connected MIDI devices.");Localisation::bindTip(monitor,"Listen to the selected MIDI input through the target track.");Localisation::bindTip(countIn,"Wait zero, one or two bars before recording starts.");Localisation::bindTip(quantise,"Align recorded note starts to this grid, preserving their lengths.");Localisation::bindTip(overdub,"Add notes to existing MIDI. Turn off to replace notes in the recorded region.");Localisation::bindTip(recordButton,"Start recording MIDI into the target track; press again to finish one undoable take.");Localisation::bindTip(stopButton,"Finish the MIDI take and stop playback.");
        refreshMidiInputs();Localisation::bindText(refreshInput,"REFRESH");refreshInput.onClick=[this]{refreshMidiInputs();};
        device.onChange=[this]{int index=device.getSelectedId()-2;auto r=owner.audio.setMidiInput(index>=0&&index<midiDevices.size()?midiDevices[index].identifier:juce::String());if(r.failed())owner.message(r.getErrorMessage(),true);sampleInput.setSelectedId(device.getSelectedId(),juce::dontSendNotification);};
        countIn.addItemList({"No count-in","1 bar count-in","2 bars count-in"},1);countIn.setSelectedId(2);
        quantise.addItemList({"Quantise off","1/4 notes","1/8 notes","1/16 notes","1/32 notes"},1);quantise.setSelectedId(1);overdub.setToggleState(true,juce::dontSendNotification);
        monitor.onClick=[this]{owner.audio.setMidiMonitor(track,monitor.getToggleState());};offset.setRange(-200,200,1);offset.setTextValueSuffix(" ms");Localisation::bindTip(offset,"Recording latency correction");offset.onValueChange=[this]{owner.audio.setRecordOffset(offset.getValue());};
        recordButton.setColour(juce::TextButton::buttonColourId,Daw::meterHigh.darker(.6f));recordButton.onClick=[this]{if(owner.audio.isRecording()){finish();return;}if(selected().locked){owner.message(tr("Unlock the track before recording"),true);return;}pending.clear();auto r=owner.audio.startRecording(track,countIn.getSelectedId()-1);if(r.failed())owner.message(r.getErrorMessage(),true);};stopButton.onClick=[this]{finish();owner.audio.stop();};
    }
    void finish(){if(!owner.audio.isRecording())return;auto take=owner.audio.finishRecording();pending.insert(pending.end(),take.events.begin(),take.events.end());take.events=std::move(pending);pending.clear();const double grids[]{0,1,.5,.25,.125};assembleMidiTake(take,grids[std::clamp(quantise.getSelectedId()-1,0,4)]);auto next=owner.project;if(applyMidiTake(next,take,overdub.getToggleState())){owner.checkpoint();owner.project=std::move(next);owner.changed(true,take.track);}if(take.overflow)owner.message("MIDI input overflow: check the recorded take",true);}
    void handleNoteOn(juce::MidiKeyboardState*,int ch,int note,float velocity)override{owner.audio.liveMidi(track,juce::MidiMessage::noteOn(ch,note,velocity));}
    void handleNoteOff(juce::MidiKeyboardState*,int ch,int note,float velocity)override{owner.audio.liveMidi(track,juce::MidiMessage::noteOff(ch,note,velocity));}
    void setupBuses(){
        for(auto& meter:busMeters){buses.addAndMakeVisible(meter);meter.setLevel(0);Localisation::bindTip(meter,"Mixer group or return output level.");}
        Localisation::bindTip(route,"Send this track directly to the master or through one of the two groups.");buses.addAndMakeVisible(route);route.addItemList({"Direct to master","GROUP A","GROUP B"},1);route.onChange=[this]{if(updating)return;owner.checkpoint();selected().group=route.getSelectedId()-2;owner.changed(false);};
        for(int send=0;send<2;++send){auto* slider=control<juce::Slider>(buses);slider->setRange(0,1,.01);slider->setTextBoxStyle(juce::Slider::TextBoxLeft,false,200,24);Localisation::bind(*slider,[slider,send]{slider->setTextValueSuffix(" "+tr(send==0?"REVERB SEND":"DELAY SEND"));});slider->onValueChange=[this,slider,send]{if(updating)return;owner.checkpoint(slider);selected().sends[(size_t)send]=slider->getValue();owner.changed(false);};owner.trackGesture(*slider);Localisation::bindTip(*slider,send==0?"Amount of this track sent to the shared reverb.":"Amount of this track sent to the shared tempo delay.");busControls.push_back(slider);busRefresh.push_back([this,slider,send]{slider->setValue(selected().sends[(size_t)send],juce::dontSendNotification);});}
        for(int i=0;i<4;++i){auto* name=control<juce::Label>(buses);name->setFont(Daw::font(13,true));name->setEditable(false,true,false);Localisation::bindTip(*name,"Double-click to rename this group or return.");name->onTextChange=[this,name,i]{if(updating)return;owner.checkpoint();owner.project.buses[(size_t)i].name=name->getText().trim().toStdString();owner.changed(false);};busControls.push_back(name);busRefresh.push_back([this,name,i]{name->setText(juce::String(owner.project.buses[(size_t)i].name),juce::dontSendNotification);});
            busScalar(i,"LEVEL",&BusSettings::gain,0,2);busScalar(i,"PAN",&BusSettings::pan,-1,1);
            for(bool solo:{false,true}){auto* b=control<juce::ToggleButton>(buses);b->setButtonText(solo?"SOLO":"MUTE");b->onClick=[this,b,i,solo]{owner.checkpoint();auto& bus=owner.project.buses[(size_t)i];(solo?bus.solo:bus.mute)=b->getToggleState();owner.changed(false);};Localisation::bindTip(*b,solo?"Listen to this mixer group or return on its own.":"Mute this mixer group or return.");busControls.push_back(b);busRefresh.push_back([this,b,i,solo]{auto& bus=owner.project.buses[(size_t)i];b->setToggleState(solo?bus.solo:bus.mute,juce::dontSendNotification);});}
            if(i>=2){busScalar(i,i==2?"ROOM":"CROSSFEED",&BusSettings::amount,0,1);busScalar(i,"DAMPING",&BusSettings::damping,0,1);}if(i==3){busScalar(i,"FEEDBACK",&BusSettings::feedback,0,.9);busScalar(i,"DELAY (beats)",&BusSettings::division,.125,4);}
        }
    }
    void busScalar(int i,juce::String name,double BusSettings::*member,double lo,double hi){auto* s=control<juce::Slider>(buses);s->setRange(lo,hi,.001);Localisation::bind(*s,[s,name]{s->setTextValueSuffix(" "+tr(name));s->setTextBoxStyle(juce::Slider::TextBoxLeft,false,std::clamp(40+(int)juce::GlyphArrangement::getStringWidth(Daw::font(11),tr(name)),80,150),24);});owner.trackGesture(*s);s->onValueChange=[this,s,i,member]{if(updating)return;owner.checkpoint(s);owner.project.buses[(size_t)i].*member=s->getValue();owner.changed(false);};Localisation::bindTip(*s,name=="LEVEL"?"Mixer group or return output level.":name=="PAN"?"Mixer group or return stereo position.":name=="ROOM"?"Size and decay of the shared reverb.":name=="DAMPING"?"Reduce high frequencies in the effect tail.":name=="FEEDBACK"?"Amount of delay fed back for successive echoes.":name=="DELAY (beats)"?"Delay time in quarter-note beats, following the song tempo.":"Amount of stereo crossfeed between the delay channels.");busControls.push_back(s);busRefresh.push_back([this,s,i,member]{s->setValue(owner.project.buses[(size_t)i].*member,juce::dontSendNotification);});}
    void runMedia(juce::File file,bool collect){if(owner.audio.isRecording()||owner.loadingPlugin)return;owner.audio.capturePluginStates(owner.project);task=std::make_unique<MediaTask>(owner.project,file,collect);juce::Component::SafePointer<Panel> safe(this);++owner.loadingPlugin;busy=true;setEnabled(false);task->done=[safe,file,collect](juce::Result r,Project p,RelinkResult found){if(!safe)return;auto& o=safe->owner;--o.loadingPlugin;safe->busy=false;safe->setEnabled(true);o.setEnabled(true);if(r.failed()){o.message(r.getErrorMessage(),true);return;}o.checkpoint();o.project=std::move(p);if(collect){o.projectFile=file;o.dirty=false;}else o.dirty=true;o.restoreInstruments();o.refreshControls();o.message(collect?"Project and samples collected":"Relinked: "+juce::String(found.found)+"; missing: "+juce::String(found.missing.size()));};task->launchThread();}
    void timerCallback()override{if(!isShowing()&&!owner.audio.isRecording())return;
        if(++inputPoll>=15&&!owner.audio.isRecording()){inputPoll=0;const auto available=juce::MidiInput::getAvailableDevices();bool changed=available.size()!=midiDevices.size();if(!changed)for(int i=0;i<available.size();++i)if(available[i].identifier!=midiDevices[i].identifier||available[i].name!=midiDevices[i].name){changed=true;break;}if(changed)refreshMidiInputs();}
        for(int i=0;i<4;++i){const float level=owner.audio.busLevel(i);busMeters[(size_t)i].setLevel(level);}
        if((tooltips!=nullptr)!=Localisation::tipsEnabled()){if(Localisation::tipsEnabled())tooltips=std::make_unique<juce::TooltipWindow>(this,500);else tooltips.reset();}updateNotes();if(lastTab!=tabs.getCurrentTabIndex()){lastTab=tabs.getCurrentTabIndex();refresh();}if(owner.audio.isRecording()){owner.audio.drainRecorded(pending);if(!owner.audio.isPlaying()||owner.audio.recordingOverflow())finish();}bool recording=owner.audio.isRecording();owner.setEnabled(!recording&&!busy);sampler.setEnabled(!recording&&!busy);target.setEnabled(!recording&&target.getNumItems()>0);countIn.setEnabled(!recording);quantise.setEnabled(!recording);overdub.setEnabled(!recording);device.setEnabled(!recording);refreshInput.setEnabled(!recording);recordButton.setButtonText(tr(recording?"FINISH TAKE":"RECORD"));recordStatus.setText(recording?tr("Recording")+" · "+tr("Count-in")+": "+juce::String(owner.audio.countInRemaining(),1):tr("Notes, velocity and sustain are recorded. Quantisation preserves note lengths."),juce::dontSendNotification);if(!juce::ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown()&&!recording&&!dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent()))refresh();}
};
StudioWindow::StudioWindow(MainComponent& owner):DocumentWindow("FASE GROOVE · STUDIO",Daw::base,juce::DocumentWindow::closeButton){setLookAndFeel(&owner.theme);setUsingNativeTitleBar(true);panel=new Panel(owner);setContentOwned(panel,true);setResizable(true,false);setResizeLimits(940,660,1600,1200);const auto* display=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();setSize(display?std::clamp(display->userArea.getWidth()-48,940,1120):1120,display?std::clamp(display->userArea.getHeight()-48,660,960):960);setAlwaysOnTop(true);}
StudioWindow::~StudioWindow(){clearContentComponent();setLookAndFeel(nullptr);}
void StudioWindow::closeButtonPressed(){panel->endSession();setVisible(false);}
bool StudioWindow::verify(const juce::File& folder){bool ok=panel->verify(folder);setTopLeftPosition(7,9);closeButtonPressed();ok=!isVisible()&&ok;open(0);ok=isAlwaysOnTop()&&isVisible()&&getPosition()!=juce::Point<int>(7,9)&&ok;folder.getChildFile("studio.txt").replaceWithText(ok?"PASS native Studio panels, sample load/trim/undo, MIDI routing and take recovery on close, drum slice pitches/editor, always-on-top and centred reopening":"FAIL Studio integration");return ok;}
void StudioWindow::open(int tab){panel->showTab(tab);if(!isVisible())centreWithSize(getWidth(),getHeight());setAlwaysOnTop(true);setVisible(true);toFront(true);}
}
