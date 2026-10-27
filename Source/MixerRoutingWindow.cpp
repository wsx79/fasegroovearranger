// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MixerRoutingWindow.h"
#include "MainComponent.h"
#include "Localisation.h"
namespace fga {
namespace {
class BusMeter : public juce::Component,public juce::SettableTooltipClient {
public:
    float level=0;
    juce::String valueText()const{return level>.00001f?juce::String(juce::Decibels::gainToDecibels(level,-60.f),1)+" dB":tr("Silence");}
    void paint(juce::Graphics& g)override{auto r=getLocalBounds().toFloat().reduced(0,7);auto bar=r.withTrimmedRight(65);g.setColour(Daw::well);g.fillRoundedRectangle(bar,2);float amount=std::clamp((juce::Decibels::gainToDecibels(level,-60.f)+60)/66,0.f,1.f);g.setColour(Daw::meterColour(level));g.fillRoundedRectangle(bar.withWidth(bar.getWidth()*amount),2);g.setColour(Daw::textDim);g.setFont(Daw::font(11));g.drawText(valueText(),getLocalBounds(),juce::Justification::centredRight);}
};
const char* busTitle(int bus){return bus==0?"GROUP A":bus==1?"GROUP B":bus==2?"A - REVERB":"B - DELAY";}
}
class MixerRoutingWindow::Panel : public juce::Component,private juce::Timer {
public:
    explicit Panel(MainComponent& o):owner(o){
        addAndMakeVisible(target);addAndMakeVisible(route);
        Localisation::bindTip(target,"Choose the track whose output and effect sends you want to mix.");Localisation::bindTip(route,"Send this track directly to the master or through one of the two groups.");
        target.setScrollWheelEnabled(false);route.setScrollWheelEnabled(false);target.onChange=[this]{if(!updating&&target.getSelectedId()>0){owner.select(target.getSelectedId()-1,owner.selectedSection);refresh();}};
        route.onChange=[this]{if(updating)return;owner.checkpoint();selected().group=route.getSelectedId()-2;owner.mixChanged();};
        for(int i=0;i<2;++i){auto& send=sends[(size_t)i];addAndMakeVisible(send);send.setComponentID(i==0?"send-a-level":"send-b-level");send.setRange(0,100,1);send.setTextBoxStyle(juce::Slider::TextBoxLeft,false,180,24);send.setScrollWheelEnabled(false);send.setDoubleClickReturnValue(true,0);owner.trackGesture(send);Localisation::bind(send,[&send,i]{send.setTextValueSuffix(" %  "+tr(i==0?"A - REVERB":"B - DELAY"));});Localisation::bindTip(send,i==0?"Amount of this track sent to the shared reverb.":"Amount of this track sent to the shared tempo delay.");send.onValueChange=[this,i]{if(updating)return;owner.checkpoint(&sends[(size_t)i]);selected().sends[(size_t)i]=sends[(size_t)i].getValue()/100.;owner.mixChanged();};}
        for(int bus=0;bus<4;++bus){auto* name=control<juce::Label>(bus);name->setEditable(false,true,false);name->setFont(Daw::font(13,true));Localisation::bindTip(*name,"Double-click to rename this group or return.");name->onTextChange=[this,name,bus]{if(updating)return;auto value=name->getText().trim();if(value.isEmpty()){refresh();return;}owner.checkpoint();owner.project.buses[(size_t)bus].name=value.substring(0,40).toStdString();owner.mixChanged();};updates.push_back([this,name,bus]{name->setText(juce::String(owner.project.buses[(size_t)bus].name),juce::dontSendNotification);});
            scalar(bus,"LEVEL",&BusSettings::gain,0,2);scalar(bus,"PAN",&BusSettings::pan,-1,1);
            for(bool solo:{false,true}){auto* b=control<juce::ToggleButton>(bus);Localisation::bindText(*b,solo?"SOLO":"MUTE");b->setComponentID("bus-"+juce::String(bus)+(solo?"-solo":"-mute"));Localisation::bindTip(*b,solo?"Listen to this mixer group or return on its own.":"Mute this mixer group or return.");b->onClick=[this,bus,solo]{owner.checkpoint();auto& settings=owner.project.buses[(size_t)bus];auto& value=solo?settings.solo:settings.mute;value=!value;owner.mixChanged();};updates.push_back([this,b,bus,solo]{const auto& settings=owner.project.buses[(size_t)bus];b->setToggleState(solo?settings.solo:settings.mute,juce::dontSendNotification);});}
            if(bus>=2){scalar(bus,bus==2?"ROOM":"CROSSFEED",&BusSettings::amount,0,1);scalar(bus,"DAMPING",&BusSettings::damping,0,1);}if(bus==3){scalar(bus,"FEEDBACK",&BusSettings::feedback,0,.9);scalar(bus,"DELAY (beats)",&BusSettings::division,.125,4);}
            addAndMakeVisible(meters[(size_t)bus]);Localisation::bindTip(meters[(size_t)bus],"Mixer group or return output level.");
        }
        startTimerHz(15);refresh();
    }
    void refresh(){if(owner.project.tracks.empty())return;updating=true;
        juce::StringArray names;for(auto& track:owner.project.tracks)names.add(juce::String(track.name));if(names!=trackNames){trackNames=names;target.clear(juce::dontSendNotification);target.addItemList(names,1);}target.setSelectedId(index()+1,juce::dontSendNotification);target.setEnabled(!owner.audio.isRecording());
        const auto& buses=owner.project.buses;if(route.getNumItems()!=3){route.addItemList({"Direct to master","GROUP A","GROUP B"},1);}route.changeItemText(1,tr("Direct to master"));for(int i=0;i<2;++i){const auto name=juce::String(buses[(size_t)i].name);route.changeItemText(i+2,tr(busTitle(i))+(name==busTitle(i)||name==tr(busTitle(i))?juce::String():" / "+name));}route.setSelectedId(selected().group+2,juce::dontSendNotification);
        for(int i=0;i<2;++i)sends[(size_t)i].setValue(selected().sends[(size_t)i]*100,juce::dontSendNotification);for(auto& update:updates)update();updating=false;
        if((tips!=nullptr)!=Localisation::tipsEnabled()){if(Localisation::tipsEnabled())tips=std::make_unique<juce::TooltipWindow>(this,500);else tips.reset();}repaint();
    }
    void paint(juce::Graphics& g)override{g.fillAll(Daw::base);g.setColour(Daw::accent);g.setFont(Daw::font(18,true));g.drawText(tr("BUS & SEND"),16,12,getWidth()-32,30,juce::Justification::centredLeft);for(int i=0;i<4;++i){auto r=cards[(size_t)i].toFloat();g.setColour(Daw::surface);g.fillRoundedRectangle(r,Daw::radius);g.setColour(Daw::line);g.drawRoundedRectangle(r,Daw::radius,.7f);Daw::drawCaps(g,tr(busTitle(i)),cards[(size_t)i].reduced(12,4).withHeight(24),i>=2?Daw::accent:Daw::textDim);}}
    void resized()override{auto r=getLocalBounds().reduced(16);r.removeFromTop(40);auto row=r.removeFromTop(32);target.setBounds(row.removeFromLeft((row.getWidth()-12)/2));row.removeFromLeft(12);route.setBounds(row);r.removeFromTop(10);row=r.removeFromTop(32);for(auto& send:sends){send.setBounds(row.removeFromLeft((r.getWidth()-12)/2));row.removeFromLeft(12);}r.removeFromTop(16);int width=(r.getWidth()-24)/4;for(int bus=0;bus<4;++bus){cards[(size_t)bus]=r.removeFromLeft(width);auto content=cards[(size_t)bus].reduced(10,8);content.removeFromTop(26);for(auto* c:columns[(size_t)bus])c->setBounds(content.removeFromTop(32).reduced(0,2));meters[(size_t)bus].setBounds(content.removeFromTop(28));r.removeFromLeft(8);}}
    bool verify(const juce::File& folder){
        const auto saved=owner.project;auto undos=owner.undoStack,redos=owner.redoStack;const int track=owner.selectedTrack;const bool dirty=owner.dirty;owner.select(0,owner.selectedSection);refresh();const auto midi=midiFile(owner.project);bool ok=true;
        auto change=[&](std::function<void()> action,std::function<bool()> check){const auto count=owner.undoStack.size();action();ok=ok&&check()&&owner.undoStack.size()==count+1&&midiFile(owner.project)==midi;owner.undo(false);refresh();};
        const int nextGroup=selected().group==1?0:1;change([&]{route.setSelectedId(nextGroup+2,juce::sendNotificationSync);},[&]{return selected().group==nextGroup;});
        const double nextSend=selected().sends[0]>.4?.23:.61;change([&]{sends[0].setValue(nextSend*100,juce::sendNotificationSync);},[&]{return std::abs(selected().sends[0]-nextSend)<1e-9;});
        if(auto* gain=dynamic_cast<juce::Slider*>(findChildWithID("bus-0-LEVEL"))){const double next=owner.project.buses[0].gain>.5?.3:.9;change([&]{gain->setValue(next,juce::sendNotificationSync);},[&]{return std::abs(owner.project.buses[0].gain-next)<1e-9;});}else ok=false;
        if(auto* mute=dynamic_cast<juce::Button*>(findChildWithID("bus-2-mute"))){const bool old=owner.project.buses[2].mute;change([&]{mute->onClick();},[&]{return owner.project.buses[2].mute!=old;});}else ok=false;
        const auto json=juce::JSON::toString(projectJson(owner.project),false,17);const auto count=owner.undoStack.size();for(auto* c:getChildren())if(dynamic_cast<juce::Slider*>(c)||dynamic_cast<juce::ComboBox*>(c)){const auto p=c->getLocalBounds().getCentre().toFloat();const auto now=juce::Time::getCurrentTime();juce::MouseEvent e(juce::Desktop::getInstance().getMainMouseSource(),p,{},1,0,0,0,0,c,c,now,p,now,1,false);juce::MouseWheelDetails wheel{};wheel.deltaY=1;c->mouseWheelMove(e,wheel);}ok=ok&&count==owner.undoStack.size()&&json==juce::JSON::toString(projectJson(owner.project),false,17);
        BusMeter meter;ok=ok&&meter.valueText()==tr("Silence");meter.level=1;ok=ok&&meter.valueText()=="0.0 dB";meter.level=.5;ok=ok&&meter.valueText()=="-6.0 dB";
        for(auto* c:getChildren())if(c->isVisible()&&c!=tips.get())ok=ok&&getLocalBounds().contains(c->getBounds())&&!c->getBounds().isEmpty();auto image=createComponentSnapshot(getLocalBounds());if(auto out=folder.getChildFile("mixer-buses.png").createOutputStream())ok=juce::PNGImageFormat().writeImageToStream(image,*out)&&ok;else ok=false;
        owner.project=saved;owner.undoStack=std::move(undos);owner.redoStack=std::move(redos);owner.selectedTrack=track;owner.dirty=dirty;owner.audio.updateMix(owner.project);owner.refreshControls();refresh();folder.getChildFile("mixer-routing.txt").replaceWithText(ok?"PASS mixer output groups, send amounts, bus gain/mute and Undo preserve MIDI; safe wheel gestures, meter labels and panel layout":"FAIL mixer routing");return ok;
    }
private:
    MainComponent& owner;bool updating=false;juce::ComboBox target,route;juce::StringArray trackNames;std::array<juce::Slider,2>sends;std::array<BusMeter,4>meters;std::array<juce::Rectangle<int>,4>cards;
    std::vector<std::unique_ptr<juce::Component>>controls;std::array<std::vector<juce::Component*>,4>columns;std::vector<std::function<void()>>updates;std::unique_ptr<juce::TooltipWindow>tips;
    int index()const{return std::clamp(owner.selectedTrack,0,(int)owner.project.tracks.size()-1);}Track& selected(){return owner.project.tracks[(size_t)index()];}
    template<class T>T* control(int bus){auto item=std::make_unique<T>();auto* p=item.get();addAndMakeVisible(p);columns[(size_t)bus].push_back(p);controls.push_back(std::move(item));return p;}
    void scalar(int bus,juce::String title,double BusSettings::*member,double low,double high){auto* s=control<juce::Slider>(bus);s->setComponentID("bus-"+juce::String(bus)+"-"+title);s->setRange(low,high,.001);s->setScrollWheelEnabled(false);Localisation::bind(*s,[s,title]{s->setTextValueSuffix(" "+tr(title));s->setTextBoxStyle(juce::Slider::TextBoxLeft,false,std::clamp(40+(int)juce::GlyphArrangement::getStringWidth(Daw::font(11),tr(title)),80,150),24);});owner.trackGesture(*s);s->onValueChange=[this,s,bus,member]{if(updating)return;owner.checkpoint(s);owner.project.buses[(size_t)bus].*member=s->getValue();owner.mixChanged();};Localisation::bindTip(*s,title=="LEVEL"?"Mixer group or return output level.":title=="PAN"?"Mixer group or return stereo position.":title=="ROOM"?"Size and decay of the shared reverb.":title=="DAMPING"?"Reduce high frequencies in the effect tail.":title=="FEEDBACK"?"Amount of delay fed back for successive echoes.":title=="DELAY (beats)"?"Delay time in quarter-note beats, following the song tempo.":"Amount of stereo crossfeed between the delay channels.");updates.push_back([this,s,bus,member]{s->setValue(owner.project.buses[(size_t)bus].*member,juce::dontSendNotification);});}
    void timerCallback()override{if(!isShowing())return;for(int i=0;i<4;++i){auto& meter=meters[(size_t)i];float level=owner.audio.busLevel(i);if(meter.level!=level){meter.level=level;meter.repaint();}}if(!juce::ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown()&&!dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent())&&!target.isPopupActive()&&!route.isPopupActive())refresh();}
};
MixerRoutingWindow::MixerRoutingWindow(MainComponent& owner):DocumentWindow("FASE GROOVE - MIXER",Daw::base,juce::DocumentWindow::closeButton){setLookAndFeel(&owner.theme);setUsingNativeTitleBar(true);panel=new Panel(owner);setContentOwned(panel,true);setResizable(true,false);setResizeLimits(940,600,1600,1000);setSize(1020,660);setAlwaysOnTop(true);}
MixerRoutingWindow::~MixerRoutingWindow(){clearContentComponent();setLookAndFeel(nullptr);}
void MixerRoutingWindow::open(){panel->refresh();if(!isVisible())centreWithSize(getWidth(),getHeight());setAlwaysOnTop(true);setVisible(true);toFront(true);}
void MixerRoutingWindow::closeButtonPressed(){setVisible(false);}
bool MixerRoutingWindow::verify(const juce::File& folder){bool ok=panel->verify(folder);setTopLeftPosition(7,9);closeButtonPressed();open();ok=ok&&isAlwaysOnTop()&&getPosition()!=juce::Point<int>(7,9);return ok;}
}
