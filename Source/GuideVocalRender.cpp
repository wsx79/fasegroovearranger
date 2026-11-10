// SPDX-License-Identifier: AGPL-3.0-or-later
#include "GuideVocalRender.h"
#include "GuideProcess.h"
#include <juce_cryptography/juce_cryptography.h>
#include <map>
namespace fga {
juce::Result guideVocalPlan(const Project& p,int track,int index,GuideVocalPlan& result){
    if(track<0||track>=(int)p.tracks.size()||index<0||index>=(int)p.tracks[(size_t)track].clips.size())return juce::Result::fail("Vocal clip missing.");
    const auto& t=p.tracks[(size_t)track];const auto& clip=t.clips[(size_t)index];
    if(!clip.vocal||clip.audioOffset>=0||t.vocalChops||!isManualTrack(t.kind)||clip.vocal->voice!="ecantorix-en")return juce::Result::fail("Select eCantorix English guide.");
    if(!std::isfinite(p.settings.bpm)||p.settings.bpm<20||p.settings.bpm>400)return juce::Result::fail("Invalid tempo.");
    struct Sung {double start,end;int pitch,velocity,syllable;juce::String text;};std::vector<Sung> sung;
    std::map<int,const VocalSyllable*> links;for(const auto& s:clip.vocal->syllables)for(int id:s.noteIds)links[id]=&s;
    const double origin=p.clipStart(track,index);
    for(const auto& n:clip.notes){auto found=links.find(n.vocalId);if(found==links.end()||found->second->text.empty())continue;
        const auto& s=*found->second;
        // eSpeak accepts SSML: keep lyrics plain, never interpret project markup or code.
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
juce::Result renderGuideVocal(const Project& p,int track,int clip,const juce::File& script,const juce::File& cache,AudioTake& take,const std::function<bool()>& cancelled,const std::function<void(double)>& progress,bool* cacheHit){
    if(cacheHit)*cacheHit=false;
    GuideVocalPlan plan;if(auto r=guideVocalPlan(p,track,clip,plan);r.failed())return r;
    if(!script.existsAsFile()||!script.loadFileAsString().contains("FGA_PORTABLE_GUIDE_1")||script.getFileExtension()!=".pl"||!script.getSiblingFile("eCantorix").getChildFile("Util.pm").existsAsFile())return juce::Result::fail("Select ecantorix.pl prepared with helpers/ecantorix/prepare.pl from trusted upstream source.");
    if(cancelled())return juce::Result::fail("Guide cancelled.");
    if(cache.createDirectory().failed())return juce::Result::fail("Cannot create guide cache.");
    const auto work=cache.getChildFile("work-"+juce::Uuid().toString());
    if(work.createDirectory().failed())return juce::Result::fail("Cannot create guide workspace.");
    struct Cleanup {juce::File dir;~Cleanup(){dir.deleteRecursively();}} cleanup{work};
    const auto log=work.getChildFile("tool.log");
    auto tool=[](const char* name,const char* fallback){return juce::SystemStats::getEnvironmentVariable(name,fallback);};
    const auto perl=tool("FGA_PERL_BIN","perl"),espeak=tool("FGA_ESPEAK_BIN","espeak"),sox=tool("FGA_SOX_BIN","sox");
    // MIDI rounds to 960 PPQ. Include exact masking/duration too: sub-tick edits
    // must not collide and repeatedly overwrite one shared cache entry.
    juce::MemoryOutputStream identity;
    identity.write(plan.midi.getData(),plan.midi.getSize());identity.writeString("FGA-guide-3");
    identity.writeDouble(plan.duration);
    for(const auto& interval:plan.audible){identity.writeDouble(interval.first);identity.writeDouble(interval.second);}
    identity.writeString(juce::SHA256(script).toHexString());
    identity.writeString(juce::SHA256(script.getSiblingFile("eCantorix").getChildFile("Util.pm")).toHexString());
    for(const auto& args:{juce::StringArray{perl,"-MMath::FFT","-MMIDI","-MURI::Escape","-e","print qq($^V $Math::FFT::VERSION $MIDI::VERSION $URI::Escape::VERSION)"},juce::StringArray{espeak,"--version"},juce::StringArray{sox,"--version"}}){
        if(auto r=runGuideProcess(args,log,5000,cancelled);r.failed())return r;
        const auto version=log.loadFileAsString().trim();
        if(version.isEmpty())return juce::Result::fail("Guide dependency version missing.");
        identity.writeString(args[0]);identity.writeString(version);
        if(args[0]==espeak){
            const auto path=tool("FGA_ESPEAK_DATA",version.fromFirstOccurrenceOf("Data at:",false,false).trim().toRawUTF8());
            if(!juce::File::isAbsolutePath(path))return juce::Result::fail("Cannot locate eSpeak voice data for cache validation.");
            for(const auto& name:{"en_dict","phondata","phonindex","phontab","intonations"}){
                const auto data=juce::File(path).getChildFile(name);
                if(!data.existsAsFile())return juce::Result::fail("Missing eSpeak English voice data: "+juce::String(name));
                identity.writeString(juce::SHA256(data).toHexString());
            }
            auto voice=juce::File(path).getChildFile("voices/en");
            if(!voice.existsAsFile())voice=voice.getChildFile("en");
            if(!voice.existsAsFile())return juce::Result::fail("Missing default eSpeak English voice.");
            identity.writeString(juce::SHA256(voice).toHexString());
        }
    }
    const auto key=juce::SHA256(identity.getMemoryBlock()).toHexString();const auto file=cache.getChildFile(key+".wav");
    juce::WavAudioFormat format;
    auto open=[&](const juce::File& f){auto stream=f.createInputStream();return std::unique_ptr<juce::AudioFormatReader>(stream?format.createReaderFor(stream.release(),true):nullptr);};
    auto valid=[&](juce::AudioFormatReader* reader){return reader&&reader->sampleRate==22050&&reader->lengthInSamples==(juce::int64)std::ceil(plan.duration*22050)&&reader->numChannels==1;};
    auto reader=open(file);
    const bool reused=valid(reader.get())&&file.getSiblingFile(key+".sha256").loadFileAsString()==juce::SHA256(file).toHexString();
    if(!reused){
        reader.reset();
        const auto input=work.getChildFile("input.mid"),raw=work.getChildFile("raw.wav");
        if(!input.replaceWithData(plan.midi.getData(),plan.midi.getSize()))return juce::Result::fail("Cannot write guide MIDI.");
        progress(.1);
        if(auto r=runGuideProcess({perl,script.getFullPathName(),"-v","en","-t","24","-r","22050","-c",work.getFullPathName(),"-O","wav","-o",raw.getFullPathName(),input.getFullPathName()},log,180000,cancelled);r.failed())return r;
        progress(.8);
        reader=open(raw);if(!reader||reader->numChannels>2||reader->sampleRate!=22050||reader->lengthInSamples>22050*62)return juce::Result::fail("Invalid guide WAV.");
        const int frames=(int)std::ceil(plan.duration*22050);juce::AudioBuffer<float> audio(1,frames);audio.clear();if(!reader->read(&audio,0,std::min(frames,(int)reader->lengthInSamples),0,true,false))return juce::Result::fail("Cannot read guide WAV.");
        // Force rests and intro/outro boundaries to exact silence, including synthesis tails.
        size_t segment=0;for(int i=0;i<frames;++i){const double at=i/22050.;while(segment<plan.audible.size()&&at>=plan.audible[segment].second)++segment;auto& value=audio.getWritePointer(0)[i];if(segment==plan.audible.size()||at<plan.audible[segment].first||!std::isfinite(value))value=0;}
        if(cancelled())return juce::Result::fail("Guide cancelled.");
        juce::TemporaryFile temporary(file);
        std::unique_ptr<juce::OutputStream> stream=temporary.getFile().createOutputStream();
        std::unique_ptr<juce::AudioFormatWriter> writer;
        if(stream)writer=format.createWriterFor(stream,juce::AudioFormatWriterOptions{}.withSampleRate(22050).withNumChannels(1).withBitsPerSample(16));
        if(!writer||!writer->writeFromAudioSampleBuffer(audio,0,frames))return juce::Result::fail("Cannot write guide audio.");
        writer.reset();
        if(!temporary.overwriteTargetFileWithTemporary())return juce::Result::fail("Cannot finalize guide audio.");
        reader=open(file);
        if(!file.getSiblingFile(key+".sha256").replaceWithText(juce::SHA256(file).toHexString()))return juce::Result::fail("Cannot write guide cache checksum.");
    }
    if(cancelled())return juce::Result::fail("Guide cancelled.");
    if(!valid(reader.get()))return juce::Result::fail("Guide cache is invalid.");
    take={file,22050,plan.start,p.settings.bpm,reader->lengthInSamples};if(cacheHit)*cacheHit=reused;progress(1);return juce::Result::ok();
}
}
