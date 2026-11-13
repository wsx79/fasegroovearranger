// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MixerRoutingWindow.h"
#include "MainComponent.h"
#include "Localisation.h"
#include "StudioTheme.h"
#include "EffectsUi.h"
#include "NativeCompressor.h"
namespace fga {
namespace {
class BusMeter : public juce::Component,public juce::SettableTooltipClient {
public:
    float level=0;const juce::Slider* scale=nullptr;
    juce::String valueText()const{return level>.00001f?juce::String(juce::Decibels::gainToDecibels(level,-60.f),1)+" dB":tr("Silence");}
    void paint(juce::Graphics& g)override{
        auto r=getLocalBounds().toFloat();float norm=0;
        if(level>.001f&&scale){const float db=std::clamp(juce::Decibels::gainToDecibels(level,-60.f),-60.f,6.f);const float y=std::clamp(scale->getY()+scale->getPositionOfValue(db)-getY(),r.getY(),r.getBottom());norm=1.f-(y-r.getY())/std::max(1.f,r.getHeight());}
        Daw::drawLedMeter(g,r,norm);
    }
};
const char* displayBusTitle(int bus){return bus==0?"GROUP 1":bus==1?"GROUP 2":bus==2?"REVERB":"DELAY";}
const char* busTitle(int bus){return bus==0?"GROUP A":bus==1?"GROUP B":bus==2?"REVERB":"DELAY";}

}
class MixerRoutingWindow::Panel : public juce::Component,private juce::Timer {
public:
    explicit Panel(MainComponent& o):owner(o){
        setLookAndFeel(&faderTheme);
        addAndMakeVisible(target);addAndMakeVisible(route);
        Localisation::bindTip(target,"Choose the track whose output and effect sends you want to mix.");Localisation::bindTip(route,"Send this track directly to the master or through one of the two groups.");
        target.setScrollWheelEnabled(false);route.setScrollWheelEnabled(false);target.onChange=[this]{if(!updating&&target.getSelectedId()>0){owner.select(target.getSelectedId()-1,owner.selectedSection);refresh();}};
        route.onChange=[this]{if(updating)return;owner.checkpoint();selected().group=route.getSelectedId()-2;owner.mixChanged();};
        for(int i=0;i<2;++i){auto& send=sends[(size_t)i];addAndMakeVisible(send);addAndMakeVisible(sendCaptions[(size_t)i]);Localisation::bindLabel(sendCaptions[(size_t)i],i==0?"A - REVERB":"B - DELAY");sendCaptions[(size_t)i].setFont(StudioStyle::font(Daw::small));sendCaptions[(size_t)i].setJustificationType(juce::Justification::centred);send.setComponentID(i==0?"send-a-level":"send-b-level");send.setRange(0,200,1);send.setColour(juce::Slider::rotarySliderFillColourId,Daw::returnColour(i));rotary(send);send.setTextValueSuffix(" %");send.setDoubleClickReturnValue(true,25);owner.trackGesture(send,{},true);Localisation::bindTip(send,i==0?"Shared return A level for every enabled track. Editing this unifies historic per-track send amounts.":"Shared return B level for every enabled track. Editing this unifies historic per-track send amounts.");send.onValueChange=[this,i]{if(updating)return;owner.checkpointMix(&sends[(size_t)i]);owner.project.buses[(size_t)(i+2)].gain=sends[(size_t)i].getValue()/100.;for(auto& track:owner.project.tracks)if(track.sends[(size_t)i]>0)track.sends[(size_t)i]=1.;owner.mixChanged();};}
        for(int bus=0;bus<4;++bus){
            auto* name=control<juce::Label>();names[(size_t)bus]=name;name->setEditable(false,true,false);name->setFont(StudioStyle::font(14,true));name->setMinimumHorizontalScale(.75f);name->setColour(juce::Label::textColourId,bus>=2?Daw::returnColour(bus-2):StudioStyle::text);name->onEditorShow=[this,name,bus]{if(auto* editor=name->getCurrentTextEditor())editor->setText(juce::String(owner.project.buses[(size_t)bus].name),false);};Localisation::bindTip(*name,"Double-click to rename this group or return.");name->onTextChange=[this,name,bus]{if(updating)return;auto value=name->getText().trim();if(value.isEmpty()){refresh();return;}if(value.toStdString()==owner.project.buses[(size_t)bus].name)return;owner.checkpoint();owner.project.buses[(size_t)bus].name=value.substring(0,40).toStdString();owner.mixChanged();};updates.push_back([this,name,bus]{auto raw=juce::String::fromUTF8(owner.project.buses[(size_t)bus].name.c_str());name->setText(raw==busTitle(bus)?tr(displayBusTitle(bus)):raw,juce::dontSendNotification);});
            scalar(bus,"LEVEL",&BusSettings::gain,0,2);scalar(bus,"PAN",&BusSettings::pan,-1,1);
            for(bool solo:{false,true}){auto* b=control<juce::TextButton>();(solo?solos:mutes)[(size_t)bus]=b;b->setButtonText(solo?"S":"M");b->setColour(juce::TextButton::buttonOnColourId,solo?StudioStyle::solo:StudioStyle::mute);b->setComponentID("bus-"+juce::String(bus)+(solo?"-solo":"-mute"));Localisation::bindTip(*b,solo?"Listen to this mixer group or return on its own.":"Mute this mixer group or return.");b->onClick=[this,b,bus,solo]{owner.checkpoint();auto& settings=owner.project.buses[(size_t)bus];auto& value=solo?settings.solo:settings.mute;value=!value;b->setToggleState(value,juce::dontSendNotification);owner.mixChanged();};updates.push_back([this,b,bus,solo]{const auto& settings=owner.project.buses[(size_t)bus];b->setToggleState(solo?settings.solo:settings.mute,juce::dontSendNotification);});}
            if(bus<2){
                auto* members=control<juce::Label>();memberLabels[(size_t)bus]=members;members->setFont(StudioStyle::font(Daw::small));updates.push_back([this,members,bus]{juce::StringArray tracks;for(const auto& track:owner.project.tracks)if(track.group==bus)tracks.add(juce::String(track.name));members->setText(tracks.isEmpty()?tr("No tracks assigned to this group"):tr("Tracks:")+" "+tracks.joinIntoString(", "),juce::dontSendNotification);members->setTooltip(members->getText());members->setColour(juce::Label::textColourId,tracks.isEmpty()?StudioStyle::mute:StudioStyle::textDim);});
                scalar(bus,"SIDECHAIN",&BusSettings::sidechainAmount,0,1);
                scalar(bus,"ATTACK (ms)",&BusSettings::sidechainAttackMs,0,200);
                scalar(bus,"RELEASE (ms)",&BusSettings::sidechainReleaseMs,10,2000);
                scalar(bus,"CURVE",&BusSettings::sidechainCurve,.25,4);
                auto* source=control<juce::ComboBox>();sources[(size_t)bus]=source;source->setComponentID("bus-"+juce::String(bus)+"-source");source->setScrollWheelEnabled(false);
                Localisation::bindTip(*source,"Ducking source: an internal track before group routing. Muted tracks do not trigger; group mute does not mute the detector.");
                source->onChange=[this,source,bus]{if(updating||source->getSelectedId()<1)return;owner.checkpoint();owner.project.buses[(size_t)bus].sidechainSource=source->getSelectedId()-1;owner.mixChanged();};
                updates.push_back([this,source,bus]{for(int i=0;i<trackCount;++i){const auto name=i<(int)owner.project.tracks.size()?juce::String(owner.project.tracks[(size_t)i].name):"Track "+juce::String(i+1);if(source->getNumItems()<=i)source->addItem(name,i+1);else source->changeItemText(i+1,name);}source->setSelectedId(owner.project.buses[(size_t)bus].sidechainSource+1,juce::dontSendNotification);});
            }if(bus>=2){scalar(bus,bus==2?"ROOM":"CROSSFEED",&BusSettings::amount,0,1);scalar(bus,"DAMPING",&BusSettings::damping,0,1);}if(bus==3){scalar(bus,"FEEDBACK",&BusSettings::feedback,0,.9);scalar(bus,"DELAY (beats)",&BusSettings::division,.125,4);}
            addAndMakeVisible(meters[(size_t)bus]);meters[(size_t)bus].scale=levels[(size_t)bus];Localisation::bindTip(meters[(size_t)bus],"Mixer group or return output level.");
        }
        for(int i=0;i<2;++i){auto& tab=tabs[(size_t)i];addAndMakeVisible(tab);tab.setButtonText(tr(i==0?"RETURNS A / B":"GROUPS"));tab.setComponentID(i==0?"routing-returns-tab":"routing-groups-tab");tab.setColour(juce::TextButton::buttonColourId,Daw::chrome);tab.setColour(juce::TextButton::buttonOnColourId,juce::Colour(0xff3a3124));tab.setColour(juce::TextButton::textColourOffId,StudioStyle::textDim);tab.setColour(juce::TextButton::textColourOnId,StudioStyle::accent);tab.onClick=[this,i]{groupView=i==1;refresh();};chainViews[(size_t)i]=std::make_unique<EffectChainView>(owner);addAndMakeVisible(chainPorts[(size_t)i]);chainPorts[(size_t)i].setViewedComponent(chainViews[(size_t)i].get(),false);chainPorts[(size_t)i].setScrollBarsShown(true,false);}
        addAndMakeVisible(trackLabel);addAndMakeVisible(outputLabel);addAndMakeVisible(hint);addAndMakeVisible(convert);
        Localisation::bindLabel(trackLabel,"TRACK");Localisation::bindLabel(outputLabel,"OUTPUT");
        for(auto* label:{&trackLabel,&outputLabel,&hint}){label->setFont(StudioStyle::font(Daw::small));label->setColour(juce::Label::textColourId,StudioStyle::textDim);}
        convert.setButtonText(tr("Enable editable returns"));convert.onClick=[this]{juce::PopupMenu menu;menu.addSectionHeader(tr("Return effects will restart from their defaults. Undo restores the old routing."));menu.addItem(1,tr("Convert returns A / B"));juce::Component::SafePointer<Panel> safe(this);menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&convert),[safe](int choice){if(!safe||choice!=1||safe->owner.pluginsBusy()||safe->owner.editingRestricted())return;safe->autoConvert=true;safe->convertFixedReturns();safe->refresh();});};
        for(int i=0;i<2;++i){addAndMakeVisible(sourceLabels[(size_t)i]);Localisation::bindLabel(sourceLabels[(size_t)i],"SIDECHAIN SOURCE");sourceLabels[(size_t)i].setFont(StudioStyle::font(Daw::small));Localisation::bindLabel(sendCaptions[(size_t)i],i==0?"A - SHARED LEVEL":"B - SHARED LEVEL");}
        startTimerHz(15);refresh();
    }
    ~Panel()override{for(auto& port:chainPorts)port.setViewedComponent(nullptr,false);setLookAndFeel(nullptr);stopTimer();for(auto* slider:levels)if(slider)slider->setLookAndFeel(nullptr);}
    // Returns A / B are always editable: a project saved with the old fixed returns is converted
    // the first time this view shows them, to the preset chains (Reverb on A, Tempo Delay on B).
    // Send and return levels stay; one Undo restores the old routing.
    bool autoConvert=true;   // off while the self-test exercises the historic fixed returns
    void convertFixedReturns(){
        if(!autoConvert||groupView||owner.project.returnChains||!isShowing()||owner.pluginsBusy()||owner.editingRestricted())return;
        owner.checkpoint();owner.project.enableReturnChains();
        for(int b=0;b<2;++b)for(const auto& fx:owner.project.returnEffects[(size_t)b].effects)owner.loadEffectFile(returnEffectTrack+b,juce::File(),juce::String::fromUTF8(fx.pluginId.c_str()),juce::String::fromUTF8(fx.state.c_str()),fx.slot);
        owner.changed(false);owner.message(tr("Returns A / B are now editable: Reverb on A, Tempo Delay on B."));
    }
    void refresh(){if(owner.project.tracks.empty())return;convertFixedReturns();updating=true;
        juce::StringArray names;for(auto& track:owner.project.tracks)names.add(juce::String::fromUTF8(track.name.c_str()));if(names!=trackNames){trackNames=names;target.clear(juce::dontSendNotification);target.addItemList(names,1);}target.setSelectedId(index()+1,juce::dontSendNotification);target.setEnabled(!owner.audio.isRecording());
        const auto& buses=owner.project.buses;if(route.getNumItems()!=3){route.addItemList({"Direct to master","GROUP A","GROUP B"},1);}route.changeItemText(1,tr("Direct to master"));for(int i=0;i<2;++i){const auto name=juce::String::fromUTF8(buses[(size_t)i].name.c_str());route.changeItemText(i+2,tr(displayBusTitle(i))+(name==busTitle(i)||name==tr(busTitle(i))?juce::String():" / "+name));}route.setSelectedId(selected().group+2,juce::dontSendNotification);
        for(int i=0;i<2;++i)sends[(size_t)i].setValue(owner.project.buses[(size_t)(i+2)].gain*100,juce::dontSendNotification);for(auto& update:updates)update();
        for(int b=0;b<2;++b){if(chainSessions[(size_t)b]!=owner.project.returnEffects[(size_t)b].sessionId){chainSessions[(size_t)b]=owner.project.returnEffects[(size_t)b].sessionId;chainViews[(size_t)b]->setTrackSession(chainSessions[(size_t)b]);}else chainViews[(size_t)b]->refresh();if(owner.project.returnChains){auto& name=*this->names[(size_t)(b+2)];const auto raw=juce::String(owner.project.buses[(size_t)(b+2)].name);name.setText((b==0?"A  /  ":"B  /  ")+tr(raw),juce::dontSendNotification);}}
        tabs[0].setToggleState(!groupView,juce::dontSendNotification);tabs[1].setToggleState(groupView,juce::dontSendNotification);
        hint.setText(tr(groupView?"Groups receive track outputs. Sidechain reduces the tracks assigned to that group.":owner.project.returnChains?"A / B are shared by all tracks. Enable them in the mixer; set their common level and effects here.":"Historic project: fixed reverb / delay preserved. Conversion is optional."),juce::dontSendNotification);
        if(!groupView&&std::any_of(owner.project.tracks.begin(),owner.project.tracks.end(),[](const Track& t){return (t.sends[0]>0&&t.sends[0]!=1)||(t.sends[1]>0&&t.sends[1]!=1);}))hint.setText(tr("Historic individual sends preserved. Editing a common level makes all enabled sends equal."),juce::dontSendNotification);
        convert.setVisible(!groupView&&!owner.project.returnChains);convert.setEnabled(!owner.pluginsBusy()&&!owner.editingRestricted());
        trackLabel.setText(tr(groupView?"TRACK":"SHARED RETURNS"),juce::dontSendNotification);outputLabel.setText(tr(groupView?"OUTPUT":"ENABLE A / B IN THE MIXER"),juce::dontSendNotification);target.setVisible(groupView);route.setVisible(groupView);
        updating=false;resized();
        if((tips!=nullptr)!=Localisation::tipsEnabled()){if(Localisation::tipsEnabled())tips=std::make_unique<juce::TooltipWindow>(this,500);else tips.reset();}repaint();
    }
    void paint(juce::Graphics& g)override{
        g.fillAll(StudioStyle::base);Daw::deviceHeader(g,{16,4,getWidth()-32,34},tr("Bus & Send"),groupView?tr("Groups and sidechain"):tr("Shared returns A / B"),{});
        Daw::devicePanel(g,sourceFrame.toFloat(),StudioStyle::lineStrong);
        
        for(int slot=0;slot<2;++slot){const int i=groupView?slot:slot+2;const auto card=cards[(size_t)i];const auto tone=i>=2?Daw::returnColour(i-2):StudioStyle::accent;Daw::devicePanel(g,card.toFloat(),tone);
            g.setColour(tone);g.fillRoundedRectangle((float)card.getX()+12,(float)card.getY()+14,3.f,14.f,1.5f);
            g.setColour(juce::Colour(0xff222529));g.drawHorizontalLine(card.getY()+40,(float)card.getX()+12,(float)card.getRight()-12);
            if(groupView){auto* gain=levels[(size_t)i];juce::ignoreUnused(gain);}
            auto* gain=levels[(size_t)i];g.setFont(StudioStyle::font(9));const float x=gain->getX()+gain->getWidth()*.5f-18;
            if(groupView)for(double db:{6.,0.,-6.,-12.,-24.,-48.}){const float y=gain->getY()+gain->getPositionOfValue(db);g.setColour(StudioStyle::textFaint);g.fillRect(x-3,y,3.f,1.f);if(gain->getHeight()>130||db==0||db==-24)g.drawText(juce::String((int)db),juce::Rectangle<float>(x-30,y-5,23,10),juce::Justification::centredRight);}
            StudioStyle::drawCaps(g,tr("PAN"),{pans[(size_t)i]->getX(),pans[(size_t)i]->getBottom(),pans[(size_t)i]->getWidth(),14},StudioStyle::textFaint,juce::Justification::centred);
            if(i<2){const float reduction=owner.audio.busReduction(i);const float db=std::max(0.f,-juce::Decibels::gainToDecibels(std::max(1.e-6f,1.f-reduction),-120.f));{auto gr=juce::Rectangle<int>(card.getX()+12,card.getBottom()-32,std::min(220,card.getWidth()-138),22);StudioStyle::drawCaps(g,tr("GAIN REDUCTION"),gr.reduced(8,0),StudioStyle::textFaint);g.setFont(Daw::font(11.f,true));g.setColour(db>.05f?StudioStyle::accent:StudioStyle::textFaint);g.drawText("-"+juce::String(db,1)+" dB",gr.reduced(8,0),juce::Justification::centredRight);}}
            g.setFont(StudioStyle::font(Daw::small));g.setColour(StudioStyle::textDim);g.drawText(meters[(size_t)i].valueText(),signalReadouts[(size_t)i],juce::Justification::centred);
        }
    }
    void resized()override{
        auto area=getLocalBounds().reduced(16);area.removeFromTop(30);
        auto tabsArea=area.removeFromTop(30);tabs[0].setBounds(tabsArea.removeFromLeft(180));tabsArea.removeFromLeft(8);tabs[1].setBounds(tabsArea.removeFromLeft(150));area.removeFromTop(10);
        sourceFrame=area.removeFromTop(groupView?76:36);auto source=sourceFrame.reduced(12,6);auto left=source.removeFromLeft((source.getWidth()-12)/2);source.removeFromLeft(12);
        trackLabel.setBounds(left.removeFromTop(20));target.setBounds(left.removeFromTop(30));outputLabel.setBounds(source.removeFromTop(20));route.setBounds(source.removeFromTop(30));
        auto hintArea=area.removeFromTop(38);if(!groupView&&!owner.project.returnChains)convert.setBounds(hintArea.removeFromRight(220).reduced(0,4));hint.setBounds(hintArea);area.removeFromTop(4);
        const int width=(area.getWidth()-12)/2;
        for(int i=0;i<4;++i){const bool visible=groupView?i<2:i>=2;
            for(auto* c:std::initializer_list<juce::Component*>{names[(size_t)i],levels[(size_t)i],pans[(size_t)i],mutes[(size_t)i],solos[(size_t)i],&meters[(size_t)i]})c->setVisible(visible);
            for(auto& field:effects[(size_t)i]){field.slider->setVisible(visible&&(i<2||!owner.project.returnChains));field.caption->setVisible(field.slider->isVisible());}
            if(i<2){memberLabels[(size_t)i]->setVisible(groupView);sources[(size_t)i]->setVisible(groupView);sourceLabels[(size_t)i].setVisible(groupView);sends[(size_t)i].setVisible(!groupView);sendCaptions[(size_t)i].setVisible(!groupView);chainPorts[(size_t)i].setVisible(!groupView&&owner.project.returnChains);}
            if(!visible)continue;
            const int column=i%2;auto card=juce::Rectangle<int>(area.getX()+column*(width+12),area.getY(),width,area.getHeight());cards[(size_t)i]=card;names[(size_t)i]->setBounds(card.reduced(12,0).withTrimmedTop(8).withHeight(26).withTrimmedLeft(8));
            if(i<2)memberLabels[(size_t)i]->setBounds(card.reduced(12,0).withTrimmedTop(40).withHeight(20));
            auto body=card.reduced(12,0).withTrimmedTop(i<2?66:50).withTrimmedBottom(10);const int faderX=groupView?body.getRight()-100:body.getX();
            const int panTop=groupView?body.getY():body.getY()+86;pans[(size_t)i]->setBounds(faderX+13,panTop,74,50);
            const int faderY=panTop+68,footer=body.getBottom();levels[(size_t)i]->setVisible(groupView);levels[(size_t)i]->setBounds(faderX+1,faderY,80,std::max(28,footer-faderY-58));mutes[(size_t)i]->setBounds(faderX+8,footer-24,36,24);solos[(size_t)i]->setBounds(faderX+50,footer-24,36,24);signalReadouts[(size_t)i]={faderX,footer-48,100,16};
            const int meterTop=juce::roundToInt(levels[(size_t)i]->getY()+levels[(size_t)i]->getPositionOfValue(6)),meterBottom=juce::roundToInt(levels[(size_t)i]->getY()+levels[(size_t)i]->getPositionOfValue(-60));meters[(size_t)i].setBounds(faderX+66,meterTop,10,std::max(1,meterBottom-meterTop));
            if(i<2){auto controls=body.withTrimmedRight(116);const int cell=controls.getWidth()/2;
                sourceLabels[(size_t)i].setBounds(controls.withHeight(18));sources[(size_t)i]->setBounds(controls.withTrimmedTop(20).withHeight(28));
                for(size_t n=0;n<effects[(size_t)i].size();++n){auto field=juce::Rectangle<int>(controls.getX()+((int)n%2)*cell,controls.getY()+64+((int)n/2)*112,cell,108);auto& control=effects[(size_t)i][n];control.caption->setBounds(field.removeFromBottom(18));control.slider->setBounds(field.reduced(2,0));}
            }else{const int b=i-2;sends[(size_t)b].setBounds(faderX+13,body.getY(),74,62);sendCaptions[(size_t)b].setBounds(faderX,body.getY()+62,100,18);
                auto rack=body.withTrimmedLeft(116);auto& port=chainPorts[(size_t)b];port.setBounds(rack);chainViews[(size_t)b]->setSize(std::max(1,rack.getWidth()-port.getScrollBarThickness()),chainViews[(size_t)b]->preferredHeight());
                if(!owner.project.returnChains){const int cell=rack.getWidth()/2;for(size_t n=0;n<effects[(size_t)i].size();++n){auto field=juce::Rectangle<int>(rack.getX()+((int)n%2)*cell,rack.getY()+((int)n/2)*82,cell,82);auto& control=effects[(size_t)i][n];control.caption->setBounds(field.removeFromBottom(18));control.slider->setBounds(field.reduced(2,0));}}
            }
        }
        repaint();
    }
    bool verify(const juce::File& folder){
        autoConvert=false;const auto saved=owner.project;auto undos=owner.undoStack,redos=owner.redoStack;const int track=owner.selectedTrack;const bool dirty=owner.dirty;owner.select(0,owner.selectedSection);refresh();const auto midi=midiFile(owner.project);bool ok=true;const double originalGain=owner.project.buses[0].gain;owner.project.buses[0].gain=.7341;const auto precise=juce::JSON::toString(projectJson(owner.project),false,17);refresh();ok=ok&&precise==juce::JSON::toString(projectJson(owner.project),false,17);owner.project.buses[0].gain=originalGain;refresh();
        auto change=[&](std::function<void()> action,std::function<bool()> check){const auto count=owner.undoStack.size();action();ok=ok&&check()&&owner.undoStack.size()==count+1&&midiFile(owner.project)==midi;owner.undo(false);refresh();owner.undo(true);refresh();ok=ok&&check()&&midiFile(owner.project)==midi;owner.undo(false);refresh();};
        const int nextGroup=selected().group==1?0:1;change([&]{route.setSelectedId(nextGroup+2,juce::sendNotificationSync);},[&]{return selected().group==nextGroup;});
        selected().sends[0]=.23;owner.project.tracks[1].sends[0]=.48;const double nextSend=.62;change([&]{sends[0].setValue(nextSend*100,juce::sendNotificationSync);},[&]{return std::abs(owner.project.buses[2].gain-nextSend)<1e-9&&selected().sends[0]==1&&owner.project.tracks[1].sends[0]==1;});
        if(auto* gain=dynamic_cast<juce::Slider*>(findChildWithID("bus-0-LEVEL"))){const double next=owner.project.buses[0].gain>.5?-9.:-3.;change([&]{gain->setValue(next,juce::sendNotificationSync);},[&]{return std::abs(owner.project.buses[0].gain-juce::Decibels::decibelsToGain(next))<1e-9;});owner.project.buses[0].gain=.5;refresh();change([&]{gain->setValue(0,juce::sendNotificationSync);},[&]{return owner.project.buses[0].gain==1;});owner.project.buses[0].gain=originalGain;refresh();change([&]{gain->setValue(-60,juce::sendNotificationSync);},[&]{return owner.project.buses[0].gain==0;});}else ok=false;
        if(auto* pan=dynamic_cast<juce::Slider*>(findChildWithID("bus-1-PAN"))){change([&]{pan->setValue(pan->getValueFromText(owner.project.buses[1].pan<0?"R25":"L25"),juce::sendNotificationSync);},[&]{return std::abs(owner.project.buses[1].pan)==.25;});}else ok=false;
        if(auto* sidechain=dynamic_cast<juce::Slider*>(findChildWithID("bus-0-SIDECHAIN")))change([&]{sidechain->setValue(.63,juce::sendNotificationSync);},[&]{return std::abs(owner.project.buses[0].sidechainAmount-.63)<1e-9;});else ok=false;
        struct SidechainCheck{const char* title;double BusSettings::*member;double value;};
        for(const auto field:std::array<SidechainCheck,3>{{{"ATTACK (ms)",&BusSettings::sidechainAttackMs,37.},{"RELEASE (ms)",&BusSettings::sidechainReleaseMs,420.},{"CURVE",&BusSettings::sidechainCurve,1.7}}}){
            auto* slider=dynamic_cast<juce::Slider*>(findChildWithID("bus-0-"+juce::String(field.title)));if(!slider){ok=false;continue;}
            change([&]{slider->setValue(field.value,juce::sendNotificationSync);},[&]{return std::abs(owner.project.buses[0].*field.member-field.value)<1.e-9;});
        }
        change([&]{sources[0]->setSelectedId(2,juce::sendNotificationSync);},[&]{return owner.project.buses[0].sidechainSource==1;});
        const auto beforeRefresh=juce::JSON::toString(projectJson(owner.project),false,17);refresh();ok=ok&&beforeRefresh==juce::JSON::toString(projectJson(owner.project),false,17);
        if(auto* mute=dynamic_cast<juce::Button*>(findChildWithID("bus-2-mute"))){const bool old=owner.project.buses[2].mute;change([&]{mute->onClick();},[&]{return owner.project.buses[2].mute!=old;});}else ok=false;
        const auto json=juce::JSON::toString(projectJson(owner.project),false,17);const auto count=owner.undoStack.size();for(auto* c:getChildren())if(dynamic_cast<juce::Slider*>(c)||dynamic_cast<juce::ComboBox*>(c)){const auto p=c->getLocalBounds().getCentre().toFloat();const auto now=juce::Time::getCurrentTime();juce::MouseEvent e(juce::Desktop::getInstance().getMainMouseSource(),p,{},1,0,0,0,0,c,c,now,p,now,1,false);juce::MouseWheelDetails wheel{};wheel.deltaY=1;c->mouseWheelMove(e,wheel);}ok=ok&&count==owner.undoStack.size()&&json==juce::JSON::toString(projectJson(owner.project),false,17);
        BusMeter meter;ok=ok&&meter.valueText()==tr("Silence");meter.level=1;ok=ok&&meter.valueText()=="0.0 dB";meter.level=.5;ok=ok&&meter.valueText()=="-6.0 dB";
        const juce::Point<int> normal(getWidth(),getHeight());const bool previousView=groupView;
        for(auto size:{juce::Point<int>(940,600),normal})for(bool groups:{false,true}){groupView=groups;setSize(size.x,size.y);refresh();resized();for(int slot=0;slot<2;++slot){const int i=groups?slot:slot+2;ok=ok&&levels[(size_t)i]->getSliderStyle()==juce::Slider::LinearVertical&&pans[(size_t)i]->getSliderStyle()==juce::Slider::RotaryHorizontalVerticalDrag&&meters[(size_t)i].getHeight()>0;for(auto& field:effects[(size_t)i])if(field.slider->isVisible())ok=ok&&field.slider->getSliderStyle()==juce::Slider::RotaryHorizontalVerticalDrag;}
            for(auto* c:getChildren())if(c->isVisible()&&c!=tips.get())ok=ok&&getLocalBounds().contains(c->getBounds())&&!c->getBounds().isEmpty();}
        groupView=previousView;setSize(normal.x,normal.y);refresh();
        ok=ok&&effects[0].size()==4&&effects[1].size()==4&&effects[2].size()==2&&effects[3].size()==4&&sends[0].getSliderStyle()==juce::Slider::RotaryHorizontalVerticalDrag;
        for(auto* c:getChildren())if(c->isVisible()&&c!=tips.get())ok=ok&&getLocalBounds().contains(c->getBounds())&&!c->getBounds().isEmpty();auto image=createComponentSnapshot(getLocalBounds());if(auto out=folder.getChildFile("mixer-buses.png").createOutputStream())ok=juce::PNGImageFormat().writeImageToStream(image,*out)&&ok;else ok=false;
        {   // Historic fixed returns are converted the first time this view shows them (one Undo).
            autoConvert=true;owner.project.returnChains=false;for(auto& rack:owner.project.returnEffects)rack.effects.clear();groupView=false;const auto count=owner.undoStack.size();refresh();
            const auto& r=owner.project.returnEffects;const bool converted=!isShowing()||(owner.project.returnChains&&r[0].effects.size()==1&&r[0].effects[0].pluginId=="fga:native-reverb"&&r[1].effects.size()==1&&r[1].effects[0].pluginId=="fga:native-delay"&&owner.undoStack.size()==count+1&&!convert.isVisible());
            const auto settleUntil=juce::Time::getMillisecondCounterHiRes()+5000;while(owner.pluginsBusy()&&juce::Time::getMillisecondCounterHiRes()<settleUntil)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
            folder.getChildFile("mixer-return-conversion.txt").replaceWithText(!isShowing()?"NOT RUN routing window hidden":converted?"PASS historic fixed returns convert to Reverb / Tempo Delay chains on first view, one Undo":"FAIL historic return conversion");
            if(isShowing()&&owner.undoStack.size()>count)owner.undo(false);ok=ok&&converted;autoConvert=false;}   // stays off for the rest of this self-test session
        owner.project=saved;owner.undoStack=std::move(undos);owner.redoStack=std::move(redos);owner.selectedTrack=track;owner.dirty=dirty;owner.audio.updateMix(owner.project);owner.refreshControls();refresh();folder.getChildFile("mixer-routing.txt").replaceWithText(ok?"PASS sidechain source/attack/release/curve/depth Undo/Redo and MIDI preservation; default/minimum layout; output groups and rotary sends; dB faders map 0 dB to unity and floor to silence; editable pan and bus mute support Undo and preserve MIDI; vertical meters and faders align at minimum/default sizes; return FX knobs remain return-only; safe wheel gestures and plain meter labels":"FAIL mixer routing");return ok;
    }
    bool verifyChains(const juce::File& folder){
#if !FGA_SELF_TESTS
        juce::ignoreUnused(folder);return false;
#else
        owner.captureUndoPluginStates();const auto saved=owner.project;auto undos=owner.undoStack,redos=owner.redoStack;const bool dirty=owner.dirty;bool ok=true;juce::String report;
        auto check=[&](bool pass,const char* what){ok=ok&&pass;report+=juce::String(pass?"PASS ":"FAIL ")+what+"\n";};
        auto settle=[&]{const auto deadline=juce::Time::getMillisecondCounterHiRes()+10000;while((owner.pluginsBusy()||owner.restoringProject)&&juce::Time::getMillisecondCounterHiRes()<deadline)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);return !owner.pluginsBusy()&&!owner.restoringProject;};
        owner.project.enableReturnChains();owner.restoreInstruments();check(settle()&&owner.audio.effectPlugin(returnEffectTrack,1)&&owner.audio.effectPlugin(returnEffectTrack+1,1),"default A reverb and B delay load without external plugins");refresh();
        check(trackLabel.isVisible()&&outputLabel.isVisible()&&chainPorts[0].isVisible()&&chainPorts[1].isVisible()&&!convert.isVisible(),"return view has explicit track/output labels and two editable racks");
        // Common level changes adopt uniform activation, but merely loading an
        // old project preserves its per-track sends. One Undo restores both.
        owner.project.tracks[0].sends={.23,.32};owner.project.tracks[1].sends={.48,0};owner.project.tracks[2].sends={0,0};refresh();
        const auto historic=owner.project;const auto commonUndo=owner.undoStack.size();
        sends[0].setValue(62,juce::sendNotificationSync);
        check(owner.project.buses[2].gain==.62&&owner.project.tracks[0].sends[0]==1&&owner.project.tracks[1].sends[0]==1&&owner.project.tracks[2].sends[0]==0&&owner.project.tracks[0].sends[1]==.32&&owner.undoStack.size()==commonUndo+1,"A 62 percent applies to every enabled track, preserves B/off tracks and creates one Undo");
        owner.select(1,owner.selectedSection);refresh();check(sends[0].getValue()==62&&!target.isVisible()&&!route.isVisible(),"common return level does not change with selected track");
        Project sharedRead;check(projectFromJson(projectJson(owner.project),sharedRead).wasOk()&&sharedRead.buses[2].gain==.62&&sharedRead.tracks[0].sends[0]==sharedRead.tracks[1].sends[0],"shared return level and activations survive save/reopen without new project fields");
        owner.undo(false);settle();refresh();check(owner.project.buses[2].gain==historic.buses[2].gain&&owner.project.tracks[0].sends==historic.tracks[0].sends&&owner.project.tracks[1].sends==historic.tracks[1].sends,"Undo restores historical unequal sends and common gain exactly");
        owner.undo(true);settle();refresh();sends[0].setValue(0,juce::sendNotificationSync);check(owner.project.buses[2].gain==0&&owner.project.tracks[0].sends[0]==1&&owner.project.tracks[1].sends[0]==1,"zero common level keeps A activation for later restoration");
        sends[0].setValue(62,juce::sendNotificationSync);check(owner.project.buses[2].gain==.62&&owner.project.tracks[0].sends[0]==1,"common return restores after zero without per-track memory");

        owner.openHostedEditor(returnEffectTrack,1);auto* reverb=owner.audio.effectPlugin(returnEffectTrack,1);auto* reverbEditor=reverb?reverb->getActiveEditor():nullptr;auto* room=reverbEditor?dynamic_cast<juce::Slider*>(reverbEditor->findChildWithID("native-return-parameter-0")):nullptr;
        check(room!=nullptr,"reverb has themed editable controls");if(reverbEditor){if(auto stream=folder.getChildFile("reverb-editor.png").createOutputStream())juce::PNGImageFormat().writeImageToStream(reverbEditor->createComponentSnapshot(reverbEditor->getLocalBounds()),*stream);}if(room){const float before=reverb->getParameters()[0]->getValue();const auto count=owner.undoStack.size();room->setValue(.7,juce::sendNotificationSync);check(std::abs(reverb->getParameters()[0]->getValue()-.7f)<1e-6&&owner.undoStack.size()==count+1,"reverb parameter edit reaches DSP with one Undo");owner.undo(false);check(settle()&&std::abs(owner.audio.effectPlugin(returnEffectTrack,1)->getParameters()[0]->getValue()-before)<1e-6,"reverb parameter Undo restores state");owner.undo(true);check(settle()&&std::abs(owner.audio.effectPlugin(returnEffectTrack,1)->getParameters()[0]->getValue()-.7f)<1e-6,"reverb parameter Redo restores state");owner.undo(false);settle();auto readback=[&]{auto* fx=owner.audio.effectPlugin(returnEffectTrack,1);auto* ed=fx?fx->getActiveEditor():nullptr;auto* control=ed?dynamic_cast<juce::Slider*>(ed->findChildWithID("native-return-parameter-0")):nullptr;return control&&std::abs(control->getValue()-before)<1e-6;};const auto deadline=juce::Time::getMillisecondCounterHiRes()+2000;while(!readback()&&juce::Time::getMillisecondCounterHiRes()<deadline)juce::MessageManager::getInstance()->runDispatchLoopUntil(5);check(readback(),"reverb editor follows restored parameter");auto* current=owner.audio.effectPlugin(returnEffectTrack,1)->getActiveEditor();if(auto stream=folder.getChildFile("reverb-editor.png").createOutputStream())check(current&&juce::PNGImageFormat().writeImageToStream(current->createComponentSnapshot(current->getLocalBounds()),*stream),"reverb editor screenshot");}
        owner.closePluginEditor(returnEffectTrack,1);
        owner.openHostedEditor(returnEffectTrack+1,1);auto* delay=owner.audio.effectPlugin(returnEffectTrack+1,1);auto* delayEditor=delay?delay->getActiveEditor():nullptr;auto* delayTime=delayEditor?dynamic_cast<juce::Slider*>(delayEditor->findChildWithID("native-return-parameter-0")):nullptr;
        check(delayTime!=nullptr,"delay editor exposes beat-time control");if(delayEditor){if(auto stream=folder.getChildFile("delay-editor.png").createOutputStream())check(juce::PNGImageFormat().writeImageToStream(delayEditor->createComponentSnapshot(delayEditor->getLocalBounds()),*stream),"delay editor screenshot");for(const auto size:std::array<juce::Point<int>,2>{{{740,440},{1000,650}}}){delayEditor->setSize(size.x,size.y);bool fits=true;for(auto* child:delayEditor->getChildren())if(child->isVisible())fits=fits&&delayEditor->getLocalBounds().contains(child->getBounds())&&!child->getBounds().isEmpty();check(fits,"delay editor controls fit supported size");}}if(delayTime){const float before=delay->getParameters()[0]->getValue();delayTime->setValue(delayTime->getValueFromText("0,125 beats"),juce::sendNotificationSync);check(delay->getParameters()[0]->getValue()==0&&delayTime->getTextFromValue(delayTime->getValue()).startsWith("0.125"),"typed musical division accepts decimal comma without normalized-slider rounding");owner.undo(false);check(settle()&&std::abs(owner.audio.effectPlugin(returnEffectTrack+1,1)->getParameters()[0]->getValue()-before)<1e-6,"typed delay time supports Undo");}owner.closePluginEditor(returnEffectTrack+1,1);
        owner.addNativeCompressor(returnEffectTrack);check(settle()&&owner.project.returnEffects[0].effects.size()==2,"existing insert lifecycle adds compressor after reverb on A");
        auto* compressor=owner.audio.effectPlugin(returnEffectTrack,2);auto* editor=compressor?compressor->getActiveEditor():nullptr;
        auto* selector=editor?dynamic_cast<juce::ComboBox*>(editor->findChildWithID("native-compressor-source")):nullptr;
        check(selector!=nullptr,"compressor editor exposes explicit source selector");
        if(selector){const auto count=owner.undoStack.size();selector->setSelectedId(6,juce::sendNotificationSync);check(nativeCompressorSource(compressor)==4&&owner.undoStack.size()==count+1,"selecting Kick changes actual detector and creates one Undo");owner.undo(false);check(settle()&&nativeCompressorSource(owner.audio.effectPlugin(returnEffectTrack,2))==-1,"source selection Undo restores internal detector");owner.undo(true);check(settle()&&nativeCompressorSource(owner.audio.effectPlugin(returnEffectTrack,2))==4,"source selection Redo restores Kick");}
        owner.moveEffect(returnEffectTrack,2,0);check(owner.project.returnEffects[0].effects.front().slot==2,"return chain reorder preserves stable effect IDs");owner.undo(false);check(settle()&&owner.project.returnEffects[0].effects.front().slot==1,"return reorder supports Undo");
        owner.removeEffect(returnEffectTrack,1);check(!owner.audio.effectPlugin(returnEffectTrack,1)&&owner.project.returnEffects[0].effects.size()==1,"removing reverb retains compressor and really destroys only reverb");owner.undo(false);check(settle()&&owner.audio.effectPlugin(returnEffectTrack,1)&&owner.audio.effectPlugin(returnEffectTrack,2),"Undo reloads return chain and native state");
        owner.captureUndoPluginStates();Project loaded;check(projectFromJson(projectJson(owner.project),loaded).wasOk()&&loaded.returnChains&&loaded.returnEffects[0].effects.size()==2&&!loaded.returnEffects[0].effects[1].state.empty(),"return chains and compressor source state persist in version 7");
        auto snapshot=[&](const char* name){if(auto output=folder.getChildFile(name).createOutputStream())return juce::PNGImageFormat().writeImageToStream(createComponentSnapshot(getLocalBounds()),*output);return false;};
        groupView=false;refresh();check(snapshot("mixer-returns.png"),"editable returns screenshot");groupView=true;refresh();check(sources[0]->isVisible()&&!chainPorts[0].isVisible()&&snapshot("mixer-groups.png"),"groups remain a separate view with source/depth controls");
        owner.closePluginEditor();owner.project=saved;owner.restoreInstruments();const bool restored=settle();owner.undoStack=std::move(undos);owner.redoStack=std::move(redos);owner.dirty=dirty;groupView=false;owner.refreshControls();refresh();check(restored,"test restores original project and loaded processors");
        folder.getChildFile("mixer-return-chains.txt").replaceWithText(report);return ok;
#endif
    }

private:
    std::array<juce::TextButton,2> tabs;bool groupView=false;
    juce::Label trackLabel,outputLabel,hint;std::array<juce::Label,2> sourceLabels;juce::TextButton convert;
    std::array<std::uint64_t,2> chainSessions{};
    std::array<std::unique_ptr<EffectChainView>,2> chainViews;std::array<juce::Viewport,2> chainPorts;
    MainComponent& owner;std::uint64_t lastUiRevision=~std::uint64_t{0};bool lastBusy=false,lastRestricted=false;StudioLookAndFeel faderTheme;bool updating=false;juce::ComboBox target,route;juce::StringArray trackNames;std::array<juce::Slider,2>sends;std::array<juce::Label,2>sendCaptions;std::array<BusMeter,4>meters;std::array<juce::Rectangle<int>,4>cards,signalReadouts;juce::Rectangle<int>sourceFrame;
    std::array<juce::Label*,2>memberLabels{};std::array<juce::ComboBox*,2>sources{};std::array<juce::Slider*,4>levels{},pans{};std::array<juce::Label*,4>names{};std::array<juce::TextButton*,4>mutes{},solos{};
    struct EffectField{juce::Slider* slider;juce::Label* caption;};std::array<std::vector<EffectField>,4>effects;
    std::vector<std::unique_ptr<juce::Component>>controls;std::vector<std::function<void()>>updates;std::unique_ptr<juce::TooltipWindow>tips;
    int index()const{return std::clamp(owner.selectedTrack,0,(int)owner.project.tracks.size()-1);}Track& selected(){return owner.project.tracks[(size_t)index()];}
    template<class T>T* control(){auto item=std::make_unique<T>();auto* p=item.get();addAndMakeVisible(p);controls.push_back(std::move(item));return p;}
    void rotary(juce::Slider& s){s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,74,18);s.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);s.setScrollWheelEnabled(false);s.setMouseDragSensitivity(200);}
    void scalar(int bus,juce::String title,double BusSettings::*member,double low,double high){
        auto* s=control<juce::Slider>();const bool gain=member==&BusSettings::gain,pan=member==&BusSettings::pan;s->setComponentID("bus-"+juce::String(bus)+"-"+title);s->setRange(gain?-60:low,gain?6:high,gain?.1:.001);s->setScrollWheelEnabled(false);
        if(gain){levels[(size_t)bus]=s;s->setLookAndFeel(&faderTheme);s->setSliderStyle(juce::Slider::LinearVertical);s->setTextBoxStyle(juce::Slider::TextBoxBelow,false,76,18);s->setSkewFactor(2.2);s->setDoubleClickReturnValue(true,0);s->textFromValueFunction=[](double v){return v<=-59.99?tr("OFF"):juce::String(std::abs(v)<.05?0.:v,1)+" dB";};s->valueFromTextFunction=[](const juce::String& text){return (text.trim().equalsIgnoreCase(tr("OFF"))||text.trim().equalsIgnoreCase("OFF"))?-60.:text.getDoubleValue();};}
        else{rotary(*s);if(pan){pans[(size_t)bus]=s;s->setDoubleClickReturnValue(true,0);s->textFromValueFunction=[](double v){return std::abs(v)<.0005?juce::String("C"):(v<0?"L":"R")+juce::String(std::abs(v)*100,1);};s->valueFromTextFunction=[](juce::String text){text=text.trim().toUpperCase();if(text=="C")return 0.;const bool left=text.startsWithChar('L'),right=text.startsWithChar('R');double value=(left||right?text.substring(1):text).getDoubleValue()/100.;return left?-std::abs(value):right?std::abs(value):value;};}else{auto* label=control<juce::Label>();Localisation::bindLabel(*label,title);label->setFont(StudioStyle::font(Daw::small));label->setJustificationType(juce::Justification::centred);effects[(size_t)bus].push_back({s,label});}}
        if(member==&BusSettings::sidechainAmount){s->textFromValueFunction=[](double v){return juce::String(v*100,1)+" %";};s->valueFromTextFunction=[](const juce::String& text){return text.getDoubleValue()/100.;};}
        if(member==&BusSettings::sidechainAttackMs||member==&BusSettings::sidechainReleaseMs){s->setTextValueSuffix(" ms");s->setNumDecimalPlacesToDisplay(1);}
        if(member==&BusSettings::sidechainCurve)s->setNumDecimalPlacesToDisplay(2);
        if(member==&BusSettings::sidechainAttackMs)s->setDoubleClickReturnValue(true,0);
        if(member==&BusSettings::sidechainReleaseMs)s->setDoubleClickReturnValue(true,180);
        if(member==&BusSettings::sidechainCurve)s->setDoubleClickReturnValue(true,1);
        s->updateText();owner.trackGesture(*s,{},true);s->onValueChange=[this,s,bus,member,gain]{if(updating)return;owner.checkpointMix(s);owner.project.buses[(size_t)bus].*member=gain?(s->getValue()<=-59.99?0.:juce::Decibels::decibelsToGain(s->getValue())):s->getValue();owner.mixChanged();};Localisation::bindTip(*s,title=="SIDECHAIN"?"Ducking depth: zero disables reduction. The selected source is tapped before group mute and routing.":title=="ATTACK (ms)"?"Detector attack in milliseconds; zero preserves the historical instant attack.":title=="RELEASE (ms)"?"Detector decay in milliseconds, independent of tempo; default 180 ms.":title=="CURVE"?"Envelope exponent: 1 preserves the historical response; below 1 deepens the tail.":title=="LEVEL"?"Bus level in dB: 0 dB is unity. Double-click resets to 0 dB; the bottom position silences the bus.":title=="PAN"?"Pan: C centres the bus; enter L25 or R25 for 25% left or right. Double-click to centre.":title=="ROOM"?"Size and decay of the shared reverb.":title=="DAMPING"?"Reduce high frequencies in the effect tail.":title=="FEEDBACK"?"Amount of delay fed back for successive echoes.":title=="DELAY (beats)"?"Delay time in quarter-note beats, following the song tempo.":"Amount of stereo crossfeed between the delay channels.");updates.push_back([this,s,bus,member,gain]{const double value=owner.project.buses[(size_t)bus].*member;s->setValue(gain?juce::Decibels::gainToDecibels(value,-60.):value,juce::dontSendNotification);});
    }
    void timerCallback()override{if(!isShowing())return;if(groupView)for(int b=0;b<2;++b)repaint(cards[(size_t)b].getX(),cards[(size_t)b].getBottom()-28,cards[(size_t)b].getWidth(),16);for(int i=0;i<4;++i){auto& meter=meters[(size_t)i];float level=owner.audio.busLevel(i);if(meter.level!=level){meter.level=level;meter.repaint();repaint(signalReadouts[(size_t)i]);}}if(!juce::ModifierKeys::getCurrentModifiersRealtime().isAnyMouseButtonDown()&&!dynamic_cast<juce::TextEditor*>(juce::Component::getCurrentlyFocusedComponent())&&!target.isPopupActive()&&!route.isPopupActive()&&!sources[0]->isPopupActive()&&!sources[1]->isPopupActive())if(lastUiRevision!=owner.controlRevision()||lastBusy!=owner.pluginsBusy()||lastRestricted!=owner.editingRestricted()){lastUiRevision=owner.controlRevision();lastBusy=owner.pluginsBusy();lastRestricted=owner.editingRestricted();refresh();}}
};
MixerRoutingWindow::MixerRoutingWindow(MainComponent& m):DocumentWindow("FASE GROOVE - MIXER",StudioStyle::base,juce::DocumentWindow::closeButton),owner(m){setLookAndFeel(&m.theme);setUsingNativeTitleBar(true);panel=new Panel(m);setContentOwned(panel,true);setResizable(true,false);setResizeLimits(940,600,2880,1600);setSize(1020,720);setAlwaysOnTop(true);}
MixerRoutingWindow::~MixerRoutingWindow(){clearContentComponent();setLookAndFeel(nullptr);}
void MixerRoutingWindow::open(){panel->refresh();if(!isVisible())centreWithSize(getWidth(),getHeight());setAlwaysOnTop(true);setVisible(true);toFront(true);}
void MixerRoutingWindow::closeButtonPressed(){owner.dismissAuxiliaryWindow(*this);}
bool MixerRoutingWindow::verify(const juce::File& folder){bool ok=panel->verify(folder)&&owner.mixer.verifySends(folder)&&panel->verifyChains(folder);setTopLeftPosition(7,9);closeButtonPressed();open();ok=ok&&isAlwaysOnTop()&&getPosition()!=juce::Point<int>(7,9);return ok;}
}
