// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AssistantProject.h"
#include "AudioEngine.h"
#include "NativeCompressor.h"
#include "CompressorDSP.h"
#include <numeric>
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
        AssistantChange c;c.kind=AssistantChange::eq;c.session=advice.session;c.originalEq=eq;c.frequency=advice.frequency>0?advice.frequency:100;c.after=-1.5;c.reason=tr("Alternative gentle bass EQ cut; audition before accepting.");c.selected=false;out.push_back(std::move(c));break;
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
    for(const auto& c:changes){if(!c.selected)continue;const int ti=target(p,c.session);if(ti<0&&c.kind!=AssistantChange::sidechain)return juce::Result::fail(tr("Suggested track no longer exists; analyse again."));
        if(!std::isfinite(c.before)||!std::isfinite(c.after))return juce::Result::fail(tr("Invalid suggested value."));
        if(c.kind==AssistantChange::gain){const double current=ti==masterEffectTrack?p.masterGain:p.tracks[(size_t)ti].gain;if(std::abs(current-c.before)>1e-9||c.after<0||c.after>2)return juce::Result::fail(tr("Project changed; analyse again."));if(ti==masterEffectTrack)result.masterGain=c.after;else result.tracks[(size_t)ti].gain=c.after;}
        else if(c.kind==AssistantChange::eq){const bool cut=c.filterType==(int)EqFilterType::LowCut;
            if(!(p.effectTarget(ti).eq==c.originalEq)||!std::isfinite(c.frequency)||!std::isfinite(c.q)||c.q<.1||c.q>18||c.frequency<20||c.frequency>20000||c.after>2||c.after< -3||c.filterType<0||c.filterType>3)return juce::Result::fail(tr("EQ changed or unavailable; analyse again."));
            auto& eq=result.effectTarget(ti).eq;const int saved=(int)eq.bands.size();eq.ensureBands();if(!eq.enabled)for(int i=saved;i<(int)eq.bands.size();++i)eq.bands[(size_t)i].enabled=false;if(eq.bands.size()>=16)return juce::Result::fail(tr("No free EQ band."));eq.enabled=true;eq.bands.push_back({true,c.filterType,c.frequency,cut?0.:c.after,c.q});eq.syncLegacyFromBands();}
        else if(c.kind==AssistantChange::insert){auto& rack=result.effectTarget(ti);
            if(c.slot<1||c.slot>maxTrackEffects||p.effectTarget(ti).effect(c.slot)||rack.effect(c.slot)||(int)rack.effects.size()>=maxTrackEffects)return juce::Result::fail(tr("No free effect slot; analyse again."));
            for(int i=0;i<6;++i)if(!std::isfinite(c.values[(size_t)i]))return juce::Result::fail(tr("Invalid suggested value."));
            EffectSlot fx;fx.slot=c.slot;fx.name=c.effect.toStdString();const auto description=nativeCompressorDescription();fx.pluginName=description.name.toStdString();fx.path=description.fileOrIdentifier.toStdString();if(auto xml=description.createXml())fx.pluginId=xml->toString().toStdString();fx.state=nativeCompressorState(c.values,c.effect=="Limiter").toStdString();rack.effects.push_back(fx);}
        else if(c.kind==AssistantChange::send){if(ti<0||ti>=maxTracks||c.parameter<0||c.parameter>1||std::abs(p.tracks[(size_t)ti].sends[(size_t)c.parameter]-c.before)>1e-9||c.after<0||c.after>1)return juce::Result::fail(tr("Project changed; analyse again."));result.tracks[(size_t)ti].sends[(size_t)c.parameter]=c.after;}
        else if(c.kind==AssistantChange::group){if(ti<0||ti>=maxTracks||p.tracks[(size_t)ti].group!=(int)c.before||c.after<0||c.after>1)return juce::Result::fail(tr("Project changed; analyse again."));result.tracks[(size_t)ti].group=(int)c.after;}
        else if(c.kind==AssistantChange::sidechain){if(c.parameter<0||c.parameter>1||std::abs(p.buses[(size_t)c.parameter].sidechainAmount-c.before)>1e-9||c.after<0||c.after>1)return juce::Result::fail(tr("Project changed; analyse again."));result.buses[(size_t)c.parameter].sidechainAmount=c.after;result.buses[(size_t)c.parameter].sidechainSource=4;}
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

juce::String nativeCompressorState(const std::array<float,6>& values,bool limiter){
    static constexpr const char* ids[]{"threshold","ratio","knee","attack","release","makeup"};
    juce::XmlElement state("FgaCompressor");state.setAttribute("version",1);
    for(int i=0;i<6;++i)state.setAttribute(ids[i],(double)CompressorDSP::bounded(values[(size_t)i],i));
    if(limiter)state.setAttribute("limiter",1);state.setAttribute("sidechainSource",-1);state.setAttribute("mix",1.);
    juce::MemoryBlock data;juce::AudioProcessor::copyXmlToBinary(state,data);return data.toBase64Encoding();
}
namespace {
double db(double power){return 10*std::log10(std::max(1e-20,power));}
bool hasCompressor(const Track& rack){return std::any_of(rack.effects.begin(),rack.effects.end(),[](const EffectSlot& fx){return fx.path=="fga:native-compressor"||juce::String(fx.pluginName).containsIgnoreCase("comp")||juce::String(fx.pluginName).containsIgnoreCase("limit");});}
bool hasBand(const EqSettings& eq,int type,double from,double to){if(!eq.enabled)return false;std::vector<EqBand> list=eq.bands;if(list.empty()){const auto legacy=eq.legacyBands();list.assign(legacy.begin(),legacy.end());}for(const auto& b:list)if(b.enabled&&b.type==type&&b.freq>=from&&b.freq<=to&&(type==(int)EqFilterType::LowCut||std::abs(b.gain)>.1))return true;return false;}
int freeSlot(const Track& rack,const std::vector<AssistantChange>& out,std::uint64_t session){for(int slot=1;slot<=maxTrackEffects;++slot){if(rack.effect(slot))continue;if(std::any_of(out.begin(),out.end(),[&](const auto& c){return c.kind==AssistantChange::insert&&c.session==session&&c.slot==slot;}))continue;return slot;}return 0;}
AssistantChange eqChange(const Track& rack,std::uint64_t session,int type,double frequency,double gain,double q,const char* reason){AssistantChange c;c.kind=AssistantChange::eq;c.session=session;c.originalEq=rack.eq;c.filterType=type;c.frequency=frequency;c.after=gain;c.q=q;c.reason=tr(reason);return c;}
AssistantChange insertChange(const Track& rack,std::uint64_t session,const std::vector<AssistantChange>& out,const juce::String& name,std::array<float,6> values,const char* reason){AssistantChange c;c.kind=AssistantChange::insert;c.session=session;c.slot=freeSlot(rack,out,session);c.effect=name;c.values=values;c.reason=tr(reason);return c;}
}
void assistantMixProcessing(const Project& p,const MixReport& report,const std::vector<int>& tracks,std::vector<AssistantChange>& out){
    // Roles from the track slot: 0 piano, 1 bass, 2 strings, 3 guitar, 4 kick, other drums, lead,
    // then the user's audio/MIDI tracks. Each suggestion is conservative and selectable.
    bool kickPlays=false,bassPlays=false;std::vector<int> busy;
    for(size_t i=0;i<tracks.size()&&i+1<report.inputLevels.size();++i){const int ti=tracks[i];if(!audible(p,ti)||report.inputLevels[i+1].rms<1e-4)continue;busy.push_back((int)i);if(p.tracks[(size_t)ti].kind==4)kickPlays=true;if(p.tracks[(size_t)ti].kind==1)bassPlays=true;}
    for(int i:busy){const int ti=tracks[(size_t)i];const auto& t=p.tracks[(size_t)ti];const auto& m=report.inputLevels[(size_t)i+1];const int kind=t.kind;const auto session=t.sessionId;
        const bool bass=kind==1,kick=kind==4,drum=isDrumTrack(kind)&&!kick,lead=kind==leadTrack,keys=kind==0||kind==3,pads=kind==2,user=kind>leadTrack;
        // ---- EQ: clean the low end, then one tonal move from the stem's own balance ----
        const bool cymbal=kind>=8&&kind<=12||kind==16;const double cut=bass||kick?30:kind==15?60:cymbal?300:drum?100:lead?180:pads?140:keys?110:90;
        if(!hasBand(t.eq,(int)EqFilterType::LowCut,cut*.6,20000))out.push_back(eqChange(t,session,(int)EqFilterType::LowCut,cut,0,.707,bass||kick?"High-pass below the sub: removes rumble without touching the low end.":"High-pass: leaves the low end to kick and bass."));
        const auto& band=m.bandPower;const double total=std::accumulate(band.begin(),band.end(),0.);
        if(total>1e-14&&(keys||pads||user)&&band[2]/total>.45&&!hasBand(t.eq,(int)EqFilterType::Bell,200,500))out.push_back(eqChange(t,session,(int)EqFilterType::Bell,300,-2,1.,"Low-mid build-up (measured): gentle cut against mud."));
        if(total>1e-14&&(lead||keys||user)&&band[4]/total<.06&&!hasBand(t.eq,(int)EqFilterType::Bell,2000,5000))out.push_back(eqChange(t,session,(int)EqFilterType::Bell,3000,1.5,.8,"Little presence (measured): small lift to come forward."));
        // ---- Compressor: insert one by role when the track has none ----
        // Thresholds follow the part's peaks: its RMS over the whole song also counts the bars it rests.
        const double peakDb=juce::Decibels::gainToDecibels(m.peak,-120.);
        if(!hasCompressor(t)&&peakDb>-40){
            if(bass)out.push_back(insertChange(t,session,out,"Compressor",{(float)std::clamp(peakDb-10,-40.,-6.),4,6,10,120,2},"Bass compressor: steadier low end."));
            else if(lead||user)out.push_back(insertChange(t,session,out,"Compressor",{(float)std::clamp(peakDb-9,-40.,-6.),3,6,5,100,1.5f},"Lead compressor: even level in front of the mix."));
            else if(keys)out.push_back(insertChange(t,session,out,"Compressor",{(float)std::clamp(peakDb-8,-40.,-6.),2.5f,6,20,150,1},"Gentle compressor: controls the peaks of the part."));
        }
        // ---- Reverb (return A) and delay (return B) by role; low end stays dry ----
        if(ti<maxTracks){double wanted[2]{0,0};
            if(pads){wanted[0]=.30;wanted[1]=.05;}else if(keys){wanted[0]=.18;wanted[1]=kind==3?.12:.08;}else if(lead){wanted[0]=.20;wanted[1]=.18;}else if(user){wanted[0]=.16;wanted[1]=.10;}
            for(int b=0;b<2;++b){const double now=t.sends[(size_t)b];const bool dry=bass||kick;
                if((dry&&now>.05)||(!dry&&!drum&&wanted[b]>0&&std::abs(now-wanted[b])>.08)){AssistantChange c;c.kind=AssistantChange::send;c.session=session;c.parameter=b;c.before=now;c.after=dry?0.:wanted[b];c.reason=tr(b==0?(dry?"Reverb off the low end: keeps kick and bass tight.":"Reverb send by role: depth without washing the mix."):(dry?"Delay off the low end: keeps kick and bass tight.":"Delay send by role: space and movement."));out.push_back(c);}}
        }
    }
    // ---- Kick sidechain: bass (and pads) on group A, ducked by the kick ----
    const int genre=p.settings.genre;const double amount=isElectronicGenre(genre)?.55:genre==deepHouseGenre||isAfroLatinGenre(genre)||genre==1||genre==3?.3:genre==10||genre==11?0.:.4;
    if(kickPlays&&bassPlays&&amount>0&&p.buses[0].sidechainAmount<.05){
        std::vector<int> members;for(size_t ti=0;ti<p.tracks.size();++ti)if(p.tracks[ti].group==0)members.push_back((int)ti);
        const bool free=std::all_of(members.begin(),members.end(),[&](int ti){return p.tracks[(size_t)ti].kind==1||p.tracks[(size_t)ti].kind==2;});
        if(free){for(size_t ti=0;ti<p.tracks.size()&&ti<(size_t)maxTracks;++ti){const auto& t=p.tracks[ti];if((t.kind!=1&&t.kind!=2)||t.group==0||!audible(p,(int)ti))continue;AssistantChange c;c.kind=AssistantChange::group;c.session=t.sessionId;c.before=t.group;c.after=0;c.reason=tr("Group A for the kick sidechain.");out.push_back(c);}
            AssistantChange c;c.kind=AssistantChange::sidechain;c.session=0;c.parameter=0;c.before=p.buses[0].sidechainAmount;c.after=amount;c.reason=tr("Kick sidechain on group A: the bass makes room for the kick.");out.push_back(c);}
    }
}
void assistantMasterChain(const Project& p,const MasteringReport& report,const MasteringTargets& targets,std::vector<AssistantChange>& out){
    if(report.files.empty())return;const auto& m=report.files[0].mix;const auto& rack=p.masterEffects;
    // ---- Master EQ: sub clean-up, then shelves from the measured balance (or the reference) ----
    if(!hasBand(rack.eq,(int)EqFilterType::LowCut,15,20000))out.push_back(eqChange(rack,0,(int)EqFilterType::LowCut,25,0,.707,"Master high-pass at 25 Hz: removes inaudible sub energy."));
    constexpr double width[]{1.386,.693,1.139,1.386,1.099,1.386};   // ln(f2/f1) of the six bands: a pink balance is flat
    auto tilt=[&](const std::array<double,6>& b,double& low,double& high){const double mid=b[3]/width[3];if(mid<=1e-20)return false;low=db((b[0]/width[0]+b[1]/width[1])*.5/mid);high=db(b[5]/width[5]/mid);return true;};
    double low=0,high=0,lowTarget=6,highTarget=-6;   // club masters sit above pink in the lows, below it in the air
    if(report.hasReference){double rl=0,rh=0;if(tilt(report.reference.mix.bandPower,rl,rh)){lowTarget=rl;highTarget=rh;}}
    if(tilt(m.bandPower,low,high)){
        if(std::abs(low-lowTarget)>3&&!hasBand(rack.eq,(int)EqFilterType::LowShelf,40,250))out.push_back(eqChange(rack,0,(int)EqFilterType::LowShelf,100,std::clamp(-(low-lowTarget)/3,-1.5,1.5),.707,low>lowTarget?"Master low shelf: the low end is heavier than the target balance.":"Master low shelf: the low end is lighter than the target balance."));
        if(std::abs(high-highTarget)>3&&!hasBand(rack.eq,(int)EqFilterType::HighShelf,4000,16000))out.push_back(eqChange(rack,0,(int)EqFilterType::HighShelf,9000,std::clamp(-(high-highTarget)/3,-1.5,1.5),.707,high>highTarget?"Master high shelf: the top is brighter than the target balance.":"Master high shelf: the top is duller than the target balance."));
    }
    if(hasCompressor(rack)||m.rms<=0)return;
    // ---- Glue compressor, then a limiter that sets the loudness under the ceiling ----
    const double rmsDb=juce::Decibels::gainToDecibels(m.rms,-120.),peakDb=juce::Decibels::gainToDecibels(std::max(report.files[0].truePeak,m.peak),-120.);
    out.push_back(insertChange(rack,0,out,"Glue",{(float)std::clamp(peakDb-6,-40.,-3.),2,6,30,200,0},"Master glue compressor (2:1, slow attack): holds the mix together."));
    const double lufs=m.lufs> -99?m.lufs:rmsDb,target=targets.useLufs?targets.lufs:-14.;
    // Lookahead brickwall: Threshold is the ceiling (0.3 dB under it for inter-sample peaks), Makeup
    // the input gain.
    const double lift=target-lufs,drive=std::clamp(lift>3?lift*.9:lift,-12.,24.);   // measured: a full lift lands ~1.3 LU over the target
    out.push_back(insertChange(rack,0,out,"Limiter",{(float)std::clamp(targets.ceilingDb-.3,-60.,0.),20,0,.1f,80,(float)drive},targets.useLufs?"Master limiter: reaches the LUFS target under the dBTP ceiling.":"Master limiter: about -14 LUFS under the dBTP ceiling."));
}
}
