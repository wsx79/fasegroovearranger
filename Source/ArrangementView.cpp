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
ArrangementView::ArrangementView(MainComponent& m):owner(m){
    setWantsKeyboardFocus(true);trackHeights.fill(trackHeight);
}
void ArrangementView::saveTrackHeights(){
    for(int track=0;track<maxTracks;++track){const auto key="trackHeight"+juce::String(track);Localisation::setSetting(key.toRawUTF8(),trackHeights[(size_t)track]);}
}
int ArrangementView::heightEdgeAt(const juce::MouseEvent& e) const {
    const auto view=owner.arrangementPort.getViewArea();
    if(e.y<view.getY()+rulerHeight||e.x<view.getX()||e.x>=view.getX()+headerWidth)return -1;
    for(int row=0;row<(int)owner.project.tracks.size();++row)if(std::abs(e.y-(rowY(row)+rowHeight(row)))<=4)return trackAtRow(row);
    return -1;
}
void ArrangementView::paint(juce::Graphics& g){
    const auto view=owner.arrangementPort.getViewArea();const auto& p=owner.project;const auto dirtyArea=g.getClipBounds();
    const int lanesLeft=std::max(view.getX()+headerWidth,headerWidth),lanesRight=std::min(getWidth(),view.getRight());const float bottom=(float)totalHeight();
    auto beatX=[this](double beat){return headerWidth+(float)(beat/4*pixelsPerBar);};
    g.setColour(Daw::well);g.fillRect(dirtyArea);
    // Lanes: every other track a shade lighter, the selected track lifted a little.
    for(int row=0;row<(int)owner.project.tracks.size();++row){const int y=rowY(row),h=rowHeight(row);if(y>dirtyArea.getBottom())break;if(y+h<dirtyArea.getY())continue;
        if(row%2==1){g.setColour(Daw::laneAlt);g.fillRect(lanesLeft,y,lanesRight-lanesLeft,h);}
        if(trackAtRow(row)==owner.selectedTrack){g.setColour(juce::Colours::white.withAlpha(.025f));g.fillRect(lanesLeft,y,lanesRight-lanesLeft,h);}}
    if(p.loop&&p.loopEnd>p.loopStart){g.setColour(Daw::accent.withAlpha(.035f));g.fillRect(juce::Rectangle<float>(beatX(p.loopStart),(float)rulerHeight,beatX(p.loopEnd)-beatX(p.loopStart),bottom-rulerHeight));}
    // Grid in four levels: four-bar phrases, bars, beats and sixteenths, each fainter, drawn
    // only when they are far enough apart to read; section edges are the strongest lines.
    {
        int barStep=1;while(barStep<32&&barStep*pixelsPerBar<8)barStep*=2;
        const bool beats=pixelsPerBar>=40,sixteenths=pixelsPerBar>=160;
        const double firstBeat=std::max(0.,(dirtyArea.getX()-headerWidth)/pixelsPerBar*4),lastBeat=(dirtyArea.getRight()-headerWidth)/pixelsPerBar*4;
        const double step=sixteenths?.25:beats?1.:barStep*4.;
        for(double beat=std::floor(firstBeat/step)*step;beat<=lastBeat+step;beat+=step){
            const int x=(int)std::lround(beatX(beat));if(x<lanesLeft)continue;const double bar=beat/4;const bool onBar=std::abs(bar-std::round(bar))<1e-9;
            float alpha=.03f;if(onBar){const int b=(int)std::lround(bar);alpha=b%(barStep*4)==0?.13f:b%barStep==0?.075f:0.f;}else if(std::abs(beat-std::round(beat))<1e-9)alpha=.045f;
            if(alpha<=0)continue;g.setColour(juce::Colours::white.withAlpha(alpha));g.drawVerticalLine(x,(float)rulerHeight,bottom);}
    }
    // Clips: rounded bodies in the track colour, a title bar with the section name and the
    // notes drawn inside (a pitch outline for melodic parts, velocity ticks for drums).
    // Section dividers sit under the clips: a 1 px line on the exact boundary, so the first
    // note of a section (crash, kick) is never covered.
    for(size_t section=0;section<p.sections.size();++section){const float x=std::round(beatX(p.sectionStart((int)section)));
        if(x>=lanesLeft&&x<lanesRight){g.setColour(Daw::sectionColour((int)p.sections[section].type).withAlpha(.85f));const float top=(float)rulerHeight;g.fillRect(x,top,1.f,std::max(0.f,(float)getHeight()-top));}}
    for(int row=0;row<(int)owner.project.tracks.size();++row){const int ti=trackAtRow(row),y=rowY(row),h=rowHeight(row);if(y>dirtyArea.getBottom())break;if(y+h<dirtyArea.getY())continue;
        const auto& t=p.tracks[(size_t)ti];const bool muted=t.mute;auto ink=colour(ti);if(muted)ink=ink.withSaturation(.08f).withMultipliedBrightness(.7f);
        double occupied=0,sectionBegin=0;
        for(size_t si=0;si<t.clips.size();++si){
            const double begin=t.freeTimeline?t.clips[si].position:sectionBegin,duration=p.clipLength(ti,(int)si),end=begin+duration,visible=std::max(begin,occupied);
            if(si<p.sections.size())sectionBegin+=p.sections[si].bars*4.0;occupied=std::max(occupied,end);if(visible>=end)continue;
            const float x0=beatX(visible),x1=beatX(end);const juce::Rectangle<float> cell(x0+1.f,(float)y+2.f,std::max(1.f,x1-x0-1.f),(float)h-4.f);
            if(cell.getRight()<=lanesLeft||cell.getX()>=lanesRight||cell.getRight()<dirtyArea.getX()||cell.getX()>dirtyArea.getRight())continue;
            const auto& c=t.clips[si];const bool sel=selectedClips.count({ti,(int)si})>0||(selectedClips.empty()&&ti==owner.selectedTrack&&(int)si==owner.clipIndex());const bool hover=ti==hoverTrack&&(int)si==hoverClip;
            juce::Graphics::ScopedSaveState state(g);g.reduceClipRegion(juce::Rectangle<int>(lanesLeft,y,lanesRight-lanesLeft,h));
            if(c.notes.empty()){
                // An empty slot: outline, plus a hint once the cell is wide enough to read.
                g.setColour(ink.withAlpha(sel?.14f:.05f));g.fillRoundedRectangle(cell,Daw::radius);
                g.setColour(sel?Daw::text.withAlpha(.8f):ink.withAlpha(hover?.45f:.16f));g.drawRoundedRectangle(cell.reduced(.5f),Daw::radius,sel?1.5f:1.f);

                continue;}
            Daw::fillElevatedSurface(g,cell,Daw::clipBody(ink,sel),6.f);
            const float titleHeight=h>=34&&cell.getWidth()>18?13.f:0.f;
            if(titleHeight>0){juce::Path bar;bar.addRoundedRectangle(cell.getX(),cell.getY(),cell.getWidth(),titleHeight,6.f,6.f,true,true,false,false);g.setGradientFill(juce::ColourGradient(ink.brighter(.1f),cell.getX(),cell.getY(),ink.darker(.08f),cell.getX(),cell.getY()+titleHeight,false));g.fillPath(bar);g.setColour(juce::Colours::white.withAlpha(.18f));g.fillRect(cell.getX()+6.f,cell.getY(),std::max(0.f,cell.getWidth()-12.f),1.f);
                if(cell.getWidth()>28){const int guide=std::clamp(p.sectionAtBeat(begin),0,(int)p.sections.size()-1);auto title=cell.withHeight(titleHeight).reduced(5.f,0);if(std::any_of(c.feel.begin(),c.feel.end(),[](double f){return f>=0;})){const auto badge=title.removeFromRight(9.f);g.setColour(Daw::automation);g.fillEllipse(badge.getCentreX()-2.5f,badge.getCentreY()-2.5f,5.f,5.f);}if(c.locked||cell.getWidth()>42){IconButton::drawIcon(g,IconButton::Icon::lock,title.removeFromRight(10.f).withSizeKeepingCentre(9.f,9.f),c.locked?Daw::onAccent:Daw::onAccent.withAlpha(.38f));title.removeFromRight(3.f);}
                    g.setColour(Daw::onAccent);g.setFont(Daw::font(Daw::small,true));const auto titleText=(c.name.empty()?utf(p.sections[(size_t)guide].name):utf(c.name));g.drawFittedText(titleText,title.toNearestInt(),juce::Justification::centredLeft,1,.6f);}}
            else if(c.locked){IconButton::drawIcon(g,IconButton::Icon::lock,{cell.getRight()-13.f,cell.getY()+3.f,10.f,10.f},ink);}
            const auto notesArea=cell.withTrimmedTop(titleHeight).reduced(1.f,3.f);
            if(cell.getWidth()>6&&notesArea.getHeight()>3){g.saveState();g.reduceClipRegion(notesArea.withLeft(cell.getX()-1.f).toNearestInt());g.setColour(ink.brighter(.35f).withAlpha(muted?.5f:.92f));
                const float from=std::max(cell.getX(),(float)dirtyArea.getX())-2,to=std::min(cell.getRight(),(float)dirtyArea.getRight())+2;
                // A sampler instrument still plays MIDI. Only recorded audio regions
                // have a source waveform on the timeline; the sampler's full-file
                // preview belongs in Studio, independently of the MIDI clip length.
                const bool audioRegion=t.instrument=="sample"&&c.audioOffset>=0&&c.audioDuration>0;
                if(audioRegion){if(const auto asset=owner.audio.sampleAsset(ti);asset&&!asset->peaks.empty()&&asset->seconds()>0){
                    g.setColour(ink.brighter(.3f).withAlpha(muted?.3f:.7f));
                    const double startX=beatX(begin),width=duration/4*pixelsPerBar,binsPerSecond=asset->peaks.size()/asset->seconds();
                    const float cy=notesArea.getCentreY(),halfH=notesArea.getHeight()*.46f;
                    const int first=(int)std::floor(std::max((double)from,startX)/2)*2,last=(int)std::ceil(std::min((double)to,startX+width));
                    for(int px=first;px<last;px+=2){
                        double a=std::clamp((px-startX)/width,0.,1.),b=std::clamp((px+2-startX)/width,0.,1.);
                        if(t.sampler.reverse){const double previous=a;a=1-b;b=1-previous;}
                        const double sourceFrom=(c.audioOffset+a*c.audioDuration)*binsPerSecond,sourceTo=(c.audioOffset+b*c.audioDuration)*binsPerSecond;
                        if(sourceFrom>=asset->peaks.size())continue;
                        const auto binFrom=(size_t)std::max(0.,std::floor(sourceFrom)),binTo=(size_t)std::min((double)asset->peaks.size(),std::max((double)binFrom+1,std::ceil(sourceTo)));
                        float peak=0;for(size_t bin=binFrom;bin<binTo;++bin){const auto values=asset->peaks[bin];peak=std::max(peak,std::max(std::abs(values[0]),std::abs(values[1])));}
                        const float height=std::min(1.f,peak)*halfH;if(height>0)g.fillRect((float)px,cy-height,1.5f,height*2.f);
                    }
                }}
                else if(isDrumTrack(ti)){for(const auto& n:c.notes){const float nx=std::max(notesArea.getX(),beatX(begin+n.start));if(nx<from||nx>to)continue;const float tick=notesArea.getHeight()*(.3f+.7f*n.velocity/127.f);g.fillRect(nx,notesArea.getBottom()-tick,std::max(1.f,std::min(2.f,(float)(n.length/4*pixelsPerBar))),tick);}}
                else{int low=127,high=0;for(const auto& n:c.notes){low=std::min(low,n.pitch);high=std::max(high,n.pitch);}const float span=std::max(1.f,notesArea.getHeight()-2.f);const float noteH=juce::jlimit(1.5f,4.f,span/(float)std::max(6,high-low+1));
                    for(const auto& n:c.notes){const float nx=beatX(begin+n.start),nw=std::max(1.5f,(float)(n.length/4*pixelsPerBar)-.5f);if(nx+nw<from||nx>to)continue;const float ny=notesArea.getBottom()-noteH-(float)((n.pitch-low)/(double)std::max(1,high-low))*(span-noteH);g.fillRect(nx,ny,nw,noteH);}}
                g.restoreState();}
            if(c.audioOffset>=0){g.setColour(ink.brighter(.5f));g.fillRect(cell.getX()+3,cell.getY()+titleHeight+1,5.f,5.f);g.fillRect(cell.getRight()-8,cell.getY()+titleHeight+1,5.f,5.f);const float a=(float)(c.clipFadeIn*owner.project.settings.bpm/60./4*pixelsPerBar),b=(float)(c.clipFadeOut*owner.project.settings.bpm/60./4*pixelsPerBar);if(a>0)g.drawLine(cell.getX()+2,cell.getBottom()-3,std::min(cell.getRight(),cell.getX()+a),cell.getY()+titleHeight+2,1.f);if(b>0)g.drawLine(std::max(cell.getX(),cell.getRight()-b),cell.getY()+titleHeight+2,cell.getRight()-2,cell.getBottom()-3,1.f);}
            if(c.loopLength>0&&duration>c.loopLength){g.setColour(juce::Colours::black.withAlpha(.45f));for(double at=begin+c.loopLength;at<end-1e-9;at+=c.loopLength)if(at>=visible){const float lx=beatX(at);g.fillRect(lx-.5f,cell.getY()+titleHeight,1.f,cell.getHeight()-titleHeight);g.setColour(ink.withAlpha(.6f));g.fillEllipse(lx-2.f,cell.getBottom()-6.f,4.f,4.f);g.setColour(juce::Colours::black.withAlpha(.45f));}}
            if(!sel){g.setColour(hover?ink.brighter(.3f):juce::Colours::black.withAlpha(.6f));g.drawRoundedRectangle(cell.reduced(.5f),6.f,1.f);}else{g.setColour(Daw::text.withAlpha(.85f));g.drawRoundedRectangle(cell.reduced(.5f),6.f,1.5f);}
            if(sel||hover){g.setColour(sel?Daw::text.withAlpha(.8f):ink.brighter(.3f).withAlpha(.8f));g.fillRoundedRectangle(cell.getRight()-4.f,cell.getCentreY()-6.f,2.f,12.f,1.f);}
        }
    }
    // Automation lanes: a darker band with the curve, the area under it and its points.
    for(int row=0;row<(int)owner.project.tracks.size();++row){const int ti=trackAtRow(row);if(!volumeVisible[(size_t)ti])continue;
        const int top=rowY(row)+rowHeight(row);if(top>dirtyArea.getBottom()||top+80<dirtyArea.getY())continue;const auto& volume=envelope(ti);
        g.setColour(Daw::chrome);g.fillRect(lanesLeft,top,lanesRight-lanesLeft,80);g.setColour(Daw::line);g.drawHorizontalLine(top,(float)lanesLeft,(float)lanesRight);
        const float zero=volumeY(ti,envelopeMax(ti)*.5);g.setColour(juce::Colours::white.withAlpha(.06f));g.drawHorizontalLine((int)zero,(float)lanesLeft,(float)lanesRight);
        g.setColour(juce::Colours::white.withAlpha(.12f));for(size_t si=1;si<p.sections.size();++si)g.drawVerticalLine((int)std::lround(beatX(p.sectionStart((int)si))),(float)top,(float)top+80);
        juce::Path line;
        const int from=std::max(lanesLeft,dirtyArea.getX()-2),to=std::min(lanesRight,dirtyArea.getRight()+2);
        for(int x=from;x<=to;x+=2){const float y=volumeY(ti,volume.valueAt((x-headerWidth)*4./pixelsPerBar,true));if(x==from)line.startNewSubPath((float)x,y);else line.lineTo((float)x,y);}
        const auto curve=volume.enabled?Daw::automation:Daw::textFaint;
        if(!line.isEmpty()){auto area=line;area.lineTo((float)to,(float)top+79);area.lineTo((float)from,(float)top+79);area.closeSubPath();g.setGradientFill(juce::ColourGradient(curve.withAlpha(.22f),0.f,(float)top,curve.withAlpha(.02f),0.f,(float)top+79.f,false));g.fillPath(area);}
        g.setColour(curve.withAlpha(.2f));g.strokePath(line,juce::PathStrokeType(3.8f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));g.setColour(curve);g.strokePath(line,juce::PathStrokeType(1.85f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
        for(size_t pi=0;pi+1<volume.points.size();++pi){const double midBeat=(volume.points[pi].beat+volume.points[pi+1].beat)*.5;const float mx=beatX(midBeat),my=volumeY(ti,volume.valueAt(midBeat,true));if(mx>=lanesLeft&&mx<=lanesRight){const bool bent=std::abs(volume.points[pi].curve)>.01;g.setColour(bent?curve:Daw::well);g.fillEllipse(mx-3.f,my-3.f,6.f,6.f);g.setColour(curve.withAlpha(bent?1.f:.55f));g.drawEllipse(mx-3.f,my-3.f,6.f,6.f,1.2f);}}
        for(const auto& point:volume.points){const float x=beatX(point.beat),y=volumeY(ti,point.value);if(x<lanesLeft||x>lanesRight)continue;g.setColour(Daw::well);g.fillEllipse(x-4.f,y-4.f,8.f,8.f);g.setColour(curve);g.drawEllipse(x-3.5f,y-3.5f,7.f,7.f,1.5f);}
    }
    // Track headers stay visible while the song scrolls horizontally.
    const int hx=view.getX();
    for(int row=0;row<(int)owner.project.tracks.size();++row){const int ti=trackAtRow(row),y=rowY(row),h=rowHeight(row);if(y>dirtyArea.getBottom())break;if(y+h+(volumeVisible[(size_t)ti]?80:0)<dirtyArea.getY())continue;
        const auto& t=p.tracks[(size_t)ti];const bool current=ti==owner.selectedTrack;const auto ink=colour(ti);
        Daw::fillElevatedSurface(g,{(float)hx,(float)y,(float)headerWidth,(float)h},current?Daw::surfaceHi:Daw::surface);
        const auto slots=headerChrome(hx,y,h);
        g.setColour(t.mute?ink.withAlpha(.3f):ink);g.fillRect(hx,y,Daw::unit,h-1);
        if(current){g.setColour(juce::Colours::white.withAlpha(.05f));g.fillRect(hx+Daw::unit,y,headerWidth-Daw::unit,1);}
        const auto icon=t.vocalChops?IconButton::Icon::studio:(ti==0||isManualTrack(ti))?IconButton::Icon::piano:ti==1?IconButton::Icon::bass:ti==2?IconButton::Icon::strings:ti==3?IconButton::Icon::guitar:ti==leadTrack?IconButton::Icon::lead:IconButton::Icon::drum;
        IconButton::drawIcon(g,icon,{(float)hx+3*Daw::unit,(float)y+2*Daw::unit+2,16,16},ink.withAlpha(t.mute?.35f:.85f));
        g.setColour(t.mute?Daw::textFaint:current?Daw::text:Daw::text.withAlpha(.88f));g.setFont(Daw::font(Daw::body,true));g.drawFittedText(Localisation::upper(utf(t.name)),slots.name,juce::Justification::centredLeft,1,.6f);
        {const bool bare=t.instrument!="internal"&&t.instrument!="sample"&&owner.audio.plugin(ti)==nullptr;auto source=bare?tr("Drag a VST here"):owner.cachedSourceName(ti);if(t.locked)source=tr("LOCK")+juce::String::fromUTF8(" · ")+source;auto textArea=slots.source;if(bare){IconButton::drawIcon(g,IconButton::Icon::folder,textArea.removeFromLeft(12).toFloat().withSizeKeepingCentre(10.f,10.f),Daw::textDim);}g.setColour(bare?Daw::textDim:Daw::textFaint);g.setFont(Daw::font(Daw::small));g.drawFittedText(source,textArea,juce::Justification::centredLeft,1,.55f);}
        {auto& ticks=headerHoldTick[(size_t)ti];float peakNow=0;for(int side=0;side<2;++side)peakNow=std::max(peakNow,owner.audio.level(ti,side));if(peakNow>=headerHold[(size_t)ti][0]||peakNow>=headerHold[(size_t)ti][1]){headerHold[(size_t)ti][0]=owner.audio.level(ti,0);headerHold[(size_t)ti][1]=owner.audio.level(ti,1);ticks=24;}else if(--ticks<=0){headerHold[(size_t)ti][0]=std::max(owner.audio.level(ti,0),headerHold[(size_t)ti][0]*.9f);headerHold[(size_t)ti][1]=std::max(owner.audio.level(ti,1),headerHold[(size_t)ti][1]*.9f);}
            const auto meter=slots.meter.toFloat();g.setColour(juce::Colours::black.withAlpha(.45f));g.fillRoundedRectangle(meter,1.f);const float gap=1.f,band=(meter.getHeight()-gap)*.5f;
            for(int side=0;side<2;++side){const float level=Daw::meterPosition(owner.audio.level(ti,side));const auto part=juce::Rectangle<float>(meter.getX(),meter.getY()+side*(band+gap),meter.getWidth()*level,band);if(level>0){g.setColour(Daw::meterColour(owner.audio.level(ti,side)));g.fillRect(part);}const float held=Daw::meterPosition(headerHold[(size_t)ti][(size_t)side]);if(held>.01f){g.setColour(Daw::meterColour(headerHold[(size_t)ti][(size_t)side]));g.fillRect(meter.getX()+meter.getWidth()*held,meter.getY()+side*(band+gap),1.5f,band);}}
            g.setColour(owner.audio.trackClipped(ti)?Daw::meterHigh:Daw::line);g.fillRoundedRectangle(slots.led.toFloat(),1.f);}
        auto pill=[&](const char* text,juce::Rectangle<int> r,bool on,juce::Colour c){const auto box=r.toFloat().reduced(1.f,((float)r.getHeight()-16.f)*.5f);g.setColour(on?c:Daw::control);g.fillRoundedRectangle(box,Daw::radius);
            g.setColour(on?c.darker(.3f):juce::Colours::black.withAlpha(.45f));g.drawRoundedRectangle(box.reduced(.5f),Daw::radius,1);g.setColour(on?Daw::onAccent:Daw::textDim);g.setFont(Daw::font(Daw::small,true));g.drawText(text,box,juce::Justification::centred);};
        const bool anyAutomation=t.volume.enabled||t.panAutomation.enabled||std::any_of(t.parameters.begin(),t.parameters.end(),[](const ParameterAutomation& l){return l.curve.enabled;});
        pill("A",slots.automation,volumeVisible[(size_t)ti],Daw::automation);pill("M",slots.mute,t.mute,Daw::mute);pill("S",slots.solo,t.solo,Daw::solo);
        if(anyAutomation&&!volumeVisible[(size_t)ti]){g.setColour(Daw::automation);g.fillEllipse((float)slots.automation.getRight()-2.f,(float)y+h-8.f,3.5f,3.5f);}
        IconButton::drawIcon(g,IconButton::Icon::lock,slots.lock.toFloat().reduced(3.f),t.locked?Daw::accent:Daw::textFaint.withAlpha(.42f));
        IconButton::drawIcon(g,IconButton::Icon::gear,slots.gear.toFloat().reduced(2.f),current?Daw::accent:Daw::textFaint);
        g.setColour(juce::Colours::black.withAlpha(.5f));g.drawHorizontalLine(y+h-1,(float)hx,(float)hx+headerWidth);g.drawVerticalLine(hx+headerWidth-1,(float)y,(float)(y+h));
        if(volumeVisible[(size_t)ti]){const int top=y+h;g.setColour(Daw::chrome);g.fillRect(hx,top,headerWidth,80);g.setColour(Daw::automation.withAlpha(.5f));g.fillRect(hx,top,4,79);const auto& shown=envelope(ti);const int target=laneTarget[(size_t)ti];
            g.setColour(shown.moves()?Daw::automation:Daw::text.withAlpha(.8f));g.setFont(Daw::font(Daw::small,true));g.drawFittedText(envelopeTitle(ti)+juce::String::fromUTF8(" \xe2\x96\xbe"),{hx+16,top,190,20},juce::Justification::centredLeft,1,.7f);
            const juce::Rectangle<float> state((float)hx+16,(float)top+26,64,18);g.setColour(shown.enabled?Daw::automation.withAlpha(.2f):Daw::control);g.fillRoundedRectangle(state,Daw::radius);
            g.setColour(shown.enabled?Daw::automation:Daw::textDim);g.setFont(Daw::font(Daw::small,true));g.drawText(tr(shown.enabled?"ON":"BYPASS"),state,juce::Justification::centred);
            g.setColour(Daw::textFaint);g.setFont(Daw::font(Daw::small));const double value=dragTrack==ti&&dragPoint>=0&&dragPoint<(int)shown.points.size()?shown.points[(size_t)dragPoint].value:shown.active()?shown.valueAt(owner.timelinePosition()):envelopeDefault(ti);g.drawText(target==lanePan?juce::String("L / C / R"):target>=lanePlugin?envelopeValueText(ti,value):juce::String("0 / 100 / 200%"),juce::Rectangle<int>(hx+16,top+52,184,20),juce::Justification::centredLeft);
            g.setColour(juce::Colours::black.withAlpha(.5f));g.drawHorizontalLine(top+79,(float)hx,(float)hx+headerWidth);g.drawVerticalLine(hx+headerWidth-1,(float)top,(float)top+80);}
    }
    // Ruler: section markers above, bar numbers and the loop brace below.
    const int y=view.getY();
    if(dirtyArea.getY()<y+rulerHeight){
        g.setColour(Daw::chrome);g.fillRect(view.getX(),y,view.getWidth(),32);g.setColour(Daw::well.brighter(.04f));g.fillRect(view.getX(),y+32,view.getWidth(),rulerHeight-32);
        for(size_t si=0;si<p.sections.size();++si){const float x0=beatX(p.sectionStart((int)si)),x1=beatX(p.sectionStart((int)si+1));if(x1<lanesLeft||x0>lanesRight)continue;
            const bool current=(int)si==owner.selectedSection;const auto sc=Daw::sectionColour((int)p.sections[si].type);const juce::Rectangle<float> marker(x0+1.f,(float)y+5,std::max(2.f,x1-x0-2.f),23.f);
            g.setColour(sc.withAlpha(current?.3f:.14f));g.fillRoundedRectangle(marker,Daw::radius);g.setColour(sc);g.fillRoundedRectangle(marker.withWidth(3.f),1.f);
            if(current){g.setColour(sc.withAlpha(.8f));g.drawRoundedRectangle(marker.reduced(.5f),Daw::radius,1.f);}
            if(marker.getWidth()>24){auto r=marker.reduced(8.f,0).toNearestInt();g.setColour(current?Daw::text:Daw::text.withAlpha(.85f));auto font=Daw::font(Daw::small,true);const auto name=utf(p.sections[si].name);
                const int nameWidth=std::min(r.getWidth(),(int)std::ceil(juce::GlyphArrangement::getStringWidth(font,name))+2);
                g.setFont(font);g.drawFittedText(name,r.withWidth(nameWidth),juce::Justification::centredLeft,1,.6f);
                if(r.getWidth()>nameWidth+36){g.setColour(Daw::textFaint);g.setFont(Daw::font(Daw::small));g.drawFittedText(juce::String(p.sections[si].bars)+" "+tr("bars"),r.withTrimmedLeft(nameWidth+6),juce::Justification::centredLeft,1,.7f);}}}
        if(p.loopEnd>p.loopStart){const float from=beatX(p.loopStart),to=beatX(p.loopEnd);const auto ink=p.loop?Daw::accent:Daw::textFaint;
            g.setColour(ink.withAlpha(p.loop?.2f:.12f));g.fillRect(from,(float)y+33,to-from,rulerHeight-34.f);g.setColour(ink);g.fillRect(from,(float)y+33,to-from,3.f);g.fillRect(from,(float)y+33,2.f,rulerHeight-34.f);g.fillRect(to-2,(float)y+33,2.f,rulerHeight-34.f);}
        int labelStep=1;while(labelStep<1048576&&labelStep*pixelsPerBar<(owner.timeDisplay?78:34))labelStep*=2;int tickStep=1;while(tickStep<64&&tickStep*pixelsPerBar<6)tickStep*=2;
        const int firstBar=std::max(0,(int)((lanesLeft-headerWidth)/pixelsPerBar)-1),lastBar=(int)((lanesRight-headerWidth)/pixelsPerBar)+1;
        for(int bar=firstBar/tickStep*tickStep;bar<=lastBar;bar+=tickStep){const int x=(int)std::lround(beatX(bar*4.));if(x<lanesLeft)continue;const bool labelled=bar%labelStep==0;
            g.setColour(labelled?Daw::textFaint:juce::Colours::white.withAlpha(.14f));g.drawVerticalLine(x,(float)y+(labelled?40:50),(float)y+rulerHeight);
            if(labelled){g.setColour(Daw::textDim);g.setFont(Daw::font(Daw::small));g.drawText(owner.rulerText(bar*4.),x+4,y+37,owner.timeDisplay?76:48,14,juce::Justification::centredLeft);}
            if(pixelsPerBar>=48)for(int beat=1;beat<4;++beat){g.setColour(juce::Colours::white.withAlpha(.12f));g.drawVerticalLine((int)std::lround(beatX(bar*4.+beat)),(float)y+54,(float)y+rulerHeight);}}
        if(!owner.timeDisplay&&pixelsPerBar>=24){const double viewStart=std::max(0.,(lanesLeft-headerWidth)/pixelsPerBar*4.),viewEnd=std::max(viewStart,(lanesRight-headerWidth)/pixelsPerBar*4.);double beat=viewStart,start=0,end=0;std::string chord;chordCell(p,beat,start,end,chord);beat=start;const auto numberFont=Daw::font(Daw::small);
            for(int guard=0;beat<viewEnd&&guard<4096;++guard){chordCell(p,beat,start,end,chord);if(end<=start+1e-9)break;const float x0=beatX(start),x1=beatX(end);
                if(x1>lanesLeft&&x0<lanesRight&&!chord.empty()){const auto cname=utf(chord);float left=x0+3.f;const double barPos=start/4.;
                    if(std::abs(barPos-std::round(barPos))<1e-6){const int bar=(int)std::lround(barPos);if(labelStep>0&&bar%labelStep==0)left=x0+4.f+std::ceil(juce::GlyphArrangement::getStringWidth(numberFont,owner.rulerText(start)))+4.f;}
                    float right=x1;const float nextNumber=beatX((((int)std::floor(barPos/std::max(1,labelStep)))+1)*std::max(1,labelStep)*4.);if(nextNumber>left)right=std::min(right,nextNumber);
                    const int room=std::max(0,(int)std::floor(right-left-1.f));const int textW=(int)std::ceil(juce::GlyphArrangement::getStringWidth(Daw::font(9.f,true),cname))+8;
                    if(room>=18){const juce::Rectangle<float> pill(left,(float)y+34.f,(float)std::min(textW,room),11.f);g.setColour(Daw::accent.withAlpha(.18f));g.fillRoundedRectangle(pill,3.f);g.setColour(Daw::accent.withAlpha(.55f));g.drawRoundedRectangle(pill.reduced(.5f),3.f,1.f);g.setColour(Daw::accent.brighter(.15f));g.setFont(Daw::font(9.f,true));g.drawFittedText(cname,pill.toNearestInt(),juce::Justification::centred,1,.7f);}}
                beat=end;}}
        g.setColour(juce::Colours::black.withAlpha(.6f));g.drawHorizontalLine(y+32,(float)view.getX(),(float)view.getRight());g.drawHorizontalLine(y+rulerHeight-1,(float)view.getX(),(float)view.getRight());
        g.setColour(Daw::chrome);g.fillRect(view.getX(),y,headerWidth,rulerHeight);g.setColour(juce::Colours::black.withAlpha(.6f));g.drawVerticalLine(view.getX()+headerWidth-1,(float)y,(float)y+rulerHeight);g.drawHorizontalLine(y+rulerHeight-1,(float)view.getX(),(float)view.getX()+headerWidth);
    }
    if(dropTrack>=0){const int row=rowForTrack(dropTrack);const juce::Rectangle<float> target((float)view.getX()+1,(float)rowY(row)+1,(float)view.getWidth()-2,(float)rowHeight(row)-2);
        g.setColour(Daw::accent.withAlpha(.1f));g.fillRect(target);g.setColour(Daw::accent);g.drawRect(target,1.5f);}
    const double target=targetBeat(owner.project);if(target>=0){const float tx=std::round(beatX(target));if(tx>=lanesLeft&&tx<view.getRight()){g.setColour(Daw::accent.withAlpha(.9f));g.fillRect(tx,(float)y+rulerHeight,2.f,(float)std::max(0,view.getHeight()-y-rulerHeight));g.setFont(Daw::font(9.f,true));g.drawText(owner.timeText(owner.project.settings.targetSeconds,0),(int)tx+4,y+rulerHeight+2,52,12,juce::Justification::centredLeft);}}
    const float playX=std::round(beatX(owner.timelinePosition()));if(playX>=lanesLeft&&playX<view.getRight()){g.setColour(Daw::playhead.withAlpha(.9f));g.fillRect(playX,(float)y+33,1.f,(float)view.getHeight()-33);
        juce::Path head;head.addTriangle(playX-5.5f,(float)y+33,playX+6.5f,(float)y+33,playX+.5f,(float)y+40);g.setColour(Daw::playhead);g.fillPath(head);}
    if(clipMoving&&clipTrack>=0&&clipTarget>=0){auto indices=selectedClips;indices.insert({clipTrack,clipSource});const int rowDelta=rowForTrack(clipTarget)-rowForTrack(clipTrack);double delta=clipDropBeat-p.clipStart(clipTrack,clipSource);for(const auto& id:indices)if(id.track>=0&&id.track<(int)p.tracks.size()&&id.index>=0&&id.index<(int)p.tracks[(size_t)id.track].clips.size())delta=std::max(delta,-p.clipStart(id.track,id.index));g.setColour(Daw::accent);for(const auto& id:indices){if(id.track<0||id.track>=(int)p.tracks.size()||id.index<0||id.index>=(int)p.tracks[(size_t)id.track].clips.size())continue;const int row=rowForTrack(id.track)+rowDelta;if(row<0||row>=(int)p.tracks.size())continue;const int dest=trackAtRow(row);const float x=beatX(p.clipStart(id.track,id.index)+delta);g.drawRoundedRectangle({x+1,(float)rowY(rowForTrack(dest))+2,std::max(2.f,(float)(p.clipLength(id.track,id.index)/4*pixelsPerBar)-1),(float)rowHeight(rowForTrack(dest))-4},Daw::radius,2.f);}}
    if(clipMoving&&!dragImages.empty()){const double startBeat=p.clipStart(clipTrack,clipSource),live=clipLiveBeat-startBeat;const int rowDelta=rowForTrack(clipTarget)-rowForTrack(clipTrack);
        for(const auto& d:dragImages){if(!copyingClip){g.setColour(Daw::well.withAlpha(.55f));g.fillRect(d.source);}
            const int dy=rowY(std::clamp(rowForTrack(d.track)+rowDelta,0,(int)p.tracks.size()-1))-rowY(rowForTrack(d.track));
            g.setOpacity(.82f);g.drawImageAt(d.image,d.source.getX()+(int)std::lround(live/4*pixelsPerBar),d.source.getY()+dy);g.setOpacity(1.f);}
        // Landing line and the musical position it snaps to (bar.beat.tick, 960 ticks per beat).
        const float lineX=beatX(clipDropBeat);g.setColour(Daw::accent.withAlpha(.9f));g.fillRect(lineX-.5f,(float)owner.arrangementPort.getViewArea().getY()+rulerHeight,1.f,(float)getHeight());
        const int bar=(int)std::floor(clipDropBeat/4)+1,beatInBar=(int)std::floor(std::fmod(clipDropBeat,4.))+1,tick=(int)std::lround((clipDropBeat-std::floor(clipDropBeat))*960)%960;
        const juce::String label=juce::String(bar)+"."+juce::String(beatInBar)+"."+juce::String(tick).paddedLeft('0',3)+(gridBeats()>0?juce::String():"  "+tr("free"));
        const int targetY=rowY(rowForTrack(clipTarget));auto tag=juce::Rectangle<float>(lineX+4,(float)targetY-18,(float)juce::GlyphArrangement::getStringWidthInt(Daw::font(Daw::small,true),label)+12,16);
        g.setColour(Daw::surfaceHi);g.fillRoundedRectangle(tag,3.f);g.setColour(Daw::text);g.setFont(Daw::font(Daw::small,true));g.drawText(label,tag,juce::Justification::centred);}
}
int ArrangementView::loopEdgeAt(float x)const{
    const auto& p=owner.project;if(!p.hasLoopRegion())return loopNone;const float from=headerWidth+(float)(p.loopStart/4*pixelsPerBar),to=headerWidth+(float)(p.loopEnd/4*pixelsPerBar);
    const float toFrom=std::abs(x-from),toTo=std::abs(x-to);if(std::min(toFrom,toTo)>5)return loopNone;return toTo<=toFrom?loopRightEdge:loopLeftEdge;
}
// Edges and moves snap to the timeline grid (Shift: free); a loop is at least one beat long.
// The first real movement makes one Undo step; a region that was off stays off.
void ArrangementView::dragLoop(const juce::MouseEvent& e){
    if(!loopGestureActive){if(std::abs(e.position.x-loopPressX)<(loopGesture==loopLeftEdge||loopGesture==loopRightEdge?2.f:4.f))return;owner.checkpoint();loopGestureActive=true;}
    auto& p=owner.project;const double length=p.length(),beat=std::clamp((e.position.x-headerWidth)*4./pixelsPerBar,0.,length);const bool free=e.mods.isShiftDown();
    auto snap=[&](double b){return std::clamp(snapBeat(b,free),0.,length);};double from=loopFrom,to=loopTo;
    if(loopGesture==loopLeftEdge)from=std::clamp(snap(beat),0.,std::max(0.,loopTo-1));
    else if(loopGesture==loopRightEdge)to=std::clamp(snap(beat),std::min(length,loopFrom+1),length);
    else if(loopGesture==loopMove){const double delta=std::clamp(snap(loopFrom+beat-loopPressBeat)-loopFrom,-loopFrom,length-loopTo);from=loopFrom+delta;to=loopTo+delta;}
    else{from=snap(loopPressBeat);to=snap(beat);if(std::abs(to-from)<1){to=beat>=loopPressBeat?from+1:from-1;if(to<0){from=1;to=0;}if(to>length){to=length;from=length-1;}}}
    p.setLoopRegion(from,to);if(loopGesture!=loopDraw&&!loopWasOn)p.loop=false;owner.changed(false);
    const auto local=e.getEventRelativeTo(&owner.arrangementPort);owner.arrangementPort.autoScroll(local.x,local.y,30,12);
}
bool ArrangementView::verifyLoopEditing(){
    // At 32 px per bar (8 px per beat), snap 1 beat, a loop from beat 4 to 16.
    const auto saved=owner.project;const double zoom=pixelsPerBar;const double snap=owner.timelineSnap;pixelsPerBar=32;owner.timelineSnap=1;owner.updateSizes();owner.arrangementPort.setViewPosition(0,0);
    auto at=[](double beat){return headerWidth+(float)beat*8;};
    auto event=[this](float x,int mods=0){const juce::Point<float> p(x,46);return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier|mods),1,0,0,0,0,this,this,juce::Time::getCurrentTime(),p,juce::Time::getCurrentTime(),1,false);};
    auto drag=[&](float from,float to,int mods=0){mouseDown(event(from,mods));mouseDrag(event((from+to)*.5f,mods));mouseDrag(event(to,mods));mouseUp(event(to,mods));};
    auto& p=owner.project;owner.checkpoint();p.setLoopRegion(4,16);owner.changed(false);const auto history=owner.undoStack.size();
    bool ok=loopEdgeAt(at(16)+3)==loopRightEdge&&loopEdgeAt(at(4)-3)==loopLeftEdge&&loopEdgeAt(at(10))==loopNone;
    drag(at(16),at(18)+2);ok=ok&&p.loop&&p.loopStart==4&&p.loopEnd==18;                              // right edge, snapped
    drag(at(4),at(2)-3);ok=ok&&p.loopStart==2&&p.loopEnd==18;                                         // left edge
    drag(at(10),at(14));ok=ok&&p.loopStart==6&&p.loopEnd==22;                                         // whole region
    drag(at(22),at(1));ok=ok&&p.loopStart==6&&p.loopEnd==7;                                           // at least one beat
    drag(at(24),at(29));ok=ok&&p.loop&&p.loopStart==24&&p.loopEnd==29;                                // a new loop drawn on the ruler
    ok=ok&&owner.undoStack.size()==history+5;owner.undo(false);ok=ok&&p.loopStart==6&&p.loopEnd==7;owner.undo(false);ok=ok&&p.loopStart==6&&p.loopEnd==22;
    const double position=owner.timelinePosition();mouseDown(event(at(10)));mouseUp(event(at(10)));ok=ok&&p.loopStart==6&&p.loopEnd==22&&std::abs(owner.timelinePosition()-10)<1e-9&&position!=10;   // a click seeks
    // Grid: fixed steps, Auto by zoom (the finest step at least 14 px wide), and snap off.
    owner.timelineSnap=.125;ok=ok&&std::abs(snapBeat(1.06)-1.)<1e-12&&std::abs(snapBeat(1.07)-1.125)<1e-12&&std::abs(snapBeat(1.06,true)-1.06)<=.5/960.;
    owner.timelineSnap=-1;{const double g=gridBeats();ok=ok&&(g==4.||g/4*pixelsPerBar>=14)&&(g==.125||g/8*pixelsPerBar<14)&&std::abs(snapBeat(2.9)-std::round(2.9/g)*g)<1e-12;}
    const bool on=owner.timelineSnapOn;owner.timelineSnapOn=false;ok=ok&&gridBeats()==0.&&std::abs(snapBeat(2.9)-2.9)<=.5/960.;
    // SNAP off + Shift snaps for that drag (the opposite of SNAP); SNAP on + Shift is free.
    {const double g=gridStep(true);ok=ok&&g>0&&std::abs(snapBeat(2.9,true)-std::round(2.9/g)*g)<1e-12;}owner.timelineSnapOn=true;ok=ok&&gridStep(true)==0.;owner.timelineSnapOn=on;
    translateControls(owner);ok=ok&&owner.snapButton.getTooltip().contains(tr("SNAP on: moving clips").substring(0,10))&&!owner.snapButton.getTooltip().contains(tr("Changes re-render this part right away; locked tracks and clips are kept."));
    owner.project=saved;pixelsPerBar=zoom;owner.timelineSnap=snap;owner.changed(false);owner.updateSizes();return ok;
}
void ArrangementView::setHover(int track,int clip){
    if(track==hoverTrack&&clip==hoverClip)return;
    const auto view=owner.arrangementPort.getViewArea();
    for(int t:{hoverTrack,track})if(t>=0){const int row=rowForTrack(t);repaint(view.getX()+headerWidth,rowY(row),view.getWidth(),rowHeight(row));}
    hoverTrack=track;hoverClip=clip;
}
void ArrangementView::mouseExit(const juce::MouseEvent&){setHover(-1,-1);}
void ArrangementView::mouseDown(const juce::MouseEvent& e){
    {const int row=rowAt(e.y);if(row>=0&&!owner.editingRestricted()){const int ti=trackAtRow(row),ci=clipAt(ti,e.position.x);if(ci>=0&&e.y>=rowY(row)+15&&e.y<=rowY(row)+24&&e.x>=owner.arrangementPort.getViewPositionX()+headerWidth){const auto& c=owner.project.tracks[(size_t)ti].clips[(size_t)ci];const double x=headerWidth+owner.project.clipStart(ti,ci)/4*pixelsPerBar,end=x+owner.project.clipLength(ti,ci)/4*pixelsPerBar;if(c.audioOffset>=0&&(std::abs(e.x-x)<10||std::abs(e.x-end)<10)){fadeTrack=ti;fadeClip=ci;fadeSide=std::abs(e.x-x)<10?0:1;owner.checkpoint();return;}}}}

    if(e.mods.isMiddleButtonDown()){panning=true;panStart=owner.arrangementPort.getViewPosition();panAnchor=e.getEventRelativeTo(&owner).position;grabKeyboardFocus();owner.suspendFollow();return;}
    const int heightEdge=heightEdgeAt(e);
    if(heightEdge>=0&&!e.mods.isPopupMenu()){heightTrack=heightEdge;heightBefore=trackHeights[(size_t)heightTrack];heightAnchorY=e.position.y;grabKeyboardFocus();return;}
    auto v=owner.arrangementPort.getViewArea();const int si=sectionAt(e.position.x);
    if(e.y<v.getY()+rulerHeight){
        if(e.x<v.getX()+headerWidth)return;
        if(e.mods.isPopupMenu()){owner.select(owner.selectedTrack,si);owner.showSectionMenu(si);return;}
        if(e.y<v.getY()+32&&!e.mods.isPopupMenu()){const int edge=edgeAt(-1,e.position.x);if(edge>=0){beginResize(edge,-1,e);return;}}
        if(e.y<v.getY()+32){owner.select(owner.selectedTrack,si);owner.seekTimeline(std::max(0.,(e.position.x-headerWidth)*4./pixelsPerBar));owner.audio.play(true);}
        else{const double beat=std::clamp(std::floor((e.x-headerWidth)/pixelsPerBar)*4.,0.,owner.project.length());
            if(e.mods.isAltDown()||(!owner.timeDisplay&&pixelsPerBar>=24&&e.y<v.getY()+45&&!e.mods.isShiftDown()&&loopEdgeAt(e.position.x)==loopNone)){const int bar=(int)std::floor(beat/4.);const int chordBars=std::max(1,owner.project.settings.chordSteps/16);const int slot=(bar/chordBars)%std::max(1,owner.project.settings.chordCount);owner.showChordCirclePopover(slot);return;}
            if(e.mods.isShiftDown()){owner.checkpoint();loopDragging=true;loopAnchor=beat;owner.setLoopRange(beat,std::min(owner.project.length(),beat+4));}
            else{const auto& p=owner.project;const int edge=loopEdgeAt(e.position.x);loopPressX=e.position.x;loopPressBeat=std::max(0.,(e.position.x-headerWidth)*4./pixelsPerBar);loopFrom=p.loopStart;loopTo=p.loopEnd;loopWasOn=p.loop;loopGestureActive=false;
                loopGesture=edge!=loopNone?edge:p.hasLoopRegion()&&loopPressBeat>p.loopStart&&loopPressBeat<p.loopEnd?loopMove:loopDraw;grabKeyboardFocus();}}
        return;
    }
    const int row=rowAt(e.y);if(row<0)return;const int ti=trackAtRow(row);
    if(e.y>=rowY(row)+rowHeight(row)&&volumeVisible[(size_t)ti]){
        owner.selectedTrack=ti;auto& volume=envelope(ti);
        // The lane's title chooses what it automates; the rest of its header switches it on and off.
        if(e.x<v.getX()+headerWidth&&e.y<rowY(row)+rowHeight(row)+20){showLaneTargetMenu(ti);return;}
        if(e.x<v.getX()+headerWidth){owner.checkpoint();makeEnvelopeManual(ti);volume.enabled=!volume.enabled;owner.changed(false);return;}
        const int hit=pointAt(ti,e.position);
        if(e.mods.isPopupMenu()){
            juce::PopupMenu menu;menu.addItem(1,tr(volume.enabled?"Bypass automation":"Enable automation"));menu.addItem(2,tr("Delete point"),hit>=0);menu.addItem(3,tr("Reset curve"),hit>=0);menu.addItem(4,tr("Clear automation"),!volume.points.empty());menu.addSeparator();menu.addItem(5,tr("Choose automation parameter"));menu.addItem(6,tr("Random automation for this lane"),laneTarget[(size_t)ti]!=laneMaster&&!owner.project.tracks[(size_t)ti].locked&&!volume.locked);menu.addItem(7,tr("Random automation for this track"),!owner.project.tracks[(size_t)ti].locked);menu.addItem(8,tr("Lock this lane"),true,volume.locked);menu.addItem(9,tr("Write automation when regenerating"),true,owner.project.settings.randomAutomation);
            juce::Component::SafePointer<ArrangementView> safe(this);menu.showMenuAsync(juce::PopupMenu::Options(),[safe,ti,hit](int result){if(!safe||!result)return;auto& o=safe->owner;if(result==5){o.showAutomationPicker(ti);return;}o.checkpoint();if(result!=9)safe->makeEnvelopeManual(ti);if(result==7)for(auto& lane:o.project.tracks[(size_t)ti].parameters)if(!lane.curve.locked)lane.generatedTone=false;auto& a=safe->envelope(ti);if(result==6||result==7){const int target=safe->laneTarget[(size_t)ti];const int lane=result==7?-1:target==laneVolume?0:target==lanePan?1:2+(target-lanePlugin);randomiseAutomation(o.project,Scope::track,ti,0,(std::uint32_t)juce::Random::getSystemRandom().nextInt(),lane);}else if(result==8)a.locked=!a.locked;else if(result==9)o.project.settings.randomAutomation=!o.project.settings.randomAutomation;if(result==1)a.enabled=!a.enabled;else if(result==2&&hit>=0&&hit<(int)a.points.size())a.points.erase(a.points.begin()+hit);else if(result==3&&hit>=0&&hit<(int)a.points.size())a.points[(size_t)hit].curve=0;else if(result==4){a.points.clear();a.enabled=false;}o.changed(false);});return;
        }
        if(hit<0&&!e.mods.isAltDown()&&volume.points.size()>=16384)return;
        owner.checkpoint();dragTrack=ti;dragPoint=hit;bending=e.mods.isAltDown();dragY=e.position.y;
        if(bending){const double beat=std::clamp((e.position.x-headerWidth)*4./pixelsPerBar,0.,owner.project.length());auto it=std::upper_bound(volume.points.begin(),volume.points.end(),beat,[](double b,const VolumePoint& p){return b<p.beat;});dragPoint=(int)(it-volume.points.begin())-1;if(dragPoint<0||dragPoint+1>=(int)volume.points.size()){dragTrack=-1;return;}originalCurve=volume.points[(size_t)dragPoint].curve;}
        else {const double flat=envelopeDefault(ti);if(volume.points.empty())volume.points={{0,flat,0},{owner.project.length(),flat,0}};if(dragPoint<0){const double beat=std::clamp(std::round((e.x-headerWidth)*4./pixelsPerBar),0.,owner.project.length());auto it=std::lower_bound(volume.points.begin(),volume.points.end(),beat,[](const VolumePoint& p,double b){return p.beat<b;});dragPoint=(int)(it-volume.points.begin());if(it==volume.points.end()||std::abs(it->beat-beat)>1./960.)volume.points.insert(it,{beat,volume.valueAt(beat),0});}volume.enabled=true;editPoint(e.position,!e.mods.isShiftDown());}
        makeEnvelopeManual(ti);owner.changed(false);return;
    }
    if(e.x<v.getX()+headerWidth){const auto slots=headerChrome(v.getX(),rowY(row),rowHeight(row));const auto at=e.getPosition();if(slots.led.expanded(3).contains(at)){owner.audio.clearTrackClip(ti);repaint();return;}if(slots.lock.contains(at)){owner.checkpoint();owner.project.tracks[(size_t)ti].locked=!owner.project.tracks[(size_t)ti].locked;owner.changed(false);return;}if(slots.automation.contains(at)){volumeVisible[(size_t)ti]=!volumeVisible[(size_t)ti];owner.select(ti,owner.selectedSection);owner.updateSizes();repaint();if(volumeVisible[(size_t)ti])owner.showAutomationPicker(ti);return;}owner.select(ti,owner.selectedSection);if(e.mods.isPopupMenu())owner.showTrackMenu(ti);else if(slots.mute.contains(at)){owner.checkpoint();owner.project.tracks[(size_t)ti].mute=!owner.project.tracks[(size_t)ti].mute;owner.changed(false);}else if(slots.solo.contains(at)){owner.checkpoint();owner.project.tracks[(size_t)ti].solo=!owner.project.tracks[(size_t)ti].solo;owner.changed(false);}else if(slots.gear.contains(at))owner.openInstrumentFor(ti);return;}
    const int edge=e.mods.isPopupMenu()?-1:edgeAt(ti,e.position.x),hit=clipAt(ti,e.position.x),index=edge>=0?edge:hit;
    if(e.mods.isShiftDown()&&index>=0){const ClipId id{ti,index};const bool had=selectedClips.count(id)>0;if(had)selectedClips.erase(id);else selectedClips.insert(id);if(!had)owner.selectClip(ti,index,true);else repaint();grabKeyboardFocus();return;}
    const bool inSelection=index>=0&&selectedClips.count({ti,index})>0;
    owner.selectClip(ti,index,inSelection);
    if(e.mods.isPopupMenu()){owner.showClipMenu(ti,index);return;}
    if(edge>=0){beginResize(edge,ti,e);return;}
    // The front edge trims from the lower part of the clip; its upper part still moves it, so a
    // clip grabbed at its start is dragged as before.
    if(const int front=e.mods.isPopupMenu()||e.position.y<rowY(row)+rowHeight(row)*.45f?-1:frontEdgeAt(ti,e.position.x);front>=0){beginResize(front,ti,e);resizeFront=true;resizeOriginalStart=owner.project.clipStart(ti,front);return;}
    if(index>=0&&!e.mods.isShiftDown()&&!e.mods.isAltDown()&&!e.mods.isCommandDown()&&rowHeight(row)>=34&&e.position.y<rowY(row)+15.f&&owner.project.clipLength(ti,index)/4.*pixelsPerBar>42.){const double endBeat=owner.project.clipStart(ti,index)+owner.project.clipLength(ti,index);const float clipRight=headerWidth+(float)(endBeat/4.*pixelsPerBar);if(e.position.x>=clipRight-18.f&&e.position.x<clipRight-5.f){owner.checkpoint();owner.project.tracks[(size_t)ti].clips[(size_t)index].locked=!owner.project.tracks[(size_t)ti].clips[(size_t)index].locked;owner.changed(false);return;}}
    repaint();
    if(index>=0){clipTrack=ti;clipSource=index;clipTarget=ti;clipAnchorX=e.position.x;clipAnchorY=e.position.y;clipDropBeat=clipLiveBeat=owner.project.clipStart(ti,index);clipMoving=false;dragImages.clear();copyingClip=e.mods.isAltDown()||e.mods.isCommandDown();}grabKeyboardFocus();

}
void ArrangementView::makeEnvelopeManual(int track){
    auto& lanes=owner.project.tracks[(size_t)track].parameters;const int index=laneTarget[(size_t)track]-lanePlugin;
    if(index>=0&&index<(int)lanes.size())lanes[(size_t)index].generatedTone=false;
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
        if(auto* plugin=lane.slot==0?owner.audio.plugin(track):lane.slot>0?owner.audio.effectPlugin(track,lane.slot):nullptr)if(lane.index>=0&&lane.index<plugin->getParameters().size())return plugin->getParameters()[lane.index]->getValue();
        if(lane.slot==-1){if(t.instrument=="sample"){if(lane.index==4)return std::log(std::clamp(t.sampler.cutoff,20.,20000.)/20.)/std::log(1000.);if(lane.index==5)return t.sampler.resonance;}else{if(lane.index==0)return t.engine.tone;if(lane.index==1)return t.engine.reverb;if(lane.index==2)return t.engine.chorus;if(lane.index==3)return t.engine.pump;}}
        return .5;}
    return 1.;
}
juce::String ArrangementView::envelopeValueText(int track,double value)const{
    const auto& t=owner.project.tracks[(size_t)track];const int target=laneTarget[(size_t)track];if(target>=lanePlugin&&target-lanePlugin<(int)t.parameters.size()){const auto& lane=t.parameters[(size_t)(target-lanePlugin)];if(lane.slot==-1&&lane.index==4){const double hz=20*std::pow(1000.,value);return hz>=1000?juce::String(hz/1000.,2)+" kHz":juce::String(hz,0)+" Hz";}if(lane.slot>=0)if(auto* plugin=lane.slot==0?owner.audio.plugin(track):owner.audio.effectPlugin(track,lane.slot))if(lane.index>=0&&lane.index<plugin->getParameters().size()){auto* p=plugin->getParameters()[lane.index];return p->getText((float)value,32)+" "+p->getLabel();}}
    return juce::String(value*100,1)+"%";
}
juce::String ArrangementView::envelopeTitle(int track) const{
    const auto& t=owner.project.tracks[(size_t)track];const int target=laneTarget[(size_t)track];
    if(target==lanePan)return tr("PAN AUTOMATION");
    if(target==laneMaster)return tr("MASTER VOLUME AUTOMATION");
    if(target>=lanePlugin&&target-lanePlugin<(int)t.parameters.size()){const auto& lane=t.parameters[(size_t)(target-lanePlugin)];return (lane.slot<0?tr("Sound"):lane.slot==0?tr("Instrument"):t.effect(lane.slot)?utf(t.effect(lane.slot)->displayName()):tr("Missing effect"))+": "+juce::String::fromUTF8(lane.name.c_str());}
    return tr("VOLUME AUTOMATION");
}
// The lane's target: volume, pan, the master volume, or a parameter of the track's instrument
// or insert plugin (a new lane per parameter, at most maxParameterLanes per track).
void ArrangementView::showLaneTargetMenu(int track){const int target=laneTarget[(size_t)track];const auto& lanes=owner.project.tracks[(size_t)track].parameters;owner.showAutomationPicker(track,target>=lanePlugin&&target-lanePlugin<(int)lanes.size()?lanes[(size_t)(target-lanePlugin)].slot:-2);}

int ArrangementView::rowY(int row) const {int y=rulerHeight;for(int r=0;r<row;++r)y+=rowHeight(r)+(volumeVisible[(size_t)trackAtRow(r)]?80:0);return y;}
int ArrangementView::rowAt(int y) const {if(y<rulerHeight||y>=totalHeight())return -1;for(int r=0;r<(int)owner.project.tracks.size();++r)if(y<rowY(r+1))return r;return -1;}
float ArrangementView::volumeY(int track,double value) const {return (float)(rowY(rowForTrack(track))+rowHeight(rowForTrack(track))+72-value*64./envelopeMax(track));}
int ArrangementView::pointAt(int track,juce::Point<float> pos) const {const auto& points=envelope(track).points;for(size_t i=0;i<points.size();++i)if(pos.getDistanceFrom({headerWidth+(float)(points[i].beat/4*pixelsPerBar),volumeY(track,points[i].value)})<8)return (int)i;return -1;}
void ArrangementView::editPoint(juce::Point<float> pos,bool snap){if(dragTrack<0||dragPoint<0)return;auto& points=envelope(dragTrack).points;auto& point=points[(size_t)dragPoint];double beat=(pos.x-headerWidth)*4./pixelsPerBar;if(snap)beat=std::round(beat);const double low=dragPoint>0?points[(size_t)dragPoint-1].beat+1./960.:0,high=dragPoint+1<(int)points.size()?points[(size_t)dragPoint+1].beat-1./960.:owner.project.length();point.beat=std::clamp(beat,low,high);const double ceiling=envelopeMax(dragTrack);point.value=std::clamp((volumeY(dragTrack,0)-pos.y)*ceiling/64.,0.,ceiling);}
int ArrangementView::sectionAt(float x) const {int result=0;const double beat=(x-headerWidth)*4./pixelsPerBar;for(size_t i=0;i<owner.project.sections.size();++i)if(beat>=owner.project.sectionStart((int)i))result=(int)i;return result;}
double ArrangementView::gridBeats()const{return gridStep(false);}
// SNAP on: gestures lock to the grid; off: free. Shift during a drag does the opposite.
double ArrangementView::gridStep(bool shift)const{
    if(owner.timelineSnapOn==shift)return 0.;if(owner.timelineSnap>0)return owner.timelineSnap;
    // Auto: the finest musical step that is still at least 14 px wide at this zoom.
    for(double step:{.125,.25,.5,1.,2.,4.})if(step/4*pixelsPerBar>=14)return step;return 4.;}
double ArrangementView::snapBeat(double beat,bool shift)const{const double grid=gridStep(shift);return std::max(0.,grid>0?std::round(beat/grid)*grid:std::round(beat*960)/960.);}
void ArrangementView::captureDragImages(){
    dragImages.clear();auto indices=selectedClips;indices.insert({clipTrack,clipSource});const auto& p=owner.project;
    for(const auto& id:indices){if(id.track<0||id.track>=(int)p.tracks.size()||id.index<0||id.index>=(int)p.tracks[(size_t)id.track].clips.size())continue;
        const int row=rowForTrack(id.track);const float x0=headerWidth+(float)(p.clipStart(id.track,id.index)/4*pixelsPerBar),x1=headerWidth+(float)((p.clipStart(id.track,id.index)+p.clipLength(id.track,id.index))/4*pixelsPerBar);
        auto area=juce::Rectangle<int>((int)std::floor(x0),rowY(row),std::max(2,(int)std::ceil(x1-x0)+1),rowHeight(row)).getIntersection(getLocalBounds());
        const auto view=owner.arrangementPort.getViewArea();area=area.getIntersection(view.withTrimmedLeft(headerWidth));if(area.isEmpty())continue;
        dragImages.push_back({createComponentSnapshot(area,true,1.f),area,id.track,id.index});}
}
juce::Rectangle<int> ArrangementView::dragBounds()const{
    juce::Rectangle<int> all;const auto& p=owner.project;if(clipTrack<0||clipSource<0)return all;
    const double live=clipLiveBeat-p.clipStart(clipTrack,clipSource),snapped=clipDropBeat-p.clipStart(clipTrack,clipSource);const int rowDelta=rowForTrack(clipTarget)-rowForTrack(clipTrack);
    for(const auto& d:dragImages){const int dy=rowY(std::clamp(rowForTrack(d.track)+rowDelta,0,(int)p.tracks.size()-1))-rowY(rowForTrack(d.track));
        for(double shift:{live,snapped}){auto r=d.source.translated((int)std::lround(shift/4*pixelsPerBar),dy);all=all.isEmpty()?r:all.getUnion(r);}}
    const int lineX=headerWidth+(int)std::lround(clipDropBeat/4*pixelsPerBar);all=all.getUnion({lineX-1,owner.arrangementPort.getViewArea().getY(),3,getHeight()});
    return all.expanded(4,22);
}
int ArrangementView::clipAt(int track,float x)const{return owner.project.clipAtBeat(track,(x-headerWidth)*4./pixelsPerBar);}
int ArrangementView::frontEdgeAt(int track,float x)const{
    const auto& p=owner.project;if(track<0||track>=(int)p.tracks.size()||!p.tracks[(size_t)track].freeTimeline)return -1;const float left=(float)(owner.arrangementPort.getViewPositionX()+headerWidth);if(x<left)return -1;
    for(size_t i=0;i<p.tracks[(size_t)track].clips.size();++i){const float edge=headerWidth+(float)(p.clipStart(track,(int)i)/4*pixelsPerBar);if(edge>=left&&x>=edge&&x-edge<=5&&p.clipLength(track,(int)i)/4*pixelsPerBar>12)return (int)i;}return -1;
}
int ArrangementView::edgeAt(int track,float x)const{
    const auto& p=owner.project;const float left=(float)(owner.arrangementPort.getViewPositionX()+headerWidth);if(x<left)return -1;double occupied=0;
    const size_t count=track<0?p.sections.size():p.tracks[(size_t)track].clips.size();
    for(size_t i=0;i<count;++i){const double start=track<0?p.sectionStart((int)i):p.clipStart(track,(int)i),end=track<0?p.sectionStart((int)i+1):start+p.clipLength(track,(int)i),from=std::max(start,occupied);occupied=std::max(occupied,end);if(from>=end)continue;const float edge=headerWidth+(float)(end/4*pixelsPerBar);if(edge>=left&&x<=edge&&edge-x<=5)return (int)i;}return -1;
}
void ArrangementView::beginResize(int section,int track,const juce::MouseEvent& e){
    resizeFront=false;
    if(track<0)owner.select(owner.selectedTrack,section);else owner.selectClip(track,section);resizeSectionIndex=section;resizeTrack=track;resizeAnchorX=e.position.x;
    resizeOriginalLength=resizeLastLength=track<0?owner.project.sections[(size_t)section].bars*4.:owner.project.clipLength(track,section);resizeBefore=std::make_unique<Project>(owner.project);
    clipTrack=clipSource=clipTarget=-1;grabKeyboardFocus();
}
void ArrangementView::cancelResize(){if(resizeBefore){owner.project=*resizeBefore;owner.refreshControls();repaint();}resizeBefore.reset();resizeSectionIndex=resizeTrack=-1;}
bool ArrangementView::keyPressed(const juce::KeyPress& key){if(owner.editingRestricted())return owner.keyPressed(key);if(heightTrack>=0){if(key==juce::KeyPress::escapeKey){trackHeights[(size_t)heightTrack]=heightBefore;heightTrack=-1;owner.updateSizes();repaint();}return true;}if(resizeBefore){if(key==juce::KeyPress::escapeKey)cancelResize();return true;}if(clipTrack>=0||panning){if(key==juce::KeyPress::escapeKey){clipTrack=clipSource=clipTarget=-1;clipMoving=panning=false;dragImages.clear();dragDirty={};repaint();}return true;}
    // Left / right nudge the selected clips by one grid step (Shift: one bar; snap off: 1/16).
    const int code=key.getKeyCode();const auto mods=key.getModifiers();
    if((code==juce::KeyPress::leftKey||code==juce::KeyPress::rightKey)&&!mods.isCommandDown()&&!mods.isAltDown()){
        int track=owner.selectedTrack,index=owner.clipIndex();if(!selectedClips.empty()){track=selectedClips.begin()->track;index=selectedClips.begin()->index;}
        if(track>=0&&track<(int)owner.project.tracks.size()&&index>=0&&index<(int)owner.project.tracks[(size_t)track].clips.size()){
            const double step=mods.isShiftDown()?4.:gridBeats()>0?gridBeats():.25,start=owner.project.clipStart(track,index),target=std::max(0.,start+(code==juce::KeyPress::leftKey?-step:step));
            if(target!=start)owner.moveSelectedClips(track,index,track,target,false);return true;}}
    return owner.keyPressed(key);}
void ArrangementView::mouseDoubleClick(const juce::MouseEvent& e){const int heightEdge=heightEdgeAt(e);if(heightEdge>=0){heightTrack=-1;trackHeights[(size_t)heightEdge]=trackHeight;saveTrackHeights();owner.updateSizes();repaint();return;}const auto v=owner.arrangementPort.getViewArea();
    const int headerRow=rowAt(e.y);
    if(e.x>=v.getX()&&e.x<v.getX()+132&&e.y>=v.getY()+rulerHeight&&headerRow>=0&&e.y<rowY(headerRow)+rowHeight(headerRow)){
        owner.select(trackAtRow(headerRow),owner.selectedSection);owner.inspectorVisible=true;owner.inspectorToggle.setToggleState(true,juce::dontSendNotification);owner.resized();return;
    }
    // Double-click inside the loop strip removes the loop region.
    if(e.y>=v.getY()+32&&e.y<v.getY()+rulerHeight&&e.x>=v.getX()+headerWidth&&!e.mods.isShiftDown()){const double beat=(e.position.x-headerWidth)*4./pixelsPerBar;const auto& p=owner.project;if(p.hasLoopRegion()&&beat>=p.loopStart&&beat<=p.loopEnd){loopDragging=false;owner.clearLoop();}return;}if(e.y>=v.getY()+rulerHeight&&e.x>=v.getX()+headerWidth&&rowAt(e.y)>=0){const int lane=rowAt(e.y);const int ti=trackAtRow(lane),ci=clipAt(ti,e.position.x);clipTrack=clipSource=clipTarget=-1;resizeBefore.reset();resizeSectionIndex=resizeTrack=-1;if(ci>=0&&rowHeight(lane)>=34&&e.y<rowY(lane)+15.f&&owner.project.clipLength(ti,ci)/4.*pixelsPerBar>18.){owner.renameClip(ti,ci);return;}
        // An audio clip (stem, loop) opens in the Studio, where its trims, speed and gain are edited.
        if(ci>=0&&owner.project.tracks[(size_t)ti].instrument=="sample"&&owner.project.tracks[(size_t)ti].clips[(size_t)ci].audioOffset>=0){owner.select(ti,owner.selectedSection);owner.showStudio(0);return;}owner.showEditor();}}
void ArrangementView::mouseDrag(const juce::MouseEvent& e){
    if(fadeTrack>=0){auto& c=owner.project.tracks[(size_t)fadeTrack].clips[(size_t)fadeClip];const double from=owner.project.clipStart(fadeTrack,fadeClip),duration=owner.project.clipLength(fadeTrack,fadeClip),at=(e.x-headerWidth)*4./pixelsPerBar;const double seconds=std::clamp((fadeSide?from+duration-at:at-from)*60./owner.project.settings.bpm,0.,std::min(30.,duration*60./owner.project.settings.bpm));(fadeSide?c.clipFadeOut:c.clipFadeIn)=seconds;repaint();return;}

    if(panning){const auto offset=e.getEventRelativeTo(&owner).position-panAnchor;owner.arrangementPort.setViewPosition(panStart.x-juce::roundToInt(offset.x),panStart.y-juce::roundToInt(offset.y));return;}
    if(heightTrack>=0){trackHeights[(size_t)heightTrack]=std::clamp(heightBefore+juce::roundToInt(e.position.y-heightAnchorY),40,240);owner.updateSizes();repaint();return;}
    if(resizeBefore&&resizeFront&&resizeTrack>=0){
        const double grid=gridStep(e.mods.isShiftDown());double start=resizeOriginalStart+(e.position.x-resizeAnchorX)*4./pixelsPerBar;
        if(grid>0)start=std::round(start/grid)*grid;
        owner.project.tracks[(size_t)resizeTrack]=resizeBefore->tracks[(size_t)resizeTrack];
        if(owner.project.trimClipStart(resizeTrack,resizeSectionIndex,start)){resizeLastLength=owner.project.clipLength(resizeTrack,resizeSectionIndex);}
        owner.refreshControls();repaint();return;}
    if(resizeBefore){const double step=gridStep(e.mods.isShiftDown()),grid=resizeTrack<0?4.:step<=0?1./960.:step;const double duration=std::clamp(std::round((resizeOriginalLength+(e.position.x-resizeAnchorX)*4./pixelsPerBar)/grid)*grid,resizeTrack<0?4.:.25,resizeTrack<0?512.:maxClipBeats);
        if(duration!=resizeLastLength){
            // A clip edge only touches its own track (and the loop locators): restore just those
            // instead of copying the whole project on every mouse move.
            if(resizeTrack>=0){auto previousTrack=owner.project.tracks[(size_t)resizeTrack];const double ls=owner.project.loopStart,le=owner.project.loopEnd;
                owner.project.tracks[(size_t)resizeTrack]=resizeBefore->tracks[(size_t)resizeTrack];owner.project.loopStart=resizeBefore->loopStart;owner.project.loopEnd=resizeBefore->loopEnd;
                if(!owner.project.resizeClipLoop(resizeTrack,resizeSectionIndex,duration)){owner.project.tracks[(size_t)resizeTrack]=std::move(previousTrack);owner.project.loopStart=ls;owner.project.loopEnd=le;return;}
                resizeLastLength=duration;owner.refreshControls();repaint();}
            else{auto previous=std::move(owner.project);owner.project=*resizeBefore;
            const bool ok=resizeTrack<0?owner.project.resizeGuide(resizeSectionIndex,(int)(duration/4)):owner.project.resizeClipLoop(resizeTrack,resizeSectionIndex,duration);
            if(!ok){owner.project=std::move(previous);return;}resizeLastLength=duration;owner.refreshControls();repaint();}}
        const auto local=e.getEventRelativeTo(&owner.arrangementPort);owner.arrangementPort.autoScroll(local.x,local.y,30,12);return;}
    if(loopGesture!=loopNone){dragLoop(e);return;}
    if(loopDragging){const double beat=std::clamp(std::floor((e.x-headerWidth)/pixelsPerBar)*4.,0.,owner.project.length());owner.setLoopRange(std::min(loopAnchor,beat),std::min(owner.project.length(),std::max(loopAnchor,beat)+4));return;}
    if(clipTrack>=0){const bool starting=!clipMoving;clipMoving=clipMoving||std::abs(e.position.x-clipAnchorX)>2||rowAt(e.y)!=rowForTrack(clipTrack);if(!clipMoving)return;
        if(starting)captureDragImages();
        const int row=rowAt(e.y);clipTarget=row>=0?trackAtRow(row):clipTarget;
        clipLiveBeat=std::max(0.,owner.project.clipStart(clipTrack,clipSource)+(e.position.x-clipAnchorX)*4./pixelsPerBar);clipDropBeat=snapBeat(clipLiveBeat,e.mods.isShiftDown());
        const auto now=dragBounds();repaint(dragDirty.isEmpty()?now:dragDirty.getUnion(now));dragDirty=now;
        const auto local=e.getEventRelativeTo(&owner.arrangementPort);owner.arrangementPort.autoScroll(local.x,local.y,30,12);return;}
    if(dragTrack<0||dragPoint<0)return;auto& points=envelope(dragTrack).points;if(dragPoint>=(int)points.size())return;if(bending)points[(size_t)dragPoint].curve=std::clamp(originalCurve+(dragY-e.position.y)/40.,-1.,1.);else editPoint(e.position,!e.mods.isShiftDown());owner.dirty=true;owner.audio.updateMix(owner.project);repaint();}
void ArrangementView::mouseUp(const juce::MouseEvent&){if(fadeTrack>=0){fadeTrack=fadeClip=-1;owner.changed();return;}
    if(panning){panning=false;return;}
    // A click without a drag on the loop strip seeks there, as before.
    if(loopGesture!=loopNone){const bool moved=loopGestureActive;loopGesture=loopNone;loopGestureActive=false;if(!moved)owner.seekTimeline(loopPressBeat);repaint();return;}
    if(heightTrack>=0){heightTrack=-1;saveTrackHeights();return;}
    if(resizeBefore){if(resizeLastLength!=resizeOriginalLength){auto after=std::move(owner.project);owner.project=*resizeBefore;owner.checkpoint();owner.project=std::move(after);resizeBefore.reset();resizeSectionIndex=resizeTrack=-1;owner.changed();}else{const double beat=std::clamp((resizeAnchorX-headerWidth)*4./pixelsPerBar,0.,owner.project.length());const bool header=resizeTrack<0;cancelResize();if(header){owner.seekTimeline(beat);owner.audio.play(true);}}return;}
    if(clipMoving&&clipTrack>=0&&clipTarget>=0)owner.moveSelectedClips(clipTrack,clipSource,clipTarget,clipDropBeat,copyingClip);if(!dragImages.empty()){dragImages.clear();repaint();}dragDirty={};clipMoving=false;
    if(dragTrack>=0)owner.changed(false);
    clipTrack=clipSource=clipTarget=-1;dragTrack=dragPoint=-1;loopDragging=false;repaint();
}
void ArrangementView::mouseMove(const juce::MouseEvent& e){{const auto v=owner.arrangementPort.getViewArea();const int row=rowAt(e.y);const bool lane=row>=0&&e.y<rowY(row)+rowHeight(row)&&e.x>=v.getX()+headerWidth&&e.y>=v.getY()+rulerHeight;setHover(lane?trackAtRow(row):-1,lane?clipAt(trackAtRow(row),e.position.x):-1);}if(heightEdgeAt(e)>=0){setMouseCursor(juce::MouseCursor::UpDownResizeCursor);setTooltip(tr("Drag the lower edge of this track header to change only its height. Double-click resets; Escape cancels."));return;}const auto view=owner.arrangementPort.getViewArea();const int hoverRow=rowAt(e.y);const bool header=e.y<view.getY()+32;
    if(hoverRow>=0&&e.y>=view.getY()+rulerHeight&&e.x<view.getX()+headerWidth&&e.y<rowY(hoverRow)+rowHeight(hoverRow)) {
        const int ti=trackAtRow(hoverRow);const auto slots=headerChrome(view.getX(),rowY(hoverRow),rowHeight(hoverRow));const auto at=e.getPosition();setMouseCursor(juce::MouseCursor::PointingHandCursor);
        juce::String instrumentTip=tr("Open the original Fase Groove instrument controls.");
        if(const auto* plugin=owner.audio.plugin(ti))instrumentTip=tr("Open %plugin%").replace("%plugin%",plugin->getName());
        else if(owner.project.tracks[(size_t)ti].instrument=="sample")instrumentTip=tr("Open this track's sample in Studio.");
        setTooltip(slots.gear.contains(at)?instrumentTip:slots.solo.contains(at)?tr("Listen to soloed tracks."):slots.mute.contains(at)?tr("Mute this track."):slots.automation.contains(at)?controlTip("AUTOMATION"):tr("Double-click to open the inspector."));return;
    }
    if(hoverTrack>=0&&hoverClip>=0&&e.y<rowY(rowForTrack(hoverTrack))+18){const auto& c=owner.project.tracks[(size_t)hoverTrack].clips[(size_t)hoverClip];if(std::any_of(c.feel.begin(),c.feel.end(),[](double v){return v>=0;})){setTooltip(tr("Frozen Feel")+": "+tr("Density")+" "+juce::String(c.feel[0]<0?owner.project.settings.density:c.feel[0],2)+" / "+tr("Swing")+" "+juce::String(c.feel[1]<0?owner.project.settings.swing:c.feel[1],2)+" / "+tr("Feel")+" "+juce::String(c.feel[2]<0?owner.project.settings.humanize:c.feel[2],2));return;}}
    const bool edge=header?edgeAt(-1,e.position.x)>=0:e.y>=view.getY()+rulerHeight&&hoverRow>=0&&e.y<rowY(hoverRow)+rowHeight(hoverRow)&&(edgeAt(trackAtRow(hoverRow),e.position.x)>=0||(e.position.y>=rowY(hoverRow)+rowHeight(hoverRow)*.45f&&frontEdgeAt(trackAtRow(hoverRow),e.position.x)>=0));
    setMouseCursor(edge?juce::MouseCursor::LeftRightResizeCursor:juce::MouseCursor::NormalCursor);
    if(edge){setTooltip(tr(header?"Drag this guide edge to change its duration. Music and automation after it move with the following sections. Escape cancels; Undo restores.":"Drag this edge to resize only this clip. With SNAP on it follows the grid, with SNAP off it moves freely; Shift does the opposite during the drag. Shortening keeps the source, so extending restores it. Escape cancels; Undo restores."));return;}
    const auto v=owner.arrangementPort.getViewArea();if(e.y>=v.getY()+32&&e.y<v.getY()+rulerHeight&&e.x>=v.getX()+headerWidth){const int loopEdge=loopEdgeAt(e.position.x);const double beat=(e.position.x-headerWidth)*4./pixelsPerBar;const bool inside=owner.project.hasLoopRegion()&&beat>owner.project.loopStart&&beat<owner.project.loopEnd;
        setMouseCursor(loopEdge!=loopNone?juce::MouseCursor::LeftRightResizeCursor:inside?juce::MouseCursor::DraggingHandCursor:juce::MouseCursor::NormalCursor);setTooltip(tr("Drag the loop edges to resize it, drag inside it to move it, or drag on the ruler to draw a new loop. Click to seek; double-click the loop to remove it."));return;}
    if(e.y<v.getY()+rulerHeight){setTooltip(tr("Click a section name to start playback from that position."));return;}const int row=rowAt(e.y);if(row<0)return;if(e.x>=v.getX()&&e.x<v.getX()+132&&e.y<rowY(row)+rowHeight(row)){const int ti=trackAtRow(row);const auto& t=owner.project.tracks[(size_t)ti];setTooltip(Localisation::upper(utf(t.name))+juce::String::fromUTF8("  ·  ")+owner.cachedSourceName(ti)+"\n"+tr("Double-click a track name to show its inspector."));return;}if(e.y<rowY(row)+rowHeight(row)){setTooltip(tr("Click to select without moving playback. Click the section-name header to listen from a position. Double-click or E opens MIDI editing. Drag to move; Alt-drag copies. Shift-click adds clips on any track. Double-click the title to rename a clip. Use the edit toolbar or right-click for split, cut, copy, paste, duplicate and delete."));return;}setTooltip(tr("Volume automation: click to add, drag to move, Shift for free timing, Alt-drag a segment to curve it; right-click for bypass or delete. Hiding a lane does not bypass it."));}
bool ArrangementView::verifyAutomation(const juce::File& folder){
    const auto saved=owner.project;const auto visibility=volumeVisible;const int track=4;const double zoom=pixelsPerBar;pixelsPerBar=24;owner.select(track,0);owner.arrangementPort.setViewPosition(0,0);
    bool ok=!volumeVisible[(size_t)track]&&!owner.project.tracks[(size_t)track].volume.enabled;
    auto click=[&](juce::Point<float> p,int mods=0){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier|mods),1,0,0,0,0,this,this,juce::Time::getCurrentTime(),p,juce::Time::getCurrentTime(),1,false);};
    const auto automationSlot=headerChrome(0,rulerHeight,rowHeight(0)).automation;
    mouseDown(click({(float)automationSlot.getCentreX(),(float)rulerHeight+16}));ok=volumeVisible[(size_t)track]&&owner.automationPicker&&ok;if(owner.automationPicker)owner.automationPicker->dismiss();juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    juce::Point<float> at(headerWidth+24,volumeY(track,.5));mouseDown(click(at));mouseUp(click(at));
    auto& v=owner.project.tracks[(size_t)track].volume;ok=v.enabled&&v.points.size()==3&&std::abs(v.valueAt(4)-.5)<1e-6&&ok;
    mouseDown(click(at));mouseDrag(click(at.translated(24,-8)));mouseUp(click(at));ok=v.points.size()>1&&std::abs(v.points[1].beat-8)<1e-6&&std::abs(v.points[1].value-.75)<1e-6&&ok;
    at={headerWidth+12,volumeY(track,.9)};mouseDown(click(at,juce::ModifierKeys::altModifier));mouseDrag(click(at.translated(0,-20),juce::ModifierKeys::altModifier));mouseUp(click(at));ok=!v.points.empty()&&v.points[0].curve>.4&&ok;
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
        const bool previousChecks=ok;const auto ownedVisibility=volumeVisible;const auto viewBefore=owner.arrangementPort.getViewPosition();owner.select(0,0);volumeVisible[0]=true;owner.updateSizes();
        ParameterAutomation generated;generated.slot=-1;generated.index=0;generated.generatedTone=true;generated.curve.enabled=true;generated.curve.points={{0,.2,0},{owner.project.length(),.8,0}};
        auto& lanes=owner.project.tracks[0].parameters;lanes.push_back(generated);laneTarget[0]=lanePlugin+(int)lanes.size()-1;
        p={headerWidth+24,volumeY(0,.6)};mouseDown(click(p));mouseUp(click(p));
        const bool edited=!owner.project.tracks[0].parameters.back().generatedTone;ok=edited&&ok;
        owner.undo(false);const bool restored=owner.project.tracks[0].parameters.back().generatedTone;ok=restored&&ok;
        folder.getChildFile("tone-ownership-ui.txt").replaceWithText(juce::String("Previous checks: ")+(previousChecks?"PASS":"FAIL")+"\nManual edit: "+(edited?"PASS":"FAIL")+"\nUndo ownership: "+(restored?"PASS":"FAIL"));
        owner.select(track,0);volumeVisible=ownedVisibility;owner.updateSizes();owner.arrangementPort.setViewPosition(viewBefore);
        laneTarget=targets;
    }
    mouseDown(click({(float)automationSlot.getCentreX(),(float)rulerHeight+16}));ok=!volumeVisible[(size_t)track]&&ok;
    owner.project=saved;volumeVisible=visibility;pixelsPerBar=zoom;owner.changed();owner.undoStack.clear();owner.redoStack.clear();folder.getChildFile("volume-automation-ui.txt").replaceWithText(ok?"PASS hidden and bypassed default, lane button, add/move/curve, bypass, pan/master lanes and generated Tone ownership with Undo, hide, MIDI unchanged":"FAIL automation editing");return ok;
}

}
