// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Branding.h"
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
constexpr int trackHeight=40,headerWidth=216,rulerHeight=60;
}
ArrangementView::ArrangementView(MainComponent& m):owner(m){
    setWantsKeyboardFocus(true);trackHeights.fill(trackHeight);
}
void ArrangementView::saveTrackHeights(){
    for(int track=0;track<trackCount;++track){const auto key="trackHeight"+juce::String(track);Localisation::setSetting(key.toRawUTF8(),trackHeights[(size_t)track]);}
}
int ArrangementView::heightEdgeAt(const juce::MouseEvent& e) const {
    const auto view=owner.arrangementPort.getViewArea();
    if(e.y<view.getY()+rulerHeight||e.x<view.getX()||e.x>=view.getX()+headerWidth)return -1;
    for(int row=0;row<trackCount;++row)if(std::abs(e.y-(rowY(row)+rowHeight(row)))<=4)return trackAtRow(row);
    return -1;
}
void ArrangementView::paint(juce::Graphics& g){
    g.fillAll(Palette::field);auto view=owner.arrangementPort.getViewArea();const auto& p=owner.project;
    // Readable grid: alternate track shading, bar lines (stronger every 4 shown) and section edges.
    {for(int row=1;row<trackCount;row+=2){g.setColour(Palette::panel.withAlpha(.32f));g.fillRect(headerWidth,rowY(row),getWidth()-headerWidth,rowHeight(row));}
        const int interval=pixelsPerBar<12?16:pixelsPerBar<30?8:pixelsPerBar<60?4:1;const int first=std::max(0,(int)(view.getX()/pixelsPerBar)/interval*interval);const float bottom=(float)totalHeight();
        for(int bar=first;;bar+=interval){const int x=headerWidth+(int)(bar*pixelsPerBar);if(x>view.getRight()||x>getWidth())break;g.setColour(Palette::panelEdge.withAlpha(bar%(interval*4)==0?.6f:.3f));g.drawVerticalLine(x,(float)rulerHeight,bottom);}
        g.setColour(Palette::panelEdge.brighter(.25f));{double start=0;for(size_t si=1;si<p.sections.size();++si){start+=p.sections[si-1].bars*4.0;g.drawVerticalLine(headerWidth+(int)(start/4*pixelsPerBar),(float)rulerHeight,bottom);}}}
    for(int row=0;row<trackCount;++row){const size_t ti=(size_t)trackAtRow(row);int y=rowY(row);const auto& t=p.tracks[ti];double occupied=0,sectionBegin=0;
        if((int)ti==owner.selectedTrack){g.setColour(colour((int)ti).withAlpha(.045f));g.fillRect(0,y,getWidth(),rowHeight(row));}
        for(size_t si=0;si<t.clips.size();++si){const double begin=t.freeTimeline?t.clips[si].position:sectionBegin,duration=p.clipLength((int)ti,(int)si),end=begin+duration,visible=std::max(begin,occupied);if(si<p.sections.size())sectionBegin+=p.sections[si].bars*4.0;occupied=std::max(occupied,end);if(visible>=end)continue;int x=headerWidth+(int)(visible/4*pixelsPerBar);int w=(int)((end-visible)/4*pixelsPerBar);juce::Rectangle<int> cell(x+1,y+3,std::max(1,w-2),rowHeight(row)-6);if(cell.getRight()<=view.getX()+headerWidth||cell.getX()>=view.getRight())continue;const auto& c=t.clips[si];bool sel=(int)ti==owner.selectedTrack&&(owner.arrangement.selectedClips.count((int)si)>0||(int)si==owner.clipIndex());
            g.setColour(colour((int)ti).withAlpha(c.notes.empty()?.035f:t.mute?.07f:sel?.24f:.12f));g.fillRoundedRectangle(cell.toFloat(),4);
            g.setColour(sel?colour((int)ti):Palette::panelEdge);g.drawRoundedRectangle(cell.toFloat(),4,sel?1.5f:.5f);
            if(w>12){int low=127,high=0;for(auto n:c.notes){low=std::min(low,n.pitch);high=std::max(high,n.pitch);}g.saveState();g.reduceClipRegion(cell.reduced(3));g.setColour(colour((int)ti).withAlpha(.65f));for(auto n:c.notes){float nx=headerWidth+(float)((begin+n.start)/4*pixelsPerBar);float ny=cell.getBottom()-5-(float)((n.pitch-low)/(double)std::max(1,high-low)*std::max(16,rowHeight(row)-18));g.fillRect(nx,ny,std::max(1.f,(float)(n.length/4*pixelsPerBar)),2.f);}g.restoreState();}
            if(c.loopLength>0&&duration>c.loopLength){g.setColour(colour((int)ti).withAlpha(.25f));for(double at=begin+c.loopLength;at<end;at+=c.loopLength)if(at>=visible)g.drawVerticalLine(headerWidth+(int)(at/4*pixelsPerBar),(float)y+7,(float)(y+rowHeight(row)-7));}
            g.setColour(sel?colour((int)ti):Palette::dim.withAlpha(.45f));g.fillRoundedRectangle((float)cell.getRight()-4,(float)y+11,2,18,1);
            if(c.locked)label(g,"L",{cell.getRight()-17,y+4,14,16},10,Palette::accent);
        }g.setColour(Palette::panelEdge.withAlpha(.5f));g.drawHorizontalLine(y+rowHeight(row)-1,0,(float)getWidth());
    }
    for(int row=0;row<trackCount;++row){const int ti=trackAtRow(row);if(!volumeVisible[(size_t)ti])continue;
        const int top=rowY(row)+rowHeight(row);const auto& volume=envelope(ti);
        g.setColour(Palette::panel);g.fillRect(headerWidth,top,getWidth()-headerWidth,80);
        const float zero=volumeY(ti,envelopeMax(ti)*.5);g.setColour(Palette::panelEdge);g.drawHorizontalLine((int)zero,(float)headerWidth,(float)getWidth());
        {double start=0;for(const auto& section:p.sections){g.drawVerticalLine(headerWidth+(int)(start/4*pixelsPerBar),(float)top,(float)top+80);start+=section.bars*4.0;}}
        auto displayed=volume;displayed.enabled=true;juce::Path line;
        const int from=std::max(headerWidth,view.getX()+headerWidth),to=std::min(getWidth(),view.getRight());
        for(int x=from;x<=to;x+=2){const float y=volumeY(ti,displayed.valueAt((x-headerWidth)*4./pixelsPerBar));if(x==from)line.startNewSubPath((float)x,y);else line.lineTo((float)x,y);}
        g.setColour(volume.enabled?Palette::accent2:Palette::dim);g.strokePath(line,juce::PathStrokeType(1.5f));
        for(const auto& point:volume.points){const float x=headerWidth+(float)(point.beat/4*pixelsPerBar),y=volumeY(ti,point.value);if(x<view.getX()+headerWidth||x>view.getRight())continue;g.fillEllipse(x-4,y-4,8,8);}
    }
    // Track headers stay visible while the song scrolls horizontally.
    for(int row=0;row<trackCount;++row){const size_t ti=(size_t)trackAtRow(row);int y=rowY(row);const auto& t=p.tracks[ti];int x=view.getX();g.setColour((int)ti==owner.selectedTrack?Palette::control:Palette::panel);g.fillRect(x,y,headerWidth,rowHeight(row)-1);g.setColour(colour((int)ti));g.fillRect(x+8,y+8,3,20);label(g,juce::String(row+1).paddedLeft('0',2),{x+18,y+2,26,30},10,Palette::dim);g.setColour((int)ti==owner.selectedTrack?Palette::text:Palette::text.withAlpha(.88f));g.setFont(FamilyTheme::font(11.5f,true));g.drawFittedText(Localisation::upper(utf(t.name)),{x+48,y+1,82,22},juce::Justification::centredLeft,1,.8f);{auto source=owner.cachedSourceName((int)ti);if(t.locked)source=tr("LOCK")+juce::String::fromUTF8(" · ")+source;g.setColour(Palette::dim);g.setFont(FamilyTheme::font(8.5f));g.drawFittedText(source,{x+48,y+21,82,12},juce::Justification::centredLeft,1,.75f);}
        // A / M / S read as small switches: filled when on, outlined when off.
        auto pill=[&](const char* text,int px,bool on,juce::Colour c){const juce::Rectangle<float> r((float)px,(float)y+8,17,18);g.setColour(on?c.withAlpha(.24f):Palette::field.withAlpha(.6f));g.fillRoundedRectangle(r,3);g.setColour(on?c:Palette::panelEdge.brighter(.15f));g.drawRoundedRectangle(r.reduced(.5f),3,1);
            g.setColour(on?c:Palette::dim);g.setFont(FamilyTheme::font(10.f,true));g.drawText(text,r,juce::Justification::centred);};
        const bool anyAutomation=t.volume.enabled||t.panAutomation.enabled||std::any_of(t.parameters.begin(),t.parameters.end(),[](const ParameterAutomation& l){return l.curve.enabled;});
        pill("A",x+132,anyAutomation||volumeVisible[ti],anyAutomation?Palette::accent2:Palette::text);pill("M",x+153,t.mute,Palette::toms);pill("S",x+174,t.solo,Palette::accent);
        auto config=juce::Rectangle<float>((float)x+194,(float)y+6,19,24);const bool current=(int)ti==owner.selectedTrack;
        g.setColour(current?Palette::accent.withAlpha(.18f):Palette::field.withAlpha(.6f));g.fillRoundedRectangle(config,4);
        g.setColour(current?Palette::accent.withAlpha(.7f):Palette::panelEdge.brighter(.15f));g.drawRoundedRectangle(config.reduced(.5f),4,1);
        IconButton::drawIcon(g,IconButton::Icon::gear,config.reduced(3,5),current?Palette::accent:Palette::dim);
        if(volumeVisible[ti]){g.setColour(Palette::panel);g.fillRect(x,y+rowHeight(row),headerWidth,80);const auto& shown=envelope((int)ti);const int target=laneTarget[ti];
            g.setColour(shown.moves()?Palette::accent2:Palette::text.withAlpha(.75f));g.setFont(FamilyTheme::font(10.f,true));g.drawFittedText(envelopeTitle((int)ti)+juce::String::fromUTF8(" \xe2\x96\xbe"),{x+16,y+rowHeight(row),190,20},juce::Justification::centredLeft,1,.7f);
            label(g,shown.enabled?"ON":"BYPASS",{x+16,y+rowHeight(row)+24,88,24},11,shown.enabled?Palette::accent2:Palette::dim);label(g,target==lanePan?"L / C / R":target>=lanePlugin?"0 / 50 / 100%":"0 / 100 / 200%",{x+16,y+rowHeight(row)+51,184,20},9,Palette::dim);}
        g.setColour(Palette::panelEdge);g.fillRoundedRectangle((float)x+88,(float)(y+rowHeight(row)-4),40,2,1);
        float meter=std::clamp(owner.audio.level((int)ti),0.f,1.f);g.setColour(Palette::cymbals.withAlpha(.7f));g.fillRect(x+212,y+rowHeight(row)-5-(int)(meter*25),2,(int)(meter*25));
    }
    int y=view.getY();g.setColour(Palette::panel);g.fillRect(view.getX(),y,view.getWidth(),rulerHeight);
    double sectionX=0;for(size_t si=0;si<p.sections.size();++si){int x=headerWidth+(int)(sectionX/4*pixelsPerBar);sectionX+=p.sections[si].bars*4.0;int w=(int)(p.sections[si].bars*pixelsPerBar);g.setColour((int)si==owner.selectedSection?Palette::accent.withAlpha(.17f):Palette::control);g.fillRect(x+2,y+2,w-4,28);label(g,utf(p.sections[si].name)+juce::String::fromUTF8("  ·  ")+juce::String(p.sections[si].bars)+" "+tr("bars"),{x+10,y,w-20,30},11,(int)si==owner.selectedSection?Palette::accent:Palette::text);g.setColour(Palette::dim.withAlpha(.5f));g.fillRoundedRectangle((float)(x+w-5),(float)y+8,2,16,1);}
    if(p.loopEnd>p.loopStart){
        const float from=headerWidth+(float)(p.loopStart/4*pixelsPerBar),to=headerWidth+(float)(p.loopEnd/4*pixelsPerBar);
        g.setColour(p.loop?Palette::accent2.withAlpha(.24f):Palette::panelEdge);g.fillRect(from,(float)y+33,to-from,25.f);
        g.setColour(p.loop?Palette::accent2:Palette::dim);g.fillRect(from,(float)y+33,2.f,25.f);g.fillRect(to-2,(float)y+33,2.f,25.f);
    }
    const int interval=pixelsPerBar<12?16:pixelsPerBar<30?8:pixelsPerBar<60?4:1;
    const int first=std::max(0,(int)(view.getX()/pixelsPerBar)/interval*interval);
    for(int bar=first;bar<=(getWidth()-headerWidth)/pixelsPerBar;bar+=interval){const int x=headerWidth+(int)(bar*pixelsPerBar);if(x>view.getRight())break;g.setColour(Palette::panelEdge);g.drawVerticalLine(x,(float)y+34,(float)y+60);label(g,juce::String(bar+1),{x+4,y+33,44,24},10,Palette::dim);}
    g.setColour(Palette::panel);g.fillRect(view.getX(),y,headerWidth,rulerHeight);label(g,"INSTRUMENT / TRACK",{view.getX()+16,y+12,190,32},10,Palette::dim);
    if(dropTrack>=0){const int row=rowForTrack(dropTrack);const juce::Rectangle<float> target((float)view.getX()+1,(float)rowY(row)+1,(float)view.getWidth()-2,(float)rowHeight(row)-2);
        g.setColour(Palette::accent2.withAlpha(.12f));g.fillRect(target);g.setColour(Palette::accent2);g.drawRect(target,1.5f);}
    const float playX=headerWidth+(float)(owner.timelinePosition()/4*pixelsPerBar);if(playX>view.getX()+headerWidth&&playX<view.getRight()){g.setColour(Palette::accent2);g.fillRect(playX,(float)y+32,1.5f,(float)view.getHeight()-32);
        juce::Path head;head.addTriangle(playX-5,(float)y+32,playX+6.5f,(float)y+32,playX+.75f,(float)y+39);g.fillPath(head);}
    if(clipMoving&&clipTrack>=0&&clipTarget>=0){auto indices=selectedClips;indices.insert(clipSource);double delta=clipDropBeat-p.clipStart(clipTrack,clipSource);for(int index:indices)delta=std::max(delta,-p.clipStart(clipTrack,index));g.setColour(Palette::accent2);for(int index:indices){if(index<0||index>=(int)p.tracks[(size_t)clipTrack].clips.size())continue;const float x=headerWidth+(float)((p.clipStart(clipTrack,index)+delta)/4*pixelsPerBar);g.drawRoundedRectangle({x+2,(float)rowY(rowForTrack(clipTarget))+3,std::max(2.f,(float)(p.clipLength(clipTrack,index)/4*pixelsPerBar)-4),(float)rowHeight(rowForTrack(clipTarget))-6},4,2.f);}}

}
void ArrangementView::mouseDown(const juce::MouseEvent& e){
    if(e.mods.isMiddleButtonDown()){panning=true;panStart=owner.arrangementPort.getViewPosition();panAnchor=e.getEventRelativeTo(&owner).position;grabKeyboardFocus();owner.suspendFollow();return;}
    const int heightEdge=heightEdgeAt(e);
    if(heightEdge>=0&&!e.mods.isPopupMenu()){heightTrack=heightEdge;heightBefore=trackHeights[(size_t)heightTrack];heightAnchorY=e.position.y;grabKeyboardFocus();return;}
    auto v=owner.arrangementPort.getViewArea();const int si=sectionAt(e.position.x);
    if(e.y<v.getY()+rulerHeight){
        if(e.x<v.getX()+headerWidth)return;
        if(e.mods.isPopupMenu()){owner.select(owner.selectedTrack,si);owner.showSectionMenu(si);return;}
        if(e.y<v.getY()+32&&!e.mods.isPopupMenu()){const int edge=edgeAt(-1,e.position.x);if(edge>=0){beginResize(edge,-1,e);return;}}
        if(e.y<v.getY()+32){owner.select(owner.selectedTrack,si);owner.seekTimeline(std::max(0.,(e.position.x-headerWidth)*4./pixelsPerBar));}
        else{const double beat=std::clamp(std::floor((e.x-headerWidth)/pixelsPerBar)*4.,0.,owner.project.length());
            if(e.mods.isShiftDown()){owner.checkpoint();loopDragging=true;loopAnchor=beat;owner.setLoopRange(beat,std::min(owner.project.length(),beat+4));}
            else owner.seekTimeline(std::max(0.,(e.position.x-headerWidth)*4./pixelsPerBar));}
        return;
    }
    const int row=rowAt(e.y);if(row<0)return;const int ti=trackAtRow(row);
    if(e.y>=rowY(row)+rowHeight(row)&&volumeVisible[(size_t)ti]){
        owner.selectedTrack=ti;auto& volume=envelope(ti);
        // The lane's title chooses what it automates; the rest of its header switches it on and off.
        if(e.x<v.getX()+headerWidth&&e.y<rowY(row)+rowHeight(row)+20){showLaneTargetMenu(ti);return;}
        if(e.x<v.getX()+headerWidth){owner.checkpoint();volume.enabled=!volume.enabled;owner.changed(false);return;}
        const int hit=pointAt(ti,e.position);
        if(e.mods.isPopupMenu()){
            juce::PopupMenu menu;menu.addItem(1,tr(volume.enabled?"Bypass automation":"Enable automation"));menu.addItem(2,tr("Delete point"),hit>=0);menu.addItem(3,tr("Reset curve"),hit>=0);menu.addItem(4,tr("Clear automation"),!volume.points.empty());
            juce::Component::SafePointer<ArrangementView> safe(this);menu.showMenuAsync(juce::PopupMenu::Options(),[safe,ti,hit](int result){if(!safe||!result)return;auto& o=safe->owner;o.checkpoint();auto& a=safe->envelope(ti);if(result==1)a.enabled=!a.enabled;else if(result==2&&hit>=0&&hit<(int)a.points.size())a.points.erase(a.points.begin()+hit);else if(result==3&&hit>=0&&hit<(int)a.points.size())a.points[(size_t)hit].curve=0;else if(result==4){a.points.clear();a.enabled=false;}o.changed(false);});return;
        }
        if(hit<0&&!e.mods.isAltDown()&&volume.points.size()>=16384)return;
        owner.checkpoint();dragTrack=ti;dragPoint=hit;bending=e.mods.isAltDown();dragY=e.position.y;
        if(bending){const double beat=std::clamp((e.position.x-headerWidth)*4./pixelsPerBar,0.,owner.project.length());auto it=std::upper_bound(volume.points.begin(),volume.points.end(),beat,[](double b,const VolumePoint& p){return b<p.beat;});dragPoint=(int)(it-volume.points.begin())-1;if(dragPoint<0||dragPoint+1>=(int)volume.points.size()){dragTrack=-1;return;}originalCurve=volume.points[(size_t)dragPoint].curve;}
        else {const double flat=envelopeDefault(ti);if(volume.points.empty())volume.points={{0,flat,0},{owner.project.length(),flat,0}};if(dragPoint<0){const double beat=std::clamp(std::round((e.x-headerWidth)*4./pixelsPerBar),0.,owner.project.length());auto it=std::lower_bound(volume.points.begin(),volume.points.end(),beat,[](const VolumePoint& p,double b){return p.beat<b;});dragPoint=(int)(it-volume.points.begin());if(it==volume.points.end()||std::abs(it->beat-beat)>1./960.)volume.points.insert(it,{beat,volume.valueAt(beat),0});}volume.enabled=true;editPoint(e.position,!e.mods.isShiftDown());}
        owner.changed(false);return;
    }
    if(e.x<v.getX()+headerWidth&&e.x>=v.getX()+132&&e.x<v.getX()+151){volumeVisible[(size_t)ti]=!volumeVisible[(size_t)ti];owner.select(ti,owner.selectedSection);owner.updateSizes();repaint();return;}
    if(e.x<v.getX()+headerWidth){owner.select(ti,owner.selectedSection);if(e.mods.isPopupMenu())owner.showTrackMenu(ti);else if(e.x>=v.getX()+150&&e.x<v.getX()+171){owner.checkpoint();owner.project.tracks[(size_t)ti].mute=!owner.project.tracks[(size_t)ti].mute;owner.changed(false);}else if(e.x>=v.getX()+171&&e.x<v.getX()+194){owner.checkpoint();owner.project.tracks[(size_t)ti].solo=!owner.project.tracks[(size_t)ti].solo;owner.changed(false);}else if(e.x>=v.getX()+194&&e.x<v.getX()+214)owner.showInstrument();return;}
    const int edge=e.mods.isPopupMenu()?-1:edgeAt(ti,e.position.x),hit=clipAt(ti,e.position.x),index=edge>=0?edge:hit;
    if(index<0&&e.mods.isPopupMenu())owner.seekTimeline(std::max(0.,(e.position.x-headerWidth)*4./pixelsPerBar));
    const bool sameTrack=owner.selectedTrack==ti;const auto previous=selectedClips;
    owner.selectClip(ti,index,sameTrack&&previous.count(index)>0);
    if(e.mods.isShiftDown()&&index>=0){selectedClips=sameTrack?previous:std::set<int>{};if(selectedClips.count(index))selectedClips.erase(index);else selectedClips.insert(index);repaint();grabKeyboardFocus();return;}
    if(e.mods.isPopupMenu()){owner.showClipMenu(ti,index);return;}
    if(edge>=0){beginResize(edge,ti,e);return;}
    owner.seekTimeline(std::max(0.,(e.position.x-headerWidth)*4./pixelsPerBar));repaint();
    if(index>=0){clipTrack=ti;clipSource=index;clipTarget=ti;clipAnchorX=e.position.x;clipDropBeat=owner.project.clipStart(ti,index);clipMoving=false;copyingClip=e.mods.isAltDown()||e.mods.isCommandDown();}grabKeyboardFocus();

}
VolumeAutomation& ArrangementView::envelope(int track) const{
    auto& t=owner.project.tracks[(size_t)track];const int target=laneTarget[(size_t)track];
    if(target==lanePan)return t.panAutomation;
    if(target==laneMaster)return owner.project.masterAutomation;
    if(target>=lanePlugin&&target-lanePlugin<(int)t.parameters.size())return t.parameters[(size_t)(target-lanePlugin)].curve;
    return t.volume;
}
double ArrangementView::envelopeMax(int track) const{const int target=laneTarget[(size_t)track];return target==laneVolume||target==laneMaster||(target>=lanePlugin&&target-lanePlugin>=(int)owner.project.tracks[(size_t)track].parameters.size())?2.:1.;}
double ArrangementView::envelopeDefault(int track) const{
    const auto& t=owner.project.tracks[(size_t)track];const int target=laneTarget[(size_t)track];
    if(target==lanePan)return automationFromPan(t.pan);
    if(target>=lanePlugin&&target-lanePlugin<(int)t.parameters.size()){const auto& lane=t.parameters[(size_t)(target-lanePlugin)];
        if(auto* plugin=lane.slot==0?owner.audio.plugin(track):owner.audio.effectPlugin(track))if(lane.index>=0&&lane.index<plugin->getParameters().size())return plugin->getParameters()[lane.index]->getValue();
        return .5;}
    return 1.;
}
juce::String ArrangementView::envelopeTitle(int track) const{
    const auto& t=owner.project.tracks[(size_t)track];const int target=laneTarget[(size_t)track];
    if(target==lanePan)return tr("PAN AUTOMATION");
    if(target==laneMaster)return tr("MASTER VOLUME AUTOMATION");
    if(target>=lanePlugin&&target-lanePlugin<(int)t.parameters.size()){const auto& lane=t.parameters[(size_t)(target-lanePlugin)];return Localisation::upper(tr(lane.slot==0?"Instrument":"Insert"))+": "+juce::String::fromUTF8(lane.name.c_str());}
    return tr("VOLUME AUTOMATION");
}
// The lane's target: volume, pan, the master volume, or a parameter of the track's instrument
// or insert plugin (a new lane per parameter, at most maxParameterLanes per track).
void ArrangementView::showLaneTargetMenu(int track){
    auto& t=owner.project.tracks[(size_t)track];const int current=laneTarget[(size_t)track];
    juce::PopupMenu menu;const auto ink=[](const VolumeAutomation& v){return v.moves()?Palette::accent2:Palette::text;};   // automated lanes read in blue
    menu.addColouredItem(1,tr("Volume"),ink(t.volume),true,current==laneVolume);menu.addColouredItem(2,tr("Pan"),ink(t.panAutomation),true,current==lanePan);menu.addColouredItem(3,tr("Master volume"),ink(owner.project.masterAutomation),true,current==laneMaster);
    if(!t.parameters.empty()){menu.addSeparator();for(size_t k=0;k<t.parameters.size();++k)menu.addColouredItem(100+(int)k,juce::String::fromUTF8(t.parameters[k].name.c_str())+(t.parameters[k].slot==0?"":" ("+tr("Insert")+")"),ink(t.parameters[k].curve),true,current==lanePlugin+(int)k);}
    std::vector<std::pair<int,int>> choices; // slot, parameter index
    auto addPlugin=[&](juce::AudioPluginInstance* plugin,int slot,const char* caption){
        if(plugin==nullptr)return;juce::PopupMenu sub;const auto& params=plugin->getParameters();
        for(int i=0;i<params.size()&&i<512;++i){const auto name=params[i]->getName(64);if(name.isEmpty())continue;choices.push_back({slot,i});sub.addItem(1000+(int)choices.size()-1,name,t.parameters.size()<(size_t)maxParameterLanes);}
        menu.addSubMenu(tr(caption)+": "+plugin->getName(),sub,sub.getNumItems()>0);};
    menu.addSeparator();
    addPlugin(t.instrument=="plugin"?owner.audio.plugin(track):nullptr,0,"Automate an instrument parameter");
    addPlugin(owner.audio.effectPlugin(track),1,"Automate an insert parameter");
    if(t.instrument!="plugin"&&owner.audio.effectPlugin(track)==nullptr)menu.addItem(4,tr("Load a VST3 / AU plugin to automate its parameters"),false);
    if(current>=lanePlugin&&current-lanePlugin<(int)t.parameters.size()){menu.addSeparator();menu.addItem(5,tr("Remove this plugin lane"));}
    // Dice: musical movement for this lane or the whole track (build-up sweeps and risers, pan
    // and level movement; no fades). Undoable; a locked lane, clip or track stays as it is.
    menu.addSeparator();
    const bool masterLane=current==laneMaster;
    menu.addItem(6,tr("Random automation for this lane"),!masterLane&&!t.locked&&!envelope(track).locked);
    menu.addItem(7,tr("Random automation for this track"),!t.locked);
    menu.addItem(8,tr("Lock this lane"),true,envelope(track).locked);
    menu.addItem(9,tr("Write automation when regenerating"),true,owner.project.settings.randomAutomation);
    juce::Component::SafePointer<ArrangementView> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options(),[safe,track,choices](int result){
        if(!safe||!result)return;auto& o=safe->owner;auto& lanes=o.project.tracks[(size_t)track].parameters;auto& target=safe->laneTarget[(size_t)track];
        if(result>=1&&result<=3)target=result-1;
        else if(result==6||result==7){const int lane=result==7?-1:target==laneVolume?0:target==lanePan?1:2+(target-lanePlugin);
            o.checkpoint();randomiseAutomation(o.project,Scope::track,track,0,(std::uint32_t)juce::Random::getSystemRandom().nextInt(),lane);o.changed(false);}
        else if(result==8){o.checkpoint();auto& env=safe->envelope(track);env.locked=!env.locked;o.changed(false);}
        else if(result==9){o.checkpoint();o.project.settings.randomAutomation=!o.project.settings.randomAutomation;o.changed(false);}
        else if(result>=100&&result<100+(int)lanes.size())target=lanePlugin+result-100;
        else if(result==5&&target>=lanePlugin&&target-lanePlugin<(int)lanes.size()){o.checkpoint();lanes.erase(lanes.begin()+(target-lanePlugin));target=laneVolume;o.changed(false);}
        else if(result>=1000&&result-1000<(int)choices.size()&&lanes.size()<(size_t)maxParameterLanes){
            const auto [slot,index]=choices[(size_t)(result-1000)];auto* plugin=slot==0?o.audio.plugin(track):o.audio.effectPlugin(track);
            ParameterAutomation lane;lane.slot=slot;lane.index=index;if(plugin!=nullptr&&index<plugin->getParameters().size())lane.name=plugin->getParameters()[index]->getName(64).toStdString();
            o.checkpoint();lanes.push_back(std::move(lane));target=lanePlugin+(int)lanes.size()-1;o.changed(false);}
        safe->volumeVisible[(size_t)track]=true;o.updateSizes();safe->repaint();});
}
int ArrangementView::rowY(int row) const {int y=rulerHeight;for(int r=0;r<row;++r)y+=rowHeight(r)+(volumeVisible[(size_t)trackAtRow(r)]?80:0);return y;}
int ArrangementView::rowAt(int y) const {if(y<rulerHeight||y>=totalHeight())return -1;for(int r=0;r<trackCount;++r)if(y<rowY(r+1))return r;return -1;}
float ArrangementView::volumeY(int track,double value) const {return (float)(rowY(rowForTrack(track))+rowHeight(rowForTrack(track))+72-value*64./envelopeMax(track));}
int ArrangementView::pointAt(int track,juce::Point<float> pos) const {const auto& points=envelope(track).points;for(size_t i=0;i<points.size();++i)if(pos.getDistanceFrom({headerWidth+(float)(points[i].beat/4*pixelsPerBar),volumeY(track,points[i].value)})<8)return (int)i;return -1;}
void ArrangementView::editPoint(juce::Point<float> pos,bool snap){if(dragTrack<0||dragPoint<0)return;auto& points=envelope(dragTrack).points;auto& point=points[(size_t)dragPoint];double beat=(pos.x-headerWidth)*4./pixelsPerBar;if(snap)beat=std::round(beat);const double low=dragPoint>0?points[(size_t)dragPoint-1].beat+1./960.:0,high=dragPoint+1<(int)points.size()?points[(size_t)dragPoint+1].beat-1./960.:owner.project.length();point.beat=std::clamp(beat,low,high);const double ceiling=envelopeMax(dragTrack);point.value=std::clamp((volumeY(dragTrack,0)-pos.y)*ceiling/64.,0.,ceiling);}
int ArrangementView::sectionAt(float x) const {int result=0;const double beat=(x-headerWidth)*4./pixelsPerBar;for(size_t i=0;i<owner.project.sections.size();++i)if(beat>=owner.project.sectionStart((int)i))result=(int)i;return result;}
double ArrangementView::snapBeat(double beat,bool bypass)const{return std::max(0.,!bypass&&owner.timelineSnap>0?std::round(beat/owner.timelineSnap)*owner.timelineSnap:std::round(beat*960)/960.);}
int ArrangementView::clipAt(int track,float x)const{return owner.project.clipAtBeat(track,(x-headerWidth)*4./pixelsPerBar);}
int ArrangementView::edgeAt(int track,float x)const{
    const auto& p=owner.project;const float left=(float)(owner.arrangementPort.getViewPositionX()+headerWidth);if(x<left)return -1;double occupied=0;
    const size_t count=track<0?p.sections.size():p.tracks[(size_t)track].clips.size();
    for(size_t i=0;i<count;++i){const double start=track<0?p.sectionStart((int)i):p.clipStart(track,(int)i),end=track<0?p.sectionStart((int)i+1):start+p.clipLength(track,(int)i),from=std::max(start,occupied);occupied=std::max(occupied,end);if(from>=end)continue;const float edge=headerWidth+(float)(end/4*pixelsPerBar);if(edge>=left&&x<=edge&&edge-x<=5)return (int)i;}return -1;
}
void ArrangementView::beginResize(int section,int track,const juce::MouseEvent& e){
    if(track<0)owner.select(owner.selectedTrack,section);else owner.selectClip(track,section);resizeSectionIndex=section;resizeTrack=track;resizeAnchorX=e.position.x;
    resizeOriginalLength=resizeLastLength=track<0?owner.project.sections[(size_t)section].bars*4.:owner.project.clipLength(track,section);resizeBefore=std::make_unique<Project>(owner.project);
    clipTrack=clipSource=clipTarget=-1;grabKeyboardFocus();
}
void ArrangementView::cancelResize(){if(resizeBefore){owner.project=*resizeBefore;owner.refreshControls();repaint();}resizeBefore.reset();resizeSectionIndex=resizeTrack=-1;}
bool ArrangementView::keyPressed(const juce::KeyPress& key){if(heightTrack>=0){if(key==juce::KeyPress::escapeKey){trackHeights[(size_t)heightTrack]=heightBefore;heightTrack=-1;owner.updateSizes();repaint();}return true;}if(resizeBefore){if(key==juce::KeyPress::escapeKey)cancelResize();return true;}if(clipTrack>=0||panning){if(key==juce::KeyPress::escapeKey){clipTrack=clipSource=clipTarget=-1;clipMoving=panning=false;repaint();}return true;}return owner.keyPressed(key);}
void ArrangementView::mouseDoubleClick(const juce::MouseEvent& e){const int heightEdge=heightEdgeAt(e);if(heightEdge>=0){heightTrack=-1;trackHeights[(size_t)heightEdge]=trackHeight;saveTrackHeights();owner.updateSizes();repaint();return;}const auto v=owner.arrangementPort.getViewArea();
    // Double-click inside the loop strip removes the loop region.
    if(e.y>=v.getY()+32&&e.y<v.getY()+rulerHeight&&e.x>=v.getX()+headerWidth&&!e.mods.isShiftDown()){const double beat=(e.position.x-headerWidth)*4./pixelsPerBar;const auto& p=owner.project;if(p.hasLoopRegion()&&beat>=p.loopStart&&beat<=p.loopEnd){loopDragging=false;owner.clearLoop();}return;}if(e.y>=v.getY()+rulerHeight&&e.x>=v.getX()+headerWidth&&rowAt(e.y)>=0){clipTrack=clipSource=clipTarget=-1;resizeBefore.reset();resizeSectionIndex=resizeTrack=-1;owner.showEditor();}}
void ArrangementView::mouseDrag(const juce::MouseEvent& e){
    if(panning){const auto offset=e.getEventRelativeTo(&owner).position-panAnchor;owner.arrangementPort.setViewPosition(panStart.x-juce::roundToInt(offset.x),panStart.y-juce::roundToInt(offset.y));return;}
    if(heightTrack>=0){trackHeights[(size_t)heightTrack]=std::clamp(heightBefore+juce::roundToInt(e.position.y-heightAnchorY),40,240);owner.updateSizes();repaint();return;}
    if(resizeBefore){const double grid=resizeTrack<0?4.:e.mods.isShiftDown()?.25:1.;const double duration=std::clamp(std::round((resizeOriginalLength+(e.position.x-resizeAnchorX)*4./pixelsPerBar)/grid)*grid,resizeTrack<0?4.:.25,512.);
        if(duration!=resizeLastLength){auto previous=std::move(owner.project);owner.project=*resizeBefore;
            const bool ok=resizeTrack<0?(owner.project.resizeGuide(resizeSectionIndex,(int)(duration/4)),true):owner.project.resizeClipLoop(resizeTrack,resizeSectionIndex,duration);
            if(!ok){owner.project=std::move(previous);return;}resizeLastLength=duration;owner.refreshControls();repaint();}
        const auto local=e.getEventRelativeTo(&owner.arrangementPort);owner.arrangementPort.autoScroll(local.x,local.y,30,12);return;}
    if(loopDragging){const double beat=std::clamp(std::floor((e.x-headerWidth)/pixelsPerBar)*4.,0.,owner.project.length());owner.setLoopRange(std::min(loopAnchor,beat),std::min(owner.project.length(),std::max(loopAnchor,beat)+4));return;}
    if(clipTrack>=0){clipMoving=clipMoving||std::abs(e.position.x-clipAnchorX)>4||rowAt(e.y)!=rowForTrack(clipTrack);const int row=rowAt(e.y);clipTarget=row>=0?trackAtRow(row):clipTrack;clipDropBeat=snapBeat(owner.project.clipStart(clipTrack,clipSource)+(e.position.x-clipAnchorX)*4./pixelsPerBar,e.mods.isShiftDown());const auto local=e.getEventRelativeTo(&owner.arrangementPort);owner.arrangementPort.autoScroll(local.x,local.y,30,12);repaint();return;}
    if(dragTrack<0||dragPoint<0)return;auto& points=envelope(dragTrack).points;if(dragPoint>=(int)points.size())return;if(bending)points[(size_t)dragPoint].curve=std::clamp(originalCurve+(dragY-e.position.y)/40.,-1.,1.);else editPoint(e.position,!e.mods.isShiftDown());owner.changed(false);}
void ArrangementView::mouseUp(const juce::MouseEvent&){
    if(panning){panning=false;return;}
    if(heightTrack>=0){heightTrack=-1;saveTrackHeights();return;}
    if(resizeBefore){if(resizeLastLength!=resizeOriginalLength){auto after=std::move(owner.project);owner.project=*resizeBefore;owner.checkpoint();owner.project=std::move(after);resizeBefore.reset();resizeSectionIndex=resizeTrack=-1;owner.changed();}else{const double beat=std::clamp((resizeAnchorX-headerWidth)*4./pixelsPerBar,0.,owner.project.length());cancelResize();owner.seekTimeline(beat);}return;}
    if(clipMoving&&clipTrack>=0&&clipTarget>=0)owner.moveSelectedClips(clipTrack,clipSource,clipTarget,clipDropBeat,copyingClip);clipMoving=false;
    clipTrack=clipSource=clipTarget=-1;dragTrack=dragPoint=-1;loopDragging=false;repaint();
}
void ArrangementView::mouseMove(const juce::MouseEvent& e){if(heightEdgeAt(e)>=0){setMouseCursor(juce::MouseCursor::UpDownResizeCursor);setTooltip(tr("Drag the lower edge of this track header to change only its height. Double-click resets; Escape cancels."));return;}const auto view=owner.arrangementPort.getViewArea();const int hoverRow=rowAt(e.y);const bool header=e.y<view.getY()+32;
    if(hoverRow>=0&&e.y>=view.getY()+rulerHeight&&e.x<view.getX()+headerWidth&&e.y<rowY(hoverRow)+rowHeight(hoverRow)) {
        const int x=e.x-view.getX();setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setTooltip(x>=194&&x<214?tr("Open the original Fase Groove instrument controls."):x>=171&&x<194?tr("Listen to soloed tracks."):x>=151&&x<171?tr("Mute this track."):x>=132&&x<151?controlTip("AUTOMATION"):tr("Select this track. Use the gear beside Solo to open its instrument controls."));return;
    }
    const bool edge=header?edgeAt(-1,e.position.x)>=0:e.y>=view.getY()+rulerHeight&&hoverRow>=0&&e.y<rowY(hoverRow)+rowHeight(hoverRow)&&edgeAt(trackAtRow(hoverRow),e.position.x)>=0;
    setMouseCursor(edge?juce::MouseCursor::LeftRightResizeCursor:juce::MouseCursor::NormalCursor);
    if(edge){setTooltip(tr(header?"Drag this guide edge to change its duration. Clips and automation stay in place. Escape cancels; Undo restores.":"Drag this edge to resize only this clip loop. Shorten and extend without losing the source; Shift for quarter-beat timing. Escape cancels; Undo restores."));return;}
    const auto v=owner.arrangementPort.getViewArea();if(e.y<v.getY()+rulerHeight){setTooltip(tr("Click the ruler to seek; Shift-drag to set the loop range."));return;}const int row=rowAt(e.y);if(row<0)return;if(e.y<rowY(row)+rowHeight(row)){setTooltip(tr("Click to select and position playback. Double-click or E opens MIDI editing. Drag to move; Alt-drag copies. Shift-click selects more clips on this track. Use the edit toolbar or right-click for split, cut, copy, paste, duplicate and delete."));return;}setTooltip(tr("Volume automation: click to add, drag to move, Shift for free timing, Alt-drag a segment to curve it; right-click for bypass or delete. Hiding a lane does not bypass it."));}
bool ArrangementView::verifyAutomation(const juce::File& folder){
    const auto saved=owner.project;const auto visibility=volumeVisible;const int track=4;const double zoom=pixelsPerBar;pixelsPerBar=24;owner.select(track,0);owner.arrangementPort.setViewPosition(0,0);
    bool ok=!volumeVisible[(size_t)track]&&!owner.project.tracks[(size_t)track].volume.enabled;
    auto click=[&](juce::Point<float> p,int mods=0){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier|mods),1,0,0,0,0,this,this,juce::Time::getCurrentTime(),p,juce::Time::getCurrentTime(),1,false);};
    mouseDown(click({140,(float)rulerHeight+16}));ok=volumeVisible[(size_t)track]&&ok;
    juce::Point<float> at(headerWidth+24,volumeY(track,.5));mouseDown(click(at));mouseUp(click(at));
    auto& v=owner.project.tracks[(size_t)track].volume;ok=v.enabled&&v.points.size()==3&&std::abs(v.valueAt(4)-.5)<1e-6&&ok;
    mouseDown(click(at));mouseDrag(click(at.translated(24,-8)));mouseUp(click(at));ok=std::abs(v.points[1].beat-8)<1e-6&&std::abs(v.points[1].value-.75)<1e-6&&ok;
    at={headerWidth+12,volumeY(track,.9)};mouseDown(click(at,juce::ModifierKeys::altModifier));mouseDrag(click(at.translated(0,-20),juce::ModifierKeys::altModifier));mouseUp(click(at));ok=v.points[0].curve>.4&&ok;
    owner.changed(false);auto screenshot=owner.createComponentSnapshot(owner.getLocalBounds());if(auto out=folder.getChildFile("volume-automation.png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(screenshot,*out)&&ok;}
    const auto midi=midiFile(owner.project);mouseDown(click({24,(float)rulerHeight+rowHeight(0)+30}));ok=!v.enabled&&midiFile(owner.project)==midi&&ok;
    {
        // The same lane edits pan and the master volume; Undo restores them.
        const auto targets=laneTarget;laneTarget[(size_t)track]=lanePan;
        juce::Point<float> p(headerWidth+48,volumeY(track,.25));mouseDown(click(p));mouseUp(click(p));
        const auto& pan=owner.project.tracks[(size_t)track].panAutomation;ok=pan.enabled&&std::abs(pan.valueAt(8)-.25)<.02&&std::abs(panFromAutomation(pan.valueAt(0))-owner.project.tracks[(size_t)track].pan)<1e-6&&ok;
        laneTarget[(size_t)track]=laneMaster;p={headerWidth+48,volumeY(track,1.5)};mouseDown(click(p));mouseUp(click(p));
        ok=owner.project.masterAutomation.enabled&&std::abs(owner.project.masterAutomation.valueAt(8)-1.5)<.04&&ok;
        owner.undo(false);ok=owner.project.masterAutomation.isDefault()&&ok;   // (quick edits may share one Undo step)
        if(auto out=folder.getChildFile("pan-master-automation.png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(owner.createComponentSnapshot(owner.getLocalBounds()),*out)&&ok;}
        laneTarget=targets;
    }
    mouseDown(click({140,(float)rulerHeight+16}));ok=!volumeVisible[(size_t)track]&&ok;
    owner.project=saved;volumeVisible=visibility;pixelsPerBar=zoom;owner.changed();owner.undoStack.clear();owner.redoStack.clear();folder.getChildFile("volume-automation-ui.txt").replaceWithText(ok?"PASS hidden and bypassed default, lane button, add/move/curve, bypass, pan and master lanes with Undo, hide, MIDI unchanged":"FAIL automation editing");return ok;
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
int PianoRoll::hit(juce::Point<float> p)const{const auto& c=owner.clip();for(int i=(int)c.notes.size()-1;i>=0;--i){const auto& n=c.notes[(size_t)i];juce::Rectangle<float> r(leftMargin()+(float)(n.start*pixelsPerBeat),noteY(n),noteWidth(n),noteHeight());if(r.contains(p))return i;}return -1;}
void PianoRoll::paint(juce::Graphics& g){
    g.fillAll(Palette::background);const auto view=owner.pianoPort.getViewArea();const auto& notes=owner.clip().notes;const float left=(float)leftMargin();
    // Long clips: grid lines and notes outside the repainted area are skipped.
    const auto dirtyArea=g.getClipBounds().expanded(4);
    const auto gridArea=juce::Rectangle<float>(left,48,(float)getWidth()-left,std::max(40.f,velocityTop()-52));
    {
        juce::Graphics::ScopedSaveState clip(g);g.reduceClipRegion(gridArea.toNearestInt());
        if(!laneView())for(int n=lowNote;n<=highNote;++n){float y=48+(highNote-n)*rowHeight();bool black=n%12==1||n%12==3||n%12==6||n%12==8||n%12==10;if(black){g.setColour(juce::Colours::black.withAlpha(.25f));g.fillRect(left,y,(float)getWidth()-left,rowHeight());}if(n%12==0){g.setColour(Palette::panelEdge);g.fillRect(left,y+rowHeight()-1,(float)getWidth()-left,1.f);}}
        const double beats=owner.clipLength();
        for(double b=0;b<=beats;b+=grid){int x=leftMargin()+(int)(b*pixelsPerBeat);if(x<dirtyArea.getX())continue;if(x>dirtyArea.getRight())break;bool bar=std::abs(std::fmod(b,4.))<.0001;g.setColour(bar?Palette::accent.withAlpha(.4f):Palette::panelEdge.withAlpha(std::abs(std::fmod(b,1.))<.0001?1.f:.35f));if(!laneView())g.drawVerticalLine(x,48,velocityTop()-4);}
    }
    if(laneView()){
        const int lanes=owner.selectedTrack==3?12:1;
        for(int l=0;l<lanes;++l){const float y=40+l*40.f;
            for(double b=0;b<owner.clipLength();b+=grid){const float x=left+(float)(b*pixelsPerBeat);if(x+pixelsPerBeat*grid<view.getX()+left||x>view.getRight())continue;g.setColour(Palette::panelEdge.withAlpha(((int)(b*4)/4)%2==0?.55f:.32f));g.fillRoundedRectangle({x+1.5f,y+1.5f,std::max(2.f,(float)(pixelsPerBeat*grid)-3),37.f},3);}
        }
    }
    for(size_t i=0;i<notes.size();++i){const auto& n=notes[i];auto r=juce::Rectangle<float>(left+(float)(n.start*pixelsPerBeat),noteY(n),noteWidth(n),noteHeight()).reduced(.5f,1.f);if(!laneView()&&(n.pitch<lowNote||n.pitch>highNote))continue;if(!r.intersects(dirtyArea.toFloat()))continue;const auto ink=owner.selectedTrack==3?guitarColour(n.lane):colour(owner.selectedTrack);g.setColour(ink.withAlpha(.5f+.5f*n.velocity/127.f));
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
        for(double b=0;b<owner.clipLength();b+=chordBeats){const int x=leftMargin()+(int)(b*pixelsPerBeat);if(x+(int)(chordBeats*pixelsPerBeat)<view.getX()+left||x>view.getRight())continue;auto r=juce::Rectangle<int>(x+1,2,(int)(chordBeats*pixelsPerBeat)-2,20);g.setColour(Palette::panelEdge);g.fillRoundedRectangle(r.toFloat(),4);label(g,utf(names[(size_t)((int)((owner.clipStart()+b)/chordBeats)%names.size())]),r.reduced(6,0),11);label(g,juce::String((int)(b/4)+1),{x+3,24,50,14},10,Palette::dim);}
    }
    g.setColour(Palette::panel);g.fillRect(vx,0,leftMargin(),40);
    g.setColour(Palette::background);g.fillRect(vx,(int)velocityTop(),view.getWidth(),getHeight()-(int)velocityTop());label(g,"VELOCITY",{vx+4,(int)velocityTop(),leftMargin()-8,(int)velocityHeight()},9,Palette::dim);
    g.setColour(Palette::panelEdge);g.drawHorizontalLine((int)velocityTop(),(float)vx,(float)view.getRight());
    if(owner.selectedTrack>=4)for(double b=0;b<owner.clipLength();b+=grid){const float x=left+(float)(b*pixelsPerBeat);if(x<view.getX()+left||x>view.getRight())continue;auto column=juce::Rectangle<float>(x+1,velocityTop()+8,std::max(3.f,(float)(pixelsPerBeat*grid)-3),velocityHeight()-4);g.setColour(Palette::panelEdge.withAlpha(((int)(b*4)/4)%2==0?.55f:.32f));g.fillRoundedRectangle(column,3);}
    for(size_t i=0;i<notes.size();++i){const auto& n=notes[i];const float x=left+(float)(n.start*pixelsPerBeat);if(x<vx+left||x>view.getRight())continue;float h=n.velocity/127.f*velocityHeight();g.setColour(selected.count((int)i)?Palette::text:colour(owner.selectedTrack).withAlpha(.8f));g.fillRect(x-1,getHeight()-12-h,3.f,h);g.fillEllipse(x-3,getHeight()-15-h,6,6);}
    const auto local=owner.audio.position()-owner.clipStart();if(local>=0&&local<owner.clipLength()){float x=left+(float)(local*pixelsPerBeat);if(x>vx+left){g.setColour(Palette::text);g.fillRect(x,24.f,1.5f,(float)getHeight()-24);}}
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
    grabKeyboardFocus();editingVelocity=false;if(e.x<owner.pianoPort.getViewArea().getX()+leftMargin())return;
    if(e.y<40){owner.seekTimeline(owner.clipStart()+std::clamp(beat(e.position.x),0.,owner.clipLength()));return;}
    if(e.y>=velocityTop()){owner.checkpoint();editingVelocity=true;editVelocity(e.position);return;}
    if(laneView()&&e.y>=40+(owner.selectedTrack==3?12:1)*40)return;
    if(!laneView()&&e.y<48)return;
    int index=hit(e.position);if(tool==1&&index<0&&!e.mods.isPopupMenu()){selecting=true;anchor=e.position;marquee={};if(!e.mods.isShiftDown())selected.clear();repaint();return;}if(e.mods.isPopupMenu()||tool==2){if(index>=0){owner.checkpoint();if(laneView()&&owner.selectedTrack==3){const auto note=owner.clip().notes[(size_t)index];auto& notes=owner.clip().notes;notes.erase(std::remove_if(notes.begin(),notes.end(),[&](const Note& n){return n.lane==note.lane&&std::abs(n.start-note.start)<.18;}),notes.end());}else owner.clip().notes.erase(owner.clip().notes.begin()+index);selected.clear();owner.changed();}return;}
    gestureOriginal=owner.clip().notes;anchor=e.position;resizing=false;velocityDrag=e.mods.isAltDown();before=owner.clip().notes;
    if(index>=0){if(e.mods.isShiftDown()){if(selected.count(index))selected.erase(index);else selected.insert(index);}else if(!selected.count(index)){selected.clear();selected.insert(index);if(laneView()&&owner.selectedTrack==3){const auto hitNote=before[(size_t)index];for(size_t k=0;k<before.size();++k)if(before[k].lane==hitNote.lane&&std::abs(before[k].start-hitNote.start)<.18)selected.insert((int)k);}}const auto& n=before[(size_t)index];resizing=!laneView()&&e.x>=leftMargin()+(n.start+n.length)*pixelsPerBeat-6&&!velocityDrag;}
    else{if(!e.mods.isShiftDown())selected.clear();double start=std::floor(beat(e.position.x)/grid)*grid;if(start>=owner.clipLength())return;if(laneView()&&owner.selectedTrack==3){const int lane=laneAt(e.position.y);const auto& settings=owner.project.settings;const int slot=((int)(start/(settings.chordSteps*.25)))%settings.chordCount;auto chord=settings.freeChords[(size_t)slot]>=0?fgg::codeChord(settings.key,settings.freeChords[(size_t)slot],std::min(2,settings.extension)):fgg::diatonicChord(settings.key,settings.scale,settings.degrees[(size_t)slot],std::min(2,settings.extension));auto notes=fgg::laneNotes((fgg::Lane)lane,fgg::tonesOf(chord),(int)(start*4)%16);for(int note:notes){owner.clip().notes.push_back({start,std::min(grid,owner.clipLength()-start),note,brushVelocity,lane});selected.insert((int)owner.clip().notes.size()-1);}}
        else{owner.clip().notes.push_back({start,std::min(grid,owner.clipLength()-start),pitch(e.position.y),owner.selectedTrack>=4?brushVelocity:(int)owner.velocitySlider.getValue(),owner.selectedTrack>=4?owner.selectedTrack-4:0});selected.insert((int)owner.clip().notes.size()-1);}if(laneView()&&flamBrush&&owner.clip().notes.size()>before.size()){auto note=owner.clip().notes.back();note.start=std::max(0.,note.start-.08);note.length=.04;note.velocity=45;owner.clip().notes.push_back(note);}before=owner.clip().notes;}
    repaint();
}
void PianoRoll::mouseDrag(const juce::MouseEvent& e){
    if(selecting){marquee=juce::Rectangle<float>(anchor,e.position);selected.clear();for(size_t i=0;i<owner.clip().notes.size();++i){const auto& n=owner.clip().notes[i];if(marquee.intersects(juce::Rectangle<float>(leftMargin()+(float)(n.start*pixelsPerBeat),noteY(n),noteWidth(n),noteHeight())))selected.insert((int)i);}repaint();return;}
    if(editingVelocity){editVelocity(e.position);return;}
    if(before.empty()||selected.empty())return;double delta=std::round((e.position.x-anchor.x)/pixelsPerBeat/grid)*grid;int deltaPitch=-(int)std::round((e.position.y-anchor.y)/rowHeight());auto& notes=owner.clip().notes;
    if(!velocityDrag&&!resizing){
        double earliest=owner.clipLength(),latest=0;int lowest=127,highest=0;
        for(int i:selected)if(i>=0&&i<(int)before.size()){const auto& n=before[(size_t)i];earliest=std::min(earliest,n.start);latest=std::max(latest,n.start+n.length);lowest=std::min(lowest,n.pitch);highest=std::max(highest,n.pitch);}
        delta=std::clamp(delta,-earliest,std::max(-earliest,owner.clipLength()-latest));deltaPitch=std::clamp(deltaPitch,-lowest,127-highest);
    }
    for(int i:selected)if(i>=0&&i<(int)notes.size()&&i<(int)before.size()){const auto n=before[(size_t)i];auto& now=notes[(size_t)i];if(velocityDrag)now.velocity=std::clamp(n.velocity+(int)(anchor.y-e.position.y),1,127);else if(resizing)now.length=std::clamp(n.length+delta,std::min(.0625,owner.clipLength()-n.start),owner.clipLength()-n.start);else{now.start=std::clamp(n.start+delta,0.,std::max(0.,owner.clipLength()-n.length));if(!laneView())now.pitch=std::clamp(n.pitch+deltaPitch,0,127);else if(owner.selectedTrack==3)now.lane=std::clamp(n.lane+(int)std::round((e.position.y-anchor.y)/40.),0,11);}}
    repaint();
}
void PianoRoll::mouseUp(const juce::MouseEvent&){
    if(selecting){selecting=false;marquee={};repaint();return;}
    if(editingVelocity){editingVelocity=false;owner.changed();return;}
    if(before.empty())return;
    before.clear();const bool modified=owner.clip().notes!=gestureOriginal;
    if(!modified){gestureOriginal.clear();repaint();return;}
    auto result=std::move(owner.clip().notes);owner.clip().notes=std::move(gestureOriginal);owner.checkpoint();owner.clip().notes=std::move(result);
    std::vector<Note> wanted;
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
    if(!laneView()){
        owner.clip().notes={{1,.5,60,100,0},{2,.5,64,100,0}};selected={0,1};fitNotes();
        auto at=juce::Point<float>(leftMargin()+(float)pixelsPerBeat+2,noteY(owner.clip().notes[0])+noteHeight()/2);
        mouseDown(event(at));mouseDrag(event(at.translated((float)(-4*pixelsPerBeat),0)));mouseUp(event(at.translated((float)(-4*pixelsPerBeat),0)));
        ok=ok&&owner.clip().notes.size()==2&&std::abs(owner.clip().notes[0].start)<1e-9&&std::abs(owner.clip().notes[1].start-1)<1e-9;
    }
    const auto notesBeforeSeek=owner.clip().notes;
    mouseDown(event({leftMargin()+(float)(2.375*pixelsPerBeat),24}));
    ok=ok&&std::abs(owner.audio.position()-owner.clipStart()-2.375)<1e-6&&owner.clip().notes.size()==notesBeforeSeek.size();
    owner.clip()=saved;tool=savedTool;flamBrush=savedFlam;clearSelection();owner.changed();fitNotes();return ok;
}
void PianoRoll::deleteSelected(){if(selected.empty())return;owner.checkpoint();auto& notes=owner.clip().notes;for(auto i=selected.rbegin();i!=selected.rend();++i)if(*i>=0&&*i<(int)notes.size())notes.erase(notes.begin()+*i);selected.clear();owner.changed();}
bool PianoRoll::keyPressed(const juce::KeyPress& key){if(editKeys(key))return true;if(key==juce::KeyPress::deleteKey||key==juce::KeyPress::backspaceKey){deleteSelected();return true;}if(key.getModifiers().isCommandDown()&&key.getKeyCode()=='C'){owner.copyButton.onClick();return true;}if(key.getModifiers().isCommandDown()&&key.getKeyCode()=='V'){owner.pasteButton.onClick();return true;}if(key.getModifiers().isCommandDown()&&key.getKeyCode()=='A'){selected.clear();for(size_t i=0;i<owner.clip().notes.size();++i)selected.insert((int)i);repaint();return true;}return owner.keyPressed(key);}

class MainComponent::PluginWindow : public juce::DocumentWindow {
public:
    explicit PluginWindow(juce::AudioPluginInstance& p):DocumentWindow(p.getName(),Palette::panel,DocumentWindow::closeButton){setUsingNativeTitleBar(true);auto* editor=p.createEditorIfNeeded();if(!editor)editor=new juce::GenericAudioProcessorEditor(p);setContentOwned(editor,true);setResizable(editor->isResizable(),false);centreWithSize(getWidth(),getHeight());setVisible(true);}
    void closeButtonPressed()override{setVisible(false);}
};
MainComponent::MainComponent(){
    Localisation::loadSaved();for(int track=0;track<trackCount;++track){const auto key="trackHeight"+juce::String(track);arrangement.trackHeights[(size_t)track]=std::clamp(Localisation::setting(key.toRawUTF8(),"40").getIntValue(),40,240);}if(Localisation::tipsEnabled())tooltips=std::make_unique<juce::TooltipWindow>(this,650);setLookAndFeel(&theme);setWantsKeyboardFocus(true);project=newProject();selectedTrack=4;mixerHeight=std::clamp(Localisation::setting("mixerHeight","244").getIntValue(),160,4096);inspectorWidth=std::clamp(Localisation::setting("inspectorWidth","216").getIntValue(),184,480);sampleBrowserWidth=std::max(0,Localisation::setting("sampleBrowserWidth","0").getIntValue());
    addAndMakeVisible(timelineGrid);timelineGrid.addItemList({"SNAP: 1 bar","SNAP: 1 beat","SNAP: 1/2 beat","SNAP: 1/4 beat","SNAP: OFF"},1);timelineGrid.setSelectedId(2);timelineGrid.onChange=[this]{const double grids[]{4,1,.5,.25,0};timelineSnap=grids[std::clamp(timelineGrid.getSelectedItemIndex(),0,4)];};
    for(juce::Component* c:std::initializer_list<juce::Component*>{&backButton,&instrumentButton,&editButton,&leadButton,&arrangeButton,&guitarNotesButton,&durationLabel,&sectionSelector,&panelSections})addAndMakeVisible(c);
    instrumentPort.setViewedComponent(&instrumentPanel,false);instrumentPort.setScrollBarsShown(true,false);addAndMakeVisible(instrumentPort);
    backButton.onClick=[this]{showArrangement();};instrumentButton.onClick=[this]{showInstrument();};editButton.onClick=[this]{showEditor();};
    addAndMakeVisible(fitNotesButton);muteButton.setClickingTogglesState(true);soloButton.setClickingTogglesState(true);
    int toolIndex=0;for(auto* button:{&drawTool,&selectTool,&eraseTool}){addAndMakeVisible(button);button->setClickingTogglesState(true);button->setRadioGroupId(4001);const int index=toolIndex++;button->onClick=[this,index]{piano.tool=index;};}drawTool.setToggleState(true,juce::dontSendNotification);
    drawTool.setConnectedEdges(juce::Button::ConnectedOnRight);selectTool.setConnectedEdges(juce::Button::ConnectedOnLeft|juce::Button::ConnectedOnRight);eraseTool.setConnectedEdges(juce::Button::ConnectedOnLeft);
    drawTool.setComponentID("tool-draw");selectTool.setComponentID("tool-select");eraseTool.setComponentID("tool-erase");
    drawTool.getProperties().set("fgaTip","Pencil: click to draw notes, drag to move, drag the edge for duration.");selectTool.getProperties().set("fgaTip","Select: drag a rectangle to select notes, then move or edit them.");eraseTool.getProperties().set("fgaTip","Eraser: click or drag over notes to delete them.");
    addAndMakeVisible(regenerateClipButton);regenerateClipButton.setButtonText("REGENERATE");regenerateClipButton.setComponentID("editor-regenerate");regenerateClipButton.iconColour=Palette::accent;regenerateClipButton.onClick=[this]{regenerateEditedClip();};
    regenerateClipButton.getProperties().set("fgaTip","Regenerate this clip from the instrument panel settings: a new variation of the same groove. Locked clips are kept; Undo restores the notes.");
    int brushIndex=0;for(auto* button:{&brushAccent,&brushHit,&brushGhost,&brushFlam}){addAndMakeVisible(button);button->setClickingTogglesState(true);button->setRadioGroupId(4002);const int index=brushIndex++;button->onClick=[this,index]{piano.brushVelocity=index==0?127:index==2?45:100;piano.flamBrush=index==3;};button->setTooltip(tr("Choose the hit to draw, as in the Drums and Guitar editor."));button->setConnectedEdges(juce::Button::ConnectedOnLeft|juce::Button::ConnectedOnRight);}brushAccent.setConnectedEdges(juce::Button::ConnectedOnRight);brushFlam.setConnectedEdges(juce::Button::ConnectedOnLeft);brushHit.setToggleState(true,juce::dontSendNotification);
    fitNotesButton.onClick=[this]{piano.fitNotes();piano.repaint();};
    guitarNotesButton.onClick=[this]{piano.guitarNotes=!piano.guitarNotes;showEditor();};
    leadButton.setClickingTogglesState(true);leadButton.onClick=[this]{if(loadingPlugin)return;checkpoint();project.settings.guitarLead=!project.settings.guitarLead;generate(project,Scope::track,0,selectedSection);generate(project,Scope::track,3,selectedSection);changed();};
    durationLabel.getProperties().set("fgaTip","Song length follows genre, harmonic phrases and groove development.");durationLabel.setTooltip(tr("Song length follows genre, harmonic phrases and groove development."));
    arrangeButton.onClick=[this]{if(loadingPlugin)return;for(const auto& t:project.tracks){if(t.locked){message("Sblocca le tracce prima di ricostruire la struttura. GENERATE SONG mantiene la struttura e i blocchi.");return;}for(const auto& c:t.clips)if(c.locked){message("Sblocca i clip prima di ricostruire la struttura.");return;}}checkpoint();audio.stop();project.settings.seed=(std::uint32_t)juce::Random::getSystemRandom().nextInt();arrangeSong(project);editCursor=-1;generate(project,Scope::song,0,0,project.settings.seed?project.settings.seed:1u);selectedSection=0;arrangement.selectedClips.clear();noClipSelection=false;changed();showArrangement();};
    panelSections.onChange=[this]{auto names=instrumentPanel.sectionNames();int index=panelSections.getSelectedItemIndex();if(index>=0&&index<names.size())instrumentPort.setViewPosition(0,instrumentPanel.sectionPosition(names[index]));};
    sectionSelector.onChange=[this]{if(!refreshing){select(selectedTrack,sectionSelector.getSelectedItemIndex());if(page==Page::editor)showEditor();}};
    addAndMakeVisible(chordEditButton);chordEditButton.onClick=[this]{showChordMenu();};
    inspectorPort.setViewedComponent(&trackInspector,false);inspectorPort.setScrollBarsShown(true,false);addAndMakeVisible(inspectorPort);addAndMakeVisible(mixer);
    for(auto* button:{&inspectorToggle,&mixerToggle,&followButton}){addAndMakeVisible(button);button->setClickingTogglesState(true);}
    addAndMakeVisible(inspectorResizer);addChildComponent(browserResizer);addAndMakeVisible(inspectorCollapse);inspectorCollapse.setComponentID("inspector-collapse");
    inspectorCollapse.getProperties().set("fgaTip","Collapse the track inspector to make room for tracks; click the arrow again to restore it. Shortcut: I.");
    inspectorCollapse.onClick=[this]{inspectorVisible=!inspectorVisible;inspectorToggle.setToggleState(inspectorVisible,juce::dontSendNotification);resized();};
    addAndMakeVisible(fitSongButton);inspectorToggle.setToggleState(true,juce::dontSendNotification);mixerToggle.setToggleState(true,juce::dontSendNotification);
    inspectorToggle.onClick=[this]{inspectorVisible=inspectorToggle.getToggleState();resized();};
    mixerToggle.onClick=[this]{mixerVisible=mixerToggle.getToggleState();resized();};
    followButton.onClick=[this]{followPlayhead=followButton.getToggleState();};
    fitSongButton.onClick=[this]{fitSong();};
    // Sample browser docked on the right of the arrangement (B).
    addAndMakeVisible(browserToggle);browserToggle.setClickingTogglesState(true);browserToggle.onClick=[this]{showSampleBrowser(browserToggle.getToggleState());};
    browserToggle.getProperties().set("fgaTip","Show the sample browser: your sample folders, preview and drag onto a track. Shortcut: B.");
    addChildComponent(sampleBrowser);
    sampleBrowser.onLoad=[this](const juce::File& file){if(loadingPlugin){message(tr("Wait for the instrument to finish loading."),true);return;}loadSampleFile(selectedTrack,file);};
    sampleBrowser.onMessage=[this](const juce::String& text,bool error){message(text,error);};
    sampleBrowser.targetName=[this]{return utf(project.tracks[(size_t)selectedTrack].name);};
    browserVisible=Localisation::setting("sampleBrowserVisible","0")=="1";browserToggle.setToggleState(browserVisible,juce::dontSendNotification);
    addAndMakeVisible(metronomeButton);metronomeButton.onClick=[this]{toggleMetronome();};
    metronomeButton.getProperties().set("fgaTip","Metronome while playing (never exported). Shortcut: C.");
    positionLabel.setFont(FamilyTheme::font(13.f,true));positionLabel.setColour(juce::Label::textColourId,Palette::accent2);positionLabel.setJustificationType(juce::Justification::centredLeft);
    for(auto* b:{&zoomSelectionButton,&loopSelectionButton})addAndMakeVisible(b);
    zoomSelectionButton.onClick=[this]{zoomSelection();};loopSelectionButton.onClick=[this]{loopSelection();};
    zoomSelectionButton.getProperties().set("fgaTip","Zoom to the selected clips or notes. Shortcut: Z.");
    loopSelectionButton.getProperties().set("fgaTip","Set the playback loop to the selected clips or notes. Shortcut: Ctrl/Cmd+L.");
    inspectorToggle.getProperties().set("fgaTip","Show track volume, pan, sound assignment and clip actions.");mixerToggle.getProperties().set("fgaTip","Show the mixer. Shortcut: F3.");fitSongButton.getProperties().set("fgaTip","Fit the complete song in the arrangement view.");followButton.getProperties().set("fgaTip","Scroll the arrangement to follow playback.");
    arrangement.getProperties().set("fgaTip","Wheel on ruler: zoom. Ctrl/Cmd+wheel: zoom at pointer. Shift+wheel: horizontal scroll.");piano.getProperties().set("fgaTip","Wheel on ruler: zoom. Ctrl/Cmd+wheel: zoom at pointer. Shift+wheel: horizontal scroll.");
    arrangementPort.setViewedComponent(&arrangement,false);pianoPort.setViewedComponent(&piano,false);arrangementPort.setScrollBarsShown(true,true);pianoPort.setScrollBarsShown(true,true);addAndMakeVisible(arrangementPort);addAndMakeVisible(pianoPort);
    for(juce::Component* c:std::initializer_list<juce::Component*>{&newButton,&openButton,&saveButton,&saveAsButton,&exportButton,&settingsButton,&aboutButton,&helpButton,&playButton,&stopButton,&rewindButton,&undoButton,&redoButton,&loopButton,&songButton,&trackButton,&sectionButton,&clipButton,&harmonyButton,&addSectionButton,&genreBox,&keyBox,&scaleBox,&extensionBox,&voicingBox,&progressionBox,&bassBox,&stringsBox,&tempoSlider,&swingSlider,&densitySlider,&humanizeSlider,&masterSlider,&zoomSlider,&positionLabel,&chordsLabel,&statusLabel,&soundButton,&pluginEditorButton,&sampleButton,&loadPluginButton,&gainSlider,&panSlider,&velocitySlider,&muteButton,&soloButton,&lockButton,&lockClipButton,&soundBox,&gridBox,&quantiseButton,&transposeUp,&transposeDown,&deleteNotes,&importButton,&copyButton,&pasteButton,&sectionLoopButton,&inspectorLabel,&editorLabel})addAndMakeVisible(c);
    auto slider=[this](juce::Slider& s,double lo,double hi,double step,const juce::String& suffix){s.setRange(lo,hi,step);s.setSliderStyle(juce::Slider::LinearHorizontal);s.setTextBoxStyle(juce::Slider::TextBoxRight,false,64,22);s.setTextValueSuffix(suffix);s.onDragStart=[this]{checkpoint();};};
    addAndMakeVisible(automationButton);automationButton.setClickingTogglesState(true);
    automationButton.onClick=[this]{auto& visible=arrangement.volumeVisible[(size_t)selectedTrack];visible=!visible;updateSizes();arrangement.repaint();};
    automationButton.setTooltip(tr("Show or hide the selected track's volume automation. Playback is controlled by ON / BYPASS in the lane."));
    slider(tempoSlider,30,300,.1," BPM");slider(swingSlider,0,150,1,"% swing");slider(densitySlider,0,100,1,"% notes");slider(humanizeSlider,0,100,1,"% feel");slider(masterSlider,-60,6,.1," dB");slider(zoomSlider,.05,160,.05," zoom");slider(gainSlider,-60,6,.1," dB");slider(panSlider,-100,100,1," pan");slider(velocitySlider,1,127,1," vel");velocitySlider.setValue(96);
    for(size_t i=0;i<genres().size();++i)genreBox.addItem(utf(genres()[i]),(int)i+1);
    for(int i=0;i<12;++i)keyBox.addItem(sgk::noteName(i),i+1);
    for(int i=0;i<13;++i)scaleBox.addItem(sgk::scaleName(i),i+1);
    for(int i=0;i<11;++i){extensionBox.addItem(sgk::extensionName(i),i+1);voicingBox.addItem(sgk::voicingName(i),i+1);}
    const auto& progressions=sgk::progressionPresets();for(size_t i=0;i<progressions.size();++i)progressionBox.addItem(progressions[i].name,(int)i+1);progressionBox.setTextWhenNothingSelected("PROGRESSION");
    for(int i=0;i<sgk::numBassModes;++i)bassBox.addItem(sgk::bassModeName(i),i+1);for(int i=0;i<sgk::numStringsModes;++i)stringsBox.addItem(sgk::stringsModeName(i),i+1);
    gridBox.addItem("1/4",1);gridBox.addItem("1/8",2);gridBox.addItem("1/16",3);gridBox.addItem("1/32",4);gridBox.setSelectedId(3);
    auto setting=[this](std::function<void()> fn){if(refreshing)return;checkpoint();fn();changed(false);message("Settings updated. REGENERATE applies them to MIDI. GENERATE SONG draws a new song.");};
    genreBox.onChange=[this,setting]{setting([this]{applyGenre(project,genreBox.getSelectedItemIndex());});message("Genre ready: NEW ARRANGEMENT rebuilds the form; GENERATE SONG varies tempo, length and patterns inside this genre.");};
    keyBox.onChange=[this,setting]{setting([this]{project.settings.key=keyBox.getSelectedItemIndex();});};
    scaleBox.onChange=[this,setting]{setting([this]{project.settings.scale=scaleBox.getSelectedItemIndex();});};
    extensionBox.onChange=[this,setting]{setting([this]{project.settings.extension=extensionBox.getSelectedItemIndex();});};
    voicingBox.onChange=[this,setting]{setting([this]{project.settings.voicing=voicingBox.getSelectedItemIndex();});};
    progressionBox.onChange=[this,setting]{setting([this]{chooseProgression(project,progressionBox.getSelectedItemIndex());});};
    bassBox.onChange=[this,setting]{setting([this]{project.settings.bassMode=bassBox.getSelectedItemIndex();});};
    stringsBox.onChange=[this,setting]{setting([this]{project.settings.stringsMode=stringsBox.getSelectedItemIndex();});};
    tempoSlider.onValueChange=[this]{if(!refreshing){project.settings.bpm=tempoSlider.getValue();changed(false);}};
    swingSlider.onValueChange=[this]{if(!refreshing){project.settings.swing=swingSlider.getValue()/100.;changed(false);}};
    densitySlider.onValueChange=[this]{if(!refreshing){project.settings.density=densitySlider.getValue()/100.;changed(false);}};
    humanizeSlider.onValueChange=[this]{if(!refreshing){project.settings.humanize=humanizeSlider.getValue()/100.;changed(false);}};
    masterSlider.onValueChange=[this]{if(!refreshing){project.masterGain=masterSlider.getValue()<=-59.99?0.:juce::Decibels::decibelsToGain(masterSlider.getValue());mixChanged();}};
    zoomSlider.onDragStart=nullptr;zoomSlider.onValueChange=[this]{const bool editor=page==Page::editor;auto& port=editor?pianoPort:arrangementPort;const double scale=editor?piano.pixelsPerBeat:arrangement.pixelsPerBar;zoomAt(editor,zoomSlider.getValue()/scale,port.getWidth()*.5);};zoomSlider.setValue(6,juce::sendNotificationSync);
    gainSlider.onValueChange=[this]{if(!refreshing){project.tracks[(size_t)selectedTrack].gain=gainSlider.getValue()<=-59.99?0.:juce::Decibels::decibelsToGain(gainSlider.getValue());mixChanged();}};
    panSlider.onValueChange=[this]{if(!refreshing){project.tracks[(size_t)selectedTrack].pan=panSlider.getValue()/100.;mixChanged();}};
    velocitySlider.onDragStart=[this]{if(!piano.selected.empty())checkpoint();};velocitySlider.onValueChange=[this]{if(!refreshing&&!piano.selected.empty()){for(int i:piano.selected)if(i>=0&&i<(int)clip().notes.size())clip().notes[(size_t)i].velocity=(int)velocitySlider.getValue();changed();}};
    auto toggle=[this](juce::Button& b,bool Track::*field){b.onClick=[this,&b,field]{checkpoint();project.tracks[(size_t)selectedTrack].*field=b.getToggleState();changed(false);};};toggle(muteButton,&Track::mute);toggle(soloButton,&Track::solo);toggle(lockButton,&Track::locked);
    lockClipButton.onClick=[this]{checkpoint();clip().locked=lockClipButton.getToggleState();changed(false);};
    playButton.onClick=[this]{togglePlayback();};stopButton.onClick=[this]{editCursor=-1;audio.stop();};rewindButton.onClick=[this]{seekTimeline(0);};
    loopButton.onClick=[this]{toggleLoop();};
    loopButton.getProperties().set("fgaTip","Loop playback on or off. Shortcut: L. Ctrl/Cmd+L loops the selection; again removes it. Double-click the loop strip to clear it.");
    sectionLoopButton.onClick=[this]{toggleClipLoop();};sectionLoopButton.getProperties().set("fgaTip","Loop the selected clip; click again to remove the loop.");
    songButton.onClick=[this]{regenerate(Scope::song);};trackButton.onClick=[this]{regenerate(Scope::track,page!=Page::instrument);};sectionButton.onClick=[this]{regenerate(Scope::section);};clipButton.onClick=[this]{regenerate(Scope::clip,page!=Page::instrument);};
    harmonyButton.onClick=[this]{checkpoint();randomiseHarmony(project,(std::uint32_t)juce::Random::getSystemRandom().nextInt());generate(project);changed();message("New chords written into unlocked tracks. GENERATE SONG draws another harmony.");};
    undoButton.onClick=[this]{undo(false);};redoButton.onClick=[this]{undo(true);};
    quantiseButton.onClick=[this]{modifySelected([this](Clip& c){quantise(c,piano.grid,1,clipLength());});};transposeUp.onClick=[this]{modifySelected([](Clip& c){transpose(c,12);});};transposeDown.onClick=[this]{modifySelected([](Clip& c){transpose(c,-12);});};deleteNotes.onClick=[this]{piano.deleteSelected();};
    gridBox.onChange=[this]{piano.grid=std::pow(.5,gridBox.getSelectedItemIndex());piano.repaint();};
    copyButton.onClick=[this]{clipboard.clear();for(size_t i=0;i<clip().notes.size();++i)if(piano.selected.empty()||piano.selected.count((int)i))clipboard.push_back(clip().notes[i]);message("Notes copied. Paste inserts at the cursor inside the clip.");};
    pasteButton.onClick=[this]{piano.pasteNotes();};
    soundButton.setButtonText("BUILT-IN");soundButton.onClick=[this]{useBuiltInSound(selectedTrack);};
    sampleButton.onClick=[this]{browseSamplesFor(selectedTrack);};
    loadPluginButton.onClick=[this]{showPluginMenu();};
    pluginEditorButton.onClick=[this]{if(loadingPlugin)return;auto* inst=audio.plugin(selectedTrack);auto* fx=audio.effectPlugin(selectedTrack);if(!inst&&!fx){message("Nessun plugin caricato su questa traccia.");return;}if(inst&&fx){juce::PopupMenu menu;menu.addItem(1,tr("OPEN INSTRUMENT"));menu.addItem(2,tr("OPEN EFFECT"));juce::Component::SafePointer<MainComponent> safe(this);menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&pluginEditorButton),[safe](int id){if(!safe||id==0)return;auto* p=id==2?safe->audio.effectPlugin(safe->selectedTrack):safe->audio.plugin(safe->selectedTrack);if(!p)return;safe->pluginWindow.reset();safe->pluginWindow=std::make_unique<PluginWindow>(*p);});return;}pluginWindow.reset();pluginWindow=std::make_unique<PluginWindow>(*(inst?inst:fx));};
    soundBox.onChange=[this]{if(refreshing)return;checkpoint();project.tracks[(size_t)selectedTrack].sound=soundBox.getSelectedItemIndex();changed(false);};
    importButton.onClick=[this]{chooseFile("Importa note nella sezione selezionata","*.mid;*.midi",juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this](juce::File f){importFile(f);});};
    addSectionButton.onClick=[this]{checkpoint();project.insertGuide(selectedSection+1,{Section::main,8,"Main"});selectedSection++;changed();};
    saveButton.onClick=[this]{save(false);};saveAsButton.onClick=[this]{save(true);};
    openButton.onClick=[this]{chooseFile("Apri progetto Fase Groove Arranger","*.fga",juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this](juce::File f){open(f);});};
    newButton.onClick=[this]{if(loadingPlugin)return;if(!requestClose())return;audio.stop();pluginWindow.reset();undoStack.clear();redoStack.clear();for(int i=0;i<trackCount;++i)audio.useInternal(i);project=newProject(genreBox.getSelectedItemIndex());editCursor=-1;projectFile=juce::File();selectedSection=0;selectedTrack=4;arrangement.selectedClips.clear();noClipSelection=false;changed();showArrangement();};
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
    addChildComponent(pluginSettings);addChildComponent(pluginBrowser);
    pluginBrowser.onChoose=[this](const PluginCatalog::BrowserRow& row){chooseBrowserRow(row);};
    pluginBrowser.onRescan=[this]{pluginCatalog.scan();};
    pluginBrowser.onSettings=[this]{pluginSettings.show();};
    pluginBrowser.onOpenFile=[this]{const int track=selectedTrack;chooseFile(tr("Open VST3 file..."),"*.vst3",juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::canSelectDirectories,[this,track](juce::File file){loadPluginFile(track,file);});};
    options.onPluginSettings=[this]{pluginSettings.show();};
    options.onAudioSettings=[this]{showAudioSettings();};
    options.onDocument=[this](int id){if(id==0)showGuide(Localisation::current()!=Localisation::Language::italian);if(id==1){const bool it=Localisation::current()==Localisation::Language::italian;showDocument(tr("Privacy and permissions"),juce::String::fromUTF8(it?BinaryData::PRIVACY_it_md:BinaryData::PRIVACY_md,it?BinaryData::PRIVACY_it_mdSize:BinaryData::PRIVACY_mdSize));}if(id==2)showDocument(tr("Licences and attributions"),juce::String::fromUTF8(BinaryData::THIRD_PARTY_NOTICES_md,BinaryData::THIRD_PARTY_NOTICES_mdSize));};
    options.onCheckUpdates=[this]{options.checkUpdatesButton.setEnabled(false);options.checkUpdatesButton.setButtonText(tr("CHECKING..."));updater.start(true);};
    updateOverlay.onInstall=[this](const Updater::Result& result){
        if(loadingPlugin){message(tr("Wait for the instrument to finish loading."),true);return;}
        if(!requestClose())return;
        const auto report=Updater::install(result);
        if(!report.done){message(tr("The files could not be replaced automatically: the download is shown, copy it as the first time."),true);return;}
        updateOverlay.setVisible(false);
        if(report.relaunching){juce::MessageManager::callAsync([]{if(auto* app=juce::JUCEApplicationBase::getInstance())app->quit();});}
        else message(tr("Updated. Close and reopen the app to use the new version."));
    };
    Updater::cleanupAfterUpdate();
    updater.onResult=[this](const Updater::Result& result,bool manual){
        options.checkUpdatesButton.setEnabled(true);options.refreshText();
        if(result.status==Updater::Result::Status::ready){updateOverlay.show(result);
        }else if(manual){
            const auto text=result.status==Updater::Result::Status::upToDate?tr("You have the latest version."):result.status==Updater::Result::Status::noRelease?tr("No published release yet. Development builds are in GitHub Actions."):tr("Could not check for updates. Check your internet connection and try again.");
            message(text);juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,JUCE_APPLICATION_NAME_STRING,text);
        }
    };
    if(Updater::autoCheckEnabled()&&Updater::firstAutoCheckThisSession()&&!juce::JUCEApplication::getCommandLineParameters().contains("smoke-test"))updater.start(false);
    for(auto* s:{&tempoSlider,&swingSlider,&densitySlider,&humanizeSlider,&masterSlider,&gainSlider,&panSlider}){
        auto callback=s->onValueChange;trackGesture(*s);
        s->onValueChange=[this,s,callback]{if(!refreshing&&!inGesture(*s))checkpoint(s);if(callback)callback();};
    }
    refreshControls();audio.publish(project);setSize(1440,980);select(4,0);showArrangement();fitSong();translateControls(*this);message(juce::String::fromUTF8("Pronto · Spazio: play/pausa · Tasto destro: opzioni · Autosave ogni minuto."));startTimerHz(20);
}
MainComponent::~MainComponent(){updater.onResult=nullptr;if(musicalToolWindow)musicalToolWindow->closeButtonPressed();stopTimer();pluginWindow.reset();audio.stop();setLookAndFeel(nullptr);}
void MainComponent::closePluginEditor(){pluginWindow.reset();}
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
    const auto fx=utf(project.tracks[(size_t)track].effectPath);if(fx.isNotEmpty())source+=" + "+(juce::File::isAbsolutePath(fx)?juce::File(fx).getFileNameWithoutExtension():fx);
    return source;
}
// The arrangement repaints its headers on every playback tick: their source names are kept
// until refreshTrack() (every edit, selection, language or plugin change) clears them.
const juce::String& MainComponent::cachedSourceName(int track){
    const auto i=(size_t)std::clamp(track,0,trackCount-1);if(!sourceNameCached[i]){sourceNames[i]=soundSourceName(track);sourceNameCached[i]=true;}return sourceNames[i];
}
void MainComponent::trackGesture(juce::Slider& s,std::function<void()> onEnd){
    s.onDragStart=[this,&s]{s.getProperties().set("fgaGesture",true);checkpoint();};
    s.onDragEnd=[&s,onEnd]{s.getProperties().set("fgaGesture",false);if(onEnd)onEnd();};
}
void MainComponent::useBuiltInSound(int track){
    if(loadingPlugin||track<0||track>=trackCount)return;checkpoint();closePluginEditor();audio.useInternal(track);
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
    ensureTimelineClip();
    const int index=clipIndex();if(index<0||index>=(int)project.tracks[(size_t)selectedTrack].clips.size())return;
    if(project.tracks[(size_t)selectedTrack].locked||clip().locked){message(tr("This clip is locked: unlock it to regenerate it."),true);return;}
    checkpoint();
    auto seed=(std::uint32_t)juce::Random::getSystemRandom().nextInt();if(!seed)seed=1;
    regenerateClip(project,selectedTrack,index,seed);
    piano.clearSelection();changed();message(tr("Clip regenerated from the instrument settings. Undo restores the previous notes."));
}
void MainComponent::applyInstrumentSettings(int track,bool harmony){
    applyInstrumentChange(project,track,harmony);piano.clearSelection();changed(true,harmony?-1:track);
}
// Plugin states are part of the snapshot, so undoing an instrument change brings back its sound.
void MainComponent::checkpoint(){lastCheckpointControl=nullptr;if(!loadingPlugin)audio.capturePluginStates(project);undoStack.push_back(project);if(undoStack.size()>40)undoStack.pop_front();redoStack.clear();}
// Wheel and keyboard steps on one control within half a second share one undo step.
void MainComponent::checkpoint(const void* control){
    const auto now=juce::Time::getMillisecondCounter();
    if(control!=nullptr&&control==lastCheckpointControl&&now-lastCheckpointTime<500){lastCheckpointTime=now;return;}
    checkpoint();lastCheckpointControl=control;lastCheckpointTime=now;
}
void MainComponent::mixChanged(){dirty=true;audio.updateMix(project);refreshControls();arrangement.repaint();mixer.repaint();repaintHeader();}
// Only the window header (project name, length, page title) is painted by the window itself.
void MainComponent::repaintHeader(){repaint(0,0,getWidth(),193);}
void MainComponent::changed(bool schedule,int onlyTrack){project.normalise();dirty=true;audio.setLoop(project.loop,project.loopStart,project.loopEnd);if(schedule)audio.publish(project,true,onlyTrack);else audio.updateMix(project);refreshControls();arrangement.repaint();piano.repaint();repaintHeader();}
void MainComponent::applyLanguage(){
    if(Localisation::tipsEnabled())tooltips=std::make_unique<juce::TooltipWindow>(this,650);else tooltips.reset();
    refreshControls();if(page==Page::instrument)instrumentPanel.rebuild();
    options.refreshText();pluginSettings.refreshText();pluginBrowser.refreshText();updateOverlay.refreshText();translateControls(*this);repaint();
}
void MainComponent::showArrangement(){page=Page::arrangement;zoomSlider.setValue(arrangement.pixelsPerBar,juce::dontSendNotification);resized();select(selectedTrack,selectedSection);repaint();}
void MainComponent::showInstrument(){page=Page::instrument;instrumentPanel.rebuild();panelSections.clear(juce::dontSendNotification);for(auto name:instrumentPanel.sectionNames())panelSections.addItem(name,panelSections.getNumItems()+1);panelSections.setTextWhenNothingSelected("ADVANCED");translateControls(panelSections);resized();instrumentPort.setViewPosition(0,0);repaint();}
void MainComponent::showEditor(){ensureTimelineClip();page=Page::editor;zoomSlider.setValue(piano.pixelsPerBeat,juce::dontSendNotification);resized();select(selectedTrack,selectedSection);piano.grabKeyboardFocus();repaint();}
void MainComponent::select(int track,int section){const int previousTrack=selectedTrack,previousSection=selectedSection;selectedTrack=std::clamp(track,0,16);selectedSection=std::clamp(section,0,(int)project.sections.size()-1);if(previousTrack!=selectedTrack||previousSection!=selectedSection){noClipSelection=false;selectedClip=project.clipAtBeat(selectedTrack,project.sectionStart(selectedSection));selectionBeat=project.sectionStart(selectedSection)+1./1920.;arrangement.selectedClips.clear();}piano.clearSelection();sectionSelector.setSelectedId(selectedSection+1,juce::dontSendNotification);refreshTrack();updateSizes();piano.fitNotes();pianoPort.setViewPosition(0,0);mixer.reveal(selectedTrack);
    if(page==Page::arrangement){const auto v=arrangementPort.getViewArea();const int y=arrangement.rowY(rowForTrack(selectedTrack));if(y<v.getY()+rulerHeight)arrangementPort.setViewPosition(v.getX(),std::max(0,y-rulerHeight));else if(y+arrangement.rowHeight(rowForTrack(selectedTrack))>v.getBottom())arrangementPort.setViewPosition(v.getX(),y+arrangement.rowHeight(rowForTrack(selectedTrack))-v.getHeight());}
    sampleBrowser.refreshTarget();arrangement.repaint();piano.repaint();}
void MainComponent::refreshControls(){
    refreshing=true;auto& s=project.settings;const auto& tempo=genreProfile(s.genre);tempoSlider.setRange(tempo.minBpm,tempo.maxBpm,.1);durationLabel.setText(juce::String((int)(project.length()*60/s.bpm)/60)+":"+juce::String((int)(project.length()*60/s.bpm)%60).paddedLeft('0',2),juce::dontSendNotification);leadButton.setToggleState(s.guitarLead,juce::dontSendNotification);
    // The section list is rebuilt (and translated) only when the sections change.
    {juce::StringArray names;for(const auto& section:project.sections)names.add(utf(section.name));if(names!=sectionSelectorNames){sectionSelectorNames=names;sectionSelector.clear(juce::dontSendNotification);for(int i=0;i<names.size();++i)sectionSelector.addItem(names[i],i+1);translateControls(sectionSelector);}}
    sectionSelector.setSelectedId(selectedSection+1,juce::dontSendNotification);genreBox.setSelectedId(s.genre+1,juce::dontSendNotification);keyBox.setSelectedId(s.key+1,juce::dontSendNotification);scaleBox.setSelectedId(s.scale+1,juce::dontSendNotification);extensionBox.setSelectedId(s.extension+1,juce::dontSendNotification);voicingBox.setSelectedId(s.voicing+1,juce::dontSendNotification);bassBox.setSelectedId(s.bassMode+1,juce::dontSendNotification);stringsBox.setSelectedId(s.stringsMode+1,juce::dontSendNotification);
    tempoSlider.setValue(s.bpm,juce::dontSendNotification);swingSlider.setValue(s.swing*100,juce::dontSendNotification);densitySlider.setValue(s.density*100,juce::dontSendNotification);humanizeSlider.setValue(s.humanize*100,juce::dontSendNotification);masterSlider.setValue(juce::Decibels::gainToDecibels(project.masterGain,-60.),juce::dontSendNotification);loopButton.setToggleState(project.loop,juce::dontSendNotification);
    juce::StringArray chords;for(auto name:chordNames(s))chords.add(utf(name));chordsLabel.setText(tr("HARMONY")+"   "+chords.joinIntoString("    /    "),juce::dontSendNotification);
    undoButton.setEnabled(!undoStack.empty());redoButton.setEnabled(!redoStack.empty());refreshing=false;refreshTrack();if(page==Page::instrument)instrumentPanel.refreshFromProject();updateSizes();
}
void MainComponent::refreshTrack(){
    sourceNameCached.fill(false);
    if(!noClipSelection){if(project.tracks[(size_t)selectedTrack].freeTimeline)selectedClip=project.clipAtBeat(selectedTrack,selectionBeat);else selectionBeat=project.sectionStart(selectedSection)+1./1920.;}
    if(project.tracks[(size_t)selectedTrack].freeTimeline&&(selectedClip<0||selectedClip>=(int)project.tracks[(size_t)selectedTrack].clips.size())){selectedClip=-1;emptyClip=Clip{};}
    refreshing=true;const auto& t=project.tracks[(size_t)selectedTrack];gainSlider.setValue(juce::Decibels::gainToDecibels(t.gain,-60.),juce::dontSendNotification);panSlider.setValue(t.pan*100,juce::dontSendNotification);muteButton.setToggleState(t.mute,juce::dontSendNotification);soloButton.setToggleState(t.solo,juce::dontSendNotification);lockButton.setToggleState(t.locked,juce::dontSendNotification);lockClipButton.setToggleState(clip().locked,juce::dontSendNotification);
    const int count=builtInSoundCount(selectedTrack);if(soundBoxTrack!=selectedTrack){soundBoxTrack=selectedTrack;soundBox.clear(juce::dontSendNotification);for(int i=0;i<count;++i)soundBox.addItem(builtInSoundName(selectedTrack,i),i+1);translateControls(soundBox);}soundBox.setSelectedId(std::min(t.sound+1,count),juce::dontSendNotification);soundBox.setEnabled(t.instrument=="internal"&&selectedTrack!=2);
    inspectorLabel.setText(utf(t.name).toUpperCase()+juce::String::fromUTF8("   ·   ")+sourceWithEffect(selectedTrack),juce::dontSendNotification);sectionLoopButton.setToggleState(clipLoopActive(),juce::dontSendNotification);editorLabel.setText(tr(selectedTrack>=4?"DRUM EDITOR":selectedTrack==3?"GUITAR EDITOR":"PIANO ROLL")+"   /   "+utf(t.name)+"   /   "+utf(project.sections[(size_t)selectedSection].name)+"   /   "+juce::String((int)clip().notes.size())+" notes",juce::dontSendNotification);pluginEditorButton.setEnabled(!loadingPlugin&&(audio.plugin(selectedTrack)!=nullptr||audio.effectPlugin(selectedTrack)!=nullptr));sampleButton.setEnabled(!loadingPlugin);refreshing=false;trackInspector.refresh();mixer.refresh();
}
void MainComponent::updateSizes(){automationButton.setToggleState(arrangement.volumeVisible[(size_t)selectedTrack],juce::dontSendNotification);arrangement.setSize(std::max(arrangementPort.getWidth()-16,headerWidth+(int)((project.length()+32)/4*arrangement.pixelsPerBar)+20),arrangement.totalHeight());piano.setSize(piano.leftMargin()+(int)(clipLength()*piano.pixelsPerBeat)+20,std::max(selectedTrack==3&&!piano.guitarNotes?640:320,pianoPort.getHeight()-18));}
void MainComponent::undo(bool redo){if(loadingPlugin)return;instrumentPanel.cancelRender();auto& source=redo?redoStack:undoStack;auto& dest=redo?undoStack:redoStack;if(source.empty())return;audio.capturePluginStates(project);dest.push_back(project);const auto& before=dest.back();project=std::move(source.back());source.pop_back();lastCheckpointControl=nullptr;project.normalise();selectedSection=std::min(selectedSection,(int)project.sections.size()-1);selectedClip=project.clipAtBeat(selectedTrack,audio.position());selectionBeat=audio.position();noClipSelection=false;arrangement.selectedClips.clear();
    bool instrumentsChanged=false;for(size_t i=0;i<project.tracks.size();++i)if(project.tracks[i].instrument!=before.tracks[i].instrument||project.tracks[i].instrumentPath!=before.tracks[i].instrumentPath||project.tracks[i].instrumentId!=before.tracks[i].instrumentId||project.tracks[i].sampleRoot!=before.tracks[i].sampleRoot||project.tracks[i].effectPath!=before.tracks[i].effectPath||project.tracks[i].effectId!=before.tracks[i].effectId)instrumentsChanged=true;
    // Same instruments: bring each plugin back to the sound it had in that snapshot.
    if(instrumentsChanged)restoreInstruments();else for(size_t i=0;i<project.tracks.size();++i){const auto& t=project.tracks[i];if(t.instrument=="plugin"&&!t.instrumentState.empty()&&t.instrumentState!=before.tracks[i].instrumentState)audio.restorePluginState((int)i,juce::String::fromUTF8(t.instrumentState.c_str()));if(!t.effectState.empty()&&t.effectState!=before.tracks[i].effectState)audio.restoreEffectState((int)i,juce::String::fromUTF8(t.effectState.c_str()));}
    piano.clearSelection();changed();if(page==Page::instrument)instrumentPanel.rebuild();}
void MainComponent::regenerate(Scope scope,bool newTake){
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
    arrangement.selectedClips.clear();piano.clearSelection();changed();
    if(newTake&&scope==Scope::song&&page==Page::instrument) {
        juce::Component::SafePointer<MainComponent> safe(this);
        juce::MessageManager::callAsync([safe]{if(safe&&safe->page==Page::instrument)safe->instrumentPanel.rebuild();});
    }
    if(newTake&&scope==Scope::song)
        message(tr(project.settings.key!=oldKey?"New key, tempo, form and patterns ready. Undo restores the previous song.":"New patterns ready. Locked harmonic parts keep the key and progression. Unlock them for a new harmony."));
    else message("New part ready. Locks were kept. Undo restores the previous notes.");
}
void MainComponent::modifySelected(std::function<void(Clip&)> action){checkpoint();if(piano.selected.empty())action(clip());else{Clip part;for(int i:piano.selected)if(i>=0&&i<(int)clip().notes.size())part.notes.push_back(clip().notes[(size_t)i]);action(part);size_t k=0;for(int i:piano.selected)if(i>=0&&i<(int)clip().notes.size())clip().notes[(size_t)i]=part.notes[k++];}tidyNotes(clip(),clipLength());piano.clearSelection();changed();}
void MainComponent::paint(juce::Graphics& g){
    g.fillAll(Palette::background);Branding::drawFullLogo(g,{20,18,238,46});
    label(g,tr("by")+" Fabio 'Pitch' Serra",{78,74,200,14},11,Palette::dim);
    auto panel=[&](juce::Rectangle<int> r){g.setColour(Palette::panel);g.fillRoundedRectangle(r.toFloat(),8);g.setColour(Palette::panelEdge);g.drawRoundedRectangle(r.toFloat().reduced(.5f),8,1);};
    panel({624,18,390,68});panel({1024,18,getWidth()-1044,68});
    g.setColour(Palette::accent);g.fillRoundedRectangle(636,27,3,12,1);
    label(g,"TRANSPORT",{645,23,82,20},11.5f);
    const double seconds=project.length()*60/project.settings.bpm;
    label(g,utf(project.name)+(dirty?" *":"")+" / "+juce::String((int)seconds/60)+":"+juce::String((int)seconds%60).paddedLeft('0',2),{1036,23,getWidth()-1060,20},13);
    for(int y:{110,192,getHeight()-31}){g.setColour(Palette::panelEdge);g.fillRect(20,y,getWidth()-40,1);}
    if(page!=Page::arrangement)label(g,tr(page==Page::instrument?"INSTRUMENT":"MIDI EDITOR")+" / "+utf(project.tracks[(size_t)selectedTrack].name).toUpperCase(),{196,122,400,28},18,colour(selectedTrack));
}
void MainComponent::resized(){
    const int w=getWidth();auto place=[](juce::Component& c,int x,int y,int width,int height=28){c.setBounds(x,y,width,height);};
    const bool song=page==Page::arrangement,engine=page==Page::instrument,edit=page==Page::editor;
    arrangementPort.setVisible(song);instrumentPort.setVisible(engine);pianoPort.setVisible(edit);inspectorPort.setVisible(song);inspectorCollapse.setVisible(song);inspectorResizer.setVisible(song&&inspectorVisible);mixer.setVisible(song);
    for(auto* button:{&inspectorToggle,&mixerToggle,&fitSongButton,&followButton})button->setVisible(song);
    for(juce::Component* c:std::initializer_list<juce::Component*>{&genreBox,&songButton,&trackButton,&sectionButton,&clipButton,&densitySlider,&swingSlider,&humanizeSlider,&progressionBox,&harmonyButton,&extensionBox,&voicingBox,&bassBox,&stringsBox,&addSectionButton,&durationLabel,&arrangeButton,&chordEditButton,&browserToggle})c->setVisible(song);
    zoomSlider.setVisible(edit); // the arrangement zooms with +/-, the wheel, FIT SONG and ZOOM SELECTION
    sampleBrowser.setVisible(song&&browserVisible);browserResizer.setVisible(song&&browserVisible);
    automationButton.setVisible(song);
    bassBox.setVisible(false);stringsBox.setVisible(false); // instrument-owned styles live in their panels
    for(juce::Component* c:std::initializer_list<juce::Component*>{&soundButton,&soundBox,&sampleButton,&loadPluginButton,&pluginEditorButton,&gainSlider,&panSlider,&muteButton,&soloButton,&lockButton,&lockClipButton,&inspectorLabel})c->setVisible(engine);
    for(juce::Component* c:std::initializer_list<juce::Component*>{&editorLabel,&sectionLoopButton,&importButton,&copyButton,&pasteButton,&quantiseButton,&transposeDown,&transposeUp,&deleteNotes,&gridBox,&velocitySlider})c->setVisible(edit);
    for(juce::Component* button:std::initializer_list<juce::Component*>{&drawTool,&selectTool,&eraseTool,&regenerateClipButton})button->setVisible(edit);
    for(auto* button:{&brushAccent,&brushHit,&brushGhost,&brushFlam})button->setVisible(edit&&piano.laneView());fitNotesButton.setVisible(edit&&!piano.laneView());
    if(edit&&selectedTrack>=4){transposeDown.setVisible(false);transposeUp.setVisible(false);}
    backButton.setVisible(!song);instrumentButton.setVisible(edit);editButton.setVisible(engine);sectionSelector.setVisible(!song);leadButton.setVisible(song||(edit&&selectedTrack==3));guitarNotesButton.setVisible(edit&&selectedTrack==3);panelSections.setVisible(engine);
    // Same two utility rows to the right of the logo as the family editors.
    // File controls replace plugin presets; transport and global harmony stay shared.
    int x=282;
    for(const auto& item:std::initializer_list<std::pair<juce::Component*,int>>{{&newButton,46},{&openButton,54},{&saveButton,54},{&saveAsButton,70},{&undoButton,30},{&redoButton,30}}){place(*item.first,x,22,item.second,26);x+=item.second+4;}
    undoButton.setConnectedEdges(juce::Button::ConnectedOnRight);redoButton.setConnectedEdges(juce::Button::ConnectedOnLeft);
    x=282;for(const auto& item:std::initializer_list<std::pair<juce::Component*,int>>{{&exportButton,74},{&settingsButton,70},{&aboutButton,100},{&helpButton,70}}){place(*item.first,x,60,item.second,26);x+=item.second+4;}
    // Transport: position counter, loop and click on the title row; buttons and tempo below.
    place(positionLabel,730,22,122,22);place(loopButton,858,22,70,22);place(metronomeButton,932,22,74,22);
    place(rewindButton,636,48,34,30);place(playButton,670,48,40,30);place(stopButton,710,48,34,30);place(tempoSlider,756,48,250,30);
    place(keyBox,1036,48,64,30);place(scaleBox,1108,48,154,30);place(masterSlider,1270,48,w-1310,30);
    for(auto* button:{&zoomSelectionButton,&loopSelectionButton})button->setVisible(song);timelineGrid.setVisible(song);
    if(song){
        x=16;place(genreBox,x,118,178);x+=188;place(durationLabel,x,118,82);x+=92;place(arrangeButton,x,118,170);x+=180;place(songButton,x,118,154);x+=164;place(trackButton,x,118,140);x+=148;place(sectionButton,x,118,154);x+=162;place(clipButton,x,118,140);x+=148;place(addSectionButton,x,118,114);
        // Harmony row: progression, chords and the global feel controls.
        x=16;place(progressionBox,x,159,210);x+=218;place(harmonyButton,x,159,118);x+=126;place(chordEditButton,x,159,118);x+=126;place(extensionBox,x,159,116);x+=124;place(voicingBox,x,159,116);x+=124;place(densitySlider,x,159,150);x+=158;place(swingSlider,x,159,150);x+=158;place(humanizeSlider,x,159,150);
        // Arrangement toolbar: editing and view on the left, panels on the right.
        x=16;for(const auto& item:std::initializer_list<std::pair<juce::Component*,int>>{{&timelineGrid,150},{&zoomSelectionButton,140},{&loopSelectionButton,140},{&fitSongButton,100},{&followButton,90},{&automationButton,140}}){place(*item.first,x,199,item.second,26);x+=item.second+8;}
        x=w-16;for(const auto& item:std::initializer_list<std::pair<juce::Component*,int>>{{&mixerToggle,90},{&browserToggle,100},{&inspectorToggle,110},{&leadButton,170}}){x-=item.second;place(*item.first,x,199,item.second,26);x-=8;}
        const int dockHeight=mixerVisible?std::clamp(mixerHeight,160,std::max(160,getHeight()-476)):28;
        const int footer=getHeight()-31-dockHeight-38;
        inspectorWidth=std::clamp(inspectorWidth,184,std::max(184,std::min(480,w/3)));
        const int left=inspectorVisible?inspectorWidth+24:56;
        // The browser takes the full height on the right; arrangement, harmony line and mixer share the rest.
        const int browserWidth=browserVisible?(sampleBrowserWidth>0?clampBrowserWidth(sampleBrowserWidth):std::clamp(w/4,300,380)):0,right=w-12-(browserVisible?browserWidth+8:0);
        sampleBrowser.setBounds(w-12-browserWidth,238,browserWidth,std::max(120,getHeight()-35-238));
        browserResizer.setBounds(w-12-browserWidth-8,238,8,std::max(120,getHeight()-35-238));browserResizer.toFront(false);
        arrangementPort.setBounds(left,238,right-left,std::max(60,footer-246));
        inspectorPort.setScrollBarsShown(inspectorVisible,false);
        inspectorPort.setBounds(12,238,inspectorVisible?inspectorWidth:32,std::max(60,footer-238));trackInspector.setSize(inspectorVisible?inspectorWidth-16:32,inspectorVisible?trackInspector.preferredHeight():inspectorPort.getHeight());
        inspectorCollapse.setBounds(inspectorVisible?inspectorWidth-32:16,242,24,22);inspectorCollapse.setButtonText(inspectorVisible?"<":">");inspectorCollapse.toFront(false);
        inspectorResizer.setBounds(12+inspectorWidth,238,8,std::max(60,footer-238));inspectorResizer.toFront(false);
        mixer.setBounds(12,footer+36,right-12,dockHeight);
        place(chordsLabel,18,footer,right-24,28);
    }else{
        sampleBrowser.setVisible(false);chordEditButton.setVisible(true);place(chordEditButton,1118,122,114);
        place(backButton,16,122,160);place(sectionSelector,600,122,230);place(editButton,w-170,122,150);place(instrumentButton,w-200,122,180);place(leadButton,850,122,260);place(panelSections,850,122,260);
        if(engine){
            inspectorLabel.setVisible(false);soundButton.setVisible(true);sampleButton.setVisible(true);soundBox.setVisible(false);
            x=18;for(const auto& item:std::initializer_list<std::pair<juce::Component*,int>>{{&soundButton,80},{&sampleButton,110},{&loadPluginButton,130},{&pluginEditorButton,96},{&gainSlider,118},{&panSlider,108},{&muteButton,50},{&soloButton,50},{&lockButton,112},{&lockClipButton,112},{&trackButton,114},{&clipButton,114}}){place(*item.first,x,162,item.second);x+=item.second+6;}
            trackButton.setVisible(true);trackButton.setButtonText("APPLY TRACK");clipButton.setVisible(true);clipButton.setButtonText("APPLY CLIP");
            // Keep the plugins' 1180px design width. Extra DAW width must not
            // stretch only the Groove card and alter the family's proportions.
            const int panelWidth=std::min(1180,w-40);
            instrumentPort.setBounds((w-panelWidth-16)/2,204,panelWidth+16,getHeight()-243);
            instrumentPanel.setSize(panelWidth,instrumentPanel.getHeight());
        }else{
            place(editorLabel,20,159,640,28);place(guitarNotesButton,w-340,159,180);place(sectionLoopButton,w-150,159,132);
            x=18;for(auto* b:{&importButton,&copyButton,&pasteButton,&quantiseButton,&transposeDown,&transposeUp,&deleteNotes}){int width=b==&importButton?115:b==&quantiseButton?103:64;place(*b,x,203,width,28);x+=width+6;}place(gridBox,x+3,203,88);place(velocitySlider,x+101,203,154);place(zoomSlider,x+265,203,160);zoomSlider.setVisible(true);place(drawTool,1034,203,36);place(selectTool,1070,203,36);place(eraseTool,1106,203,36);place(regenerateClipButton,1152,203,138);place(brushAccent,700,159,80);place(brushHit,780,159,70);place(brushGhost,850,159,80);place(brushFlam,930,159,74);place(fitNotesButton,w-110,203,88);pianoPort.setBounds(12,246,w-24,getHeight()-285);
        }
    }
    chordsLabel.setVisible(song);
    if(song){trackButton.setButtonText("REGENERATE TRACK");sectionButton.setButtonText("REGENERATE SECTION");clipButton.setButtonText("REGENERATE CLIP");}
    place(statusLabel,18,getHeight()-29,w-36,25);options.setBounds(getLocalBounds());if(options.isVisible())options.toFront(false);pluginSettings.setBounds(getLocalBounds());if(pluginSettings.isVisible())pluginSettings.toFront(false);pluginBrowser.setBounds(getLocalBounds());if(pluginBrowser.isVisible())pluginBrowser.toFront(false);updateOverlay.setBounds(getLocalBounds());if(updateOverlay.isVisible())updateOverlay.toFront(false);updateSizes();for(auto* b:{&trackButton,&sectionButton,&clipButton,&inspectorCollapse})translateControls(*b);
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
    juce::String fail;
    auto mark=[&](const char* step,bool value){if(!value&&fail.isEmpty())fail=step;return value;};
    bool ok=mark("language",writeLanguageEvidence(folder)&&writeHelpEvidence(folder));pluginSettings.show();ok=mark("plugin-settings",pluginSettings.verifyLayout())&&ok;pluginSettings.setVisible(false);auto capture=[&](const juce::String& name){auto screenshot=createComponentSnapshot(getLocalBounds());bool wrote=false;if(auto out=folder.getChildFile(name+".png").createOutputStream()){juce::PNGImageFormat png;wrote=png.writeImageToStream(screenshot,*out);}ok=mark(name=="plugin-browser"?"png-browser":name=="plugin-settings"?"png-settings":"png",wrote)&&ok;};
    // Context-dependent panels must not expose controls ignored by the renderer.
    {
        const auto savedProject=project;
        auto expect=[&](const char* step,bool value,const juce::StringArray& names){if(!value&&fail.isEmpty())fail=juce::String(step)+": "+names.joinIntoString(" | ");ok=value&&ok;};
        select(1,1);project.settings.bassMode=sgk::bassModeForLine(8);showInstrument();
        auto names=instrumentPanel.controlNames();expect("bass-line",!names.contains("RND BASS")&&!names.contains("GROOVE")&&!names.contains("A/B")&&names.contains("BASS LINE"),names);
        project.settings.bassMode=(int)sgk::BassMode::root;showInstrument();names=instrumentPanel.controlNames();expect("bass-root",names.contains("RND BASS")&&names.contains("GROOVE"),names);
        select(2,1);project.settings.stringsMode=(int)sgk::StringsMode::pad;showInstrument();names=instrumentPanel.controlNames();expect("strings-pad",!names.contains("RND PIANO")&&!names.contains("NEW FEEL")&&names.contains("STRINGS STYLE"),names);
        project.settings.stringsMode=(int)sgk::StringsMode::stabs;showInstrument();names=instrumentPanel.controlNames();expect("strings-stabs",names.contains("RND PIANO")&&names.contains("GROOVE"),names);
        select(4,1);showInstrument();names=instrumentPanel.controlNames();expect("kick",!names.contains("LEVEL")&&!names.contains("Drum volume")&&!names.contains("Mute")&&names.contains("MIDI NOTE")&&names.contains("VELOCITY")&&!names.contains("Crash after fill")&&names.contains("Bass space"),names);
        select(5,1);showInstrument();names=instrumentPanel.controlNames();expect("snare",!names.contains("Bass space")&&!names.contains("Crash after fill"),names);
        select(16,1);showInstrument();names=instrumentPanel.controlNames();expect("crash",names.contains("Crash after fill")&&!names.contains("Bass space"),names);
        select(4,1);
        // Track gain stays on the arranger strip. A sample hides the guide tone and reverb.
        project.tracks[4].instrument="sample";showInstrument();names=instrumentPanel.controlNames();expect("sample",!names.contains("TONE")&&!names.contains("REVERB")&&!names.contains("VOLUME"),names);
        project=savedProject;changed();
    }
    pluginSettings.show();capture("plugin-settings");pluginSettings.setVisible(false);
    pluginBrowser.show(0,juce::String("fg")+"\x1f"+"0");ok=mark("plugin-browser",pluginBrowser.verifyLayout())&&ok;capture("plugin-browser");pluginBrowser.setVisible(false);
    select(0,1);showInstrument();capture("piano-family-extended");const auto take=clip().seed;regenerate(Scope::clip);ok=mark("regenerate",clip().seed!=take&&clip().motifSeed!=0)&&ok;const auto generated=clip();if(clipButton.onClick)clipButton.onClick();ok=mark("apply-clip",clip()==generated)&&ok;
    showArrangement();fitSong();ok=mark("fit-song",arrangementPort.isVisible()&&!pianoPort.isVisible()&&headerWidth+project.length()/4*arrangement.pixelsPerBar<arrangementPort.getWidth())&&ok;capture("interface");
    ok=mark("automation",arrangement.verifyAutomation(folder))&&ok;
    capture("daw-workspace");ok=mark("daw-workspace",verifyDawWorkspace(folder))&&ok;ok=mark("timeline",verifyTimelineEditing(folder))&&ok;ok=mark("navigation",verifyNavigation(folder))&&ok;ok=mark("sample-browser",verifySampleBrowser(folder))&&ok;
    juce::String layoutReport,editingReport;
    for(int track:{4,1,0,2,3}){select(track,1);showInstrument();auto problems=instrumentPanel.layoutProblems();layoutReport+=juce::String(track)+": "+(problems.isEmpty()?"PASS":problems)+"\n";ok=mark("panel",problems.isEmpty())&&ok;ok=mark("instrument-page",instrumentPort.isVisible()&&!pianoPort.isVisible()&&sectionSelector.getSelectedId()==selectedSection+1)&&ok;capture("instrument-"+juce::String(track));if(track==3){instrumentPort.setViewPosition(0,instrumentPanel.getHeight());capture("guitar-mixer");instrumentPort.setViewPosition(0,0);}showEditor();ok=mark("editor-page",pianoPort.getHeight()>400&&!arrangementPort.isVisible())&&ok;if(track>=4)ok=mark("drum-lanes",piano.laneView())&&ok;if(track==1||track==2)ok=mark("pitched-lanes",!piano.laneView())&&ok;capture("editor-"+juce::String(track));const bool edits=piano.verifyEditing();ok=mark("editing",edits)&&ok;editingReport+=juce::String(track)+": "+(edits?"PASS add, move, velocity, resize where pitched, delete, precise ruler seek":"FAIL editing gestures")+"\n";}
    folder.getChildFile("panel-layout.txt").replaceWithText(layoutReport);folder.getChildFile("midi-editing.txt").replaceWithText(editingReport);
    select(3,1);piano.guitarNotes=true;showEditor();ok=mark("guitar-lanes",!piano.laneView())&&ok;capture("guitar-notes");ok=mark("guitar-notes",piano.verifyEditing())&&ok;piano.guitarNotes=false;showArrangement();select(4,0);
    folder.getChildFile("ui.txt").replaceWithText(ok?"PASS separate arrangement, instrument panels, full MIDI editors, drum isolation, guitar lanes/notes, instrument regeneration creates a new take":"FAIL interface routing: "+fail);return ok;
}
void MainComponent::timerCallback(){
    const double beat=timelinePosition();positionLabel.setText(juce::String((int)beat/4+1).paddedLeft('0',3)+" : "+juce::String((int)beat%4+1)+"   /   "+juce::String((int)project.length()/4),juce::dontSendNotification);playButton.setIcon(audio.isPlaying()?IconButton::Icon::pause:IconButton::Icon::play);mixer.tick();
    if(page==Page::arrangement&&followPlayhead&&audio.isPlaying()){const auto v=arrangementPort.getViewArea();const int x=headerWidth+(int)(beat/4*arrangement.pixelsPerBar);if(x>v.getRight()-40||x<v.getX()+headerWidth)arrangementPort.setViewPosition(std::max(0,x-headerWidth-60),v.getY());}
    // Repaint the timeline only while something moves (playback, meters falling, cursor jumps):
    // an idle arranger uses no CPU for drawing.
    const bool mouseDown=juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown();
    const bool moving=audio.isPlaying()||beat!=lastTimerBeat||audio.masterLevel()>0||mouseDown;lastTimerBeat=beat;
    if(moving){
        // Playing hands-off, only the playhead strip and the track headers (meters) move.
        const int playX=headerWidth+(int)(beat/4*arrangement.pixelsPerBar);
        if(audio.isPlaying()&&!mouseDown&&page==Page::arrangement){const auto v=arrangementPort.getViewArea();arrangement.repaint(v.getX(),v.getY(),headerWidth,v.getHeight());for(int x:{lastPlayX,playX})if(x>=0)arrangement.repaint(x-7,v.getY(),16,v.getHeight());}
        else arrangement.repaint();
        lastPlayX=playX;
        if(page==Page::editor)piano.repaint();if(page==Page::instrument)instrumentPanel.repaintMeters();}audio.collectGarbage();
    if(++ticks%1200==0&&dirty&&!loadingPlugin){audio.capturePluginStates(project);auto path=juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("FS Pitch/Fase Groove Arranger/autosave.fga");path.getParentDirectory().createDirectory();auto result=saveProject(project,path);if(result.failed())message(result.getErrorMessage(),true);}
}
void MainComponent::message(const juce::String& text,bool error){statusLabel.setColour(juce::Label::textColourId,error?Palette::toms:Palette::dim);statusLabel.getProperties().set("labelSource",text);statusLabel.getProperties().set("labelLast",tr(text));statusLabel.setText(tr(text),juce::dontSendNotification);}
bool MainComponent::keyPressed(const juce::KeyPress& k){
    if(pluginSettings.isVisible()){if(k==juce::KeyPress::escapeKey)pluginSettings.setVisible(false);return true;}
    if(pluginBrowser.isVisible()){if(k==juce::KeyPress::escapeKey)pluginBrowser.setVisible(false);return true;}
    if(k==juce::KeyPress::escapeKey){if(updateOverlay.isVisible()){updateOverlay.setVisible(false);return true;}if(options.isVisible()){options.setVisible(false);return true;}showArrangement();return true;}
    if(k==juce::KeyPress::spaceKey){togglePlayback();return true;}
    if(k==juce::KeyPress::homeKey){seekTimeline(0);return true;}
    if(k.getKeyCode()==juce::KeyPress::F3Key){if(page!=Page::arrangement){mixerVisible=true;mixerToggle.setToggleState(true,juce::dontSendNotification);showArrangement();}else mixerToggle.triggerClick();return true;}
    const int code=k.getKeyCode();
    if(k.getModifiers().isCommandDown()){
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
        if(page==Page::arrangement&&code=='A'){arrangement.selectedClips.clear();for(int i=0;i<(int)project.tracks[(size_t)selectedTrack].clips.size();++i)arrangement.selectedClips.insert(i);arrangement.repaint();return true;}
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
        if(code=='H'&&(page==Page::arrangement||page==Page::editor)){checkpoint();humanize(clip(),.65,(std::uint32_t)juce::Random::getSystemRandom().nextInt(),clipLength());tidyNotes(clip(),clipLength());piano.clearSelection();changed();return true;}
        if(page==Page::arrangement&&(k==juce::KeyPress::deleteKey||k==juce::KeyPress::backspaceKey)){timelineCommand(6);return true;}
    }
    return false;
}

// Save / Discard / Cancel, as in any DAW. An untitled project opens SAVE AS and keeps the
// current song open: close again once it is saved.
bool MainComponent::requestClose(){if(!dirty)return true;
    const int choice=juce::AlertWindow::showYesNoCancelBox(juce::MessageBoxIconType::QuestionIcon,tr("Unsaved changes"),tr("The project has unsaved changes. Save them before closing?"),tr("SAVE"),tr("Discard"),tr("Cancel"));
    if(choice==2)return true;
    if(choice!=1)return false;
    if(loadingPlugin||projectFile==juce::File()){save(true);return false;}
    audio.capturePluginStates(project);const auto r=saveProject(project,projectFile);
    if(r.failed()){message(r.getErrorMessage(),true);return false;}
    dirty=false;return true;}
void MainComponent::chooseFile(const juce::String& title,const juce::String& filter,int flags,std::function<void(juce::File)> callback){chooser=std::make_unique<juce::FileChooser>(tr(title),projectFile.existsAsFile()?projectFile.getParentDirectory():juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),filter);juce::Component::SafePointer<MainComponent> safe(this);chooser->launchAsync(flags,[safe,callback](const juce::FileChooser& fc){auto f=fc.getResult();if(safe!=nullptr&&f!=juce::File())callback(f);});}
void MainComponent::save(bool as){if(loadingPlugin){message("Attendi il caricamento dello strumento.");return;}audio.capturePluginStates(project);if(as||projectFile==juce::File()){chooseFile("Salva progetto","*.fga",juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[this](juce::File file){const auto target=file.withFileExtension("fga");auto next=project;next.name=target.getFileNameWithoutExtension().toStdString();auto r=saveProject(next,target);if(r.wasOk()){projectFile=target;project.name=next.name;dirty=false;message("Progetto salvato: "+projectFile.getFullPathName());repaint();}else message(r.getErrorMessage(),true);});}else{auto r=saveProject(project,projectFile);if(r.wasOk()){dirty=false;message("Progetto salvato.");repaint();}else message(r.getErrorMessage(),true);}}
void MainComponent::open(const juce::File& file){if(loadingPlugin){message("Attendi il caricamento dei plugin.");return;}if(!requestClose())return;Project next;auto r=loadProject(file,next);if(r.failed()){message(r.getErrorMessage(),true);return;}audio.stop();undoStack.clear();redoStack.clear();project=std::move(next);editCursor=-1;projectFile=file;selectedTrack=selectedSection=0;dirty=false;piano.clearSelection();audio.publish(project);refreshControls();restoreInstruments();select(4,0);showArrangement();message("Progetto aperto. I file audio e i plugin restano esterni al progetto.");repaint();}
void MainComponent::restoreInstruments(){pluginWindow.reset();for(int i=0;i<trackCount;++i){audio.useInternal(i);audio.clearEffect(i);auto t=project.tracks[(size_t)i];if(t.instrument=="sample"){auto r=audio.loadSample(i,juce::File(utf(t.instrumentPath)),t.sampleRoot);if(r.failed()){audio.markMissing(i);message("Campione mancante su "+utf(t.name)+": ricollegalo con LOAD SAMPLE.",true);}}else if(t.instrument=="plugin")loadPluginFile(i,juce::File::isAbsolutePath(utf(t.instrumentPath))?juce::File(utf(t.instrumentPath)):juce::File(),utf(t.instrumentId),utf(t.instrumentState));if(!t.effectPath.empty()||!t.effectId.empty())loadEffectFile(i,juce::File::isAbsolutePath(utf(t.effectPath))?juce::File(utf(t.effectPath)):juce::File(),utf(t.effectId),utf(t.effectState));}audio.publish(project);}
void MainComponent::loadSampleFile(int track,const juce::File& file){pluginWindow.reset();checkpoint();auto& t=project.tracks[(size_t)track];int root=track>=4?(t.engine.midiNote>=0?t.engine.midiNote:fgd::defaultNote(track-4)):60;auto r=audio.loadSample(track,file,root);if(r.failed()){message(r.getErrorMessage(),true);return;}t.instrument="sample";t.instrumentPath=file.getFullPathName().toStdString();t.sampleRoot=root;t.instrumentId.clear();t.instrumentState.clear();changed(false);sampleBrowser.refreshTarget();
    message(tr("Sample loaded")+": "+file.getFileName()+juce::String::fromUTF8(" → ")+Localisation::upper(utf(t.name))+juce::String::fromUTF8("  ·  ")+tr("root note")+" "+juce::MidiMessage::getMidiNoteName(root,true,true,3)+" ("+tr("right-click the track to change it")+").");}
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
    juce::PluginDescription* selected=nullptr,*effect=nullptr;
    for(auto* d:found){if(d->createIdentifierString()==id)selected=d;if(d->isInstrument&&selected==nullptr)selected=d;if(!d->isInstrument&&effect==nullptr)effect=d;}
    if(selected==nullptr)selected=effect;
    if(!selected){message("Nessuno strumento compatibile trovato. Scegli il bundle VST3.",true);return;}
    loadPluginDescription(track,*selected,state,id.isNotEmpty());
}
void MainComponent::loadEffectFile(int track,const juce::File& file,const juce::String& id,const juce::String& state){
    if(id.isNotEmpty())if(auto xml=juce::XmlDocument::parse(id)){juce::PluginDescription description;if(description.loadFromXml(*xml)){if(description.pluginFormatName=="VST3"&&!juce::File(description.fileOrIdentifier).exists()){message("Plugin mancante: usa LOAD VST3 / AU per ricollegarlo.",true);return;}loadEffectDescription(track,description,state,true);return;}}
    if(!file.exists()){message("Plugin mancante su "+utf(project.tracks[(size_t)track].name)+": usa LOAD VST3 / AU per ricollegarlo.",true);return;}
    juce::OwnedArray<juce::PluginDescription> found;for(int i=0;i<audio.formats.getNumFormats();++i)audio.formats.getFormat(i)->findAllTypesForFile(found,file.getFullPathName());
    juce::PluginDescription* selected=nullptr;for(auto* d:found)if(!d->isInstrument){selected=d;break;}if(selected==nullptr&&!found.isEmpty())selected=found.getFirst();
    if(!selected){message("Nessuno strumento compatibile trovato. Scegli il bundle VST3.",true);return;}
    loadEffectDescription(track,*selected,state,id.isNotEmpty());
}
void MainComponent::loadEffectDescription(int track,const juce::PluginDescription& description,const juce::String& state,bool restoring){
    pluginWindow.reset();if(!restoring)checkpoint();++loadingPlugin;loadPluginButton.setEnabled(false);
    juce::Component::SafePointer<MainComponent> safe(this);
    audio.loadEffect(track,description,[safe,track,description,state,restoring](juce::Result r){
        if(safe==nullptr)return;
        --safe->loadingPlugin;safe->loadPluginButton.setEnabled(safe->loadingPlugin==0);
        if(r.failed()){safe->message(r.getErrorMessage(),true);safe->refreshTrack();safe->arrangement.repaint();return;}
        auto& t=safe->project.tracks[(size_t)track];t.effectPath=description.fileOrIdentifier.toStdString();
        if(auto xml=description.createXml())t.effectId=xml->toString().toStdString();
        t.effectState=state.toStdString();
        if(state.isNotEmpty())safe->audio.restoreEffectState(track,state);
        if(restoring)safe->refreshTrack();else safe->changed(false);
        safe->message(tr("Effect ready:")+" "+description.name);
    });
}
void MainComponent::chooseBrowserRow(const PluginCatalog::BrowserRow& row){
    if(loadingPlugin){message(tr("Wait for the instrument to finish loading."));return;}
    const int track=selectedTrack;auto& t=project.tracks[(size_t)track];
    if(row.kind==PluginCatalog::BrowserRow::Kind::noEffect){checkpoint();closePluginEditor();audio.clearEffect(track);t.effectPath.clear();t.effectId.clear();t.effectState.clear();changed(false);message(tr("Effect removed."));return;}
    if(row.kind==PluginCatalog::BrowserRow::Kind::engine){const int family=t.kind>=4?4:t.kind;if(row.engine!=family){message(tr("This track keeps its own FaseGroove engine."),true);return;}checkpoint();closePluginEditor();audio.useInternal(track);t.instrument="internal";t.instrumentPath.clear();t.instrumentId.clear();t.instrumentState.clear();changed(false);message(tr("Built-in engine restored."));return;}
    const auto plugins=pluginCatalog.state().plugins;if(!juce::isPositiveAndBelow(row.plugin,plugins.size()))return;
    loadPluginDescription(track,plugins.getReference(row.plugin));
}
void MainComponent::loadPluginDescription(int track,const juce::PluginDescription& description,const juce::String& state,bool restoring){
    if(!description.isInstrument){loadEffectDescription(track,description,state,restoring);return;}
    pluginWindow.reset();if(!restoring)checkpoint();++loadingPlugin;loadPluginButton.setEnabled(false);
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
        if(restoring){safe->audio.updateMix(safe->project);safe->refreshTrack();}else safe->changed(false);
        safe->message("Plugin pronto: "+description.name);
    });
}
void MainComponent::showPluginMenu(juce::Component*){
    if(loadingPlugin){message(tr("Wait for the instrument to finish loading."));return;}
    const auto& t=project.tracks[(size_t)selectedTrack];const int family=t.kind>=4?4:t.kind;juce::String key;
    if(t.instrument=="plugin")if(auto xml=juce::XmlDocument::parse(utf(t.instrumentId))){juce::PluginDescription description;if(description.loadFromXml(*xml))key=PluginCatalog::pluginKey(description);}
    if(key.isEmpty())key="fg\x1f"+juce::String(family);
    pluginBrowser.show(family,key);
}
void MainComponent::importFile(const juce::File& f){auto imported=clip();const double duration=clipIndex()<0?4.:clipLength();auto r=importMidi(f,imported,duration);if(r.failed()){message(r.getErrorMessage(),true);return;}if(clipIndex()<0){imported.span=duration;auto edited=project;const int placed=edited.placeClip(selectedTrack,std::move(imported),arrangement.snapBeat(timelinePosition()));if(placed<0)return;checkpoint();project=std::move(edited);selectClip(selectedTrack,placed);}else{checkpoint();clip()=std::move(imported);}piano.clearSelection();changed();message("MIDI importato nel clip selezionato.");}

void MainComponent::showSampleBrowser(bool visible){
    browserVisible=visible;browserToggle.setToggleState(visible,juce::dontSendNotification);Localisation::setSetting("sampleBrowserVisible",visible?"1":"0");
    if(visible&&page!=Page::arrangement)showArrangement();resized();
}
bool MainComponent::isInterestedInDragSource(const SourceDetails& details){return details.description.toString().startsWith(SampleBrowser::dragPrefix);}
void MainComponent::itemDragMove(const SourceDetails& details){
    int track=-1;
    if(page==Page::arrangement&&arrangementPort.getBounds().contains(details.localPosition)){const auto point=arrangement.getLocalPoint(this,details.localPosition);const int row=arrangement.rowAt(point.y);if(row>=0&&point.y>=arrangementPort.getViewArea().getY()+rulerHeight)track=trackAtRow(row);}
    if(track!=arrangement.dropTrack){arrangement.dropTrack=track;arrangement.repaint();}
}
void MainComponent::itemDragExit(const SourceDetails&){if(arrangement.dropTrack>=0){arrangement.dropTrack=-1;arrangement.repaint();}}
void MainComponent::itemDropped(const SourceDetails& details){
    arrangement.dropTrack=-1;arrangement.repaint();
    if(sampleBrowser.isVisible()&&sampleBrowser.getBounds().contains(details.localPosition))return; // dropped back on the browser
    const auto path=details.description.toString().fromFirstOccurrenceOf(SampleBrowser::dragPrefix,false,false);
    filesDropped(juce::StringArray(path),details.localPosition.x,details.localPosition.y);
}
bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files){for(auto f:files)if(juce::File(f).hasFileExtension("fga;mid;midi;wav;aif;aiff;flac;ogg"))return true;return false;}
void MainComponent::filesDropped(const juce::StringArray& files,int x,int y){
    if(loadingPlugin){message("Attendi il caricamento dei plugin.");return;}
    juce::File file;for(const auto& path:files)if(isInterestedInFileDrag(juce::StringArray(path))){file=juce::File(path);break;}if(file==juce::File())return;
    if(file.hasFileExtension("fga")){open(file);return;}
    if(page==Page::arrangement&&arrangementPort.getBounds().contains(x,y)){
        const auto point=arrangement.getLocalPoint(this,juce::Point<int>(x,y));const int row=arrangement.rowAt(point.y);const auto view=arrangementPort.getViewArea();
        if(row>=0&&point.y>=view.getY()+rulerHeight){const int track=trackAtRow(row);if(point.x>=view.getX()+headerWidth){seekTimeline(std::max(0.,(point.x-headerWidth)*4./arrangement.pixelsPerBar));selectClip(track,project.clipAtBeat(track,timelinePosition()));}else select(track,selectedSection);}
    }
    if(file.hasFileExtension("mid;midi"))importFile(file);else loadSampleFile(selectedTrack,file);
}
}
