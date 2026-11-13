// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
// Shared presentation for the Mix and Mastering assistants: measured-value cards with their
// scale, the Analyse -> Review -> Apply -> Compare stepper and a severity-coloured report.
// Everything drawn here comes from analysis results; no value is invented for display.
#include "DawTheme.h"
#include "Localisation.h"
#include <cmath>
#include <vector>
namespace fga::assistantui {

enum class Level {neutral,good,warn,bad};
inline juce::Colour levelColour(Level l){return l==Level::good?Daw::meterLow:l==Level::warn?Daw::meterMid:l==Level::bad?Daw::meterHigh:Daw::textDim;}

// One measured quantity. `low`/`high` define the drawn scale; `mark` an optional threshold.
struct Metric {
    juce::String label,value,unit;Level level=Level::neutral;
    double position=std::nan(""),low=0,high=1,mark=std::nan("");
};
inline juce::String db(double gain,int decimals=1){return gain>1e-9?juce::String(juce::Decibels::gainToDecibels(gain),decimals):juce::String("--");}

inline void drawMetrics(juce::Graphics& g,juce::Rectangle<int> area,const std::vector<Metric>& metrics,const juce::String& emptyText){
    Daw::devicePanel(g,area.toFloat(),{});
    if(metrics.empty()){g.setColour(Daw::textFaint);g.setFont(Daw::font(12.f));g.drawText(emptyText,area,juce::Justification::centred);return;}
    auto row=area.reduced(6,8);const int w=row.getWidth()/(int)metrics.size();
    for(size_t i=0;i<metrics.size();++i){
        const auto& m=metrics[i];auto cell=row.removeFromLeft(i+1==metrics.size()?row.getWidth():w).reduced(10,2);
        if(i>0){g.setColour(juce::Colour(0xff222529));g.drawVerticalLine(cell.getX()-10,(float)cell.getY()+4,(float)cell.getBottom()-4);}
        Daw::drawCaps(g,m.label,cell.removeFromTop(14),Daw::textFaint);
        auto value=cell.removeFromTop(30);g.setFont(Daw::font(22.f,true));g.setColour(m.level==Level::neutral?Daw::text:levelColour(m.level));
        const auto shown=m.value.isEmpty()?juce::String("--"):m.value;const int vw=juce::GlyphArrangement::getStringWidthInt(g.getCurrentFont(),shown);
        g.drawText(shown,value.withWidth(vw+2),juce::Justification::centredLeft);
        g.setFont(Daw::font(11.f));g.setColour(Daw::textDim);g.drawText(m.unit,value.withTrimmedLeft(vw+6),juce::Justification::centredLeft);
        // Scale bar: where the value sits, and the threshold it is compared with.
        if(std::isfinite(m.position)&&m.high>m.low){
            auto bar=cell.removeFromTop(10).withTrimmedTop(4).toFloat();g.setColour(juce::Colour(0xff1d1f23));g.fillRoundedRectangle(bar,2.f);
            const float at=(float)std::clamp((m.position-m.low)/(m.high-m.low),0.,1.);
            g.setColour(levelColour(m.level==Level::neutral?Level::good:m.level).withAlpha(.85f));g.fillRoundedRectangle(bar.withWidth(std::max(2.f,bar.getWidth()*at)),2.f);
            if(std::isfinite(m.mark)){const float x=bar.getX()+bar.getWidth()*(float)std::clamp((m.mark-m.low)/(m.high-m.low),0.,1.);g.setColour(Daw::text);g.fillRect(x-.75f,bar.getY()-3,1.5f,bar.getHeight()+6);}
        }
    }
}

// Analyse -> Review -> Apply -> Compare. `step` is the stage reached (0..3), -1 before analysis.
inline void drawSteps(juce::Graphics& g,juce::Rectangle<int> area,int step,bool busy){
    static const char* names[]{"ANALYSE","REVIEW","APPLY","COMPARE"};
    const int w=area.getWidth()/4;
    for(int i=0;i<4;++i){auto cell=area.removeFromLeft(w).reduced(3,0);const bool done=i<step,current=i==step||(busy&&i==0);
        const auto ink=current?Daw::accent:done?Daw::text:Daw::textFaint;
        auto dot=cell.removeFromLeft(18).toFloat().withSizeKeepingCentre(16,16);
        g.setColour(current?Daw::accent.withAlpha(.18f):juce::Colour(0xff1d1f23));g.fillEllipse(dot);g.setColour(ink);g.drawEllipse(dot.reduced(.5f),1.f);
        g.setFont(Daw::font(10.f,true));g.drawText(done?juce::String::fromUTF8("\xe2\x9c\x93"):juce::String(i+1),dot.toNearestInt(),juce::Justification::centred);
        cell.removeFromLeft(6);Daw::drawCaps(g,tr(names[i]),cell,ink);
        if(i<3){g.setColour(juce::Colour(0xff2a2d33));g.drawHorizontalLine(area.getCentreY(),(float)cell.getRight()-8,(float)cell.getRight()+2);}}
}

// Fills a read-only editor with the report, colouring each line by what it is: section
// headings, PASS, items to check, measurements, unchecked items and numbered suggestions.
// The plain text stays exactly the report text (self-tests and copy/paste use it).
inline void showReport(juce::TextEditor& editor,const juce::String& report){
    editor.clear();editor.setCaretPosition(0);
    const auto lines=juce::StringArray::fromLines(report);
    for(int i=0;i<lines.size();++i){const auto& line=lines[i];const auto t=line.trimStart();
        int letters=0,upper=0;for(auto c:t.substring(0,40)){if(juce::CharacterFunctions::isLetter(c)){++letters;upper+=juce::CharacterFunctions::isUpperCase(c);}}
        const bool heading=letters>=6&&upper*10>=letters*8&&!t.containsChar(':');
        auto starts=[&](std::initializer_list<const char*> keys){for(auto* k:keys)if(t.startsWithIgnoreCase(juce::String::fromUTF8(k)))return true;return false;};
        juce::Colour colour=Daw::text;bool bold=false;
        if(heading||t.endsWith(":")&&letters>=6&&upper*10>=letters*8){colour=Daw::accent;bold=true;}
        else if(starts({"PASS","SUPERATO","OK:"}))colour=Daw::meterLow;
        else if(starts({"FAIL","CHECK","VERIFY","DA VERIFICARE","REVIEW","ATTENZIONE","WARNING"}))colour=Daw::meterMid;
        else if(starts({"NOT CHECKED","NON VERIFICATO","NOT RUN"}))colour=Daw::textFaint;
        else if(starts({"MEASURED","MISURA"}))colour=Daw::textDim;
        else if(t.isNotEmpty()&&juce::CharacterFunctions::isDigit(t[0])&&t.indexOfChar('.')>0&&t.indexOfChar('.')<4){colour=Daw::text;bold=true;}
        editor.setFont(Daw::font(heading?11.5f:13.f,bold));editor.setColour(juce::TextEditor::textColourId,colour);
        editor.insertTextAtCaret(line+(i+1<lines.size()?"\n":""));}
    if(report.endsWithChar('\n'))editor.insertTextAtCaret("\n");
    editor.setColour(juce::TextEditor::textColourId,Daw::text);editor.setFont(Daw::font(13.f));editor.setCaretPosition(0);
}

inline void stylePrimary(juce::TextButton& b){b.setColour(juce::TextButton::buttonColourId,Daw::accent);b.setColour(juce::TextButton::textColourOffId,Daw::onAccent);}
inline void styleReport(juce::TextEditor& e){e.setColour(juce::TextEditor::backgroundColourId,juce::Colour(0xff0f1013));e.setColour(juce::TextEditor::outlineColourId,juce::Colour(0xff25282d));e.setColour(juce::TextEditor::focusedOutlineColourId,juce::Colour(0xff25282d));e.setIndents(12,10);e.setLineSpacing(1.25f);}

// Metric sets built from real measurements.
inline Level peakLevel(double truePeak,double ceilingDb){const double tp=juce::Decibels::gainToDecibels(std::max(1e-9,truePeak));return tp>ceilingDb?Level::bad:tp>ceilingDb-1?Level::warn:Level::good;}
inline std::vector<Metric> mixMetrics(double lufs,double truePeak,double peak,double rms,bool correlationValid,double correlation,double ceilingDb){
    std::vector<Metric> m;const double tp=juce::Decibels::gainToDecibels(std::max(1e-9,truePeak));
    m.push_back({tr("INTEGRATED"),lufs> -99?juce::String(lufs,1):juce::String(),"LUFS",Level::neutral,lufs> -99?lufs:std::nan(""),-36,-4});
    m.push_back({tr("TRUE PEAK"),truePeak>1e-9?juce::String(tp,1):juce::String(),"dBTP",peakLevel(truePeak,ceilingDb),truePeak>1e-9?tp:std::nan(""),-24,3,ceilingDb});
    m.push_back({tr("SAMPLE PEAK"),db(peak),"dBFS",peak>=1?Level::bad:Level::neutral});
    const double crest=rms>1e-12&&peak>0?juce::Decibels::gainToDecibels(peak/rms):std::nan("");
    m.push_back({tr("CREST"),std::isfinite(crest)?juce::String(crest,1):juce::String(),"dB",std::isfinite(crest)&&crest<6?Level::warn:Level::neutral,crest,0,24});
    m.push_back({tr("CORRELATION"),correlationValid?juce::String(correlation,2):juce::String(),"",correlationValid&&correlation<0?Level::bad:correlationValid&&correlation<.2?Level::warn:Level::neutral,correlationValid?correlation:std::nan(""),-1,1,0.});
    return m;
}
}
