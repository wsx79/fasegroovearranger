// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AssistantProject.h"
#include "AudioEngine.h"
#include "NativeCompressor.h"
#include "CompressorDSP.h"
namespace fga {
namespace {
int target(const Project& p,std::uint64_t session){if(!session)return masterEffectTrack;for(size_t i=0;i<p.tracks.size();++i)if(p.tracks[i].sessionId==session)return (int)i;return -1;}
bool audible(const Project& p,int ti){
    const auto& t=p.tracks[(size_t)ti];if(t.mute||t.gain<=0)return false;
    if(std::any_of(p.tracks.begin(),p.tracks.end(),[](const auto& x){return x.solo;})&&!t.solo)return false;
    const bool groupSolo=p.buses[0].solo||p.buses[1].solo;
    if(t.group>=0&&t.group<2){const auto& group=p.buses[(size_t)t.group];if(group.mute||group.gain<=0||(groupSolo&&!group.solo))return false;}else if(groupSolo)return false;
    if(p.buses[2].solo||p.buses[3].solo){bool sent=false;for(int b=0;b<2;++b)sent=sent||(t.sends[(size_t)b]>0&&p.buses[(size_t)(b+2)].solo&&!p.buses[(size_t)(b+2)].mute&&p.buses[(size_t)(b+2)].gain>0);if(!sent)return false;}
    return true;
}
bool automated(const Track& t,int slot,int index,const juce::String& id){for(const auto& lane:t.parameters)if(lane.slot==slot&&lane.curve.active()&&(lane.parameterId==id.toStdString()||lane.index==index))return true;return false;}
}
std::vector<MixInput> AssistantAudio::mixInputs()const{std::vector<MixInput> result{{mix()}};for(int ti:tracks){const auto& t=snapshot.tracks[(size_t)ti];result.push_back({stem(ti),t.sessionId,t.kind==4?1:t.kind==1?2:3});}return result;}
std::vector<AssistantChange> assistantGainChanges(const Project& p,const MixReport& report){
    std::vector<AssistantChange> out;
    for(const auto& advice:report.suggestions){if(advice.trimDb>=0)continue;if(std::any_of(out.begin(),out.end(),[&](const auto& c){return c.session==advice.session;}))continue;const int ti=target(p,advice.session);if(ti<0||(ti<maxTracks&&!audible(p,ti)))continue;const double before=ti==masterEffectTrack?p.masterGain:p.tracks[(size_t)ti].gain;if(before<=0)continue;
        AssistantChange c;c.session=advice.session;c.before=before;c.after=before*juce::Decibels::decibelsToGain(advice.trimDb);c.reason=advice.session==0?advice.text:tr(p.tracks[(size_t)ti].kind==1?"Possible kick/bass overlap; audition this attenuation.":"Possible spectral masking; audition this attenuation.");c.selected=advice.session==0;out.push_back(std::move(c));}
    return out;
}
void assistantEqChanges(const Project& p,const MixReport& report,std::vector<AssistantChange>& out){
    // Alternative, explicitly selectable 1.5-dB bell cut for measured kick/bass overlap.
    for(const auto& advice:report.suggestions){const int ti=target(p,advice.session);if(ti<0||ti>=maxTracks||advice.trimDb>=0||p.tracks[(size_t)ti].kind!=1||!audible(p,ti))continue;
        const auto& eq=p.tracks[(size_t)ti].eq;if(eq.bands.size()>=16)continue;
        AssistantChange c;c.kind=AssistantChange::eq;c.session=advice.session;c.originalEq=eq;c.frequency=100;c.after=-1.5;c.reason=tr("Alternative gentle bass EQ cut; audition before accepting.");c.selected=false;out.push_back(std::move(c));break;
    }
}
void assistantCompressorChanges(AudioEngine& audio,const Project& p,int track,const MixMeasure& measure,std::vector<AssistantChange>& out){
    if(measure.rms<=1e-12||measure.peak<=0||(track<maxTracks&&!audible(p,track)))return;
    const double crest=juce::Decibels::gainToDecibels(measure.peak/measure.rms);
    const auto& rack=p.effectTarget(track);
    for(const auto& slot:rack.effects){if(slot.bypassed)continue;auto* processor=audio.effectPlugin(track,slot.slot);if(!processor||nativeCompressorSource(processor)!=-1)continue;juce::PluginDescription description;processor->fillInPluginDescription(description);if(!isNativeCompressor(description))continue;
        const std::array<float,6> wanted=crest<6?CompressorDSP::Values{-18,1.5f,6,30,180,0}:CompressorDSP::Values{-18,2,6,30,180,0};
        for(int index:{1,3,4}){auto* parameter=processor->getParameters()[index];auto* hosted=dynamic_cast<juce::HostedAudioProcessorParameter*>(parameter);if(!hosted||automated(rack,slot.slot,index,hosted->getParameterID()))continue;
            const double next=(wanted[(size_t)index]-CompressorDSP::minimum[(size_t)index])/(CompressorDSP::maximum[(size_t)index]-CompressorDSP::minimum[(size_t)index]);
            if(std::abs(parameter->getValue()-next)<1e-5)continue;
            AssistantChange c;c.kind=AssistantChange::compressor;c.session=track==masterEffectTrack?0:p.tracks[(size_t)track].sessionId;c.slot=slot.slot;c.parameter=index;c.before=parameter->getValue();c.after=next;c.selected=false;c.reason=tr("Conservative existing compressor setting; compare at matched volume.");out.push_back(std::move(c));
        }break;
    }
}
juce::Result assistantCandidate(const Project& p,AudioEngine& audio,const std::vector<AssistantChange>& changes,Project& result){
    result=p;
    for(const auto& c:changes){if(!c.selected)continue;const int ti=target(p,c.session);if(ti<0)return juce::Result::fail(tr("Suggested track no longer exists; analyse again."));
        if(!std::isfinite(c.before)||!std::isfinite(c.after))return juce::Result::fail(tr("Invalid suggested value."));
        if(c.kind==AssistantChange::gain){const double current=ti==masterEffectTrack?p.masterGain:p.tracks[(size_t)ti].gain;if(std::abs(current-c.before)>1e-9||c.after<0||c.after>2)return juce::Result::fail(tr("Project changed; analyse again."));if(ti==masterEffectTrack)result.masterGain=c.after;else result.tracks[(size_t)ti].gain=c.after;}
        else if(c.kind==AssistantChange::eq){if(ti==masterEffectTrack||!(p.tracks[(size_t)ti].eq==c.originalEq)||!std::isfinite(c.frequency)||!std::isfinite(c.q)||c.q<.1||c.q>18||c.frequency<20||c.frequency>20000||c.after>0||c.after< -3)return juce::Result::fail(tr("EQ changed or unavailable; analyse again."));
            auto& eq=result.tracks[(size_t)ti].eq;eq.ensureBands();if(!eq.enabled)for(auto& band:eq.bands)band.enabled=false;if(eq.bands.size()>=16)return juce::Result::fail(tr("No free EQ band."));eq.enabled=true;eq.bands.push_back({true,(int)EqFilterType::Bell,c.frequency,c.after,c.q});eq.syncLegacyFromBands();}
        else {auto* fx=audio.effectPlugin(ti,c.slot);auto* slot=p.effectTarget(ti).effect(c.slot);if(!fx||!slot||slot->bypassed||c.parameter<0||c.parameter>=6||c.after<0||c.after>1)return juce::Result::fail(tr("Suggested compressor is unavailable."));
            juce::PluginDescription description;fx->fillInPluginDescription(description);
            if(!isNativeCompressor(description)||nativeCompressorSource(fx)!=-1||c.parameter>=fx->getParameters().size())return juce::Result::fail(tr("Compressor changed or automated; analyse again."));
            auto* parameter=fx->getParameters()[c.parameter];auto* hosted=dynamic_cast<juce::HostedAudioProcessorParameter*>(parameter);
            if(!hosted||std::abs(parameter->getValue()-c.before)>1e-5||automated(p.effectTarget(ti),c.slot,c.parameter,hosted->getParameterID()))return juce::Result::fail(tr("Compressor changed or automated; analyse again."));}
    }
    return juce::Result::ok();
}
void assistantParameters(AudioEngine& audio,const Project& p,const std::vector<AssistantChange>& changes,bool after){
    for(const auto& c:changes)if(c.selected&&c.kind==AssistantChange::compressor){const int ti=target(p,c.session);if(auto* plugin=audio.effectPlugin(ti,c.slot))plugin->getParameters()[c.parameter]->setValue((float)(after?c.after:c.before));}
}
}
