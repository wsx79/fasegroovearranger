// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "GraphicEqWindow.h"
#include "MainComponent.h"
#include "AudioEngine.h"
#include "core/EqResponse.h"
#include "core/EqEditing.h"
#include "EqAnalyzer.h"
#if FGA_SELF_TESTS
#include "ChannelStrip.h"
#endif
#include "Localisation.h"
#include "StudioTheme.h"
#include <cmath>
#include <vector>
#include <array>
#include <algorithm>
#include <juce_dsp/juce_dsp.h>

namespace fga {
namespace {

static const juce::Colour bandColours[] = {
    juce::Colour(0xff7aa2f7), // slate blue
    juce::Colour(0xff7dcfff), // cool cyan
    juce::Colour(0xff9ece6a), // muted green
    juce::Colour(0xffe0af68), // sand
    juce::Colour(0xffbb9af7), // soft violet
    juce::Colour(0xff73daca), // teal
    juce::Colour(0xfff7768e), // dusty rose
    juce::Colour(0xffa9b1d6)  // steel
};

static inline juce::Colour getBandColour(size_t index) {
    return bandColours[index % (sizeof(bandColours) / sizeof(bandColours[0]))];
}

static const char* filterTypeNames[] = {
    "Bell",
    "LOW CUT",
    "LOW SHELF",
    "HIGH SHELF",
    "HIGH CUT",
    "NOTCH"
};

// The filter glyphs are miniature DSP responses, not decorative approximations.
static void drawEqShape(juce::Graphics& g,juce::Rectangle<float> area,int type,juce::Colour colour){
    const EqBand band{true,type,1000,6,type==(int)EqFilterType::Notch?2.5:.707};
    const auto filter=eqCoefficients(band,48000);
    const float zero=area.getY()+area.getHeight()*.32f;
    g.setColour(colour.withAlpha(.17f));g.drawHorizontalLine((int)zero,area.getX(),area.getRight());
    juce::Path curve,fill;
    for(int i=0;i<72;++i){const float x=area.getX()+area.getWidth()*i/71.f;const double hz=80*std::pow(150.,i/71.);
        const float y=zero-(float)std::clamp(filter.magnitudeDb(hz,48000),-18.,8.)*area.getHeight()/26.f;
        if(i==0){curve.startNewSubPath(x,y);fill.startNewSubPath(x,zero);}else curve.lineTo(x,y);fill.lineTo(x,y);
    }
    fill.lineTo(area.getRight(),zero);fill.closeSubPath();g.setColour(colour.withAlpha(.07f));g.fillPath(fill);
    g.setColour(colour);g.strokePath(curve,juce::PathStrokeType(1.7f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));
}
class EqShapeButton final:public juce::Button{
public:
    EqShapeButton():Button("Filter shape"){setClickingTogglesState(false);}
    int type=0;juce::Colour accent=Daw::accent;
    void paintButton(juce::Graphics& g,bool hover,bool down)override{
        const bool active=getToggleState();auto r=getLocalBounds().toFloat().reduced(.5f);
        const auto ink=!isEnabled()?Daw::textFaint:active?accent: hover?Daw::text:Daw::textDim;
        g.setColour(active?accent.withAlpha(.09f):juce::Colour(hover?0xff272c34:0xff20242b));g.fillRoundedRectangle(r,4);
        g.setColour(active?accent.withAlpha(.72f):juce::Colour(down?0xff626b79:0xff333a44));g.drawRoundedRectangle(r,4,active?1.2f:.8f);
        auto icon=r.reduced(11,6);icon.removeFromBottom(15);drawEqShape(g,icon,type,ink);
        g.setFont(Daw::font(9.2f,true));g.setColour(ink);g.drawFittedText(getButtonText(),getLocalBounds().removeFromBottom(18).reduced(3,0),juce::Justification::centred,1);
    }
};
class EqLookAndFeel final:public juce::LookAndFeel_V4{
public:
    EqLookAndFeel(){
        setColour(juce::ComboBox::textColourId,Daw::text);setColour(juce::ComboBox::backgroundColourId,juce::Colour(0xff1d2229));
        setColour(juce::TextButton::textColourOffId,Daw::textDim);setColour(juce::TextButton::textColourOnId,Daw::text);
        setColour(juce::Slider::textBoxTextColourId,Daw::text);setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xff12161b));
        setColour(juce::Slider::textBoxOutlineColourId,juce::Colour(0xff343b45));
        setColour(juce::TextEditor::backgroundColourId,juce::Colour(0xff12161b));setColour(juce::TextEditor::textColourId,Daw::text);
        setColour(juce::PopupMenu::backgroundColourId,juce::Colour(0xff1d2229));setColour(juce::PopupMenu::textColourId,Daw::text);
    }
    juce::Font getTextButtonFont(juce::TextButton&,int)override{return Daw::font(10.5f,true);}
    juce::Font getComboBoxFont(juce::ComboBox&)override{return Daw::font(11.5f);}
    juce::Font getLabelFont(juce::Label&)override{return Daw::font(13.f,true);}
    void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour&,bool hover,bool down)override{
        auto r=b.getLocalBounds().toFloat().reduced(.5f);const bool on=b.getToggleState();
        g.setColour(on?Daw::accent.withAlpha(.11f):juce::Colour(down?0xff303640:hover?0xff292f38:0xff20252d));g.fillRoundedRectangle(r,4);
        g.setColour(on?Daw::accent.withAlpha(.6f):juce::Colour(hover?0xff515b69:0xff363e49));g.drawRoundedRectangle(r,4,.8f);
    }
    void drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,bool hover,bool down)override{
        drawButtonBackground(g,b,{},hover,down);auto r=b.getLocalBounds().toFloat();const auto ink=b.getToggleState()?Daw::accent:Daw::textDim;
        const float cx=17,cy=r.getCentreY();juce::Path power;power.addCentredArc(cx,cy,5,5,0,.6f,juce::MathConstants<float>::twoPi-.6f,true);
        g.setColour(ink);g.strokePath(power,juce::PathStrokeType(1.4f));g.drawLine(cx,cy-7,cx,cy-1,1.4f);
        g.setFont(Daw::font(10.5f,true));g.drawFittedText(b.getButtonText(),b.getLocalBounds().withTrimmedLeft(30).reduced(3,0),juce::Justification::centredLeft,1);
    }
    void drawComboBox(juce::Graphics& g,int w,int h,bool down,int,int,int,int,juce::ComboBox&)override{
        auto r=juce::Rectangle<float>(.5f,.5f,(float)w-1,(float)h-1);g.setColour(juce::Colour(down?0xff292f38:0xff1b2027));g.fillRoundedRectangle(r,4);
        g.setColour(juce::Colour(0xff343c47));g.drawRoundedRectangle(r,4,.8f);
        const float x=w-14.f,y=h*.5f;juce::Path arrow;arrow.startNewSubPath(x-3,y-2);arrow.lineTo(x,y+1);arrow.lineTo(x+3,y-2);g.setColour(Daw::textDim);g.strokePath(arrow,juce::PathStrokeType(1.2f));
    }
    void positionComboBoxText(juce::ComboBox& b,juce::Label& l)override{l.setBounds(10,0,b.getWidth()-32,b.getHeight());l.setFont(getComboBoxFont(b));}
    juce::Slider::SliderLayout getSliderLayout(juce::Slider& slider)override{
        juce::Slider::SliderLayout layout;auto r=slider.getLocalBounds();auto value=r.removeFromBottom(27);layout.textBoxBounds=value.withSizeKeepingCentre(std::min(104,value.getWidth()),25);r.removeFromBottom(7);layout.sliderBounds=r;return layout;
    }
    void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float start,float end,juce::Slider& slider)override{
        juce::Graphics::ScopedSaveState clip(g);g.reduceClipRegion(x,y,std::max(0,w),std::max(0,h));
        const float radius=std::max(4.f,std::min(w,h)*.5f-13),cx=x+w*.5f,cy=y+h*.5f;
        const bool enabled=slider.isEnabled();const auto accent=slider.findColour(juce::Slider::rotarySliderFillColourId).withMultipliedAlpha(enabled?1.f:.2f);
        for(int i=0;i<=10;++i){const float a=start+(end-start)*i/10.f;const float inner=radius+7,outer=radius+(i%5==0?11:9);
            g.setColour(juce::Colour(enabled?0xff5a626e:0xff343a43));g.drawLine(cx+std::sin(a)*inner,cy-std::cos(a)*inner,cx+std::sin(a)*outer,cy-std::cos(a)*outer,i%5==0?1.2f:.8f);}
        juce::Path track;track.addCentredArc(cx,cy,radius+3,radius+3,0,start,end,true);g.setColour(juce::Colour(0xff343b45));g.strokePath(track,juce::PathStrokeType(2));
        const float angle=start+(end-start)*pos,origin=(bool)slider.getProperties()["eqBipolar"]?(start+end)*.5f:start;
        if(std::abs(angle-origin)>.001f){juce::Path arc;arc.addCentredArc(cx,cy,radius+3,radius+3,0,std::min(origin,angle),std::max(origin,angle),true);g.setColour(accent);g.strokePath(arc,juce::PathStrokeType(2.6f,juce::PathStrokeType::curved,juce::PathStrokeType::rounded));}
        g.setColour(juce::Colours::black.withAlpha(.22f));g.fillEllipse(cx-radius,cy-radius+2,radius*2,radius*2);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff373e48),cx,cy-radius,juce::Colour(0xff1e242d),cx,cy+radius,false));g.fillEllipse(cx-radius,cy-radius,radius*2,radius*2);
        g.setColour(juce::Colour(enabled?0xff515a67:0xff343c46));g.drawEllipse(cx-radius,cy-radius,radius*2,radius*2,.8f);
        g.setColour(juce::Colour(0xff161b22));g.drawEllipse(cx-radius+3,cy-radius+3,radius*2-6,radius*2-6,.7f);
        g.setColour(enabled?juce::Colour(0xffedf0f4):Daw::textFaint);g.drawLine(cx+std::sin(angle)*radius*.45f,cy-std::cos(angle)*radius*.45f,cx+std::sin(angle)*radius*.82f,cy-std::cos(angle)*radius*.82f,2.2f);
    }
};

class KnobName final : public juce::Component {
public:
    void paint(juce::Graphics&) override {}
};
class EqDisplayComponent : public juce::Component, private juce::Timer {
public:
    std::function<void()> onSettingsChanged;
    std::function<void()> onDragStart;
    std::function<void()> onDragEnd;
    std::function<bool(std::vector<float>&, std::vector<float>&)> getSpectrum;
    std::function<double()> getSampleRate;
    std::function<void()> onSelectionChanged;
    std::function<void()> onWheelStart;

    EqDisplayComponent() {
        setOpaque(true);
        setWantsKeyboardFocus(true);
        addChildComponent(editor);
        editor.setVisible(false);
        editor.setFont(Daw::font(Daw::body, true));
        editor.setJustification(juce::Justification::centred);
        editor.setColour(juce::TextEditor::backgroundColourId, Daw::well);
        editor.setColour(juce::TextEditor::textColourId, Daw::text);
        editor.setColour(juce::TextEditor::outlineColourId, Daw::lineStrong);
        editor.setColour(juce::TextEditor::focusedOutlineColourId, Daw::accent);
        editor.setColour(juce::TextEditor::highlightColourId, Daw::accent.withAlpha(0.3f));
        editor.onReturnKey = [this] { commitEdit(); };
        editor.onFocusLost = [this] { commitEdit(); };
        editor.onEscapeKey = [this] { cancelEdit(); };
        auto logarithmic=[](double low,double high){return juce::NormalisableRange<double>(low,high,[](double a,double b,double t){return a*std::pow(b/a,t);},[](double a,double b,double value){return std::log(value/a)/std::log(b/a);});};
        knobs[0].setNormalisableRange(logarithmic(20,20000));knobs[1].setRange(-24,24,.1);knobs[2].setNormalisableRange(logarithmic(.1,18));
        knobs[1].getProperties().set("eqBipolar",true);
        for(int i=0;i<3;++i){auto& dial=knobs[(size_t)i];addAndMakeVisible(dial);dial.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);dial.setRotaryParameters(juce::MathConstants<float>::pi*1.25f,juce::MathConstants<float>::pi*2.75f,true);dial.setTextBoxStyle(juce::Slider::TextBoxBelow,false,104,25);dial.setMouseDragSensitivity(280);dial.setDoubleClickReturnValue(true,i==0?1000:i==1?0:.707);
            dial.textFromValueFunction=[this,i](double value){if(i==0)return value>=1000?juce::String(value/1000,2)+" kHz":juce::String(value,value<100?1:0)+" Hz";if(i==1)return (value>0?"+":"")+juce::String(value,1)+" dB";return juce::String(value,2);};
            dial.valueFromTextFunction=[this,i](const juce::String& text){double value=0;return parseEqValue(text.toStdString(),i,value)?value:knobs[(size_t)i].getValue();};
            dial.onDragStart=[this]{knobDragging=true;knobCaptured=false;};
            dial.onValueChange=[this,i]{changeKnob(i);};
            dial.onDragEnd=[this]{knobDragging=false;if(knobCaptured&&onDragEnd)onDragEnd();knobCaptured=false;};
            auto& title=knobTitle[(size_t)i];addAndMakeVisible(title);title.setInterceptsMouseClicks(false,false);
        }
        for(int i=0;i<6;++i){auto& button=shapes[(size_t)i];button.type=i;addAndMakeVisible(button);button.onClick=[this,i]{if(validBand())editOnce([&]{currentEq.bands[(size_t)selectedBand].type=i;});updateControls();};}
        for(auto* button:{&bandPower,&invertButton,&removeButton})addAndMakeVisible(button);
        bandPower.onClick=[this]{if(validBand())editOnce([&]{auto& b=currentEq.bands[(size_t)selectedBand];b.enabled=!b.enabled;});updateControls();};
        invertButton.onClick=[this]{if(validBand()&&eqHasGain(currentEq.bands[(size_t)selectedBand].type))editOnce([&]{auto& b=currentEq.bands[(size_t)selectedBand];b.gain=-b.gain;});updateControls();};
        removeButton.onClick=[this]{deleteSelectedBand();updateControls();};
        updateControls();
        startTimerHz(30);
    }

    ~EqDisplayComponent() override { stopTimer(); }

    void setEq(const EqSettings& settings, int trackIndex, const juce::String& trackName,std::uint64_t identity=0) {
        auto canonical=settings;canonical.ensureBands();
        const bool moved=track!=trackIndex||trackIdentity!=identity;
        if(moved||!(currentEq==canonical))++revision;
        if(moved){cancelEdit();for(auto& dial:knobs)dial.hideTextBox(true);knobDragging=knobCaptured=false;selectedBand=-1;preBands.fill(0);postBands.fill(0);}
        currentEq = std::move(canonical);
        currentEq.ensureBands();
        track = trackIndex;trackIdentity=identity;
        name = trackName;
        if(moved&&!currentEq.bands.empty()){selectedBand=0;for(size_t i=0;i<currentEq.bands.size();++i)if(currentEq.bands[i].type==(int)EqFilterType::Bell){selectedBand=(int)i;break;}}
        if (selectedBand >= (int)currentEq.bands.size()) selectedBand = -1;
        updateControls();repaint();
    }

    const EqSettings& getEq() const { return currentEq; }

    int getSelectedBand() const { return selectedBand; }
    void selectBand(int index){cancelEdit();selectedBand=index>=0&&index<(int)currentEq.bands.size()?index:-1;updateControls();if(onSelectionChanged)onSelectionChanged();repaint();}
    void setView(int mode,int speed,double range){analyzerMode=mode;spectrumSpeed=speed;displayRange=range;repaint();}
    void editOnce(const std::function<void()>& action){const auto before=currentEq;action();currentEq.syncLegacyFromBands();if(currentEq==before){repaint();return;}if(onDragStart)onDragStart();if(onSettingsChanged)onSettingsChanged();if(onDragEnd)onDragEnd();updateControls();repaint();}
    void flatten(){editOnce([this]{currentEq.flatten();});}


#if FGA_SELF_TESTS
    void simulateKnobGesture(int parameter,double first,double last){auto& dial=knobs[(size_t)parameter];dial.onDragStart();dial.setValue(first,juce::sendNotificationSync);dial.setValue(last,juce::sendNotificationSync);dial.onDragEnd();}
    bool verifyInteractions(juce::StringArray& checks){
        bool ok=true;auto expect=[&](bool passed,const char* label){checks.add(juce::String(passed?"PASS ":"FAIL ")+label);ok=passed&&ok;};
        int undos=0;onDragStart=[&]{++undos;};onSettingsChanged=[]{};onDragEnd=[]{};
        EqSettings authored;authored.enabled=true;authored.bands={{true,0,150,3,1},{true,0,1500,-4,2},{true,1,35,0,.707}};authored.syncLegacyFromBands();setEq(authored,0,"Test");
        auto event=[&](juce::Point<float> pos,int clicks=1){return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(),pos,juce::ModifierKeys::leftButtonModifier,1,0,0,0,0,this,this,juce::Time::getCurrentTime(),pos,juce::Time::getCurrentTime(),clicks,false);};
        const int selectionUndo=undos;mouseDown(event(getNodePosition(0,getPlotArea())));mouseUp(event(getNodePosition(0,getPlotArea())));expect(undos==selectionUndo,"selecting a band does not add an empty Undo step");
        selectBand(1);const int before=undos;mouseDoubleClick(event(getNodePosition(0,getPlotArea()),2));
        expect(currentEq.bands.size()==3&&currentEq.bands[0].gain==0&&currentEq.bands[1].gain==-4&&undos==before+1,"double-click resets the clicked band, never deletes another one");
        selectBand(1);beginEdit(0);editor.setText("nonsense");const auto saved=currentEq;const int edits=undos;commitEdit();expect(currentEq==saved&&undos==edits&&editor.isVisible(),"invalid text keeps the band and Undo unchanged");
        editor.setText("2,5 kHz");commitEdit();expect(currentEq.bands[1].freq==2500&&undos==edits+1,"unit/decimal frequency entry updates exactly one band and Undo step");
        beginEdit(1);editor.setText("-6.5 dB");commitEdit();expect(currentEq.bands[1].gain==-6.5&&undos==edits+2,"valid entry is one undoable edit");
        selectBand(2);beginEdit(1);expect(editParam<0,"cut/notch gain editor is disabled");
        beginEdit(0);editor.setText("200");setEq(authored,1,"Another track");commitEdit();expect(currentEq==authored&&!editor.isVisible(),"track changes discard stale editor input");
        selectBand(0);const auto wheelEvent=event(getNodePosition(0,getPlotArea()));juce::MouseWheelDetails wheel;wheel.deltaY=.1f;const int scroll=undos;mouseWheelMove(wheelEvent,wheel);expect(undos==scroll+1,"Q wheel edit participates in Undo");
        for(auto size:std::initializer_list<juce::Point<int>>{{732,354},{932,514}}){setSize(size.x,size.y);const auto hud=getHudBounds(getPlotArea());const auto l=hudLayout(hud);expect(hud.contains(l.del)&&hud.contains(l.freq)&&hud.contains(l.q)&&getPlotArea().getBottom()<hud.getY(),"parameter strip fits the minimum and normal sizes without covering the graph");}
        setEq(authored,0,"Knobs");selectBand(1);const int gestureStart=undos;
        knobs[0].onDragStart();knobs[0].setValue(2100,juce::sendNotificationSync);knobs[0].setValue(2500,juce::sendNotificationSync);knobs[0].onDragEnd();
        expect(currentEq.bands[1].freq==2500&&undos==gestureStart+1,"rotary frequency drag updates the band with one Undo gesture");
        knobs[1].setValue(-8,juce::sendNotificationSync);expect(currentEq.bands[1].gain==-8,"rotary gain controls the selected band");
        knobs[2].setValue(3.2,juce::sendNotificationSync);expect(std::abs(currentEq.bands[1].q-3.2)<.001,"rotary Q controls the selected band");
        expect(knobs[0].getValueFromText("1,25 kHz")==1250,"rotary readout accepts manual frequency units");
        selectBand(2);expect(!knobs[1].isEnabled()&&knobs[0].isEnabled()&&knobs[2].isEnabled(),"gain knob is disabled for a cut while frequency and Q remain active");
        selectBand(0);shapes[5].onClick();expect(currentEq.bands[0].type==5&&!knobs[1].isEnabled(),"filter cards update the real shape and knob availability");
        onDragStart=nullptr;onSettingsChanged=nullptr;onDragEnd=nullptr;return ok;
    }
#endif

    void addBandAt(double hz, double db, int type = (int)EqFilterType::Bell) {
        if (currentEq.bands.size() >= 16) return;
        if (onDragStart) onDragStart();
        EqBand b;
        b.enabled = true;
        b.type = type;
        b.freq = std::clamp(hz, 20.0, 20000.0);
        b.gain = std::clamp(db, -24.0, 24.0);
        b.q = (type == (int)EqFilterType::Notch) ? 4.0 : 0.707;
        b.normalise();
        currentEq.bands.push_back(b);
        currentEq.syncLegacyFromBands();
        selectedBand = (int)currentEq.bands.size() - 1;
        dragBand = -1;
        if (onSettingsChanged) onSettingsChanged();
        if (onDragEnd) onDragEnd();
        updateControls();if(onSelectionChanged)onSelectionChanged();
        repaint();
    }

    void deleteSelectedBand() {
        if (selectedBand >= 0 && selectedBand < (int)currentEq.bands.size()) {
            if (onDragStart) onDragStart();
            currentEq.bands.erase(currentEq.bands.begin() + selectedBand);
            currentEq.syncLegacyFromBands();
            selectedBand = -1;
            dragBand = -1;
            if (onSettingsChanged) onSettingsChanged();
            if (onDragEnd) onDragEnd();
            updateControls();if(onSelectionChanged)onSelectionChanged();
            repaint();
        }
    }

    bool keyPressed(const juce::KeyPress& key) override {
        if(!key.getModifiers().isCommandDown()&&!key.getModifiers().isCtrlDown()){
            const auto letter=juce::CharacterFunctions::toLowerCase(key.getTextCharacter());
            if(letter=='f'||letter=='g'||letter=='q'){beginEdit(letter=='f'?0:letter=='g'?1:2);return true;}
        }
        if (key.isKeyCode(juce::KeyPress::deleteKey) || key.isKeyCode(juce::KeyPress::backspaceKey)) {
            deleteSelectedBand();
            return true;
        }
        if (key.isKeyCode(juce::KeyPress::escapeKey)) {
            selectBand(-1);
            return true;
        }
        return false;
    }

    void timerCallback() override {
        if(!isShowing())return;
        if(getSampleRate){const double rate=getSampleRate();if(std::isfinite(rate)&&rate>=8000)sampleRate=rate;}
        std::vector<float> pre,post;
        const bool heard=analyzerMode!=0&&getSpectrum&&getSpectrum(pre,post);
        if(heard){auto peak=[](const std::vector<float>& v){float p=0;for(float s:v)p=std::max(p,std::abs(s));return p;};inPeak=peak(pre);outPeak=peak(post);}
        inShown=inPeak>inShown?inPeak:inShown*.9f;outShown=outPeak>outShown?outPeak:outShown*.9f;
        analyzer.analyse(pre,sampleRate,preBands,spectrumSpeed);
        analyzer.analyse(post,sampleRate,postBands,spectrumSpeed);
        repaint();
    }

    void beginEdit(int param) {
        if (selectedBand < 0 || selectedBand >= (int)currentEq.bands.size()) return;
        const auto& b = currentEq.bands[(size_t)selectedBand];
        if(param==1&&!eqHasGain(b.type))return;
        editor.setColour(juce::TextEditor::outlineColourId,Daw::lineStrong);
        editor.setColour(juce::TextEditor::focusedOutlineColourId,Daw::accent);
        editParam = param;
        const juce::String text = param == 0 ? juce::String(b.freq, b.freq < 100.0 ? 1 : 0)
                                : param == 1 ? juce::String(b.gain, 1)
                                             : juce::String(b.q, 2);
        editor.setInputRestrictions(24);
        editor.setText(text, false);
        const auto l = hudLayout(getHudBounds(getPlotArea()));
        editor.setBounds((param == 0 ? l.freq : param == 1 ? l.gain : l.q).toNearestInt().reduced(3));
        editor.setVisible(true);
        editor.grabKeyboardFocus();
        editor.selectAll();
    }

    void cancelEdit() {
        editParam = -1;
        editor.setVisible(false);
        repaint();
    }

    void commitEdit() {
        if(editParam<0)return;
        if(selectedBand<0||selectedBand>=(int)currentEq.bands.size()){cancelEdit();return;}
        double value=0;
        if(!parseEqValue(editor.getText().toStdString(),editParam,value)){
            editor.setColour(juce::TextEditor::outlineColourId,Daw::meterHigh);
            editor.setColour(juce::TextEditor::focusedOutlineColourId,Daw::meterHigh);
            editor.setTooltip(tr("Enter a valid frequency, gain or Q within its range."));return;
        }
        const int parameter=editParam,index=selectedBand;
        cancelEdit();
        const auto& old=currentEq.bands[(size_t)index];
        if(value==(parameter==0?old.freq:parameter==1?old.gain:old.q))return;
        editOnce([&]{auto& b=currentEq.bands[(size_t)index];if(parameter==0)b.freq=value;else if(parameter==1)b.gain=value;else b.q=value;b.normalise();});
    }

    void resized() override {
        if(editor.isVisible())cancelEdit();
        const auto rack=getHudBounds(getPlotArea()).reduced(12,8);auto area=rack;
        const int order[]{0,2,3,1,4,5};
        if(getWidth()>=800){
            auto actions=area.removeFromRight(84);area.removeFromRight(8);
            auto knobsArea=area.removeFromRight(std::min(460.f,area.getWidth()*.48f));area.removeFromRight(8);
            const float cell=area.getWidth()/6.f;
            for(int cellIndex=0;cellIndex<6;++cellIndex)shapes[(size_t)order[cellIndex]].setBounds(juce::Rectangle<float>(area.getX()+cellIndex*cell,area.getY()+22,cell-4,area.getHeight()-22).toNearestInt());
            const float columnW=knobsArea.getWidth()/3.f;
            for(int i=0;i<3;++i){auto cell=juce::Rectangle<float>(knobsArea.getX()+i*columnW+2,knobsArea.getY(),columnW-4,knobsArea.getHeight()).toNearestInt();auto cap=cell.removeFromTop(28);knobTitle[(size_t)i].setBounds(cap.reduced(1,1));knobs[(size_t)i].setBounds(cell.reduced(2,0));}
            const int button=28,gap=6,stack=button*3+gap*2;const int y0=(int)(actions.getY()+(actions.getHeight()-stack)*.5f);
            bandPower.setBounds((int)actions.getX(),y0,(int)actions.getWidth(),button);
            invertButton.setBounds((int)actions.getX(),y0+button+gap,(int)actions.getWidth(),button);
            removeButton.setBounds((int)actions.getX(),y0+(button+gap)*2,(int)actions.getWidth(),button);
            return;
        }
        const float leftWidth=std::clamp(area.getWidth()*.27f,216.f,250.f);
        auto types=area.removeFromLeft(leftWidth);area.removeFromLeft(14);
        auto actions=area.removeFromRight(82);area.removeFromRight(14);
        auto grid=types.withTrimmedTop(38);
        const float cellWidth=grid.getWidth()/3;
        for(int cell=0;cell<6;++cell)shapes[(size_t)order[cell]].setBounds(juce::Rectangle<float>(grid.getX()+cell%3*cellWidth,grid.getY()+cell/3*61,cellWidth-5,54).toNearestInt());
        const float columnW=area.getWidth()/3;
        for(int i=0;i<3;++i){auto cell=juce::Rectangle<float>(area.getX()+i*columnW+4,area.getY()+28,columnW-8,std::max(48.f,area.getHeight()-32)).toNearestInt();auto cap=cell.removeFromTop(28);knobTitle[(size_t)i].setBounds(cap.reduced(1,1));knobs[(size_t)i].setBounds(cell.reduced(2,0));}
        bandPower.setBounds(actions.withY(actions.getY()+49).withHeight(29).toNearestInt());
        invertButton.setBounds(actions.withY(actions.getY()+89).withHeight(29).toNearestInt());
        removeButton.setBounds(actions.withY(actions.getY()+129).withHeight(29).toNearestInt());
    }

    void mouseExit(const juce::MouseEvent&) override {
        if (hoverBand != -1 || hoverPill != -1) {
            hoverBand = hoverPill = -1;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(Daw::base);
        const auto plotArea = getPlotArea();

        g.setColour(Daw::lcd);
        g.fillRoundedRectangle(plotArea, 4.0f);
        g.setColour(Daw::line);
        g.drawRoundedRectangle(plotArea, 4.0f, 1.0f);

        {
            const juce::Graphics::ScopedSaveState saved(g);
            g.reduceClipRegion(plotArea.toNearestInt());

            const double dbTicks[] = {displayRange,displayRange*.75,displayRange*.5,displayRange*.25,0,-displayRange*.25,-displayRange*.5,-displayRange*.75,-displayRange};
            for (double db : dbTicks) {
                const float y = yFromDb(db, plotArea);
                const bool zero = std::abs(db) < 1e-4;
                g.setColour(zero ? Daw::lineStrong : Daw::line.withMultipliedAlpha(0.7f));
                g.drawHorizontalLine(juce::roundToInt(y), plotArea.getX(), plotArea.getRight());
            }

            static constexpr double majorFreqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
            static constexpr double minorFreqs[] = { 30, 40, 60, 70, 80, 90, 300, 400, 600, 700, 800, 900, 3000, 4000, 6000, 7000, 8000, 9000, 15000 };
            g.setColour(Daw::line.withMultipliedAlpha(0.45f));
            for (double hz : minorFreqs)
                g.drawVerticalLine(juce::roundToInt(xFromFreq(hz, plotArea)), plotArea.getY(), plotArea.getBottom());
            g.setColour(Daw::line.withMultipliedAlpha(0.9f));
            for (double hz : majorFreqs)
                g.drawVerticalLine(juce::roundToInt(xFromFreq(hz, plotArea)), plotArea.getY(), plotArea.getBottom());
        }

        g.setFont(Daw::smallFont(9.5f));
        const double dbLabels[] = {displayRange,displayRange*.5,0,-displayRange*.5,-displayRange};
        for (double db : dbLabels) {
            const float y = yFromDb(db, plotArea);
            g.setColour(std::abs(db) < 1e-4 ? Daw::textDim : Daw::textFaint);
            const juce::String t = (db > 0 ? "+" : "") + juce::String((int)db);
            g.drawText(t, 0, juce::roundToInt(y - 7.0f), juce::roundToInt(plotArea.getX()) - 8, 14, juce::Justification::centredRight);
        }

        static constexpr struct { double hz; const char* label; } freqLabels[] = {
            { 20, "20" }, { 50, "50" }, { 100, "100" }, { 200, "200" }, { 500, "500" },
            { 1000, "1k" }, { 2000, "2k" }, { 5000, "5k" }, { 10000, "10k" }, { 20000, "20k" }
        };
        for (const auto& f : freqLabels) {
            if(f.hz>maxFrequency())continue;
            const float x = xFromFreq(f.hz, plotArea);
            g.setColour(Daw::textFaint);
            g.drawText(f.label, juce::roundToInt(x - 18.0f), juce::roundToInt(plotArea.getBottom() + 6.0f), 36, 14, juce::Justification::centred);
        }

        if (name.isNotEmpty()) {
            g.setColour(Daw::textFaint);
            g.setFont(Daw::caps(9.5f));
            g.drawText(name.toUpperCase(), juce::roundToInt(plotArea.getX() + 8.0f), juce::roundToInt(plotArea.getY() + 6.0f),
                       juce::roundToInt(plotArea.getWidth() - 16.0f), 14, juce::Justification::centredLeft);
        }

        {
            const juce::Graphics::ScopedSaveState saved(g);
            g.reduceClipRegion(plotArea.toNearestInt());
            auto drawSpectrum = [&](const EqAnalyzer::Trace& values, juce::Colour fill, juce::Colour line) {
                if(*std::max_element(values.begin(),values.end())<.001f)return;
                juce::Path path;
                path.startNewSubPath(plotArea.getX(), plotArea.getBottom());
                for (int i = 0; i < EqAnalyzer::points; ++i) {
                    const float x = plotArea.getX() + plotArea.getWidth() * (float)i / (EqAnalyzer::points-1);
                    const float y = plotArea.getBottom() - values[(size_t)i] * plotArea.getHeight();
                    path.lineTo(x, y);
                }
                path.lineTo(plotArea.getRight(), plotArea.getBottom());
                path.closeSubPath();
                g.setColour(fill);
                g.fillPath(path);
                g.setColour(line);
                g.strokePath(path, juce::PathStrokeType(1.0f));
            };
            if(analyzerMode==1||analyzerMode==2)drawSpectrum(preBands, Daw::textFaint.withAlpha(0.08f), Daw::textFaint.withAlpha(0.22f));
            if (analyzerMode==1||analyzerMode==3)
                drawSpectrum(postBands, juce::Colour(0xff4cc3f0).withAlpha(0.10f), juce::Colour(0xff4cc3f0).withAlpha(0.45f));
        }

        std::vector<EqCoefficients> bandCoeffs;
        bandCoeffs.reserve(currentEq.bands.size());
        for (const auto& b : currentEq.bands) bandCoeffs.push_back(eqCoefficients(b, sampleRate));

        const int steps = std::max(2, (int)plotArea.getWidth());
        const float yZero = yFromDb(0.0, plotArea);

        {
            const juce::Graphics::ScopedSaveState saved(g);
            g.reduceClipRegion(plotArea.toNearestInt());

            if (currentEq.enabled && (selectedBand >= 0 || hoverBand >= 0)) {
                const int which = selectedBand >= 0 ? selectedBand : hoverBand;
                if (which >= 0 && which < (int)currentEq.bands.size() && currentEq.bands[(size_t)which].enabled) {
                    juce::Path bandPath;
                    for (int i = 0; i < steps; ++i) {
                        const float x = plotArea.getX() + (float)i * (plotArea.getWidth() / (float)(steps - 1));
                        const double hz = freqFromX(x, plotArea);
                        const float y = yFromDb(std::clamp(bandCoeffs[(size_t)which].magnitudeDb(hz, sampleRate), -26.0, 26.0), plotArea);
                        if (i == 0) bandPath.startNewSubPath(x, y);
                        else bandPath.lineTo(x, y);
                    }
                    juce::Path bandFill=bandPath;bandFill.lineTo(plotArea.getRight(),yZero);bandFill.lineTo(plotArea.getX(),yZero);bandFill.closeSubPath();
                    g.setColour(getBandColour((size_t)which).withAlpha(0.14f));g.fillPath(bandFill);
                    g.setColour(getBandColour((size_t)which).withAlpha(0.9f));
                    g.strokePath(bandPath, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved));
                }
            }

            juce::Path curvePath, fillPath;
            for (int i = 0; i < steps; ++i) {
                const float x = plotArea.getX() + (float)i * (plotArea.getWidth() / (float)(steps - 1));
                const double hz = freqFromX(x, plotArea);
                double totalDb = 0.0;
                if (currentEq.enabled)
                    for (const auto& coeff : bandCoeffs) totalDb += coeff.magnitudeDb(hz, sampleRate);
                const float y = yFromDb(std::clamp(totalDb, -26.0, 26.0), plotArea);
                if (i == 0) {
                    curvePath.startNewSubPath(x, y);
                    fillPath.startNewSubPath(x, yZero);
                } else {
                    curvePath.lineTo(x, y);
                }
                fillPath.lineTo(x, y);
            }
            fillPath.lineTo(plotArea.getRight(), yZero);
            fillPath.closeSubPath();

            const auto curveColour=currentEq.enabled?juce::Colour(0xffe7dfc8):Daw::textFaint;
            g.setColour(curveColour.withMultipliedAlpha(.045f));
            g.fillPath(fillPath);
            g.setColour(curveColour.withAlpha(currentEq.enabled ? 0.95f : 0.6f));
            g.strokePath(curvePath, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        for (size_t i = 0; i < currentEq.bands.size(); ++i) {
            const auto& b = currentEq.bands[i];
            const auto pt = getNodePosition((int)i, plotArea);
            const bool isHovered = hoverBand == (int)i;
            const bool isSelected = selectedBand == (int)i;
            const bool isDragging = dragBand == (int)i;
            const auto col = getBandColour(i);

            if (isDragging || isHovered) {
                g.setColour(Daw::lineStrong.withAlpha(isDragging ? 0.8f : 0.5f));
                g.drawVerticalLine(juce::roundToInt(pt.x), plotArea.getY(), plotArea.getBottom());
                g.drawHorizontalLine(juce::roundToInt(pt.y), plotArea.getX(), plotArea.getRight());
            }

            g.setColour(b.enabled ? col : col.withAlpha(0.35f));
            g.fillEllipse(pt.x - 6.0f, pt.y - 6.0f, 12.0f, 12.0f);
            g.setColour(Daw::base);
            g.fillEllipse(pt.x - 2.5f, pt.y - 2.5f, 5.0f, 5.0f);
            const juce::String tagText = isSelected ? (b.freq>=1000?juce::String(b.freq/1000.0,2)+" kHz":juce::String(b.freq,b.freq<100?1:0)+" Hz") + (eqHasGain(b.type)?"   "+juce::String(b.gain,1):juce::String()) : juce::String((int)i+1);
            auto tag = juce::Rectangle<float>(pt.x + 10.0f, pt.y - 20.0f, isSelected ? 108.0f : 22.0f, 18.0f);
            if (tag.getRight() > plotArea.getRight() - 4.0f) tag.setX(pt.x - tag.getWidth() - 10.0f);
            if (tag.getY() < plotArea.getY() + 2.0f) tag.setY(pt.y + 8.0f);
            g.setColour(Daw::base.withAlpha(0.9f));
            g.fillRoundedRectangle(tag, 4.0f);
            g.setColour(col.withAlpha(0.85f));
            g.drawRoundedRectangle(tag, 4.0f, Daw::hairline());
            g.setColour(b.enabled ? Daw::text : Daw::textFaint);
            g.setFont(Daw::font(10.5f, true));
            g.drawText(tagText, tag.toNearestInt(), juce::Justification::centred);
        }
        g.setFont(Daw::font(11.f, true));
        g.setFont(Daw::smallFont(9)); g.setColour(Daw::textFaint);
        g.drawText("dB", 2, (int)plotArea.getY()-16, 40, 14, juce::Justification::centredRight);
        const float scaleX=plotArea.getRight()+4;
        g.drawText("dBFS", (int)scaleX, (int)plotArea.getY()-16, 40, 14, juce::Justification::centredLeft);
        for(int db:{0,-24,-48,-72,-96}){const float y=plotArea.getY()+plotArea.getHeight()*(-db/96.f);g.drawText(juce::String(db),(int)scaleX,(int)y-7,36,14,juce::Justification::centredLeft);}
        if (analyzerMode==1||analyzerMode==2) { g.setColour(Daw::textDim); g.fillRoundedRectangle(plotArea.getRight()-150, plotArea.getY()+8, 10, 10, 2); g.drawText(tr("INPUT"), (int)plotArea.getRight()-136, (int)plotArea.getY()+5, 56, 16, juce::Justification::centredLeft); }
        if (analyzerMode==1||analyzerMode==3) { g.setColour(juce::Colour(0xff4cc3f0)); g.fillRoundedRectangle(plotArea.getRight()-78, plotArea.getY()+8, 10, 10, 2); g.drawText(tr("OUTPUT"), (int)plotArea.getRight()-64, (int)plotArea.getY()+5, 60, 16, juce::Justification::centredLeft); }
        drawRail(g, plotArea);
        drawBandHud(g,plotArea);

    }

    void paintOverChildren(juce::Graphics& g) override {
        const char* names[]={"FREQUENCY","GAIN","Q"};
        g.setFont(Daw::font(13.f,true));
        for(int i=0;i<3;++i){
            const auto r=knobTitle[(size_t)i].getBounds();
            if(r.getWidth()<8||r.getHeight()<12)continue;
            const bool enabled=knobs[(size_t)i].isEnabled();
            g.setColour(juce::Colour(0xff243044));
            g.fillRect(r);
            g.setColour(enabled?juce::Colour(0xfff7f8fa):juce::Colour(0xff8b93a0));
            g.drawText(tr(names[i]),r.reduced(2,0),juce::Justification::centred,false);
        }
    }

    void mouseMove(const juce::MouseEvent& e) override {
        const auto plotArea = getPlotArea();
        const int found = findBandAt(e.position, plotArea);
        int pill = -1;
        if (selectedBand >= 0 && selectedBand < (int)currentEq.bands.size()) {
            const auto l = hudLayout(getHudBounds(plotArea));
            for (int i = 0; i < 6; ++i)
                if (l.pills[i].contains(e.position)) { pill = i; break; }
        }
        if (hoverBand != found || hoverPill != pill) {
            hoverBand = found;
            hoverPill = pill;
            repaint();
        }
    }

    void mouseDown(const juce::MouseEvent& e) override {
        const auto plotArea = getPlotArea();
        dragBand = -1;
        gesture=false;

        // Check if clicked in HUD
        if (selectedBand >= 0 && selectedBand < (int)currentEq.bands.size()) {
            auto hudRect = getHudBounds(plotArea);
            if (hudRect.contains(e.position)) {
                // Native knobs and buttons own the parameter-rack input.
                return;
            }
        }

        // Check node click
        int found = findBandAt(e.position, plotArea);
        if (found >= 0) {
            if (e.mods.isPopupMenu()) {
                showBandMenu(found, e.getScreenPosition());
                return;
            }
            selectBand(found);
            nodeStartFreq=currentEq.bands[(size_t)found].freq;nodeStartGain=currentEq.bands[(size_t)found].gain;
            dragBand = found;
            repaint();
            return;
        }

        // Click on empty canvas
        if (e.mods.isPopupMenu()) {
            const double hz = std::clamp(freqFromX(e.position.x, plotArea), 20.0, 20000.0);
            const double db = std::clamp(dbFromY(e.position.y, plotArea), -24.0, 24.0);
            showCanvasMenu(hz, db, e.getScreenPosition());
            return;
        }

        // Click outside: deselect band
        if (selectedBand != -1) {
            selectBand(-1);
        }
    }

    void mouseDoubleClick(const juce::MouseEvent& e) override {
        if (e.mods.isPopupMenu()) return;
        if (selectedBand >= 0 && selectedBand < (int)currentEq.bands.size()) {
            const auto& b=currentEq.bands[(size_t)selectedBand];
            const auto l = hudLayout(getHudBounds(getPlotArea()));
            if (l.freq.contains(e.position)) { beginEdit(0); return; }
            if (l.gain.contains(e.position)&&eqHasGain(b.type)) { beginEdit(1); return; }
            if (l.q.contains(e.position)) { beginEdit(2); return; }
        }
        const auto plotArea = getPlotArea();
        if(getHudBounds(plotArea).contains(e.position))return;
        const int hit=findBandAt(e.position,plotArea);
        if(hit>=0){selectBand(hit);if(eqHasGain(currentEq.bands[(size_t)hit].type)&&currentEq.bands[(size_t)hit].gain!=0)editOnce([&]{currentEq.bands[(size_t)hit].gain=0;});return;}
        if (!plotArea.contains(e.position)) return;

        // Double-click creates a new band!
        const double hz = std::clamp(freqFromX(e.position.x, plotArea), 20.0, 20000.0);
        const double db = std::clamp(dbFromY(e.position.y, plotArea), -24.0, 24.0);
        int type = (int)EqFilterType::Bell;
        if (hz < 60.0 && db < -4.0) type = (int)EqFilterType::LowCut;
        else if (hz > 14000.0 && db < -4.0) type = (int)EqFilterType::HighCut;
        else if (hz < 150.0) type = (int)EqFilterType::LowShelf;
        else if (hz > 8000.0) type = (int)EqFilterType::HighShelf;

        addBandAt(hz, db, type);
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        const auto plotArea = getPlotArea();
        if((dragBand>=0)&&e.getDistanceFromDragStart()>1&&!gesture){if(onDragStart)onDragStart();gesture=true;}
        if(!gesture)return;

        // Dragging band node
        if (dragBand >= 0 && dragBand < (int)currentEq.bands.size()) {
            auto& b = currentEq.bands[(size_t)dragBand];
            const double precision=e.mods.isShiftDown()?.1:1.;
            b.freq=std::clamp(nodeStartFreq*std::pow(maxFrequency()/20.,e.getDistanceFromDragStartX()*precision/plotArea.getWidth()),20.,20000.);
            if (b.type != (int)EqFilterType::LowCut && b.type != (int)EqFilterType::HighCut && b.type != (int)EqFilterType::Notch) {
                b.gain=std::clamp(nodeStartGain-e.getDistanceFromDragStartY()*precision*2*displayRange/plotArea.getHeight(),-24.,24.);
            }
            b.normalise();
            currentEq.syncLegacyFromBands();
            if (onSettingsChanged) onSettingsChanged();
            repaint();
        }
    }

    void mouseUp(const juce::MouseEvent&) override {
        if (dragBand >= 0) {
            dragBand = -1;
                if(gesture&&onDragEnd)onDragEnd();
            gesture=false;
            repaint();
        }
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override {
        int targetBand = (hoverBand >= 0) ? hoverBand : selectedBand;
        if (targetBand >= 0 && targetBand < (int)currentEq.bands.size()) {
            auto& b = currentEq.bands[(size_t)targetBand];
            if(wheel.deltaY==0)return;
            if(onWheelStart)onWheelStart();else if(onDragStart)onDragStart();
            b.q=std::clamp(b.q*std::pow(2.,wheel.deltaY*2.),.1,18.);
            b.normalise();
            currentEq.syncLegacyFromBands();
            if (onSettingsChanged) onSettingsChanged();
            if(onDragEnd)onDragEnd();
            repaint();
        }
    }

private:
    EqSettings currentEq;
    int track = 0;
    juce::String name;
    int hoverBand = -1;
    int hoverPill = -1;
    int dragBand = -1;
    int selectedBand = -1;

    // HUD parameter dragging
    bool gesture=false;
    std::array<juce::Slider,3> knobs;std::array<KnobName,3> knobTitle;
    std::array<EqShapeButton,6> shapes;
    juce::TextButton bandPower,invertButton,removeButton;
    bool controlsUpdating=false,knobDragging=false,knobCaptured=false;
    bool validBand()const{return selectedBand>=0&&selectedBand<(int)currentEq.bands.size();}
    void updateControls(){
        controlsUpdating=true;const bool selected=validBand();
        const auto colour=selected?getBandColour((size_t)selectedBand):Daw::textFaint;
        for(int i=0;i<3;++i){auto& dial=knobs[(size_t)i];const bool enabled=selected&&(i!=1||eqHasGain(currentEq.bands[(size_t)selectedBand].type));dial.setEnabled(enabled);dial.setName(tr(i==0?"FREQUENCY":i==1?"GAIN":"Q"));dial.setColour(juce::Slider::rotarySliderFillColourId,colour);
            if(selected){const auto& b=currentEq.bands[(size_t)selectedBand];dial.setValue(i==0?b.freq:i==1?b.gain:b.q,juce::dontSendNotification);}}
        for(int i=0;i<6;++i){auto& shape=shapes[(size_t)i];shape.setEnabled(selected);shape.accent=colour;shape.setButtonText(tr(filterTypeNames[i]));shape.setTooltip(tr(filterTypeNames[i]));shape.setToggleState(selected&&currentEq.bands[(size_t)selectedBand].type==i,juce::dontSendNotification);shape.repaint();}
        bandPower.setButtonText(tr(selected&&currentEq.bands[(size_t)selectedBand].enabled?"ON":"OFF"));bandPower.setEnabled(selected);bandPower.setToggleState(selected&&currentEq.bands[(size_t)selectedBand].enabled,juce::dontSendNotification);
        invertButton.setButtonText(tr("INVERT"));invertButton.setTooltip(tr("Invert Gain"));invertButton.setEnabled(selected&&eqHasGain(currentEq.bands[(size_t)selectedBand].type));
        removeButton.setButtonText(tr("DELETE"));removeButton.setTooltip(tr("Delete Band"));removeButton.setColour(juce::TextButton::textColourOffId,juce::Colour(0xffc69a9a));removeButton.setEnabled(selected);controlsUpdating=false;
    }
    void changeKnob(int parameter){
        if(controlsUpdating||!validBand())return;
        const double value=knobs[(size_t)parameter].getValue();
        auto apply=[&]{auto& b=currentEq.bands[(size_t)selectedBand];if(parameter==0)b.freq=value;else if(parameter==1)b.gain=value;else b.q=value;b.normalise();};
        if(knobDragging){if(!knobCaptured){if(onDragStart)onDragStart();knobCaptured=true;}apply();currentEq.syncLegacyFromBands();if(onSettingsChanged)onSettingsChanged();repaint();}
        else editOnce(apply);
    }
    EqAnalyzer analyzer;
    EqAnalyzer::Trace preBands{},postBands{};
    float inPeak=0,outPeak=0,inShown=0,outShown=0;
    double sampleRate=44100,displayRange=12,nodeStartFreq=1000,nodeStartGain=0;
    int analyzerMode=1,spectrumSpeed=1;
    std::uint64_t revision=0,trackIdentity=0;
    juce::TextEditor editor;
    int editParam = -1;

    float hudHeight() const { return getWidth()>=800?196.f:220.f; }
    juce::Rectangle<float> getPlotArea() const {
        const float padL=48, padR=getWidth()>=800?228.f:52.f, padT=22, padB=hudHeight()+20;
        return juce::Rectangle<float>(padL, padT, (float)getWidth() - padL - padR, (float)getHeight() - padT - padB);
    }
    void drawRail(juce::Graphics& g,const juce::Rectangle<float>& plot) const {
        if(getWidth()<800)return;
        auto rail=juce::Rectangle<float>(plot.getRight()+46,plot.getY(),(float)getWidth()-plot.getRight()-58,plot.getHeight());
        const auto colour=validBand()?getBandColour((size_t)selectedBand):Daw::textFaint;
        Daw::devicePanel(g,rail,colour);
        auto card=rail.reduced(12,12);
        g.setColour(colour);g.setFont(Daw::caps(10.5f));
        g.drawText(validBand()?tr("BAND")+" "+juce::String(selectedBand+1).paddedLeft('0',2)+"   "+tr(filterTypeNames[currentEq.bands[(size_t)selectedBand].type]):tr("SELECT BAND"),card.removeFromTop(16).toNearestInt(),juce::Justification::centredLeft);
        card.removeFromTop(6);
        auto readout=[&](const juce::String& k,const juce::String& v){auto row=card.removeFromTop(46);g.setColour(Daw::textFaint);g.setFont(Daw::caps(10));g.drawText(k,row.removeFromTop(14).toNearestInt(),juce::Justification::centredLeft);g.setColour(Daw::text);g.setFont(Daw::font(22.f,true));g.drawText(v,row.toNearestInt(),juce::Justification::centredLeft);};
        if(validBand()){const auto& b=currentEq.bands[(size_t)selectedBand];readout(tr("FREQUENCY"),b.freq>=1000?juce::String(b.freq/1000,2)+" kHz":juce::String(b.freq,b.freq<100?1:0)+" Hz");readout(tr("GAIN"),eqHasGain(b.type)?juce::String(b.gain,1)+" dB":"--");readout("Q",juce::String(b.q,2));}
        else{readout(tr("FREQUENCY"),"--");readout(tr("GAIN"),"--");readout("Q","--");}
        auto meters=card.removeFromTop(std::min(150.f,card.getHeight()));
        auto meter=[&](juce::Rectangle<float> r,const juce::String& name,float linear){const float db=linear>1.e-8f?20.f*std::log10(linear):-48.f;g.setColour(Daw::textDim);g.setFont(Daw::caps(10));g.drawText(name,r.removeFromTop(14).toNearestInt(),juce::Justification::centred);g.setColour(Daw::text);g.setFont(Daw::font(12.f,true));g.drawText(linear>1.e-8f?juce::String(db,1):"--",r.removeFromBottom(16).toNearestInt(),juce::Justification::centred);auto bar=r.withSizeKeepingCentre(std::min(16.f,r.getWidth()),std::max(8.f,r.getHeight()-4));g.setColour(Daw::lcd);g.fillRoundedRectangle(bar,3);g.setColour(Daw::lineStrong);g.drawRoundedRectangle(bar,3,Daw::hairline());const float shown=std::clamp((db+48.f)/48.f,0.f,1.f);auto lit=bar.withTrimmedTop(bar.getHeight()*(1.f-shown));g.setGradientFill(juce::ColourGradient(Daw::meterHigh,bar.getX(),bar.getY(),Daw::meterLow,bar.getX(),bar.getBottom(),false));{juce::Graphics::ScopedSaveState clip(g);g.reduceClipRegion(lit.toNearestInt());g.fillRoundedRectangle(bar,3);}};
        meter(meters.removeFromLeft(meters.getWidth()*.5f),"IN",inShown);meter(meters,"OUT",outShown);
    }

    double maxFrequency()const{return std::min(20000.,sampleRate*.499);}

    float xFromFreq(double hz, const juce::Rectangle<float>& r) const {
        hz=std::clamp(hz,20.,maxFrequency());
        const double norm=std::log10(hz/20.)/std::log10(maxFrequency()/20.);
        return r.getX() + (float)(norm * r.getWidth());
    }

    double freqFromX(float x, const juce::Rectangle<float>& r) const {
        const double norm = std::clamp((double)(x - r.getX()) / (double)r.getWidth(), 0.0, 1.0);
        return 20.*std::pow(maxFrequency()/20.,norm);
    }

    float yFromDb(double db, const juce::Rectangle<float>& r) const {
        const double minDb=-displayRange,maxDb=displayRange;
        const double norm = (std::clamp(db, minDb, maxDb) - minDb) / (maxDb - minDb);
        return r.getY() + (float)((1.0 - norm) * r.getHeight());
    }

    double dbFromY(float y, const juce::Rectangle<float>& r) const {
        const double minDb=-displayRange,maxDb=displayRange;
        const double norm = 1.0 - std::clamp((double)(y - r.getY()) / (double)r.getHeight(), 0.0, 1.0);
        return minDb + norm * (maxDb - minDb);
    }

    juce::Point<float> getNodePosition(int index, const juce::Rectangle<float>& r) const {
        if (index < 0 || index >= (int)currentEq.bands.size()) return { 0, 0 };
        const auto& b = currentEq.bands[(size_t)index];
        const double db = (b.type == (int)EqFilterType::LowCut || b.type == (int)EqFilterType::HighCut || b.type == (int)EqFilterType::Notch) ? 0.0 : b.gain;
        return {xFromFreq(b.freq,r),yFromDb(std::clamp(db,-displayRange,displayRange),r)};
    }

    int findBandAt(juce::Point<float> pos,const juce::Rectangle<float>& r)const{
        int best=-1;float distance=15;
        for(int i=0;i<(int)currentEq.bands.size();++i){const float d=pos.getDistanceFrom(getNodePosition(i,r));if(d<distance||(d==distance&&i==selectedBand)){distance=d;best=i;}}
        return best;
    }

    struct HudLayout {
        juce::Rectangle<float> badge, pills[6], freq, gain, q, bypass, invert, del;
    };

    HudLayout hudLayout(const juce::Rectangle<float>& hud)const{
        HudLayout l;l.badge=hud.reduced(14,10).withHeight(25);
        for(int i=0;i<6;++i)l.pills[i]=shapes[(size_t)i].getBounds().toFloat();
        auto readout=[](const juce::Slider& dial){auto r=dial.getBounds().toFloat();return r.withY(r.getBottom()-27).withHeight(25).withSizeKeepingCentre(std::min(104.f,r.getWidth()),25);};
        l.freq=readout(knobs[0]);l.gain=readout(knobs[1]);l.q=readout(knobs[2]);
        l.bypass=bandPower.getBounds().toFloat();l.invert=invertButton.getBounds().toFloat();l.del=removeButton.getBounds().toFloat();return l;
    }
    juce::Rectangle<float> getHudBounds(const juce::Rectangle<float>&)const{const float h=hudHeight();return {12.f,(float)getHeight()-h-8.f,(float)getWidth()-24.f,h};}
    void drawBandHud(juce::Graphics& g,const juce::Rectangle<float>& plot){
        const auto rack=getHudBounds(plot);g.setColour(juce::Colour(0xff1a1f26));g.fillRoundedRectangle(rack,6);g.setColour(juce::Colour(0xff333b46));g.drawRoundedRectangle(rack,6,.8f);
        auto left=rack.reduced(14,10);const bool selected=validBand();
        const bool wide=getWidth()>=800;const auto colour=selected?getBandColour((size_t)selectedBand):Daw::textFaint;
        g.setColour(colour.withAlpha(.14f));g.fillRoundedRectangle(left.getX(),left.getY()+1,29,20,4);
        g.setColour(colour);g.setFont(Daw::font(12,true));g.drawText(selected?juce::String(selectedBand+1).paddedLeft('0',2):"--",juce::Rectangle<float>(left.getX(),left.getY()+1,29,20),juce::Justification::centred);
        g.setColour(Daw::text);g.setFont(Daw::font(11,true));g.drawText(tr("FILTER SHAPE"),juce::Rectangle<float>(left.getX()+34,left.getY()+1,160,20),juce::Justification::centredLeft);
        (void)wide;
        const float line=knobs[0].getX()-8.f;g.setColour(juce::Colour(0xff303740));g.drawVerticalLine((int)line,rack.getY()+16,rack.getBottom()-16);
        if(!selected){g.setFont(Daw::font(10));g.setColour(Daw::textFaint);g.drawFittedText(tr("SELECT BAND"),removeButton.getBounds().withY((int)left.getY()).withHeight(27),juce::Justification::centred,2);}
    }

    void showBandMenu(int index,juce::Point<int> screen){
        selectBand(index);juce::PopupMenu menu;
        menu.addSectionHeader(tr("BAND")+" "+juce::String(index+1));
        for(int i=0;i<6;++i)menu.addItem(10+i,tr(filterTypeNames[i]),true,currentEq.bands[(size_t)index].type==i);
        menu.addSeparator();menu.addItem(1,tr("Invert Gain"),eqHasGain(currentEq.bands[(size_t)index].type));
        menu.addItem(2,tr(currentEq.bands[(size_t)index].enabled?"Bypass Band":"Enable Band"));menu.addItem(3,tr("Delete Band"));
        const auto epoch=revision;juce::Component::SafePointer<EqDisplayComponent> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({screen.x,screen.y,1,1}),[safe,index,epoch](int result){
            if(!safe||safe->revision!=epoch||index>=(int)safe->currentEq.bands.size()||result==0)return;
            if(result==3){safe->deleteSelectedBand();return;}
            safe->editOnce([&]{auto& b=safe->currentEq.bands[(size_t)index];if(result>=10&&result<=15)b.type=result-10;else if(result==1&&eqHasGain(b.type))b.gain=-b.gain;else if(result==2)b.enabled=!b.enabled;b.normalise();});
        });
    }
    void showCanvasMenu(double hz,double db,juce::Point<int> screen){
        juce::PopupMenu menu;menu.addSectionHeader(tr("+ ADD BAND"));
        for(int i=0;i<6;++i)menu.addItem(10+i,tr(filterTypeNames[i]),currentEq.bands.size()<16);
        menu.addSeparator();menu.addItem(1,tr("RESET EQ"));
        const auto epoch=revision;juce::Component::SafePointer<EqDisplayComponent> safe(this);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({screen.x,screen.y,1,1}),[safe,hz,db,epoch](int result){
            if(!safe||safe->revision!=epoch)return;
            if(result>=10&&result<=15)safe->addBandAt(hz,db,result-10);else if(result==1)safe->flatten();
        });
    }

};

} // namespace

class GraphicEqWindow::Panel : public juce::Component {
public:
    explicit Panel(MainComponent& m):owner(m){
        setLookAndFeel(&consoleLook);
        for(juce::Component* c:std::initializer_list<juce::Component*>{&eqDisplay,&trackSelector,&bandSelector,&addBandButton,&enableButton,&resetButton,&analyzerMode,&speed,&range})addAndMakeVisible(c);
        Localisation::bindTip(addBandButton,"Add a new parametric band to the EQ.");
        Localisation::bindTip(enableButton,"Toggle EQ on or off for this track.");
        Localisation::bindTip(resetButton,"Flatten gains and bypass cut/notch filters. Undo restores the EQ.");
        eqDisplay.getSampleRate=[this]{return owner.audio.spectrumSampleRate();};
        eqDisplay.getSpectrum=[this](std::vector<float>& pre,std::vector<float>& post){
            const auto stamp=owner.audio.spectrumRevision();
            if(stamp==lastSpectrumRevision)return false;
            lastSpectrumRevision=stamp;if(stamp-spectrumGate<(std::uint32_t)EqAnalyzer::size)return false;
            owner.audio.readSpectrum(currentTrack,pre,post,EqAnalyzer::size,true);return true;
        };
        addBandButton.onClick=[this]{if(validTrack())eqDisplay.addBandAt(1000,0);};
        enableButton.onClick=[this]{if(updating||!validTrack())return;owner.checkpointMix();owner.project.tracks[(size_t)currentTrack].eq.enabled=enableButton.getToggleState();owner.mixChanged();};
        resetButton.onClick=[this]{if(validTrack())eqDisplay.flatten();};
        trackSelector.onChange=[this]{if(updating||trackSelector.getSelectedId()<1)return;currentTrack=trackSelector.getSelectedId()-1;owner.select(currentTrack,owner.selectedSection);refresh();};
        bandSelector.onChange=[this]{if(!updating)eqDisplay.selectBand(bandSelector.getSelectedId()-2);};
        eqDisplay.onSelectionChanged=[this]{refresh();};
        eqDisplay.onDragStart=[this]{if(validTrack())owner.checkpointMix();};
        eqDisplay.onSettingsChanged=[this]{if(updating||!validTrack())return;owner.project.tracks[(size_t)currentTrack].eq=eqDisplay.getEq();owner.mixChanged();};
        eqDisplay.onDragEnd=[this]{refresh();};
        eqDisplay.onWheelStart=[this]{if(validTrack())owner.checkpointMix(&eqDisplay);};
        for(auto* combo:{&analyzerMode,&speed,&range})combo->onChange=[this]{if(updating)return;Localisation::setSetting("eqAnalyzerMode",analyzerMode.getSelectedId()-1);Localisation::setSetting("eqAnalyzerSpeed",speed.getSelectedId()-1);Localisation::setSetting("eqDisplayRange",range.getSelectedId()-1);updateView();};
        analyzerMode.setSelectedId(std::clamp(Localisation::setting("eqAnalyzerMode","1").getIntValue(),0,3)+1,juce::dontSendNotification);
        speed.setSelectedId(std::clamp(Localisation::setting("eqAnalyzerSpeed","1").getIntValue(),0,2)+1,juce::dontSendNotification);
        range.setSelectedId(std::clamp(Localisation::setting("eqDisplayRange","1").getIntValue(),0,2)+1,juce::dontSendNotification);
        for(int i=0;i<16;++i){auto& chip=chips[(size_t)i];addAndMakeVisible(chip);chip.index=i;chip.onClick=[this,i]{if(!updating)eqDisplay.selectBand(i);};}
    }
    ~Panel()override{setLookAndFeel(nullptr);}
    void setTrack(int track){meterTrack=-2;currentTrack=std::clamp(track>=0?track:owner.selectedTrack,0,(int)owner.project.tracks.size()-1);refresh();}
    void refresh(){
        if(owner.project.tracks.empty())return;updating=true;
        currentTrack=std::clamp(currentTrack,0,(int)owner.project.tracks.size()-1);
        const auto& track=owner.project.tracks[(size_t)currentTrack];
        trackSelector.clear(juce::dontSendNotification);for(size_t i=0;i<owner.project.tracks.size();++i)trackSelector.addItem(juce::String::fromUTF8(owner.project.tracks[i].name.c_str()),(int)i+1);
        trackSelector.setSelectedId(currentTrack+1,juce::dontSendNotification);
        eqDisplay.setEq(track.eq,currentTrack,juce::String::fromUTF8(track.name.c_str()),track.sessionId);
        bandSelector.clear(juce::dontSendNotification);bandSelector.addItem(tr("SELECT BAND"),1);
        const auto& bands=eqDisplay.getEq().bands;
        for(size_t i=0;i<bands.size();++i){const auto& b=bands[i];bandSelector.addItem(juce::String((int)i+1)+juce::String::fromUTF8(" · ")+tr(filterTypeNames[b.type])+juce::String::fromUTF8(" · ")+(b.freq>=1000?juce::String(b.freq/1000,2)+"k":juce::String((int)b.freq))+(!b.enabled?" ["+tr("OFF")+"]":""),(int)i+2);}
        bandSelector.setSelectedId(eqDisplay.getSelectedBand()+2,juce::dontSendNotification);
        for(int i=0;i<16;++i){auto& chip=chips[(size_t)i];const bool on=i<(int)bands.size();chip.setVisible(on);if(on){chip.ink=getBandColour((size_t)i);chip.setToggleState(i==eqDisplay.getSelectedBand(),juce::dontSendNotification);chip.repaint();}}
        addBandButton.setButtonText(tr("+ ADD BAND"));addBandButton.setEnabled(bands.size()<16);
        enableButton.setButtonText(tr(track.eq.enabled?"EQ ACTIVE":"EQ BYPASSED"));enableButton.setToggleState(track.eq.enabled,juce::dontSendNotification);
        resetButton.setButtonText(tr("RESET EQ"));
        auto populate=[](juce::ComboBox& c,std::initializer_list<juce::String> texts,int fallback){const int id=c.getSelectedId()>0?c.getSelectedId():fallback;c.clear(juce::dontSendNotification);int i=1;for(const auto& t:texts)c.addItem(t,i++);c.setSelectedId(id,juce::dontSendNotification);};
        populate(analyzerMode,{tr("ANALYZER OFF"),tr("INPUT / OUTPUT"),tr("INPUT ONLY"),tr("OUTPUT ONLY")},std::clamp(Localisation::setting("eqAnalyzerMode","1").getIntValue(),0,3)+1);
        populate(speed,{tr("RESPONSE: FAST"),tr("RESPONSE: MEDIUM"),tr("RESPONSE: SLOW")},std::clamp(Localisation::setting("eqAnalyzerSpeed","1").getIntValue(),0,2)+1);
        populate(range,{juce::String::fromUTF8("±6 dB"),juce::String::fromUTF8("±12 dB"),juce::String::fromUTF8("±24 dB")},std::clamp(Localisation::setting("eqDisplayRange","1").getIntValue(),0,2)+1);
        updating=false;updateView();resized();repaint();
    }
    void updateView(){
        const int mode=std::max(0,analyzerMode.getSelectedId()-1),selectedRange=std::max(0,range.getSelectedId()-1);
        eqDisplay.setView(mode,std::max(0,speed.getSelectedId()-1),selectedRange==0?6:selectedRange==1?12:24);
        const int wanted=mode==0?-1:currentTrack;
        if(wanted!=meterTrack){owner.audio.setSpectrumTrack(wanted);meterTrack=wanted;spectrumGate=lastSpectrumRevision=owner.audio.spectrumRevision();}
    }
    void paint(juce::Graphics& g)override{
        g.fillAll(Daw::base);
        Daw::deviceHeader(g,{16,6,440,50},tr("EQUALISER"),tr("PARAMETRIC"),juce::Colour(0xffe7dfc8));
        g.setColour(Daw::line);g.drawHorizontalLine(eqDisplay.getY()-8,14.f,(float)getWidth()-14);
        g.setColour(Daw::textFaint);g.setFont(Daw::smallFont(9));
        g.drawFittedText(tr(juce::String::fromUTF8("Double-click: add band · Wheel: Q · Shift: fine adjustment")),getLocalBounds().removeFromBottom(22).reduced(18,1).withTrimmedRight(118),juce::Justification::centredLeft,1);
        g.drawText(juce::String(owner.audio.spectrumSampleRate()/1000.,1)+juce::String::fromUTF8(" kHz · ")+juce::String((int)eqDisplay.getEq().bands.size())+"/16",getWidth()-140,getHeight()-22,122,20,juce::Justification::centredRight);
    }
    void resized()override{
        auto r=getLocalBounds().reduced(14);auto top=r.removeFromTop(50);
        if(getWidth()>=740){
            resetButton.setBounds(top.removeFromRight(72).reduced(0,10));top.removeFromRight(8);enableButton.setBounds(top.removeFromRight(132).reduced(0,10));top.removeFromRight(8);addBandButton.setBounds(top.removeFromRight(114).reduced(0,10));
        }else{
            r.removeFromTop(4);auto actions=r.removeFromTop(30);
            addBandButton.setBounds(actions.removeFromLeft(114));actions.removeFromLeft(8);enableButton.setBounds(actions.removeFromLeft(132));actions.removeFromLeft(8);resetButton.setBounds(actions.removeFromLeft(96));
        }
        r.removeFromTop(6);auto chipsRow=r.removeFromTop(30);
        trackSelector.setBounds(chipsRow.removeFromRight(std::min(220,chipsRow.getWidth()/3)).reduced(0,2));chipsRow.removeFromRight(8);
        int shown=0;for(const auto& chip:chips)if(chip.isVisible())++shown;
        const int chipW=shown>0?std::min(78,chipsRow.getWidth()/shown):0;int x=chipsRow.getX();
        for(auto& chip:chips)if(chip.isVisible()){chip.setBounds(x,chipsRow.getY(),std::max(0,chipW-4),chipsRow.getHeight());x+=chipW;}
        r.removeFromTop(6);
        if(getWidth()<680){auto bandRow=r.removeFromTop(30);bandSelector.setBounds(bandRow);r.removeFromTop(6);}
        auto settings=r.removeFromTop(30);
        range.setBounds(settings.removeFromRight(92));settings.removeFromRight(8);speed.setBounds(settings.removeFromRight(std::min(144,std::max(0,settings.getWidth()-90))));settings.removeFromRight(8);
        if(getWidth()<680)analyzerMode.setBounds(settings);else{analyzerMode.setBounds(settings.removeFromRight(std::min(174,std::max(0,settings.getWidth()/2))));settings.removeFromRight(8);bandSelector.setBounds(settings);}
        r.removeFromTop(8);r.removeFromBottom(12);eqDisplay.setBounds(r);
    }

#if FGA_SELF_TESTS
    bool writeEvidence(const juce::File& folder);
#endif
private:
    class Chip : public juce::Button {
    public:
        Chip():juce::Button("band"){}
        int index=0;juce::Colour ink{Daw::textDim};
        void paintButton(juce::Graphics& g,bool hover,bool) override {
            auto r=getLocalBounds().toFloat().reduced(2.f,2.f);const bool on=getToggleState();
            g.setColour(on?ink.withAlpha(.22f):hover?Daw::surfaceHi:Daw::surface);g.fillRoundedRectangle(r,6);
            g.setColour(on?ink:Daw::lineStrong);g.drawRoundedRectangle(r,6,on?1.4f:Daw::hairline());
            g.setColour(ink);g.fillEllipse(r.getX()+8,r.getCentreY()-4,8,8);
            g.setColour(Daw::text);g.setFont(Daw::font(11.f,true));g.drawText(juce::String(index+1).paddedLeft('0',2),r.withTrimmedLeft(18).toNearestInt(),juce::Justification::centredLeft);
        }
    };
    MainComponent& owner;EqLookAndFeel consoleLook;EqDisplayComponent eqDisplay;std::array<Chip,16> chips;
    juce::ComboBox trackSelector,bandSelector,analyzerMode,speed,range;
    juce::TooltipWindow tooltip{this,650};
    juce::TextButton addBandButton,resetButton;juce::ToggleButton enableButton;
    int currentTrack=0,meterTrack=-2;bool updating=false;
    std::uint32_t lastSpectrumRevision=0,spectrumGate=0;
    bool validTrack()const{return currentTrack>=0&&currentTrack<(int)owner.project.tracks.size();}
};

GraphicEqWindow::GraphicEqWindow(MainComponent& m)
    : DocumentWindow(tr("PARAMETRIC EQ"), Daw::base, juce::DocumentWindow::closeButton),
      owner(m) {
    setLookAndFeel(&m.theme);
    setUsingNativeTitleBar(true);
    panel = new Panel(m);
    setContentOwned(panel, false);
    setResizable(true, false);
    // Fit the editor inside the display's usable area (DPI and small screens included).
    const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
    const auto area = display ? display->userArea : juce::Rectangle<int>(0, 0, 1280, 800);
    const int maxW=juce::jmax(480,area.getWidth()-32);
    const int maxH=juce::jmax(360,area.getHeight()-48);
    setResizeLimits(std::min(780,maxW),std::min(620,maxH),maxW,maxH);
    setSize(std::min(1040,maxW),std::min(760,maxH));
    setAlwaysOnTop(true);
}

GraphicEqWindow::~GraphicEqWindow() {
    owner.audio.setSpectrumTrack(-1);
    clearContentComponent();
    setLookAndFeel(nullptr);
}

void GraphicEqWindow::open(int track) {
    panel->setTrack(track);
    if (!isVisible()) {
        // Open at a safe offset; centring a scaled top-level window can push it off-screen.
        const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        const auto area = display ? display->userArea : juce::Rectangle<int>(0, 0, 1280, 800);
        setTopLeftPosition(area.getX() + 40, area.getY() + 40);
    }
    setAlwaysOnTop(true);
    setVisible(true);
    toFront(true);
}

void GraphicEqWindow::refresh() {
    setName(tr("PARAMETRIC EQ"));
    if (panel) panel->refresh();
}

void GraphicEqWindow::resized() {
    DocumentWindow::resized();

}

void GraphicEqWindow::closeButtonPressed() {
    owner.audio.setSpectrumTrack(-1);
    owner.dismissAuxiliaryWindow(*this);
}

#if FGA_SELF_TESTS
bool GraphicEqWindow::Panel::writeEvidence(const juce::File& folder){
    juce::StringArray checks;bool ok=true;auto expect=[&](bool passed,const char* label){checks.add(juce::String(passed?"PASS ":"FAIL ")+label);ok=passed&&ok;};
    EqDisplayComponent test;addAndMakeVisible(test);test.setBounds(eqDisplay.getBounds());ok=test.verifyInteractions(checks)&&ok;removeChildComponent(&test);
    for(double rate:{44100.,48000.,96000.}){
        EqAnalyzer analyzer;EqAnalyzer::Trace trace{};std::vector<float> sine(EqAnalyzer::size*2);
        const int bin=(int)std::lround(1000.*EqAnalyzer::size/rate);const double hz=bin*rate/EqAnalyzer::size;
        for(int i=0;i<EqAnalyzer::size;++i){sine[(size_t)i]=(float)std::sin(juce::MathConstants<double>::twoPi*bin*i/EqAnalyzer::size);sine[(size_t)i+EqAnalyzer::size]=-sine[(size_t)i];}
        for(int frame=0;frame<20;++frame)analyzer.analyse(sine,rate,trace,0);
        const auto peak=std::max_element(trace.begin(),trace.end());const double measuredHz=20.*std::pow(1000.,std::distance(trace.begin(),peak)/(double)(EqAnalyzer::points-1));
        expect(std::abs(measuredHz/hz-1)<.1&&std::abs((*peak-1)*96)<1,"spectrum follows the actual sample rate and retains opposite-phase stereo energy");
        for(int frame=0;frame<80;++frame)analyzer.analyse({},rate,trace,0);expect(*std::max_element(trace.begin(),trace.end())<.01,"stopped/missing input decays instead of freezing an old spectrum");
        for(int shape=0;shape<6;++shape){
            EqSettings eq;eq.enabled=true;eq.bands={{true,shape,700,4,1.2},{true,0,1200,-3,.9},{true,0,3200,2,2},{true,0,5200,-1,1}};eq.syncLegacyFromBands();
            ChannelStrip strip;strip.configure(eq,rate);const int frames=(int)rate;juce::AudioBuffer<float> audio(2,frames);
            for(int i=0;i<frames;++i)for(int ch=0;ch<2;++ch)audio.setSample(ch,i,(float)(.1*std::sin(juce::MathConstants<double>::twoPi*1000*i/rate)));
            const float dry=audio.getRMSLevel(0,frames/2,frames/2);strip.process(audio,frames);const double measured=20*std::log10(audio.getRMSLevel(0,frames/2,frames/2)/dry);double response=0;for(const auto& band:eq.bands)response+=eqCoefficients(band,rate).magnitudeDb(1000,rate);
            expect(std::abs(measured-response)<.15,"all six graph filter shapes match the processed audio, including four custom bands");
        }
    }
    setTrack(0);owner.undoStack.clear();owner.redoStack.clear();
    const auto original=owner.project.tracks[0].eq;const auto midiBefore=midiFile(owner.project);
    eqDisplay.addBandAt(2500,3);const auto added=owner.project.tracks[0].eq;
    expect(added.bands.size()==eqDisplay.getEq().bands.size()&&owner.undoStack.size()==1,"adding a band records exactly one real mix Undo");
    owner.undo(false);expect(owner.project.tracks[0].eq==original,"real Undo restores the previous EQ without regeneration");
    owner.undo(true);expect(owner.project.tracks[0].eq==added,"real Redo restores the added band");
    eqDisplay.selectBand((int)added.bands.size()-1);const auto historyBefore=owner.undoStack.size();
    eqDisplay.simulateKnobGesture(0,2800,3100);
    expect(owner.project.tracks[0].eq.bands.back().freq==3100&&owner.undoStack.size()==historyBefore+1,"real rotary gesture remains one Undo despite live graph and inspector refresh");
    owner.undo(false);expect(owner.project.tracks[0].eq==added,"real rotary Undo restores the previous parameter and selected curve");
    eqDisplay.flatten();owner.undo(false);expect(owner.project.tracks[0].eq==added&&midiFile(owner.project)==midiBefore,"reset/Undo preserves the curve and the song MIDI");
    owner.undoStack.clear();owner.redoStack.clear();
    auto& eq=owner.project.tracks[0].eq;eq.enabled=true;
    eq.bands={{true,1,35,0,.707},{true,2,110,2,.707},{true,0,420,-3.5,1.4},{true,0,2400,2.8,1.1},{true,3,9000,1.5,.707}};eq.syncLegacyFromBands();setTrack(0);eqDisplay.selectBand(2);
    const auto language=Localisation::current();
    for(auto lang:{Localisation::Language::italian,Localisation::Language::english}){
        Localisation::setCurrent(lang);refresh();
        for(auto size:std::initializer_list<juce::Point<int>>{{780,620},{1040,760}}){setSize(size.x,size.y);const auto image=createComponentSnapshot(getLocalBounds());const auto name=juce::String(lang==Localisation::Language::italian?"eq-it-":"eq-en-")+juce::String(size.x)+".png";if(auto stream=folder.getChildFile(name).createOutputStream()){juce::PNGImageFormat png;png.writeImageToStream(image,*stream);}}
    }
    Localisation::setCurrent(language);folder.getChildFile("eq-checks.txt").replaceWithText(checks.joinIntoString("\n"));return ok;
}
bool GraphicEqWindow::writeUiEvidence(const juce::File& folder){
    folder.createDirectory();const auto before=owner.project.tracks[0].eq;
    owner.openGraphicEq(0);auto* live=owner.graphicEqWindow.get();
    const bool readOnly=owner.project.tracks[0].eq==before;
    const bool ok=live&&live->panel->writeEvidence(folder);
    folder.getChildFile("eq-checks.txt").appendText(juce::String(readOnly?"\nPASS ":"\nFAIL ")+"opening the editor does not mutate stored settings");
    return readOnly&&ok;
}

#endif

} // namespace fga
