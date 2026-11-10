// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Branding.h"
#include "MainComponent.h"
#include "StudioIO.h"
#include "AiComposerWindow.h"
#include "AiComposer.h"
#include "UiText.h"
#include "ExportSupport.h"
#include "DarkTitleBar.h"
#include "../ThirdParty/fasegroovepiano/Source/engine/GrooveEngine.h"
#include <BinaryData.h>
#include <cmath>
#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX 1
 #endif
 #include <windows.h>
 #undef min
 #undef max
 #undef small
#endif
#if JUCE_MAC
 #include <CoreGraphics/CoreGraphics.h>
#endif
namespace fga {
#include "WorkspaceViewHelpers.h"
#include "NativeWindows.h"
PianoRoll::PianoRoll(MainComponent& m):owner(m){
    setWantsKeyboardFocus(true);addAndMakeVisible(ghostTrack);addAndMakeVisible(expressionLane);
    showScaleGuide=Localisation::setting("pianoScaleGuide","0")=="1";showActiveNotes=Localisation::setting("pianoActiveNotes","1")!="0";
    guitarNotes=Localisation::setting("pianoGuitarNotes","0")=="1";ghost=std::clamp(Localisation::setting("pianoGhostTrack","-1").getIntValue(),-1,maxTracks-1);
    expressionMode=std::clamp(Localisation::setting("pianoExpressionLane","0").getIntValue(),0,3);grid=std::pow(.5,std::clamp(Localisation::setting("pianoGrid","3").getIntValue(),1,4)-1);
    Localisation::bindTip(ghostTrack,"Ghost notes from another track");Localisation::bindTip(expressionLane,"Clip MIDI expression");
    expressionLane.addItemList({"Velocity","Sustain CC64","ModWheel CC1","Pitch Bend"},1);Localisation::bindItems(expressionLane,{"Velocity","Sustain CC64","ModWheel CC1","Pitch Bend"});expressionLane.setSelectedId(expressionMode+1,juce::dontSendNotification);
    expressionLane.onChange=[this]{expressionMode=expressionLane.getSelectedId()-1;Localisation::setSetting("pianoExpressionLane",expressionMode);repaint();};
    ghostTrack.onChange=[this]{ghost=ghostTrack.getSelectedId()-2;Localisation::setSetting("pianoGhostTrack",ghost);fitNotes();repaint();};
}
void PianoRoll::setActiveNotesVisible(bool visible){
    if(showActiveNotes==visible)return;
    if(!before.empty()||selecting||erasing||editingVelocity)finishGesture();
    stopPreview();selected.clear();showActiveNotes=visible;Localisation::setSetting("pianoActiveNotes",visible?"1":"0");
    owner.activeNotesButton.setToggleState(visible,juce::dontSendNotification);owner.updatePianoEditingState();fitNotes();repaint();
}
std::vector<Note> PianoRoll::ghostNotes()const{
    std::vector<Note> result;if(ghost<0||ghost>=(int)owner.project.tracks.size()||ghost==owner.selectedTrack)return result;
    const auto& source=owner.project.tracks[(size_t)ghost];const double begin=owner.clipStart(),end=begin+owner.clipLength();
    for(size_t ci=0;ci<source.clips.size();++ci){const double at=owner.project.clipStart(ghost,(int)ci);for(auto n:source.clips[ci].notes){
        const double from=std::max(begin,at+n.start),to=std::min(end,at+n.start+n.length);if(to<=from||n.velocity<=0)continue;
        n.start=from-begin;n.length=to-from;result.push_back(n);
    }}return result;
}
void PianoRoll::resized(){ghostTrack.clear(juce::dontSendNotification);ghostTrack.addItem(tr("Ghost notes: Off"),1);for(size_t i=0;i<owner.project.tracks.size();++i)ghostTrack.addItem(utf(owner.project.tracks[i].name),(int)i+2);if(ghost>=(int)owner.project.tracks.size())ghost=-1;ghostTrack.setSelectedId(ghost+2,juce::dontSendNotification);}
bool PianoRoll::ownerTrackIsDrum()const{const auto& track=owner.project.tracks[(size_t)owner.selectedTrack];return isDrumTrack(owner.selectedTrack)&&!(track.instrument=="sample"&&track.sampler.mode==2);}
bool PianoRoll::laneView()const{const auto& track=owner.project.tracks[(size_t)owner.selectedTrack];if(track.instrument=="sample"&&track.sampler.mode==2)return false;return ownerTrackIsDrum()||(owner.selectedTrack==3&&!guitarNotes);}
bool PianoRoll::scaleGuideAvailable()const{const auto& track=owner.project.tracks[(size_t)owner.selectedTrack];return !laneView()&&!(track.instrument=="sample"&&track.sampler.mode==2);}
bool PianoRoll::noteInScale(int midiNote,double relativeBeat)const{const auto settings=resolveSectionHarmony(owner.project,owner.project.sectionAtBeat(owner.clipStart()+relativeBeat));const auto& intervals=sgk::scaleIntervals(settings.scale);const int pc=(midiNote-settings.key+120)%12;return std::find(intervals.begin(),intervals.end(),pc)!=intervals.end();}
void PianoRoll::fitNotes(){
    int lo=127,hi=0;auto include=[&](const auto& notes){for(const auto& n:notes){lo=std::min(lo,n.pitch);hi=std::max(hi,n.pitch);}};if(showActiveNotes)include(owner.clip().notes);include(ghostNotes());
    if(lo>hi){lo=owner.selectedTrack==1?28:48;hi=lo+24;}lo-=2;hi+=2;while(hi-lo<24){--lo;++hi;}
    lowNote=std::clamp(lo,0,103);highNote=std::clamp(hi,lowNote+24,127);
}
float PianoRoll::noteY(const Note& n)const{return laneView()?40.f+(owner.selectedTrack==3?n.lane:0)*40.f:48.f+(highNote-n.pitch)*rowHeight();}
float PianoRoll::noteWidth(const Note& n)const{return std::max(5.f,(float)((laneView()&&!ownerTrackIsDrum()?grid*.85:n.length)*pixelsPerBeat));}
float PianoRoll::noteHeight()const{return laneView()?36.f:rowHeight()-1.f;}
int PianoRoll::laneAt(float y)const{return owner.selectedTrack==3?std::clamp((int)((y-40)/40),0,11):0;}
int PianoRoll::pitch(float y)const{return ownerTrackIsDrum()?(owner.project.tracks[(size_t)owner.selectedTrack].engine.midiNote<0?fgd::defaultNote(owner.selectedTrack-4):owner.project.tracks[(size_t)owner.selectedTrack].engine.midiNote):std::clamp(highNote-(int)((y-48)/rowHeight()),0,127);}
int PianoRoll::hit(juce::Point<float> p)const{if(!showActiveNotes)return -1;const auto& c=owner.clip();for(int i=(int)c.notes.size()-1;i>=0;--i){const auto& n=c.notes[(size_t)i];juce::Rectangle<float> r(leftMargin()+(float)(n.start*pixelsPerBeat),noteY(n),noteWidth(n),noteHeight());if(r.contains(p))return i;}return -1;}
void PianoRoll::paint(juce::Graphics& g){
    g.fillAll(Daw::well);const auto view=owner.pianoPort.getViewArea();const auto& notes=owner.clip().notes;const float left=(float)leftMargin();
    // Long clips: grid lines and notes outside the repainted area are skipped.
    const auto dirtyArea=g.getClipBounds().expanded(4);
    const auto gridArea=juce::Rectangle<float>(left,48,(float)getWidth()-left,std::max(40.f,velocityTop()-52));
    auto blackKey=[](int n){const int k=n%12;return k==1||k==3||k==6||k==8||k==10;};
    const bool highlightScale=showScaleGuide&&scaleGuideAvailable();
    struct ScaleBand{double from,to;int key,scale;};std::vector<ScaleBand> scaleBands;
    if(highlightScale)for(int si=0;si<(int)owner.project.sections.size();++si){const double start=std::max(owner.clipStart(),owner.project.sectionStart(si)),end=std::min(owner.clipStart()+owner.clipLength(),si+1==(int)owner.project.sections.size()?owner.clipStart()+owner.clipLength():owner.project.sectionStart(si+1));if(end<=start)continue;const auto h=resolveSectionHarmony(owner.project,si);scaleBands.push_back({start-owner.clipStart(),end-owner.clipStart(),h.key,h.scale});}

    {
        juce::Graphics::ScopedSaveState clip(g);g.reduceClipRegion(gridArea.toNearestInt());
        // Rows of the black keys a shade darker, octaves (C) marked by a stronger line.
        if(!laneView())for(int n=lowNote;n<=highNote;++n){float y=48+(highNote-n)*rowHeight();if(blackKey(n)){g.setColour(juce::Colours::black.withAlpha(.22f));g.fillRect(left,y,(float)getWidth()-left,rowHeight());}
            for(const auto& band:scaleBands){const int pc=(n-band.key+120)%12;const auto& scale=sgk::scaleIntervals(band.scale);if(std::find(scale.begin(),scale.end(),pc)==scale.end())continue;g.setColour(Daw::accent.withAlpha(pc==0?.18f:.10f));g.fillRect(left+(float)(band.from*pixelsPerBeat),y,(float)((band.to-band.from)*pixelsPerBeat),rowHeight());}
            g.setColour(juce::Colours::white.withAlpha(n%12==0?.1f:n%12==5?.04f:0.f));if(n%12==0||n%12==5)g.fillRect(left,y+rowHeight()-1,(float)getWidth()-left,1.f);}
        // Grid: bars, beats and the chosen division, each fainter.
        const double beats=owner.clipLength();
        for(double b=0;b<=beats;b+=grid){int x=leftMargin()+(int)(b*pixelsPerBeat);if(x<dirtyArea.getX())continue;if(x>dirtyArea.getRight())break;const bool bar=std::abs(std::fmod(b,4.))<.0001,beat=std::abs(std::fmod(b,1.))<.0001;
            g.setColour(juce::Colours::white.withAlpha(bar?.16f:beat?.07f:.03f));if(!laneView())g.drawVerticalLine(x,48,velocityTop()-4);}
    }
    if(laneView()){
        const int lanes=owner.selectedTrack==3?12:1;
        for(int l=0;l<lanes;++l){const float y=40+l*40.f;
            for(double b=0;b<owner.clipLength();b+=grid){const float x=left+(float)(b*pixelsPerBeat);if(x+pixelsPerBeat*grid<view.getX()+left||x>view.getRight())continue;const bool downbeat=std::abs(std::fmod(b,4.))<.0001,beat=std::abs(std::fmod(b,1.))<.0001;
                g.setColour(juce::Colours::white.withAlpha(downbeat?.09f:beat?.065f:((int)(b*4)/4)%2==0?.045f:.03f));g.fillRoundedRectangle({x+1.5f,y+1.5f,std::max(2.f,(float)(pixelsPerBeat*grid)-3),37.f},Daw::radius);}
        }
    }
    {juce::Graphics::ScopedSaveState clip(g);g.reduceClipRegion(gridArea.toNearestInt());
        const auto ink=Daw::trackColour(std::max(0,ghost));for(const auto& n:ghostNotes()){
            if(!laneView()&&(n.pitch<lowNote||n.pitch>highNote))continue;const auto r=juce::Rectangle<float>(left+(float)(n.start*pixelsPerBeat),noteY(n),noteWidth(n),noteHeight()).reduced(.5f,1.f);if(!r.intersects(dirtyArea.toFloat()))continue;
            g.setColour(ink.withAlpha(showActiveNotes?.22f:.48f));g.fillRoundedRectangle(r,2.f);g.setColour(ink.withAlpha(showActiveNotes?.4f:.8f));g.drawRoundedRectangle(r,2.f,1.f);
        }
    }
    if(showActiveNotes)for(size_t i=0;i<notes.size();++i){const auto& n=notes[i];auto r=juce::Rectangle<float>(left+(float)(n.start*pixelsPerBeat),noteY(n),noteWidth(n),noteHeight()).reduced(.5f,1.f);if(!laneView()&&(n.pitch<lowNote||n.pitch>highNote))continue;if(!r.intersects(dirtyArea.toFloat()))continue;const auto ink=owner.selectedTrack==3?guitarColour(n.lane):colour(owner.selectedTrack);const bool chosen=selected.count((int)i)>0;
        if(laneView()&&n.velocity<55)r=r.withSizeKeepingCentre(ownerTrackIsDrum()?r.getWidth():r.getWidth()*.45f,r.getHeight()*.4f);
        // Velocity reads as the note's brightness.
        g.setColour(Daw::well.interpolatedWith(ink,.55f+.45f*n.velocity/127.f));g.fillRoundedRectangle(r,2.5f);
        if(r.getHeight()>5){g.setColour(juce::Colours::white.withAlpha(.18f));g.fillRect(r.getX()+2,r.getY()+.5f,std::max(0.f,r.getWidth()-4),1.f);}
        g.setColour(chosen?Daw::text:juce::Colours::black.withAlpha(.55f));g.drawRoundedRectangle(r,2.5f,chosen?1.6f:.8f);
        if(laneView()&&n.velocity>=115){g.setColour(Daw::text.withAlpha(.35f));g.fillRoundedRectangle(r.withHeight(3).reduced(2,0).translated(0,2),1.5f);}
        if(!laneView()&&r.getWidth()>34&&r.getHeight()>=9){g.setColour(Daw::onAccent);g.setFont(Daw::font(std::min(10.f,r.getHeight()-1),true));juce::String label=juce::MidiMessage::getMidiNoteName(n.pitch,true,true,4);
            if(owner.clip().vocal)for(const auto& syllable:owner.clip().vocal->syllables)if(std::find(syllable.noteIds.begin(),syllable.noteIds.end(),n.vocalId)!=syllable.noteIds.end()){label=juce::String::fromUTF8(syllable.text.c_str());break;}
            g.drawText(label,r.reduced(4,0),juce::Justification::centredLeft);}
    }
    if(!marquee.isEmpty()){g.setColour(Daw::accent.withAlpha(.08f));g.fillRect(marquee);g.setColour(Daw::accent.withAlpha(.8f));g.drawRect(marquee,1);}
    int vx=view.getX();
    if(laneView()){const int lanes=owner.selectedTrack==3?12:1;for(int l=0;l<lanes;++l){int y=40+l*40;const auto ink=owner.selectedTrack==3?guitarColour(l):colour(owner.selectedTrack);auto h=juce::Rectangle<float>((float)vx+2,(float)y+1,left-4,38);g.setColour(Daw::surface);g.fillRoundedRectangle(h,Daw::radius);g.setColour(ink.withAlpha(.9f));g.fillRoundedRectangle(h.withWidth(4),1.5f);g.setFont(Daw::font(Daw::small,true));g.setColour(Daw::text);g.drawText(tr(owner.selectedTrack==3?fgg::laneShortName(l):utf(owner.project.tracks[(size_t)owner.selectedTrack].name)),h.reduced(16,0).withHeight(23),juce::Justification::centredLeft);g.setFont(Daw::font(9.5f));g.setColour(Daw::textFaint);g.drawText(ownerTrackIsDrum()?"MIDI "+juce::String(pitch(0)):tr("Guitar part"),h.withTrimmedTop(21).withTrimmedLeft(16),juce::Justification::centredLeft);}}
    else{
        // The keyboard: white keys across the column, black keys shorter on top of them.
        g.setColour(Daw::well);g.fillRect((float)vx,48.f,left,velocityTop()-48);
        for(int n=lowNote;n<=highNote;++n){const float y=48+(highNote-n)*rowHeight();const auto key=juce::Rectangle<float>((float)vx,y,left-2,rowHeight());
            if(!blackKey(n)){g.setColour(juce::Colour(0xffd6d9de));g.fillRect(key);g.setColour(juce::Colour(0xff9da3ab));if(n%12==0||n%12==5)g.fillRect(key.getX(),key.getBottom()-1,key.getWidth(),1.f);}
            else{g.setColour(juce::Colour(0xffd6d9de));g.fillRect(key);g.setColour(juce::Colour(0xff1c1f24));g.fillRoundedRectangle(key.withWidth(key.getWidth()*.6f).reduced(0,.5f),1.5f);}
            if(highlightScale&&noteInScale(n)){g.setColour(Daw::accent);g.fillRect(key.getRight()-3,y+1,3.f,rowHeight()-2);}
            if(n%12==0){g.setColour(juce::Colour(0xff2a2d33));g.setFont(Daw::font(Daw::small,true));g.drawText(juce::MidiMessage::getMidiNoteName(n,true,true,4),key.reduced(4,0),juce::Justification::centredRight);}}
        g.setColour(juce::Colours::black.withAlpha(.6f));g.drawVerticalLine(vx+(int)left-2,48.f,velocityTop());
    }
    // The original Piano chord strip and velocity lane, bound to this track's MIDI.
    g.setColour(Daw::chrome);g.fillRect(vx,0,view.getWidth(),40);
    if(ownerTrackIsDrum()){
        for(double b=0;b<owner.clipLength();b+=1){const float x=left+(float)(b*pixelsPerBeat);if(x<view.getX()+left||x>view.getRight())continue;label(g,juce::String((int)b%4+1),{(int)x+3,23,40,14},10,(int)b%4==0?Daw::accent:Daw::textFaint);if((int)b%4==0)label(g,tr("Bar")+" "+juce::String((int)b/4+1),{(int)x+3,2,140,18},10,Daw::text);}
    }else{
        for(double b=0;b<owner.clipLength()-1./960.;){
            const double abs=owner.clipStart()+b;double cellFrom,cellEnd;std::string planned;chordCell(owner.project,abs,cellFrom,cellEnd,planned);
            const auto harmony=resolveSectionHarmony(owner.project,owner.project.sectionAtBeat(abs));const double step=std::min(owner.clipLength()-b,std::max(1./960.,cellEnd-abs));
            const int x=leftMargin()+(int)(b*pixelsPerBeat),width=std::max(1,(int)(step*pixelsPerBeat)-2);
            if(x+width>=view.getX()+left&&x<=view.getRight()){
                auto r=juce::Rectangle<int>(x+1,3,width,19);g.setColour(Daw::control);g.fillRoundedRectangle(r.toFloat(),Daw::radius);g.setColour(Daw::accent);g.fillRoundedRectangle(r.toFloat().withWidth(3.f),1.f);
                const auto detected=chordHeardInClip(notes,b,b+step,harmony.key,harmony.scale,{},planned);
                label(g,utf(detected.empty()?planned:detected),r.reduced(8,0),11,detected.empty()?Daw::textDim:Daw::text);
                const double local=owner.audio.position()-owner.clipStart();g.setFont(Daw::font(Daw::small,true));g.setColour(local>=b&&local<b+step?Daw::accent:Daw::textDim);g.drawText(juce::String((int)(b/4)+1),x+4,24,50,14,juce::Justification::centredLeft);
            }b+=step;
        }
    }
    g.setColour(juce::Colours::black.withAlpha(.6f));g.drawHorizontalLine(39,(float)vx,(float)view.getRight());
    g.setColour(Daw::chrome);g.fillRect(vx,0,leftMargin(),40);
    g.setColour(Daw::chrome);g.fillRect(vx,(int)velocityTop(),view.getWidth(),getHeight()-(int)velocityTop());
    // Three guides (0, 64, 127) make the lane readable without a numeric scale.
    for(int v:{0,64,127}){const float y=velocityTop()+velocityHeight()*(1.f-v/127.f);g.setColour(juce::Colours::white.withAlpha(v==64?.09f:.05f));g.drawHorizontalLine((int)y,left,(float)view.getRight());}
    label(g,expressionMode==0?"VELOCITY":expressionMode==1?"CC64":expressionMode==2?"CC1":"PITCH BEND",{vx+6,(int)velocityTop(),leftMargin()-8,(int)velocityHeight()},10,Daw::textDim);
    g.setColour(juce::Colours::black.withAlpha(.6f));g.drawHorizontalLine((int)velocityTop(),(float)vx,(float)view.getRight());
    if(ownerTrackIsDrum())for(double b=0;b<owner.clipLength();b+=grid){const float x=left+(float)(b*pixelsPerBeat);if(x<view.getX()+left||x>view.getRight())continue;auto column=juce::Rectangle<float>(x+1,velocityTop()+8,std::max(3.f,(float)(pixelsPerBeat*grid)-3),velocityHeight()-4);g.setColour(juce::Colours::white.withAlpha(((int)(b*4)/4)%2==0?.05f:.03f));g.fillRoundedRectangle(column,Daw::radius);}
    if(showActiveNotes&&expressionMode>0){const auto& lane=owner.clip().expression[(size_t)(expressionMode-1)];juce::Path path;bool first=true;g.setColour(Daw::automation);for(const auto& p:lane.points){const float px=left+(float)(p.beat*pixelsPerBeat),py=velocityTop()+(1-(float)p.value)*velocityHeight();if(first){path.startNewSubPath(px,py);first=false;}else path.lineTo(px,py);g.fillEllipse(px-3,py-3,6,6);}g.strokePath(path,juce::PathStrokeType(1.5f));}
    if(showActiveNotes&&expressionMode==0)for(size_t i=0;i<notes.size();++i){const auto& n=notes[i];const float x=left+(float)(n.start*pixelsPerBeat);if(x<vx+left||x>view.getRight())continue;float h=n.velocity/127.f*velocityHeight();g.setColour(selected.count((int)i)?Daw::text:colour(owner.selectedTrack).withAlpha(.85f));g.fillRect(x-1.f,getHeight()-12-h,2.f,h);g.fillEllipse(x-3,getHeight()-15-h,6,6);}
    if(!showActiveNotes)label(g,tr("Current track notes hidden"),{vx+leftMargin()+10,(int)velocityTop()+12,std::max(0,view.getWidth()-leftMargin()-20),22},12,Daw::textDim);
    const auto local=owner.audio.position()-owner.clipStart();if(local>=0&&local<owner.clipLength()){float x=std::round(left+(float)(local*pixelsPerBeat));if(x>vx+left){g.setColour(Daw::playhead.withAlpha(.9f));g.fillRect(x,24.f,1.f,(float)getHeight()-24);}}
}
void PianoRoll::editVelocity(juce::Point<float> point){
    if(!showActiveNotes)return;
    if(expressionMode>0){const double at=std::clamp(std::round(beat(point.x)/grid)*grid,0.,owner.clipLength());const double value=std::clamp((double)(getHeight()-12-point.y)/velocityHeight(),0.,1.);if(!velocityCheckpoint){owner.checkpoint();velocityCheckpoint=true;}auto& lane=owner.clip().expression[(size_t)(expressionMode-1)];lane.enabled=true;auto found=std::find_if(lane.points.begin(),lane.points.end(),[at](const auto& p){return std::abs(p.beat-at)<1./960.;});if(found!=lane.points.end())found->value=value;else lane.points.push_back({at,value,0});lane.normalise(owner.clipLength(),1);repaint();return;}
    const double at=beat(point.x);int nearest=-1;double distance=12./pixelsPerBeat;
    for(size_t i=0;i<owner.clip().notes.size();++i){const auto& n=owner.clip().notes[i];if(std::abs(n.start-at)<distance){nearest=(int)i;distance=std::abs(n.start-at);}}
    if(nearest<0)return;const auto time=owner.clip().notes[(size_t)nearest].start;
    const int value=std::clamp((int)((getHeight()-12-point.y)/velocityHeight()*127),1,127);
    if(std::none_of(owner.clip().notes.begin(),owner.clip().notes.end(),[&](const Note& n){return std::abs(n.start-time)<1./960.&&n.velocity!=value;}))return;
    if(!velocityCheckpoint){owner.checkpoint();velocityCheckpoint=true;}
    for(auto& n:owner.clip().notes)if(std::abs(n.start-time)<1./960.)n.velocity=value;
    repaint();
}

PianoRoll::~PianoRoll(){stopPreview();}
void PianoRoll::visibilityChanged(){if(!isShowing())stopPreview();}
void PianoRoll::stopPreview(){stopTimer();if(previewTrack>=0)for(int pitch:previewPitches)owner.audio.liveMidi(previewTrack,juce::MidiMessage::noteOff(1,pitch),false);previewPitches.clear();previewTrack=-1;}
void PianoRoll::previewSelection(){
    stopPreview();if(owner.audio.isPlaying()||owner.audio.isRecording()||owner.audio.isAudioRecording())return;
    previewTrack=owner.selectedTrack;
    for(int index:selected)if(juce::isPositiveAndBelow(index,(int)owner.clip().notes.size())){const auto& note=owner.clip().notes[(size_t)index];if(std::find(previewPitches.begin(),previewPitches.end(),note.pitch)!=previewPitches.end())continue;
        previewPitches.push_back(note.pitch);owner.audio.liveMidi(previewTrack,juce::MidiMessage::noteOn(1,note.pitch,(juce::uint8)note.velocity),false);if(previewPitches.size()>=12)break;}
    if(!previewPitches.empty())startTimer(220);else previewTrack=-1;
}
void PianoRoll::mouseMove(const juce::MouseEvent& e){
    const int index=hit(e.position);bool edge=false;
    if(index>=0&&(!laneView()||ownerTrackIsDrum())){const auto& n=owner.clip().notes[(size_t)index];edge=e.x>=leftMargin()+n.start*pixelsPerBeat+noteWidth(n)-std::min(6.f,noteWidth(n)*.4f);}
    setMouseCursor(edge?juce::MouseCursor::LeftRightResizeCursor:juce::MouseCursor::NormalCursor);
}
void PianoRoll::mouseDown(const juce::MouseEvent& e){if(!showActiveNotes){if(e.y<40&&e.x>=owner.pianoPort.getViewArea().getX()+leftMargin())owner.seekTimeline(owner.clipStart()+std::clamp(beat(e.position.x),0.,owner.clipLength()));return;}if(expressionMode>0&&e.y>=velocityTop()&&e.mods.isPopupMenu()){if(owner.editingRestricted())return;auto& lane=owner.clip().expression[(size_t)(expressionMode-1)];const double at=beat(e.position.x);auto nearest=std::min_element(lane.points.begin(),lane.points.end(),[at](const auto& a,const auto& b){return std::abs(a.beat-at)<std::abs(b.beat-at);});if(nearest!=lane.points.end()&&std::abs(nearest->beat-at)*pixelsPerBeat<12){owner.checkpoint();lane.points.erase(nearest);if(lane.points.empty())lane.enabled=false;owner.changed();}return;}
    grabKeyboardFocus();editingVelocity=false;if(e.x<owner.pianoPort.getViewArea().getX()+leftMargin())return;
    if(e.y<40){owner.seekTimeline(owner.clipStart()+std::clamp(beat(e.position.x),0.,owner.clipLength()));return;}
    if(e.y>=velocityTop()){velocityCheckpoint=false;editingVelocity=true;editVelocity(e.position);return;}
    if(laneView()&&e.y>=40+(owner.selectedTrack==3?12:1)*40)return;
    if(!laneView()&&e.y<48)return;
    int index=hit(e.position);if(tool==1&&index<0&&!e.mods.isPopupMenu()){selecting=true;anchor=e.position;marquee={};if(!e.mods.isShiftDown())selected.clear();repaint();return;}if(e.mods.isPopupMenu()||tool==2){erasing=true;eraseCheckpoint=false;eraseAt(e.position);return;}
    gestureOriginal=owner.clip().notes;anchor=e.position;resizing=false;velocityDrag=e.mods.isAltDown();before=owner.clip().notes;
    if(index>=0){if(e.mods.isShiftDown()){if(selected.count(index))selected.erase(index);else selected.insert(index);}else if(!selected.count(index)){selected.clear();selected.insert(index);if(laneView()&&owner.selectedTrack==3){const auto hitNote=before[(size_t)index];for(size_t k=0;k<before.size();++k)if(before[k].lane==hitNote.lane&&std::abs(before[k].start-hitNote.start)<.18)selected.insert((int)k);}}const auto& n=before[(size_t)index];resizing=(!laneView()||ownerTrackIsDrum())&&e.x>=leftMargin()+n.start*pixelsPerBeat+noteWidth(n)-std::min(6.f,noteWidth(n)*.4f)&&!velocityDrag;}
    else{if(!e.mods.isShiftDown())selected.clear();double start=std::floor(beat(e.position.x)/grid)*grid;if(start>=owner.clipLength())return;if(laneView()&&owner.selectedTrack==3){const int lane=laneAt(e.position.y);const double abs=owner.clipStart()+start;const auto settings=resolveSectionHarmony(owner.project,owner.project.sectionAtBeat(abs));const double chordBeats=std::max(.25,settings.chordSteps*.25);const int slot=((int)(abs/chordBeats))%settings.chordCount;auto chord=bandChord(settings.key,settings.scale,settings.degrees[(size_t)slot],settings.freeChords[(size_t)slot],(sgk::Extension)settings.extension);auto notes=fgg::laneNotes((fgg::Lane)lane,fgg::tonesOf(chord),(int)(start*4)%16);for(int note:notes){owner.clip().notes.push_back({start,std::min(grid,owner.clipLength()-start),note,brushVelocity,lane});selected.insert((int)owner.clip().notes.size()-1);}}
        else{owner.clip().notes.push_back({start,std::min(ownerTrackIsDrum()?defaultDrumNoteLength(owner.selectedTrack):grid,owner.clipLength()-start),pitch(e.position.y),ownerTrackIsDrum()?brushVelocity:(int)owner.velocitySlider.getValue(),ownerTrackIsDrum()?owner.selectedTrack-4:0});selected.insert((int)owner.clip().notes.size()-1);}if(laneView()&&flamBrush&&owner.clip().notes.size()>before.size()){auto note=owner.clip().notes.back();note.start=std::max(0.,note.start-.08);note.length=.04;note.velocity=45;owner.clip().notes.push_back(note);}before=owner.clip().notes;}
    if(index>=0)previewSelection();
    repaint();
}
void PianoRoll::eraseAt(juce::Point<float> point){
    if(!showActiveNotes)return;
    const int index=hit(point);if(index<0)return;if(!eraseCheckpoint){owner.checkpoint();eraseCheckpoint=true;}
    if(laneView()&&owner.selectedTrack==3){const auto note=owner.clip().notes[(size_t)index];auto& notes=owner.clip().notes;notes.erase(std::remove_if(notes.begin(),notes.end(),[&](const Note& n){return n.lane==note.lane&&std::abs(n.start-note.start)<.18;}),notes.end());}
    else owner.clip().notes.erase(owner.clip().notes.begin()+index);selected.clear();tidyNotes(owner.clip(),owner.clipLength());owner.dirty=true;owner.audio.publish(owner.project,true,owner.selectedTrack);repaint();
}
void PianoRoll::mouseDrag(const juce::MouseEvent& e){
    if(erasing){eraseAt(e.position);return;}
    if(selecting){marquee=juce::Rectangle<float>(anchor,e.position);selected.clear();for(size_t i=0;i<owner.clip().notes.size();++i){const auto& n=owner.clip().notes[i];if(marquee.intersects(juce::Rectangle<float>(leftMargin()+(float)(n.start*pixelsPerBeat),noteY(n),noteWidth(n),noteHeight())))selected.insert((int)i);}repaint();return;}
    if(editingVelocity){editVelocity(e.position);return;}
    if(before.empty()||selected.empty())return;double delta=std::round((e.position.x-anchor.x)/pixelsPerBeat/grid)*grid;int deltaPitch=-(int)std::round((e.position.y-anchor.y)/rowHeight());auto& notes=owner.clip().notes;
    if(!velocityDrag&&!resizing){
        double earliest=owner.clipLength(),latest=0;int lowest=127,highest=0;
        for(int i:selected)if(i>=0&&i<(int)before.size()){const auto& n=before[(size_t)i];earliest=std::min(earliest,n.start);latest=std::max(latest,n.start+n.length);lowest=std::min(lowest,n.pitch);highest=std::max(highest,n.pitch);}
        delta=std::clamp(delta,-earliest,std::max(-earliest,owner.clipLength()-latest));deltaPitch=std::clamp(deltaPitch,-lowest,127-highest);
    }
    bool auditionChanged=false;
    for(int i:selected)if(i>=0&&i<(int)notes.size()&&i<(int)before.size()){const auto n=before[(size_t)i];auto& now=notes[(size_t)i];if(velocityDrag)now.velocity=std::clamp(n.velocity+(int)(anchor.y-e.position.y),1,127);else if(resizing)now.length=std::clamp(n.length+delta,std::min(.0625,owner.clipLength()-n.start),owner.clipLength()-n.start);else{now.start=std::clamp(n.start+delta,0.,std::max(0.,owner.clipLength()-n.length));if(!laneView()){const int nextPitch=std::clamp(n.pitch+deltaPitch,0,127);auditionChanged=auditionChanged||now.pitch!=nextPitch;now.pitch=nextPitch;}else if(owner.selectedTrack==3)now.lane=std::clamp(n.lane+(int)std::round((e.position.y-anchor.y)/40.),0,11);}}
    if(auditionChanged)previewSelection();
    repaint();
}
void PianoRoll::mouseUp(const juce::MouseEvent&){finishGesture();}
void PianoRoll::finishGesture(){
    if(erasing){erasing=false;if(eraseCheckpoint)owner.changed(true,owner.selectedTrack);return;}
    if(selecting){selecting=false;marquee={};repaint();return;}
    if(editingVelocity){editingVelocity=false;if(velocityCheckpoint)owner.changed();return;}
    if(before.empty())return;
    before.clear();const bool modified=owner.clip().notes!=gestureOriginal;
    if(!modified){gestureOriginal.clear();repaint();return;}
    auto result=std::move(owner.clip().notes);owner.clip().notes=std::move(gestureOriginal);owner.checkpoint();owner.clip().notes=std::move(result);
    std::vector<Note> wanted;
    for(int i:selected)if(i>=0&&i<(int)owner.clip().notes.size())wanted.push_back(owner.clip().notes[(size_t)i]);
    tidyNotes(owner.clip(),owner.clipLength());selected.clear();
    for(size_t i=0;i<owner.clip().notes.size();++i){const auto& n=owner.clip().notes[i];for(const auto& old:wanted)if(n.pitch==old.pitch&&std::abs(n.start-old.start)<1./960.){selected.insert((int)i);break;}}
    owner.changed();previewSelection();
}
bool PianoRoll::verifyEditing(){
    const auto saved=owner.clip();const int savedTool=tool;const bool savedFlam=flamBrush;tool=0;flamBrush=false;owner.clip().notes.clear();fitNotes();
    const auto source=juce::Desktop::getInstance().getMainMouseSource();const auto time=juce::Time::getCurrentTime();
    auto event=[&](juce::Point<float> point){return juce::MouseEvent(source,point,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,this,this,time,point,time,1,false);};
    const float x=leftMargin()+(float)(2*pixelsPerBeat)+2;const float y=laneView()?58:noteY({2,.25,(lowNote+highNote)/2,100,0})+noteHeight()/2;
    mouseDown(event({x,y}));mouseUp(event({x,y}));bool ok=!owner.clip().notes.empty();
    if(ok&&!owner.audio.isPlaying()){ok=previewTrack==owner.selectedTrack&&!previewPitches.empty()&&isTimerRunning()&&ok;
        juce::AudioBuffer<float> sound(2,512);juce::AudioIODeviceCallbackContext context;float peak=0;
        for(int block=0;block<6;++block){owner.audio.audioDeviceIOCallbackWithContext(nullptr,0,sound.getArrayOfWritePointers(),2,512,context);peak=std::max(peak,sound.getMagnitude(0,512));}
        ok=peak>1.e-6f&&ok;stopPreview();ok=previewPitches.empty()&&!isTimerRunning()&&ok;
    }if(ok&&ownerTrackIsDrum())ok=std::abs(owner.clip().notes.front().length-std::min(defaultDrumNoteLength(owner.selectedTrack),owner.clipLength()-owner.clip().notes.front().start))<1e-9;
    if(ok){const auto n=owner.clip().notes.front();auto at=juce::Point<float>(leftMargin()+(float)(n.start*pixelsPerBeat),getHeight()-12-velocityHeight()*.5f);mouseDown(event(at));mouseUp(event(at));ok=owner.clip().notes.front().velocity>=60&&owner.clip().notes.front().velocity<=66&&ok;
        const auto beforeMove=owner.clip().notes.front();at={leftMargin()+(float)(beforeMove.start*pixelsPerBeat)+2,noteY(beforeMove)+noteHeight()/2};mouseDown(event(at));mouseDrag(event(at.translated((float)pixelsPerBeat,0)));mouseUp(event(at.translated((float)pixelsPerBeat,0)));ok=std::abs(owner.clip().notes.front().start-beforeMove.start-1)<1./960.&&ok;
        if(!laneView()||ownerTrackIsDrum()){const auto beforeResize=owner.clip().notes.front();at={leftMargin()+(float)((beforeResize.start+beforeResize.length)*pixelsPerBeat)-1,noteY(beforeResize)+noteHeight()/2};mouseDown(event(at));mouseDrag(event(at.translated((float)(grid*pixelsPerBeat),0)));mouseUp(event(at.translated((float)(grid*pixelsPerBeat),0)));ok=owner.clip().notes.front().length>beforeResize.length&&ok;}
        deleteSelected();ok=owner.clip().notes.empty()&&ok;
    }
    if(!laneView()){
        // Audition each new pitch before release, without retriggering within a row.
        const auto undos=owner.undoStack,redos=owner.redoStack;owner.undoStack.clear();owner.redoStack.clear();
        owner.clip().notes={{1,.5,60,100,0},{2,.5,64,100,0}};selected={0,1};fitNotes();
        auto start=juce::Point<float>(leftMargin()+(float)pixelsPerBeat+2,noteY(owner.clip().notes[0])+noteHeight()/2);
        juce::AudioBuffer<float> sound(2,512);juce::AudioIODeviceCallbackContext context;
        auto drain=[&]{owner.audio.audioDeviceIOCallbackWithContext(nullptr,0,sound.getArrayOfWritePointers(),2,512,context);};
        auto heard=[&](int a,int b){const auto active=owner.audio.activeNotes(owner.selectedTrack);return previewTrack==owner.selectedTrack&&previewPitches==std::vector<int>{a,b}&&active[(size_t)a]&&active[(size_t)b];};
        mouseDown(event(start));drain();
        mouseDrag(event(start.translated(0,-rowHeight())));drain();
        auto active=owner.audio.activeNotes(owner.selectedTrack);ok=heard(61,65)&&!active[60]&&!active[64]&&owner.undoStack.empty()&&ok;
        const auto played=owner.audio.noteOnsSent(owner.selectedTrack);mouseDrag(event(start.translated(0,-rowHeight())));drain();
        ok=owner.audio.noteOnsSent(owner.selectedTrack)==played&&ok;
        mouseDrag(event(start.translated(0,-2*rowHeight())));drain();active=owner.audio.activeNotes(owner.selectedTrack);
        ok=heard(62,66)&&!active[61]&&!active[65]&&owner.undoStack.empty()&&ok;
        mouseUp(event(start.translated(0,-2*rowHeight())));stopPreview();drain();active=owner.audio.activeNotes(owner.selectedTrack);
        ok=owner.undoStack.size()==1&&!active[62]&&!active[66]&&previewPitches.empty()&&ok;
        owner.undoStack=undos;owner.redoStack=redos;
    }
    if(scaleGuideAvailable()){
        const int key=owner.project.settings.key,scale=owner.project.settings.scale;const bool guide=showScaleGuide;
        const auto preference=Localisation::setting("pianoScaleGuide","0");const auto notes=owner.clip().notes;
        owner.project.settings.key=0;owner.project.settings.scale=0;owner.clip().notes.clear();showScaleGuide=false;
        const auto off=createComponentSnapshot(getLocalBounds());
        const auto json=juce::JSON::toString(projectJson(owner.project),false,17);const auto history=owner.undoStack.size();
        owner.scaleGuideButton.onClick();const auto on=createComponentSnapshot(getLocalBounds());
        auto pixel=[&](const juce::Image& image,int note){return image.getPixelAt(leftMargin()+10,(int)(noteY({0,.25,note,100,0})+noteHeight()/2));};
        ok=showScaleGuide&&owner.scaleGuideButton.getToggleState()&&Localisation::setting("pianoScaleGuide","0")=="1"&&pixel(off,60)!=pixel(on,60)&&pixel(off,61)==pixel(on,61)&&ok;
        ok=noteInScale(60)&&noteInScale(64)&&!noteInScale(61)&&!noteInScale(66)&&ok;
        owner.project.settings.key=2;ok=noteInScale(61)&&noteInScale(66)&&!noteInScale(60)&&!noteInScale(65)&&ok;
        owner.project.settings.scale=1;ok=noteInScale(60)&&noteInScale(65)&&!noteInScale(61)&&!noteInScale(66)&&ok;
        owner.project.settings.key=0;owner.project.settings.scale=0;owner.scaleGuideButton.onClick();
        ok=!showScaleGuide&&!owner.scaleGuideButton.getToggleState()&&owner.undoStack.size()==history&&json==juce::JSON::toString(projectJson(owner.project),false,17)&&ok;
        owner.project.settings.key=key;owner.project.settings.scale=scale;owner.clip().notes=notes;showScaleGuide=guide;
        owner.scaleGuideButton.setToggleState(guide,juce::dontSendNotification);Localisation::setSetting("pianoScaleGuide",preference);
    }
    if(!laneView()){
        owner.clip().notes={{1,.5,60,100,0},{2,.5,64,100,0}};selected={0,1};fitNotes();
        auto at=juce::Point<float>(leftMargin()+(float)pixelsPerBeat+2,noteY(owner.clip().notes[0])+noteHeight()/2);
        mouseDown(event(at));mouseDrag(event(at.translated((float)(-4*pixelsPerBeat),0)));mouseUp(event(at.translated((float)(-4*pixelsPerBeat),0)));
        ok=ok&&owner.clip().notes.size()==2&&std::abs(owner.clip().notes[0].start)<1e-9&&std::abs(owner.clip().notes[1].start-1)<1e-9;
    }
    if(owner.selectedTrack==3){
        const auto harmony=owner.project.settings;const bool noteView=guitarNotes;guitarNotes=false;
        struct ChordCase{int scale,degree,extension,root;unsigned expected;};
        for(const auto& c:{ChordCase{6,0,1,0,(1U<<0)|(1U<<3)|(1U<<7)|(1U<<11)},ChordCase{6,4,1,7,(1U<<0)|(1U<<4)|(1U<<7)|(1U<<10)},ChordCase{6,4,2,7,(1U<<0)|(1U<<1)|(1U<<4)|(1U<<7)|(1U<<10)}}){
            auto& settings=owner.project.settings;settings.key=0;settings.scale=c.scale;settings.extension=c.extension;settings.chordCount=1;settings.degrees[0]=c.degree;settings.freeChords[0]=-1;owner.clip().notes.clear();
            auto at=juce::Point<float>(leftMargin()+2,58);mouseDown(event(at));mouseUp(event(at));unsigned actual=0;for(const auto& note:owner.clip().notes)actual|=1U<<((note.pitch-c.root+120)%12);
            const unsigned colour=c.degree==0?1U<<11:(1U<<10)|(c.extension==2?1U<<1:0U);
            ok=ok&&actual!=0&&(actual&~c.expected)==0&&(actual&colour)==colour;stopPreview();
        }
        owner.project.settings=harmony;guitarNotes=noteView;
    }
    const auto notesBeforeSeek=owner.clip().notes;
    mouseDown(event({leftMargin()+(float)(2.375*pixelsPerBeat),24}));
    ok=ok&&std::abs(owner.audio.position()-owner.clipStart()-2.375)<1e-6&&owner.clip().notes.size()==notesBeforeSeek.size();
    stopPreview();owner.clip()=saved;tool=savedTool;flamBrush=savedFlam;clearSelection();owner.changed();fitNotes();return ok;
}
void PianoRoll::deleteSelected(){if(!showActiveNotes||selected.empty())return;owner.checkpoint();auto& notes=owner.clip().notes;for(auto i=selected.rbegin();i!=selected.rend();++i)if(*i>=0&&*i<(int)notes.size())notes.erase(notes.begin()+*i);selected.clear();owner.changed();}
bool PianoRoll::keyPressed(const juce::KeyPress& key){if(owner.editingRestricted()||!showActiveNotes)return owner.keyPressed(key);if(editKeys(key))return true;if(key==juce::KeyPress::deleteKey||key==juce::KeyPress::backspaceKey){deleteSelected();return true;}if(key.getModifiers().isCommandDown()&&key.getKeyCode()=='C'){owner.copyButton.onClick();return true;}if(key.getModifiers().isCommandDown()&&key.getKeyCode()=='V'){owner.pasteButton.onClick();return true;}if(key.getModifiers().isCommandDown()&&key.getKeyCode()=='A'){selected.clear();for(size_t i=0;i<owner.clip().notes.size();++i)selected.insert((int)i);repaint();return true;}return owner.keyPressed(key);}

bool MainComponent::instrumentIsOpen()const{return instrumentWindow&&instrumentWindow->isVisible();}
void MainComponent::deleteMidiTrack(int track){
    if(loadingPlugin||!isManualTrack(track)||track>=(int)project.tracks.size())return;
    instrumentPanel.flushRender();trackInspector.finishNameEdit();checkpoint();closePluginEditor();
    if(!project.deleteMidiTrack(track))return;
    selectedTrack=std::clamp(selectedTrack>track?selectedTrack-1:selectedTrack,0,(int)project.tracks.size()-1);
    selectedClip=-1;noClipSelection=false;arrangement.selectedClips.clear();piano.clearSelection();
    for(int ti=track;ti<maxTracks-1;++ti){arrangement.trackHeights[(size_t)ti]=arrangement.trackHeights[(size_t)ti+1];arrangement.volumeVisible[(size_t)ti]=arrangement.volumeVisible[(size_t)ti+1];arrangement.laneTarget[(size_t)ti]=arrangement.laneTarget[(size_t)ti+1];}
    arrangement.trackHeights.back()=40;arrangement.volumeVisible.back()=false;arrangement.laneTarget.back()=0;
    soundBoxTrack=-1;restoreInstruments();select(selectedTrack,selectedSection);changed();
    if(instrumentIsOpen()){instrumentPanel.rebuild();instrumentWindow->refresh();}
}
void MainComponent::showAddTrackMenu(){
    if(editingRestricted()||loadingPlugin||project.tracks.size()>=maxTracks)return;
    juce::PopupMenu menu;menu.addItem(1,tr("Add MIDI track"));menu.addItem(2,tr("VOCAL CHOPS"));menu.addItem(3,"Vocal Studio (English)");
    juce::Component::SafePointer<MainComponent> safe(this);menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&addTrackButton),[safe](int id){if(!safe||id<=0)return;safe->addMidiTrack(id==2);if(id==3)safe->showVocalStudio();});
}
void MainComponent::addMidiTrack(bool vocal){
    if(editingRestricted()||loadingPlugin||project.tracks.size()>=maxTracks)return;
    instrumentPanel.flushRender();checkpoint();const int track=vocal?project.addVocalChopsTrack():project.addMidiTrack();if(track<0)return;
    select(track,0);changed();showEditor();if(vocal)message(tr("Load a voice sample, then shape its slices in Studio. REGENERATE writes another sparse vocal phrase."));
}
void MainComponent::openHostedEditor(int track,int slot){
    if(loadingPlugin||editingRestricted()||!validEffectTrack(track))return;
    auto* plugin=slot==0?audio.plugin(track):audio.effectPlugin(track,slot);if(!plugin)return;const auto session=effectTrack(track).sessionId;
    pluginWindow=nullptr;for(auto& window:pluginWindows)if(window->session==session&&window->slot==slot){pluginWindow=window.get();break;}
    if(!pluginWindow){auto window=std::make_unique<PluginWindow>(*this,*plugin,session,slot,track);pluginWindow=window.get();pluginWindows.push_back(std::move(window));}
    else if(!pluginWindow->isVisible())pluginWindow->showCentred();
    pluginWindow->setName(plugin->getName());if(slot>0)if(auto* fx=effectTrack(track).effect(slot))if(!fx->name.empty())pluginWindow->setName(utf(fx->name)+juce::String::fromUTF8(" \xc2\xb7 ")+plugin->getName());
    lastActivePluginSession=session;lastActivePluginSlot=slot;pluginWindow->setAlwaysOnTop(true);pluginWindow->toFront(true);
}
void MainComponent::captureUndoPluginStates(){
    // Native editors can change opaque state without notifying JUCE mouse/parameter listeners.
    // Refresh the most recently active visible editor; closed/unrelated instances keep their cache.
    for(const auto& window:pluginWindows)if(window->isVisible()&&window->session==lastActivePluginSession&&window->slot==lastActivePluginSlot){window->markState();break;}
    audio.capturePluginStates(project);
}
bool MainComponent::hasOpenPluginEditors()const{for(const auto& window:pluginWindows)if(window->isVisible())return true;return false;}


namespace {
void restoreFocusAfterHide(juce::Component::SafePointer<juce::Component> target,juce::Component::SafePointer<juce::Component> closed,juce::Time mouseDown,int remaining){
    if(!target||!target->isShowing()||!target->isEnabled()||target->hasKeyboardFocus(false)||(closed&&closed->isShowing())||target->isCurrentlyBlockedByAnotherModalComponent()||remaining<=0)return;
    // A subsequent click or reopened tool cancels the request. Wait for native
    // activation without bringing an application back from the background.
    if(juce::Desktop::getInstance().getMainMouseSource().getLastMouseDownTime()!=mouseDown)return;
    // Windows can retain native thread focus while another process is foreground.
    // Reconcile JUCE focus on that already-focused peer without raising a window.
    const bool foreground=juce::Process::isForegroundProcess();
    if(auto* peer=target->getPeer();peer&&peer->isFocused()){
        if(foreground||juce::Component::getCurrentlyFocusedComponent()==nullptr){target->grabKeyboardFocus();if(target->hasKeyboardFocus(false))return;}
    }else if(foreground)target->getTopLevelComponent()->toFront(true);
    juce::Timer::callAfterDelay(50,[target,closed,mouseDown,remaining]{restoreFocusAfterHide(target,closed,mouseDown,remaining-1);});
}
}
void MainComponent::dismissAuxiliaryWindow(juce::Component& window){
    window.setVisible(false);
    auto release=[safe=juce::Component::SafePointer<juce::Component>(&window)]{
        if(!safe||safe->isShowing())return;
        if(auto* peer=safe->getPeer()){
           #if JUCE_WINDOWS
            const auto native=static_cast<HWND>(peer->getNativeHandle());const auto captured=::GetCapture();
            if(captured&&(captured==native||::IsChild(native,captured)))::ReleaseCapture();
           #endif
            auto source=juce::Desktop::getInstance().getMainMouseSource();
            const auto modifiers=physicalMouseModifiers();
            auto* pressed=source.getComponentUnderMouse();
            if(source.isDragging()&&!modifiers.isAnyMouseButtonDown()&&pressed&&(pressed==safe.getComponent()||safe->isParentOf(pressed))){
                // Cancel a missed release without invoking a hidden button's action.
                const bool enabled=safe->isEnabled();safe->setEnabled(false);
                peer->handleMouseEvent(source.getType(),peer->globalToLocal(source.getRawScreenPosition()),modifiers,juce::MouseInputSource::defaultPressure,0,juce::Time::currentTimeMillis());
                if(safe)safe->setEnabled(enabled);
            }
        }
        juce::Desktop::getInstance().getMainMouseSource().triggerFakeMove();
    };
    release();juce::MessageManager::callAsync(release);
    if(isShowing()&&isEnabled()){
        getTopLevelComponent()->toFront(true);grabKeyboardFocus();
        // Native focus-loss notifications may follow the synchronous grab.
        const auto mouseDown=juce::Desktop::getInstance().getMainMouseSource().getLastMouseDownTime();
        juce::Component::SafePointer<juce::Component> target(this),closed(&window);
        juce::Timer::callAfterDelay(100,[target,closed,mouseDown]{restoreFocusAfterHide(target,closed,mouseDown,20);});
    }
}
void MainComponent::closePluginEditor(int track,int slot){
    const auto session=validEffectTrack(track)?effectTrack(track).sessionId:0;
    pluginWindows.erase(std::remove_if(pluginWindows.begin(),pluginWindows.end(),[&](const auto& window){return track<0||(window->session==session&&(slot<0||window->slot==slot));}),pluginWindows.end());
    pluginWindow=pluginWindows.empty()?nullptr:pluginWindows.back().get();
}
void MainComponent::showAiComposer(){if(loadingPlugin)return;if(!aiWindow)aiWindow=std::make_unique<AiComposerWindow>(*this);aiWindow->show();}
juce::Result MainComponent::captureAiProject(Project& base){if(editingRestricted())return juce::Result::fail("Finish recording or wait for the media operation before editing the project.");if(loadingPlugin)return juce::Result::fail("Wait for the instrument to finish loading.");instrumentPanel.flushRender();trackInspector.finishNameEdit();captureProjectStates(audio,project,hasOpenPluginEditors());base=project;return juce::Result::ok();}
juce::Result MainComponent::applyAiComposition(const Project& base,const Project& generated,bool listen){
    if(editingRestricted())return juce::Result::fail("Finish recording or wait for the media operation before editing the project.");
    if(loadingPlugin)return juce::Result::fail("Wait for the instrument to finish loading.");
    auto musical=[](Project p){for(auto& t:p.tracks){t.instrumentState.clear();for(auto& fx:t.effects)fx.state.clear();}return juce::JSON::toString(projectJson(p),false,17);};
    if(generated.tracks.size()!=project.tracks.size()||generated.sections.empty())return juce::Result::fail("The composition is incomplete.");
    if(musical(project)!=musical(base))return juce::Result::fail("The project changed while AI was composing. Generate again to include your latest edits.");
    instrumentPanel.flushRender();checkpoint();audio.stop();auto next=generated;
    for(size_t i=0;i<next.tracks.size()&&i<project.tracks.size();++i){next.tracks[i].instrumentState=project.tracks[i].instrumentState;for(auto& fx:next.tracks[i].effects)if(auto* old=project.tracks[i].effect(fx.slot))fx.state=old->state;}
    project=std::move(next);selectedTrack=std::clamp(selectedTrack,0,(int)project.tracks.size()-1);selectedSection=0;selectedClip=-1;noClipSelection=false;editCursor=-1;selectionBeat=0;arrangement.selectedClips.clear();piano.clearSelection();changed();showArrangement();fitGeneratedView();if(instrumentIsOpen())instrumentPanel.rebuild();if(listen)audio.play(true);return juce::Result::ok();
}
void MainComponent::fitGeneratedView(){
    piano.fitNotes();
    if(page==Page::editor){
        piano.pixelsPerBeat=std::clamp((pianoPort.getWidth()-piano.leftMargin()-32.)/std::max(.25,clipLength()),2.,160.);
        zoomSlider.setValue(piano.pixelsPerBeat,juce::dontSendNotification);updateSizes();pianoPort.setViewPosition(0,0);piano.repaint();
    }else if(page==Page::arrangement)fitSong();
}
void MainComponent::fitSong(){
    const double width=std::max(80,arrangementPort.getWidth()-headerWidth-38);
    arrangement.pixelsPerBar=std::clamp(width/std::max(1.,project.length()/4),.05,160.);
    zoomSlider.setValue(arrangement.pixelsPerBar,juce::dontSendNotification);updateSizes();arrangementPort.setViewPosition(0,arrangementPort.getViewPositionY());arrangement.repaint();
}
void MainComponent::setLoopRange(double from,double to){project.setLoopRegion(from,to);changed(false);}
void MainComponent::toggleLoop(){checkpoint();project.toggleLoop();changed(false);}
void MainComponent::clearLoop(){if(!project.loop&&!project.hasLoopRegion())return;checkpoint();project.clearLoop();changed(false);}
bool MainComponent::clipLoopActive()const{const int index=clipIndex();if(!project.loop||index<0||index>=(int)project.tracks[(size_t)selectedTrack].clips.size())return false;return project.loopMatches(clipStart(),clipStart()+clipLength());}
void MainComponent::toggleClipLoop(){const int index=clipIndex();if(index<0||index>=(int)project.tracks[(size_t)selectedTrack].clips.size())return;checkpoint();project.toggleLoopRegion(clipStart(),clipStart()+clipLength());changed(false);}
juce::String MainComponent::soundSourceName(int track){
    if(track<0||track>=(int)project.tracks.size())return {};
    const auto& t=project.tracks[(size_t)track];const auto path=utf(t.instrumentPath);
    auto fileName=[](const juce::String& s){return juce::File::isAbsolutePath(s)?juce::File(s).getFileNameWithoutExtension():s;};
    if(t.instrument=="sample")return path.isEmpty()?tr("SAMPLE"):fileName(path);
    if(t.instrument=="plugin"){if(auto* instrument=audio.plugin(track))return instrument->getName();const auto name=path.isNotEmpty()?fileName(path):utf(t.instrumentId);return loadingPlugin?name:tr("Missing instrument")+(name.isNotEmpty()?": "+name:juce::String());}
    return builtInSoundName(track,std::clamp(t.sound,0,builtInSoundCount(track)-1));
}
juce::String MainComponent::sourceWithEffect(int track){
    auto source=soundSourceName(track);if(track<0||track>=(int)project.tracks.size())return source;
    const auto& effects=project.tracks[(size_t)track].effects;if(effects.size()==1)source+=" + "+utf(effects.front().displayName());else if(!effects.empty())source+=" + "+juce::String((int)effects.size())+" FX";
    return source;
}
// The arrangement repaints its headers on every playback tick: their source names are kept
// until refreshTrack() (every edit, selection, language or plugin change) clears them.
const juce::String& MainComponent::cachedSourceName(int track){
    const auto i=(size_t)std::clamp(track,0,(int)project.tracks.size()-1);if(!sourceNameCached[i]){sourceNames[i]=soundSourceName(track);sourceNameCached[i]=true;}return sourceNames[i];
}
void MainComponent::trackGesture(juce::Slider& s,std::function<void()> onEnd,bool mix){
    s.onDragStart=[this,&s,mix]{s.getProperties().set("fgaGesture",true);if(mix)checkpointMix();else checkpoint();};
    s.onDragEnd=[&s,onEnd]{s.getProperties().set("fgaGesture",false);if(onEnd)onEnd();};
}
void MainComponent::queueGlobalFeel(juce::Slider& control,unsigned parameter){
    pendingGlobalFeel|=parameter;dirty=true;
    if(!inGesture(control))flushGlobalFeel();
}
void MainComponent::flushGlobalFeel(){
    const auto parameters=std::exchange(pendingGlobalFeel,0u);if(!parameters)return;
    instrumentPanel.flushRender();applyGlobalFeel(project,parameters);piano.clearSelection();changed();
    if(instrumentIsOpen())instrumentPanel.rebuildAsync();
}
void MainComponent::useBuiltInSound(int track){
    if(loadingPlugin||track<0||track>=(int)project.tracks.size())return;checkpoint();closePluginEditor(track,0);audio.useInternal(track);
    auto& t=project.tracks[(size_t)track];t.instrument="internal";t.instrumentPath.clear();t.instrumentId.clear();t.instrumentState.clear();changed(false);
}
// LOAD SAMPLE opens the integrated browser; its LOAD targets the selected track.
void MainComponent::browseSamplesFor(int track){
    if(loadingPlugin){message(tr("Wait for the instrument to finish loading."),true);return;}
    // Pressed again for the same track while the browser is open: it closes (a toggle).
    if(track==selectedTrack&&browserVisible&&page==Page::arrangement){showSampleBrowser(false);return;}
    if(track!=selectedTrack)select(track,selectedSection);
    if(!browserVisible||page!=Page::arrangement)showSampleBrowser(true);
    sampleBrowser.refreshTarget();sampleBrowser.focusList();
    message(tr("Choose a sample in the browser: double-click it, press LOAD or drag it onto the track."));
}
void MainComponent::toggleTrackLock(){checkpoint();auto& t=project.tracks[(size_t)selectedTrack];t.locked=!t.locked;changed(false);}
void MainComponent::toggleClipLock(){const int index=clipIndex();if(index<0||index>=(int)project.tracks[(size_t)selectedTrack].clips.size())return;checkpoint();clip().locked=!clip().locked;changed(false);}
void MainComponent::regenerateEditedClip(){
    if(loadingPlugin)return;
    flushGlobalFeel();
    ensureTimelineClip();
    const int index=clipIndex();if(index<0||index>=(int)project.tracks[(size_t)selectedTrack].clips.size())return;
    if(project.tracks[(size_t)selectedTrack].locked||clip().locked){message(tr("This clip is locked: unlock it to regenerate it."),true);return;}
    checkpoint();
    auto seed=(std::uint32_t)juce::Random::getSystemRandom().nextInt();if(!seed)seed=1;
    regenerateClip(project,selectedTrack,index,seed);
    piano.clearSelection();changed();if(instrumentIsOpen())instrumentPanel.rebuildAsync();fitGeneratedView();message(tr("Clip regenerated from the instrument settings. Undo restores the previous notes."));
}
void MainComponent::applyInstrumentSettings(int track,bool harmony){
    applyInstrumentChange(project,track,harmony);piano.clearSelection();changed(true,harmony?-1:track);
}
// Plugin states are part of the snapshot, so undoing an instrument change brings back its sound.
bool MainComponent::editingRestricted()const{return mediaBusy||audio.isRecording()||audio.isAudioRecording()||audio.isExporting();}
void MainComponent::updateEditingAvailability(){
    if(availabilityUpdating||!isEnabled())return;juce::ScopedValueSetter<bool> updating(availabilityUpdating,true);
    if(editingRestricted()){
        if(restrictedComponents.empty())for(auto* child:getChildren()){
            if(child==&stopButton||child==&playButton||child==&studioButton||child==&positionLabel||child==&positionModeButton||child==&statusLabel)continue;
            restrictedComponents.emplace_back(child,child->isEnabled());
        }
        auto restrictWindow=[&](juce::Component* component){if(component&&std::none_of(restrictedComponents.begin(),restrictedComponents.end(),[component](const auto& e){return e.first.getComponent()==component;}))restrictedComponents.emplace_back(component,component->isEnabled());};
        restrictWindow(instrumentWindow.get());restrictWindow(musicalToolWindow.getComponent());restrictWindow(aiWindow.get());
        if(mediaBusy){restrictWindow(mixerRoutingWindow.get());for(auto& window:pluginWindows)restrictWindow(window.get());}
        for(auto& entry:restrictedComponents)if(entry.first)entry.first->setEnabled(false);
    }else if(!restrictedComponents.empty()){
        auto saved=std::move(restrictedComponents);restrictedComponents.clear();for(auto& entry:saved)if(entry.first)entry.first->setEnabled(entry.second);refreshControls();
    }
}
void MainComponent::stopPlaybackAndRecording(){audio.stop();if(studioWindow&&(audio.isRecording()||audio.isAudioRecording()))studioWindow->finishRecording();updateEditingAvailability();}
void MainComponent::checkpoint(){lastCheckpointControl=nullptr;if(!loadingPlugin)captureUndoPluginStates();undoStack.push_back({project,false});if(undoStack.size()>40)undoStack.pop_front();redoStack.clear();}
void MainComponent::checkpointMix(const void* control){const auto now=juce::Time::getMillisecondCounter();if(!undoStack.empty()&&control!=nullptr&&control==lastCheckpointControl&&now-lastCheckpointTime<500){lastCheckpointTime=now;return;}lastCheckpointControl=control;lastCheckpointTime=now;if(!loadingPlugin)audio.reusePluginStates(project);undoStack.push_back({project,true});if(undoStack.size()>40)undoStack.pop_front();redoStack.clear();}
// Wheel and keyboard steps on one control within half a second share one undo step.
void MainComponent::checkpoint(const void* control){
    const auto now=juce::Time::getMillisecondCounter();
    if(!undoStack.empty()&&control!=nullptr&&control==lastCheckpointControl&&now-lastCheckpointTime<500){lastCheckpointTime=now;return;}
    checkpoint();lastCheckpointControl=control;lastCheckpointTime=now;
}
void MainComponent::mixChanged(){dirty=true;audio.updateMix(project);refreshControls();refreshGraphicEq();arrangement.repaint();mixer.repaint();repaintHeader();}
// Only the window header (display legends, page title) and the status bar (project name and
// length) are painted by the window itself.
void MainComponent::repaintHeader(){repaint(0,0,getWidth(),toolbarBottom()+1);repaint(0,getHeight()-statusHeight,getWidth(),statusHeight);}
void MainComponent::changed(bool schedule,int onlyTrack){audio.invalidateFrozenTempo(project.settings.bpm);audio.previewKey=project.settings.key;project.normalise();songLengthCache=-1;chordNameCacheReady=false;dirty=true;audio.setLoop(project.loop,project.loopStart,project.loopEnd);if(schedule)audio.publish(project,true,onlyTrack);else audio.updateMix(project);refreshControls();arrangement.repaint();piano.repaint();repaintHeader();}
// Derived data asked for by paint() and the 20 Hz timer: recomputed only when the
// harmony settings (chord names) or the form (length) change.
const std::vector<std::string>& MainComponent::cachedChordNames(){
    auto s=project.settings;if(selectedSection>=0&&selectedSection<(int)project.sections.size()){const auto& h=project.sections[(size_t)selectedSection].harmony;if(h.enabled){if(h.key>=0)s.key=h.key;if(h.scale>=0)s.scale=h.scale;s.chordCount=h.chordCount;s.chordSteps=h.chordSteps;s.degrees=h.degrees;s.freeChords=h.freeChords;s.manualChords=h.manualChords;}}
    std::uint32_t key=2166136261u;
    auto fold=[&key](int value){key=(key^(std::uint32_t)value)*16777619u;};
    fold((int)s.seed);fold(s.key);fold(s.scale);fold(s.extension);fold(s.chordSteps);fold(s.chordCount);
    for(int i=0;i<s.chordCount&&i<8;++i){fold(s.degrees[(size_t)i]);fold(s.freeChords[(size_t)i]+2);}
    if(!chordNameCacheReady||key!=chordNameCacheKey){chordNameCache=chordNames(s);chordNameCacheKey=key;chordNameCacheReady=true;}
    return chordNameCache;
}
double MainComponent::songLength(){if(songLengthCache<0)songLengthCache=project.length();return songLengthCache;}
void MainComponent::applyLanguage(){    if(Localisation::tipsEnabled())tooltips=std::make_unique<juce::TooltipWindow>(this,650);else tooltips.reset();
    refreshControls();if(instrumentIsOpen())instrumentPanel.rebuild();
    if(aiWindow)aiWindow->refreshText();
    options.refreshText();pluginSettings.refreshText();pluginBrowser.refreshText();updateOverlay.refreshText();translateControls(*this);repaint();
}
void MainComponent::showArrangement(){page=Page::arrangement;zoomSlider.setValue(arrangement.pixelsPerBar,juce::dontSendNotification);resized();select(selectedTrack,selectedSection);repaint();}
void MainComponent::openInstrumentFor(int track){
    if(track<0||track>=(int)project.tracks.size())return;
    select(track,selectedSection);
    if(audio.plugin(track)){openHostedEditor(track,0);return;}
    if(project.tracks[(size_t)track].instrument=="sample"){showStudio(0);return;}
    showInstrument();
}
void MainComponent::showInstrument(){
    instrumentPanel.flushRender();instrumentPanel.setSize(1180,instrumentPanel.getHeight());instrumentPanel.rebuild();
    if(!instrumentWindow)instrumentWindow=std::make_unique<InstrumentWindow>(*this);
    instrumentWindow->refresh();
    if(!instrumentWindow->isVisible())instrumentWindow->centreWithSize(instrumentWindow->getWidth(),instrumentWindow->getHeight());
    instrumentWindow->fitToContent();
    instrumentWindow->setAlwaysOnTop(true);instrumentWindow->setVisible(true);instrumentPort.setVisible(true);instrumentPort.setViewPosition(0,0);instrumentWindow->toFront(true);
}
void MainComponent::showEditor(){const bool entering=page!=Page::editor;ensureTimelineClip();page=Page::editor;if(entering){piano.tool=1;selectTool.setToggleState(true,juce::dontSendNotification);}zoomSlider.setValue(piano.pixelsPerBeat,juce::dontSendNotification);resized();select(selectedTrack,selectedSection);piano.grabKeyboardFocus();repaint();}
void MainComponent::updatePianoEditingState(){
    const bool editable=piano.showActiveNotes&&!editingRestricted();
    for(juce::Component* c:std::initializer_list<juce::Component*>{&drawTool,&selectTool,&eraseTool,&brushAccent,&brushHit,&brushGhost,&brushFlam,&quantiseButton,&transposeUp,&transposeDown,&deleteNotes,&copyButton,&pasteButton,&importButton,&velocitySlider,&piano.expressionLane})c->setEnabled(editable);
    regenerateClipButton.setEnabled(editable&&hasGenerator(project.tracks[(size_t)selectedTrack]));
}
void MainComponent::select(int track,int section){++uiRevision;instrumentPanel.flushRender();trackInspector.finishNameEdit();const int previousTrack=selectedTrack,previousSection=selectedSection;selectedTrack=std::clamp(track,0,(int)project.tracks.size()-1);selectedSection=std::clamp(section,0,(int)project.sections.size()-1);if(previousTrack!=selectedTrack||previousSection!=selectedSection){if(!automationAllTracks&&std::any_of(arrangement.volumeVisible.begin(),arrangement.volumeVisible.end(),[](bool on){return on;}))showAutomationScope(true);if(audio.isClipPlaying())audio.play(false);piano.stopPreview();noClipSelection=false;selectedClip=project.clipAtBeat(selectedTrack,project.sectionStart(selectedSection));selectionBeat=project.sectionStart(selectedSection)+1./1920.;arrangement.selectedClips.clear();}piano.clearSelection();sectionSelector.setSelectedId(selectedSection+1,juce::dontSendNotification);refreshTrack();updateSizes();piano.fitNotes();pianoPort.setViewPosition(0,0);mixer.reveal(selectedTrack);
    if(page==Page::arrangement){const auto v=arrangementPort.getViewArea();const int y=arrangement.rowY(rowForTrack(selectedTrack));if(y<v.getY()+rulerHeight)arrangementPort.setViewPosition(v.getX(),std::max(0,y-rulerHeight));else if(y+arrangement.rowHeight(rowForTrack(selectedTrack))>v.getBottom())arrangementPort.setViewPosition(v.getX(),y+arrangement.rowHeight(rowForTrack(selectedTrack))-v.getHeight());}
    sampleBrowser.refreshTarget();if(instrumentIsOpen()&&(previousTrack!=selectedTrack||previousSection!=selectedSection)){instrumentPanel.rebuild();instrumentWindow->refresh();}arrangement.repaint();piano.repaint();}
void MainComponent::refreshControls(){
    ++uiRevision;
    refreshing=true;auto& s=project.settings;const auto& tempo=genreProfile(s.genre);tempoSlider.setRange(s.freeTempo?30:tempo.minBpm,s.freeTempo?300:tempo.maxBpm,.1);durationLabel.setText(juce::String((int)(project.length()*60/s.bpm)/60)+":"+juce::String((int)(project.length()*60/s.bpm)%60).paddedLeft('0',2),juce::dontSendNotification);refreshTargetDurationEditor();leadButton.setToggleState(s.guitarLead,juce::dontSendNotification);
    // The section list is rebuilt (and translated) only when the sections change.
    {juce::StringArray names;for(const auto& section:project.sections)names.add(utf(section.name));if(names!=sectionSelectorNames){sectionSelectorNames=names;sectionSelector.clear(juce::dontSendNotification);for(int i=0;i<names.size();++i)sectionSelector.addItem(names[i],i+1);translateControls(sectionSelector);}}
    const auto songHarmony=activeHarmony(project,selectedSection);
    for(int i=0;i<12&&keyBox.getNumItems()==12;++i){auto name=juce::String::fromUTF8(sgk::spelledNote(i,songHarmony.key,songHarmony.scale));if(keyBox.getItemText(i)!=name)keyBox.changeItemText(i+1,name);}
    for(int pc=0;pc<12;++pc)keyBox.changeItemText(pc+1,sgk::spelledNote(pc,songHarmony.key,songHarmony.scale));
    sectionSelector.setSelectedId(selectedSection+1,juce::dontSendNotification);genreBox.setSelectedId(s.genre+1,juce::dontSendNotification);keyBox.setSelectedId(songHarmony.key+1,juce::dontSendNotification);scaleBox.setSelectedId(songHarmony.scale+1,juce::dontSendNotification);extensionBox.setSelectedId(s.extension+1,juce::dontSendNotification);voicingBox.setSelectedId(s.voicing+1,juce::dontSendNotification);bassBox.setSelectedId(s.bassMode+1,juce::dontSendNotification);stringsBox.setSelectedId(s.stringsMode+1,juce::dontSendNotification);
    tempoSlider.setValue(s.bpm,juce::dontSendNotification);swingSlider.setValue(s.swing*100,juce::dontSendNotification);densitySlider.setValue(s.density*100,juce::dontSendNotification);humanizeSlider.setValue(s.humanize*100,juce::dontSendNotification);updateGainDisplay(masterSlider,project.masterGain,project.masterAutomation);loopButton.setToggleState(project.loop,juce::dontSendNotification);
    int matchedProgression=0;
    const auto& presets=sgk::progressionPresets();
    for(size_t candidate=0;candidate<=presets.size();++candidate){const int preferred=progressionBox.getSelectedItemIndex();const int index=candidate==0?preferred:(int)candidate-1;if(index<0||index>=(int)presets.size())continue;const size_t pi=(size_t)index;const auto& preset=presets[pi];if(preset.scale!=songHarmony.scale)continue;const auto slots=sgk::presetSlots(preset,s.extension);if((int)slots.size()!=songHarmony.chordCount)continue;bool same=true;
        for(int i=0;i<songHarmony.chordCount;++i){const auto& slot=slots[(size_t)i];same=same&&songHarmony.freeChords[(size_t)i]==slot.freeCode&&(slot.freeCode>=0||songHarmony.degrees[(size_t)i]==slot.degree);}if(same){matchedProgression=(int)pi+1;break;}}
    progressionBox.setSelectedId(matchedProgression,juce::dontSendNotification);
    juce::StringArray chords;for(const auto& name:cachedChordNames())chords.add(utf(name));const auto harmony=tr("HARMONY")+"   "+chords.joinIntoString("    /    ");chordsLabel.setText(harmony,juce::dontSendNotification);
    addTrackButton.setEnabled(project.tracks.size()<maxTracks&&!loadingPlugin);addSectionButton.setEnabled(project.sections.size()<64&&!loadingPlugin);
    undoButton.setEnabled(!undoStack.empty());redoButton.setEnabled(!redoStack.empty());showPosition(timelinePosition());refreshing=false;refreshTrack();if(instrumentIsOpen()){instrumentPanel.refreshFromProject();instrumentWindow->refresh();}updateSizes();updateEditingAvailability();
}
void MainComponent::refreshTrack(){++uiRevision;
    sourceNameCached.fill(false);
    if(!noClipSelection){if(project.tracks[(size_t)selectedTrack].freeTimeline)selectedClip=project.clipAtBeat(selectedTrack,selectionBeat);else selectionBeat=project.sectionStart(selectedSection)+1./1920.;}
    if(project.tracks[(size_t)selectedTrack].freeTimeline&&(selectedClip<0||selectedClip>=(int)project.tracks[(size_t)selectedTrack].clips.size())){selectedClip=-1;emptyClip=Clip{};}
    refreshing=true;const auto& t=project.tracks[(size_t)selectedTrack];updateGainDisplay(gainSlider,t.gain,t.volume);updatePanDisplay(panSlider,t.pan,t.panAutomation);muteButton.setToggleState(t.mute,juce::dontSendNotification);soloButton.setToggleState(t.solo,juce::dontSendNotification);lockButton.setToggleState(t.locked,juce::dontSendNotification);lockClipButton.setToggleState(clip().locked,juce::dontSendNotification);
    const int count=builtInSoundCount(selectedTrack);if(soundBoxTrack!=selectedTrack){soundBoxTrack=selectedTrack;soundBox.clear(juce::dontSendNotification);for(int i=0;i<count;++i)soundBox.addItem(builtInSoundName(selectedTrack,i),i+1);translateControls(soundBox);}soundBox.setSelectedId(std::min(t.sound+1,count),juce::dontSendNotification);soundBox.setEnabled(t.instrument=="internal"&&selectedTrack!=2);
    inspectorLabel.setText(utf(t.name).toUpperCase()+juce::String::fromUTF8("   ·   ")+sourceWithEffect(selectedTrack),juce::dontSendNotification);sectionLoopButton.setToggleState(clipLoopActive(),juce::dontSendNotification);editorLabel.setText(tr(piano.ownerTrackIsDrum()?"DRUM EDITOR":selectedTrack==3?"GUITAR EDITOR":"PIANO ROLL")+"   /   "+utf(t.name)+"   /   "+utf(project.sections[(size_t)selectedSection].name)+"   /   "+juce::String((int)clip().notes.size())+" notes",juce::dontSendNotification);pluginEditorButton.setEnabled(!loadingPlugin&&audio.plugin(selectedTrack)!=nullptr);sampleButton.setEnabled(!loadingPlugin);trackButton.setEnabled(hasGenerator(project.tracks[(size_t)selectedTrack]));clipButton.setEnabled(hasGenerator(project.tracks[(size_t)selectedTrack]));regenerateClipButton.setEnabled(hasGenerator(project.tracks[(size_t)selectedTrack]));
    scaleGuideButton.setVisible(page==Page::editor&&piano.scaleGuideAvailable());
    updatePianoEditingState();refreshing=false;trackInspector.refresh();mixer.refresh();
}
void MainComponent::updateSizes(){if(automationAllTracks&&std::any_of(arrangement.volumeVisible.begin(),arrangement.volumeVisible.end(),[](bool visible){return visible;}))for(size_t i=0;i<arrangement.volumeVisible.size();++i)arrangement.volumeVisible[i]=i<project.tracks.size();timelineZoom.setValue(arrangement.pixelsPerBar,juce::dontSendNotification);automationButton.setToggleState(arrangement.volumeVisible[(size_t)selectedTrack],juce::dontSendNotification);const double extent=std::max(project.length(),std::max(0.,targetBeat(project)));arrangement.setSize(std::max(arrangementPort.getWidth()-16,headerWidth+(int)((extent+32)/4*arrangement.pixelsPerBar)+20),arrangement.totalHeight());piano.setSize(piano.leftMargin()+(int)(clipLength()*piano.pixelsPerBeat)+20,std::max(selectedTrack==3&&!piano.guitarNotes?640:320,pianoPort.getHeight()-18));}
void MainComponent::undo(bool redo){cancelSampleImport();if(loadingPlugin)return;flushGlobalFeel();instrumentPanel.cancelRender();auto& source=redo?redoStack:undoStack;auto& dest=redo?undoStack:redoStack;if(source.empty())return;const int focus=clipIndex();const bool mixOnly=source.back().mixOnly;if(mixOnly)audio.reusePluginStates(project);else captureUndoPluginStates();dest.push_back({project,mixOnly});const auto& before=dest.back().project;project=std::move(source.back().project);source.pop_back();lastCheckpointControl=nullptr;project.normalise();selectedSection=std::min(selectedSection,(int)project.sections.size()-1);
    selectedTrack=std::clamp(selectedTrack,0,(int)project.tracks.size()-1);
    // Editing selection is independent of the playhead. A note, mix or loop Undo keeps
    // the selected clip when its timeline range is unchanged in the restored snapshot.
    const bool sameRange=project.tracks.size()==before.tracks.size()&&focus>=0&&focus<(int)project.tracks[(size_t)selectedTrack].clips.size()&&focus<(int)before.tracks[(size_t)selectedTrack].clips.size()&&project.clipStart(selectedTrack,focus)==before.clipStart(selectedTrack,focus)&&project.clipLength(selectedTrack,focus)==before.clipLength(selectedTrack,focus);
    selectedClip=sameRange?focus:project.clipAtBeat(selectedTrack,audio.position());selectionBeat=selectedClip>=0?project.clipStart(selectedTrack,selectedClip)+1./1920.:audio.position();noClipSelection=false;arrangement.selectedClips.clear();
    bool instrumentsChanged=project.tracks.size()!=before.tracks.size()||!sameEffectPlugins(project.masterEffects,before.masterEffects);for(size_t i=0;i<project.tracks.size();++i)if(i>=before.tracks.size()||project.tracks[i].instrument!=before.tracks[i].instrument||project.tracks[i].instrumentPath!=before.tracks[i].instrumentPath||project.tracks[i].instrumentId!=before.tracks[i].instrumentId||project.tracks[i].sampleRoot!=before.tracks[i].sampleRoot||!sameEffectPlugins(project.tracks[i],before.tracks[i]))instrumentsChanged=true;
    // Same instruments: bring each plugin back to the sound it had in that snapshot.
    if(mixOnly){for(size_t i=0;i<project.tracks.size()&&i<before.tracks.size();++i){project.tracks[i].instrumentState=before.tracks[i].instrumentState;for(auto& fx:project.tracks[i].effects)if(auto* old=before.tracks[i].effect(fx.slot))fx.state=old->state;}}
    if(instrumentsChanged)restoreInstruments();else if(!mixOnly)for(size_t i=0;i<project.tracks.size();++i){const auto& t=project.tracks[i];if(t.instrument=="plugin"&&!t.instrumentState.empty()&&t.instrumentState!=before.tracks[i].instrumentState)audio.restorePluginState((int)i,juce::String::fromUTF8(t.instrumentState.c_str()));for(const auto& fx:t.effects){auto* old=before.tracks[i].effect(fx.slot);if(!fx.state.empty()&&old&&fx.state!=old->state)audio.restoreEffectState((int)i,utf(fx.state),fx.slot);}}
    if(mixOnly){for(auto& fx:project.masterEffects.effects)if(auto* old=before.masterEffects.effect(fx.slot))fx.state=old->state;}
    else if(!instrumentsChanged)for(const auto& fx:project.masterEffects.effects){auto* old=before.masterEffects.effect(fx.slot);if(old&&old->state!=fx.state&&!fx.state.empty())audio.restoreEffectState(masterEffectTrack,utf(fx.state),fx.slot);}
    piano.clearSelection();changed();if(instrumentIsOpen())instrumentPanel.rebuild();}
void MainComponent::showGenerateMenu(){
    if(editingRestricted())return;
    juce::PopupMenu menu;
    // The main button already generates the song: the menu lists the other scopes.
    menu.addSectionHeader(tr("GENERATE SONG"));
    menu.addItem(2,tr("REGENERATE TRACK"),true,false);
    menu.addItem(3,tr("REGENERATE SECTION"),true,false);
    menu.addItem(4,tr("REGENERATE CLIP"),true,false);
    juce::Component::SafePointer<MainComponent> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&generateMenuButton).withMinimumWidth(generateMenuButton.getWidth()),
        [safe](int id){
            if(!safe||id==0)return;
            if(id==1)safe->regenerate(Scope::song);
            else if(id==2)safe->regenerate(Scope::track,safe->page!=Page::instrument);
            else if(id==3)safe->regenerate(Scope::section);
            else safe->regenerate(Scope::clip,safe->page!=Page::instrument);
        });
}
void MainComponent::regenerate(Scope scope,bool newTake){
    flushGlobalFeel();
    if(editingRestricted())return;
    if(loadingPlugin)return;
    const auto oldKey=project.settings.key;
    // Playback keeps running: the engine swaps in the new notes and restrikes only what changed.
    checkpoint();
    auto seed=newTake?(std::uint32_t)juce::Random::getSystemRandom().nextInt():0u;
    if(newTake&&seed==0)seed=1;
    if(scope==Scope::clip)ensureTimelineClip(false);
    generate(project,scope,selectedTrack,scope==Scope::clip?clipIndex():selectedSection,seed);
    // A new song may bring a new form: keep the selection inside it.
    if(scope==Scope::song){selectedSection=std::clamp(selectedSection,0,(int)project.sections.size()-1);selectedClip=project.clipAtBeat(selectedTrack,project.sectionStart(selectedSection));selectionBeat=project.sectionStart(selectedSection)+1./1920.;editCursor=-1;}
    arrangement.selectedClips.clear();piano.clearSelection();changed();fitGeneratedView();
    if(newTake&&instrumentIsOpen())instrumentPanel.rebuildAsync();
    if(newTake&&scope==Scope::song)
        message(tr(project.settings.key!=oldKey?"New key, tempo, form and patterns ready. Undo restores the previous song.":"New patterns ready. Locked harmonic parts keep the key and progression. Unlock them for a new harmony."));
    else message("New part ready. Locks were kept. Undo restores the previous notes.");
}
void MainComponent::modifySelected(std::function<void(Clip&)> action){
    if(page==Page::editor&&!piano.showActiveNotes)return;
    if(editingRestricted()||clipIndex()<0)return;
    const bool hadSelection=!piano.selected.empty();Clip part;
    if(hadSelection)for(int i:piano.selected)if(juce::isPositiveAndBelow(i,(int)clip().notes.size()))part.notes.push_back(clip().notes[(size_t)i]);
    if(hadSelection&&part.notes.empty())return;
    checkpoint();if(!hadSelection)action(clip());else{action(part);size_t k=0;for(int i:piano.selected)if(juce::isPositiveAndBelow(i,(int)clip().notes.size()))clip().notes[(size_t)i]=part.notes[k++];}
    const bool audition=audio.isSelectedNotePlayback();tidyNotes(clip(),clipLength());changed();if(hadSelection)piano.retainSelection(part.notes);
    if(audition&&hadSelection){std::vector<Note> notes;for(int index:piano.selected)if(juce::isPositiveAndBelow(index,(int)clip().notes.size()))notes.push_back(clip().notes[(size_t)index]);audio.playSelectedNotes(selectedTrack,notes,clipStart(),project.loop,false);}
}
void MainComponent::paint(juce::Graphics& g){
    const int w=getWidth(),h=getHeight();
    g.fillAll(Daw::base);
    // Control bar: logo and file actions, the transport with its display, master and panels.
    Daw::fillElevatedSurface(g,{0.f,0.f,(float)w,(float)controlBarHeight},Daw::surface);
    Daw::fillElevatedSurface(g,{0.f,(float)controlBarHeight,(float)w,(float)(toolbarBottom()-controlBarHeight)},Daw::chrome);
    g.setColour(juce::Colours::black.withAlpha(.6f));g.drawHorizontalLine(controlBarHeight-1,0,(float)w);g.drawHorizontalLine(toolbarBottom(),0,(float)w);
    g.setColour(juce::Colours::white.withAlpha(.04f));g.drawHorizontalLine(controlBarHeight,0,(float)w);
    Branding::drawFullLogo(g,{14,14,152,28});
    for(const auto& divider:toolbarDividers){g.setColour(juce::Colours::black.withAlpha(.5f));g.fillRect(divider.withWidth(1));g.setColour(juce::Colours::white.withAlpha(.05f));g.fillRect(divider.withWidth(1).translated(1,0));}
    // Transport display: position, tempo, metre and key on a dark screen.
    g.setGradientFill(juce::ColourGradient(Daw::lcd.brighter(.08f),(float)lcdArea.getCentreX(),(float)lcdArea.getY(),Daw::lcd,(float)lcdArea.getCentreX(),(float)lcdArea.getBottom(),false));g.fillRoundedRectangle(lcdArea.toFloat(),Daw::panelRadius);g.setColour(Daw::accent.withAlpha(.22f));g.drawRoundedRectangle(lcdArea.toFloat().reduced(.5f),Daw::panelRadius,1.f);g.setColour(juce::Colours::white.withAlpha(.09f));g.fillRect((float)lcdArea.getX()+6.f,(float)lcdArea.getY()+1.f,(float)lcdArea.getWidth()-12.f,1.f);
    g.setColour(juce::Colours::black);g.drawRoundedRectangle(lcdArea.toFloat().reduced(.5f),Daw::panelRadius,1.f);
    g.setColour(juce::Colours::white.withAlpha(.06f));g.drawHorizontalLine(lcdArea.getBottom(),(float)lcdArea.getX()+4,(float)lcdArea.getRight()-4);
    for(int x:lcdDividers){g.setColour(juce::Colours::white.withAlpha(.07f));g.drawVerticalLine(x,(float)lcdArea.getY()+7,(float)lcdArea.getBottom()-7);}
    const int legendY=lcdArea.getBottom()-15;
    Daw::drawCaps(g,positionLegend,{positionLabel.getX(),legendY,positionLabel.getWidth(),12});
    Daw::drawCaps(g,tr("TEMPO"),{tempoSlider.getX(),legendY,tempoSlider.getWidth(),12});
    g.setColour(Daw::textFaint);g.setFont(Daw::font(Daw::body,true));g.drawText("4/4",juce::Rectangle<int>(lcdDividers.size()>1?lcdDividers[1]:0,lcdArea.getY()+4,lcdDividers.size()>2?lcdDividers[2]-lcdDividers[1]:0,16),juce::Justification::centred);
    if(lcdDividers.size()>2){const int cpuPct=juce::roundToInt(audio.cpuLoad());Daw::drawCaps(g,"CPU "+juce::String(cpuPct)+"%",{lcdDividers[1],legendY,lcdDividers[2]-lcdDividers[1],12},cpuPct>75?Daw::meterHigh:cpuPct>50?Daw::meterMid:Daw::textFaint);}
    Daw::drawCaps(g,tr("KEY"),{keyBox.getX()+4,legendY,keyBox.getWidth()+scaleBox.getWidth(),12});
    // The master section's caption.
    g.setColour(Daw::textDim);g.setFont(Daw::font(Daw::small,true));g.drawText(tr("MASTER"),masterSlider.getRight()-64,3,64,12,juce::Justification::centred);
    if(page!=Page::arrangement){const auto ink=colour(selectedTrack);g.setColour(ink);g.fillRoundedRectangle(184.f,(float)controlBarHeight+12,3.f,14.f,1.f);
        g.setColour(Daw::text);g.setFont(Daw::font(Daw::title,true));g.drawText(tr(page==Page::instrument?"INSTRUMENT":"MIDI EDITOR")+" / "+utf(project.tracks[(size_t)selectedTrack].name).toUpperCase(),194,controlBarHeight+4,390,30,juce::Justification::centredLeft,true);}
    if(page==Page::arrangement&&chordsLabel.isVisible()){const auto r=chordsLabel.getBounds().toFloat().expanded(4.f,1.f);g.setColour(Daw::chrome);g.fillRoundedRectangle(r,Daw::radius);g.setColour(juce::Colours::black.withAlpha(.5f));g.drawRoundedRectangle(r.reduced(.5f),Daw::radius,1.f);g.setColour(Daw::accent);g.fillRoundedRectangle(r.getX()+1.f,r.getY()+1.f,3.f,r.getHeight()-2.f,1.f);}
    // Status bar: messages on the left, the project and its length on the right.
    g.setColour(Daw::chrome);g.fillRect(0,h-statusHeight,w,statusHeight);g.setColour(juce::Colours::black.withAlpha(.6f));g.drawHorizontalLine(h-statusHeight,0,(float)w);
    const double seconds=project.length()*60/project.settings.bpm;
    const auto title=utf(project.name)+(dirty?" *":"")+juce::String::fromUTF8("  ·  ")+juce::String((int)seconds/60)+":"+juce::String((int)seconds%60).paddedLeft('0',2);
    g.setColour(Daw::textFaint);g.setFont(Daw::font(Daw::small));g.drawText(tr("by")+" Fabio 'Pitch' Serra",w-176,h-statusHeight,164,statusHeight,juce::Justification::centredRight);
    g.setColour(Daw::textDim);g.setFont(Daw::font(Daw::small,true));g.drawText(title,w-536,h-statusHeight,352,statusHeight,juce::Justification::centredRight,true);
}
void MainComponent::resized(){
    loadingView.setBounds(getLocalBounds());
    const int w=getWidth();auto place=[](juce::Component& c,int x,int y,int width,int height=28){c.setBounds(x,y,width,height);};
    const bool song=page==Page::arrangement,engine=page==Page::instrument,edit=page==Page::editor;
    if(piano.ghostTrack.getParentComponent()!=this){addChildComponent(piano.ghostTrack);addChildComponent(piano.expressionLane);}
    piano.ghostTrack.setVisible(edit);piano.expressionLane.setVisible(edit);activeNotesButton.setVisible(edit);updatePianoEditingState();
    arrangementPort.setVisible(song);if(!instrumentWindow)instrumentPort.setVisible(engine);pianoPort.setVisible(edit);inspectorPort.setVisible(song&&inspectorVisible);inspectorCollapse.setVisible(song&&inspectorVisible);inspectorResizer.setVisible(song&&inspectorVisible);mixer.setVisible(song);
    for(auto* button:{&inspectorToggle,&mixerToggle,&fitSongButton,&followButton})button->setVisible(song);
    for(juce::Component* c:std::initializer_list<juce::Component*>{&genreBox,&songButton,&generateMenuButton,&densitySlider,&swingSlider,&humanizeSlider,&progressionBox,&harmonyButton,&extensionBox,&voicingBox,&bassBox,&stringsBox,&addSectionButton,&spotlightButton,&durationLabel,&targetDurationLabel,&targetDurationEditor,&targetToleranceLabel,&targetToleranceEditor,&targetFitLabel,&arrangeButton,&chordEditButton,&browserToggle})c->setVisible(song);
    aiComposeButton.setVisible(song);studioButton.setVisible(song);
    for(juce::Component* c:std::initializer_list<juce::Component*>{&timelineZoomOut,&timelineZoomIn,&timelineFitButton,&timelineZoom})c->setVisible(song);
    zoomSlider.setVisible(edit); // the arrangement zooms with +/-, the wheel, FIT SONG and ZOOM SELECTION
    sampleBrowser.setVisible(song&&browserVisible);browserResizer.setVisible(song&&browserVisible);
    automationButton.setVisible(song);addTrackButton.setVisible(song);addTrackButton.setEnabled(project.tracks.size()<maxTracks&&!loadingPlugin);
    bassBox.setVisible(false);stringsBox.setVisible(false); // instrument-owned styles live in their panels
    for(juce::Component* c:std::initializer_list<juce::Component*>{&soundButton,&soundBox,&sampleButton,&loadPluginButton,&pluginEditorButton,&gainSlider,&panSlider,&muteButton,&soloButton,&lockButton,&lockClipButton,&inspectorLabel})c->setVisible(engine);
    for(juce::Component* c:std::initializer_list<juce::Component*>{&editorLabel,&sectionLoopButton,&pianoPlayButton,&importButton,&copyButton,&pasteButton,&quantiseButton,&transposeDown,&transposeUp,&deleteNotes,&gridBox,&velocitySlider})c->setVisible(edit);
    for(juce::Component* button:std::initializer_list<juce::Component*>{&drawTool,&selectTool,&eraseTool,&regenerateClipButton})button->setVisible(edit);
    for(auto* button:{&brushAccent,&brushHit,&brushGhost,&brushFlam})button->setVisible(edit&&piano.laneView());fitNotesButton.setVisible(edit&&!piano.laneView());
    scaleGuideButton.setVisible(edit&&piano.scaleGuideAvailable());
    if(edit&&piano.ownerTrackIsDrum()){transposeDown.setVisible(false);transposeUp.setVisible(false);}
    backButton.setVisible(!song);instrumentButton.setVisible(edit);editButton.setVisible(engine);sectionSelector.setVisible(!song);leadButton.setVisible(song||(edit&&selectedTrack==3));guitarNotesButton.setVisible(edit&&selectedTrack==3&&!(project.tracks[(size_t)selectedTrack].instrument=="sample"&&project.tracks[(size_t)selectedTrack].sampler.mode==2));panelSections.setVisible(engine);
    toolbarDividers.clear();lcdDividers.clear();
    auto divider=[this](int x,int y,int height){toolbarDividers.push_back({x,y,1,height});};
    // Control bar. Left: file actions and history, as icons with tooltips.
    const int barY=(controlBarHeight-28)/2;int x=176;
    for(auto* b:{&newButton,&openButton,&saveButton,&saveAsButton}){place(*b,x,barY,30);x+=32;}
    divider(x+3,barY+4,20);x+=8;place(exportButton,x,barY,30);x+=38;divider(x-5,barY+4,20);
    place(undoButton,x,barY,30);place(redoButton,x+30,barY,30);
    undoButton.setConnectedEdges(juce::Button::ConnectedOnRight);redoButton.setConnectedEdges(juce::Button::ConnectedOnLeft);
    // Centre: transport buttons and the display, centred on the window.
    const int lcdWidth=428,transportWidth=3*30+6+2*30,start=(w-(transportWidth+10+lcdWidth))/2;
    x=start;place(rewindButton,x,barY,30);place(playButton,x+30,barY,30);place(stopButton,x+60,barY,30);x+=96;
    place(loopButton,x,barY,30);place(metronomeButton,x+30,barY,30);
    loopButton.setConnectedEdges(juce::Button::ConnectedOnRight);metronomeButton.setConnectedEdges(juce::Button::ConnectedOnLeft);x+=70;
    lcdArea={x,6,lcdWidth,controlBarHeight-12};
    place(positionLabel,x+8,lcdArea.getY()+2,120,26);positionModeButton.setBounds(x,lcdArea.getY(),132,lcdArea.getHeight());positionModeButton.toFront(false);lcdDividers.push_back(x+132);
    place(tempoSlider,x+136,lcdArea.getY()+2,78,26);lcdDividers.push_back(x+218);lcdDividers.push_back(x+258);
    place(keyBox,x+260,lcdArea.getY()+2,60,26);place(scaleBox,x+322,lcdArea.getY()+2,lcdWidth-326,26);
    // Right: master level, settings and help, then the panel switches.
    creditsButton.setBounds(14,14,152,28);const auto* topWindow=dynamic_cast<juce::ResizableWindow*>(getTopLevelComponent());const bool showQuit=appFullScreen||(topWindow&&topWindow->isFullScreen());quitButton.setVisible(showQuit);aboutButton.setVisible(true);
    x=w-14;for(auto* b:{&quitButton,&fullScreenButton,&mixerToggle,&browserToggle,&inspectorToggle}){if(!b->isVisible())continue;x-=30;place(*b,x,barY,30);x-=2;}divider(x-4,barY+4,20);x-=10;
    for(auto* b:{&helpButton,&aboutButton,&settingsButton}){if(!b->isVisible())continue;x-=30;place(*b,x,barY,30);x-=2;}divider(x-4,barY+4,20);x-=12;
    const int masterWidth=showQuit?120:152;place(masterSlider,x-masterWidth,8,masterWidth,40);
    // Arrangement toolbars: generation on the first row, harmony and feel on the second.
    const int rowA=controlBarHeight+4,rowB=controlBarHeight+toolRowHeight+4;
    for(auto* b:{&zoomSelectionButton,&loopSelectionButton})b->setVisible(song);timelineGrid.setVisible(song);
    if(song){

        // The three scope buttons are only used by the instrument page (APPLY TRACK / CLIP)
        // since the arrangement folds them into GENERATE.
        for(auto* b:{&trackButton,&sectionButton,&clipButton})b->setVisible(false);
        x=12;place(genreBox,x,rowA,170);x+=176;place(durationLabel,x,rowA,48);x+=52;
        const int editorW=56,tolEditorW=34,leadX=w-12-140,insertMax=leadX-8-(x+610);
        const int labelW=std::clamp(insertMax-(4+editorW+4+64+4+tolEditorW+4+78+8),48,108);
        const int afterTarget=insertMax-(labelW+4+editorW+4);
        const int tolLabelW=std::clamp(afterTarget-(tolEditorW+4+8),28,72);
        const int fitW=std::max(0,afterTarget-(tolLabelW+4+tolEditorW+8));
        place(targetDurationLabel,x,rowA,labelW);x+=labelW+4;place(targetDurationEditor,x,rowA+2,editorW,24);x+=editorW+4;
        place(targetToleranceLabel,x,rowA,tolLabelW);x+=tolLabelW+4;place(targetToleranceEditor,x,rowA+2,tolEditorW,24);x+=tolEditorW+4;
        place(targetFitLabel,x,rowA,fitW);targetFitLabel.setVisible(song&&fitW>=36);x+=std::max(fitW,0)+4;divider(x,rowA+4,20);x+=9;
        place(arrangeButton,x,rowA,170);x+=176;place(songButton,x,rowA,150);place(generateMenuButton,x+150,rowA,26);x+=180;divider(x+4,rowA+4,20);x+=13;
        place(addSectionButton,x,rowA,120);place(spotlightButton,x+132,rowA,100);
        place(leadButton,w-12-140,rowA,140);
        const bool compactB=w<1360;const int progW=compactB?164:206,harmW=compactB?96:118;x=12;place(progressionBox,x,rowB,progW);x+=progW+6;place(harmonyButton,x,rowB,harmW);x+=harmW+4;place(chordEditButton,x,rowB,harmW);x+=harmW+4;divider(x+4,rowB+4,20);x+=13;
        place(extensionBox,x,rowB,112);x+=116;place(voicingBox,x,rowB,124);x+=128;divider(x+4,rowB+4,20);x+=13;
        const int feelWidth=std::clamp((w-272-x)/3-8,92,164);for(auto* s:{&densitySlider,&swingSlider,&humanizeSlider}){place(*s,x,rowB-6,feelWidth,48);x+=feelWidth+8;}place(studioButton,w-264,rowB,112);place(aiComposeButton,w-144,rowB,132);
        const int top=toolbarBottom()+8;
        const int dockHeight=mixerVisible?std::clamp(mixerHeight,160,std::max(160,getHeight()-476)):28;
        const int footer=getHeight()-31-dockHeight-38;
        inspectorWidth=std::clamp(inspectorWidth,184,std::max(184,std::min(480,w/3)));
        const int left=inspectorVisible?inspectorWidth+24:12;
        // The browser takes the full height on the right; arrangement, harmony line and mixer share the rest.
        const int browserWidth=browserVisible?(sampleBrowserWidth>0?clampBrowserWidth(sampleBrowserWidth):std::clamp(w/4,300,380)):0,right=w-12-(browserVisible?browserWidth+8:0);
        sampleBrowser.setBounds(w-12-browserWidth,top,browserWidth,std::max(120,getHeight()-35-top));
        browserResizer.setBounds(w-12-browserWidth-8,top,8,std::max(120,getHeight()-35-top));browserResizer.toFront(false);
        arrangementPort.setBounds(left,top,right-left,std::max(60,footer-8-top));
        // Snap and the view tools sit in the corner above the track headers.
        const int corner=arrangementPort.getX();
        place(timelineGrid,corner+8,top+4,headerWidth-16,24);
        place(addTrackButton,corner+160,top+31,48,24);addTrackButton.toFront(false);
        x=corner+4;for(auto* b:{&zoomSelectionButton,&loopSelectionButton,&fitSongButton,&followButton,&automationButton}){place(*b,x,top+31,22,24);x+=24;}
        for(juce::Component* c:std::initializer_list<juce::Component*>{&timelineGrid,&zoomSelectionButton,&loopSelectionButton,&fitSongButton,&followButton,&automationButton})c->toFront(false);
        inspectorPort.setScrollBarsShown(inspectorVisible,false);
        inspectorPort.setBounds(12,top,inspectorVisible?inspectorWidth:0,std::max(60,footer-top));trackInspector.setSize(inspectorWidth-16,trackInspector.preferredHeight());
        inspectorCollapse.setVisible(inspectorVisible);inspectorCollapse.toFront(false);
        inspectorResizer.setBounds(12+inspectorWidth,top,8,std::max(60,footer-top));inspectorResizer.toFront(false);
        mixer.setBounds(12,footer+36,right-12,dockHeight);
        const int tools=right-336;
        place(chordsLabel,22,footer+2,std::max(100,tools-34),24);
        place(timelineZoomOut,tools,footer+1,28,26);place(timelineZoom,tools+34,footer+4,102,20);place(timelineZoomIn,tools+142,footer+1,28,26);place(timelineFitButton,tools+180,footer+1,156,26);
    }else{
        sampleBrowser.setVisible(false);chordEditButton.setVisible(true);
        place(backButton,12,rowA,160);place(sectionSelector,600,rowA,230);place(chordEditButton,1118,rowA,114);place(editButton,w-162,rowA,150);place(instrumentButton,w-192,rowA,180);place(leadButton,850,rowA,260);place(panelSections,850,rowA,260);
        if(engine){
            inspectorLabel.setVisible(false);soundButton.setVisible(true);sampleButton.setVisible(true);soundBox.setVisible(false);
            x=12;for(const auto& item:std::initializer_list<std::pair<juce::Component*,int>>{{&soundButton,80},{&sampleButton,110},{&loadPluginButton,130},{&pluginEditorButton,96},{&gainSlider,124},{&panSlider,112},{&muteButton,50},{&soloButton,50},{&lockButton,112},{&lockClipButton,112},{&trackButton,114},{&clipButton,114}}){place(*item.first,x,rowB,item.second);x+=item.second+6;}
            trackButton.setVisible(true);trackButton.setButtonText("APPLY TRACK");clipButton.setVisible(true);clipButton.setButtonText("APPLY CLIP");
            // Keep the plugins' 1180px design width. Extra DAW width must not
            // stretch only the Groove card and alter the family's proportions.
            const int panelWidth=std::min(1180,w-40);const int top=toolbarBottom()+8;
            instrumentPort.setBounds((w-panelWidth-16)/2,top,panelWidth+16,getHeight()-statusHeight-8-top);
            instrumentPanel.setSize(panelWidth,instrumentPanel.getHeight());
        }else{
            const int rowC=controlBarHeight+2*toolRowHeight+4;
            place(editorLabel,16,rowB,640,28);place(scaleGuideButton,w-638,rowB,132);place(guitarNotesButton,w-496,rowB,180);place(pianoPlayButton,w-302,rowB,146);place(sectionLoopButton,w-144,rowB,132);
            x=12;for(auto* b:{&importButton,&copyButton,&pasteButton,&quantiseButton,&transposeDown,&transposeUp,&deleteNotes}){if(!b->isVisible())continue;int width=b==&importButton?115:b==&quantiseButton?103:64;place(*b,x,rowC,width,28);x+=width+6;}place(gridBox,x+3,rowC,88);place(velocitySlider,x+101,rowC,154);place(zoomSlider,x+265,rowC,160);zoomSlider.setVisible(true);place(drawTool,1034,rowC,36);place(selectTool,1070,rowC,36);place(eraseTool,1106,rowC,36);place(regenerateClipButton,1152,rowC,138);place(brushAccent,700,rowB,80);place(brushHit,780,rowB,70);place(brushGhost,850,rowB,80);place(brushFlam,930,rowB,74);place(fitNotesButton,w-110,rowC,98);
            const int toolsTop=toolbarBottom()+8;piano.resized();place(piano.ghostTrack,12,toolsTop,220,24);place(piano.expressionLane,240,toolsTop,160,24);place(activeNotesButton,408,toolsTop,156,24);const int top=toolsTop+30;pianoPort.setBounds(12,top,w-24,getHeight()-statusHeight-8-top);
        }
    }
    chordsLabel.setVisible(song);
    if(song){trackButton.setButtonText("REGENERATE TRACK");sectionButton.setButtonText("REGENERATE SECTION");clipButton.setButtonText("REGENERATE CLIP");}
    place(statusLabel,12,getHeight()-statusHeight+1,w-560,statusHeight-2);options.setBounds(getLocalBounds());if(options.isVisible())options.toFront(false);pluginSettings.setBounds(getLocalBounds());if(pluginSettings.isVisible())pluginSettings.toFront(false);pluginBrowser.setBounds(getLocalBounds());if(pluginBrowser.isVisible())pluginBrowser.toFront(false);updateOverlay.setBounds(getLocalBounds());if(updateOverlay.isVisible())updateOverlay.toFront(false);updateSizes();for(juce::Button* b:std::initializer_list<juce::Button*>{&trackButton,&sectionButton,&clipButton,&inspectorCollapse})translateControls(*b);
    if(loadingView.isVisible())loadingView.toFront(false);
}
void MainComponent::mouseDown(const juce::MouseEvent& e){if(e.eventComponent==&chordsLabel)showChordCirclePopover(0);}
juce::StringArray MainComponent::iconButtonsWithoutTips(){
    juce::StringArray missing;std::function<void(juce::Component&,const juce::String&)> scan=[&](juce::Component& c,const juce::String& path){
        if(!c.isVisible())return;
        // It must also be the component the pointer finds there (not covered by another one).
        if(auto* icon=dynamic_cast<IconButton*>(&c))if(icon->getTooltip().trim().isEmpty()||(getComponentAt(getLocalPoint(&c,c.getLocalBounds().getCentre()))!=&c))missing.add(path+"/"+(c.getComponentID().isNotEmpty()?c.getComponentID():c.getName().isNotEmpty()?c.getName():juce::String(c.getX())+","+juce::String(c.getY())));
        for(auto* child:c.getChildren())scan(*child,path+"/"+juce::String(c.getParentComponent()?c.getParentComponent()->getIndexOfChildComponent(&c):0));};
    scan(*this,"main");return missing;
}
void MainComponent::applyFullScreen(bool on){
    if(on==appFullScreen)return;
    appFullScreen=on;
    fullScreenButton.setToggleState(on,juce::dontSendNotification);
    fullScreenButton.setIcon(on?IconButton::Icon::exitFullScreen:IconButton::Icon::fullScreen);
    if(fullScreenApplying)return;
    fullScreenApplying=true;
    juce::Component::SafePointer<MainComponent> safe(this);
    // Defer past this click: switching the native title bar destroys the peer.
    juce::MessageManager::callAsync([safe]{
        if(safe==nullptr)return;
        const bool enter=safe->appFullScreen;
        auto* window=dynamic_cast<juce::DocumentWindow*>(safe->getTopLevelComponent());
        if(window==nullptr){safe->fullScreenApplying=false;return;}
        if(enter){
            if(safe->fullScreenRestore.isEmpty()){safe->fullScreenRestore=window->getBounds();Localisation::setSetting("mainWindowState",window->getWindowStateAsString());}
            window->setUsingNativeTitleBar(false);
            window->setTitleBarHeight(0);
            auto* display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(safe->fullScreenRestore);
            if(display==nullptr)display=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
            const auto area=display!=nullptr?display->totalArea:safe->fullScreenRestore;
           #if JUCE_WINDOWS
            window->setBounds(area);
           #else
            window->setBounds(area);
            window->setFullScreen(true);
            juce::Desktop::getInstance().setKioskModeComponent(window,false);
           #endif
        }else{
            if(juce::Desktop::getInstance().getKioskModeComponent()==window)juce::Desktop::getInstance().setKioskModeComponent(nullptr,false);
            window->setFullScreen(false);
            window->setUsingNativeTitleBar(true);
            const auto back=safe->fullScreenRestore;safe->fullScreenRestore={};
            juce::MessageManager::callAsync([safe,back]{if(safe==nullptr)return;if(auto* w=dynamic_cast<juce::DocumentWindow*>(safe->getTopLevelComponent()))if(!back.isEmpty())w->setBounds(back);safe->fullScreenApplying=false;if(safe->appFullScreen)safe->applyFullScreen(true);});
            return;
        }
        safe->fullScreenApplying=false;
        if(safe->appFullScreen!=enter)safe->applyFullScreen(safe->appFullScreen);
    });
}
void MainComponent::updateLoadingProgress(){
    if(!restoringProject)return;
    const int remaining=(int)restorePluginTasks.size()+loadingPlugin+(sampleImportActive()?1:0);
    if(!remaining){restoringProject=false;loadingView.setVisible(false);presentMissingResources();return;}
    loadingView.show(tr("Loading instruments")+" "+juce::String(restorePluginTotal-remaining)+" / "+juce::String(restorePluginTotal),double(restorePluginTotal-remaining)/std::max(1,restorePluginTotal));
    if(!loadingPlugin&&!sampleImportActive()&&!restoreTaskScheduled&&!restorePluginTasks.empty()){
        restoreTaskScheduled=true;juce::Component::SafePointer<MainComponent> safe(this);
        // Give the overlay a paint cycle before entering the next plugin's code.
        juce::Timer::callAfterDelay(30,[safe]{if(!safe)return;safe->restoreTaskScheduled=false;if(!safe->restoringProject||safe->restorePluginTasks.empty())return;auto task=std::move(safe->restorePluginTasks.front());safe->restorePluginTasks.pop_front();task();});
    }
}
void MainComponent::timerCallback(){
    flushGlobalFeel();
    pollSampleImport();
    sampleButton.setButtonText(sampleImportActive()?tr("CANCEL LOAD"):tr("LOAD SAMPLE"));
    sampleButton.setTooltip(sampleImportActive()?sampleImportMessage():tr("Load sample audio into the selected track."));
    updateLoadingProgress();
    if(suspendedEffectsSession)resumeEffectChain();
    fga::applyDarkTitleBarToAll();
    // The OS can leave full screen on its own (window manager, Escape in some shells):
    // keep the button's icon and state in step without repainting when nothing changed.
    const double beat=timelinePosition();showPosition(beat);playButton.setIcon(audio.isPlaying()?IconButton::Icon::pause:IconButton::Icon::play);playButton.setToggleState(audio.isPlaying(),juce::dontSendNotification);mixer.tick();trackInspector.tick();pianoPlayButton.setToggleState(audio.isClipPlaying(),juce::dontSendNotification);pianoPlayButton.setIcon(audio.isClipPlaying()?IconButton::Icon::stop:IconButton::Icon::play);pianoPlayButton.setButtonText(tr(audio.isClipPlaying()?"STOP CLIP":"PLAY CLIP"));pianoPlayButton.setEnabled(!editingRestricted()&&!loadingPlugin&&clipIndex()>=0);audio.flushParameterNotifications();updateGainDisplay(masterSlider,project.masterGain,project.masterAutomation);
    if(juce::isPositiveAndBelow(selectedTrack,(int)project.tracks.size())){const auto& selected=project.tracks[(size_t)selectedTrack];updateGainDisplay(gainSlider,selected.gain,selected.volume);updatePanDisplay(panSlider,selected.pan,selected.panAutomation);}
    if(page==Page::arrangement&&followPlayhead&&audio.isPlaying()){const auto v=arrangementPort.getViewArea();const int x=headerWidth+(int)(beat/4*arrangement.pixelsPerBar);if(x>v.getRight()-40||x<v.getX()+headerWidth)arrangementPort.setViewPosition(std::max(0,x-headerWidth-60),v.getY());}
    // Repaint the timeline only while something moves (playback, meters falling, cursor jumps):
    // an idle arranger uses no CPU for drawing.
    const bool mouseDown=arrangement.isMouseButtonDown(true)||piano.isMouseButtonDown(true);
    const bool moving=audio.isPlaying()||beat!=lastTimerBeat||audio.masterLevel()>0||mouseDown;lastTimerBeat=beat;
    if(moving){
        // Playing hands-off, only the playhead strip and the track headers (meters) move.
        const int playX=headerWidth+(int)(beat/4*arrangement.pixelsPerBar);
        if(audio.isPlaying()&&!mouseDown&&page==Page::arrangement){const auto v=arrangementPort.getViewArea();arrangement.repaint(v.getX(),v.getY()+rulerHeight,headerWidth,v.getHeight()-rulerHeight);for(int x:{lastPlayX,playX})if(x>=0)arrangement.repaint(x-7,v.getY(),16,v.getHeight());}
        else arrangement.repaint();
        lastPlayX=playX;
        if(page==Page::editor)piano.repaint();}
    if(moving||ticks%5==0)repaint(lcdArea);
    if(instrumentIsOpen())instrumentPanel.repaintMeters(moving);audio.collectGarbage();
    updateEditingAvailability();
    if(++ticks%1200==0&&dirty)autosaveDue=true;
    if(autosaveDue)autosaveIfIdle();
}
// Capture native plugin state only with a quiet, stopped transport; JSON and disk
// work run on an owned snapshot outside the message and audio threads.
void MainComponent::autosaveIfIdle(const juce::File& target){
    if(!dirty){autosaveDue=false;return;}
    if(autosaveJob||loadingPlugin||editingRestricted()||audio.isPlaying()||audio.isSampleAuditioning()||audio.isExporting()||(audio.devices.getCurrentAudioDevice()!=nullptr&&audio.masterLevel()>1.e-5f))return;
    captureProjectStates(audio,project,hasOpenPluginEditors());auto path=target!=juce::File()?target:juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("FS Pitch/Fase Groove Arranger/autosave.fga");
    if(auto result=path.getParentDirectory().createDirectory();result.failed()){autosaveDue=false;message(result.getErrorMessage(),true);return;}
    const int id=++autosaveRequestId;juce::Component::SafePointer<MainComponent> safe(this);autosaveDue=false;
    autosaveJob=std::make_unique<AutosaveJob>(project,path,recoveryArmed?recoveryDirectory():juce::File(),projectFile.getFullPathName(),[safe,id](juce::Result result){if(!safe||id!=safe->autosaveRequestId)return;safe->retiredJobs.retire(std::move(safe->autosaveJob));if(result.failed())safe->message(result.getErrorMessage(),true);});if(!autosaveJob->startThread()){autosaveJob.reset();message("Automatic backup could not start. Save the project manually.",true);}
}
// The display reads bars.beats.ticks (960 ticks per beat).
void MainComponent::offerRecovery(){
    RecoveryOffer offer;if(!pendingRecovery(recoveryDirectory(),offer)){openRecoverySession(recoveryDirectory());return;}
    juce::Component::SafePointer<MainComponent> safe(this);
    juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon,tr("Restore unsaved work?"),tr("Arranger did not close normally. Restore the recovered edits? The original project file will not be changed."),tr("Restore"),tr("Discard"),nullptr,juce::ModalCallbackFunction::create([safe,offer](int result){if(!safe)return;if(result==0){clearRecoverySnapshots(recoveryDirectory());openRecoverySession(recoveryDirectory());return;}safe->applyRecovery(offer);}));
}
void MainComponent::applyRecovery(const RecoveryOffer& offer){
    Project next;if(loadProject(offer.snapshot,next).failed()){message(tr("The recovery file is damaged and was not applied. The original project was not changed."),true);openRecoverySession(recoveryDirectory());return;}
    audio.stop();undoStack.clear();redoStack.clear();project=std::move(next);editCursor=-1;projectFile=offer.originalPath.isNotEmpty()?juce::File(offer.originalPath):juce::File();selectedTrack=selectedSection=0;dirty=true;piano.clearSelection();audio.publish(project);refreshControls();restoreInstruments();select(4,0);showArrangement();message(tr("Recovered edits are open. Save them when you want to keep them. The previous project file was not replaced."));
}
void MainComponent::reportExternal(const juce::String& line){if(restoringProject){missingExternal.addIfNotAlreadyThere(line);return;}message(line,true);}
void MainComponent::presentMissingResources(){
    if(missingExternal.isEmpty())return;const auto lines=missingExternal;missingExternal.clear();
    const bool samples=std::any_of(lines.begin(),lines.end(),[](const juce::String& line){return line.startsWith(tr("Sample"));});
    message(tr("Some external files are missing. Nothing was replaced.")+"\n"+lines.joinIntoString("\n"),true);
    if(!samples)return;juce::Component::SafePointer<MainComponent> safe(this);
    juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::InfoIcon,tr("Missing samples"),tr("Search a folder for the missing samples? Plugins are not guessed, and cancelling leaves every path unchanged."),tr("Locate"),tr("Not now"),nullptr,juce::ModalCallbackFunction::create([safe](int result){if(!safe||result==0)return;safe->chooseFile(tr("RELINK SAMPLES"),"*",juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,[safe](juce::File folder){if(!safe)return;RelinkResult report;auto done=relinkProjectMedia(safe->project,folder,report);if(done.failed()){safe->message(done.getErrorMessage(),true);return;}if(report.found>0)safe->dirty=true;safe->restoreInstruments();safe->message(tr("Samples located")+" "+juce::String(report.found)+(report.missing.isEmpty()?juce::String():"\n"+tr("Still missing")+"\n"+report.missing.joinIntoString("\n"))+(report.unverified.isEmpty()?juce::String():"\n"+tr("Matched by filename only")+"\n"+report.unverified.joinIntoString("\n")),report.missing.size()>0);});}));
}
void MainComponent::refreshTargetDurationEditor(){
    if(!targetDurationEditor.hasKeyboardFocus(true)){juce::String text;if(project.settings.targetEnabled){const int seconds=(int)std::lround(project.settings.targetSeconds);text=juce::String(seconds/60)+":"+juce::String(seconds%60).paddedLeft('0',2);}if(targetDurationEditor.getText()!=text)targetDurationEditor.setText(text,false);}
    if(!targetToleranceEditor.hasKeyboardFocus(true)){const auto text=juce::String((int)std::lround(project.settings.targetToleranceSeconds));if(targetToleranceEditor.getText()!=text)targetToleranceEditor.setText(text,false);}
    const auto fit=targetFit(project);const auto word=fit==TargetFit::within?"On target":fit==TargetFit::below?"Too short":fit==TargetFit::above?"Too long":"";
    targetFitLabel.getProperties().set("fgaRawText",true);targetFitLabel.setText(word[0]?tr(word):juce::String(),juce::dontSendNotification);
    const auto ink=fit==TargetFit::within?Daw::play:fit==TargetFit::below?Daw::meterMid:fit==TargetFit::above?Daw::meterHigh:Daw::text;
    durationLabel.setColour(juce::Label::textColourId,ink);targetFitLabel.setColour(juce::Label::textColourId,ink);
}
void MainComponent::commitTargetDuration(){
    if(!acceptTargetEdit)return;
    const auto text=targetDurationEditor.getText().trim();
    auto show=[&]{juce::String shown;if(project.settings.targetEnabled){const int seconds=(int)std::lround(project.settings.targetSeconds);shown=juce::String(seconds/60)+":"+juce::String(seconds%60).paddedLeft('0',2);}if(targetDurationEditor.getText()!=shown)targetDurationEditor.setText(shown,false);};
    auto publish=[&]{dirty=true;updateSizes();arrangement.repaint();showPosition(timelinePosition());show();};
    if(text.isEmpty()){if(project.settings.targetEnabled||project.settings.targetSeconds!=0){project.settings.targetEnabled=false;project.settings.targetSeconds=0;publish();}else show();return;}
    int minutes=0,secs=0;if(text.contains(":")){minutes=text.upToFirstOccurrenceOf(":",false,false).getIntValue();secs=text.fromFirstOccurrenceOf(":",false,false).getIntValue();}else secs=text.getIntValue();
    const int total=minutes*60+secs;if(total<=0||total>24*3600){message(tr("Enter a duration such as 3:30."),true);show();return;}
    if(project.settings.targetEnabled&&(int)std::lround(project.settings.targetSeconds)==total){show();return;}
    project.settings.targetEnabled=true;project.settings.targetSeconds=total;publish();
}
void MainComponent::commitTargetTolerance(){
    if(!acceptTargetEdit)return;
    const auto text=targetToleranceEditor.getText().trim();
    const int seconds=text.containsOnly("0123456789")?text.getIntValue():-1;
    if(text.isEmpty()||seconds<0||seconds>600){message(tr("Enter a tolerance from 0 to 600 seconds."),true);targetToleranceEditor.setText(juce::String((int)std::lround(project.settings.targetToleranceSeconds)),false);return;}
    if((int)std::lround(project.settings.targetToleranceSeconds)==seconds){targetToleranceEditor.setText(juce::String(seconds),false);return;}
    project.settings.targetToleranceSeconds=seconds;dirty=true;arrangement.repaint();showPosition(timelinePosition());refreshTargetDurationEditor();
}
void MainComponent::collectPortable(){
    chooseFile(tr("Collect portable project"),"*",juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectDirectories,[this](juce::File folder){PortableReport report;auto done=packagePortableProject(project,folder,report);if(done.failed()){message(done.getErrorMessage(),true);return;}auto note=tr("Portable project saved")+" "+report.project.getFullPathName();if(report.externalPlugins.isEmpty())note+="\n"+tr("Installed plugins were not copied. This copy has no SFZ maps to bundle.");else note+="\n"+tr("Installed plugins were not copied")+"\n"+report.externalPlugins.joinIntoString("\n");message(note,true);});
}
void MainComponent::showPosition(double beat){
    const int ticksIn=std::clamp((int)std::floor((beat-std::floor(beat))*960+1e-6),0,959);
    positionLabel.setText(timeDisplay?timeText(beat*60/project.settings.bpm,3):juce::String((int)beat/4+1)+"."+juce::String((int)beat%4+1)+"."+juce::String(ticksIn).paddedLeft('0',3),juce::dontSendNotification);
    const auto length=songLength();const auto legend=timeDisplay?"-"+timeText(std::max(0.,length-beat)*60/project.settings.bpm)+" "+tr("remaining"):"/ "+juce::String((int)length/4)+" "+tr("bars");
    if(positionLegend!=legend){positionLegend=legend;repaint(lcdArea.withWidth(132));}
}
void MainComponent::message(const juce::String& text,bool error){statusLabel.setColour(juce::Label::textColourId,error?Palette::toms:Palette::dim);statusLabel.getProperties().set("labelSource",text);statusLabel.getProperties().set("labelLast",tr(text));statusLabel.setText(tr(text),juce::dontSendNotification);}
bool MainComponent::keyPressed(const juce::KeyPress& k){
    if(loadingView.isVisible())return true;
    if(k.getKeyCode()==juce::KeyPress::F11Key){applyFullScreen(!appFullScreen);return true;}
    if(pluginSettings.isVisible()){if(k==juce::KeyPress::escapeKey)pluginSettings.setVisible(false);return true;}
    if(pluginBrowser.isVisible()){if(k==juce::KeyPress::escapeKey)pluginBrowser.setVisible(false);return true;}
    if(k==juce::KeyPress::escapeKey){if(appFullScreen){applyFullScreen(false);return true;}if(updateOverlay.isVisible()){updateOverlay.setVisible(false);return true;}if(options.isVisible()){options.setVisible(false);return true;}showArrangement();return true;}
    if(k==juce::KeyPress::spaceKey){if(audio.isRecording()||audio.isAudioRecording())stopPlaybackAndRecording();else togglePlayback();return true;}
    if(editingRestricted())return true;
    if(k==juce::KeyPress::homeKey){seekTimeline(0);return true;}
    if(k.getKeyCode()==juce::KeyPress::F3Key){if(page!=Page::arrangement){mixerVisible=true;mixerToggle.setToggleState(true,juce::dontSendNotification);showArrangement();}else mixerToggle.triggerClick();return true;}
    const int code=k.getKeyCode();
    if(k.getModifiers().isCommandDown()){
        if(code=='K'){showSpotlightSearch();return true;}
        if(code=='S'){save(k.getModifiers().isShiftDown());return true;}
        if(code=='O'){openButton.triggerClick();return true;}
        if(code=='N'){newButton.triggerClick();return true;}
        if(code=='Z'){undo(k.getModifiers().isShiftDown());return true;}
        if(code=='Y'){undo(true);return true;}
        if(code=='L'&&(page==Page::arrangement||page==Page::editor)){loopSelection();return true;}
        if(page==Page::arrangement&&code=='C'){timelineCommand(2);return true;}
        if(page==Page::arrangement&&code=='X'){timelineCommand(1);return true;}
        if(page==Page::arrangement&&code=='D'){timelineCommand(5);return true;}
        if(page==Page::arrangement&&code=='E'){timelineCommand(4);return true;}
        if(page==Page::arrangement&&code=='A'){arrangement.selectedClips.clear();for(int i=0;i<(int)project.tracks[(size_t)selectedTrack].clips.size();++i)arrangement.selectedClips.insert({selectedTrack,i});arrangement.repaint();return true;}
        if(page==Page::arrangement&&code=='V'){timelineCommand(3);return true;}
    }else if(!k.getModifiers().isAltDown()){
        if(code=='S'&&page==Page::arrangement){timelineCommand(4);return true;}
        if(code=='E'){showEditor();return true;}
        if(code=='I'&&page==Page::arrangement){inspectorToggle.triggerClick();return true;}
        if(code=='Z'&&(page==Page::arrangement||page==Page::editor)){zoomSelection();return true;}
        if((code=='+'||code=='='||code=='-')&&(page==Page::arrangement||page==Page::editor)){auto& port=page==Page::editor?pianoPort:arrangementPort;zoomAt(page==Page::editor,code=='-'?.8:1.25,port.getWidth()*.5);return true;}
        if(code=='F'&&page==Page::arrangement){fitSong();return true;}
        if(code=='B'&&page==Page::arrangement){showSampleBrowser(!browserVisible);return true;}
        if(code=='C'){toggleMetronome();return true;}
        if(code=='L'){toggleLoop();return true;}
        if(code=='H'&&(page==Page::arrangement||page==Page::editor)){if(page==Page::editor&&!piano.showActiveNotes)return true;checkpoint();humanize(clip(),.65,(std::uint32_t)juce::Random::getSystemRandom().nextInt(),clipLength());tidyNotes(clip(),clipLength());piano.clearSelection();changed();return true;}
        if(page==Page::arrangement&&(k==juce::KeyPress::deleteKey||k==juce::KeyPress::backspaceKey)){timelineCommand(6);return true;}
    }
    return false;
}

// Save / Discard / Cancel, as in any DAW. An untitled project opens SAVE AS and keeps the
// current song open: close again once it is saved.
void MainComponent::createNewProject(){pendingGlobalFeel=0;instrumentPanel.cancelRender();audio.stop();closePluginEditor();audio.clearEffect(masterEffectTrack);undoStack.clear();redoStack.clear();lastCheckpointControl=nullptr;for(int i=0;i<maxTracks;++i){audio.useInternal(i);audio.clearEffect(i);};project=newProject(genreBox.getSelectedItemIndex());editCursor=-1;projectFile=juce::File();selectedSection=0;selectedTrack=4;arrangement.selectedClips.clear();noClipSelection=false;changed();showArrangement();}
void MainComponent::requestClose(std::function<void()> accepted){
    if(closePending)return;
    if(loadingPlugin){message(tr("Wait for the instrument to finish loading."),true);return;}
    if(!dirty&&!audio.isRecording()&&!audio.isAudioRecording()){accepted();return;}
    closePending=true;juce::Component::SafePointer<MainComponent> safe(this);
    juce::AlertWindow::showYesNoCancelBox(juce::MessageBoxIconType::QuestionIcon,tr("Unsaved changes"),tr("The project has unsaved changes. Save them before closing?"),tr("SAVE"),tr("Discard"),tr("Cancel"),studioWindow&&studioWindow->isVisible()?static_cast<juce::Component*>(studioWindow.get()):getTopLevelComponent(),
        juce::ModalCallbackFunction::create([safe,accepted=std::move(accepted)](int choice)mutable{if(safe)safe->completeCloseChoice(choice,std::move(accepted));}));
}
void MainComponent::completeCloseChoice(int choice,std::function<void()> accepted){
    if(choice==0){closePending=false;return;}
    if(choice==2){if(studioWindow&&(audio.isRecording()||audio.isAudioRecording()))studioWindow->closeButtonPressed();if(recoveryArmed)clearRecoverySnapshots(recoveryDirectory());closePending=false;accepted();return;}
    if(choice!=1){closePending=false;return;}
    if(projectFile==juce::File()){
        juce::Component::SafePointer<MainComponent> safe(this);
        chooseFile(tr("Save project"),"*.fga",juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,
            [safe,accepted=std::move(accepted)](juce::File file)mutable{if(safe)safe->finishPendingSave(file,std::move(accepted));},[safe]{if(safe)safe->closePending=false;});
    }else finishPendingSave(projectFile,std::move(accepted));
}
void MainComponent::finishPendingSave(const juce::File& file,std::function<void()> accepted){
    closePending=false;if(file==juce::File())return;
    if(loadingPlugin){message(tr("Wait for the instrument to finish loading."),true);return;}
    if(studioWindow&&(audio.isRecording()||audio.isAudioRecording()))studioWindow->closeButtonPressed();
    if(writeProjectFile(file.withFileExtension("fga")))accepted();
}
bool MainComponent::writeProjectFile(const juce::File& target){
    flushGlobalFeel();instrumentPanel.flushRender();
    captureProjectStates(audio,project,true);auto next=project;next.name=target.getFileNameWithoutExtension().toStdString();auto result=saveProject(next,target);
    if(result.failed()){message(result.getErrorMessage(),true);return false;}
    projectFile=target;Localisation::setSetting("lastSaveFolder",target.getParentDirectory().getFullPathName());project.name=next.name;dirty=false;if(recoveryArmed)clearRecoverySnapshots(recoveryDirectory());message(tr("Project saved")+": "+projectFile.getFullPathName());repaint();return true;
}
void MainComponent::chooseFile(const juce::String& title,const juce::String& filter,int flags,std::function<void(juce::File)> callback,std::function<void()> cancelled,const juce::File& initial){
    auto start=initial;const bool saving=(flags&juce::FileBrowserComponent::saveMode)!=0&&filter=="*.fga";
    if(start==juce::File()&&saving){const auto stored=Localisation::setting("lastSaveFolder");if(juce::File::isAbsolutePath(stored)&&juce::File(stored).isDirectory())start=juce::File(stored);}
    if(start==juce::File())start=projectFile.existsAsFile()?projectFile.getParentDirectory():juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
#if FGA_SELF_TESTS
    if(fileChoiceForTest){juce::Component::SafePointer<MainComponent> safe(this);fileChoiceForTest(start,flags,[safe,callback=std::move(callback)](juce::File file){if(safe)callback(file);},[safe,cancelled=std::move(cancelled)]{if(safe&&cancelled)cancelled();});return;}
#endif
    chooser=std::make_unique<juce::FileChooser>(tr(title),start,filter,true,false,studioWindow&&studioWindow->isVisible()?static_cast<juce::Component*>(studioWindow.get()):getTopLevelComponent());
    juce::Component::SafePointer<MainComponent> safe(this);chooser->launchAsync(flags,[safe,callback=std::move(callback),cancelled=std::move(cancelled)](const juce::FileChooser& fc){if(!safe)return;auto file=fc.getResult();if(file!=juce::File())callback(file);else if(cancelled)cancelled();});
}
void MainComponent::save(bool as){
    if(loadingPlugin){message("Please wait for the instrument to load.");return;}
    if(as||projectFile==juce::File()){juce::Component::SafePointer<MainComponent> safe(this);chooseFile(tr("Save project"),"*.fga",juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[safe](juce::File file){if(safe)safe->writeProjectFile(file.withFileExtension("fga"));});}
    else writeProjectFile(projectFile);
}
void MainComponent::open(const juce::File& file,bool recovered){
    if(loadingPlugin||loadingView.isVisible()){message(tr("Wait for the instrument to finish loading."));return;}
    juce::Component::SafePointer<MainComponent> safe(this);requestClose([safe,file,recovered]{if(!safe)return;
        safe->addChildComponent(safe->loadingView);safe->loadingView.setBounds(safe->getLocalBounds());safe->loadingView.show(tr("Loading project..."));safe->loadingView.grabKeyboardFocus();
        juce::Timer::callAfterDelay(40,[safe,file,recovered]{if(!safe)return;
        Project next;auto result=loadProject(file,next);if(result.failed()){safe->loadingView.setVisible(false);safe->message(result.getErrorMessage(),true);return;}
        auto& o=*safe;o.audio.stop();o.undoStack.clear();o.redoStack.clear();o.project=std::move(next);o.editCursor=-1;o.projectFile=recovered?juce::File():file;o.selectedTrack=o.selectedSection=0;o.dirty=recovered;o.piano.clearSelection();o.audio.publish(o.project);o.refreshControls();o.restoreInstruments();o.select(4,0);o.showArrangement();o.message(tr("Project opened. Audio files and plugins remain external to the project."));o.repaint();
        });
    });
}

void MainComponent::showMixerRouting(int track){audio.stopSampleAudition();if(track>=0&&track<(int)project.tracks.size())select(track,selectedSection);if(!mixerRoutingWindow)mixerRoutingWindow=std::make_unique<MixerRoutingWindow>(*this);mixerRoutingWindow->open();}
void MainComponent::openGraphicEq(int track){if(track>=0&&track<(int)project.tracks.size())select(track,selectedSection);if(!graphicEqWindow)graphicEqWindow=std::make_unique<GraphicEqWindow>(*this);graphicEqWindow->open(track>=0?track:selectedTrack);}
void MainComponent::refreshGraphicEq(){if(graphicEqWindow&&graphicEqWindow->isVisible())graphicEqWindow->refresh();}
void MainComponent::showStudio(int tab){if(mediaBusy||audio.isExporting())return;if(!studioWindow)studioWindow=std::make_unique<StudioWindow>(*this);studioWindow->open(tab);}
void MainComponent::restoreInstruments(){cancelSampleImport();for(int track=0;track<maxTracks;++track)audio.unfreezeTrack(track);pendingGlobalFeel=0;addChildComponent(loadingView);loadingView.setBounds(getLocalBounds());loadingView.show(tr("Loading project..."));restoringProject=true;restorePluginTotal=0;restorePluginTasks.clear();closePluginEditor();audio.clearEffect(masterEffectTrack);for(const auto& fx:project.masterEffects.effects)restorePluginTasks.push_back([this,fx]{loadEffectFile(masterEffectTrack,juce::File::isAbsolutePath(utf(fx.path))?juce::File(utf(fx.path)):juce::File(),utf(fx.pluginId),utf(fx.state),fx.slot);});for(int i=0;i<maxTracks;++i){audio.useInternal(i);audio.clearEffect(i);}for(int i=0;i<(int)project.tracks.size();++i){auto t=project.tracks[(size_t)i];if(t.instrument=="sample"){audio.markMissing(i);restorePluginTasks.push_back([this,session=t.sessionId,file=sampleFile(t,projectFile),settings=t.sampler,root=t.sampleRoot]{for(size_t index=0;index<project.tracks.size();++index)if(project.tracks[index].sessionId==session){startSampleImport((int)index,file,settings,root,false,false,true);break;}});}else if(t.instrument=="plugin")restorePluginTasks.push_back([this,i,t]{loadPluginFile(i,juce::File::isAbsolutePath(utf(t.instrumentPath))?juce::File(utf(t.instrumentPath)):juce::File(),utf(t.instrumentId),utf(t.instrumentState));});for(const auto& fx:t.effects)restorePluginTasks.push_back([this,i,fx]{loadEffectFile(i,juce::File::isAbsolutePath(utf(fx.path))?juce::File(utf(fx.path)):juce::File(),utf(fx.pluginId),utf(fx.state),fx.slot);});}restorePluginTotal=(int)restorePluginTasks.size();audio.publish(project);updateLoadingProgress();}
void MainComponent::loadPluginFile(int track,const juce::File& file,const juce::String& id,const juce::String& state){
    if(id.isNotEmpty())audio.markMissing(track);
    if(auto xml=juce::XmlDocument::parse(id)){
        juce::PluginDescription description;
        if(description.loadFromXml(*xml)){
            if(description.pluginFormatName=="VST3"&&!juce::File(description.fileOrIdentifier).exists()){reportExternal(tr("Plugin")+" · "+description.name+" · "+description.fileOrIdentifier);return;}
            loadPluginDescription(track,description,state,true);return;
        }
    }
    if(!file.exists()){reportExternal(tr("Plugin")+" · "+utf(project.tracks[(size_t)track].name)+" · "+file.getFullPathName());return;}
    juce::OwnedArray<juce::PluginDescription> found;
    for(int i=0;i<audio.formats.getNumFormats();++i)audio.formats.getFormat(i)->findAllTypesForFile(found,file.getFullPathName());
    juce::PluginDescription* selected=nullptr,*effect=nullptr;
    for(auto* d:found){if(d->createIdentifierString()==id)selected=d;if(d->isInstrument&&selected==nullptr)selected=d;if(!d->isInstrument&&effect==nullptr)effect=d;}
    if(selected==nullptr)selected=effect;
    if(!selected){message("Nessuno strumento compatibile trovato. Scegli il bundle VST3.",true);return;}
    loadPluginDescription(track,*selected,state,id.isNotEmpty());
}
void MainComponent::loadEffectFile(int track,const juce::File& file,const juce::String& id,const juce::String& state,int slot){
    if(id.isNotEmpty())if(auto xml=juce::XmlDocument::parse(id)){juce::PluginDescription description;if(description.loadFromXml(*xml)){if(description.pluginFormatName=="VST3"&&!juce::File(description.fileOrIdentifier).exists()){reportExternal(tr("Plugin")+" · "+description.name+" · "+description.fileOrIdentifier);return;}loadEffectDescription(track,description,state,true,slot);return;}}
    if(!file.exists()){reportExternal(tr("Plugin")+" · "+utf(effectTrack(track).name)+" · "+file.getFullPathName());return;}
    juce::OwnedArray<juce::PluginDescription> found;for(int i=0;i<audio.formats.getNumFormats();++i)audio.formats.getFormat(i)->findAllTypesForFile(found,file.getFullPathName());
    juce::PluginDescription* selected=nullptr;for(auto* d:found)if(!d->isInstrument){selected=d;break;}if(!selected){message("Choose an audio effect plugin, not an instrument.",true);return;}
    loadEffectDescription(track,*selected,state,id.isNotEmpty(),slot);
}
void MainComponent::loadEffectDescription(int track,const juce::PluginDescription& description,const juce::String& state,bool restoring,int slot){
    if(!validEffectTrack(track)||description.isInstrument||(!restoring&&(loadingPlugin||editingRestricted())))return;
    auto& t=effectTrack(track);if(slot==0)slot=t.nextEffectSlot();if(slot<1||slot>maxTrackEffects){message("The effect chain is full.",true);return;}
    const auto session=t.sessionId;if(!restoring)checkpoint();closePluginEditor(track,slot);++loadingPlugin;loadPluginButton.setEnabled(false);
    juce::Component::SafePointer<MainComponent> safe(this);
    audio.loadEffect(track,description,[safe,session,description,state,restoring,slot](juce::Result r){
        if(!safe)return;--safe->loadingPlugin;safe->loadPluginButton.setEnabled(safe->loadingPlugin==0);const int ti=safe->trackForSession(session);if(ti<0)return;
        if(r.failed()){safe->message(r.getErrorMessage(),true);safe->refreshTrack();safe->resumeEffectChain();return;}
        auto& target=safe->effectTrack(ti);auto* fx=target.effect(slot);if(!fx){EffectSlot added;added.slot=slot;target.effects.push_back(added);fx=&target.effects.back();}
        bool samePlugin=false;if(auto old=juce::XmlDocument::parse(utf(fx->pluginId))){juce::PluginDescription previous;if(previous.loadFromXml(*old))samePlugin=previous.uniqueId==description.uniqueId&&previous.name==description.name&&previous.manufacturerName==description.manufacturerName&&previous.pluginFormatName==description.pluginFormatName;}
        const auto restoredState=state.isNotEmpty()?state:(samePlugin?utf(fx->state):juce::String());
        if(!restoring&&!samePlugin)target.parameters.erase(std::remove_if(target.parameters.begin(),target.parameters.end(),[slot](const auto& l){return l.slot==slot;}),target.parameters.end());if(!restoring&&!samePlugin)safe->resetAutomationTarget(ti);
        fx->path=description.fileOrIdentifier.toStdString();fx->pluginName=description.name.toStdString();if(auto xml=description.createXml())fx->pluginId=xml->toString().toStdString();fx->state=restoredState.toStdString();
        if(restoredState.isNotEmpty())safe->audio.restoreEffectState(ti,restoredState,slot);safe->resolveAutomationParameters(ti,slot);
        if(restoring){safe->audio.updateMix(safe->project);safe->refreshTrack();}else safe->changed(false);
        safe->message(tr("Effect ready:")+" "+description.name);
        safe->resumeEffectChain();if(!restoring)safe->openHostedEditor(ti,slot);
    },slot);
}
void MainComponent::chooseBrowserRow(const PluginCatalog::BrowserRow& row){
    if(loadingPlugin){message(tr("Wait for the instrument to finish loading."));return;}
    const int track=trackForSession(browserTrackSession);if(track<0||editingRestricted())return;auto& t=effectTrack(track);
    if(row.kind==PluginCatalog::BrowserRow::Kind::noEffect){if(t.effects.empty())return;checkpoint();for(const auto& fx:t.effects)closePluginEditor(track,fx.slot);audio.clearEffect(track);t.effects.clear();t.parameters.erase(std::remove_if(t.parameters.begin(),t.parameters.end(),[](const auto& l){return l.slot>0;}),t.parameters.end());changed(false);message(tr("Effect removed."));return;}
    if(row.kind==PluginCatalog::BrowserRow::Kind::engine){const int family=instrumentFamily(t.kind);if(row.engine!=family){message(tr("This track keeps its own FaseGroove engine."),true);return;}checkpoint();closePluginEditor(track,0);audio.useInternal(track);t.instrument="internal";t.instrumentPath.clear();t.instrumentId.clear();t.instrumentState.clear();changed(false);message(tr("Built-in engine restored."));return;}
    const auto plugins=pluginCatalog.state().plugins;const juce::PluginDescription* description=nullptr;for(const auto& candidate:plugins)if(PluginCatalog::pluginKey(candidate)==row.key){description=&candidate;break;}if(!description){message(tr("The plugin catalogue changed. Select the plugin again."),true);return;}
    if(effectsBrowser){if(!description->isInstrument){loadEffectDescription(track,*description,{},false,browserEffectSlot);pluginBrowser.setVisible(false);}}else{loadPluginDescription(track,*description);if(!description->isInstrument)pluginBrowser.setVisible(false);}
}
void MainComponent::loadPluginDescription(int track,const juce::PluginDescription& description,const juce::String& state,bool restoring){
    if(!description.isInstrument){loadEffectDescription(track,description,state,restoring);return;}
    if(!restoring)checkpoint();closePluginEditor(track,0);++loadingPlugin;loadPluginButton.setEnabled(false);
    juce::Component::SafePointer<MainComponent> safe(this);
    audio.loadPlugin(track,description,[safe,track,description,state,restoring](juce::Result r){
        if(safe==nullptr)return;
        --safe->loadingPlugin;safe->loadPluginButton.setEnabled(safe->loadingPlugin==0);
        if(r.failed()){safe->message(r.getErrorMessage(),true);safe->refreshTrack();safe->arrangement.repaint();return;}
        auto& t=safe->project.tracks[(size_t)track];t.instrument="plugin";
        t.instrumentPath=description.fileOrIdentifier.toStdString();
        if(auto xml=description.createXml())t.instrumentId=xml->toString().toStdString();
        t.instrumentState=state.toStdString();
        if(state.isNotEmpty())safe->audio.restorePluginState(track,state);
        if(!restoring)t.parameters.erase(std::remove_if(t.parameters.begin(),t.parameters.end(),[](const auto& l){return l.slot==0;}),t.parameters.end());safe->resetAutomationTarget(track);safe->resolveAutomationParameters(track,0);
        if(restoring){safe->audio.updateMix(safe->project);safe->refreshTrack();}else safe->changed(false);
        safe->message(tr("Plugin ready")+": "+description.name);
        // A freshly loaded instrument opens its editor straight away, centred; the user
        // closes it when done, as the other tools behave.
        if(!restoring)safe->openHostedEditor(track,0);
    });
}
void MainComponent::showPluginMenu(juce::Component*){
    if(loadingPlugin){message(tr("Wait for the instrument to finish loading."));return;}
    effectsBrowser=false;browserEffectSlot=0;browserTrackSession=project.tracks[(size_t)selectedTrack].sessionId;const auto& t=project.tracks[(size_t)selectedTrack];const int family=instrumentFamily(t.kind);juce::String key;
    if(t.instrument=="plugin")if(auto xml=juce::XmlDocument::parse(utf(t.instrumentId))){juce::PluginDescription description;if(description.loadFromXml(*xml))key=PluginCatalog::pluginKey(description);}
    if(key.isEmpty())key="fg\x1f"+juce::String(family);
    pluginBrowser.show(family,key);
}
void MainComponent::importFile(const juce::File& f,double placeAt){auto imported=clip();const double duration=clipIndex()<0?4.:clipLength();size_t used=0;for(size_t ti=0;ti<project.tracks.size();++ti)for(size_t ci=0;ci<project.tracks[ti].clips.size();++ci)if((int)ti!=selectedTrack||(int)ci!=clipIndex())used+=project.tracks[ti].clips[ci].notes.size()+project.tracks[ti].clips[ci].loopSource.size();auto r=importMidi(f,imported,duration,used>=maxProjectNotes?0:maxProjectNotes-used);if(r.failed()){message(r.getErrorMessage(),true);return;}if(clipIndex()<0){imported.span=duration;auto edited=project;const int placed=edited.placeClip(selectedTrack,std::move(imported),arrangement.snapBeat(placeAt>=0?placeAt:timelinePosition()));if(placed<0){message("Clip edit exceeds the project limits.",true);return;}checkpoint();project=std::move(edited);selectClip(selectedTrack,placed);}else{checkpoint();clip()=std::move(imported);}piano.clearSelection();changed();message("MIDI importato nel clip selezionato.");}

void MainComponent::showSampleBrowser(bool visible){
    browserVisible=visible;browserToggle.setToggleState(visible,juce::dontSendNotification);Localisation::setSetting("sampleBrowserVisible",visible?"1":"0");
    if(visible&&page!=Page::arrangement)showArrangement();resized();
}
bool MainComponent::isInterestedInDragSource(const SourceDetails& details){return !editingRestricted()&&details.description.toString().startsWith(SampleBrowser::dragPrefix);}
void MainComponent::itemDragMove(const SourceDetails& details){
    int track=-1;
    if(page==Page::arrangement&&arrangementPort.getBounds().contains(details.localPosition)){const auto point=arrangement.getLocalPoint(this,details.localPosition);const int row=arrangement.rowAt(point.y);if(row>=0&&point.y>=arrangementPort.getViewArea().getY()+rulerHeight)track=trackAtRow(row);}
    if(track!=arrangement.dropTrack){arrangement.dropTrack=track;arrangement.repaint();}
}
void MainComponent::itemDragExit(const SourceDetails&){if(arrangement.dropTrack>=0){arrangement.dropTrack=-1;arrangement.repaint();}}
void MainComponent::itemDropped(const SourceDetails& details){
    if(editingRestricted())return;
    arrangement.dropTrack=-1;arrangement.repaint();
    if(sampleBrowser.isVisible()&&sampleBrowser.getBounds().contains(details.localPosition))return; // dropped back on the browser
    const auto path=details.description.toString().fromFirstOccurrenceOf(SampleBrowser::dragPrefix,false,false);
    filesDropped(juce::StringArray(path),details.localPosition.x,details.localPosition.y);
}
bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files){if(editingRestricted())return false;for(auto f:files)if(juce::File(f).hasFileExtension("fga;mid;midi;wav;aif;aiff;flac;ogg"))return true;return false;}
void MainComponent::filesDropped(const juce::StringArray& files,int x,int y){
    if(loadingPlugin){message("Please wait for the plugins to load.");return;}
    juce::File file;for(const auto& path:files)if(isInterestedInFileDrag(juce::StringArray(path))){file=juce::File(path);break;}if(file==juce::File())return;
    if(file.hasFileExtension("fga")){open(file);return;}
    double dropBeat=-1;
    if(page==Page::arrangement&&arrangementPort.getBounds().contains(x,y)){
        const auto point=arrangement.getLocalPoint(this,juce::Point<int>(x,y));const int row=arrangement.rowAt(point.y);const auto view=arrangementPort.getViewArea();
        if(row>=0&&point.y>=view.getY()+rulerHeight){const int track=trackAtRow(row);if(point.x>=view.getX()+headerWidth){dropBeat=arrangement.snapBeat(std::max(0.,(point.x-headerWidth)*4./arrangement.pixelsPerBar));selectClip(track,project.clipAtBeat(track,dropBeat),false,dropBeat);}else select(track,selectedSection);}
    }
    if(file.hasFileExtension("mid;midi"))importFile(file,dropBeat);else loadSampleFile(selectedTrack,file);
}
void MainComponent::showChordCirclePopover(int initialSlot){
    if(editingRestricted()||loadingPlugin||!juce::isPositiveAndBelow(selectedSection,(int)project.sections.size()))return;
    if(dismissChordPopover)dismissChordPopover();
    class ChordCirclePopover : public juce::Component,private juce::Timer {
    public:
        juce::Component::SafePointer<MainComponent> owner;
        CircleOfFifths circle;juce::ToggleButton audition{"Audition"};std::vector<int> held;
        std::array<juce::TextButton,8> slotButtons;
        ActionButton randomBtn{tr("NEW CHORDS")},presetsBtn{tr("CHORD MENU...")};
        IconButton close{IconButton::Icon::close};
        int activeSlot=0;
        ChordCirclePopover(MainComponent& o,int slot):owner(&o),activeSlot(std::clamp(slot,0,std::max(0,o.project.settings.chordCount-1))){
            addAndMakeVisible(circle);addAndMakeVisible(audition);Localisation::bindText(audition,"Audition");Localisation::bindTip(audition,"Audition the selected chord on Piano");audition.onClick=[this]{if(audition.getToggleState())hear();else timerCallback();};
            circle.setFamilyFont(true);
            circle.setFreeMode(true);
            circle.onPick=[this](int root,bool minor){if(!valid())return;
                owner->checkpoint();auto& s=owner->project.settings;auto& h=owner->project.sections[(size_t)owner->selectedSection].harmony;
                if(h.enabled){if(juce::ModifierKeys::currentModifiers.isShiftDown()){h.key=root;h.scale=minor?2:0;}else{h.freeChords[(size_t)activeSlot]=sgk::freeChordCode(root-(h.key>=0?h.key:s.key),minor);h.manualChords[(size_t)activeSlot]=true;}generate(owner->project,Scope::section,0,owner->selectedSection,0);owner->changed();refreshState();hear();return;}
                if(juce::ModifierKeys::currentModifiers.isShiftDown()){
                    const bool modeMinor=sgk::scaleIsMinor(s.scale);s.key=root;if(minor!=modeMinor)s.scale=minor?2:0;
                }else{
                    const int sc=std::clamp(activeSlot,0,std::max(0,s.chordCount-1));
                    s.freeChords[(size_t)sc]=sgk::freeChordCode(root-s.key,minor);s.manualChords[(size_t)sc]=true;
                }
                owner->applyInstrumentSettings(0,true);refreshState();hear();
            };
            circle.onMoveChord=[this](int sl,int degree,int root,bool minor){if(!valid())return;
                auto& s=owner->project.settings;if(sl<0||sl>=harmonySettings().progressionLength)return;
                activeSlot=sl;owner->checkpoint();auto& h=owner->project.sections[(size_t)owner->selectedSection].harmony;if(h.enabled){if(degree>=0){h.degrees[(size_t)sl]=degree;h.freeChords[(size_t)sl]=-1;h.manualChords[(size_t)sl]=false;}else{h.freeChords[(size_t)sl]=sgk::freeChordCode(root-(h.key>=0?h.key:s.key),minor);h.manualChords[(size_t)sl]=true;}generate(owner->project,Scope::section,0,owner->selectedSection,0);owner->changed();refreshState();hear();return;}
                if(degree>=0){s.degrees[(size_t)sl]=degree;s.freeChords[(size_t)sl]=-1;s.manualChords[(size_t)sl]=false;}
                else{s.freeChords[(size_t)sl]=sgk::freeChordCode(root-s.key,minor);s.manualChords[(size_t)sl]=true;}
                owner->applyInstrumentSettings(0,true);refreshState();hear();
            };
            for(int i=0;i<8;++i){
                auto& b=slotButtons[(size_t)i];addAndMakeVisible(b);
                b.onClick=[this,i]{activeSlot=i;refreshState();hear();};
            }
            randomBtn.setPrimary(false);presetsBtn.setPrimary(false);
            addAndMakeVisible(randomBtn);addAndMakeVisible(presetsBtn);
            randomBtn.onClick=[this]{if(!valid())return;if(owner->harmonyButton.onClick)owner->harmonyButton.onClick();refreshState();};
            presetsBtn.onClick=[this]{if(!valid())return;owner->showChordMenu();};
            addAndMakeVisible(close);close.setComponentID("chord-circle-close");close.accentHover=true;close.setTooltip(tr("Close"));
            close.onClick=[this]{if(auto* callout=findParentComponentOfClass<juce::CallOutBox>())callout->dismiss();};
            setComponentID("chord-circle-body");audition.setComponentID("chord-audition");setSize(340,422);
            refreshState();
        }
        bool valid()const{return owner&&juce::isPositiveAndBelow(owner->selectedSection,(int)owner->project.sections.size());}
        void detach(){timerCallback();owner=nullptr;if(auto* callout=findParentComponentOfClass<juce::CallOutBox>())callout->dismiss();}
        ~ChordCirclePopover()override{timerCallback();}
        void timerCallback()override{stopTimer();if(owner)for(int note:held)owner->audio.liveMidi(0,juce::MidiMessage::noteOff(1,note),false);held.clear();}
        sgk::GrooveSettings harmonySettings()const{Clip context;context.position=owner->project.sectionStart(owner->selectedSection);return pianoEngineSettings(owner->project,0,owner->project.settings.seed,0,&context);}
        void hear(){timerCallback();if(!valid())return;if(!audition.getToggleState()||owner->audio.isPlaying())return;const auto s=harmonySettings();const auto chord=sgk::voiceChord(sgk::GrooveEngine::chordForSlot(s,activeSlot),s.voicing,s.centre,nullptr);for(int i=0;i<chord.rightHand.size;++i){const int note=chord.rightHand[i];held.push_back(note);owner->audio.liveMidi(0,juce::MidiMessage::noteOn(1,note,(juce::uint8)96),false);}startTimer(650);}
        void refreshState(){if(!valid()){timerCallback();return;}
            auto s=harmonySettings();activeSlot=std::clamp(activeSlot,0,s.progressionLength-1);
            std::vector<CircleOfFifths::ChordMark> chords,diatonic;
            std::array<sgk::Chord,8> cs;std::array<sgk::VoicedChord,8> voiced;
            for(int i=0;i<s.progressionLength;++i){cs[(size_t)i]=sgk::GrooveEngine::chordForSlot(s,i);voiced[(size_t)i]=sgk::voiceChord(cs[(size_t)i],s.voicing,s.centre,nullptr);}
            auto scores=sgk::GrooveEngine::harmonyScores(s,cs,voiced);std::vector<float> transitions;
            for(int i=0;i<s.progressionLength;++i){const auto& c=cs[(size_t)i];chords.push_back({c.rootPc,c.third==3,juce::String::fromUTF8(sgk::GrooveEngine::numeralForSlot(s,i).c_str()),scores.slot[(size_t)i]});transitions.push_back(scores.transition[(size_t)i]);}
            for(int i=0;i<7;++i){auto c=sgk::buildChord(s.keyRoot,s.scale,i,s.extension);diatonic.push_back({c.rootPc,c.third==3,juce::String::fromUTF8(sgk::romanNumeral(i,c).c_str()),1});}
            circle.setState(s.keyRoot,sgk::scaleIsMinor(s.scale),tr(sgk::scaleName(s.scale)),chords,transitions,activeSlot,diatonic);
            for(int i=0;i<8;++i){
                auto& b=slotButtons[(size_t)i];
                const bool vis=i<s.progressionLength;b.setVisible(vis);
                if(vis){b.setButtonText(juce::String(i+1)+": "+juce::String(fgg::pitchClassName(cs[(size_t)i].rootPc))+(cs[(size_t)i].third==3?"m":"")+"  "+juce::String((int)std::lround(scores.transition[(size_t)i]*100))+"%");b.setToggleState(i==activeSlot,juce::dontSendNotification);}
            }
            resized();repaint();
        }
        void paint(juce::Graphics& g)override{
            Daw::fillElevatedSurface(g,getLocalBounds().toFloat(),Daw::bgPanel(),Daw::panelRadius);
            Daw::drawCaps(g,tr("CIRCLE OF FIFTHS"),{12,2,getWidth()-48,14},Daw::accent);
            g.setColour(Daw::textDim);g.setFont(Daw::font(Daw::small));
            g.drawFittedText(tr("Edit chord")+" "+juce::String(activeSlot+1),{12,16,getWidth()-48,14},juce::Justification::centredLeft,1,.8f);
        }
        void resized()override{if(!valid())return;
            const auto& h=owner->project.sections[(size_t)owner->selectedSection].harmony;const int count=std::clamp(h.enabled?h.chordCount:owner->project.settings.chordCount,1,8);
            const int cols=std::min(4,count),rows=(count+cols-1)/cols;
            const int bw=(getWidth()-20)/cols;
            close.setBounds(getWidth()-30,2,24,24);
            for(int i=0;i<count;++i)slotButtons[(size_t)i].setBounds(10+(i%cols)*bw,34+(i/cols)*26,bw-4,24);
            const int topY=36+rows*26;
            circle.setBounds(10,topY,getWidth()-20,getHeight()-topY-40);
            audition.setBounds(10,getHeight()-34,80,26);randomBtn.setBounds(92,getHeight()-34,(getWidth()-108)/2,26);
            presetsBtn.setBounds(98+(getWidth()-108)/2,getHeight()-34,(getWidth()-108)/2,26);
        }
    };
    const auto target=chordEditButton.isShowing()?chordEditButton.getScreenBounds():getScreenBounds().withSizeKeepingCentre(120,28);
    auto content=std::make_unique<ChordCirclePopover>(*this,initialSlot);juce::Component::SafePointer<ChordCirclePopover> safe(content.get());
    dismissChordPopover=[safe]{if(safe)safe->detach();};juce::CallOutBox::launchAsynchronously(std::move(content),target,nullptr);
}
void MainComponent::showSpotlightSearch(){
    if(editingRestricted()||loadingPlugin)return;
    class SpotlightPalette : public juce::Component,private juce::ListBoxModel {
    public:
        struct Entry { juce::String category,title; std::function<void()> action; };
        MainComponent& owner;
        juce::TextEditor search;
        IconButton close{IconButton::Icon::close};
        juce::ListBox list{"spotlight",this};
        std::vector<Entry> allEntries,filtered;
        explicit SpotlightPalette(MainComponent& o):owner(o){
            addAndMakeVisible(search);addAndMakeVisible(list);addAndMakeVisible(close);close.setComponentID("search-close");close.accentHover=true;close.setTooltip(tr("Close"));close.onClick=[this]{if(auto* callout=findParentComponentOfClass<juce::CallOutBox>())callout->dismiss();};
            search.setTextToShowWhenEmpty(tr("Search actions, genres, tracks, sounds, VST3/AU plugins... (Enter to run)"),Daw::textFaint);
            search.onTextChange=[this]{filter(search.getText());};
            search.onReturnKey=[this]{trigger(list.getSelectedRow());};
            search.onEscapeKey=[this]{if(auto* callout=findParentComponentOfClass<juce::CallOutBox>())callout->dismiss();};
            list.setRowHeight(26);list.setColour(juce::ListBox::backgroundColourId,Daw::lcd);
            buildEntries();filter({});
            setSize(460,340);
            juce::Component::SafePointer<juce::TextEditor> safeSearch(&search);
            juce::MessageManager::callAsync([safeSearch]{if(safeSearch)safeSearch->grabKeyboardFocus();});
        }
        void buildEntries(){
            allEntries.push_back({"ACTION",tr("Generate Song"),[this]{owner.regenerate(Scope::song);}});
            allEntries.push_back({"ACTION",tr("Regenerate Selected Track"),[this]{owner.regenerate(Scope::track);}});
            allEntries.push_back({"ACTION",tr("Regenerate Selected Clip"),[this]{owner.regenerate(Scope::clip);}});
            allEntries.push_back({"ACTION",tr("New Chords (Randomise Harmony)"),[this]{if(owner.harmonyButton.onClick)owner.harmonyButton.onClick();}});
            allEntries.push_back({"ACTION",tr("Circle of Fifths Interactive Chord Editor"),[this]{owner.showChordCirclePopover(0);}});
            allEntries.push_back({"ACTION",tr("Open AI Composer"),[this]{owner.showAiComposer();}});
            allEntries.push_back({"ACTION",tr("Open Studio / Sampler / Recording"),[this]{owner.showStudio();}});
            allEntries.push_back({"ACTION",tr("Toggle Sample Browser"),[this]{owner.showSampleBrowser(!owner.browserVisible);}});
            allEntries.push_back({"ACTION",tr("Toggle Mixer"),[this]{owner.mixerToggle.triggerClick();}});
            allEntries.push_back({"ACTION",tr("Fit Song to Timeline"),[this]{owner.fitSong();}});
            for(size_t g=0;g<genres().size();++g){
                const int gi=(int)g;
                allEntries.push_back({"GENRE",utf(genres()[g]),[this,gi]{owner.genreBox.setSelectedId(gi+1,juce::sendNotificationSync);}});
            }
            for(size_t t=0;t<owner.project.tracks.size();++t){
                const int ti=(int)t;const auto tName=utf(owner.project.tracks[t].name);
                allEntries.push_back({"TRACK",tName+" ("+owner.soundSourceName(ti)+")",[this,ti]{owner.select(ti,owner.selectedSection);owner.showArrangement();}});
            }
            const int sCount=builtInSoundCount(owner.selectedTrack);
            for(int s=0;s<sCount;++s){
                const auto sName=builtInSoundName(owner.selectedTrack,s);
                allEntries.push_back({"SOUND",utf(owner.project.tracks[(size_t)owner.selectedTrack].name)+": "+sName,[this,s]{owner.useBuiltInSound(owner.selectedTrack);owner.project.tracks[(size_t)owner.selectedTrack].sound=s;owner.changed(false);}});
            }
            const auto plugins=owner.pluginCatalog.state().plugins;
            for(int p=0;p<plugins.size();++p){
                const auto desc=plugins.getReference(p);
                allEntries.push_back({desc.isInstrument?"VST/AU INST":"VST/AU FX",desc.name+" ("+desc.manufacturerName+")",[this,desc]{
                    if(desc.isInstrument)owner.loadPluginDescription(owner.selectedTrack,desc);
                    else owner.loadEffectDescription(owner.selectedTrack,desc);
                }});
            }
        }
        void filter(const juce::String& query){
            filtered.clear();const auto q=query.trim().toLowerCase();
            for(const auto& e:allEntries)if(q.isEmpty()||e.title.toLowerCase().contains(q)||e.category.toLowerCase().contains(q))filtered.push_back(e);
            list.updateContent();if(!filtered.empty())list.selectRow(0);
        }
        int getNumRows()override{return (int)filtered.size();}
        void paintListBoxItem(int row,juce::Graphics& g,int width,int height,bool selected)override{
            if(row<0||row>=(int)filtered.size())return;
            if(selected){g.setColour(Daw::accent.withAlpha(.22f));g.fillRoundedRectangle(2.f,1.f,(float)width-4.f,(float)height-2.f,4.f);}
            const auto& item=filtered[(size_t)row];
            g.setColour(Daw::accent);g.setFont(Daw::smallFont(10.0f));
            g.drawText(item.category,10,0,78,height,juce::Justification::centredLeft,true);
            g.setColour(selected?Daw::text:Daw::textDim);g.setFont(Daw::font(Daw::body,selected));
            g.drawText(item.title,92,0,width-100,height,juce::Justification::centredLeft,true);
        }
        void listBoxItemClicked(int row,const juce::MouseEvent&)override{trigger(row);}
        void trigger(int row){
            if(row<0||row>=(int)filtered.size())return;
            auto fn=filtered[(size_t)row].action;
            if(auto* callout=findParentComponentOfClass<juce::CallOutBox>())callout->dismiss();
            if(fn)fn();
        }
        void paint(juce::Graphics& g)override{
            Daw::fillElevatedSurface(g,getLocalBounds().toFloat(),Daw::bgPanel(),Daw::panelRadius);
            Daw::drawCaps(g,tr("SPOTLIGHT QUICK SEARCH (CTRL/CMD+K)"),{12,6,getWidth()-52,16},Daw::accent);
        }
        void resized()override{
            close.setBounds(getWidth()-34,3,24,20);
            search.setBounds(10,28,getWidth()-20,28);
            list.setBounds(10,62,getWidth()-20,getHeight()-72);
        }
    };
    const auto target=lcdArea.isEmpty()?getScreenBounds().withSizeKeepingCentre(200,32):localAreaToGlobal(lcdArea);
    juce::CallOutBox::launchAsynchronously(std::make_unique<SpotlightPalette>(*this),target,nullptr);
}
}
