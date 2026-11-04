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
namespace {
juce::Colour colour(int t){return Daw::trackColour(t);}
juce::Colour guitarColour(int lane){return lane<=1?Palette::accent:lane<=4?Palette::accent2:lane<=6?Palette::percussion:lane<=8?Palette::cymbals:Palette::toms;}
juce::String utf(const std::string& s){return juce::String::fromUTF8(s.c_str());}
// Editor notifications invalidate one instance. Explicit saves also refresh opaque state.
void captureProjectStates(AudioEngine& audio,Project& project,bool fresh){if(fresh)audio.invalidatePluginStates();audio.capturePluginStates(project);}
void label(juce::Graphics& g,const juce::String& s,juce::Rectangle<int> r,float size=12,juce::Colour c=Daw::text){g.setColour(c);g.setFont(Daw::font(size));g.drawText(tr(s),r,juce::Justification::centredLeft);}
constexpr int trackHeight=40,headerWidth=216,rulerHeight=60;
// Header controls come off the right edge in steps of Daw::unit, so hit-testing and
// painting share one rectangle. The automation slot still covers x=140.
struct HeaderChrome { juce::Rectangle<int> name,source,meter,led,lock,automation,mute,solo,gear; };
HeaderChrome headerChrome(int hx,int y,int h){
    HeaderChrome c;auto band=juce::Rectangle<int>(hx,y,headerWidth,h).withTrimmedRight(Daw::unit);const int side=5*Daw::unit;
    c.gear=band.removeFromRight(side);band.removeFromRight(Daw::unit);c.solo=band.removeFromRight(side);band.removeFromRight(Daw::unit);c.mute=band.removeFromRight(side);band.removeFromRight(Daw::unit);c.automation=band.removeFromRight(side);
    c.lock={c.automation.getX()-4*Daw::unit-2,y+Daw::unit,4*Daw::unit,4*Daw::unit};
    c.name={hx+9*Daw::unit,y+Daw::unit,std::max(24,c.lock.getX()-(hx+9*Daw::unit)-2),16};
    c.source={c.name.getX(),y+5*Daw::unit,c.name.getWidth(),12};
    // The meter and clip light stop before the automation button, so a click on A is not the clip reset.
    const int meterRight=c.automation.getX()-Daw::unit;
    c.led={meterRight-2*Daw::unit,y+h-2*Daw::unit-2,2*Daw::unit,6};
    c.meter={c.name.getX(),c.led.getY(),std::max(6*Daw::unit,c.led.getX()-c.name.getX()-Daw::unit),6};
    return c;
}
juce::ModifierKeys physicalMouseModifiers(){
    auto result=juce::ModifierKeys::getCurrentModifiersRealtime();
   #if JUCE_MAC
    // JUCE refreshes Cocoa keyboard modifiers but retains the last mouse event's
    // buttons. A missed mouse-up therefore needs the actual session button state.
    result=result.withoutMouseButtons();
    const int flags[]{juce::ModifierKeys::leftButtonModifier,juce::ModifierKeys::rightButtonModifier,juce::ModifierKeys::middleButtonModifier,juce::ModifierKeys::backButtonModifier,juce::ModifierKeys::forwardButtonModifier};
    for(int button=0;button<5;++button)if(CGEventSourceButtonState(kCGEventSourceStateCombinedSessionState,static_cast<CGMouseButton>(button)))result=result.withFlags(flags[button]);
   #endif
    return result;
}
}
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
            const auto& c=t.clips[si];const bool sel=ti==owner.selectedTrack&&(selectedClips.count((int)si)>0||(int)si==owner.clipIndex());const bool hover=ti==hoverTrack&&(int)si==hoverClip;
            juce::Graphics::ScopedSaveState state(g);g.reduceClipRegion(juce::Rectangle<int>(lanesLeft,y,lanesRight-lanesLeft,h));
            if(c.notes.empty()){
                // An empty slot: outline, plus a hint once the cell is wide enough to read.
                g.setColour(ink.withAlpha(sel?.14f:.05f));g.fillRoundedRectangle(cell,Daw::radius);
                g.setColour(sel?Daw::text.withAlpha(.8f):ink.withAlpha(hover?.45f:.16f));g.drawRoundedRectangle(cell.reduced(.5f),Daw::radius,sel?1.5f:1.f);

                continue;}
            Daw::fillElevatedSurface(g,cell,Daw::clipBody(ink,sel),6.f);
            const float titleHeight=h>=34&&cell.getWidth()>18?13.f:0.f;
            if(titleHeight>0){juce::Path bar;bar.addRoundedRectangle(cell.getX(),cell.getY(),cell.getWidth(),titleHeight,6.f,6.f,true,true,false,false);g.setGradientFill(juce::ColourGradient(ink.brighter(.1f),cell.getX(),cell.getY(),ink.darker(.08f),cell.getX(),cell.getY()+titleHeight,false));g.fillPath(bar);g.setColour(juce::Colours::white.withAlpha(.18f));g.fillRect(cell.getX()+6.f,cell.getY(),std::max(0.f,cell.getWidth()-12.f),1.f);
                if(cell.getWidth()>28){const int guide=std::clamp(p.sectionAtBeat(begin),0,(int)p.sections.size()-1);auto title=cell.withHeight(titleHeight).reduced(5.f,0);if(c.locked||cell.getWidth()>42){IconButton::drawIcon(g,IconButton::Icon::lock,title.removeFromRight(10.f).withSizeKeepingCentre(9.f,9.f),c.locked?Daw::onAccent:Daw::onAccent.withAlpha(.38f));title.removeFromRight(3.f);}
                    g.setColour(Daw::onAccent);g.setFont(Daw::font(Daw::small,true));g.drawFittedText(utf(p.sections[(size_t)guide].name),title.toNearestInt(),juce::Justification::centredLeft,1,.6f);}}
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
            if(labelled){g.setColour(Daw::textDim);g.setFont(Daw::font(Daw::small));g.drawText(owner.rulerText(bar*4.),x+4,y+37,owner.timeDisplay?76:48,14,juce::Justification::centredLeft);
                if(!owner.timeDisplay&&pixelsPerBar>=24){const auto& chords=owner.cachedChordNames();if(!chords.empty()){const int chordBars=std::max(1,p.settings.chordSteps/16);const auto cname=utf(chords[(size_t)((bar/chordBars)%(int)chords.size())]);const int pillW=std::clamp((int)cname.length()*6+10,22,std::max(22,(int)(pixelsPerBar*labelStep)-22));const juce::Rectangle<float> pill((float)x+18.f,(float)y+34.f,(float)pillW,11.f);g.setColour(Daw::accent.withAlpha(.18f));g.fillRoundedRectangle(pill,3.f);g.setColour(Daw::accent.withAlpha(.55f));g.drawRoundedRectangle(pill.reduced(.5f),3.f,1.f);g.setColour(Daw::accent.brighter(.15f));g.setFont(Daw::font(9.f,true));g.drawFittedText(cname,pill.toNearestInt(),juce::Justification::centred,1,.7f);}}}
            if(pixelsPerBar>=48)for(int beat=1;beat<4;++beat){g.setColour(juce::Colours::white.withAlpha(.12f));g.drawVerticalLine((int)std::lround(beatX(bar*4.+beat)),(float)y+54,(float)y+rulerHeight);}}
        g.setColour(juce::Colours::black.withAlpha(.6f));g.drawHorizontalLine(y+32,(float)view.getX(),(float)view.getRight());g.drawHorizontalLine(y+rulerHeight-1,(float)view.getX(),(float)view.getRight());
        g.setColour(Daw::chrome);g.fillRect(view.getX(),y,headerWidth,rulerHeight);g.setColour(juce::Colours::black.withAlpha(.6f));g.drawVerticalLine(view.getX()+headerWidth-1,(float)y,(float)y+rulerHeight);g.drawHorizontalLine(y+rulerHeight-1,(float)view.getX(),(float)view.getX()+headerWidth);
    }
    if(dropTrack>=0){const int row=rowForTrack(dropTrack);const juce::Rectangle<float> target((float)view.getX()+1,(float)rowY(row)+1,(float)view.getWidth()-2,(float)rowHeight(row)-2);
        g.setColour(Daw::accent.withAlpha(.1f));g.fillRect(target);g.setColour(Daw::accent);g.drawRect(target,1.5f);}
    const float playX=std::round(beatX(owner.timelinePosition()));if(playX>=lanesLeft&&playX<view.getRight()){g.setColour(Daw::playhead.withAlpha(.9f));g.fillRect(playX,(float)y+33,1.f,(float)view.getHeight()-33);
        juce::Path head;head.addTriangle(playX-5.5f,(float)y+33,playX+6.5f,(float)y+33,playX+.5f,(float)y+40);g.setColour(Daw::playhead);g.fillPath(head);}
    if(clipMoving&&clipTrack>=0&&clipTarget>=0){auto indices=selectedClips;indices.insert(clipSource);double delta=clipDropBeat-p.clipStart(clipTrack,clipSource);for(int index:indices)delta=std::max(delta,-p.clipStart(clipTrack,index));g.setColour(Daw::accent);for(int index:indices){if(index<0||index>=(int)p.tracks[(size_t)clipTrack].clips.size())continue;const float x=beatX(p.clipStart(clipTrack,index)+delta);g.drawRoundedRectangle({x+1,(float)rowY(rowForTrack(clipTarget))+2,std::max(2.f,(float)(p.clipLength(clipTrack,index)/4*pixelsPerBar)-1),(float)rowHeight(rowForTrack(clipTarget))-4},Daw::radius,2.f);}}
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

    const bool sameTrack=owner.selectedTrack==ti;const auto previous=selectedClips;
    owner.selectClip(ti,index,sameTrack&&previous.count(index)>0);
    if(e.mods.isShiftDown()&&index>=0){selectedClips=sameTrack?previous:std::set<int>{};if(selectedClips.count(index))selectedClips.erase(index);else selectedClips.insert(index);repaint();grabKeyboardFocus();return;}
    if(e.mods.isPopupMenu()){owner.showClipMenu(ti,index);return;}
    if(edge>=0){beginResize(edge,ti,e);return;}
    if(index>=0&&!e.mods.isShiftDown()&&!e.mods.isAltDown()&&!e.mods.isCommandDown()&&rowHeight(row)>=34&&e.position.y<rowY(row)+15.f&&owner.project.clipLength(ti,index)/4.*pixelsPerBar>42.){const double endBeat=owner.project.clipStart(ti,index)+owner.project.clipLength(ti,index);const float clipRight=headerWidth+(float)(endBeat/4.*pixelsPerBar);if(e.position.x>=clipRight-18.f&&e.position.x<clipRight-5.f){owner.checkpoint();owner.project.tracks[(size_t)ti].clips[(size_t)index].locked=!owner.project.tracks[(size_t)ti].clips[(size_t)index].locked;owner.changed(false);return;}}
    repaint();
    if(index>=0){clipTrack=ti;clipSource=index;clipTarget=ti;clipAnchorX=e.position.x;clipDropBeat=owner.project.clipStart(ti,index);clipMoving=false;copyingClip=e.mods.isAltDown()||e.mods.isCommandDown();}grabKeyboardFocus();

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
bool ArrangementView::keyPressed(const juce::KeyPress& key){if(owner.editingRestricted())return owner.keyPressed(key);if(heightTrack>=0){if(key==juce::KeyPress::escapeKey){trackHeights[(size_t)heightTrack]=heightBefore;heightTrack=-1;owner.updateSizes();repaint();}return true;}if(resizeBefore){if(key==juce::KeyPress::escapeKey)cancelResize();return true;}if(clipTrack>=0||panning){if(key==juce::KeyPress::escapeKey){clipTrack=clipSource=clipTarget=-1;clipMoving=panning=false;repaint();}return true;}return owner.keyPressed(key);}
void ArrangementView::mouseDoubleClick(const juce::MouseEvent& e){const int heightEdge=heightEdgeAt(e);if(heightEdge>=0){heightTrack=-1;trackHeights[(size_t)heightEdge]=trackHeight;saveTrackHeights();owner.updateSizes();repaint();return;}const auto v=owner.arrangementPort.getViewArea();
    const int headerRow=rowAt(e.y);
    if(e.x>=v.getX()&&e.x<v.getX()+132&&e.y>=v.getY()+rulerHeight&&headerRow>=0&&e.y<rowY(headerRow)+rowHeight(headerRow)){
        owner.select(trackAtRow(headerRow),owner.selectedSection);owner.inspectorVisible=true;owner.inspectorToggle.setToggleState(true,juce::dontSendNotification);owner.resized();return;
    }
    // Double-click inside the loop strip removes the loop region.
    if(e.y>=v.getY()+32&&e.y<v.getY()+rulerHeight&&e.x>=v.getX()+headerWidth&&!e.mods.isShiftDown()){const double beat=(e.position.x-headerWidth)*4./pixelsPerBar;const auto& p=owner.project;if(p.hasLoopRegion()&&beat>=p.loopStart&&beat<=p.loopEnd){loopDragging=false;owner.clearLoop();}return;}if(e.y>=v.getY()+rulerHeight&&e.x>=v.getX()+headerWidth&&rowAt(e.y)>=0){clipTrack=clipSource=clipTarget=-1;resizeBefore.reset();resizeSectionIndex=resizeTrack=-1;owner.showEditor();}}
void ArrangementView::mouseDrag(const juce::MouseEvent& e){
    if(panning){const auto offset=e.getEventRelativeTo(&owner).position-panAnchor;owner.arrangementPort.setViewPosition(panStart.x-juce::roundToInt(offset.x),panStart.y-juce::roundToInt(offset.y));return;}
    if(heightTrack>=0){trackHeights[(size_t)heightTrack]=std::clamp(heightBefore+juce::roundToInt(e.position.y-heightAnchorY),40,240);owner.updateSizes();repaint();return;}
    if(resizeBefore){const double grid=resizeTrack<0?4.:e.mods.isShiftDown()||owner.timelineSnap<=0?1./960.:owner.timelineSnap;const double duration=std::clamp(std::round((resizeOriginalLength+(e.position.x-resizeAnchorX)*4./pixelsPerBar)/grid)*grid,resizeTrack<0?4.:.25,resizeTrack<0?512.:maxClipBeats);
        if(duration!=resizeLastLength){auto previous=std::move(owner.project);owner.project=*resizeBefore;
            const bool ok=resizeTrack<0?(owner.project.resizeGuide(resizeSectionIndex,(int)(duration/4)),true):owner.project.resizeClipLoop(resizeTrack,resizeSectionIndex,duration);
            if(!ok){owner.project=std::move(previous);return;}resizeLastLength=duration;owner.refreshControls();repaint();}
        const auto local=e.getEventRelativeTo(&owner.arrangementPort);owner.arrangementPort.autoScroll(local.x,local.y,30,12);return;}
    if(loopGesture!=loopNone){dragLoop(e);return;}
    if(loopDragging){const double beat=std::clamp(std::floor((e.x-headerWidth)/pixelsPerBar)*4.,0.,owner.project.length());owner.setLoopRange(std::min(loopAnchor,beat),std::min(owner.project.length(),std::max(loopAnchor,beat)+4));return;}
    if(clipTrack>=0){clipMoving=clipMoving||std::abs(e.position.x-clipAnchorX)>4||rowAt(e.y)!=rowForTrack(clipTrack);const int row=rowAt(e.y);clipTarget=row>=0?trackAtRow(row):clipTrack;clipDropBeat=snapBeat(owner.project.clipStart(clipTrack,clipSource)+(e.position.x-clipAnchorX)*4./pixelsPerBar,e.mods.isShiftDown());const auto local=e.getEventRelativeTo(&owner.arrangementPort);owner.arrangementPort.autoScroll(local.x,local.y,30,12);repaint();return;}
    if(dragTrack<0||dragPoint<0)return;auto& points=envelope(dragTrack).points;if(dragPoint>=(int)points.size())return;if(bending)points[(size_t)dragPoint].curve=std::clamp(originalCurve+(dragY-e.position.y)/40.,-1.,1.);else editPoint(e.position,!e.mods.isShiftDown());owner.dirty=true;owner.audio.updateMix(owner.project);repaint();}
void ArrangementView::mouseUp(const juce::MouseEvent&){
    if(panning){panning=false;return;}
    // A click without a drag on the loop strip seeks there, as before.
    if(loopGesture!=loopNone){const bool moved=loopGestureActive;loopGesture=loopNone;loopGestureActive=false;if(!moved)owner.seekTimeline(loopPressBeat);repaint();return;}
    if(heightTrack>=0){heightTrack=-1;saveTrackHeights();return;}
    if(resizeBefore){if(resizeLastLength!=resizeOriginalLength){auto after=std::move(owner.project);owner.project=*resizeBefore;owner.checkpoint();owner.project=std::move(after);resizeBefore.reset();resizeSectionIndex=resizeTrack=-1;owner.changed();}else{const double beat=std::clamp((resizeAnchorX-headerWidth)*4./pixelsPerBar,0.,owner.project.length());const bool header=resizeTrack<0;cancelResize();if(header){owner.seekTimeline(beat);owner.audio.play(true);}}return;}
    if(clipMoving&&clipTrack>=0&&clipTarget>=0)owner.moveSelectedClips(clipTrack,clipSource,clipTarget,clipDropBeat,copyingClip);clipMoving=false;
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
    const bool edge=header?edgeAt(-1,e.position.x)>=0:e.y>=view.getY()+rulerHeight&&hoverRow>=0&&e.y<rowY(hoverRow)+rowHeight(hoverRow)&&edgeAt(trackAtRow(hoverRow),e.position.x)>=0;
    setMouseCursor(edge?juce::MouseCursor::LeftRightResizeCursor:juce::MouseCursor::NormalCursor);
    if(edge){setTooltip(tr(header?"Drag this guide edge to change its duration. Clips and automation stay in place. Escape cancels; Undo restores.":"Drag this edge to resize only this clip loop. Shorten and extend without losing the source; Shift for quarter-beat timing. Escape cancels; Undo restores."));return;}
    const auto v=owner.arrangementPort.getViewArea();if(e.y>=v.getY()+32&&e.y<v.getY()+rulerHeight&&e.x>=v.getX()+headerWidth){const int loopEdge=loopEdgeAt(e.position.x);const double beat=(e.position.x-headerWidth)*4./pixelsPerBar;const bool inside=owner.project.hasLoopRegion()&&beat>owner.project.loopStart&&beat<owner.project.loopEnd;
        setMouseCursor(loopEdge!=loopNone?juce::MouseCursor::LeftRightResizeCursor:inside?juce::MouseCursor::DraggingHandCursor:juce::MouseCursor::NormalCursor);setTooltip(tr("Drag the loop edges to resize it, drag inside it to move it, or drag on the ruler to draw a new loop. Click to seek; double-click the loop to remove it."));return;}
    if(e.y<v.getY()+rulerHeight){setTooltip(tr("Click a section name to start playback from that position."));return;}const int row=rowAt(e.y);if(row<0)return;if(e.x>=v.getX()&&e.x<v.getX()+132&&e.y<rowY(row)+rowHeight(row)){const int ti=trackAtRow(row);const auto& t=owner.project.tracks[(size_t)ti];setTooltip(Localisation::upper(utf(t.name))+juce::String::fromUTF8("  ·  ")+owner.cachedSourceName(ti)+"\n"+tr("Double-click a track name to show its inspector."));return;}if(e.y<rowY(row)+rowHeight(row)){setTooltip(tr("Click to select without moving playback. Click the section-name header to listen from a position. Double-click or E opens MIDI editing. Drag to move; Alt-drag copies. Shift-click selects more clips on this track. Use the edit toolbar or right-click for split, cut, copy, paste, duplicate and delete."));return;}setTooltip(tr("Volume automation: click to add, drag to move, Shift for free timing, Alt-drag a segment to curve it; right-click for bypass or delete. Hiding a lane does not bypass it."));}
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
PianoRoll::PianoRoll(MainComponent& m):owner(m){setWantsKeyboardFocus(true);}
bool PianoRoll::ownerTrackIsDrum()const{const auto& track=owner.project.tracks[(size_t)owner.selectedTrack];return isDrumTrack(owner.selectedTrack)&&!(track.instrument=="sample"&&track.sampler.mode==2);}
bool PianoRoll::laneView()const{const auto& track=owner.project.tracks[(size_t)owner.selectedTrack];if(track.instrument=="sample"&&track.sampler.mode==2)return false;return ownerTrackIsDrum()||(owner.selectedTrack==3&&!guitarNotes);}
bool PianoRoll::scaleGuideAvailable()const{const auto& track=owner.project.tracks[(size_t)owner.selectedTrack];return !laneView()&&!(track.instrument=="sample"&&track.sampler.mode==2);}
bool PianoRoll::noteInScale(int midiNote)const{const auto& settings=owner.project.settings;const auto& intervals=sgk::scaleIntervals(settings.scale);const int pc=(midiNote-settings.key+120)%12;return std::find(intervals.begin(),intervals.end(),pc)!=intervals.end();}
void PianoRoll::fitNotes(){
    int lo=127,hi=0;for(const auto& n:owner.clip().notes){lo=std::min(lo,n.pitch);hi=std::max(hi,n.pitch);}
    if(lo>hi){lo=owner.selectedTrack==1?28:48;hi=lo+24;}lo-=2;hi+=2;while(hi-lo<24){--lo;++hi;}
    lowNote=std::clamp(lo,0,103);highNote=std::clamp(hi,lowNote+24,127);
}
float PianoRoll::noteY(const Note& n)const{return laneView()?40.f+(owner.selectedTrack==3?n.lane:0)*40.f:48.f+(highNote-n.pitch)*rowHeight();}
float PianoRoll::noteWidth(const Note& n)const{return std::max(5.f,(float)((laneView()&&!ownerTrackIsDrum()?grid*.85:n.length)*pixelsPerBeat));}
float PianoRoll::noteHeight()const{return laneView()?36.f:rowHeight()-1.f;}
int PianoRoll::laneAt(float y)const{return owner.selectedTrack==3?std::clamp((int)((y-40)/40),0,11):0;}
int PianoRoll::pitch(float y)const{return ownerTrackIsDrum()?(owner.project.tracks[(size_t)owner.selectedTrack].engine.midiNote<0?fgd::defaultNote(owner.selectedTrack-4):owner.project.tracks[(size_t)owner.selectedTrack].engine.midiNote):std::clamp(highNote-(int)((y-48)/rowHeight()),0,127);}
int PianoRoll::hit(juce::Point<float> p)const{const auto& c=owner.clip();for(int i=(int)c.notes.size()-1;i>=0;--i){const auto& n=c.notes[(size_t)i];juce::Rectangle<float> r(leftMargin()+(float)(n.start*pixelsPerBeat),noteY(n),noteWidth(n),noteHeight());if(r.contains(p))return i;}return -1;}
void PianoRoll::paint(juce::Graphics& g){
    g.fillAll(Daw::well);const auto view=owner.pianoPort.getViewArea();const auto& notes=owner.clip().notes;const float left=(float)leftMargin();
    // Long clips: grid lines and notes outside the repainted area are skipped.
    const auto dirtyArea=g.getClipBounds().expanded(4);
    const auto gridArea=juce::Rectangle<float>(left,48,(float)getWidth()-left,std::max(40.f,velocityTop()-52));
    auto blackKey=[](int n){const int k=n%12;return k==1||k==3||k==6||k==8||k==10;};
    const bool highlightScale=showScaleGuide&&scaleGuideAvailable();
    {
        juce::Graphics::ScopedSaveState clip(g);g.reduceClipRegion(gridArea.toNearestInt());
        // Rows of the black keys a shade darker, octaves (C) marked by a stronger line.
        if(!laneView())for(int n=lowNote;n<=highNote;++n){float y=48+(highNote-n)*rowHeight();if(blackKey(n)){g.setColour(juce::Colours::black.withAlpha(.22f));g.fillRect(left,y,(float)getWidth()-left,rowHeight());}
            if(highlightScale&&noteInScale(n)){g.setColour(Daw::accent.withAlpha((n-owner.project.settings.key+120)%12==0?.18f:.10f));g.fillRect(left,y,(float)getWidth()-left,rowHeight());}
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
    for(size_t i=0;i<notes.size();++i){const auto& n=notes[i];auto r=juce::Rectangle<float>(left+(float)(n.start*pixelsPerBeat),noteY(n),noteWidth(n),noteHeight()).reduced(.5f,1.f);if(!laneView()&&(n.pitch<lowNote||n.pitch>highNote))continue;if(!r.intersects(dirtyArea.toFloat()))continue;const auto ink=owner.selectedTrack==3?guitarColour(n.lane):colour(owner.selectedTrack);const bool chosen=selected.count((int)i)>0;
        if(laneView()&&n.velocity<55)r=r.withSizeKeepingCentre(ownerTrackIsDrum()?r.getWidth():r.getWidth()*.45f,r.getHeight()*.4f);
        // Velocity reads as the note's brightness.
        g.setColour(Daw::well.interpolatedWith(ink,.55f+.45f*n.velocity/127.f));g.fillRoundedRectangle(r,2.5f);
        if(r.getHeight()>5){g.setColour(juce::Colours::white.withAlpha(.18f));g.fillRect(r.getX()+2,r.getY()+.5f,std::max(0.f,r.getWidth()-4),1.f);}
        g.setColour(chosen?Daw::text:juce::Colours::black.withAlpha(.55f));g.drawRoundedRectangle(r,2.5f,chosen?1.6f:.8f);
        if(laneView()&&n.velocity>=115){g.setColour(Daw::text.withAlpha(.35f));g.fillRoundedRectangle(r.withHeight(3).reduced(2,0).translated(0,2),1.5f);}
        if(!laneView()&&r.getWidth()>34&&r.getHeight()>=9){g.setColour(Daw::onAccent);g.setFont(Daw::font(std::min(10.f,r.getHeight()-1),true));g.drawText(juce::MidiMessage::getMidiNoteName(n.pitch,true,true,4),r.reduced(4,0),juce::Justification::centredLeft);}
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
        const auto& names=owner.cachedChordNames();const double chordBeats=owner.project.settings.chordSteps*.25;
        for(double b=0;b<owner.clipLength();b+=chordBeats){const int x=leftMargin()+(int)(b*pixelsPerBeat);if(x+(int)(chordBeats*pixelsPerBeat)<view.getX()+left||x>view.getRight())continue;auto r=juce::Rectangle<int>(x+1,3,(int)(chordBeats*pixelsPerBeat)-2,19);g.setColour(Daw::control);g.fillRoundedRectangle(r.toFloat(),Daw::radius);g.setColour(Daw::accent);g.fillRoundedRectangle(r.toFloat().withWidth(3.f),1.f);label(g,utf(names[(size_t)((int)((owner.clipStart()+b)/chordBeats)%names.size())]),r.reduced(8,0),11,Daw::text);const double local=owner.audio.position()-owner.clipStart();const bool currentBar=local>=b&&local<b+4.;g.setFont(Daw::font(Daw::small,true));g.setColour(currentBar?Daw::accent:Daw::textDim);g.drawText(juce::String((int)(b/4)+1),x+4,24,50,14,juce::Justification::centredLeft);}
    }
    g.setColour(juce::Colours::black.withAlpha(.6f));g.drawHorizontalLine(39,(float)vx,(float)view.getRight());
    g.setColour(Daw::chrome);g.fillRect(vx,0,leftMargin(),40);
    g.setColour(Daw::chrome);g.fillRect(vx,(int)velocityTop(),view.getWidth(),getHeight()-(int)velocityTop());
    // Three guides (0, 64, 127) make the lane readable without a numeric scale.
    for(int v:{0,64,127}){const float y=velocityTop()+velocityHeight()*(1.f-v/127.f);g.setColour(juce::Colours::white.withAlpha(v==64?.09f:.05f));g.drawHorizontalLine((int)y,left,(float)view.getRight());}
    label(g,"VELOCITY",{vx+6,(int)velocityTop(),leftMargin()-8,(int)velocityHeight()},10,Daw::textDim);
    g.setColour(juce::Colours::black.withAlpha(.6f));g.drawHorizontalLine((int)velocityTop(),(float)vx,(float)view.getRight());
    if(ownerTrackIsDrum())for(double b=0;b<owner.clipLength();b+=grid){const float x=left+(float)(b*pixelsPerBeat);if(x<view.getX()+left||x>view.getRight())continue;auto column=juce::Rectangle<float>(x+1,velocityTop()+8,std::max(3.f,(float)(pixelsPerBeat*grid)-3),velocityHeight()-4);g.setColour(juce::Colours::white.withAlpha(((int)(b*4)/4)%2==0?.05f:.03f));g.fillRoundedRectangle(column,Daw::radius);}
    for(size_t i=0;i<notes.size();++i){const auto& n=notes[i];const float x=left+(float)(n.start*pixelsPerBeat);if(x<vx+left||x>view.getRight())continue;float h=n.velocity/127.f*velocityHeight();g.setColour(selected.count((int)i)?Daw::text:colour(owner.selectedTrack).withAlpha(.85f));g.fillRect(x-1.f,getHeight()-12-h,2.f,h);g.fillEllipse(x-3,getHeight()-15-h,6,6);}
    const auto local=owner.audio.position()-owner.clipStart();if(local>=0&&local<owner.clipLength()){float x=std::round(left+(float)(local*pixelsPerBeat));if(x>vx+left){g.setColour(Daw::playhead.withAlpha(.9f));g.fillRect(x,24.f,1.f,(float)getHeight()-24);}}
}
void PianoRoll::editVelocity(juce::Point<float> point){
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
void PianoRoll::mouseDown(const juce::MouseEvent& e){
    grabKeyboardFocus();editingVelocity=false;if(e.x<owner.pianoPort.getViewArea().getX()+leftMargin())return;
    if(e.y<40){owner.seekTimeline(owner.clipStart()+std::clamp(beat(e.position.x),0.,owner.clipLength()));return;}
    if(e.y>=velocityTop()){velocityCheckpoint=false;editingVelocity=true;editVelocity(e.position);return;}
    if(laneView()&&e.y>=40+(owner.selectedTrack==3?12:1)*40)return;
    if(!laneView()&&e.y<48)return;
    int index=hit(e.position);if(tool==1&&index<0&&!e.mods.isPopupMenu()){selecting=true;anchor=e.position;marquee={};if(!e.mods.isShiftDown())selected.clear();repaint();return;}if(e.mods.isPopupMenu()||tool==2){erasing=true;eraseCheckpoint=false;eraseAt(e.position);return;}
    gestureOriginal=owner.clip().notes;anchor=e.position;resizing=false;velocityDrag=e.mods.isAltDown();before=owner.clip().notes;
    if(index>=0){if(e.mods.isShiftDown()){if(selected.count(index))selected.erase(index);else selected.insert(index);}else if(!selected.count(index)){selected.clear();selected.insert(index);if(laneView()&&owner.selectedTrack==3){const auto hitNote=before[(size_t)index];for(size_t k=0;k<before.size();++k)if(before[k].lane==hitNote.lane&&std::abs(before[k].start-hitNote.start)<.18)selected.insert((int)k);}}const auto& n=before[(size_t)index];resizing=(!laneView()||ownerTrackIsDrum())&&e.x>=leftMargin()+n.start*pixelsPerBeat+noteWidth(n)-std::min(6.f,noteWidth(n)*.4f)&&!velocityDrag;}
    else{if(!e.mods.isShiftDown())selected.clear();double start=std::floor(beat(e.position.x)/grid)*grid;if(start>=owner.clipLength())return;if(laneView()&&owner.selectedTrack==3){const int lane=laneAt(e.position.y);const auto& settings=owner.project.settings;const double chordBeats=settings.chordSteps*.25;const int slot=((int)((owner.clipStart()+start)/chordBeats))%settings.chordCount;auto chord=bandChord(settings.key,settings.scale,settings.degrees[(size_t)slot],settings.freeChords[(size_t)slot],(sgk::Extension)settings.extension);auto notes=fgg::laneNotes((fgg::Lane)lane,fgg::tonesOf(chord),(int)(start*4)%16);for(int note:notes){owner.clip().notes.push_back({start,std::min(grid,owner.clipLength()-start),note,brushVelocity,lane});selected.insert((int)owner.clip().notes.size()-1);}}
        else{owner.clip().notes.push_back({start,std::min(ownerTrackIsDrum()?defaultDrumNoteLength(owner.selectedTrack):grid,owner.clipLength()-start),pitch(e.position.y),ownerTrackIsDrum()?brushVelocity:(int)owner.velocitySlider.getValue(),ownerTrackIsDrum()?owner.selectedTrack-4:0});selected.insert((int)owner.clip().notes.size()-1);}if(laneView()&&flamBrush&&owner.clip().notes.size()>before.size()){auto note=owner.clip().notes.back();note.start=std::max(0.,note.start-.08);note.length=.04;note.velocity=45;owner.clip().notes.push_back(note);}before=owner.clip().notes;}
    if(index>=0)previewSelection();
    repaint();
}
void PianoRoll::eraseAt(juce::Point<float> point){
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
void PianoRoll::mouseUp(const juce::MouseEvent&){
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
void PianoRoll::deleteSelected(){if(selected.empty())return;owner.checkpoint();auto& notes=owner.clip().notes;for(auto i=selected.rbegin();i!=selected.rend();++i)if(*i>=0&&*i<(int)notes.size())notes.erase(notes.begin()+*i);selected.clear();owner.changed();}
bool PianoRoll::keyPressed(const juce::KeyPress& key){if(owner.editingRestricted())return owner.keyPressed(key);if(editKeys(key))return true;if(key==juce::KeyPress::deleteKey||key==juce::KeyPress::backspaceKey){deleteSelected();return true;}if(key.getModifiers().isCommandDown()&&key.getKeyCode()=='C'){owner.copyButton.onClick();return true;}if(key.getModifiers().isCommandDown()&&key.getKeyCode()=='V'){owner.pasteButton.onClick();return true;}if(key.getModifiers().isCommandDown()&&key.getKeyCode()=='A'){selected.clear();for(size_t i=0;i<owner.clip().notes.size();++i)selected.insert((int)i);repaint();return true;}return owner.keyPressed(key);}

class MainComponent::InstrumentWindow : public juce::DocumentWindow {
public:
    struct Body : juce::Component {
        MainComponent& owner;
        juce::ComboBox section,advanced;
        juce::TextButton applyTrack{"APPLY TRACK"},applyClip{"APPLY CLIP"},edit{"EDIT MIDI"};
        Body(MainComponent& m):owner(m){
            for(auto* c:std::initializer_list<juce::Component*>{&section,&advanced,&applyTrack,&applyClip,&edit,&m.instrumentPort})addAndMakeVisible(c);
            section.onChange=[this]{if(section.getSelectedId()>0)owner.select(owner.selectedTrack,section.getSelectedId()-1);};
            advanced.onChange=[this]{auto names=owner.instrumentPanel.sectionNames();const int index=advanced.getSelectedItemIndex();if(index>=0&&index<names.size())owner.instrumentPort.setViewPosition(0,owner.instrumentPanel.sectionPosition(names[index]));};
            applyTrack.onClick=[this]{owner.regenerate(Scope::track,false);};applyClip.onClick=[this]{owner.regenerate(Scope::clip,false);};edit.onClick=[this]{owner.showEditor();};
            section.setTooltip(tr("Choose the song section to edit."));advanced.setTooltip(tr("Jump to the instrument controls."));
            applyTrack.setTooltip(controlTip("APPLY TRACK"));applyClip.setTooltip(controlTip("APPLY CLIP"));edit.setTooltip(controlTip("EDIT MIDI"));edit.getProperties().set("fgaIconOnly",true);edit.setTitle(tr("EDIT MIDI"));
        }
        void refresh(){
            section.clear(juce::dontSendNotification);for(const auto& part:owner.project.sections)section.addItem(utf(part.name),section.getNumItems()+1);section.setSelectedId(owner.selectedSection+1,juce::dontSendNotification);
            advanced.clear(juce::dontSendNotification);for(const auto& name:owner.instrumentPanel.sectionNames())advanced.addItem(tr(name),advanced.getNumItems()+1);advanced.setTextWhenNothingSelected(tr("ADVANCED"));
            applyTrack.setEnabled(hasGenerator(owner.project.tracks[(size_t)owner.selectedTrack]));applyClip.setEnabled(hasGenerator(owner.project.tracks[(size_t)owner.selectedTrack]));translateControls(*this);
        }
        void paint(juce::Graphics& g)override{g.fillAll(Daw::surface);g.setColour(Daw::line);g.drawHorizontalLine(43,0,(float)getWidth());}
        void resized()override{const int unit=std::max(100,(getWidth()-32)/5);section.setBounds(8,8,unit,28);advanced.setBounds(16+unit,8,unit,28);applyTrack.setBounds(24+2*unit,8,unit-8,28);applyClip.setBounds(24+3*unit,8,unit-8,28);edit.setBounds(24+4*unit,8,unit-8,28);owner.instrumentPort.setBounds(0,44,getWidth(),std::max(0,getHeight()-44));}
    };
    MainComponent& owner;Body* body;
    InstrumentWindow(MainComponent& m):DocumentWindow(tr("Instrument panel"),Daw::surface,DocumentWindow::closeButton),owner(m),body(new Body(m)){
        setLookAndFeel(&m.theme);setUsingNativeTitleBar(true);setAlwaysOnTop(true);setContentOwned(body,false);
        setResizable(true,false);setResizeLimits(780,420,2880,1960);centreWithSize(1200,760);
    }
    ~InstrumentWindow()override{clearContentComponent();setLookAndFeel(nullptr);}
    void refresh(){body->refresh();setName(tr("Instrument panel")+" / "+utf(owner.project.tracks[(size_t)owner.selectedTrack].name));}
    // Each instrument panel reports its own height; the window follows it instead of
    // leaving a large dead area under short panels. The centre is kept, otherwise the
    // window grows downwards and looks off-centre, and the height is clamped to the
    // display so a tall panel cannot fall off the screen.
    void fitToContent(){
        const auto* display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(getScreenBounds());
        const int maxHeight=display?std::max(420,display->userArea.getHeight()-40):900;
        const int content=44+owner.instrumentPanel.getHeight()+2;
        const int height=juce::jlimit(420,maxHeight,content);
        if(height==lastFit)return;lastFit=height;
        const auto centre=getBounds().getCentre();
        setSize(getWidth(),height);
        setCentrePosition(centre);
    }
private:
    int lastFit=-1;
public:
    void closeButtonPressed()override{owner.instrumentPanel.flushRender();owner.dismissAuxiliaryWindow(*this);}
    bool keyPressed(const juce::KeyPress& key)override{return owner.keyPressed(key);}
};
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
    juce::PopupMenu menu;menu.addItem(1,tr("Add MIDI track"));menu.addItem(2,tr("VOCAL CHOPS"));
    juce::Component::SafePointer<MainComponent> safe(this);menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&addTrackButton),[safe](int id){if(safe&&id>0)safe->addMidiTrack(id==2);});
}
void MainComponent::addMidiTrack(bool vocal){
    if(editingRestricted()||loadingPlugin||project.tracks.size()>=maxTracks)return;
    instrumentPanel.flushRender();checkpoint();const int track=vocal?project.addVocalChopsTrack():project.addMidiTrack();if(track<0)return;
    select(track,0);changed();showEditor();if(vocal)message(tr("Load a voice sample, then shape its slices in Studio. REGENERATE writes another sparse vocal phrase."));
}
class MainComponent::AutosaveJob : public juce::Thread {
public:
    AutosaveJob(Project p,juce::File file,std::function<void(juce::Result)> finished):Thread("Autosave"),snapshot(std::move(p)),target(file),complete(std::move(finished)){}
    ~AutosaveJob()override{signalThreadShouldExit();stopThread(-1);}
    void run()override{if(threadShouldExit())return;auto result=saveProject(snapshot,target);if(!threadShouldExit())juce::MessageManager::callAsync([callback=complete,result]{callback(result);});}
private:Project snapshot;juce::File target;std::function<void(juce::Result)> complete;
};
class MainComponent::PluginWindow : public juce::DocumentWindow,private juce::AudioProcessorListener {
public:
    MainComponent& owner;juce::AudioPluginInstance& processor;
    std::uint64_t session;int slot,track;
    PluginWindow(MainComponent& m,juce::AudioPluginInstance& p,std::uint64_t id,int effectSlot,int trackIndex):DocumentWindow(p.getName(),Palette::panel,DocumentWindow::closeButton),owner(m),processor(p),session(id),slot(effectSlot),track(trackIndex){const juce::ScopedLock processorLock(p.getCallbackLock());setUsingNativeTitleBar(true);setAlwaysOnTop(true);auto* editor=p.createEditorIfNeeded();if(!editor)editor=new juce::GenericAudioProcessorEditor(p);setContentOwned(editor,true);processor.addListener(this);editor->addMouseListener(this,true);markState();setResizable(editor->isResizable(),false);setSize(editor->getWidth(),editor->getHeight());setVisible(true);
        // The native peer can size the window while showing it; centre on the next
        // message-loop turn, when the final size is known.
        juce::Component::SafePointer<PluginWindow> safe(this);
        juce::MessageManager::callAsync([safe]{if(safe)safe->centreWithSize(safe->getWidth(),safe->getHeight());});}
    void markState()noexcept{owner.audio.invalidatePluginState(track,slot);}
    void activeWindowStatusChanged()override{
        if(isActiveWindow()){owner.lastActivePluginSession=session;owner.lastActivePluginSlot=slot;}else markState();
        juce::DocumentWindow::activeWindowStatusChanged();
    }
    void audioProcessorParameterChanged(juce::AudioProcessor*,int,float)override{markState();}
    void audioProcessorChanged(juce::AudioProcessor*,const juce::AudioProcessorListener::ChangeDetails&)override{markState();}
    void mouseUp(const juce::MouseEvent& e)override{markState();juce::DocumentWindow::mouseUp(e);}
    ~PluginWindow()override{const juce::ScopedLock processorLock(processor.getCallbackLock());processor.removeListener(this);if(auto* editor=getContentComponent())editor->removeMouseListener(this);clearContentComponent();markState();}
    void closeButtonPressed()override{const juce::ScopedLock processorLock(processor.getCallbackLock());markState();owner.dismissAuxiliaryWindow(*this);}
};
void MainComponent::openHostedEditor(int track,int slot){
    if(loadingPlugin||editingRestricted()||!validEffectTrack(track))return;
    auto* plugin=slot==0?audio.plugin(track):audio.effectPlugin(track,slot);if(!plugin)return;const auto session=effectTrack(track).sessionId;
    pluginWindow=nullptr;for(auto& window:pluginWindows)if(window->session==session&&window->slot==slot){pluginWindow=window.get();break;}
    if(!pluginWindow){auto window=std::make_unique<PluginWindow>(*this,*plugin,session,slot,track);pluginWindow=window.get();pluginWindows.push_back(std::move(window));}
    else if(!pluginWindow->isVisible()){pluginWindow->setVisible(true);juce::Component::SafePointer<PluginWindow> safe(pluginWindow);juce::MessageManager::callAsync([safe]{if(safe&&safe->isVisible())safe->centreWithSize(safe->getWidth(),safe->getHeight());});}
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
void settleNativeFocus(juce::Component& component){
   #if JUCE_MAC
    // Cocoa delivers key-window activation through its event loop. Let those
    // native notifications arrive before injecting the next synthetic click.
    const auto until=juce::Time::getMillisecondCounterHiRes()+200.;
    while(!component.hasKeyboardFocus(false)&&juce::Time::getMillisecondCounterHiRes()<until)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
   #else
    juce::ignoreUnused(component);
   #endif
}
}
bool MainComponent::verifyWindowFocus(const juce::File& folder){
#if !FGA_SELF_TESTS
return false;
#else
    if(!folder.createDirectory())return false;
    const auto saved=project;const auto undos=undoStack,redos=redoStack;const bool wasDirty=dirty;
    bool ok=true;juce::String details;
    auto check=[&](const char* name,juce::DocumentWindow& window){
        window.grabKeyboardFocus();
        // Reproduce a missed mouse-up when the native tool window disappears. The
        // press is injected inside the peer, where JUCE attributes it to the window.
        auto source=juce::Desktop::getInstance().getMainMouseSource();
        if(auto* peer=window.getPeer())peer->handleMouseEvent(source.getType(),{4.f,4.f},juce::ModifierKeys::leftButtonModifier,juce::MouseInputSource::defaultPressure,0,juce::Time::currentTimeMillis());
        const bool pressed=source.isDragging();auto* down=source.getComponentUnderMouse();const bool owned=down&&(down==&window||window.isParentOf(down));const bool physicalDown=physicalMouseModifiers().isAnyMouseButtonDown();window.closeButtonPressed();
        const bool released=!source.isDragging();settleNativeFocus(*this);
        const bool focused=!window.isVisible()&&isEnabled()&&hasKeyboardFocus(false);
        const auto before=undoStack.size();const auto seed=project.settings.seed;
        const auto pos=getTopLevelComponent()->getLocalPoint(&songButton,songButton.getLocalBounds().getCentre()).toFloat();
        if(auto* peer=getPeer()){
            peer->handleMouseEvent(source.getType(),pos,juce::ModifierKeys::leftButtonModifier,juce::MouseInputSource::defaultPressure,0,juce::Time::currentTimeMillis());
            peer->handleMouseEvent(source.getType(),pos,{},juce::MouseInputSource::defaultPressure,0,juce::Time::currentTimeMillis());
        }
        // Generation is disabled on purpose while a take is running, so a generated
        // seed can only be required when editing is actually available.
        const bool expectGeneration=!editingRestricted();
        const bool generated=undoStack.size()==std::min(size_t(40),before+1)&&project.settings.seed!=seed;
        const bool clicked=expectGeneration?generated:true;
        details+=juce::String(pressed&&owned&&released&&focused&&clicked?"PASS ":"FAIL ")+name+": pressed="+juce::String((int)pressed)+", owned="+juce::String((int)owned)+", physical-buttons="+juce::String((int)physicalDown)+", source-released="+juce::String((int)released)+", focus="+juce::String((int)focused)+", first-click generation="+juce::String((int)generated)+(expectGeneration?"":" (generation disabled during take)")+"\n";
        ok=pressed&&owned&&released&&focused&&clicked&&ok;if(generated)undo(false);
    };
    showArrangement();
    for(int repeat=0;repeat<3;++repeat){
        showStudio();audio.startRecording(0,0);updateEditingAvailability();check("Studio (including active take)",*studioWindow);audio.stop();updateEditingAvailability();
        showMixerRouting();check("Mixer",*mixerRoutingWindow);
        showAiComposer();check("AI",*aiWindow);
        showInstrument();check("Instrument",*instrumentWindow);
    }
    // Recovery must cancel a hidden pressed button without firing its action,
    // and must leave a gesture in a different visible window alone.
    juce::TextButton probeButton("Input recovery probe");int actions=0;probeButton.onClick=[&]{++actions;};
    juce::DocumentWindow probe("Input recovery",Daw::base,0);probe.setUsingNativeTitleBar(true);probe.setContentNonOwned(&probeButton,false);probe.centreWithSize(160,64);probe.setVisible(true);
    auto source=juce::Desktop::getInstance().getMainMouseSource();
    auto probeEvent=[&](juce::ModifierKeys mods){if(auto* peer=probe.getPeer())peer->handleMouseEvent(source.getType(),{80.f,32.f},mods,juce::MouseInputSource::defaultPressure,0,juce::Time::currentTimeMillis());};
    probeEvent(juce::ModifierKeys::leftButtonModifier);const bool down=source.isDragging()&&probeButton.isDown();dismissAuxiliaryWindow(probe);
    const bool cancelled=down&&!source.isDragging()&&actions==0;ok=cancelled&&ok;
    details+=juce::String(cancelled?"PASS ":"FAIL ")+"stale release cancels hidden button without invoking its action\n";
    probe.setVisible(true);showMixerRouting();probeEvent(juce::ModifierKeys::leftButtonModifier);const bool begun=source.isDragging();mixerRoutingWindow->closeButtonPressed();
    const bool kept=source.isDragging()&&actions==0;probeEvent({});
    const bool independent=begun&&kept&&!source.isDragging();ok=independent&&ok;
    details+=juce::String(independent?"PASS ":"FAIL ")+"closing a tool preserves a gesture in another visible window (begun="+juce::String((int)begun)+", retained="+juce::String((int)kept)+")\n";
    dismissAuxiliaryWindow(probe);probe.clearContentComponent();
    showMixerRouting();setEnabled(false);mixerRoutingWindow->closeButtonPressed();
    const bool guarded=!isEnabled()&&!mixerRoutingWindow->isVisible();ok=guarded&&ok;
    details+=juce::String(guarded?"PASS ":"FAIL ")+"closing a tool preserves an independently disabled DAW\n";setEnabled(true);
    showArrangement();project=saved;undoStack=undos;redoStack=redos;changed(false);dirty=wasDirty;
    folder.getChildFile("window-focus.txt").replaceWithText(details);return ok;
#endif
}
bool MainComponent::verifyAuditRepairs(const juce::File& folder){
#if !FGA_SELF_TESTS
return false;
#else
    const auto saved=project;const auto savedUndo=undoStack,savedRedo=redoStack;const auto savedFile=projectFile;
    const bool savedDirty=dirty,savedDue=autosaveDue;const int savedTicks=ticks,oldTrack=selectedTrack,oldSection=selectedSection;
    const auto oldPage=page;bool ok=true;juce::String details;
    auto stage=[&](const char* label,bool passed){ok=passed&&ok;details+=juce::String(passed?"PASS ":"FAIL ")+label+"\n";};
    audio.stop();autosaveDue=false;ticks=0;undoStack.clear();redoStack.clear();
    project=newProject(1);selectedTrack=selectedSection=0;selectedClip=-1;noClipSelection=false;editCursor=-1;restoreInstruments();showArrangement();
    juce::AudioBuffer<float> block(2,512);juce::AudioIODeviceCallbackContext context;
    auto tick=[&]{audio.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);};
    auto beginTake=[&]{audio.stop();audio.seek(0);const bool started=audio.startRecording(0,0).wasOk();audio.liveMidi(0,juce::MidiMessage::noteOn(1,61,.8f));for(int i=0;i<16;++i)tick();audio.liveMidi(0,juce::MidiMessage::noteOff(1,61));tick();return started;};
    stage("take starts",beginTake());updateEditingAvailability();
    stage("recording keeps Main, Stop and Studio active while generation is disabled",isEnabled()&&stopButton.isEnabled()&&studioButton.isEnabled()&&!songButton.isEnabled());
    stopButton.onClick();stage("main Stop finalizes the take and restores editing",!audio.isRecording()&&!audio.isAudioRecording()&&!editingRestricted()&&songButton.isEnabled());
    stage("second take starts",beginTake());const auto checkpoints=undoStack.size();int accepted=0;
    closePending=true;completeCloseChoice(0,[&]{++accepted;});
    stage("Cancel close preserves the active take and Undo",audio.isRecording()&&accepted==0&&undoStack.size()==checkpoints&&!closePending);
    closePending=true;finishPendingSave({},[&]{++accepted;});
    stage("Cancel Save As preserves recording and cancels the continuation",audio.isRecording()&&accepted==0&&undoStack.size()==checkpoints&&!closePending);
    projectFile=juce::File();closePending=true;finishPendingSave(folder.getChildFile("missing-parent/failed.fga"),[&]{++accepted;});
    stage("failed save keeps unsaved work and does not continue",accepted==0&&dirty&&!audio.isRecording());
    stage("third take starts",beginTake());const auto destination=folder.getChildFile("close-saved.fga");closePending=true;
    finishPendingSave(destination,[&]{++accepted;});Project loaded;
    stage("successful first save writes the completed take and continues exactly once",accepted==1&&!dirty&&!audio.isRecording()&&projectFile==destination&&loadProject(destination,loaded).wasOk()&&std::any_of(loaded.tracks[0].clips[0].notes.begin(),loaded.tracks[0].clips[0].notes.end(),[](const Note& n){return n.pitch==61;}));
    audio.stop();dirty=false;requestClose([&]{++accepted;});stage("clean navigation continues immediately",accepted==2);
    const auto associated=folder.getChildFile("project prova con spazi.FGA");stage("project association fixture",destination.copyFileTo(associated));
    stage("project paths with spaces and uppercase extension",projectInCommandLine(associated.getFullPathName().quoted())==associated&&projectInCommandLine(associated.getFullPathName())==associated);
    const bool requestedOpen=openFromCommandLine(associated.getFullPathName().quoted());
    stage("project association shows loading before restoring the file",requestedOpen&&loadingView.isVisible());
    const auto openDeadline=juce::Time::getMillisecondCounterHiRes()+5000.;
    while(loadingView.isVisible()&&juce::Time::getMillisecondCounterHiRes()<openDeadline)juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    stage("project association opens the saved arrangement",requestedOpen&&projectFile==associated&&midiFile(project)==midiFile(loaded)&&!loadingView.isVisible());
    // Drop onto the visible Snare row while another instrument is selected.
    showArrangement();arrangement.pixelsPerBar=24;updateSizes();select(0,0);arrangementPort.setViewPosition(0,0);
    auto wavFile=folder.getChildFile("drop-routing.wav");juce::WavAudioFormat wav;
    {std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(wavFile.createOutputStream().release(),44100,1,16,{},0));juce::AudioBuffer<float> signal(1,1024);for(int i=0;i<1024;++i)signal.setSample(0,i,.2f*std::sin(i*.07f));if(writer)writer->writeFromAudioSampleBuffer(signal,0,1024);else stage("drop WAV fixture",false);}
    const auto drop=getLocalPoint(&arrangement,juce::Point<int>(216+72,arrangement.rowY(1)+10));audio.seek(5.25);audio.play(true);const double position=audio.position();
    filesDropped({wavFile.getFullPathName()},drop.x,drop.y);
    stage("sample drop targets its row without seeking or stopping",selectedTrack==5&&project.tracks[5].instrument=="sample"&&project.tracks[0].instrument=="internal"&&audio.position()==position&&audio.isPlaying());
    auto midiProject=project;for(auto& t:midiProject.tracks)for(auto& c:t.clips)c.notes.clear();midiProject.tracks[0].clips[0].notes={{0,.5,69,100,0}};
    const auto bytes=midiFile(midiProject,0);const auto midi=folder.getChildFile("drop-routing.mid");midi.replaceWithData(bytes.data(),bytes.size());select(0,0);arrangementPort.setViewPosition(0,0);
    filesDropped({midi.getFullPathName()},drop.x,drop.y);
    stage("MIDI drop targets its row without seeking",selectedTrack==5&&audio.position()==position&&audio.isPlaying()&&project.tracks[5].clips[0].notes.size()==1&&project.tracks[5].clips[0].notes[0].pitch==69);
    project.tracks[5].freeTimeline=true;project.tracks[5].clips.clear();changed();filesDropped({midi.getFullPathName()},drop.x,drop.y);
    stage("MIDI drop on an empty lane creates a clip at the drop beat",project.tracks[5].clips.size()==1&&std::abs(project.tracks[5].clips[0].position-12)<1.e-6&&audio.position()==position);
    audio.stop();select(0,0);showEditor();clip().notes={{0,.25,60,90,0},{2,.25,64,95,0}};piano.fitNotes();pianoPort.setViewPosition(0,0);
    const auto mouse=juce::Desktop::getInstance().getMainMouseSource();const auto now=juce::Time::getCurrentTime();
    auto event=[&](juce::Point<float> at){return juce::MouseEvent(mouse,at,juce::ModifierKeys::leftButtonModifier,1,0,0,0,0,&piano,&piano,now,at,now,1,false);};
    auto count=undoStack.size();dirty=false;const auto empty=juce::Point<float>(piano.leftMargin()+(float)piano.pixelsPerBeat,piano.velocityTop()+piano.velocityHeight()*.5f);
    piano.mouseDown(event(empty));piano.mouseUp(event(empty));stage("empty velocity click leaves Undo and dirty unchanged",undoStack.size()==count&&!dirty);
    const auto first=clip().notes[0],second=clip().notes[1];piano.tool=2;
    auto notePoint=[&](const Note& n){return juce::Point<float>(piano.leftMargin()+(float)(n.start*piano.pixelsPerBeat)+2,piano.noteY(n)+piano.noteHeight()*.5f);};
    piano.mouseDown(event(notePoint(first)));piano.mouseDrag(event(notePoint(second)));piano.mouseUp(event(notePoint(second)));
    stage("eraser drag removes multiple notes as one Undo gesture",clip().notes.empty()&&undoStack.size()==count+1);undo(false);stage("one Undo restores all erased notes",clip().notes.size()==2);piano.tool=0;
    audio.stop();tick();dirty=true;autosaveDue=true;audio.play(true);const auto autosave=folder.getChildFile("autosave-snapshot.fga");autosave.deleteFile();autosaveIfIdle(autosave);
    stage("autosave is deferred while transport plays",autosaveDue&&!autosaveJob&&!autosave.existsAsFile());audio.stop();tick();const auto snapshotName=project.name;autosaveIfIdle(autosave);project.name="Edited after snapshot";
    const bool exited=autosaveJob&&autosaveJob->waitForThreadToExit(10000);Project snapshot;
    stage("idle autosave writes an independent snapshot without clearing dirty",exited&&loadProject(autosave,snapshot).wasOk()&&snapshot.name==snapshotName&&project.name=="Edited after snapshot"&&dirty);
    for(int i=0;i<20&&autosaveJob;++i)juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
    if(autosaveJob){++autosaveRequestId;retiredJobs.retire(std::move(autosaveJob));}
    audio.stop();project=saved;projectFile=savedFile;undoStack=savedUndo;redoStack=savedRedo;dirty=savedDirty;autosaveDue=savedDue;ticks=savedTicks;closePending=false;
    selectedTrack=oldTrack;selectedSection=oldSection;selectedClip=-1;noClipSelection=false;restoreInstruments();updateEditingAvailability();refreshControls();if(oldPage==Page::editor)showEditor();else showArrangement();
    folder.getChildFile("audit-repairs.txt").replaceWithText(details);return ok;
#endif
}
bool MainComponent::verifyPluginWindow(const juce::File& folder){
#if !FGA_SELF_TESTS
return false;
#else
    select(0,0);trackInspector.refresh();auto* direct=dynamic_cast<juce::Button*>(trackInspector.findChildWithID("inspector-plugin-editor"));if(direct)direct->onClick();
    if(!pluginWindow)return false;
    juce::String evidence;bool ok=true;
    auto check=[&](const char* name,bool passed){evidence+=juce::String(passed?"PASS ":"FAIL ")+name+"\n";ok=passed&&ok;};
    auto centred=[this]{auto* display=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();return display&&pluginWindow->getBounds().getCentre().getDistanceFrom(display->userArea.getCentre())<3;};
    auto settleCentre=[&]{const auto until=juce::Time::getMillisecondCounterHiRes()+500.;while(pluginWindow&&!centred()&&juce::Time::getMillisecondCounterHiRes()<until)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);};
    settleCentre();
    check("VST editor visible",pluginWindow->isVisible());check("VST editor always on top",pluginWindow->isAlwaysOnTop());check("VST editor centred",centred());
    pluginWindow->setTopLeftPosition(41,52);pluginWindow->closeButtonPressed();settleNativeFocus(*this);check("VST editor hidden after close",!pluginWindow->isVisible());check("DAW keyboard focus restored",hasKeyboardFocus(false));
    pluginEditorButton.onClick();settleCentre();check("VST editor reopened",hasOpenPluginEditors());check("Reopened VST editor always on top",pluginWindow&&pluginWindow->isAlwaysOnTop());check("Reopened VST editor centred",pluginWindow&&centred());
    pluginWindow->closeButtonPressed();showArrangement();select(1,0);
    const int row=rowForTrack(0);arrangementPort.setViewPosition(0,std::max(0,arrangement.rowY(row)-rulerHeight));
    const auto view=arrangementPort.getViewArea();const auto point=headerChrome(view.getX(),arrangement.rowY(row),arrangement.rowHeight(row)).gear.getCentre().toFloat();
    const juce::MouseEvent gear(juce::Desktop::getInstance().getMainMouseSource(),point,juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),1,0,0,0,0,&arrangement,&arrangement,juce::Time::getCurrentTime(),point,juce::Time::getCurrentTime(),1,false);
    arrangement.mouseMove(gear);check("VST gear tooltip follows hovered source",audio.plugin(0)&&arrangement.getTooltip()==tr("Open %plugin%").replace("%plugin%",audio.plugin(0)->getName()));
    const auto midi=midiFile(project);const auto history=undoStack.size();arrangement.mouseDown(gear);arrangement.mouseUp(gear);settleCentre();
    check("VST header gear opens native editor without MIDI or Undo changes",selectedTrack==0&&hasOpenPluginEditors()&&pluginWindow->isVisible()&&midiFile(project)==midi&&undoStack.size()==history);
    if(auto out=folder.getChildFile("plugin-inspector.png").createOutputStream()){juce::PNGImageFormat png;png.writeImageToStream(trackInspector.createComponentSnapshot(trackInspector.getLocalBounds()),*out);}
    folder.getChildFile("plugin-window.txt").replaceWithText(evidence);if(juce::SystemStats::getEnvironmentVariable("FGA_SKIP_EXPORT_UI_TEST","0")=="1")folder.getChildFile("export-ui-skipped.txt").replaceWithText("SKIP previously verified export UI; default CI runs the complete flow.");else ok=verifyExports(folder.getChildFile("export-ui"))&&ok;closePluginEditor();return ok;
#endif
}

bool MainComponent::verifyLegacyHostedProjects(const juce::File& folder,const juce::PluginDescription& description){
#if !FGA_SELF_TESTS
    juce::ignoreUnused(folder,description);return false;
#else
    juce::String report;bool ok=true;
    auto check=[&](bool pass,const juce::String& name){report+=(pass?"PASS ":"FAIL ")+name+"\n";folder.getChildFile("host-legacy.txt").replaceWithText(report);ok=pass&&ok;return pass;};
    auto* source=audio.plugin(0);if(!check(source!=nullptr,"source instrument available"))return false;
    juce::MemoryBlock native;source->getStateInformation(native);
    auto legacy=newProject(9022);legacy.sections={{Section::main,1,"Legacy VST"}};legacy.masterGain=1;legacy.normalise();
    for(auto& t:legacy.tracks){t.mute=true;t.effects.clear();t.parameters.clear();t.volume.enabled=false;for(auto& c:t.clips){c.notes.clear();c.feel={-1,-1,-1};}}
    legacy.masterAutomation.enabled=false;
    for(int ti=0;ti<3;++ti){auto& t=legacy.tracks[(size_t)ti];t.mute=false;t.gain=.5;t.instrument="plugin";t.instrumentPath=description.fileOrIdentifier.toStdString();t.instrumentId=description.createXml()->toString().toStdString();t.instrumentState=native.toBase64Encoding().toStdString();t.clips[0].notes={{0,3,69-ti*12,100,0}};}
    ParameterAutomation lane;lane.slot=0;lane.index=0;lane.name="Gain";lane.curve.enabled=true;lane.curve.points={{0,.25,0},{4,.5,0}};legacy.tracks[1].parameters.push_back(lane);
    auto reopenAndPlay=[&](const juce::File& file){
        audio.stop();dirty=false;open(file);const auto until=juce::Time::getMillisecondCounterHiRes()+15000.;
        bool sawIntermediate=false;
        while((loadingView.isVisible()||loadingPlugin)&&juce::Time::getMillisecondCounterHiRes()<until){juce::MessageManager::getInstance()->runDispatchLoopUntil(10);const int remaining=(int)restorePluginTasks.size()+loadingPlugin;sawIntermediate=sawIntermediate||(loadingView.isVisible()&&remaining>0&&remaining<restorePluginTotal);}
        check(sawIntermediate,"loading progress advances between saved plugins");
        if(!check(projectFile==file&&!loadingView.isVisible()&&loadingPlugin==0,"project and all saved instruments finish loading"))return false;
        for(int ti=0;ti<3;++ti){auto* plugin=audio.plugin(ti);if(!check(plugin&&plugin->getTotalNumOutputChannels()==4,"saved instrument retains secondary output: track "+juce::String(ti)))return false;}
        std::array<std::uint64_t,3> before{};for(int ti=0;ti<3;++ti)before[(size_t)ti]=audio.noteOnsSent(ti);
        playButton.onClick();juce::AudioBuffer<float> output(2,512);juce::AudioIODeviceCallbackContext context;float peak=0;
        for(int b=0;b<64;++b){audio.audioDeviceIOCallbackWithContext(nullptr,0,output.getArrayOfWritePointers(),2,512,context);peak=std::max(peak,output.getMagnitude(0,512));}
        bool played=audio.isPlaying()&&audio.position()>0&&std::isfinite(peak)&&peak>.001f;
        for(int ti=0;ti<3;++ti)played=played&&audio.noteOnsSent(ti)>before[(size_t)ti];
        audio.stop();return check(played,"open then Play sounds all saved instruments: "+file.getFileName());
    };
    for(int version:{1,2,3}){auto json=projectJson(legacy);json.getDynamicObject()->setProperty("version",version);const auto file=folder.getChildFile("host-legacy-v"+juce::String(version)+".fga");
        if(!check(file.replaceWithText(juce::JSON::toString(json)),"write legacy native-state fixture")||!reopenAndPlay(file))return false;
    }
    const auto saved=folder.getChildFile("host-upgraded.fga");if(!check(writeProjectFile(saved),"save upgraded hosted project")||!reopenAndPlay(saved))return false;
    for(int ti=0;ti<3;++ti)openHostedEditor(ti,0);juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
    bool allVisible=pluginWindows.size()==3;for(int ti=0;ti<3;++ti){auto* loaded=audio.plugin(ti);auto* editor=loaded?loaded->getActiveEditor():nullptr;allVisible=allVisible&&editor&&editor->getTopLevelComponent()->isVisible();}check(allVisible,"three instrument editors stay visible together");
    if(allVisible){pluginWindows.front()->closeButtonPressed();check(pluginWindows[1]->isVisible()&&pluginWindows[2]->isVisible(),"closing one instrument editor preserves the other windows");openHostedEditor(0,0);check(pluginWindows.size()==3&&pluginWindows.front()->isVisible(),"reopening an instrument reuses only its own editor");}closePluginEditor();
    source=audio.plugin(0);if(source&&source->getNumPrograms()>1){
        source->setCurrentProgram(1);source->getParameters()[0]->setValueNotifyingHost(.37f);audio.stop();
        const auto presetProject=folder.getChildFile("host-program.fga");if(!check(writeProjectFile(presetProject),"save chosen program and custom gain before playing any audio")||!reopenAndPlay(presetProject))return false;
        source=audio.plugin(0);check(source&&source->getCurrentProgram()==1&&source->getProgramName(1)=="Bright"&&std::abs(source->getParameters()[0]->getValue()-.37f)<1.e-5f,"reopening retains the selected program and its saved custom gain");
        check(audio.exportAudio(project,folder.getChildFile("host-program.wav"),0,false,[]{return false;},[](double){}).wasOk(),"export the restored instrument program");
        source->getParameters()[0]->setValueNotifyingHost(.61f);const auto undoCount=undoStack.size();
        loadSampleFile(0,folder.getChildFile("missing-sample.wav"));check(audio.plugin(0)==source&&undoStack.size()==undoCount,"failed sample replacement retains native instrument and Undo");
        loadSampleFile(0,folder.getChildFile("host-program.wav"));check(!audio.plugin(0)&&project.tracks[0].instrument=="sample"&&undoStack.size()==undoCount+1,"sample replacement creates one real Undo checkpoint");
        undo(false);const auto end=juce::Time::getMillisecondCounterHiRes()+15000.;while((loadingPlugin||loadingView.isVisible())&&juce::Time::getMillisecondCounterHiRes()<end)juce::MessageManager::getInstance()->runDispatchLoopUntil(10);
        source=audio.plugin(0);check(source&&source->getCurrentProgram()==1&&std::abs(source->getParameters()[0]->getValue()-.61f)<1.e-5f,"Undo sample replacement restores the latest VST program and parameter edit");
    }
    return ok;
#endif
}

int ArrangementView::totalHeight() const {return rowY((int)owner.project.tracks.size());}
MainComponent::MainComponent(){
    Localisation::loadSaved();for(int track=0;track<maxTracks;++track){const auto key="trackHeight"+juce::String(track);arrangement.trackHeights[(size_t)track]=std::clamp(Localisation::setting(key.toRawUTF8(),"40").getIntValue(),40,240);}if(Localisation::tipsEnabled())tooltips=std::make_unique<juce::TooltipWindow>(this,650);setLookAndFeel(&theme);setWantsKeyboardFocus(true);project=newProject();selectedTrack=4;mixerHeight=std::clamp(Localisation::setting("mixerHeight","244").getIntValue(),160,4096);inspectorWidth=std::clamp(Localisation::setting("inspectorWidth","248").getIntValue(),224,480);sampleBrowserWidth=std::max(0,Localisation::setting("sampleBrowserWidth","0").getIntValue());
    inspectorVisible=Localisation::setting("inspectorVisible","1")=="1";
    mixerVisible=Localisation::setting("mixerVisible","1")=="1";
    arrangement.pixelsPerBar=std::clamp(Localisation::setting("arrangementZoom","6").getDoubleValue(),.05,160.);
    piano.pixelsPerBeat=std::clamp(Localisation::setting("pianoZoom","76").getDoubleValue(),2.,160.);
    addAndMakeVisible(timelineGrid);timelineGrid.addItemList({"SNAP: 1 bar","SNAP: 1 beat","SNAP: 1/2 beat","SNAP: 1/4 beat","SNAP: OFF"},1);timelineGrid.setSelectedId(2);timelineGrid.setTooltip(tr("Snap clip moves, clip edges and loop locators to bars or beats. OFF permits free positioning. Existing MIDI timing is unchanged. Hold Shift to bypass snapping."));timelineGrid.onChange=[this]{const double grids[]{4,1,.5,.25,0};timelineSnap=grids[std::clamp(timelineGrid.getSelectedItemIndex(),0,4)];};
    for(juce::Component* c:std::initializer_list<juce::Component*>{&backButton,&instrumentButton,&editButton,&leadButton,&arrangeButton,&guitarNotesButton,&durationLabel,&sectionSelector,&panelSections})addAndMakeVisible(c);
    instrumentPort.setViewedComponent(&instrumentPanel,false);instrumentPort.setScrollBarsShown(true,true);addAndMakeVisible(instrumentPort);
    backButton.onClick=[this]{showArrangement();};instrumentButton.onClick=[this]{showInstrument();};
    addAndMakeVisible(addTrackButton);addTrackButton.onClick=[this]{showAddTrackMenu();};addTrackButton.getProperties().set("fgaTip","Add a handwritten MIDI track or a sparse vocal chops part for Studio slices.");editButton.onClick=[this]{showEditor();};
    addAndMakeVisible(fitNotesButton);addAndMakeVisible(scaleGuideButton);piano.showScaleGuide=Localisation::setting("pianoScaleGuide","0")=="1";scaleGuideButton.setToggleState(piano.showScaleGuide,juce::dontSendNotification);scaleGuideButton.getProperties().set("fgaTip","Show or hide the note lanes belonging to the current key and scale.");
    scaleGuideButton.onClick=[this]{piano.showScaleGuide=!piano.showScaleGuide;scaleGuideButton.setToggleState(piano.showScaleGuide,juce::dontSendNotification);Localisation::setSetting("pianoScaleGuide",piano.showScaleGuide?"1":"0");piano.repaint();};
    muteButton.setClickingTogglesState(true);soloButton.setClickingTogglesState(true);
    int toolIndex=0;for(auto* button:{&drawTool,&selectTool,&eraseTool}){addAndMakeVisible(button);button->setClickingTogglesState(true);button->setRadioGroupId(4001);const int index=toolIndex++;button->onClick=[this,index]{piano.tool=index;};}drawTool.setToggleState(true,juce::dontSendNotification);
    drawTool.setConnectedEdges(juce::Button::ConnectedOnRight);selectTool.setConnectedEdges(juce::Button::ConnectedOnLeft|juce::Button::ConnectedOnRight);eraseTool.setConnectedEdges(juce::Button::ConnectedOnLeft);
    drawTool.setComponentID("tool-draw");selectTool.setComponentID("tool-select");eraseTool.setComponentID("tool-erase");
    drawTool.getProperties().set("fgaTip","Pencil: click to draw notes, drag to move, drag the edge for duration.");selectTool.getProperties().set("fgaTip","Select: drag a rectangle to select notes, then move or edit them.");eraseTool.getProperties().set("fgaTip","Eraser: click or drag over notes to delete them.");
    addAndMakeVisible(regenerateClipButton);regenerateClipButton.setButtonText("REGENERATE");regenerateClipButton.getProperties().set("fgaIconOnly",true);regenerateClipButton.setTitle(tr("REGENERATE"));regenerateClipButton.setTooltip(tr("Regenerate the notes in this editor."));regenerateClipButton.setComponentID("editor-regenerate");regenerateClipButton.iconColour=Palette::accent;regenerateClipButton.onClick=[this]{regenerateEditedClip();};
    regenerateClipButton.getProperties().set("fgaTip","Regenerate this clip from the instrument panel settings: a new variation of the same groove. Locked clips are kept; Undo restores the notes.");
    int brushIndex=0;for(auto* button:{&brushAccent,&brushHit,&brushGhost,&brushFlam}){addAndMakeVisible(button);button->setClickingTogglesState(true);button->setRadioGroupId(4002);const int index=brushIndex++;button->onClick=[this,index]{piano.brushVelocity=index==0?127:index==2?45:100;piano.flamBrush=index==3;};button->setTooltip(tr("Choose the hit to draw, as in the Drums and Guitar editor."));button->setConnectedEdges(juce::Button::ConnectedOnLeft|juce::Button::ConnectedOnRight);}brushAccent.setConnectedEdges(juce::Button::ConnectedOnRight);brushFlam.setConnectedEdges(juce::Button::ConnectedOnLeft);brushHit.setToggleState(true,juce::dontSendNotification);
    fitNotesButton.onClick=[this]{piano.fitNotes();piano.repaint();};
    guitarNotesButton.onClick=[this]{piano.guitarNotes=!piano.guitarNotes;showEditor();};
    leadButton.setClickingTogglesState(true);leadButton.onClick=[this]{if(loadingPlugin)return;checkpoint();project.settings.guitarLead=!project.settings.guitarLead;generate(project,Scope::track,0,selectedSection);generate(project,Scope::track,3,selectedSection);changed();};
    durationLabel.getProperties().set("fgaTip","Song length follows genre, harmonic phrases and groove development.");durationLabel.setTooltip(tr("Song length follows genre, harmonic phrases and groove development."));
    arrangeButton.onClick=[this]{if(loadingPlugin)return;for(const auto& t:project.tracks){if(t.locked){message("Sblocca le tracce prima di ricostruire la struttura. GENERATE SONG mantiene la struttura e i blocchi.");return;}for(const auto& c:t.clips)if(c.locked){message("Sblocca i clip prima di ricostruire la struttura.");return;}}auto next=project;if(!rearrangeSong(next,(std::uint32_t)juce::Random::getSystemRandom().nextInt()))return;checkpoint();audio.reusePluginStates(next);audio.stop();project=std::move(next);editCursor=-1;selectedSection=0;arrangement.selectedClips.clear();noClipSelection=false;changed();showArrangement();};
    panelSections.onChange=[this]{auto names=instrumentPanel.sectionNames();int index=panelSections.getSelectedItemIndex();if(index>=0&&index<names.size())instrumentPort.setViewPosition(0,instrumentPanel.sectionPosition(names[index]));};
    sectionSelector.onChange=[this]{if(!refreshing){select(selectedTrack,sectionSelector.getSelectedItemIndex());if(page==Page::editor)showEditor();}};
    addAndMakeVisible(chordEditButton);chordEditButton.onClick=[this]{if(juce::ModifierKeys::currentModifiers.isShiftDown())showChordMenu();else showChordCirclePopover(0);};chordsLabel.setInterceptsMouseClicks(true,false);chordsLabel.setMouseCursor(juce::MouseCursor::PointingHandCursor);chordsLabel.addMouseListener(this,false);
    inspectorPort.setViewedComponent(&trackInspector,false);inspectorPort.setScrollBarsShown(true,false);addAndMakeVisible(inspectorPort);addAndMakeVisible(mixer);
    for(auto* button:{&inspectorToggle,&mixerToggle,&followButton}){addAndMakeVisible(button);button->setClickingTogglesState(true);}
    addAndMakeVisible(creditsButton);creditsButton.setTooltip(tr("ABOUT"));creditsButton.setMouseCursor(juce::MouseCursor::PointingHandCursor);creditsButton.onClick=[this]{showAbout();};
    addAndMakeVisible(quitButton);quitButton.setComponentID("app-quit");
    quitButton.getProperties().set("fgaTip","Quit the app. Unsaved changes can be saved or kept open.");
    quitButton.onClick=[]{if(auto* app=juce::JUCEApplication::getInstance())app->systemRequestedQuit();};
    addAndMakeVisible(fullScreenButton);fullScreenButton.setClickingTogglesState(false);
    fullScreenButton.onClick=[this]{applyFullScreen(!appFullScreen);};
    addAndMakeVisible(inspectorResizer);addChildComponent(browserResizer);trackInspector.addAndMakeVisible(inspectorCollapse);inspectorCollapse.setComponentID("inspector-collapse");
    inspectorCollapse.quiet=true;inspectorCollapse.getProperties().set("fgaTip","Hide the track inspector. Restore it with the inspector icon in the toolbar. Shortcut: I.");
    inspectorCollapse.onClick=[this]{inspectorVisible=!inspectorVisible;inspectorToggle.setToggleState(inspectorVisible,juce::dontSendNotification);resized();};
    addAndMakeVisible(fitSongButton);inspectorToggle.setToggleState(inspectorVisible,juce::dontSendNotification);mixerToggle.setToggleState(mixerVisible,juce::dontSendNotification);
    inspectorToggle.onClick=[this]{inspectorVisible=inspectorToggle.getToggleState();resized();};
    mixerToggle.onClick=[this]{mixerVisible=mixerToggle.getToggleState();resized();};
    followButton.onClick=[this]{followPlayhead=followButton.getToggleState();};
    fitSongButton.onClick=[this]{fitSong();};
    // Sample browser docked on the right of the arrangement (B).
    addAndMakeVisible(browserToggle);browserToggle.setClickingTogglesState(true);browserToggle.onClick=[this]{showSampleBrowser(browserToggle.getToggleState());};
    browserToggle.getProperties().set("fgaTip","Show the sample browser: your sample folders, preview and drag onto a track. Shortcut: B.");
    addChildComponent(sampleBrowser);
    sampleBrowser.onClose=[this]{showSampleBrowser(false);};
    sampleBrowser.onLoad=[this](const juce::File& file){if(loadingPlugin){message(tr("Wait for the instrument to finish loading."),true);return;}loadSampleFile(selectedTrack,file);};
    sampleBrowser.onMessage=[this](const juce::String& text,bool error){message(text,error);};
    sampleBrowser.targetName=[this]{return utf(project.tracks[(size_t)selectedTrack].name);};
    browserVisible=Localisation::setting("sampleBrowserVisible","0")=="1";browserToggle.setToggleState(browserVisible,juce::dontSendNotification);
    addAndMakeVisible(metronomeButton);metronomeButton.onClick=[this]{toggleMetronome();};
    metronomeButton.getProperties().set("fgaTip","Metronome while playing (never exported). Shortcut: C.");
    // Monospaced digits: the transport keeps a fixed width while it counts, so the bar/beat
    // readout never jitters and the surrounding LCD blocks never shift.
    positionLabel.setBorderSize(juce::BorderSize<int>(0));positionLabel.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(),Daw::display,juce::Font::bold));positionLabel.setColour(juce::Label::textColourId,Daw::text);positionLabel.setJustificationType(juce::Justification::centredLeft);positionLabel.setInterceptsMouseClicks(false,false);
    timeDisplay=Localisation::setting("timelineTimeDisplay","0")=="1";
    addAndMakeVisible(positionModeButton);positionModeButton.onClick=[this]{toggleTimeDisplay();};
    positionModeButton.getProperties().set("fgaTip","Click to switch bars and elapsed time. The musical grid and MIDI timing stay unchanged.");
    for(juce::Component* c:std::initializer_list<juce::Component*>{&timelineZoomOut,&timelineZoomIn,&timelineFitButton,&timelineZoom})addAndMakeVisible(c);
    timelineZoomOut.setComponentID("timeline-zoom-out");timelineZoomIn.setComponentID("timeline-zoom-in");timelineFitButton.setComponentID("timeline-fit");timelineZoom.setComponentID("timeline-zoom");
    timelineZoomOut.onClick=[this]{zoomAt(false,1./1.25,arrangementPort.getWidth()*.5);};timelineZoomIn.onClick=[this]{zoomAt(false,1.25,arrangementPort.getWidth()*.5);};timelineFitButton.onClick=[this]{fitSong();};timelineFitButton.setIcon(IconButton::Icon::fitWidth);
    for(juce::Component* c:std::initializer_list<juce::Component*>{&timelineZoomOut,&timelineZoomIn,&timelineFitButton,&timelineZoom})c->setWantsKeyboardFocus(false);
    timelineZoom.setRange(.05,160,.001);timelineZoom.setSkewFactorFromMidPoint(6);timelineZoom.setSliderStyle(juce::Slider::LinearHorizontal);timelineZoom.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);timelineZoom.setScrollWheelEnabled(false);
    timelineZoom.onValueChange=[this]{zoomAt(false,timelineZoom.getValue()/arrangement.pixelsPerBar,arrangementPort.getWidth()*.5);};
    timelineZoomOut.getProperties().set("fgaTip","Zoom out of the arrangement without changing MIDI.");timelineZoomIn.getProperties().set("fgaTip","Zoom into the arrangement without changing MIDI.");timelineZoom.getProperties().set("fgaTip","Arrangement zoom. Drag to change the visible song length.");timelineFitButton.getProperties().set("fgaTip","Fit the complete song to the available timeline width. Shortcut: F.");
    for(auto* b:{&zoomSelectionButton,&loopSelectionButton})addAndMakeVisible(b);
    zoomSelectionButton.onClick=[this]{zoomSelection();};loopSelectionButton.onClick=[this]{loopSelection();};
    zoomSelectionButton.getProperties().set("fgaTip","Zoom to the selected clips or notes. Shortcut: Z.");
    loopSelectionButton.getProperties().set("fgaTip","Set the playback loop to the selected clips or notes. Shortcut: Ctrl/Cmd+L.");
    inspectorToggle.getProperties().set("fgaTip","Show track volume, pan, sound assignment and clip actions.");mixerToggle.getProperties().set("fgaTip","Show the mixer. Shortcut: F3.");fitSongButton.getProperties().set("fgaTip","Fit the complete song in the arrangement view.");followButton.getProperties().set("fgaTip","Scroll the arrangement to follow playback.");fullScreenButton.getProperties().set("fgaTip","Fill the screen with the arranger, without the window title bar. Shortcut: F11.");
    arrangement.getProperties().set("fgaTip","Wheel on ruler: zoom. Ctrl/Cmd+wheel: zoom at pointer. Shift+wheel: horizontal scroll.");piano.getProperties().set("fgaTip","Wheel on ruler: zoom. Ctrl/Cmd+wheel: zoom at pointer. Shift+wheel: horizontal scroll.");
    arrangementPort.setViewedComponent(&arrangement,false);pianoPort.setViewedComponent(&piano,false);arrangementPort.setScrollBarsShown(true,true);pianoPort.setScrollBarsShown(true,true);addAndMakeVisible(arrangementPort);addAndMakeVisible(pianoPort);
    for(juce::Component* c:std::initializer_list<juce::Component*>{&newButton,&openButton,&saveButton,&saveAsButton,&exportButton,&settingsButton,&aboutButton,&helpButton,&playButton,&stopButton,&rewindButton,&undoButton,&redoButton,&loopButton,&songButton,&generateMenuButton,&trackButton,&sectionButton,&clipButton,&harmonyButton,&addSectionButton,&spotlightButton,&genreBox,&keyBox,&scaleBox,&extensionBox,&voicingBox,&progressionBox,&bassBox,&stringsBox,&tempoSlider,&swingSlider,&densitySlider,&humanizeSlider,&masterSlider,&zoomSlider,&positionLabel,&chordsLabel,&statusLabel,&soundButton,&pluginEditorButton,&sampleButton,&loadPluginButton,&gainSlider,&panSlider,&velocitySlider,&muteButton,&soloButton,&lockButton,&lockClipButton,&soundBox,&gridBox,&quantiseButton,&transposeUp,&transposeDown,&deleteNotes,&importButton,&copyButton,&pasteButton,&sectionLoopButton,&pianoPlayButton,&inspectorLabel,&editorLabel})addAndMakeVisible(c);
    // These four labels kept JUCE's 15 px default and towered over the 10.5-12 px
    // controls around them; the theme's body size keeps the chrome consistent.
    for(auto* label:{&chordsLabel,&statusLabel,&editorLabel,&durationLabel}){label->setFont(Daw::font(Daw::body));label->setMinimumHorizontalScale(.65f);}
    inspectorLabel.getProperties().set("fgaRawText",true);
    auto slider=[this](juce::Slider& s,double lo,double hi,double step,const juce::String& suffix){s.setRange(lo,hi,step);s.setSliderStyle(juce::Slider::LinearHorizontal);s.setTextBoxStyle(juce::Slider::TextBoxRight,false,64,22);s.setTextValueSuffix(suffix);s.onDragStart=[this]{checkpoint();};};
    addAndMakeVisible(automationButton);
    automationButton.onClick=[this]{showAutomationPicker(selectedTrack);};

    slider(tempoSlider,30,300,.1," BPM");slider(swingSlider,0,150,1,"% swing");slider(densitySlider,0,100,1,"% notes");slider(humanizeSlider,0,100,1,"% feel");slider(masterSlider,-60,6,.1," dB");
    for(auto* knob:{&densitySlider,&swingSlider,&humanizeSlider,&masterSlider}){knob->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);knob->setMouseDragSensitivity(180);}
    densitySlider.setComponentID("song-density");swingSlider.setComponentID("song-swing");humanizeSlider.setComponentID("song-feel");masterSlider.setComponentID("master-gain");
    slider(zoomSlider,.05,160,.05," zoom");slider(gainSlider,-60,6,.1," dB");slider(panSlider,-100,100,1," pan");slider(velocitySlider,1,127,1," vel");velocitySlider.setValue(96);
    for(size_t i=0;i<genres().size();++i)genreBox.addItem(utf(genres()[i]),(int)i+1);
    for(int i=0;i<12;++i)keyBox.addItem(sgk::spelledNote(i,9,0),i+1);
    for(int i=0;i<13;++i)scaleBox.addItem(sgk::scaleName(i),i+1);
    for(int i=0;i<11;++i){extensionBox.addItem(sgk::extensionName(i),i+1);voicingBox.addItem(sgk::voicingName(i),i+1);}
    const auto& progressions=sgk::progressionPresets();for(size_t i=0;i<progressions.size();++i)progressionBox.addItem(progressions[i].name,(int)i+1);progressionBox.setTextWhenNothingSelected("PROGRESSION");
    for(int i=0;i<sgk::numBassModes;++i)bassBox.addItem(sgk::bassModeName(i),i+1);for(int i=0;i<sgk::numStringsModes;++i)stringsBox.addItem(sgk::stringsModeName(i),i+1);
    gridBox.addItem("1/4",1);gridBox.addItem("1/8",2);gridBox.addItem("1/16",3);gridBox.addItem("1/32",4);gridBox.setSelectedId(3);
    auto setting=[this](std::function<void()> fn){if(refreshing)return;checkpoint();fn();changed(false);message("Settings updated. REGENERATE applies them to MIDI. GENERATE SONG draws a new song.");};
    genreBox.onChange=[this,setting]{setting([this]{applyGenre(project,genreBox.getSelectedItemIndex());});message("Genre ready: NEW ARRANGEMENT rebuilds the form; GENERATE SONG varies tempo, length and patterns inside this genre.");};
    keyBox.onChange=[this]{if(refreshing||loadingPlugin||editingRestricted())return;const int key=keyBox.getSelectedItemIndex();if(key<0||key==project.settings.key)return;instrumentPanel.flushRender();checkpoint();project.settings.key=key;applyInstrumentSettings(0,true);if(instrumentIsOpen())instrumentPanel.rebuild();};
    scaleBox.onChange=[this]{if(refreshing||loadingPlugin||editingRestricted())return;const int scale=scaleBox.getSelectedItemIndex();if(scale<0||scale==project.settings.scale)return;instrumentPanel.flushRender();checkpoint();project.settings.scale=scale;applyInstrumentSettings(0,true);if(instrumentIsOpen())instrumentPanel.rebuild();};
    extensionBox.onChange=[this]{if(refreshing)return;instrumentPanel.flushRender();checkpoint();project.settings.extension=extensionBox.getSelectedItemIndex();applyInstrumentSettings(0,true);if(instrumentIsOpen())instrumentPanel.rebuild();};
    voicingBox.onChange=[this]{if(refreshing||loadingPlugin||editingRestricted())return;instrumentPanel.flushRender();checkpoint();project.settings.voicing=voicingBox.getSelectedItemIndex();applyInstrumentSettings(0,true);if(instrumentIsOpen())instrumentPanel.rebuild();};
    progressionBox.onChange=[this]{if(refreshing||loadingPlugin||editingRestricted())return;instrumentPanel.flushRender();checkpoint();chooseProgression(project,progressionBox.getSelectedItemIndex());applyInstrumentSettings(0,true);if(instrumentIsOpen())instrumentPanel.rebuild();};
    bassBox.onChange=[this,setting]{setting([this]{project.settings.bassMode=bassBox.getSelectedItemIndex();});};
    stringsBox.onChange=[this,setting]{setting([this]{project.settings.stringsMode=stringsBox.getSelectedItemIndex();});};
    tempoSlider.onValueChange=[this]{if(!refreshing){project.settings.bpm=tempoSlider.getValue();changed(false);}};
    auto feel=[this](juce::Slider& control,unsigned parameter,double Settings::* member){control.onValueChange=[this,&control,parameter,member]{if(refreshing||loadingPlugin||editingRestricted())return;project.settings.*member=control.getValue()/100.;queueGlobalFeel(control,parameter);};};
    feel(densitySlider,densityFeel,&Settings::density);feel(swingSlider,swingFeel,&Settings::swing);feel(humanizeSlider,humanizeFeel,&Settings::humanize);
    masterSlider.onValueChange=[this]{if(!refreshing){project.masterGain=gainFromControl(masterSlider);mixChanged();}};
    zoomSlider.onDragStart=nullptr;zoomSlider.onValueChange=[this]{const bool editor=page==Page::editor;auto& port=editor?pianoPort:arrangementPort;const double scale=editor?piano.pixelsPerBeat:arrangement.pixelsPerBar;zoomAt(editor,zoomSlider.getValue()/scale,port.getWidth()*.5);};zoomSlider.setValue(6,juce::sendNotificationSync);
    gainSlider.onValueChange=[this]{if(!refreshing){project.tracks[(size_t)selectedTrack].gain=gainFromControl(gainSlider);mixChanged();}};
    panSlider.onValueChange=[this]{if(!refreshing){project.tracks[(size_t)selectedTrack].pan=valueFromControl(panSlider)/100.;mixChanged();}};
    velocitySlider.onDragStart=[this]{if(!piano.selected.empty())checkpoint();};velocitySlider.onValueChange=[this]{if(!refreshing&&!piano.selected.empty()){for(int i:piano.selected)if(i>=0&&i<(int)clip().notes.size())clip().notes[(size_t)i].velocity=(int)velocitySlider.getValue();changed();}};
    auto toggle=[this](juce::Button& b,bool Track::*field){b.onClick=[this,&b,field]{checkpoint();project.tracks[(size_t)selectedTrack].*field=b.getToggleState();changed(false);};};toggle(muteButton,&Track::mute);toggle(soloButton,&Track::solo);toggle(lockButton,&Track::locked);
    lockClipButton.onClick=[this]{checkpoint();clip().locked=lockClipButton.getToggleState();changed(false);};
    playButton.onClick=[this]{piano.stopPreview();if(audio.isRecording()||audio.isAudioRecording())stopPlaybackAndRecording();else togglePlayback();};stopButton.onClick=[this]{editCursor=-1;stopPlaybackAndRecording();};rewindButton.onClick=[this]{seekTimeline(0);};
    loopButton.onClick=[this]{toggleLoop();};
    loopButton.getProperties().set("fgaTip","Loop playback on or off. Shortcut: L. Ctrl/Cmd+L loops the selection; again removes it. Double-click the loop strip to clear it.");
    pianoPlayButton.setIcon(IconButton::Icon::play);pianoPlayButton.getProperties().set("fgaTip","Play only the selected clip in a loop. Click again to stop.");
    pianoPlayButton.onClick=[this]{if(!piano.selected.empty()){togglePlayback();return;}if(editingRestricted()||loadingPlugin||clipIndex()<0)return;piano.stopPreview();if(audio.isClipPlaying())audio.play(false);else{editCursor=-1;audio.playClip(selectedTrack,clipStart(),clipStart()+clipLength());}};
    sectionLoopButton.onClick=[this]{toggleClipLoop();};sectionLoopButton.getProperties().set("fgaTip","Loop the selected clip; click again to remove the loop.");
    addAndMakeVisible(studioButton);studioButton.setIcon(IconButton::Icon::studio);studioButton.getProperties().set("dawFeatured",true);Localisation::bindTip(studioButton,"Sampler, MIDI/audio recording and project samples");studioButton.onClick=[this]{showStudio();};
    addAndMakeVisible(aiComposeButton);aiComposeButton.primary=false;aiComposeButton.setButtonText("AI COMPOSER");aiComposeButton.getProperties().set("fgaTip","Compose MIDI notes and an arrangement with your selected AI service.");aiComposeButton.onClick=[this]{showAiComposer();};
    songButton.onClick=[this]{regenerate(Scope::song);};trackButton.onClick=[this]{regenerate(Scope::track,page!=Page::instrument);};sectionButton.onClick=[this]{regenerate(Scope::section);};clipButton.onClick=[this]{regenerate(Scope::clip,page!=Page::instrument);};
    // One slot for the four generation scopes: the main button still generates the song with a
    // single click, the caret opens track / section / clip. The three old buttons stay in the
    // component (their callbacks and shortcuts are unchanged) but are no longer laid out.
    generateMenuButton.onClick=[this]{showGenerateMenu();};
    generateMenuButton.getProperties().set("fgaTip","Choose what GENERATE rewrites: the song, the selected track, the section or the clip.");
    for(auto* b:{&trackButton,&sectionButton,&clipButton})b->setVisible(false);
    harmonyButton.onClick=[this]{checkpoint();randomiseHarmony(project,(std::uint32_t)juce::Random::getSystemRandom().nextInt());applyInstrumentSettings(0,true);message("New chords written into unlocked tracks. GENERATE SONG draws another harmony.");};
    undoButton.onClick=[this]{undo(false);};redoButton.onClick=[this]{undo(true);};
    quantiseButton.onClick=[this]{modifySelected([this](Clip& c){quantise(c,piano.grid,1,clipLength());});};transposeUp.onClick=[this]{modifySelected([](Clip& c){transpose(c,12);});};transposeDown.onClick=[this]{modifySelected([](Clip& c){transpose(c,-12);});};deleteNotes.onClick=[this]{piano.deleteSelected();};
    gridBox.onChange=[this]{piano.grid=std::pow(.5,gridBox.getSelectedItemIndex());piano.repaint();};
    copyButton.onClick=[this]{clipboard.clear();for(size_t i=0;i<clip().notes.size();++i)if(piano.selected.empty()||piano.selected.count((int)i))clipboard.push_back(clip().notes[i]);message("Notes copied. Paste inserts at the cursor inside the clip.");};
    pasteButton.onClick=[this]{piano.pasteNotes();};
    soundButton.setButtonText("BUILT-IN");soundButton.onClick=[this]{useBuiltInSound(selectedTrack);};
    sampleButton.onClick=[this]{browseSamplesFor(selectedTrack);};
    loadPluginButton.onClick=[this]{showPluginMenu();};
    pluginEditorButton.onClick=[this]{openHostedEditor(selectedTrack,0);};
    soundBox.onChange=[this]{if(refreshing)return;checkpoint();project.tracks[(size_t)selectedTrack].sound=soundBox.getSelectedItemIndex();changed(false);};
    importButton.onClick=[this]{chooseFile(tr("Import notes into the selected section"),"*.mid;*.midi",juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this](juce::File f){importFile(f);});};
    addSectionButton.onClick=[this]{showAddSectionMenu();};spotlightButton.setIcon(IconButton::Icon::zoomIn);spotlightButton.setPrimary(true);spotlightButton.setTooltip(tr("Spotlight Quick Search (Ctrl/Cmd+K): search actions, genres, tracks, sounds and VST3/AU plugins."));spotlightButton.onClick=[this]{showSpotlightSearch();};
    saveButton.onClick=[this]{save(false);};saveAsButton.onClick=[this]{save(true);};
    openButton.onClick=[this]{chooseFile(tr("Open Fase Groove Arranger project"),"*.fga",juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this](juce::File f){open(f);});};
    newButton.onClick=[this]{juce::Component::SafePointer<MainComponent> safe(this);requestClose([safe]{if(safe)safe->createNewProject();});};
    exportButton.onClick=[this]{exportMenu();};settingsButton.onClick=[this]{showAudioSettings();};aboutButton.onClick=[this]{showAbout();};helpButton.onClick=[this]{showGuide(Localisation::current()!=Localisation::Language::italian);};
    playButton.onColour=Daw::play;loopButton.onColour=Daw::accent;metronomeButton.onColour=Daw::accent;
    // Icon-only actions: the tooltip names them (translated with the rest of the interface).
    for(const auto& entry:std::initializer_list<std::pair<IconButton*,const char*>>{{&newButton,"Start a new song project."},{&openButton,"Open a local .fga project."},{&saveButton,"Save the song and all instrument settings in a project."},{&saveAsButton,"SAVE AS"},{&exportButton,"Export the mix or separate tracks as MIDI, WAV or MP3."},{&settingsButton,"Choose the audio output and buffer size."},{&aboutButton,"Choose the language, control tooltips, credits and privacy."},{&helpButton,"GUIDE"},{&automationButton,"Choose a sound, instrument or effect parameter to automate."}})entry.first->getProperties().set("fgaTip",entry.second);
    for(auto* b:{&newButton,&openButton,&saveButton,&saveAsButton,&exportButton,&settingsButton,&aboutButton,&helpButton,&inspectorToggle,&mixerToggle,&browserToggle,&zoomSelectionButton,&loopSelectionButton,&fitSongButton,&followButton,&automationButton,&fullScreenButton})b->quiet=true;
    for(auto* b:{&inspectorToggle,&mixerToggle,&browserToggle,&followButton,&automationButton,&fullScreenButton})b->onColour=b==&automationButton?Daw::automation:Daw::accent;
    tempoSlider.setSliderStyle(juce::Slider::LinearBarVertical);tempoSlider.setTextValueSuffix("");tempoSlider.setTextBoxStyle(juce::Slider::TextBoxLeft,false,82,26);tempoSlider.setColour(juce::Slider::textBoxTextColourId,Daw::text);tempoSlider.setMouseDragSensitivity(400);tempoSlider.setVelocityBasedMode(false);tempoSlider.setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    for(auto* box:{&keyBox,&scaleBox}){box->getProperties().set("transportReadout",true);box->setColour(juce::ComboBox::backgroundColourId,juce::Colours::transparentBlack);box->setColour(juce::ComboBox::textColourId,Daw::text);box->getProperties().set("dawDisplay",box==&keyBox?17.f:13.f);}
    masterSlider.setTextBoxStyle(juce::Slider::TextBoxRight,false,58,22);masterSlider.setDoubleClickReturnValue(true,0);
playButton.setTooltip(tr("Start or pause playback."));stopButton.setTooltip(tr("Stop playback and return to the beginning."));rewindButton.setTooltip(tr("Back to the beginning."));undoButton.setTooltip(tr("Undo"));redoButton.setTooltip(tr("Redo"));
    rewindButton.setConnectedEdges(juce::Button::ConnectedOnRight);playButton.setConnectedEdges(juce::Button::ConnectedOnLeft|juce::Button::ConnectedOnRight);stopButton.setConnectedEdges(juce::Button::ConnectedOnLeft);
    for(auto* b:{&songButton}){b->setColour(juce::TextButton::buttonColourId,Palette::accent);b->setColour(juce::TextButton::textColourOffId,Palette::background);}
    for(const auto& entry:std::initializer_list<std::pair<juce::Component*,const char*>>{{&genreBox,"GROOVE"},{&keyBox,"KEY"},{&scaleBox,"Scale / mode"},{&extensionBox,"Extensions"},{&voicingBox,"Voicing"},{&progressionBox,"PROGRESSION"},{&tempoSlider,"TEMPO"},{&swingSlider,"SWING"},{&densitySlider,"DENSITY"},{&humanizeSlider,"HUMANIZE"},{&masterSlider,"Volume"},{&gainSlider,"Volume"},{&panSlider,"PAN"},{&velocitySlider,"VELOCITY"},{&zoomSlider,"ZOOM"},{&durationLabel,"NEW ARRANGEMENT"}})entry.first->getProperties().set("fgaControl",entry.second);
    piano.getProperties().set("fgaTip","Draw notes, drag to move, drag the edge for duration; Alt-drag changes velocity.");

    addChildComponent(options);addChildComponent(updateOverlay);
    options.onLanguageChanged=[this]{applyLanguage();};
    options.onScaleChosen=[this](float scale){if(onScaleChosen)onScaleChosen(scale);};
    options.onTipsChanged=[this](bool enabled){Localisation::setTipsEnabled(enabled);applyLanguage();};
    addChildComponent(pluginSettings);addChildComponent(pluginBrowser);
    pluginBrowser.onChoose=[this](const PluginCatalog::BrowserRow& row){chooseBrowserRow(row);};
    // The Open File button hides the browser before launching its native chooser.
    // Defer restoration so the chooser/loading guards are already set.
    pluginBrowser.onDismiss=[this]{juce::Component::SafePointer<MainComponent> safe(this);juce::MessageManager::callAsync([safe]{if(safe)safe->resumeEffectChain();});};
    pluginBrowser.onRescan=[this]{pluginCatalog.scan(PluginCatalog::ScanMode::all);};
    pluginSettings.onReport=[this]{auto text=pluginCatalog.report();auto file=PluginCatalog::defaultStorage().getChildFile("ScanReport.txt");file.getParentDirectory().createDirectory();if(file.replaceWithText(text))text+="\n\n"+tr("Report file")+": "+file.getFullPathName();showDocument(tr("SCAN REPORT"),text);};
    pluginBrowser.onSettings=[this]{pluginSettings.show();};
    pluginBrowser.onOpenFile=[this]{const auto session=browserTrackSession;const int slot=browserEffectSlot;const bool fx=effectsBrowser;pluginFileDialogOpen=true;chooseFile(tr("Open VST3 file..."),"*.vst3",juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::canSelectDirectories,[this,session,slot,fx](juce::File file){pluginFileDialogOpen=false;const int track=trackForSession(session);if(track>=0&&!loadingPlugin&&!editingRestricted()){if(fx)loadEffectFile(track,file,{},{},slot);else loadPluginFile(track,file);}resumeEffectChain();},[this]{pluginFileDialogOpen=false;resumeEffectChain();});};
    options.onPluginSettings=[this]{pluginSettings.show();};
    options.onAudioSettings=[this]{showAudioSettings();};
    options.onDocument=[this](int id){if(id==0)showGuide(Localisation::current()!=Localisation::Language::italian);if(id==1){const bool it=Localisation::current()==Localisation::Language::italian;showDocument(tr("Privacy and permissions"),juce::String::fromUTF8(it?BinaryData::PRIVACY_it_md:BinaryData::PRIVACY_md,it?BinaryData::PRIVACY_it_mdSize:BinaryData::PRIVACY_mdSize));}if(id==2)showDocument(tr("Licences and attributions"),juce::String::fromUTF8(BinaryData::THIRD_PARTY_NOTICES_md,BinaryData::THIRD_PARTY_NOTICES_mdSize));};
    options.onCheckUpdates=[this]{options.checkUpdatesButton.setEnabled(false);options.checkUpdatesButton.setButtonText(tr("CHECKING..."));updater.start(true);};
    updateOverlay.onInstall=[this](const Updater::Result& result){
        if(loadingPlugin){message(tr("Wait for the instrument to finish loading."),true);return;}
        juce::Component::SafePointer<MainComponent> safe(this);
        requestClose([safe,result]{
        if(!safe)return;auto& updateOverlay=safe->updateOverlay;
        const auto report=Updater::install(result);
        if(!report.done){safe->message(tr("The files could not be replaced automatically: the download is shown, copy it as the first time."),true);return;}
        updateOverlay.setVisible(false);
        if(report.relaunching){juce::MessageManager::callAsync([]{if(auto* app=juce::JUCEApplicationBase::getInstance())app->quit();});}
        else safe->message(tr("Updated. Close and reopen the app to use the new version."));
        });
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
        auto callback=s->onValueChange;const bool mix=s==&masterSlider||s==&gainSlider||s==&panSlider;trackGesture(*s,(s==&densitySlider||s==&swingSlider||s==&humanizeSlider)?std::function<void()>([this]{flushGlobalFeel();}):std::function<void()>(),mix);
        s->onValueChange=[this,s,callback,mix]{if(!refreshing&&!inGesture(*s)){if(mix)checkpointMix(s);else checkpoint(s);}if(callback)callback();};
    }
    for(auto* button:{&arrangeButton,&harmonyButton})button->setMouseClickGrabsKeyboardFocus(false);
    arrangeButton.setIcon(IconButton::Icon::song);songButton.setIcon(IconButton::Icon::dice);trackButton.setIcon(IconButton::Icon::dice);sectionButton.setIcon(IconButton::Icon::dice);clipButton.setIcon(IconButton::Icon::dice);
    leadButton.setIcon(IconButton::Icon::guitar);instrumentButton.setIcon(IconButton::Icon::gear);editButton.setIcon(IconButton::Icon::pencil);backButton.setIcon(IconButton::Icon::left);addSectionButton.setIcon(IconButton::Icon::song);
    refreshControls();audio.publish(project);setSize(1440,980);select(4,0);showArrangement();fitSong();translateControls(*this);message(juce::String::fromUTF8("Ready - Space: play/pause - Right-click: options - Autosave every minute."));startTimerHz(20);
}
MainComponent::~MainComponent(){
    Localisation::setSetting("inspectorVisible",inspectorVisible?"1":"0");
    Localisation::setSetting("mixerVisible",mixerVisible?"1":"0");
    Localisation::setSetting("arrangementZoom",arrangement.pixelsPerBar);
    Localisation::setSetting("pianoZoom",piano.pixelsPerBeat);
    Localisation::setSetting("appFullScreen",appFullScreen?"1":"0");
exportTask.reset();effectsWindow.reset();if(automationPicker)automationPicker->dismiss();++autosaveRequestId;retiredJobs.retire(std::move(autosaveJob));studioWindow.reset();aiWindow.reset();updater.onResult=nullptr;if(musicalToolWindow)musicalToolWindow->closeButtonPressed();stopTimer();mixerRoutingWindow.reset();studioWindow.reset();instrumentWindow.reset();closePluginEditor();audio.stop();setLookAndFeel(nullptr);}
// A native close may hide a peer before JUCE receives the matching mouse-up.
// Recover only that hidden window, never a real held button or another gesture.
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
    if(isShowing()&&isEnabled()){getTopLevelComponent()->toFront(true);grabKeyboardFocus();}
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
bool MainComponent::verifyAiComposer(const juce::File& folder){
#if !FGA_SELF_TESTS
return false;
#else
    showAiComposer();bool ok=aiComposeButton.isVisible()&&aiComposeButton.getTooltip().isNotEmpty()&&aiWindow->verify(folder);aiWindow->closeButtonPressed();
    const auto language=Localisation::current();aiWindow.reset();Localisation::setCurrent(Localisation::Language::italian);showAiComposer();const auto itFolder=folder.getChildFile("ai-it");itFolder.createDirectory();ok=aiWindow->verify(itFolder)&&ok;aiWindow->closeButtonPressed();aiWindow.reset();Localisation::setCurrent(language);
    const auto saved=project;AiOptions o;o.tracks={4,leadTrack};o.prompt="6 minute techno";
    const juce::String json=R"({"title":"AI techno","summary":"Six-minute composition","bpm":138,"key":0,"scale":1,"genre":13,"target_seconds":360,"sections":[{"name":"Intro","type":"intro","bars":16},{"name":"Drop","type":"drop","bars":32},{"name":"Outro","type":"outro","bars":16}],"patterns":[{"id":"kick","track":4,"beats":4,"notes":[[0,36,0.5,110],[1,36,0.5,100],[2,36,0.5,110],[3,36,0.5,100]]},{"id":"lead","track":17,"beats":4,"notes":[[0,60,0.5,100],[1,63,0.5,90],[2,67,1,100]]}],"clips":[{"section":0,"pattern":"kick"},{"section":1,"pattern":"kick"},{"section":1,"pattern":"lead"},{"section":2,"pattern":"kick"}]})";
    Project result;juce::String summary;ok=aiCompile(project,o,json,result,summary).wasOk()&&ok;ok=applyAiComposition(saved,result,true).wasOk()&&ok;ok=ok&&audio.isPlaying()&&project.settings.bpm==138&&tempoSlider.getValue()==138&&durationLabel.getText()=="6:00"&&std::abs(project.length()*60/138-360)<.01&&!project.tracks[leadTrack].clips[1].notes.empty();undo(false);ok=ok&&midiFile(project)==midiFile(saved);project.settings.density+=.01;ok=ok&&applyAiComposition(saved,result,false).failed();project=saved;audio.stop();changed();
    folder.getChildFile("ai-composer.txt").replaceWithText(ok?"PASS composer window, provider choices, masked session key, real six-minute MIDI insertion, 138 BPM persistence, playback, Undo and stale-result protection":"FAIL AI composer");return ok;
#endif
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
void MainComponent::mixChanged(){dirty=true;audio.updateMix(project);refreshControls();arrangement.repaint();mixer.repaint();repaintHeader();}
// Only the window header (display legends, page title) and the status bar (project name and
// length) are painted by the window itself.
void MainComponent::repaintHeader(){repaint(0,0,getWidth(),toolbarBottom()+1);repaint(0,getHeight()-statusHeight,getWidth(),statusHeight);}
void MainComponent::changed(bool schedule,int onlyTrack){project.normalise();songLengthCache=-1;chordNameCacheReady=false;dirty=true;audio.setLoop(project.loop,project.loopStart,project.loopEnd);if(schedule)audio.publish(project,true,onlyTrack);else audio.updateMix(project);refreshControls();arrangement.repaint();piano.repaint();repaintHeader();}
// Derived data asked for by paint() and the 20 Hz timer: recomputed only when the
// harmony settings (chord names) or the form (length) change.
const std::vector<std::string>& MainComponent::cachedChordNames(){
    auto& s=project.settings;
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
void MainComponent::showEditor(){ensureTimelineClip();page=Page::editor;zoomSlider.setValue(piano.pixelsPerBeat,juce::dontSendNotification);resized();select(selectedTrack,selectedSection);piano.grabKeyboardFocus();repaint();}
void MainComponent::select(int track,int section){++uiRevision;instrumentPanel.flushRender();trackInspector.finishNameEdit();const int previousTrack=selectedTrack,previousSection=selectedSection;selectedTrack=std::clamp(track,0,(int)project.tracks.size()-1);selectedSection=std::clamp(section,0,(int)project.sections.size()-1);if(previousTrack!=selectedTrack||previousSection!=selectedSection){if(!automationAllTracks&&std::any_of(arrangement.volumeVisible.begin(),arrangement.volumeVisible.end(),[](bool on){return on;}))showAutomationScope(true);if(audio.isClipPlaying())audio.play(false);piano.stopPreview();noClipSelection=false;selectedClip=project.clipAtBeat(selectedTrack,project.sectionStart(selectedSection));selectionBeat=project.sectionStart(selectedSection)+1./1920.;arrangement.selectedClips.clear();}piano.clearSelection();sectionSelector.setSelectedId(selectedSection+1,juce::dontSendNotification);refreshTrack();updateSizes();piano.fitNotes();pianoPort.setViewPosition(0,0);mixer.reveal(selectedTrack);
    if(page==Page::arrangement){const auto v=arrangementPort.getViewArea();const int y=arrangement.rowY(rowForTrack(selectedTrack));if(y<v.getY()+rulerHeight)arrangementPort.setViewPosition(v.getX(),std::max(0,y-rulerHeight));else if(y+arrangement.rowHeight(rowForTrack(selectedTrack))>v.getBottom())arrangementPort.setViewPosition(v.getX(),y+arrangement.rowHeight(rowForTrack(selectedTrack))-v.getHeight());}
    sampleBrowser.refreshTarget();if(instrumentIsOpen()&&(previousTrack!=selectedTrack||previousSection!=selectedSection)){instrumentPanel.rebuild();instrumentWindow->refresh();}arrangement.repaint();piano.repaint();}
void MainComponent::refreshControls(){
    ++uiRevision;
    refreshing=true;auto& s=project.settings;const auto& tempo=genreProfile(s.genre);tempoSlider.setRange(s.freeTempo?30:tempo.minBpm,s.freeTempo?300:tempo.maxBpm,.1);durationLabel.setText(juce::String((int)(project.length()*60/s.bpm)/60)+":"+juce::String((int)(project.length()*60/s.bpm)%60).paddedLeft('0',2),juce::dontSendNotification);leadButton.setToggleState(s.guitarLead,juce::dontSendNotification);
    // The section list is rebuilt (and translated) only when the sections change.
    {juce::StringArray names;for(const auto& section:project.sections)names.add(utf(section.name));if(names!=sectionSelectorNames){sectionSelectorNames=names;sectionSelector.clear(juce::dontSendNotification);for(int i=0;i<names.size();++i)sectionSelector.addItem(names[i],i+1);translateControls(sectionSelector);}}
    for(int i=0;i<12&&keyBox.getNumItems()==12;++i){auto name=juce::String::fromUTF8(sgk::spelledNote(i,s.key,s.scale));if(keyBox.getItemText(i)!=name)keyBox.changeItemText(i+1,name);}
    for(int pc=0;pc<12;++pc)keyBox.changeItemText(pc+1,sgk::spelledNote(pc,s.key,s.scale));
    sectionSelector.setSelectedId(selectedSection+1,juce::dontSendNotification);genreBox.setSelectedId(s.genre+1,juce::dontSendNotification);keyBox.setSelectedId(s.key+1,juce::dontSendNotification);scaleBox.setSelectedId(s.scale+1,juce::dontSendNotification);extensionBox.setSelectedId(s.extension+1,juce::dontSendNotification);voicingBox.setSelectedId(s.voicing+1,juce::dontSendNotification);bassBox.setSelectedId(s.bassMode+1,juce::dontSendNotification);stringsBox.setSelectedId(s.stringsMode+1,juce::dontSendNotification);
    tempoSlider.setValue(s.bpm,juce::dontSendNotification);swingSlider.setValue(s.swing*100,juce::dontSendNotification);densitySlider.setValue(s.density*100,juce::dontSendNotification);humanizeSlider.setValue(s.humanize*100,juce::dontSendNotification);updateGainDisplay(masterSlider,project.masterGain,project.masterAutomation);loopButton.setToggleState(project.loop,juce::dontSendNotification);
    int matchedProgression=0;
    const auto& presets=sgk::progressionPresets();
    for(size_t candidate=0;candidate<=presets.size();++candidate){const int preferred=progressionBox.getSelectedItemIndex();const int index=candidate==0?preferred:(int)candidate-1;if(index<0||index>=(int)presets.size())continue;const size_t pi=(size_t)index;const auto& preset=presets[pi];if(preset.scale!=s.scale)continue;const auto slots=sgk::presetSlots(preset,s.extension);if((int)slots.size()!=s.chordCount)continue;bool same=true;
        for(int i=0;i<s.chordCount;++i){const auto& slot=slots[(size_t)i];same=same&&s.freeChords[(size_t)i]==slot.freeCode&&(slot.freeCode>=0||s.degrees[(size_t)i]==slot.degree);}if(same){matchedProgression=(int)pi+1;break;}}
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
    refreshing=false;trackInspector.refresh();mixer.refresh();
}
void MainComponent::updateSizes(){if(automationAllTracks&&std::any_of(arrangement.volumeVisible.begin(),arrangement.volumeVisible.end(),[](bool visible){return visible;}))for(size_t i=0;i<arrangement.volumeVisible.size();++i)arrangement.volumeVisible[i]=i<project.tracks.size();timelineZoom.setValue(arrangement.pixelsPerBar,juce::dontSendNotification);automationButton.setToggleState(arrangement.volumeVisible[(size_t)selectedTrack],juce::dontSendNotification);arrangement.setSize(std::max(arrangementPort.getWidth()-16,headerWidth+(int)((project.length()+32)/4*arrangement.pixelsPerBar)+20),arrangement.totalHeight());piano.setSize(piano.leftMargin()+(int)(clipLength()*piano.pixelsPerBeat)+20,std::max(selectedTrack==3&&!piano.guitarNotes?640:320,pianoPort.getHeight()-18));}
void MainComponent::undo(bool redo){if(loadingPlugin)return;flushGlobalFeel();instrumentPanel.cancelRender();auto& source=redo?redoStack:undoStack;auto& dest=redo?undoStack:redoStack;if(source.empty())return;const int focus=clipIndex();const bool mixOnly=source.back().mixOnly;if(mixOnly)audio.reusePluginStates(project);else captureUndoPluginStates();dest.push_back({project,mixOnly});const auto& before=dest.back().project;project=std::move(source.back().project);source.pop_back();lastCheckpointControl=nullptr;project.normalise();selectedSection=std::min(selectedSection,(int)project.sections.size()-1);
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
    arrangementPort.setVisible(song);if(!instrumentWindow)instrumentPort.setVisible(engine);pianoPort.setVisible(edit);inspectorPort.setVisible(song&&inspectorVisible);inspectorCollapse.setVisible(song&&inspectorVisible);inspectorResizer.setVisible(song&&inspectorVisible);mixer.setVisible(song);
    for(auto* button:{&inspectorToggle,&mixerToggle,&fitSongButton,&followButton})button->setVisible(song);
    for(juce::Component* c:std::initializer_list<juce::Component*>{&genreBox,&songButton,&generateMenuButton,&densitySlider,&swingSlider,&humanizeSlider,&progressionBox,&harmonyButton,&extensionBox,&voicingBox,&bassBox,&stringsBox,&addSectionButton,&spotlightButton,&durationLabel,&arrangeButton,&chordEditButton,&browserToggle})c->setVisible(song);
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
        x=12;place(genreBox,x,rowA,170);x+=176;place(durationLabel,x,rowA,48);x+=52;divider(x,rowA+4,20);x+=9;
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
            const int top=toolbarBottom()+8;pianoPort.setBounds(12,top,w-24,getHeight()-statusHeight-8-top);
        }
    }
    chordsLabel.setVisible(song);
    if(song){trackButton.setButtonText("REGENERATE TRACK");sectionButton.setButtonText("REGENERATE SECTION");clipButton.setButtonText("REGENERATE CLIP");}
    place(statusLabel,12,getHeight()-statusHeight+1,w-560,statusHeight-2);options.setBounds(getLocalBounds());if(options.isVisible())options.toFront(false);pluginSettings.setBounds(getLocalBounds());if(pluginSettings.isVisible())pluginSettings.toFront(false);pluginBrowser.setBounds(getLocalBounds());if(pluginBrowser.isVisible())pluginBrowser.toFront(false);updateOverlay.setBounds(getLocalBounds());if(updateOverlay.isVisible())updateOverlay.toFront(false);updateSizes();for(juce::Button* b:std::initializer_list<juce::Button*>{&trackButton,&sectionButton,&clipButton,&inspectorCollapse})translateControls(*b);
    if(loadingView.isVisible())loadingView.toFront(false);
}
void MainComponent::mouseDown(const juce::MouseEvent& e){if(e.eventComponent==&chordsLabel)showChordCirclePopover(0);}
bool MainComponent::writeLanguageEvidence(const juce::File& folder){
    const auto saved=Localisation::current();const auto originalJson=juce::JSON::toString(projectJson(project),false,17);bool ok=true;static const char* codes[]{"en","it","es","fr","de","pt"};static const char* expected[]{"NEW TRACK","NUOVA TRACCIA","NUEVA PISTA","NOUVELLE PISTE","NEUE SPUR","NOVA FAIXA"};
    for(int i=0;i<6;++i){Localisation::setCurrent((Localisation::Language)i);showArrangement();applyLanguage();refreshControls();ok=tr("NEW TRACK")==juce::String::fromUTF8(expected[i])&&ok;
        ok=genreBox.getSelectedItemIndex()==project.settings.genre&&keyBox.getSelectedItemIndex()==project.settings.key&&scaleBox.getSelectedItemIndex()==project.settings.scale&&extensionBox.getSelectedItemIndex()==project.settings.extension&&voicingBox.getSelectedItemIndex()==project.settings.voicing&&ok;
        ok=juce::JSON::toString(projectJson(project),false,17)==originalJson&&ok;auto picture=createComponentSnapshot(getLocalBounds());if(auto output=folder.getChildFile(juce::String("language-")+codes[i]+".png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(picture,*output)&&ok;}else ok=false;}
    Localisation::setCurrent(saved);applyLanguage();select(0,1);showInstrument();
    for(const auto& pair:std::initializer_list<std::pair<const char*,const char*>>{{"PERFORMANCE","piano-performance"},{"RHYTHM / VELOCITY","piano-rhythm"},{"Musical tools","piano-tools"},{"GUIDE SOUND","piano-sound"}}){instrumentPort.setViewPosition(0,instrumentPanel.sectionPosition(pair.first));auto image=instrumentPort.createComponentSnapshot(instrumentPort.getLocalBounds());if(auto output=folder.getChildFile(juce::String(pair.second)+".png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(image,*output)&&ok;}}
    Localisation::setCurrent(Localisation::Language::italian);applyLanguage();showInstrument();
    for(const auto& pair:std::initializer_list<std::pair<const char*,const char*>>{{"PERFORMANCE","piano-extended-it"},{"RHYTHM / VELOCITY","piano-rhythm-it"}}){instrumentPort.setViewPosition(0,instrumentPanel.sectionPosition(pair.first));auto picture=instrumentPort.createComponentSnapshot(instrumentPort.getLocalBounds());if(auto output=folder.getChildFile(juce::String(pair.second)+".png").createOutputStream()){juce::PNGImageFormat png;ok=png.writeImageToStream(picture,*output)&&ok;}}
    Localisation::setCurrent(saved);applyLanguage();showInstrument();
    ok=verifyPianoTools(*this,folder)&&ok;
    int tips=0;std::function<void(juce::Component&)> count=[&](auto& c){if(auto* tip=dynamic_cast<juce::SettableTooltipClient*>(&c))if(tip->getTooltip().isNotEmpty())++tips;for(auto* child:c.getChildren())count(*child);};count(*this);ok=tips>=50&&ok;
    folder.getChildFile("localisation.txt").replaceWithText(ok?"PASS six interface languages, live translation, extended piano sections and control tooltips":"FAIL localisation or extended controls");showArrangement();return ok;
}
// Every visible icon-only button must explain itself with a tooltip.
juce::StringArray MainComponent::iconButtonsWithoutTips(){
    juce::StringArray missing;std::function<void(juce::Component&,const juce::String&)> scan=[&](juce::Component& c,const juce::String& path){
        if(!c.isVisible())return;
        // It must also be the component the pointer finds there (not covered by another one).
        if(auto* icon=dynamic_cast<IconButton*>(&c))if(icon->getTooltip().trim().isEmpty()||(getComponentAt(getLocalPoint(&c,c.getLocalBounds().getCentre()))!=&c))missing.add(path+"/"+(c.getComponentID().isNotEmpty()?c.getComponentID():c.getName().isNotEmpty()?c.getName():juce::String(c.getX())+","+juce::String(c.getY())));
        for(auto* child:c.getChildren())scan(*child,path+"/"+juce::String(c.getParentComponent()?c.getParentComponent()->getIndexOfChildComponent(&c):0));};
    scan(*this,"main");return missing;
}
bool MainComponent::writeUiEvidence(const juce::File& folder){
#if !FGA_SELF_TESTS
return false;
#else
    // A synthesised peer press is only attributed to a JUCE component on some window
    // stacks (it depends on which peer owns the pointer), so this regression runs
    // opt-in for a local check and never gates a release by itself.
    if(juce::SystemStats::getEnvironmentVariable("FGA_RUN_NATIVE_INPUT","")!="1")
        folder.getChildFile("window-focus.txt").replaceWithText("SKIP synthetic input regression: a synthesised peer press is not reliably attributed to a JUCE component. Set FGA_RUN_NATIVE_INPUT=1 to run it locally; the recovery code and its guard stay in place.");
    else if(!verifyWindowFocus(folder))return false;
    if(juce::SystemStats::getEnvironmentVariable("FGA_SKIP_EXPORT_UI_TEST","0")=="1"){
        folder.createDirectory();folder.getChildFile("export-ui-skipped.txt").replaceWithText("SKIP previously verified export UI; default CI runs the complete export flow.");
    }else if(!verifyExports(folder.getChildFile("export-ui")))return false;
    showStudio();const bool studioOk=studioWindow->verify(folder);studioWindow->closeButtonPressed();if(!studioOk)return false;
    if(!verifyAuditRepairs(folder))return false;
    {
        const auto saved=project;const int track=selectedTrack;select(0,0);showInstrument();bool passed=false;
        for(auto* child:instrumentPanel.getChildren())if(auto* circle=dynamic_cast<CircleOfFifths*>(child)){
            circle->onMoveChord(0,-1,(project.settings.key+1)%12,true);const int chosen=project.settings.freeChords[0];
            passed=project.settings.manualChords[0];for(unsigned seed=1;seed<=20;++seed)randomiseHarmony(project,seed);
            passed=passed&&project.settings.manualChords[0]&&project.settings.freeChords[0]==chosen;
            circle->onMoveChord(0,0,project.settings.key,false);passed=passed&&!project.settings.manualChords[0]&&project.settings.freeChords[0]<0;
        }
        project=saved;select(track,0);changed();undoStack.clear();redoStack.clear();
        folder.getChildFile("manual-chord-ownership.txt").replaceWithText(passed?"PASS circle edits protect custom chromatic chords across repeated dice; choosing a degree releases the override":"FAIL manual circle chord ownership");if(!passed)return false;
    }
    {
        const auto saved=project;const int track=selectedTrack,section=selectedSection;bool passed=true;juce::String report;
        for(int genre:{0,technoGenre,electroHouseGenre}){
            project=newProject(genre);project.sections={{Section::main,8,"Repeated chords"}};project.settings.randomAutomation=false;
            for(auto& t:project.tracks){t.locked=false;t.freeTimeline=false;t.clips.assign(1,{});}project.normalise();select(0,0);changed();
            std::set<juce::String> harmonies;std::set<std::uint64_t> midi;bool roundTrip=true;
            for(int press=0;press<32;++press){
                harmonyButton.onClick();juce::MessageManager::getInstance()->runDispatchLoopUntil(1);
                if(press%8==7){Project loaded;roundTrip=projectFromJson(projectJson(project),loaded).wasOk()&&roundTrip;if(roundTrip){project=std::move(loaded);changed();}}
                if(press>=8){juce::StringArray names;for(const auto& name:chordNames(project.settings))names.add(utf(name));harmonies.insert(names.joinIntoString(" / "));const auto bytes=midiFile(project);std::uint64_t hash=1469598103934665603ULL;for(auto byte:bytes){hash^=byte;hash*=1099511628211ULL;}midi.insert(hash);}
            }
            const bool automatic=std::none_of(project.settings.manualChords.begin(),project.settings.manualChords.end(),[](bool manual){return manual;});
            const bool varied=roundTrip&&automatic&&harmonies.size()>1&&midi.size()>1;passed=passed&&varied;
            report+=(varied?"PASS ":"FAIL ")+juce::String(genre)+": 32 actual New Chords actions with four save/reload cycles; late harmonies="+juce::String((int)harmonies.size())+", MIDI="+juce::String((int)midi.size())+"\n";
        }
        project=saved;select(track,section);changed();undoStack.clear();redoStack.clear();folder.getChildFile("repeated-new-chords.txt").replaceWithText(report);if(!passed)return false;
    }
    {
        const auto saved=project;const auto undos=undoStack,redos=redoStack;const bool previousDirty=dirty;const int track=selectedTrack,section=selectedSection,selected=selectedClip;const bool noSelection=noClipSelection;const double cursor=editCursor;
        for(auto& t:project.tracks){t.locked=false;for(auto& c:t.clips)c.locked=false;}
        for(auto& c:project.tracks[0].clips){c.notes={{.5,.75,123,117,0}};c.loopLength=0;c.loopSource.clear();}
        const auto before=juce::JSON::toString(projectJson(project),false,17);const auto count=undoStack.size();arrangeButton.onClick();
        bool passed=undoStack.size()==std::min((size_t)40,count+1);
        for(const auto& clip:project.tracks[0].clips)for(const auto& note:clip.notes)passed=passed&&note.pitch==123&&note.velocity==117&&note.length==.75;
        undo(false);passed=passed&&before==juce::JSON::toString(projectJson(project),false,17);
        project=saved;undoStack=undos;redoStack=redos;dirty=previousDirty;selectedTrack=track;selectedSection=section;selectedClip=selected;noClipSelection=noSelection;editCursor=cursor;audio.publish(project);refreshControls();
        folder.getChildFile("new-arrangement.txt").replaceWithText(passed?"PASS NEW ARRANGEMENT button reuses MIDI; one-step Undo restores the complete project":"FAIL NEW ARRANGEMENT command or Undo");if(!passed)return false;
    }
    {
        const auto savedProject=project;const auto savedUndo=undoStack,savedRedo=redoStack;const int oldTrack=selectedTrack,oldSection=selectedSection;const bool oldDirty=dirty;
        addMidiTrack(true);const int vocal=selectedTrack;auto initial=project.tracks[(size_t)vocal].clips;
        bool vocalOk=project.tracks[(size_t)vocal].vocalChops&&trackButton.isEnabled()&&clipButton.isEnabled()&&regenerateClipButton.isEnabled();
        regenerate(Scope::track);vocalOk=vocalOk&&project.tracks[(size_t)vocal].clips!=initial;undo(false);vocalOk=vocalOk&&project.tracks[(size_t)vocal].clips==initial;
        showInstrument();const auto names=instrumentPanel.controlNames();vocalOk=vocalOk&&names.contains("DENSITY")&&names.contains("GATE")&&names.contains("HUMANIZE")&&names.contains("STUDIO")&&!names.contains("Voicing")&&instrumentPanel.layoutProblems().isEmpty();
        for(auto* child:instrumentPanel.getChildren())if(child->getProperties()["fgaControl"].toString()=="DENSITY")if(auto* tip=dynamic_cast<juce::SettableTooltipClient*>(child))vocalOk=vocalOk&&tip->getTooltip()==tr("Number of short vocal triggers per phrase; zero leaves silence.");
        loadSampleFile(vocal,folder.getChildFile("drop-routing.wav"));showInstrument();const auto& part=project.tracks[(size_t)vocal];vocalOk=vocalOk&&part.instrument=="sample"&&part.sampler.mode==2&&part.sampler.sliceCount>0&&instrumentPanel.layoutProblems().isEmpty();
        for(const auto& c:part.clips)for(const auto& n:c.notes)vocalOk=vocalOk&&n.pitch>=part.sampler.sliceBase&&n.pitch<part.sampler.sliceBase+part.sampler.sliceCount;
        if(auto output=folder.getChildFile("vocal-chops.png").createOutputStream()){juce::PNGImageFormat png;vocalOk=png.writeImageToStream(instrumentPort.createComponentSnapshot(instrumentPort.getLocalBounds()),*output)&&vocalOk;}
        auto sound=project;sound.sections={{Section::main,4,"Vocal test"}};for(auto& t:sound.tracks){t.freeTimeline=false;t.clips.resize(1);}auto& voice=sound.tracks[(size_t)vocal];voice.clips[0].notes={{.5,.25,voice.sampler.sliceBase,100,0},{2.5,.25,voice.sampler.sliceBase+voice.sampler.sliceCount-1,100,0}};
        const auto audioFile=folder.getChildFile("vocal-chops.wav");vocalOk=audio.exportAudio(sound,audioFile,vocal,false,[]{return false;},[](double){}).wasOk()&&vocalOk;
        juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(audioFile.createInputStream().release(),true));
        if(reader&&reader->lengthInSamples>0&&reader->lengthInSamples<44100*30){juce::AudioBuffer<float> data((int)reader->numChannels,(int)reader->lengthInSamples);reader->read(&data,0,data.getNumSamples(),0,true,true);vocalOk=data.getMagnitude(0,data.getNumSamples())>.001f&&vocalOk;}else vocalOk=false;
        deleteMidiTrack(vocal);undo(false);vocalOk=vocalOk&&project.tracks[(size_t)vocal].vocalChops&&project.tracks[(size_t)vocal].instrument=="sample";
        folder.getChildFile("vocal-chops.txt").replaceWithText(vocalOk?"PASS optional vocal generator, live controls, regeneration and Undo, sampled slice mapping, panel layout and deletion recovery":"FAIL vocal chops workflow");
        project=savedProject;undoStack=savedUndo;redoStack=savedRedo;selectedTrack=oldTrack;selectedSection=oldSection;selectedClip=-1;restoreInstruments();changed();dirty=oldDirty;if(instrumentWindow)instrumentWindow->closeButtonPressed();showArrangement();
        if(!vocalOk)return false;
    }
    if(auto* busButton=dynamic_cast<juce::Button*>(mixer.findChildWithID("mixer-routing"))){busButton->onClick();if(!mixerRoutingWindow||!mixerRoutingWindow->verify(folder))return false;mixerRoutingWindow->closeButtonPressed();}else return false;
    if(!mixer.verifySends(folder)||!mixer.verifyAutomation(folder))return false;
    juce::String fail;
    auto mark=[&](const char* step,bool value){if(!value&&fail.isEmpty())fail=step;return value;};
    bool ok=mark("language",writeLanguageEvidence(folder)&&writeHelpEvidence(folder));
    for(int key=0;key<12;++key){keyBox.setSelectedId(key+1,juce::dontSendNotification);const auto text=keyBox.getText();const auto width=juce::GlyphArrangement::getStringWidth(theme.getComboBoxFont(keyBox),text);ok=mark("key-name-visible",width+8<=keyBox.getWidth()-20)&&ok;}
    keyBox.setSelectedId(project.settings.key+1,juce::dontSendNotification);
    {const auto settings=project.settings;for(const auto& key:std::array<std::pair<int,int>,4>{{{6,0},{3,2},{1,0},{1,1}}}){project.settings.key=key.first;project.settings.scale=key.second;refreshControls();ok=mark("key-spelling",keyBox.getText()==exportKey(project))&&ok;}project.settings=settings;refreshControls();}
    pluginSettings.show();ok=mark("plugin-settings",pluginSettings.verifyLayout())&&ok;
    if(pluginSettings.onReport){pluginSettings.onReport();auto* dialog=dynamic_cast<juce::DialogWindow*>(juce::Component::getCurrentlyModalComponent());ok=mark("plugin-scan-report",dialog&&dialog->isVisible()&&dialog->isAlwaysOnTop())&&ok;if(dialog)dialog->closeButtonPressed();}else ok=false;
    pluginSettings.setVisible(false);auto capture=[&](const juce::String& name){auto screenshot=(name.startsWith("instrument-")||name=="guitar-mixer"||name=="piano-family-extended")?instrumentPort.createComponentSnapshot(instrumentPort.getLocalBounds()):createComponentSnapshot(getLocalBounds());bool wrote=false;if(auto out=folder.getChildFile(name+".png").createOutputStream()){juce::PNGImageFormat png;wrote=png.writeImageToStream(screenshot,*out);}ok=mark(name=="plugin-browser"?"png-browser":name=="plugin-settings"?"png-settings":"png",wrote)&&ok;};
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
        select(leadTrack,1);showInstrument();names=instrumentPanel.controlNames();expect("lead",names.contains("Lead part")&&names.contains("ATTACK DELAY")&&names.contains("Resolve after fill")&&!names.contains("MIDI NOTE")&&!names.contains("SOLISTA"),names);
        project.tracks[leadTrack].instrument="sample";showInstrument();names=instrumentPanel.controlNames();expect("lead-sample",!names.contains("TONE")&&!names.contains("REVERB")&&!names.contains("Part volume")&&names.contains("Part level"),names);
        ok=mark("lead-external-panel",instrumentPanel.layoutProblems().isEmpty())&&ok;project.tracks[leadTrack].instrument="internal";
        select(4,1);
        // Track gain stays on the arranger strip. A sample hides the guide tone and reverb.
        project.tracks[4].instrument="sample";showInstrument();names=instrumentPanel.controlNames();expect("sample",!names.contains("TONE")&&!names.contains("REVERB")&&!names.contains("VOLUME"),names);
        project=savedProject;changed();
    }
    pluginSettings.show();capture("plugin-settings");pluginSettings.setVisible(false);
    pluginBrowser.show(0,juce::String("fg")+"\x1f"+"0");ok=mark("plugin-browser",pluginBrowser.verifyLayout())&&ok;capture("plugin-browser");pluginBrowser.setVisible(false);
    select(0,1);showInstrument();capture("piano-family-extended");const auto take=clip().seed;regenerate(Scope::clip);ok=mark("regenerate",clip().seed!=take&&clip().motifSeed!=0)&&ok;const auto generated=clip();if(instrumentWindow)instrumentWindow->body->applyClip.onClick();ok=mark("apply-clip",clip()==generated)&&ok;
    showArrangement();fitSong();ok=mark("fit-song",arrangementPort.isVisible()&&!pianoPort.isVisible()&&headerWidth+project.length()/4*arrangement.pixelsPerBar<arrangementPort.getWidth())&&ok;capture("interface");
    ok=mark("automation",arrangement.verifyAutomation(folder))&&ok;
    ok=mark("ai-composer",verifyAiComposer(folder))&&ok;
    capture("daw-workspace");ok=mark("daw-workspace",verifyDawWorkspace(folder))&&ok;ok=mark("timeline",verifyTimelineEditing(folder))&&ok;ok=mark("navigation",verifyNavigation(folder))&&ok;ok=mark("sample-browser",verifySampleBrowser(folder))&&ok;
    {
        const auto saved=project;const auto undos=undoStack,redos=redoStack;const bool wasDirty=dirty;
        auto expected=project;expected.settings.key=(project.settings.key+2)%12;applyInstrumentChange(expected,0,true);
        undoStack.clear();redoStack.clear();keyBox.setSelectedId(expected.settings.key+1,juce::sendNotificationSync);
        const bool applied=project.settings.key==expected.settings.key&&juce::JSON::toString(projectJson(project),false,17)==juce::JSON::toString(projectJson(expected),false,17)&&undoStack.size()==1;
        undo(false);const bool restored=project.settings.key==saved.settings.key&&juce::JSON::toString(projectJson(project),false,17)==juce::JSON::toString(projectJson(saved),false,17);
        ok=mark("key-change-regenerates",applied&&restored)&&ok;
        folder.getChildFile("key-change.txt").replaceWithText(applied&&restored?"PASS header key change regenerates harmonic MIDI with current chords, preserves other tracks and supports one-step Undo":"FAIL header key change or Undo");
        project=saved;undoStack=undos;redoStack=redos;dirty=wasDirty;audio.publish(project);refreshControls();
    }
    {
        const auto saved=project;const auto undos=undoStack,redos=redoStack;const bool wasDirty=dirty;
        project=newProject(0);project.settings.key=0;project.settings.scale=0;generate(project);changed();
        bool modesOk=true;juce::String report;
        for(int mode:{2,0,1}){
            const auto before=project;auto expected=project;expected.settings.scale=mode;applyInstrumentChange(expected,0,true);
            undoStack.clear();redoStack.clear();scaleBox.setSelectedId(mode+1,juce::sendNotificationSync);
            bool notesChanged=false;for(int ti:{0,1,2,3,leadTrack})for(size_t ci=0;ci<project.tracks[(size_t)ti].clips.size();++ci)notesChanged=notesChanged||project.tracks[(size_t)ti].clips[ci].notes!=before.tracks[(size_t)ti].clips[ci].notes;
            const bool applied=notesChanged&&undoStack.size()==1&&juce::JSON::toString(projectJson(project),false,17)==juce::JSON::toString(projectJson(expected),false,17);
            undo(false);const bool restored=juce::JSON::toString(projectJson(project),false,17)==juce::JSON::toString(projectJson(before),false,17);
            redoButton.onClick();const bool redone=juce::JSON::toString(projectJson(project),false,17)==juce::JSON::toString(projectJson(expected),false,17);
            modesOk=applied&&restored&&redone&&modesOk;report+=juce::String(applied&&restored&&redone?"PASS ":"FAIL ")+sgk::scaleName(mode)+" regenerates harmonic MIDI, Undo and Redo\n";
        }
        ok=mark("scale-change-regenerates",modesOk)&&ok;folder.getChildFile("scale-change.txt").replaceWithText(report);
        project=saved;undoStack=undos;redoStack=redos;dirty=wasDirty;audio.publish(project);refreshControls();
    }
    {
        const auto saved=project;const auto undos=undoStack,redos=redoStack;const bool wasDirty=dirty;const int oldTrack=selectedTrack,oldSection=selectedSection;const double oldTimelineZoom=arrangement.pixelsPerBar,oldPianoZoom=piano.pixelsPerBeat;const auto oldScroll=arrangementPort.getViewPosition();
        auto fixture=[&]{audio.stop();project=Project{};project.normalise();applyGenre(project,0);project.sections={{Section::main,4,"Controls"}};for(auto& t:project.tracks)t.clips.assign(1,{});project.normalise();generate(project);selectedClip=0;noClipSelection=false;selectionBeat=1./1920.;project.tracks[1].locked=true;project.tracks[2].clips[0].locked=true;restoreInstruments();select(0,0);changed();};
        auto json=[&]{return juce::JSON::toString(projectJson(project),false,17);};
        bool feelOk=true;juce::String report;juce::AudioBuffer<float> sound(2,512);juce::AudioIODeviceCallbackContext context;
        auto tick=[&]{audio.audioDeviceIOCallbackWithContext(nullptr,0,sound.getArrayOfWritePointers(),2,512,context);};
        for(auto* control:{&densitySlider,&swingSlider,&humanizeSlider}){
            fixture();control->setValue(0,juce::sendNotificationSync);const auto before=project;const auto baseline=json();undoStack.clear();redoStack.clear();
            audio.play(true);tick();control->setValue(control==&swingSlider?120:95,juce::sendNotificationSync);tick();
            const auto after=json();const bool midiChanged=project.tracks[0].clips[0].notes!=before.tracks[0].clips[0].notes,locksKept=project.tracks[1].clips==before.tracks[1].clips&&project.tracks[2].clips[0]==before.tracks[2].clips[0];const auto history=undoStack.size();const bool stillPlaying=audio.isPlaying();const bool applied=midiChanged&&locksKept&&history==1&&stillPlaying;
            audio.stop();tick();undo(false);const bool restored=json()==baseline;if(!restored){folder.getChildFile(control->getComponentID()+"-before.json").replaceWithText(baseline);folder.getChildFile(control->getComponentID()+"-undo.json").replaceWithText(json());}undo(true);const bool redone=json()==after;
            report+="DETAIL "+control->getComponentID()+" MIDI="+juce::String((int)midiChanged)+" locks="+juce::String((int)locksKept)+" checkpoints="+juce::String((int)history)+" playing="+juce::String((int)stillPlaying)+" undo="+juce::String((int)restored)+" redo="+juce::String((int)redone)+"\n";
            feelOk=feelOk&&applied&&restored&&redone;report+=juce::String(applied&&restored&&redone?"PASS ":"FAIL ")+control->getComponentID()+" changes piano MIDI during Play, preserves locks, Undo/Redo\n";
        }
        fixture();const auto beforeDrag=json();undoStack.clear();redoStack.clear();densitySlider.onDragStart();densitySlider.setValue(25,juce::sendNotificationSync);densitySlider.setValue(85,juce::sendNotificationSync);const bool queued=pendingGlobalFeel!=0&&undoStack.size()==1;densitySlider.onDragEnd();const auto dragged=json();undo(false);const bool gesture=queued&&pendingGlobalFeel==0&&json()==beforeDrag;undo(true);feelOk=feelOk&&gesture&&json()==dragged;report+=juce::String(gesture?"PASS ":"FAIL ")+"coalesced drag applies final value and keeps one Undo\n";
        for(auto scope:{Scope::song,Scope::section,Scope::track,Scope::clip}){
            fixture();project.tracks[1].locked=false;project.tracks[2].clips[0].locked=false;changed();const auto before=project.settings;regenerate(scope,true);
            const bool moved=std::lround(before.density*100)!=std::lround(densitySlider.getValue())&&std::lround(before.swing*100)!=std::lround(swingSlider.getValue())&&std::lround(before.humanize*100)!=std::lround(humanizeSlider.getValue());
            const bool synced=std::abs(densitySlider.getValue()-project.settings.density*100)<.01&&std::abs(swingSlider.getValue()-project.settings.swing*100)<.01&&std::abs(humanizeSlider.getValue()-project.settings.humanize*100)<.01;
            feelOk=feelOk&&moved&&synced;report+=juce::String(moved&&synced?"PASS ":"FAIL ")+"fresh regeneration updates the three visible knob positions, scope="+juce::String((int)scope)+"\n";
        }
        fixture();const auto harmonyBefore=project;undoStack.clear();redoStack.clear();const int degree=(project.settings.degrees[0]+1)%7;applyChordMenuChoice(1000+degree);
        const bool chordChanged=project.settings.degrees[0]==degree&&project.tracks[0].clips[0].notes!=harmonyBefore.tracks[0].clips[0].notes&&undoStack.size()==1;
        undo(false);const bool chordUndo=project.settings.degrees==harmonyBefore.settings.degrees&&project.tracks[0].clips==harmonyBefore.tracks[0].clips;
        feelOk=feelOk&&chordChanged&&chordUndo;report+=juce::String(chordChanged&&chordUndo?"PASS ":"FAIL ")+"classic chord menu updates MIDI and supports Undo\n";
        ok=mark("global-feel-updates-midi",feelOk)&&ok;folder.getChildFile("global-feel.txt").replaceWithText(report);
        project=saved;undoStack=undos;redoStack=redos;dirty=wasDirty;restoreInstruments();arrangement.pixelsPerBar=oldTimelineZoom;piano.pixelsPerBeat=oldPianoZoom;select(oldTrack,oldSection);refreshControls();updateSizes();arrangementPort.setViewPosition(oldScroll);
    }
    {
        auto sameImage=[](const juce::Image& a,const juce::Image& b){if(a.getBounds()!=b.getBounds())return false;for(int y=0;y<a.getHeight();++y)for(int x=0;x<a.getWidth();++x)if(a.getPixelAt(x,y)!=b.getPixelAt(x,y))return false;return true;};
        showArrangement();grabKeyboardFocus();const auto plain=mixerToggle.createComponentSnapshot(mixerToggle.getLocalBounds());
        mixerToggle.grabKeyboardFocus();const bool noStickyFocus=sameImage(plain,mixerToggle.createComponentSnapshot(mixerToggle.getLocalBounds()));grabKeyboardFocus();
        ok=mark("button-focus-feedback",noStickyFocus)&&ok;
        std::function<juce::Component*(juce::Component&)> findClose=[&](juce::Component& root)->juce::Component*{if(root.getComponentID()=="search-close")return &root;for(auto* child:root.getChildren())if(auto* found=findClose(*child))return found;return nullptr;};
        showSpotlightSearch();juce::MessageManager::getInstance()->runDispatchLoopUntil(60);
        juce::Button* close=nullptr;for(int i=0;i<juce::Desktop::getInstance().getNumComponents();++i)if(auto* found=findClose(*juce::Desktop::getInstance().getComponent(i)))close=dynamic_cast<juce::Button*>(found);
        bool searchOk=close&&close->isShowing();
        if(close){juce::Component::SafePointer<juce::Component> popup(close->findParentComponentOfClass<juce::CallOutBox>());if(popup){if(auto out=folder.getChildFile("quick-search.png").createOutputStream()){juce::PNGImageFormat png;png.writeImageToStream(popup->createComponentSnapshot(popup->getLocalBounds()),*out);}}close->triggerClick();juce::MessageManager::getInstance()->runDispatchLoopUntil(120);searchOk=searchOk&&(!popup||!popup->isShowing());}
        ok=mark("search-close-button",searchOk)&&ok;
        folder.getChildFile("new-ui-controls.txt").replaceWithText(juce::String(noStickyFocus?"PASS":"FAIL")+" no persistent focus border after button activation\n"+(searchOk?"PASS":"FAIL")+" Search opens and closes using X\n");
    }
    juce::String layoutReport,editingReport;
    for(int track:{4,16,1,0,2,3,leadTrack}){select(track,1);const auto mainPage=page;showInstrument();auto problems=instrumentPanel.layoutProblems();layoutReport+=juce::String(track)+": "+(problems.isEmpty()?"PASS":problems)+"\n";ok=mark("panel",problems.isEmpty())&&ok;ok=mark("instrument-page",instrumentIsOpen()&&instrumentPort.getTopLevelComponent()!=getTopLevelComponent()&&page==mainPage&&sectionSelector.getSelectedId()==selectedSection+1)&&ok;capture("instrument-"+juce::String(track));if(track==3){instrumentPort.setViewPosition(0,instrumentPanel.getHeight());capture("guitar-mixer");instrumentPort.setViewPosition(0,0);}showEditor();ok=mark("editor-page",pianoPort.getHeight()>400&&!arrangementPort.isVisible())&&ok;if(isDrumTrack(track))ok=mark("drum-lanes",piano.laneView())&&ok;if(track==1||track==2||track==leadTrack)ok=mark("pitched-lanes",!piano.laneView())&&ok;capture("editor-"+juce::String(track));ok=mark("scale-guide-visibility",scaleGuideButton.isVisible()==piano.scaleGuideAvailable())&&ok;if(track==0){const bool guide=piano.showScaleGuide;piano.showScaleGuide=true;scaleGuideButton.setToggleState(true,juce::dontSendNotification);capture("piano-scale-guide");piano.showScaleGuide=guide;scaleGuideButton.setToggleState(guide,juce::dontSendNotification);}const bool edits=piano.verifyEditing();ok=mark("editing",edits)&&ok;if(track==0){const auto json=juce::JSON::toString(projectJson(project),false,17);const auto undos=undoStack.size();pianoPlayButton.onClick();bool clipPlay=audio.isClipPlaying();juce::AudioBuffer<float> sound(2,512);juce::AudioIODeviceCallbackContext context;audio.audioDeviceIOCallbackWithContext(nullptr,0,sound.getArrayOfWritePointers(),2,512,context);clipPlay=clipPlay&&audio.position()>=clipStart()&&audio.position()<clipStart()+clipLength();pianoPlayButton.onClick();clipPlay=clipPlay&&!audio.isPlaying()&&json==juce::JSON::toString(projectJson(project),false,17)&&undos==undoStack.size();ok=mark("piano-clip-play",clipPlay)&&ok;folder.getChildFile("piano-clip-play.txt").replaceWithText(clipPlay?"PASS editor Play/Stop clip button uses isolated engine loop, leaves project and Undo unchanged":"FAIL piano clip playback");}editingReport+=juce::String(track)+": "+(edits?"PASS add, move with per-pitch audition, velocity, resize, delete, precise ruler seek and scale guide":"FAIL editing gestures")+"\n";}
    select(3,1);piano.guitarNotes=true;showEditor();const bool guitarEdits=piano.verifyEditing();ok=mark("guitar-note-editing",guitarEdits&&scaleGuideButton.isVisible())&&ok;editingReport+="guitar notes: "+juce::String(guitarEdits?"PASS drag audition and scale guide":"FAIL editing gestures")+"\n";piano.guitarNotes=false;
    select(leadTrack,1);showArrangement();
    if(auto output=folder.getChildFile("inspector.png").createOutputStream()){juce::PNGImageFormat png;png.writeImageToStream(trackInspector.createComponentSnapshot(trackInspector.getLocalBounds()),*output);}
    capture("arrangement-lead");
    folder.getChildFile("panel-layout.txt").replaceWithText(layoutReport);folder.getChildFile("midi-editing.txt").replaceWithText(editingReport);
    select(3,1);piano.guitarNotes=true;showEditor();ok=mark("guitar-lanes",!piano.laneView())&&ok;capture("guitar-notes");ok=mark("guitar-notes",piano.verifyEditing())&&ok;piano.guitarNotes=false;showArrangement();select(4,0);
    {juce::StringArray missing;showArrangement();showSampleBrowser(true);missing.addArray(iconButtonsWithoutTips());showSampleBrowser(false);for(int track:{0,3,4,leadTrack}){select(track,1);showInstrument();missing.addArray(iconButtonsWithoutTips());showEditor();missing.addArray(iconButtonsWithoutTips());}showArrangement();select(4,0);
        if(!missing.isEmpty())fail="icon buttons without a tooltip: "+missing.joinIntoString(", ");ok=mark("icon-tips",missing.isEmpty())&&ok;}
    ok=mark("presentation",verifyPresentation(folder))&&ok;
    folder.getChildFile("ui.txt").replaceWithText(ok?"PASS separate arrangement, instrument panels, full MIDI editors, drum isolation, guitar lanes/notes, instrument regeneration creates a new take":"FAIL interface routing: "+fail);return ok;
#endif
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
    const int remaining=(int)restorePluginTasks.size()+loadingPlugin;
    if(!remaining){restoringProject=false;loadingView.setVisible(false);return;}
    loadingView.show(tr("Loading plugins")+" "+juce::String(restorePluginTotal-remaining)+" / "+juce::String(restorePluginTotal),double(restorePluginTotal-remaining)/std::max(1,restorePluginTotal));
    if(!loadingPlugin&&!restoreTaskScheduled&&!restorePluginTasks.empty()){
        restoreTaskScheduled=true;juce::Component::SafePointer<MainComponent> safe(this);
        // Give the overlay a paint cycle before entering the next plugin's code.
        juce::Timer::callAfterDelay(30,[safe]{if(!safe)return;safe->restoreTaskScheduled=false;if(!safe->restoringProject||safe->restorePluginTasks.empty())return;auto task=std::move(safe->restorePluginTasks.front());safe->restorePluginTasks.pop_front();task();});
    }
}
void MainComponent::timerCallback(){
    flushGlobalFeel();
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
    autosaveJob=std::make_unique<AutosaveJob>(project,path,[safe,id](juce::Result result){if(!safe||id!=safe->autosaveRequestId)return;safe->retiredJobs.retire(std::move(safe->autosaveJob));if(result.failed())safe->message(result.getErrorMessage(),true);});if(!autosaveJob->startThread()){autosaveJob.reset();message("Automatic backup could not start. Save the project manually.",true);}
}
// The display reads bars.beats.ticks (960 ticks per beat).
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
    if(choice==2){if(studioWindow&&(audio.isRecording()||audio.isAudioRecording()))studioWindow->closeButtonPressed();closePending=false;accepted();return;}
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
    projectFile=target;Localisation::setSetting("lastSaveFolder",target.getParentDirectory().getFullPathName());project.name=next.name;dirty=false;message(tr("Project saved")+": "+projectFile.getFullPathName());repaint();return true;
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
void MainComponent::showStudio(int tab){if(mediaBusy||audio.isExporting())return;if(!studioWindow)studioWindow=std::make_unique<StudioWindow>(*this);studioWindow->open(tab);}
void MainComponent::restoreInstruments(){pendingGlobalFeel=0;addChildComponent(loadingView);loadingView.setBounds(getLocalBounds());loadingView.show(tr("Loading project..."));restoringProject=true;restorePluginTotal=0;restorePluginTasks.clear();closePluginEditor();audio.clearEffect(masterEffectTrack);for(const auto& fx:project.masterEffects.effects)restorePluginTasks.push_back([this,fx]{loadEffectFile(masterEffectTrack,juce::File::isAbsolutePath(utf(fx.path))?juce::File(utf(fx.path)):juce::File(),utf(fx.pluginId),utf(fx.state),fx.slot);});for(int i=0;i<maxTracks;++i){audio.useInternal(i);audio.clearEffect(i);}for(int i=0;i<(int)project.tracks.size();++i){auto t=project.tracks[(size_t)i];if(t.instrument=="sample"){auto r=audio.loadSample(i,sampleFile(t,projectFile),t.sampleRoot,t.sampler);if(r.failed()){audio.markMissing(i);message("Campione mancante su "+utf(t.name)+": ricollegalo con LOAD SAMPLE.",true);}}else if(t.instrument=="plugin")restorePluginTasks.push_back([this,i,t]{loadPluginFile(i,juce::File::isAbsolutePath(utf(t.instrumentPath))?juce::File(utf(t.instrumentPath)):juce::File(),utf(t.instrumentId),utf(t.instrumentState));});for(const auto& fx:t.effects)restorePluginTasks.push_back([this,i,fx]{loadEffectFile(i,juce::File::isAbsolutePath(utf(fx.path))?juce::File(utf(fx.path)):juce::File(),utf(fx.pluginId),utf(fx.state),fx.slot);});}restorePluginTotal=(int)restorePluginTasks.size();audio.publish(project);updateLoadingProgress();}
void MainComponent::loadSampleFile(int track,const juce::File& file){if(!juce::isPositiveAndBelow(track,(int)project.tracks.size())||loadingPlugin||editingRestricted())return;auto& t=project.tracks[(size_t)track];int root=isDrumTrack(track)?(t.engine.midiNote>=0?t.engine.midiNote:fgd::defaultNote(track-4)):60;const auto previous=t.sampler;SamplerSettings settings=t.vocalChops?vocalSamplerSettings():SamplerSettings{};if(!t.vocalChops){settings.mode=isDrumTrack(track)?1:0;settings.polyphony=8;}auto r=audio.loadSample(track,file,root,settings,[this,track]{closePluginEditor(track,0);audio.invalidatePluginState(track,0);checkpoint();});if(r.failed()){message(r.getErrorMessage(),true);return;}t.sampler=settings;if(auto asset=audio.sampleAsset(track)){t.sampleHash=asset->hash.toStdString();if(t.vocalChops){const auto points=detectSampleSlices(*asset,0,1,.5,16);if(!points.empty()){t.sampler.slices.fill(0);t.sampler.sliceCount=(int)points.size();std::copy(points.begin(),points.end(),t.sampler.slices.begin());}}}t.sampler.normalise();remapVocalChops(project,track,previous,true);t.instrument="sample";t.instrumentPath=file.getFullPathName().toStdString();t.sampleRoot=root;t.instrumentId.clear();t.instrumentState.clear();changed(false);sampleBrowser.refreshTarget();
    message(tr("Sample loaded")+": "+file.getFileName()+juce::String::fromUTF8(" → ")+Localisation::upper(utf(t.name))+juce::String::fromUTF8("  ·  ")+tr("root note")+" "+juce::MidiMessage::getMidiNoteName(root,true,true,3)+" ("+tr("right-click the track to change it")+").");}
void MainComponent::loadPluginFile(int track,const juce::File& file,const juce::String& id,const juce::String& state){
    if(id.isNotEmpty())audio.markMissing(track);
    if(auto xml=juce::XmlDocument::parse(id)){
        juce::PluginDescription description;
        if(description.loadFromXml(*xml)){
            if(description.pluginFormatName=="VST3"&&!juce::File(description.fileOrIdentifier).exists()){message("Plugin missing: use LOAD VST3 / AU to relink it.",true);return;}
            loadPluginDescription(track,description,state,true);return;
        }
    }
    if(!file.exists()){message("Plugin missing on "+utf(project.tracks[(size_t)track].name)+": use LOAD VST3 / AU to relink it.",true);return;}
    juce::OwnedArray<juce::PluginDescription> found;
    for(int i=0;i<audio.formats.getNumFormats();++i)audio.formats.getFormat(i)->findAllTypesForFile(found,file.getFullPathName());
    juce::PluginDescription* selected=nullptr,*effect=nullptr;
    for(auto* d:found){if(d->createIdentifierString()==id)selected=d;if(d->isInstrument&&selected==nullptr)selected=d;if(!d->isInstrument&&effect==nullptr)effect=d;}
    if(selected==nullptr)selected=effect;
    if(!selected){message("Nessuno strumento compatibile trovato. Scegli il bundle VST3.",true);return;}
    loadPluginDescription(track,*selected,state,id.isNotEmpty());
}
void MainComponent::loadEffectFile(int track,const juce::File& file,const juce::String& id,const juce::String& state,int slot){
    if(id.isNotEmpty())if(auto xml=juce::XmlDocument::parse(id)){juce::PluginDescription description;if(description.loadFromXml(*xml)){if(description.pluginFormatName=="VST3"&&!juce::File(description.fileOrIdentifier).exists()){message("Plugin missing: use LOAD VST3 / AU to relink it.",true);return;}loadEffectDescription(track,description,state,true,slot);return;}}
    if(!file.exists()){message("Plugin missing on "+utf(effectTrack(track).name)+": use LOAD VST3 / AU to relink it.",true);return;}
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
    const auto plugins=pluginCatalog.state().plugins;if(!juce::isPositiveAndBelow(row.plugin,plugins.size()))return;
    if(effectsBrowser){if(!plugins.getReference(row.plugin).isInstrument){loadEffectDescription(track,plugins.getReference(row.plugin),{},false,browserEffectSlot);pluginBrowser.setVisible(false);}}else{loadPluginDescription(track,plugins.getReference(row.plugin));if(!plugins.getReference(row.plugin).isInstrument)pluginBrowser.setVisible(false);}
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
void MainComponent::importFile(const juce::File& f,double placeAt){auto imported=clip();const double duration=clipIndex()<0?4.:clipLength();size_t used=0;for(size_t ti=0;ti<project.tracks.size();++ti)for(size_t ci=0;ci<project.tracks[ti].clips.size();++ci)if((int)ti!=selectedTrack||(int)ci!=clipIndex())used+=project.tracks[ti].clips[ci].notes.size()+project.tracks[ti].clips[ci].loopSource.size();auto r=importMidi(f,imported,duration,used>=maxProjectNotes?0:maxProjectNotes-used);if(r.failed()){message(r.getErrorMessage(),true);return;}if(clipIndex()<0){imported.span=duration;auto edited=project;const int placed=edited.placeClip(selectedTrack,std::move(imported),arrangement.snapBeat(placeAt>=0?placeAt:timelinePosition()));if(placed<0)return;checkpoint();project=std::move(edited);selectClip(selectedTrack,placed);}else{checkpoint();clip()=std::move(imported);}piano.clearSelection();changed();message("MIDI importato nel clip selezionato.");}

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
    if(editingRestricted()||loadingPlugin)return;
    class ChordCirclePopover : public juce::Component {
    public:
        MainComponent& owner;
        CircleOfFifths circle;
        std::array<juce::TextButton,8> slotButtons;
        ActionButton randomBtn{tr("NEW CHORDS")},presetsBtn{tr("CHORD MENU...")};
        IconButton close{IconButton::Icon::close};
        int activeSlot=0;
        ChordCirclePopover(MainComponent& o,int slot):owner(o),activeSlot(std::clamp(slot,0,std::max(0,o.project.settings.chordCount-1))){
            addAndMakeVisible(circle);
            circle.setFamilyFont(true);
            circle.setFreeMode(true);
            circle.onPick=[this](int root,bool minor){
                owner.checkpoint();auto& s=owner.project.settings;
                if(juce::ModifierKeys::currentModifiers.isShiftDown()){
                    const bool modeMinor=sgk::scaleIsMinor(s.scale);s.key=root;if(minor!=modeMinor)s.scale=minor?2:0;
                }else{
                    const int sc=std::clamp(activeSlot,0,std::max(0,s.chordCount-1));
                    s.freeChords[(size_t)sc]=sgk::freeChordCode(root-s.key,minor);s.manualChords[(size_t)sc]=true;
                }
                owner.applyInstrumentSettings(0,true);refreshState();
            };
            circle.onMoveChord=[this](int sl,int degree,int root,bool minor){
                auto& s=owner.project.settings;if(sl<0||sl>=s.chordCount)return;
                activeSlot=sl;owner.checkpoint();
                if(degree>=0){s.degrees[(size_t)sl]=degree;s.freeChords[(size_t)sl]=-1;s.manualChords[(size_t)sl]=false;}
                else{s.freeChords[(size_t)sl]=sgk::freeChordCode(root-s.key,minor);s.manualChords[(size_t)sl]=true;}
                owner.applyInstrumentSettings(0,true);refreshState();
            };
            for(int i=0;i<8;++i){
                auto& b=slotButtons[(size_t)i];addAndMakeVisible(b);
                b.onClick=[this,i]{activeSlot=i;refreshState();};
            }
            randomBtn.setPrimary(false);presetsBtn.setPrimary(false);
            addAndMakeVisible(randomBtn);addAndMakeVisible(presetsBtn);
            randomBtn.onClick=[this]{if(owner.harmonyButton.onClick)owner.harmonyButton.onClick();refreshState();};
            presetsBtn.onClick=[this]{owner.showChordMenu();};
            addAndMakeVisible(close);close.setComponentID("chord-circle-close");close.accentHover=true;close.setTooltip(tr("Close"));
            close.onClick=[this]{if(auto* callout=findParentComponentOfClass<juce::CallOutBox>())callout->dismiss();};
            setSize(340,410);
            refreshState();
        }
        void refreshState(){
            auto s=pianoEngineSettings(owner.project,0,owner.clip().seed,owner.clip().motifSeed);
            std::vector<CircleOfFifths::ChordMark> chords,diatonic;
            std::array<sgk::Chord,8> cs;std::array<sgk::VoicedChord,8> voiced;
            for(int i=0;i<s.progressionLength;++i){cs[(size_t)i]=sgk::GrooveEngine::chordForSlot(s,i);voiced[(size_t)i]=sgk::voiceChord(cs[(size_t)i],s.voicing,s.centre,nullptr);}
            auto scores=sgk::GrooveEngine::harmonyScores(s,cs,voiced);std::vector<float> transitions;
            for(int i=0;i<s.progressionLength;++i){const auto& c=cs[(size_t)i];chords.push_back({c.rootPc,c.third==3,juce::String::fromUTF8(sgk::GrooveEngine::numeralForSlot(s,i).c_str()),scores.slot[(size_t)i]});transitions.push_back(scores.transition[(size_t)i]);}
            for(int i=0;i<7;++i){auto c=sgk::buildChord(s.keyRoot,s.scale,i,s.extension);diatonic.push_back({c.rootPc,c.third==3,juce::String::fromUTF8(sgk::romanNumeral(i,c).c_str()),1});}
            circle.setState(s.keyRoot,sgk::scaleIsMinor(s.scale),tr(sgk::scaleName(s.scale)),chords,transitions,activeSlot,diatonic);
            const auto& names=owner.cachedChordNames();
            for(int i=0;i<8;++i){
                auto& b=slotButtons[(size_t)i];
                const bool vis=i<(int)names.size();b.setVisible(vis);
                if(vis){b.setButtonText(juce::String(i+1)+": "+utf(names[(size_t)i]));b.setToggleState(i==activeSlot,juce::dontSendNotification);}
            }
            resized();repaint();
        }
        void paint(juce::Graphics& g)override{
            Daw::fillElevatedSurface(g,getLocalBounds().toFloat(),Daw::bgPanel(),Daw::panelRadius);
            Daw::drawCaps(g,tr("CIRCLE OF FIFTHS — CLICK TO REVOICE SLOT")+" "+juce::String(activeSlot+1),{12,6,getWidth()-52,18},Daw::accent);
        }
        void resized()override{
            const int count=std::clamp(owner.project.settings.chordCount,1,8);
            const int cols=std::min(4,count),rows=(count+cols-1)/cols;
            const int bw=(getWidth()-20)/cols;
            close.setBounds(getWidth()-30,2,24,24);
            for(int i=0;i<count;++i)slotButtons[(size_t)i].setBounds(10+(i%cols)*bw,26+(i/cols)*26,bw-4,24);
            const int topY=28+rows*26;
            circle.setBounds(10,topY,getWidth()-20,getHeight()-topY-40);
            randomBtn.setBounds(10,getHeight()-34,(getWidth()-26)/2,26);
            presetsBtn.setBounds(16+(getWidth()-26)/2,getHeight()-34,(getWidth()-26)/2,26);
        }
    };
    const auto target=chordEditButton.isShowing()?chordEditButton.getScreenBounds():getScreenBounds().withSizeKeepingCentre(120,28);
    juce::CallOutBox::launchAsynchronously(std::make_unique<ChordCirclePopover>(*this,initialSlot),target,nullptr);
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
