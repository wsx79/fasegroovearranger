// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/MixAnalysis.h"
#include "../Source/AudioTake.h"
#include "../Source/ProjectIO.h"
#include <juce_cryptography/juce_cryptography.h>
#include "Localisation.h"
#include "../Source/ProductSettings.h"
#include <stdexcept>
#include <iostream>
static void requireMix(bool b,const char* s){if(!b)throw std::runtime_error(s);}
void runMixAnalysisTests(){
 FaseGroove::useSelfTestPreferences();
 using namespace fga;
 const auto started=juce::Time::getMillisecondCounterHiRes();
 const auto folder=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fga-mix-analysis","",false);
 requireMix(folder.createDirectory().wasOk(),"fixture directory");
 struct Clean {juce::File f;~Clean(){f.deleteRecursively();}}clean{folder};
 auto write=[&](const char* name,int mode,double rate=48000,int blocks=32,int channels=2){
  auto file=folder.getChildFile(name);juce::WavAudioFormat wav;
  std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(file.createOutputStream().release(),rate,(unsigned)channels,32,{},0));
  requireMix(writer!=nullptr,"fixture writer");juce::AudioBuffer<float> b(2,4096);
  for(int block=0;block<blocks;++block){
   for(int n=0;n<4096;++n){const auto i=block*4096+n;const auto phase=i*2*juce::MathConstants<double>::pi/rate;
    float v=mode==0?0:(float)(.8*std::sin(phase*(mode==4?1000:60)));
    // Synthesized voiced harmonics + syllabic envelope, not a human/singing quality reference.
    if(mode==7)v=(float)(.2*(std::sin(phase*220)+.5*std::sin(phase*440)+.25*std::sin(phase*880))*(.5+.5*std::sin(phase*3)));
    if(mode==3)v=std::clamp(2*v,-1.f,1.f);
    if(mode==5&&block>=16)v=0;if(mode==6&&block<16)v=0;
    b.setSample(0,n,v);b.setSample(1,n,mode==2?-v:v);
   }
   requireMix(writer->writeFromAudioSampleBuffer(b,0,4096),"write fixture");
  }
  writer.reset();return file;
 };
 auto silence=write("silence.wav",0),tone=write("tone.wav",1),anti=write("anti.wav",2),full=write("full-scale.wav",3);
 auto high=write("high-band.wav",4),voice=write("rendered-synthetic-vocal.wav",7),first=write("first.wav",5),second=write("second.wav",6);
 MixReport r;auto run=[&](const std::vector<MixInput>& inputs){return analyseMixFiles(inputs,r,[]{return false;});};
 requireMix(run({{silence}}).wasOk()&&!r.mix.correlationValid&&r.mix.peak==0&&r.suggestions.empty(),"silence undefined correlation, no spurious suggestion");
 requireMix(run({{tone}}).wasOk()&&std::abs(r.mix.correlation-1)<1e-9&&std::abs(r.mix.peak-.8)<1e-5&&!r.suggestions.empty(),"headroom and correlated stereo");
 requireMix(std::abs(r.mix.rms-.8/std::sqrt(2.))<.001&&r.mix.lufs>-40&&r.mix.lufs<0,"RMS/loudness measured, not fixed target");
 requireMix(std::abs(r.suggestions[0].trimDb-(-3.-juce::Decibels::gainToDecibels(r.mix.peak)))<1e-9,"headroom trim correct");
 const auto reference=r;requireMix(run({{tone}}).wasOk()&&r.mix.peak==reference.mix.peak&&r.mix.lufs==reference.mix.lufs&&r.suggestions[0].trimDb==reference.suggestions[0].trimDb,"deterministic repeated analysis");
 {
  const auto language=Localisation::current();Localisation::setCurrent(Localisation::Language::italian);
  requireMix(run({{tone}}).wasOk()&&r.suggestions[0].text.contains("Riduzione del master"),"Italian analysis advice");
  requireMix(r.mix.peak==reference.mix.peak&&r.mix.rms==reference.mix.rms&&r.mix.lufs==reference.mix.lufs&&r.suggestions[0].trimDb==reference.suggestions[0].trimDb,"language preserves numerical analysis exactly");
  const auto missing=folder.getChildFile("Choose mix WAV first..wav");const auto error=run({{missing}});
  requireMix(error.failed()&&error.getErrorMessage().startsWith("Impossibile aprire il WAV: ")&&error.getErrorMessage().endsWith(missing.getFileName()),"Italian errors preserve unmodified user filenames");
  requireMix(run({{write("invalid-rate.wav",1,4000)}}).getErrorMessage().startsWith("La frequenza del WAV"),"Unicode English key uses UTF-8 for Italian error lookup");
  Localisation::setCurrent(Localisation::Language::english);requireMix(run({{tone}}).wasOk()&&r.suggestions[0].text.startsWith("Suggested master trim"),"English advice restored");Localisation::setCurrent(language);
 }
 requireMix(run({{anti}}).wasOk()&&std::abs(r.mix.correlation+1)<1e-9,"antiphase");
 requireMix(run({{full}}).wasOk()&&r.mix.fullScale>0,"full-scale PCM risk detected without inferring prior clipping");
 Project project;project.normalise();AudioTake take;take.file=voice;take.rate=48000;take.frames=32*4096;
 const int vocal=addAudioTake(project,take,false);requireMix(vocal>=0,"existing audio take for rendered synthetic vocal");
 const auto state=projectJson(project);Project reopened;requireMix(projectFromJson(state,reopened).wasOk(),"project reopen");
 const auto vocalSession=project.tracks[(size_t)vocal].sessionId;const auto kickSession=project.tracks[4].sessionId,bassSession=project.tracks[1].sessionId;
 const auto voiceHash=juce::SHA256(voice).toHexString();
 requireMix(run({{tone},{tone,kickSession,1},{tone,bassSession,2},{voice,vocalSession,3}}).wasOk(),"project fixture with rendered synthetic vocal stem");
 bool conflict=false;for(auto& s:r.suggestions)if(s.text.contains("kick/bass"))conflict=s.session==bassSession;
 requireMix(conflict,"kick bass overlap targets bass");
 requireMix(voiceHash==juce::SHA256(voice).toHexString()&&juce::JSON::toString(state)==juce::JSON::toString(projectJson(project)),"audio file and project preserved");
 requireMix(reopened.tracks[(size_t)vocal].instrumentPath==voice.getFullPathName().toStdString(),"rendered audio reference persists");
 requireMix(run({{tone},{first,11,1},{second,22,2}}).wasOk(),"disjoint stems");
 for(auto& s:r.suggestions)requireMix(!s.text.contains("kick/bass"),"no temporal overlap no conflict");
 requireMix(run({{tone},{tone,11,3},{high,22,3}}).wasOk(),"separated bands");
 for(auto& s:r.suggestions)requireMix(!s.text.contains("masking"),"different bands no masking claim");
 requireMix(run({{tone},{tone,11,3},{tone,22,3}}).wasOk(),"similar bands");bool masking=false;for(auto& s:r.suggestions)masking=masking||s.text.contains("masking");requireMix(masking,"coactive spectral masking suggestion");
 requireMix(run({{tone},{write("wrong-rate.wav",1,44100),22,2}}).failed(),"mismatched rates rejected");
 requireMix(run({{tone},{write("long-stem.wav",1,48000,33),22,2}}).failed(),"longer stem rejected");
 requireMix(run({{tone},{tone,22,2},{tone,22,3}}).failed()&&run({{tone},{tone,0,3}}).failed(),"duplicate/zero stem binding rejected");
 requireMix(run({{folder.getChildFile("absent.wav")}}).failed(),"missing file error");
 int calls=0;requireMix(analyseMixFiles({{tone}},r,[&]{return ++calls>2;}).failed(),"cancellation");requireMix(run({{tone}}).wasOk(),"retry");
 const auto changedAt=tone.getLastModificationTime();bool touched=false;
 requireMix(analyseMixFiles({{tone}},r,[]{return false;},[&](double){if(!touched){tone.setLastModificationTime(changedAt+juce::RelativeTime::seconds(2));touched=true;}}).failed(),"changed input rejected");tone.setLastModificationTime(changedAt);
 requireMix(run({{write("mono.wav",1,48000,32,1)}}).wasOk()&&!r.mix.correlationValid&&std::abs(r.mix.lufs-reference.mix.lufs+3.0103)<.01,"mono LUFS uses one channel, not duplicated stereo energy");
 for(double rate:{44100.,96000.})requireMix(run({{write("rate.wav",1,rate)}}).wasOk()&&std::abs(r.mix.peak-.8)<1e-5,"44.1/96 kHz");
 auto invalid=folder.getChildFile("nonfinite.wav");
 {auto stream=invalid.createOutputStream();requireMix(stream!=nullptr,"float fixture stream");stream->write("RIFF",4);stream->writeInt(44);stream->write("WAVEfmt ",8);stream->writeInt(16);stream->writeShort(3);stream->writeShort(2);stream->writeInt(48000);stream->writeInt(384000);stream->writeShort(8);stream->writeShort(32);stream->write("data",4);stream->writeInt(8);stream->writeInt(0x7fc00000);stream->writeInt(0);}
 requireMix(run({{invalid}}).failed(),"nonfinite float WAV rejected");
 std::vector<MixInput> tooMany(34);requireMix(run(tooMany).failed(),"bounded inputs");
 const auto external=juce::SystemStats::getEnvironmentVariable("FGA_MIX_VOCAL_FIXTURE",{});
 if(external.isNotEmpty()){
  const juce::File file(external);juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(file.createInputStream().release(),true));requireMix(reader!=nullptr,"external vocal reader");
  AudioTake actual;actual.file=file;actual.rate=reader->sampleRate;actual.frames=reader->lengthInSamples;reader.reset();
  const int actualTrack=addAudioTake(project,actual,false);requireMix(actualTrack>=0,"previously rendered guide in fixture project");
  const auto actualState=projectJson(project);Project loaded;requireMix(projectFromJson(actualState,loaded).wasOk(),"external guide project reopen");
  requireMix(run({{file},{file,project.tracks[(size_t)actualTrack].sessionId,3}}).wasOk(),"external rendered vocal mix/stem analysis");
  requireMix(loaded.tracks[(size_t)actualTrack].instrumentPath==external.toStdString()&&juce::JSON::toString(actualState)==juce::JSON::toString(projectJson(project)),"actual guide analysis preserves reopened project");
  std::cout<<"PASS external rendered guide project: "<<external<<"\n";
 }
 else std::cout<<"NOT RUN optional external human/guide vocal reference (synthetic project fixture tested)\n";
 std::cout<<"PASS local WAV analysis: peak/RMS/LUFS/correlation/full-scale, temporal and spectral overlap, project/vocal persistence, deterministic/no file writes, validation, cancel/retry; fixture suite "<<juce::Time::getMillisecondCounterHiRes()-started<<" ms\n";
}
