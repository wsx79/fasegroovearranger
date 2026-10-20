// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MainComponent.h"
#include "UiText.h"
#include "../ThirdParty/fasegroovepiano/Source/engine/GrooveEngine.h"
#include <BinaryData.h>
#include <cmath>
namespace fga {
namespace {
juce::Colour colour(int t){if(t==0||t==3)return Palette::accent;if(t==1)return Palette::accent2;if(t==2)return Palette::strings;const int lane=t-4;return lane==0?Palette::accent:lane<=3?Palette::accent2:lane<=6?Palette::cymbals:lane<=10?Palette::percussion:Palette::toms;}
juce::Colour guitarColour(int lane){return lane<=1?Palette::accent:lane<=4?Palette::accent2:lane<=6?Palette::percussion:lane<=8?Palette::cymbals:Palette::toms;}
juce::String utf(const std::string& s){return juce::String::fromUTF8(s.c_str());}
void label(juce::Graphics& g,const juce::String& s,juce::Rectangle<int> r,float size=12,juce::Colour c=Palette::text){g.setColour(c);g.setFont(FamilyTheme::font(size));g.drawText(tr(s),r,juce::Justification::centredLeft);}
juce::String instrumentName(const Track& t){return t.instrument=="internal"?"Fase Groove engine":t.instrument=="sample"?juce::File(utf(t.instrumentPath)).getFileName():"VST / AU: "+(juce::File::isAbsolutePath(utf(t.instrumentPath))?juce::File(utf(t.instrumentPath)).getFileName():utf(t.instrumentPath));}
constexpr int trackHeight=36,headerWidth=216;
}
void ArrangementView::paint(juce::Graphics& g){
    g.fillAll(Palette::field);auto view=owner.arrangementPort.getViewArea();const auto& p=owner.project;
    for(int row=0;row<trackCount;++row){const size_t ti=(size_t)trackAtRow(row);int y=32+row*trackHeight;const auto& t=p.tracks[ti];
        if((int)ti==owner.selectedTrack){g.setColour(colour((int)ti).withAlpha(.045f));g.fillRect(0,y,getWidth(),trackHeight);}
        for(size_t si=0;si<p.sections.size();++si){int x=headerWidth+(int)(p.sectionStart((int)si)/4*pixelsPerBar);int w=(int)(p.sections[si].bars*pixelsPerBar);juce::Rectangle<int> cell(x+2,y+3,w-4,trackHeight-6);const auto& c=t.clips[si];bool sel=(int)ti==owner.selectedTrack&&(int)si==owner.selectedSection;
            g.setColour(colour((int)ti).withAlpha(c.notes.empty()?.035f:t.mute?.07f:sel?.24f:.12f));g.fillRoundedRectangle(cell.toFloat(),4);
            g.setColour(sel?colour((int)ti):Palette::panelEdge);g.drawRoundedRectangle(cell.toFloat(),4,sel?1.5f:.5f);
            if(w>70){int low=127,high=0;for(auto n:c.notes){low=std::min(low,n.pitch);high=std::max(high,n.pitch);}const double total=p.sections[si].bars*4.;g.setColour(colour((int)ti).withAlpha(.65f));for(auto n:c.notes){float nx=cell.getX()+4+(float)(n.start/total*(w-12));float ny=cell.getBottom()-5-(float)((n.pitch-low)/(double)std::max(1,high-low)*16);g.fillRect(nx,ny,std::max(1.f,(float)(n.length/total*(w-12))),2.f);}}
            if(c.locked)label(g,"L",{cell.getRight()-17,y+4,14,16},10,Palette::accent);
        }g.setColour(Palette::panelEdge.withAlpha(.5f));g.drawHorizontalLine(y+trackHeight-1,0,(float)getWidth());
    }
    // Track headers stay visible while the song scrolls horizontally.
    for(int row=0;row<trackCount;++row){const size_t ti=(size_t)trackAtRow(row);int y=32+row*trackHeight;const auto& t=p.tracks[ti];int x=view.getX();g.setColour((int)ti==owner.selectedTrack?Palette::control:Palette::panel);g.fillRect(x,y,headerWidth,trackHeight-1);g.setColour(colour((int)ti));g.fillRect(x+8,y+8,3,20);label(g,juce::String(row+1).paddedLeft('0',2),{x+18,y+2,26,30},10,Palette::dim);label(g,utf(t.name),{x+48,y+1,98,22},12);label(g,t.locked?"LOCK":t.instrument=="internal"?"INTERNAL":t.instrument=="sample"?"SAMPLE":"VST / AU",{x+48,y+20,104,13},8,Palette::dim);
        label(g,"M",{x+153,y+5,17,24},11,t.mute?Palette::toms:Palette::dim);label(g,"S",{x+174,y+5,17,24},11,t.solo?Palette::accent:Palette::dim);
        float meter=std::clamp(owner.audio.level((int)ti),0.f,1.f);g.setColour(Palette::cymbals.withAlpha(.7f));g.fillRect(x+199,y+trackHeight-5-(int)(meter*25),4,(int)(meter*25));
    }
    int y=view.getY();g.setColour(Palette::panel);g.fillRect(view.getX(),y,view.getWidth(),32);
    for(size_t si=0;si<p.sections.size();++si){int x=headerWidth+(int)(p.sectionStart((int)si)/4*pixelsPerBar);int w=(int)(p.sections[si].bars*pixelsPerBar);g.setColour((int)si==owner.selectedSection?Palette::accent.withAlpha(.17f):Palette::control);g.fillRect(x+2,y+2,w-4,28);label(g,utf(p.sections[si].name)+juce::String::fromUTF8("  ·  ")+juce::String(p.sections[si].bars)+" "+tr("bars"),{x+10,y,w-16,30},11,(int)si==owner.selectedSection?Palette::accent:Palette::text);}
    g.setColour(Palette::panel);g.fillRect(view.getX(),y,headerWidth,32);label(g,"INSTRUMENT / TRACK",{view.getX()+16,y,190,32},10,Palette::dim);
    const float playX=headerWidth+(float)(owner.audio.position()/4*pixelsPerBar);if(playX>view.getX()+headerWidth){g.setColour(Palette::accent2);g.fillRect(playX,(float)y+31,1.5f,(float)view.getHeight()-31);}
}
void ArrangementView::mouseDown(const juce::MouseEvent& e){
    auto v=owner.arrangementPort.getViewArea();const int ti=trackAtRow(std::clamp((e.y-32)/trackHeight,0,trackCount-1));int si=0;const double bar=(e.x-headerWidth)/pixelsPerBar;
    for(size_t i=0;i<owner.project.sections.size();++i)if(bar>=owner.project.sectionStart((int)i)/4)si=(int)i;
    si=std::clamp(si,0,(int)owner.project.sections.size()-1);
    if(e.y<v.getY()+32){if(e.x>v.getX()+headerWidth){owner.select(owner.selectedTrack,si);if(e.mods.isPopupMenu())owner.showSectionMenu(si);else owner.audio.seek(owner.project.sectionStart(si));}return;}
    if(e.x<v.getX()+headerWidth){owner.select(ti,owner.selectedSection);if(e.mods.isPopupMenu())owner.showTrackMenu(ti);else if(e.x>=v.getX()+150&&e.x<v.getX()+171){owner.checkpoint();owner.project.tracks[(size_t)ti].mute=!owner.project.tracks[(size_t)ti].mute;owner.changed(false);}else if(e.x>=v.getX()+171&&e.x<v.getX()+194){owner.checkpoint();owner.project.tracks[(size_t)ti].solo=!owner.project.tracks[(size_t)ti].solo;owner.changed(false);}else owner.showInstrument();return;}
    owner.select(ti,si);if(e.mods.isPopupMenu())owner.showClipMenu(ti,si);else owner.showEditor();
}
PianoRoll::PianoRoll(MainComponent& m):owner(m){setWantsKeyboardFocus(true);}
bool PianoRoll::ownerTrackIsDrum()const{return owner.selectedTrack>=4;}
bool PianoRoll::laneView()const{return owner.selectedTrack>=4||(owner.selectedTrack==3&&!guitarNotes);}
void PianoRoll::fitNotes(){
    int lo=127,hi=0;for(const auto& n:owner.clip().notes){lo=std::min(lo,n.pitch);hi=std::max(hi,n.pitch);}
    if(lo>hi){lo=owner.selectedTrack==1?28:48;hi=lo+24;}lo-=2;hi+=2;while(hi-lo<24){--lo;++hi;}
    lowNote=std::clamp(lo,0,103);highNote=std::clamp(hi,lowNote+24,127);
}
float PianoRoll::noteY(const Note& n)const{return laneView()?40.f+(owner.selectedTrack==3?n.lane:0)*40.f:48.f+(highNote-n.pitch)*rowHeight();}
float PianoRoll::noteWidth(const Note& n)const{return std::max(5.f,(float)((laneView()?grid*.85:n.length)*pixelsPerBeat));}
float PianoRoll::noteHeight()const{return laneView()?36.f:rowHeight()-1.f;}
int PianoRoll::laneAt(float y)const{return owner.selectedTrack==3?std::clamp((int)((y-40)/40),0,11):0;}
int PianoRoll::pitch(float y)const{return owner.selectedTrack>=4?(owner.project.tracks[(size_t)owner.selectedTrack].engine.midiNote<0?fgd::defaultNote(owner.selectedTrack-4):owner.project.tracks[(size_t)owner.selectedTrack].engine.midiNote):std::clamp(highNote-(int)((y-48)/rowHeight()),0,127);}
int PianoRoll::hit(juce::Point<float> p)const{const auto& c=owner.project.tracks[(size_t)owner.selectedTrack].clips[(size_t)owner.selectedSection];for(int i=(int)c.notes.size()-1;i>=0;--i){const auto& n=c.notes[(size_t)i];juce::Rectangle<float> r(leftMargin()+(float)(n.start*pixelsPerBeat),noteY(n),noteWidth(n),noteHeight());if(r.contains(p))return i;}return -1;}
void PianoRoll::paint(juce::Graphics& g){
    g.fillAll(Palette::background);const auto view=owner.pianoPort.getViewArea();const auto& notes=owner.clip().notes;const float left=(float)leftMargin();
    const auto gridArea=juce::Rectangle<float>(left,48,(float)getWidth()-left,std::max(40.f,velocityTop()-52));
    {
        juce::Graphics::ScopedSaveState clip(g);g.reduceClipRegion(gridArea.toNearestInt());
        if(!laneView())for(int n=lowNote;n<=highNote;++n){float y=48+(highNote-n)*rowHeight();bool black=n%12==1||n%12==3||n%12==6||n%12==8||n%12==10;if(black){g.setColour(juce::Colours::black.withAlpha(.25f));g.fillRect(left,y,(float)getWidth()-left,rowHeight());}if(n%12==0){g.setColour(Palette::panelEdge);g.fillRect(left,y+rowHeight()-1,(float)getWidth()-left,1.f);}}
        const double beats=owner.clipLength();
        for(double b=0;b<=beats;b+=grid){int x=leftMargin()+(int)(b*pixelsPerBeat);bool bar=std::abs(std::fmod(b,4.))<.0001;g.setColour(bar?Palette::accent.withAlpha(.4f):Palette::panelEdge.withAlpha(std::abs(std::fmod(b,1.))<.0001?1.f:.35f));if(!laneView())g.drawVerticalLine(x,48,velocityTop()-4);}
    }
    if(laneView()){
        const int lanes=owner.selectedTrack==3?12:1;
        for(int l=0;l<lanes;++l){const float y=40+l*40.f;
            for(double b=0;b<owner.clipLength();b+=grid){const float x=left+(float)(b*pixelsPerBeat);if(x+pixelsPerBeat*grid<view.getX()+left||x>view.getRight())continue;g.setColour(Palette::panelEdge.withAlpha(((int)(b*4)/4)%2==0?.55f:.32f));g.fillRoundedRectangle({x+1.5f,y+1.5f,std::max(2.f,(float)(pixelsPerBeat*grid)-3),37.f},3);}
        }
    }
    for(size_t i=0;i<notes.size();++i){const auto& n=notes[i];auto r=juce::Rectangle<float>(left+(float)(n.start*pixelsPerBeat),noteY(n),noteWidth(n),noteHeight()).reduced(.5f,1.f);if(!laneView()&&(n.pitch<lowNote||n.pitch>highNote))continue;const auto ink=owner.selectedTrack==3?guitarColour(n.lane):colour(owner.selectedTrack);g.setColour(ink.withAlpha(.5f+.5f*n.velocity/127.f));
        if(laneView()&&n.velocity<55)r=r.withSizeKeepingCentre(r.getWidth()*.45f,r.getHeight()*.4f);
        g.fillRoundedRectangle(r,3);g.setColour(selected.count((int)i)?Palette::text:juce::Colours::black.withAlpha(.4f));g.drawRoundedRectangle(r,3,selected.count((int)i)?2.f:.8f);
        if(laneView()&&n.velocity>=115){g.setColour(Palette::text.withAlpha(.35f));g.fillRoundedRectangle(r.withHeight(3).reduced(2,0).translated(0,2),1.5f);}
        if(!laneView()&&r.getWidth()>34){g.setColour(Palette::background);g.setFont(FamilyTheme::font(10,true));g.drawText(juce::MidiMessage::getMidiNoteName(n.pitch,true,true,4),r.reduced(3,0),juce::Justification::centredLeft);}
    }
    if(!marquee.isEmpty()){g.setColour(Palette::text.withAlpha(.08f));g.fillRect(marquee);g.setColour(Palette::text.withAlpha(.7f));g.drawRect(marquee,1);}
    int vx=view.getX();
    if(laneView()){const int lanes=owner.selectedTrack==3?12:1;for(int l=0;l<lanes;++l){int y=40+l*40;const auto ink=owner.selectedTrack==3?guitarColour(l):colour(owner.selectedTrack);auto h=juce::Rectangle<float>((float)vx+2,(float)y+1,left-4,38);g.setColour(Palette::control);g.fillRoundedRectangle(h,4);g.setColour(ink.withAlpha(.9f));g.fillRoundedRectangle(h.withWidth(3),1.5f);g.setFont(FamilyTheme::font(11,true));g.setColour(Palette::text);g.drawText(tr(owner.selectedTrack==3?fgg::laneShortName(l):utf(owner.project.tracks[(size_t)owner.selectedTrack].name)),h.reduced(20,0).withHeight(23),juce::Justification::centredLeft);g.setFont(FamilyTheme::font(9.5f));g.setColour(Palette::dim);g.drawText(owner.selectedTrack>=4?"MIDI "+juce::String(pitch(0)):tr("Guitar part"),h.withTrimmedTop(21).withTrimmedLeft(20),juce::Justification::centredLeft);}}
    else for(int n=lowNote;n<=highNote;++n){const float y=48+(highNote-n)*rowHeight();auto key=juce::Rectangle<float>((float)vx,y,left-2,rowHeight());bool black=n%12==1||n%12==3||n%12==6||n%12==8||n%12==10;g.setColour(black?Palette::panel.darker(.5f):Palette::text.withAlpha(.85f));g.fillRect(key.reduced(0,.3f));if(n%12==0){g.setColour(Palette::background);g.setFont(FamilyTheme::font(10,true));g.drawText(juce::MidiMessage::getMidiNoteName(n,true,true,4),key.reduced(3,0),juce::Justification::centredRight);}}
    // The original Piano chord strip and velocity lane, bound to this track's MIDI.
    g.setColour(Palette::panel);g.fillRect(vx,0,view.getWidth(),40);
    if(owner.selectedTrack>=4){
        for(double b=0;b<owner.clipLength();b+=1){const float x=left+(float)(b*pixelsPerBeat);if(x<view.getX()+left||x>view.getRight())continue;label(g,juce::String((int)b%4+1),{(int)x+3,23,40,14},10,(int)b%4==0?Palette::accent:Palette::dim);if((int)b%4==0)label(g,tr("Bar")+" "+juce::String((int)b/4+1),{(int)x+3,2,140,18},10,Palette::text);}
    }else{
        auto names=chordNames(owner.project.settings);const double chordBeats=owner.project.settings.chordSteps*.25;
        for(double b=0;b<owner.clipLength();b+=chordBeats){const int x=leftMargin()+(int)(b*pixelsPerBeat);if(x+(int)(chordBeats*pixelsPerBeat)<view.getX()+left||x>view.getRight())continue;auto r=juce::Rectangle<int>(x+1,2,(int)(chordBeats*pixelsPerBeat)-2,20);g.setColour(Palette::panelEdge);g.fillRoundedRectangle(r.toFloat(),4);label(g,utf(names[(size_t)((int)(b/chordBeats)%names.size())]),r.reduced(6,0),11);label(g,juce::String((int)(b/4)+1),{x+3,24,50,14},10,Palette::dim);}
    }
    g.setColour(Palette::panel);g.fillRect(vx,0,leftMargin(),40);
    g.setColour(Palette::background);g.fillRect(vx,(int)velocityTop(),view.getWidth(),getHeight()-(int)velocityTop());label(g,"VELOCITY",{vx+4,(int)velocityTop(),leftMargin()-8,(int)velocityHeight()},9,Palette::dim);
    g.setColour(Palette::panelEdge);g.drawHorizontalLine((int)velocityTop(),(float)vx,(float)view.getRight());
    if(owner.selectedTrack>=4)for(double b=0;b<owner.clipLength();b+=grid){const float x=left+(float)(b*pixelsPerBeat);if(x<view.getX()+left||x>view.getRight())continue;auto column=juce::Rectangle<float>(x+1,velocityTop()+8,std::max(3.f,(float)(pixelsPerBeat*grid)-3),velocityHeight()-4);g.setColour(Palette::panelEdge.withAlpha(((int)(b*4)/4)%2==0?.55f:.32f));g.fillRoundedRectangle(column,3);}
    for(size_t i=0;i<notes.size();++i){const auto& n=notes[i];const float x=left+(float)(n.start*pixelsPerBeat);if(x<vx+left||x>view.getRight())continue;float h=n.velocity/127.f*velocityHeight();g.setColour(selected.count((int)i)?Palette::text:colour(owner.selectedTrack).withAlpha(.8f));g.fillRect(x-1,getHeight()-12-h,3.f,h);g.fillEllipse(x-3,getHeight()-15-h,6,6);}
    const auto local=owner.audio.position()-owner.project.sectionStart(owner.selectedSection);if(local>=0&&local<owner.clipLength()){float x=left+(float)(local*pixelsPerBeat);if(x>vx+left){g.setColour(Palette::text);g.fillRect(x,24.f,1.5f,(float)getHeight()-24);}}
}
void PianoRoll::editVelocity(juce::Point<float> point){
    const double at=beat(point.x);int nearest=-1;double distance=12./pixelsPerBeat;
    for(size_t i=0;i<owner.clip().notes.size();++i){const auto& n=owner.clip().notes[i];if(std::abs(n.start-at)<distance){nearest=(int)i;distance=std::abs(n.start-at);}}
    if(nearest<0)return;const auto time=owner.clip().notes[(size_t)nearest].start;
    const int value=std::clamp((int)((getHeight()-12-point.y)/velocityHeight()*127),1,127);
    for(auto& n:owner.clip().notes)if(std::abs(n.start-time)<1./960.)n.velocity=value;
    repaint();
}

void PianoRoll::mouseDown(const juce::MouseEvent& e){
    grabKeyboardFocus();editingVelocity=false;if(e.x<owner.pianoPort.getViewArea().getX()+leftMargin()||e.y<40)return;
    if(e.y>=velocityTop()){owner.checkpoint();editingVelocity=true;editVelocity(e.position);return;}
    if(laneView()&&e.y>=40+(owner.selectedTrack==3?12:1)*40)return;
    if(!laneView()&&e.y<48)return;
    int index=hit(e.position);if(tool==1&&index<0&&!e.mods.isPopupMenu()){selecting=true;anchor=e.position;marquee={};if(!e.mods.isShiftDown())selected.clear();repaint();return;}if(e.mods.isPopupMenu()||tool==2){if(index>=0){owner.checkpoint();if(laneView()&&owner.selectedTrack==3){const auto note=owner.clip().notes[(size_t)index];auto& notes=owner.clip().notes;notes.erase(std::remove_if(notes.begin(),notes.end(),[&](const Note& n){return n.lane==note.lane&&std::abs(n.start-note.start)<.18;}),notes.end());}else owner.clip().notes.erase(owner.clip().notes.begin()+index);selected.clear();owner.changed();}return;}
    owner.checkpoint();anchor=e.position;resizing=false;velocityDrag=e.mods.isAltDown();before=owner.clip().notes;
    if(index>=0){if(e.mods.isShiftDown()){if(selected.count(index))selected.erase(index);else selected.insert(index);}else if(!selected.count(index)){selected.clear();selected.insert(index);if(laneView()&&owner.selectedTrack==3){const auto hitNote=before[(size_t)index];for(size_t k=0;k<before.size();++k)if(before[k].lane==hitNote.lane&&std::abs(before[k].start-hitNote.start)<.18)selected.insert((int)k);}}const auto& n=before[(size_t)index];resizing=!laneView()&&e.x>=leftMargin()+(n.start+n.length)*pixelsPerBeat-6&&!velocityDrag;}
    else{if(!e.mods.isShiftDown())selected.clear();double start=std::floor(beat(e.position.x)/grid)*grid;if(start>=owner.clipLength())return;if(laneView()&&owner.selectedTrack==3){const int lane=laneAt(e.position.y);const auto& settings=owner.project.settings;const int slot=((int)(start/(settings.chordSteps*.25)))%settings.chordCount;auto chord=settings.freeChords[(size_t)slot]>=0?fgg::codeChord(settings.key,settings.freeChords[(size_t)slot],std::min(2,settings.extension)):fgg::diatonicChord(settings.key,settings.scale,settings.degrees[(size_t)slot],std::min(2,settings.extension));auto notes=fgg::laneNotes((fgg::Lane)lane,fgg::tonesOf(chord),(int)(start*4)%16);for(int note:notes){owner.clip().notes.push_back({start,std::min(grid,owner.clipLength()-start),note,brushVelocity,lane});selected.insert((int)owner.clip().notes.size()-1);}}
        else{owner.clip().notes.push_back({start,std::min(grid,owner.clipLength()-start),pitch(e.position.y),owner.selectedTrack>=4?brushVelocity:(int)owner.velocitySlider.getValue(),owner.selectedTrack>=4?owner.selectedTrack-4:0});selected.insert((int)owner.clip().notes.size()-1);}if(laneView()&&flamBrush&&owner.clip().notes.size()>before.size()){auto note=owner.clip().notes.back();note.start=std::max(0.,note.start-.08);note.length=.04;note.velocity=45;owner.clip().notes.push_back(note);}before=owner.clip().notes;}
    repaint();
}
void PianoRoll::mouseDrag(const juce::MouseEvent& e){
    if(selecting){marquee=juce::Rectangle<float>(anchor,e.position);selected.clear();for(size_t i=0;i<owner.clip().notes.size();++i){const auto& n=owner.clip().notes[i];if(marquee.intersects(juce::Rectangle<float>(leftMargin()+(float)(n.start*pixelsPerBeat),noteY(n),noteWidth(n),noteHeight())))selected.insert((int)i);}repaint();return;}
    if(editingVelocity){editVelocity(e.position);return;}
    if(before.empty()||selected.empty())return;const double delta=std::round((e.position.x-anchor.x)/pixelsPerBeat/grid)*grid;const int deltaPitch=-(int)std::round((e.position.y-anchor.y)/rowHeight());auto& notes=owner.clip().notes;
    for(int i:selected)if(i>=0&&i<(int)notes.size()&&i<(int)before.size()){const auto n=before[(size_t)i];auto& now=notes[(size_t)i];if(velocityDrag)now.velocity=std::clamp(n.velocity+(int)(anchor.y-e.position.y),1,127);else if(resizing)now.length=std::clamp(n.length+delta,std::min(.0625,owner.clipLength()-n.start),owner.clipLength()-n.start);else{now.start=std::clamp(n.start+delta,0.,std::max(0.,owner.clipLength()-n.length));if(!laneView())now.pitch=std::clamp(n.pitch+deltaPitch,0,127);else if(owner.selectedTrack==3)now.lane=std::clamp(n.lane+(int)std::round((e.position.y-anchor.y)/40.),0,11);}}
    repaint();
}
void PianoRoll::mouseUp(const juce::MouseEvent&){
    if(selecting){selecting=false;marquee={};repaint();return;}
    if(editingVelocity){editingVelocity=false;owner.changed();return;}
    if(before.empty())return;
    before.clear();std::vector<Note> wanted;
    for(int i:selected)if(i>=0&&i<(int)owner.clip().notes.size())wanted.push_back(owner.clip().notes[(size_t)i]);
    tidyNotes(owner.clip(),owner.clipLength());selected.clear();
    for(size_t i=0;i<owner.clip().notes.size();++i){const auto& n=owner.clip().notes[i];for(const auto& old:wanted)if(n.pitch==old.pitch&&std::abs(n.start-old.start)<1./960.){selected.insert((int)i);break;}}
    owner.changed();
}
bool PianoRoll::verifyEditing(){
    const auto saved=owner.clip();const int savedTool=tool;const bool savedFlam=flamBrush;tool=0;flamBrush=false;owner.clip().notes.clear();fitNotes();
    const auto source=juce::Desktop::getInstance().getMainMouseSource();const auto time=juce::Time::getCurrentTime();
    auto event=[&](juce::Point<float> point){return juce::MouseEvent(source,point,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,this,this,time,point,time,1,false);};
    const float x=leftMargin()+(float)(2*pixelsPerBeat)+2;const float y=laneView()?58:noteY({2,.25,(lowNote+highNote)/2,100,0})+noteHeight()/2;
    mouseDown(event({x,y}));mouseUp(event({x,y}));bool ok=!owner.clip().notes.empty();
    if(ok){const auto n=owner.clip().notes.front();auto at=juce::Point<float>(leftMargin()+(float)(n.start*pixelsPerBeat),getHeight()-12-velocityHeight()*.5f);mouseDown(event(at));mouseUp(event(at));ok=owner.clip().notes.front().velocity>=60&&owner.clip().notes.front().velocity<=66&&ok;
        const auto beforeMove=owner.clip().notes.front();at={leftMargin()+(float)(beforeMove.start*pixelsPerBeat)+2,noteY(beforeMove)+noteHeight()/2};mouseDown(event(at));mouseDrag(event(at.translated((float)pixelsPerBeat,0)));mouseUp(event(at.translated((float)pixelsPerBeat,0)));ok=std::abs(owner.clip().notes.front().start-beforeMove.start-1)<1./960.&&ok;
        if(!laneView()){const auto beforeResize=owner.clip().notes.front();at={leftMargin()+(float)((beforeResize.start+beforeResize.length)*pixelsPerBeat)-1,noteY(beforeResize)+noteHeight()/2};mouseDown(event(at));mouseDrag(event(at.translated((float)(grid*pixelsPerBeat),0)));mouseUp(event(at.translated((float)(grid*pixelsPerBeat),0)));ok=owner.clip().notes.front().length>beforeResize.length&&ok;}
        deleteSelected();ok=owner.clip().notes.empty()&&ok;
    }
    owner.clip()=saved;tool=savedTool;flamBrush=savedFlam;clearSelection();owner.changed();fitNotes();return ok;
}
void PianoRoll::deleteSelected(){if(selected.empty())return;owner.checkpoint();auto& notes=owner.clip().notes;for(auto i=selected.rbegin();i!=selected.rend();++i)if(*i>=0&&*i<(int)notes.size())notes.erase(notes.begin()+*i);selected.clear();owner.changed();}
bool PianoRoll::keyPressed(const juce::KeyPress& key){if(key==juce::KeyPress::deleteKey||key==juce::KeyPress::backspaceKey){deleteSelected();return true;}if(key.getModifiers().isCommandDown()&&key.getKeyCode()=='C'){owner.copyButton.triggerClick();return true;}if(key.getModifiers().isCommandDown()&&key.getKeyCode()=='V'){owner.pasteButton.triggerClick();return true;}if(key.getModifiers().isCommandDown()&&key.getKeyCode()=='A'){selected.clear();for(size_t i=0;i<owner.clip().notes.size();++i)selected.insert((int)i);repaint();return true;}return owner.keyPressed(key);}

class MainComponent::PluginWindow : public juce::DocumentWindow {
public:
    explicit PluginWindow(juce::AudioPluginInstance& p):DocumentWindow(p.getName(),Palette::panel,DocumentWindow::closeButton){setUsingNativeTitleBar(true);auto* editor=p.createEditorIfNeeded();if(!editor)editor=new juce::GenericAudioProcessorEditor(p);setContentOwned(editor,true);setResizable(editor->isResizable(),false);centreWithSize(getWidth(),getHeight());setVisible(true);}
    void closeButtonPressed()override{setVisible(false);}
};
MainComponent::MainComponent(){
    Localisation::loadSaved();if(Localisation::tipsEnabled())tooltips=std::make_unique<juce::TooltipWindow>(this,650);setLookAndFeel(&theme);setWantsKeyboardFocus(true);project=newProject();selectedTrack=4;
    for(juce::Component* c:std::initializer_list<juce::Component*>{&backButton,&instrumentButton,&editButton,&leadButton,&arrangeButton,&guitarNotesButton,&durationLabel,&sectionSelector,&panelSections})addAndMakeVisible(c);
    instrumentPort.setViewedComponent(&instrumentPanel,false);instrumentPort.setScrollBarsShown(true,false);addAndMakeVisible(instrumentPort);
    backButton.onClick=[this]{showArrangement();};instrumentButton.onClick=[this]{showInstrument();};editButton.onClick=[this]{showEditor();};
    addAndMakeVisible(fitNotesButton);muteButton.setClickingTogglesState(true);soloButton.setClickingTogglesState(true);
    int toolIndex=0;for(auto* button:{&drawTool,&selectTool,&eraseTool}){addAndMakeVisible(button);button->setClickingTogglesState(true);button->setRadioGroupId(4001);const int index=toolIndex++;button->onClick=[this,index]{piano.tool=index;};}drawTool.setToggleState(true,juce::dontSendNotification);
    int brushIndex=0;for(auto* button:{&brushAccent,&brushHit,&brushGhost,&brushFlam}){addAndMakeVisible(button);button->setClickingTogglesState(true);button->setRadioGroupId(4002);const int index=brushIndex++;button->onClick=[this,index]{piano.brushVelocity=index==0?127:index==2?45:100;piano.flamBrush=index==3;};button->setTooltip(tr("Choose the hit to draw, as in the Drums and Guitar editor."));button->setConnectedEdges(juce::Button::ConnectedOnLeft|juce::Button::ConnectedOnRight);}brushAccent.setConnectedEdges(juce::Button::ConnectedOnRight);brushFlam.setConnectedEdges(juce::Button::ConnectedOnLeft);brushHit.setToggleState(true,juce::dontSendNotification);
    fitNotesButton.onClick=[this]{piano.fitNotes();piano.repaint();};
    guitarNotesButton.onClick=[this]{piano.guitarNotes=!piano.guitarNotes;showEditor();};
    leadButton.onClick=[this]{checkpoint();project.settings.guitarLead=!project.settings.guitarLead;generate(project,Scope::track,0,selectedSection);generate(project,Scope::track,3,selectedSection);changed();};
    durationLabel.getProperties().set("fgaTip","Song length follows genre, harmonic phrases and groove development.");durationLabel.setTooltip(tr("Song length follows genre, harmonic phrases and groove development."));
    arrangeButton.onClick=[this]{if(loadingPlugin)return;for(const auto& t:project.tracks){if(t.locked){message("Sblocca le tracce prima di ricostruire la struttura. GENERATE SONG mantiene la struttura e i blocchi.");return;}for(const auto& c:t.clips)if(c.locked){message("Sblocca i clip prima di ricostruire la struttura.");return;}}checkpoint();audio.stop();arrangeSong(project);generate(project,Scope::song,0,0,(std::uint32_t)juce::Random::getSystemRandom().nextInt());selectedSection=0;changed();showArrangement();};
    panelSections.onChange=[this]{auto names=instrumentPanel.sectionNames();int index=panelSections.getSelectedItemIndex();if(index>=0&&index<names.size())instrumentPort.setViewPosition(0,instrumentPanel.sectionPosition(names[index]));};
    sectionSelector.onChange=[this]{if(!refreshing){select(selectedTrack,sectionSelector.getSelectedItemIndex());if(page==Page::editor)showEditor();}};
    addAndMakeVisible(chordEditButton);chordEditButton.onClick=[this]{showChordMenu();};
    arrangementPort.setViewedComponent(&arrangement,false);pianoPort.setViewedComponent(&piano,false);arrangementPort.setScrollBarsShown(true,true);pianoPort.setScrollBarsShown(true,true);addAndMakeVisible(arrangementPort);addAndMakeVisible(pianoPort);
    for(juce::Component* c:std::initializer_list<juce::Component*>{&newButton,&openButton,&saveButton,&saveAsButton,&exportButton,&settingsButton,&aboutButton,&helpButton,&playButton,&stopButton,&rewindButton,&undoButton,&redoButton,&loopButton,&songButton,&trackButton,&sectionButton,&clipButton,&harmonyButton,&addSectionButton,&genreBox,&keyBox,&scaleBox,&extensionBox,&voicingBox,&progressionBox,&bassBox,&stringsBox,&tempoSlider,&swingSlider,&densitySlider,&humanizeSlider,&masterSlider,&zoomSlider,&positionLabel,&chordsLabel,&statusLabel,&soundButton,&pluginEditorButton,&sampleButton,&loadPluginButton,&gainSlider,&panSlider,&velocitySlider,&muteButton,&soloButton,&lockButton,&lockClipButton,&soundBox,&gridBox,&quantiseButton,&transposeUp,&transposeDown,&deleteNotes,&importButton,&copyButton,&pasteButton,&sectionLoopButton,&inspectorLabel,&editorLabel})addAndMakeVisible(c);
    auto slider=[this](juce::Slider& s,double lo,double hi,double step,const juce::String& suffix){s.setRange(lo,hi,step);s.setSliderStyle(juce::Slider::LinearHorizontal);s.setTextBoxStyle(juce::Slider::TextBoxRight,false,64,22);s.setTextValueSuffix(suffix);s.onDragStart=[this]{checkpoint();};};
    slider(tempoSlider,30,300,.1," BPM");slider(swingSlider,0,150,1,"% swing");slider(densitySlider,0,100,1,"% notes");slider(humanizeSlider,0,100,1,"% feel");slider(masterSlider,0,125,1,"% out");slider(zoomSlider,4,160,1," zoom");slider(gainSlider,0,200,1,"% gain");slider(panSlider,-100,100,1," pan");slider(velocitySlider,1,127,1," vel");velocitySlider.setValue(96);
    for(size_t i=0;i<genres().size();++i)genreBox.addItem(utf(genres()[i]),(int)i+1);
    for(int i=0;i<12;++i)keyBox.addItem(sgk::noteName(i),i+1);
    for(int i=0;i<13;++i)scaleBox.addItem(sgk::scaleName(i),i+1);
    for(int i=0;i<11;++i){extensionBox.addItem(sgk::extensionName(i),i+1);voicingBox.addItem(sgk::voicingName(i),i+1);}
    const auto& progressions=sgk::progressionPresets();for(size_t i=0;i<progressions.size();++i)progressionBox.addItem(progressions[i].name,(int)i+1);progressionBox.setTextWhenNothingSelected("PROGRESSION");
    for(int i=0;i<sgk::numBassModes;++i)bassBox.addItem(sgk::bassModeName(i),i+1);for(int i=0;i<sgk::numStringsModes;++i)stringsBox.addItem(sgk::stringsModeName(i),i+1);
    gridBox.addItem("1/4",1);gridBox.addItem("1/8",2);gridBox.addItem("1/16",3);gridBox.addItem("1/32",4);gridBox.setSelectedId(3);
    auto setting=[this](std::function<void()> fn){if(refreshing)return;checkpoint();fn();project.normalise();changed(false);message("Impostazioni aggiornate. GENERATE per applicarle al MIDI; il MIDI esistente resta modificabile.");};
    genreBox.onChange=[this,setting]{setting([this]{applyGenre(project,genreBox.getSelectedItemIndex());});message("Genre ready: NEW ARRANGEMENT builds its form from the groove; GENERATE SONG keeps existing sections.");};
    keyBox.onChange=[this,setting]{setting([this]{project.settings.key=keyBox.getSelectedItemIndex();});};
    scaleBox.onChange=[this,setting]{setting([this]{project.settings.scale=scaleBox.getSelectedItemIndex();});};
    extensionBox.onChange=[this,setting]{setting([this]{project.settings.extension=extensionBox.getSelectedItemIndex();});};
    voicingBox.onChange=[this,setting]{setting([this]{project.settings.voicing=voicingBox.getSelectedItemIndex();});};
    progressionBox.onChange=[this,setting]{setting([this]{chooseProgression(project,progressionBox.getSelectedItemIndex());});};
    bassBox.onChange=[this,setting]{setting([this]{project.settings.bassMode=bassBox.getSelectedItemIndex();});};
    stringsBox.onChange=[this,setting]{setting([this]{project.settings.stringsMode=stringsBox.getSelectedItemIndex();});};
    tempoSlider.onValueChange=[this]{if(!refreshing){project.settings.bpm=tempoSlider.getValue();changed(false);}};
    swingSlider.onValueChange=[this]{if(!refreshing){project.settings.swing=swingSlider.getValue()/100.;dirty=true;}};
    densitySlider.onValueChange=[this]{if(!refreshing){project.settings.density=densitySlider.getValue()/100.;dirty=true;}};
    humanizeSlider.onValueChange=[this]{if(!refreshing){project.settings.humanize=humanizeSlider.getValue()/100.;dirty=true;}};
    masterSlider.onValueChange=[this]{if(!refreshing){project.masterGain=masterSlider.getValue()/100.;changed(false);}};
    zoomSlider.onValueChange=[this]{if(page==Page::editor)piano.pixelsPerBeat=zoomSlider.getValue();else arrangement.pixelsPerBar=zoomSlider.getValue();updateSizes();};zoomSlider.setValue(6,juce::sendNotificationSync);
    gainSlider.onValueChange=[this]{if(!refreshing){project.tracks[(size_t)selectedTrack].gain=gainSlider.getValue()/100.;changed(false);}};
    panSlider.onValueChange=[this]{if(!refreshing){project.tracks[(size_t)selectedTrack].pan=panSlider.getValue()/100.;changed(false);}};
    velocitySlider.onDragStart=[this]{if(!piano.selected.empty())checkpoint();};velocitySlider.onValueChange=[this]{if(!refreshing&&!piano.selected.empty()){for(int i:piano.selected)if(i>=0&&i<(int)clip().notes.size())clip().notes[(size_t)i].velocity=(int)velocitySlider.getValue();changed();}};
    auto toggle=[this](juce::Button& b,bool Track::*field){b.onClick=[this,&b,field]{checkpoint();project.tracks[(size_t)selectedTrack].*field=b.getToggleState();changed(false);};};toggle(muteButton,&Track::mute);toggle(soloButton,&Track::solo);toggle(lockButton,&Track::locked);
    lockClipButton.onClick=[this]{checkpoint();clip().locked=lockClipButton.getToggleState();changed(false);};
    playButton.onClick=[this]{audio.play(!audio.isPlaying());};stopButton.onClick=[this]{audio.stop();};rewindButton.onClick=[this]{audio.seek(0);};
    loopButton.onClick=[this]{project.loop=loopButton.getToggleState();audio.setLoop(project.loop,project.loopStart,project.loopEnd);dirty=true;};
    sectionLoopButton.onClick=[this]{project.loopStart=project.sectionStart(selectedSection);project.loopEnd=project.loopStart+clipLength();project.loop=true;audio.setLoop(true,project.loopStart,project.loopEnd);refreshControls();};
    songButton.onClick=[this]{regenerate(Scope::song);};trackButton.onClick=[this]{regenerate(Scope::track);};sectionButton.onClick=[this]{regenerate(Scope::section);};clipButton.onClick=[this]{regenerate(Scope::clip);};
    harmonyButton.onClick=[this]{checkpoint();randomiseHarmony(project,(std::uint32_t)juce::Random::getSystemRandom().nextInt());changed(false);message("Nuova armonia pronta: GENERATE SONG per applicarla a tutte le tracce sbloccate.");};
    undoButton.onClick=[this]{undo(false);};redoButton.onClick=[this]{undo(true);};
    quantiseButton.onClick=[this]{modifySelected([this](Clip& c){quantise(c,piano.grid);});};transposeUp.onClick=[this]{modifySelected([](Clip& c){transpose(c,12);});};transposeDown.onClick=[this]{modifySelected([](Clip& c){transpose(c,-12);});};deleteNotes.onClick=[this]{piano.deleteSelected();};
    gridBox.onChange=[this]{piano.grid=std::pow(.5,gridBox.getSelectedItemIndex());piano.repaint();};
    copyButton.onClick=[this]{clipboard.clear();for(size_t i=0;i<clip().notes.size();++i)if(piano.selected.empty()||piano.selected.count((int)i))clipboard.push_back(clip().notes[i]);message("MIDI copiato. PASTE inserisce nella sezione selezionata.");};
    pasteButton.onClick=[this]{if(clipboard.empty())return;checkpoint();clip().notes.insert(clip().notes.end(),clipboard.begin(),clipboard.end());tidyNotes(clip(),clipLength());piano.clearSelection();changed();};
    soundButton.setButtonText("BUILT-IN");soundButton.onClick=[this]{if(loadingPlugin)return;checkpoint();closePluginEditor();audio.useInternal(selectedTrack);auto& t=project.tracks[(size_t)selectedTrack];t.instrument="internal";t.instrumentPath.clear();t.instrumentId.clear();t.instrumentState.clear();changed(false);};
    sampleButton.onClick=[this]{if(loadingPlugin)return;int track=selectedTrack;chooseFile("Campione personale","*.wav;*.aif;*.aiff;*.flac;*.ogg",juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this,track](juce::File file){loadSampleFile(track,file);});};
    loadPluginButton.onClick=[this]{showPluginMenu();};
    pluginEditorButton.onClick=[this]{if(loadingPlugin)return;if(auto* p=audio.plugin(selectedTrack)){pluginWindow.reset();pluginWindow=std::make_unique<PluginWindow>(*p);}else message("Nessun plugin caricato su questa traccia.");};
    soundBox.onChange=[this]{if(refreshing)return;checkpoint();project.tracks[(size_t)selectedTrack].sound=soundBox.getSelectedItemIndex();changed(false);};
    importButton.onClick=[this]{chooseFile("Importa note nella sezione selezionata","*.mid;*.midi",juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this](juce::File f){importFile(f);});};
    addSectionButton.onClick=[this]{checkpoint();project.insertSection(selectedSection+1,{Section::main,8,"Main"});selectedSection++;generate(project,Scope::section,selectedTrack,selectedSection,(std::uint32_t)juce::Random::getSystemRandom().nextInt());changed();};
    saveButton.onClick=[this]{save(false);};saveAsButton.onClick=[this]{save(true);};
    openButton.onClick=[this]{chooseFile("Apri progetto Fase Groove Arranger","*.fga",juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this](juce::File f){open(f);});};
    newButton.onClick=[this]{if(loadingPlugin)return;if(!requestClose())return;audio.stop();pluginWindow.reset();checkpoint();for(int i=0;i<trackCount;++i)audio.useInternal(i);project=newProject(genreBox.getSelectedItemIndex());projectFile=juce::File();selectedSection=0;selectedTrack=4;changed();showArrangement();};
    exportButton.onClick=[this]{exportMenu();};settingsButton.onClick=[this]{showAudioSettings();};aboutButton.onClick=[this]{showAbout();};helpButton.onClick=[this]{showGuide(Localisation::current()!=Localisation::Language::italian);};
    playButton.primary=true;playButton.onColour=Palette::accent2;playButton.setTooltip(tr("Start or pause playback."));stopButton.setTooltip(tr("Stop playback and return to the beginning."));rewindButton.setTooltip(tr("Back to the beginning."));undoButton.setTooltip(tr("Undo"));redoButton.setTooltip(tr("Redo"));
    rewindButton.setConnectedEdges(juce::Button::ConnectedOnRight);playButton.setConnectedEdges(juce::Button::ConnectedOnLeft|juce::Button::ConnectedOnRight);stopButton.setConnectedEdges(juce::Button::ConnectedOnLeft);
    for(auto* b:{&songButton,&clipButton}){b->setColour(juce::TextButton::buttonColourId,Palette::accent);b->setColour(juce::TextButton::textColourOffId,Palette::background);}
    for(const auto& entry:std::initializer_list<std::pair<juce::Component*,const char*>>{{&genreBox,"GROOVE"},{&keyBox,"KEY"},{&scaleBox,"Scale / mode"},{&extensionBox,"Extensions"},{&voicingBox,"Voicing"},{&progressionBox,"PROGRESSION"},{&tempoSlider,"TEMPO"},{&swingSlider,"SWING"},{&densitySlider,"DENSITY"},{&humanizeSlider,"HUMANIZE"},{&masterSlider,"Volume"},{&gainSlider,"Volume"},{&panSlider,"PAN"},{&velocitySlider,"VELOCITY"},{&zoomSlider,"ZOOM"},{&durationLabel,"NEW ARRANGEMENT"}})entry.first->getProperties().set("fgaControl",entry.second);
    piano.getProperties().set("fgaTip","Draw notes, drag to move, drag the edge for duration; Alt-drag changes velocity.");

    addChildComponent(options);addChildComponent(updateOverlay);
    options.onLanguageChanged=[this]{applyLanguage();};
    options.onScaleChosen=[this](float scale){if(onScaleChosen)onScaleChosen(scale);};
    options.onTipsChanged=[this](bool enabled){Localisation::setTipsEnabled(enabled);applyLanguage();};
    options.onAudioSettings=[this]{showAudioSettings();};
    options.onDocument=[this](int id){if(id==0)showGuide(Localisation::current()!=Localisation::Language::italian);if(id==1){const bool it=Localisation::current()==Localisation::Language::italian;showDocument(tr("Privacy and permissions"),juce::String::fromUTF8(it?BinaryData::PRIVACY_it_md:BinaryData::PRIVACY_md,it?BinaryData::PRIVACY_it_mdSize:BinaryData::PRIVACY_mdSize));}if(id==2)showDocument(tr("Licences and attributions"),juce::String::fromUTF8(BinaryData::THIRD_PARTY_NOTICES_md,BinaryData::THIRD_PARTY_NOTICES_mdSize));};
    options.onCheckUpdates=[this]{options.checkUpdatesButton.setEnabled(false);options.checkUpdatesButton.setButtonText(tr("CHECKING..."));updater.start(true);};
    updater.onResult=[this](const Updater::Result& result,bool manual){
        options.checkUpdatesButton.setEnabled(true);options.refreshText();
        if(result.status==Updater::Result::Status::ready){updateOverlay.show(result);
        }else if(manual){
            const auto text=result.status==Updater::Result::Status::upToDate?tr("You have the latest version."):result.status==Updater::Result::Status::noRelease?tr("No published release yet. Development builds are in GitHub Actions."):tr("Could not check for updates. Check your internet connection and try again.");
            message(text);juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,JUCE_APPLICATION_NAME_STRING,text);
        }
    };
    if(Updater::autoCheckEnabled()&&!juce::JUCEApplication::getCommandLineParameters().contains("smoke-test"))updater.start(false);
    refreshControls();audio.publish(project);setSize(1440,980);select(4,0);showArrangement();message(juce::String::fromUTF8("Pronto · Spazio: play/pausa · Tasto destro: opzioni · Autosave ogni minuto."));startTimerHz(20);
}
MainComponent::~MainComponent(){updater.onResult=nullptr;if(musicalToolWindow)musicalToolWindow->closeButtonPressed();stopTimer();pluginWindow.reset();audio.stop();setLookAndFeel(nullptr);}
void MainComponent::closePluginEditor(){pluginWindow.reset();}
void MainComponent::checkpoint(){undoStack.push_back(project);if(undoStack.size()>40)undoStack.erase(undoStack.begin());redoStack.clear();}
void MainComponent::changed(bool schedule){project.normalise();dirty=true;if(schedule)audio.publish(project);else audio.updateMix(project);refreshControls();arrangement.repaint();piano.repaint();repaint();}
void MainComponent::applyLanguage(){
    if(Localisation::tipsEnabled())tooltips=std::make_unique<juce::TooltipWindow>(this,650);else tooltips.reset();
    refreshControls();if(page==Page::instrument)instrumentPanel.rebuild();
    options.refreshText();updateOverlay.refreshText();repaint();
}
void MainComponent::showArrangement(){page=Page::arrangement;zoomSlider.setValue(arrangement.pixelsPerBar,juce::dontSendNotification);resized();repaint();}
void MainComponent::showInstrument(){page=Page::instrument;instrumentPanel.rebuild();panelSections.clear(juce::dontSendNotification);for(auto name:instrumentPanel.sectionNames())panelSections.addItem(name,panelSections.getNumItems()+1);panelSections.setTextWhenNothingSelected("ADVANCED");resized();instrumentPort.setViewPosition(0,0);repaint();}
void MainComponent::showEditor(){page=Page::editor;zoomSlider.setValue(piano.pixelsPerBeat,juce::dontSendNotification);resized();select(selectedTrack,selectedSection);piano.grabKeyboardFocus();repaint();}
void MainComponent::select(int track,int section){selectedTrack=std::clamp(track,0,16);selectedSection=std::clamp(section,0,(int)project.sections.size()-1);piano.clearSelection();sectionSelector.setSelectedId(selectedSection+1,juce::dontSendNotification);refreshTrack();updateSizes();piano.fitNotes();pianoPort.setViewPosition(0,0);arrangement.repaint();piano.repaint();}
void MainComponent::refreshControls(){
    refreshing=true;auto& s=project.settings;durationLabel.setText(juce::String((int)(project.length()*60/s.bpm)/60)+":"+juce::String((int)(project.length()*60/s.bpm)%60).paddedLeft('0',2),juce::dontSendNotification);leadButton.setToggleState(s.guitarLead,juce::dontSendNotification);leadButton.setButtonText("SOLISTA");sectionSelector.clear(juce::dontSendNotification);for(size_t i=0;i<project.sections.size();++i)sectionSelector.addItem(utf(project.sections[i].name),((int)i)+1);sectionSelector.setSelectedId(selectedSection+1,juce::dontSendNotification);genreBox.setSelectedId(s.genre+1,juce::dontSendNotification);keyBox.setSelectedId(s.key+1,juce::dontSendNotification);scaleBox.setSelectedId(s.scale+1,juce::dontSendNotification);extensionBox.setSelectedId(s.extension+1,juce::dontSendNotification);voicingBox.setSelectedId(s.voicing+1,juce::dontSendNotification);bassBox.setSelectedId(s.bassMode+1,juce::dontSendNotification);stringsBox.setSelectedId(s.stringsMode+1,juce::dontSendNotification);
    tempoSlider.setValue(s.bpm,juce::dontSendNotification);swingSlider.setValue(s.swing*100,juce::dontSendNotification);densitySlider.setValue(s.density*100,juce::dontSendNotification);humanizeSlider.setValue(s.humanize*100,juce::dontSendNotification);masterSlider.setValue(project.masterGain*100,juce::dontSendNotification);loopButton.setToggleState(project.loop,juce::dontSendNotification);
    juce::StringArray chords;for(auto name:chordNames(s))chords.add(utf(name));chordsLabel.setText(tr("HARMONY")+"   "+chords.joinIntoString("    /    "),juce::dontSendNotification);
    undoButton.setEnabled(!undoStack.empty());redoButton.setEnabled(!redoStack.empty());refreshing=false;refreshTrack();if(page==Page::instrument)instrumentPanel.refreshFromProject();updateSizes();translateControls(*this);
}
void MainComponent::refreshTrack(){
    refreshing=true;const auto& t=project.tracks[(size_t)selectedTrack];gainSlider.setValue(t.gain*100,juce::dontSendNotification);panSlider.setValue(t.pan*100,juce::dontSendNotification);muteButton.setToggleState(t.mute,juce::dontSendNotification);soloButton.setToggleState(t.solo,juce::dontSendNotification);lockButton.setToggleState(t.locked,juce::dontSendNotification);lockClipButton.setToggleState(clip().locked,juce::dontSendNotification);
    soundBox.clear(juce::dontSendNotification);int count=selectedTrack==2?1:selectedTrack<2?7:4;for(int i=0;i<count;++i)soundBox.addItem(selectedTrack==2?"String ensemble":selectedTrack<2?guideSoundName(i):selectedTrack==3?GuideGuitar::toneName(i):GuideDrums::kitName(i),i+1);soundBox.setSelectedId(std::min(t.sound+1,count),juce::dontSendNotification);soundBox.setEnabled(t.instrument=="internal"&&selectedTrack!=2);
    inspectorLabel.setText(utf(t.name).toUpperCase()+juce::String::fromUTF8("   ·   ")+instrumentName(t),juce::dontSendNotification);editorLabel.setText(tr(selectedTrack>=4?"DRUM EDITOR":selectedTrack==3?"GUITAR EDITOR":"PIANO ROLL")+"   /   "+utf(t.name)+"   /   "+utf(project.sections[(size_t)selectedSection].name)+"   /   "+juce::String((int)clip().notes.size())+" notes",juce::dontSendNotification);pluginEditorButton.setEnabled(!loadingPlugin&&audio.plugin(selectedTrack)!=nullptr);sampleButton.setEnabled(!loadingPlugin);refreshing=false;
}
void MainComponent::updateSizes(){arrangement.setSize(headerWidth+(int)(project.length()/4*arrangement.pixelsPerBar)+20,32+trackCount*trackHeight);piano.setSize(piano.leftMargin()+(int)(clipLength()*piano.pixelsPerBeat)+20,std::max(selectedTrack==3&&!piano.guitarNotes?640:320,pianoPort.getHeight()-18));}
void MainComponent::undo(bool redo){if(loadingPlugin)return;auto& source=redo?redoStack:undoStack;auto& dest=redo?undoStack:redoStack;if(source.empty())return;auto before=project;dest.push_back(project);project=std::move(source.back());source.pop_back();project.normalise();selectedSection=std::min(selectedSection,(int)project.sections.size()-1);
    bool instrumentsChanged=false;for(size_t i=0;i<project.tracks.size();++i)if(project.tracks[i].instrument!=before.tracks[i].instrument||project.tracks[i].instrumentPath!=before.tracks[i].instrumentPath||project.tracks[i].instrumentId!=before.tracks[i].instrumentId||project.tracks[i].sampleRoot!=before.tracks[i].sampleRoot)instrumentsChanged=true;
    if(instrumentsChanged)restoreInstruments();piano.clearSelection();changed();if(page==Page::instrument)instrumentPanel.rebuild();}
void MainComponent::regenerate(Scope scope){if(loadingPlugin)return;checkpoint();audio.stop();auto seed=page==Page::instrument?0u:(std::uint32_t)juce::Random::getSystemRandom().nextInt();if(seed==0&&page!=Page::instrument)seed=1;generate(project,scope,selectedTrack,selectedSection,seed);piano.clearSelection();changed();message(juce::String::fromUTF8("Nuova generazione pronta · blocchi rispettati · UNDO per tornare indietro."));}
void MainComponent::modifySelected(std::function<void(Clip&)> action){checkpoint();if(piano.selected.empty())action(clip());else{Clip part;for(int i:piano.selected)if(i>=0&&i<(int)clip().notes.size())part.notes.push_back(clip().notes[(size_t)i]);action(part);size_t k=0;for(int i:piano.selected)if(i>=0&&i<(int)clip().notes.size())clip().notes[(size_t)i]=part.notes[k++];}tidyNotes(clip(),clipLength());piano.clearSelection();changed();}
void MainComponent::paint(juce::Graphics& g){
    g.fillAll(Palette::background);drawFamilyLogo(g,{20,18,238,46});
    label(g,tr("by")+" Fabio 'Pitch' Serra",{78,74,200,14},11,Palette::dim);
    auto panel=[&](juce::Rectangle<int> r){g.setColour(Palette::panel);g.fillRoundedRectangle(r.toFloat(),8);g.setColour(Palette::panelEdge);g.drawRoundedRectangle(r.toFloat().reduced(.5f),8,1);};
    panel({624,18,390,68});panel({1024,18,getWidth()-1044,68});
    g.setColour(Palette::accent);g.fillRoundedRectangle(636,27,3,12,1);
    label(g,"TRANSPORT & TEMPO",{645,23,146,20},11.5f);
    const double seconds=project.length()*60/project.settings.bpm;
    label(g,utf(project.name)+(dirty?" *":"")+" / "+juce::String((int)seconds/60)+":"+juce::String((int)seconds%60).paddedLeft('0',2),{1036,23,getWidth()-1060,20},13);
    for(int y:{110,192,getHeight()-31}){g.setColour(Palette::panelEdge);g.fillRect(20,y,getWidth()-40,1);}
    if(page!=Page::arrangement)label(g,tr(page==Page::instrument?"INSTRUMENT":"MIDI EDITOR")+" / "+utf(project.tracks[(size_t)selectedTrack].name).toUpperCase(),{196,122,400,28},18,colour(selectedTrack));
}
void MainComponent::resized(){
    const int w=getWidth();auto place=[](juce::Component& c,int x,int y,int width,int height=28){c.setBounds(x,y,width,height);};
    const bool song=page==Page::arrangement,engine=page==Page::instrument,edit=page==Page::editor;
    arrangementPort.setVisible(song);instrumentPort.setVisible(engine);pianoPort.setVisible(edit);
    for(juce::Component* c:std::initializer_list<juce::Component*>{&genreBox,&songButton,&trackButton,&sectionButton,&clipButton,&densitySlider,&swingSlider,&humanizeSlider,&progressionBox,&harmonyButton,&extensionBox,&voicingBox,&bassBox,&stringsBox,&addSectionButton,&zoomSlider,&durationLabel,&arrangeButton})c->setVisible(song);
    bassBox.setVisible(false);stringsBox.setVisible(false); // instrument-owned styles live in their panels
    for(juce::Component* c:std::initializer_list<juce::Component*>{&soundButton,&soundBox,&sampleButton,&loadPluginButton,&pluginEditorButton,&gainSlider,&panSlider,&muteButton,&soloButton,&lockButton,&lockClipButton,&inspectorLabel})c->setVisible(engine);
    for(juce::Component* c:std::initializer_list<juce::Component*>{&editorLabel,&sectionLoopButton,&importButton,&copyButton,&pasteButton,&quantiseButton,&transposeDown,&transposeUp,&deleteNotes,&gridBox,&velocitySlider})c->setVisible(edit);
    for(auto* button:{&drawTool,&selectTool,&eraseTool})button->setVisible(edit);
    for(auto* button:{&brushAccent,&brushHit,&brushGhost,&brushFlam})button->setVisible(edit&&piano.laneView());fitNotesButton.setVisible(edit&&!piano.laneView());
    if(edit&&selectedTrack>=4){transposeDown.setVisible(false);transposeUp.setVisible(false);}
    backButton.setVisible(!song);instrumentButton.setVisible(edit);editButton.setVisible(engine);sectionSelector.setVisible(!song);leadButton.setVisible(edit&&selectedTrack==3);guitarNotesButton.setVisible(edit&&selectedTrack==3);panelSections.setVisible(engine);
    // Same two utility rows to the right of the logo as the family editors.
    // File controls replace plugin presets; transport and global harmony stay shared.
    int x=282;
    for(const auto& item:std::initializer_list<std::pair<juce::Component*,int>>{{&newButton,46},{&openButton,54},{&saveButton,54},{&saveAsButton,70},{&undoButton,30},{&redoButton,30}}){place(*item.first,x,22,item.second,26);x+=item.second+4;}
    undoButton.setConnectedEdges(juce::Button::ConnectedOnRight);redoButton.setConnectedEdges(juce::Button::ConnectedOnLeft);
    x=282;for(const auto& item:std::initializer_list<std::pair<juce::Component*,int>>{{&exportButton,74},{&settingsButton,70},{&aboutButton,100},{&helpButton,70}}){place(*item.first,x,60,item.second,26);x+=item.second+4;}
    place(rewindButton,636,48,34,30);place(playButton,670,48,40,30);place(stopButton,710,48,34,30);place(tempoSlider,756,48,154,30);place(loopButton,912,48,94,30);place(positionLabel,796,23,210,18);
    place(keyBox,1036,48,64,30);place(scaleBox,1108,48,154,30);place(masterSlider,1270,48,w-1310,30);place(chordEditButton,w-150,88,132,22);
    if(song){
        x=16;place(genreBox,x,118,178);x+=188;place(durationLabel,x,118,172);x+=182;place(arrangeButton,x,118,170);x+=180;place(songButton,x,118,154);x+=164;place(trackButton,x,118,116);x+=124;place(sectionButton,x,118,124);x+=132;place(clipButton,x,118,114);x+=124;place(addSectionButton,x,118,114);
        x=16;place(progressionBox,x,159,210);x+=218;place(harmonyButton,x,159,118);x+=126;place(extensionBox,x,159,116);x+=124;place(voicingBox,x,159,116);x+=124;place(densitySlider,x,159,150);x+=158;place(swingSlider,x,159,150);x+=158;place(humanizeSlider,x,159,150);x+=158;place(zoomSlider,x,159,150);
        arrangementPort.setBounds(12,202,w-24,getHeight()-284);place(chordsLabel,18,getHeight()-73,w-36,28);
    }else{
        place(backButton,16,122,160);place(sectionSelector,600,122,230);place(editButton,w-170,122,150);place(instrumentButton,w-200,122,180);place(leadButton,850,122,260);place(panelSections,850,122,260);
        if(engine){
            inspectorLabel.setVisible(false);soundButton.setVisible(false);sampleButton.setVisible(false);soundBox.setVisible(false);
            x=18;for(const auto& item:std::initializer_list<std::pair<juce::Component*,int>>{{&loadPluginButton,130},{&pluginEditorButton,96},{&gainSlider,138},{&panSlider,132},{&muteButton,60},{&soloButton,60},{&lockButton,138},{&lockClipButton,138},{&trackButton,114},{&clipButton,114}}){place(*item.first,x,162,item.second);x+=item.second+6;}
            trackButton.setVisible(true);trackButton.setButtonText("APPLY TRACK");clipButton.setVisible(true);clipButton.setButtonText("APPLY CLIP");
            // Keep the plugins' 1180px design width. Extra DAW width must not
            // stretch only the Groove card and alter the family's proportions.
            const int panelWidth=std::min(1180,w-40);
            instrumentPort.setBounds((w-panelWidth-16)/2,204,panelWidth+16,getHeight()-243);
            instrumentPanel.setSize(panelWidth,instrumentPanel.getHeight());
        }else{
            place(editorLabel,20,159,640,28);place(guitarNotesButton,w-340,159,180);place(sectionLoopButton,w-150,159,132);
            x=18;for(auto* b:{&importButton,&copyButton,&pasteButton,&quantiseButton,&transposeDown,&transposeUp,&deleteNotes}){int width=b==&importButton?115:b==&quantiseButton?103:64;place(*b,x,203,width,28);x+=width+6;}place(gridBox,x+3,203,88);place(velocitySlider,x+101,203,154);place(zoomSlider,x+265,203,160);zoomSlider.setVisible(true);place(drawTool,1034,203,52);place(selectTool,1090,203,52);place(eraseTool,1146,203,52);place(brushAccent,700,159,80);place(brushHit,780,159,70);place(brushGhost,850,159,80);place(brushFlam,930,159,74);place(fitNotesButton,w-110,203,88);pianoPort.setBounds(12,246,w-24,getHeight()-285);
        }
    }
    chordsLabel.setVisible(song);
    if(song){trackButton.setButtonText("NEW TRACK");clipButton.setButtonText("NEW CLIP");}
    place(statusLabel,18,getHeight()-29,w-36,25);options.setBounds(getLocalBounds());if(options.isVisible())options.toFront(false);updateOverlay.setBounds(getLocalBounds());if(updateOverlay.isVisible())updateOverlay.toFront(false);updateSizes();translateControls(*this);
}
bool MainComponent::writeLanguageEvidence(const juce::File& folder){
    const auto saved=Localisation::current();const auto originalJson=juce::JSON::toString(projectJson(project),false,17);bool ok=true;static const char* codes[]{"en","it","es","fr","de","pt"};static const char* expected[]{"NEW TRACK","NUOVA TRACCIA","NUEVA PISTA","NOUVELLE PISTE","NEUE SPUR","NOVA FAIXA"};
    for(int i=0;i<6;++i){Localisation::setCurrent((Localisation::Language)i);showArrangement();applyLanguage();refreshControls();ok=tr("NEW TRACK")==juce::String::fromUTF8(expected[i])&&ok;
        ok=genreBox.getSelectedItemIndex()==project.settings.genre&&keyBox.getSelectedItemIndex()==project.settings.key&&scaleBox.getSelectedItemIndex()==project.settings.scale&&extensionBox.getSelectedItemIndex()==project.settings.extension&&voicingBox.getSelectedItemIndex()==project.settings.voicing&&ok;
        ok=juce::JSON::toString(projectJson(project),false,17)==originalJson&&ok;auto picture=createComponentSnapshot(getLocalBounds());if(auto output=folder.getChildFile(juce::String("language-")+codes[i]+".png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(picture,*output)&&ok;}else ok=false;}
    Localisation::setCurrent(saved);applyLanguage();select(0,1);showInstrument();
    for(const auto& pair:std::initializer_list<std::pair<const char*,const char*>>{{"PERFORMANCE","piano-performance"},{"RHYTHM / VELOCITY","piano-rhythm"},{"Musical tools","piano-tools"},{"GUIDE SOUND","piano-sound"}}){instrumentPort.setViewPosition(0,instrumentPanel.sectionPosition(pair.first));auto image=createComponentSnapshot(getLocalBounds());if(auto output=folder.getChildFile(juce::String(pair.second)+".png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(image,*output)&&ok;}}
    Localisation::setCurrent(Localisation::Language::italian);applyLanguage();showInstrument();
    for(const auto& pair:std::initializer_list<std::pair<const char*,const char*>>{{"PERFORMANCE","piano-extended-it"},{"RHYTHM / VELOCITY","piano-rhythm-it"}}){instrumentPort.setViewPosition(0,instrumentPanel.sectionPosition(pair.first));auto picture=createComponentSnapshot(getLocalBounds());if(auto output=folder.getChildFile(juce::String(pair.second)+".png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(picture,*output)&&ok;}}
    Localisation::setCurrent(saved);applyLanguage();showInstrument();
    ok=verifyPianoTools(*this,folder)&&ok;
    int tips=0;std::function<void(juce::Component&)> count=[&](auto& c){if(auto* tip=dynamic_cast<juce::SettableTooltipClient*>(&c))if(tip->getTooltip().isNotEmpty())++tips;for(auto* child:c.getChildren())count(*child);};count(*this);ok=tips>=50&&ok;
    folder.getChildFile("localisation.txt").replaceWithText(ok?"PASS six interface languages, live translation, extended piano sections and control tooltips":"FAIL localisation or extended controls");showArrangement();return ok;
}
bool MainComponent::writeUiEvidence(const juce::File& folder){
    bool ok=writeLanguageEvidence(folder)&&writeHelpEvidence(folder);auto capture=[&](const juce::String& name){auto screenshot=createComponentSnapshot(getLocalBounds());if(auto out=folder.getChildFile(name+".png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(screenshot,*out)&&ok;}else ok=false;};
    select(0,1);showInstrument();capture("piano-family-extended");const auto take=clip().seed;regenerate(Scope::clip);ok=ok&&clip().seed==take;
    showArrangement();ok=ok&&arrangementPort.isVisible()&&!pianoPort.isVisible()&&headerWidth+project.length()/4*arrangement.pixelsPerBar<arrangementPort.getWidth();capture("interface");
    juce::String layoutReport,editingReport;
    for(int track:{4,1,0,2,3}){select(track,1);showInstrument();auto problems=instrumentPanel.layoutProblems();layoutReport+=juce::String(track)+": "+(problems.isEmpty()?"PASS":problems)+"\n";ok=problems.isEmpty()&&ok;ok=ok&&instrumentPort.isVisible()&&!pianoPort.isVisible()&&sectionSelector.getSelectedId()==selectedSection+1;capture("instrument-"+juce::String(track));if(track==3){instrumentPort.setViewPosition(0,instrumentPanel.getHeight());capture("guitar-mixer");instrumentPort.setViewPosition(0,0);}showEditor();ok=ok&&pianoPort.getHeight()>400&&!arrangementPort.isVisible();if(track>=4)ok=ok&&piano.laneView();if(track==1||track==2)ok=ok&&!piano.laneView();capture("editor-"+juce::String(track));const bool edits=piano.verifyEditing();ok=edits&&ok;editingReport+=juce::String(track)+": "+(edits?"PASS add, move, velocity, resize where pitched, delete":"FAIL editing gestures")+"\n";}
    folder.getChildFile("panel-layout.txt").replaceWithText(layoutReport);folder.getChildFile("midi-editing.txt").replaceWithText(editingReport);
    select(3,1);piano.guitarNotes=true;showEditor();ok=ok&&!piano.laneView();capture("guitar-notes");ok=piano.verifyEditing()&&ok;piano.guitarNotes=false;showArrangement();select(4,0);
    folder.getChildFile("ui.txt").replaceWithText(ok?"PASS separate arrangement, instrument panels, full MIDI editors, drum isolation, guitar lanes/notes, apply preserves take":"FAIL interface routing");return ok;
}
void MainComponent::timerCallback(){
    const double beat=audio.position();positionLabel.setText(juce::String((int)beat/4+1).paddedLeft('0',3)+" : "+juce::String((int)beat%4+1)+"   /   "+juce::String((int)project.length()/4),juce::dontSendNotification);playButton.setIcon(audio.isPlaying()?IconButton::Icon::pause:IconButton::Icon::play);arrangement.repaint();piano.repaint();if(page==Page::instrument)instrumentPanel.repaint();
    if(++ticks%1200==0&&dirty&&!loadingPlugin){audio.capturePluginStates(project);auto path=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("FS Pitch/Fase Groove Arranger/autosave.fga");path.getParentDirectory().createDirectory();auto result=saveProject(project,path);if(result.failed())message(result.getErrorMessage(),true);}
}
void MainComponent::message(const juce::String& text,bool error){statusLabel.setColour(juce::Label::textColourId,error?Palette::toms:Palette::dim);statusLabel.getProperties().set("labelSource",text);statusLabel.getProperties().set("labelLast",tr(text));statusLabel.setText(tr(text),juce::dontSendNotification);}
bool MainComponent::keyPressed(const juce::KeyPress& k){if(k==juce::KeyPress::escapeKey){if(updateOverlay.isVisible()){updateOverlay.setVisible(false);return true;}if(options.isVisible()){options.setVisible(false);return true;}showArrangement();return true;}if(k==juce::KeyPress::spaceKey){audio.play(!audio.isPlaying());return true;}if(k.getModifiers().isCommandDown()){int code=k.getKeyCode();if(code=='S'){save(k.getModifiers().isShiftDown());return true;}if(code=='Z'){undo(k.getModifiers().isShiftDown());return true;}if(code=='Y'){undo(true);return true;}}return false;}
bool MainComponent::requestClose(){if(!dirty)return true;return juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon,"Modifiche non salvate","Il progetto contiene modifiche. Scartarle?\nUsa SAVE per conservarle prima di chiudere.","Scarta","Annulla");}
void MainComponent::chooseFile(const juce::String& title,const juce::String& filter,int flags,std::function<void(juce::File)> callback){chooser=std::make_unique<juce::FileChooser>(tr(title),projectFile.existsAsFile()?projectFile.getParentDirectory():juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),filter);juce::Component::SafePointer<MainComponent> safe(this);chooser->launchAsync(flags,[safe,callback](const juce::FileChooser& fc){auto f=fc.getResult();if(safe!=nullptr&&f!=juce::File())callback(f);});}
void MainComponent::save(bool as){if(loadingPlugin){message("Attendi il caricamento dello strumento.");return;}audio.capturePluginStates(project);if(as||projectFile==juce::File()){chooseFile("Salva progetto","*.fga",juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[this](juce::File file){projectFile=file.withFileExtension("fga");project.name=projectFile.getFileNameWithoutExtension().toStdString();auto r=saveProject(project,projectFile);if(r.wasOk()){dirty=false;message("Progetto salvato: "+projectFile.getFullPathName());repaint();}else message(r.getErrorMessage(),true);});}else{auto r=saveProject(project,projectFile);if(r.wasOk()){dirty=false;message("Progetto salvato.");repaint();}else message(r.getErrorMessage(),true);}}
void MainComponent::open(const juce::File& file){if(loadingPlugin){message("Attendi il caricamento dei plugin.");return;}if(!requestClose())return;Project next;auto r=loadProject(file,next);if(r.failed()){message(r.getErrorMessage(),true);return;}audio.stop();checkpoint();project=std::move(next);projectFile=file;selectedTrack=selectedSection=0;dirty=false;piano.clearSelection();audio.publish(project);refreshControls();restoreInstruments();select(4,0);showArrangement();message("Progetto aperto. I file audio e i plugin restano esterni al progetto.");repaint();}
void MainComponent::restoreInstruments(){pluginWindow.reset();for(int i=0;i<trackCount;++i){audio.useInternal(i);auto t=project.tracks[(size_t)i];if(t.instrument=="sample"){auto r=audio.loadSample(i,juce::File(utf(t.instrumentPath)),t.sampleRoot);if(r.failed()){audio.markMissing(i);message("Campione mancante su "+utf(t.name)+": ricollegalo con LOAD SAMPLE.",true);}}else if(t.instrument=="plugin")loadPluginFile(i,juce::File::isAbsolutePath(utf(t.instrumentPath))?juce::File(utf(t.instrumentPath)):juce::File(),utf(t.instrumentId),utf(t.instrumentState));}audio.publish(project);}
void MainComponent::loadSampleFile(int track,const juce::File& file){pluginWindow.reset();checkpoint();auto& t=project.tracks[(size_t)track];int root=track>=4?fgd::defaultNote(track-4):60;auto r=audio.loadSample(track,file,root);if(r.failed()){message(r.getErrorMessage(),true);return;}t.instrument="sample";t.instrumentPath=file.getFullPathName().toStdString();t.sampleRoot=root;t.instrumentId.clear();t.instrumentState.clear();changed(false);message(juce::String::fromUTF8("Campione caricato · nota radice ")+juce::String(root)+" (tasto destro sulla traccia per cambiarla).");}
void MainComponent::loadPluginFile(int track,const juce::File& file,const juce::String& id,const juce::String& state){
    if(id.isNotEmpty())audio.markMissing(track);
    if(auto xml=juce::XmlDocument::parse(id)){
        juce::PluginDescription description;
        if(description.loadFromXml(*xml)){
            if(description.pluginFormatName=="VST3"&&!juce::File(description.fileOrIdentifier).exists()){message("Plugin mancante: usa LOAD VST3 / AU per ricollegarlo.",true);return;}
            loadPluginDescription(track,description,state,true);return;
        }
    }
    if(!file.exists()){message("Plugin mancante su "+utf(project.tracks[(size_t)track].name)+": usa LOAD VST3 / AU per ricollegarlo.",true);return;}
    juce::OwnedArray<juce::PluginDescription> found;
    for(int i=0;i<audio.formats.getNumFormats();++i)audio.formats.getFormat(i)->findAllTypesForFile(found,file.getFullPathName());
    juce::PluginDescription* selected=nullptr;
    for(auto* d:found)if((id.isEmpty()&&selected==nullptr)||d->createIdentifierString()==id)selected=d;
    if(!selected){message("Nessuno strumento compatibile trovato. Scegli il bundle VST3.",true);return;}
    loadPluginDescription(track,*selected,state,id.isNotEmpty());
}
void MainComponent::loadPluginDescription(int track,const juce::PluginDescription& description,const juce::String& state,bool restoring){
    if(!description.isInstrument){message("Scegli uno strumento: questo slot riceve MIDI e produce audio.",true);return;}
    pluginWindow.reset();if(!restoring)checkpoint();++loadingPlugin;loadPluginButton.setEnabled(false);
    juce::Component::SafePointer<MainComponent> safe(this);
    audio.loadPlugin(track,description,[safe,track,description,state,restoring](juce::Result r){
        if(safe==nullptr)return;
        --safe->loadingPlugin;safe->loadPluginButton.setEnabled(safe->loadingPlugin==0);
        if(r.failed()){safe->message(r.getErrorMessage(),true);return;}
        auto& t=safe->project.tracks[(size_t)track];t.instrument="plugin";
        t.instrumentPath=description.fileOrIdentifier.toStdString();
        if(auto xml=description.createXml())t.instrumentId=xml->toString().toStdString();
        t.instrumentState=state.toStdString();
        if(state.isNotEmpty())safe->audio.restorePluginState(track,state);
        if(restoring){safe->audio.updateMix(safe->project);safe->refreshTrack();}else safe->changed(false);
        safe->message("Plugin pronto: "+description.name);
    });
}
void MainComponent::showPluginMenu(){
    juce::PopupMenu menu;menu.addItem(1,"Strumento VST3 da file");
   #if JUCE_MAC
    menu.addItem(2,"Audio Units installate");
   #endif
    juce::Component::SafePointer<MainComponent> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&loadPluginButton),[safe](int id){
        if(safe==nullptr)return;
        if(id==1){int track=safe->selectedTrack;safe->chooseFile("Scegli un plugin VST3","*.vst3",juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::canSelectDirectories,[safe,track](juce::File file){if(safe!=nullptr)safe->loadPluginFile(track,file);});}
        if(id==2){
            auto list=std::make_shared<juce::OwnedArray<juce::PluginDescription>>();
            for(int i=0;i<safe->audio.formats.getNumFormats();++i){auto* format=safe->audio.formats.getFormat(i);if(format->getName().containsIgnoreCase("AudioUnit")){
                auto ids=format->searchPathsForPlugins(juce::FileSearchPath(),false,true);
                for(auto key:ids)format->findAllTypesForFile(*list,key);
            }}
            juce::PopupMenu units;for(int i=0;i<list->size();++i)if((*list)[i]->isInstrument)units.addItem(i+1,(*list)[i]->name+juce::String::fromUTF8(" · ")+(*list)[i]->manufacturerName);
            if(units.getNumItems()==0){safe->message("Nessuna Audio Unit strumento installata.");return;}
            int track=safe->selectedTrack;units.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&safe->loadPluginButton),[safe,list,track](int chosen){if(safe!=nullptr&&chosen>0&&chosen<=list->size())safe->loadPluginDescription(track,*(*list)[chosen-1]);});
        }
    });
}
void MainComponent::importFile(const juce::File& f){checkpoint();auto r=importMidi(f,clip(),clipLength());if(r.failed())message(r.getErrorMessage(),true);else{piano.clearSelection();changed();message("MIDI importato nel clip selezionato.");}}
bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files){for(auto f:files)if(f.endsWithIgnoreCase(".fga")||f.endsWithIgnoreCase(".mid")||f.endsWithIgnoreCase(".midi")||f.endsWithIgnoreCase(".wav")||f.endsWithIgnoreCase(".aiff")||f.endsWithIgnoreCase(".flac"))return true;return false;}
void MainComponent::filesDropped(const juce::StringArray& files,int,int){if(files.isEmpty())return;juce::File f(files[0]);if(f.hasFileExtension("fga"))open(f);else if(f.hasFileExtension("mid;midi"))importFile(f);else loadSampleFile(selectedTrack,f);}
}
