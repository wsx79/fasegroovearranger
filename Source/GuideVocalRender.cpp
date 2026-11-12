// SPDX-License-Identifier: AGPL-3.0-or-later
#include "GuideVocalRender.h"
#include <map>
namespace fga {
juce::Result guideVocalPlan(const Project& p,int track,int index,GuideVocalPlan& result){
    if(track<0||track>=(int)p.tracks.size()||index<0||index>=(int)p.tracks[(size_t)track].clips.size())return juce::Result::fail("Vocal clip missing.");
    const auto& t=p.tracks[(size_t)track];const auto& clip=t.clips[(size_t)index];
    if(!clip.vocal||clip.audioOffset>=0||t.vocalChops||!isManualTrack(t.kind))return juce::Result::fail("Vocal Studio requires a separate manual MIDI clip.");
    if(!std::isfinite(p.settings.bpm)||p.settings.bpm<20||p.settings.bpm>400)return juce::Result::fail("Invalid tempo.");
    struct Sung {double start,end;int pitch,velocity,syllable;juce::String text;};std::vector<Sung> sung;
    std::map<int,const VocalSyllable*> links;for(const auto& s:clip.vocal->syllables)for(int id:s.noteIds)links[id]=&s;
    const double origin=p.clipStart(track,index);
    for(const auto& n:clip.notes){auto found=links.find(n.vocalId);if(found==links.end()||found->second->text.empty())continue;
        const auto& s=*found->second;
        // Keep the retained MIDI preparation plain-text and engine-independent.
        if(s.text.size()>256||s.text.find_first_of("<>[]&\r\n")!=std::string::npos||s.text.find('\0')!=std::string::npos)return juce::Result::fail("Use plain English syllables without markup.");
        if(!std::isfinite(n.start)||!std::isfinite(n.length)||n.pitch<0||n.pitch>127)return juce::Result::fail("Invalid guide note.");
        double from=std::max(n.start,clip.vocal->regionStart),to=std::min(n.start+n.length,clip.vocal->regionEnd);
        if(to<=from||n.velocity<=0)continue;
        std::vector<double> cuts{from,to};for(int i=0;i<=(int)p.sections.size();++i){const double b=p.sectionStart(i)-origin;if(b>from&&b<to)cuts.push_back(b);}std::sort(cuts.begin(),cuts.end());
        for(size_t i=1;i<cuts.size();++i)if(cuts[i]-cuts[i-1]>=1./960.&&vocalBeatAllowed(p,track,index,(cuts[i]+cuts[i-1])*.5))sung.push_back({cuts[i-1],cuts[i],n.pitch,n.velocity,s.id,juce::String::fromUTF8(s.text.c_str())});
        if(sung.size()>256)return juce::Result::fail("Guide limit: 256 notes per render. Split the clip.");
    }
    if(sung.empty())return juce::Result::fail("No assigned syllables in the allowed vocal region.");
    std::sort(sung.begin(),sung.end(),[](const auto& a,const auto& b){return a.start<b.start;});
    for(size_t i=1;i<sung.size();++i)if(sung[i].start<sung[i-1].end-1./960.)return juce::Result::fail("The guide needs a monophonic melody. Remove overlapping notes.");
    const double start=sung.front().start,end=sung.back().end,seconds=60./p.settings.bpm;
    if((end-start)*seconds>60)return juce::Result::fail("Guide limit: 60 seconds per render. Split the clip.");
    GuideVocalPlan plan;plan.start=origin+start;plan.duration=(end-start)*seconds;
    juce::MidiMessageSequence sequence;sequence.addEvent(juce::MidiMessage::tempoMetaEvent((int)std::lround(60000000./p.settings.bpm)));
    for(size_t i=0;i<sung.size();++i){const auto& n=sung[i];const double on=std::round((n.start-start)*960),off=std::round((n.end-start)*960);
        if(i==0||sung[i-1].syllable!=n.syllable||std::abs(sung[i-1].end-n.start)>1./960.){auto text=juce::MidiMessage::textMetaEvent(1,n.text);text.setTimeStamp(on);sequence.addEvent(text);}
        auto message=juce::MidiMessage::noteOn(1,n.pitch,(juce::uint8)n.velocity);message.setTimeStamp(on);sequence.addEvent(message);
        message=juce::MidiMessage::noteOff(1,n.pitch);message.setTimeStamp(off);sequence.addEvent(message);
        if(i+1==sung.size()||sung[i+1].syllable!=n.syllable||std::abs(n.end-sung[i+1].start)>1./960.){message=juce::MidiMessage::textMetaEvent(1,"");message.setTimeStamp(off);sequence.addEvent(message);}
        plan.audible.push_back({(n.start-start)*seconds,(n.end-start)*seconds});
    }
    juce::MidiFile midi;midi.setTicksPerQuarterNote(960);midi.addTrack(sequence);juce::MemoryOutputStream output(plan.midi,false);if(!midi.writeTo(output,0))return juce::Result::fail("Cannot prepare guide MIDI.");result=std::move(plan);return juce::Result::ok();
}
// Deliberately fail closed: retained pipeline entry, no installed/launchable backend.
juce::Result renderGuideVocal(const Project&,int,int,AudioTake&,
    const std::function<bool()>&,const std::function<void(double)>&){
    return juce::Result::fail("Vocal Studio rendering is unavailable; no backend is installed.");
}
}
