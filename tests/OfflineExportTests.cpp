// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/AudioEngine.h"
#include <iostream>
#include <stdexcept>
namespace {
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void write(const juce::File& file,const juce::AudioBuffer<float>& audio,int rate,int bits){
    juce::WavAudioFormat wav;auto stream=file.createOutputStream();require(stream!=nullptr,"offline fixture stream");
    stream->setPosition(0);stream->truncate();juce::StringPairArray metadata;metadata.set("ICMT","offline export QA");
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(),rate,2,bits,metadata,0));require(writer!=nullptr,"offline fixture writer");stream.release();
    require(writer->writeFromAudioSampleBuffer(audio,0,audio.getNumSamples())&&writer->flush(),"offline fixture write");
}
std::unique_ptr<juce::AudioFormatReader> open(const juce::File& file){juce::WavAudioFormat wav;return std::unique_ptr<juce::AudioFormatReader>(wav.createReaderFor(file.createInputStream().release(),true));}
struct Metrics {double lufs,crest,distortion;};
Metrics measure(juce::AudioFormatReader& reader,const std::vector<double>& frequencies){
    juce::AudioBuffer<float> audio(2,(int)reader.lengthInSamples);require(reader.read(&audio,0,audio.getNumSamples(),0,true,true),"offline metrics read");
    fga::LoudnessMeter meter;meter.prepare(reader.sampleRate);meter.process(audio.getArrayOfWritePointers(),2,audio.getNumSamples());
    const int start=(int)reader.sampleRate/2,n=(int)reader.sampleRate;
    double energy=0,peak=0;for(int s=0;s<audio.getNumSamples();++s){const double v=audio.getSample(0,s);energy+=v*v;peak=std::max(peak,std::abs(v));}
    const double rms=std::sqrt(energy/std::max(1,audio.getNumSamples()));double residual=-1;
    if(!frequencies.empty()&&start+n<=audio.getNumSamples()){
        double total=0,mean=0,fundamental=0;
        for(int s=0;s<n;++s){const double value=audio.getSample(0,start+s);total+=value*value;mean+=value;}
        for(double frequency:frequencies){double sine=0,cosine=0;for(int s=0;s<n;++s){const double phase=2*juce::MathConstants<double>::pi*frequency*s/reader.sampleRate,value=audio.getSample(0,start+s);sine+=value*std::sin(phase);cosine+=value*std::cos(phase);}fundamental+=2*(sine*sine+cosine*cosine)/(n*(double)n);}
        residual=std::sqrt(std::max(0.,total/n-mean*mean/(n*(double)n)-fundamental)/std::max(1e-30,fundamental))*100;
    }
    return {meter.integrated.load(),rms>0?20*std::log10(peak/rms):0,residual};
}
void benchmark(const juce::File& directory,int seconds){
    constexpr int rate=48000,chunk=4096;const auto source=directory.getChildFile("benchmark-float.wav"),target=directory.getChildFile("benchmark-transparent.wav");
    juce::WavAudioFormat wav;auto stream=source.createOutputStream();require(stream!=nullptr,"benchmark staging stream");stream->setPosition(0);stream->truncate();
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(),rate,2,32,{},0));require(writer!=nullptr,"benchmark staging writer");stream.release();
    juce::AudioBuffer<float> block(2,chunk);
    for(int at=0;at<seconds*rate;at+=chunk){const int n=std::min(chunk,seconds*rate-at);for(int s=0;s<n;++s)for(int ch=0;ch<2;++ch)block.setSample(ch,s,1.6f*(float)std::sin(2*juce::MathConstants<double>::pi*997*(at+s)/rate));require(writer->writeFromAudioSampleBuffer(block,0,n),"benchmark staging write");}
    require(writer->flush(),"benchmark staging flush");writer.reset();
    fga::OfflineExportStats stats;const double start=juce::Time::getMillisecondCounterHiRes();
    require(fga::finishOfflineExport(source,target,24,fga::AudioExportMode::transparent,-1,[]{return false;},[](double){},&stats).wasOk(),"benchmark streamed export");
    require(stats.frames==seconds*rate,"benchmark frame count");
    std::cout<<"BENCH seconds="<<seconds<<" process_ms="<<juce::Time::getMillisecondCounterHiRes()-start<<" float_bytes="<<source.getSize()<<" pcm_bytes="<<target.getSize()<<" peak="<<stats.outputPeak<<"\n";
}
}
void runOfflineExportTests(){using namespace fga;
    const auto requested=juce::SystemStats::getEnvironmentVariable("FGA_OFFLINE_EXPORT_AUDIT_DIR",{});
    const auto directory=requested.isEmpty()?juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fga-offline-export","",false):juce::File(requested);
    require(directory.createDirectory().wasOk(),"offline QA directory");
    struct Cleanup {juce::File directory;bool keep;~Cleanup(){if(!keep)directory.deleteRecursively();}} cleanup{directory,requested.isNotEmpty()};
    const int seconds=juce::SystemStats::getEnvironmentVariable("FGA_OFFLINE_BENCH_SECONDS","0").getIntValue();
    if(seconds>0){require(seconds<=120,"benchmark duration bound");benchmark(directory,seconds);return;}
    juce::String csv="rate,signal,mode,bits,tp16_dbfs,lufs,crest_db,distortion_pct,input_distortion_pct,gain,process_ms,stereo_error\n";
    double worstTransparent=0,worstLoudImd=0;int count=0;
    for(int rate:{44100,48000,96000})for(const auto* name:{"tone0","tone1","tone3","tone7","imd","impulse","burst","dense","complex","short","silence"}){
        const juce::String signal(name);const int frames=signal=="short"?17:2*rate;
        juce::AudioBuffer<float> source(2,frames);source.clear();std::vector<double> frequencies;
        const bool tone=signal.startsWith("tone");const double drive=signal=="tone1"?1:signal=="tone3"?3:signal=="tone7"?7.18:0;
        if(tone)frequencies={997};if(signal=="imd")frequencies={60,7000};
        for(int s=0;s<frames;++s){
            const double t=s/(double)rate,phase=2*juce::MathConstants<double>::pi*t;double value=0;
            if(tone)value=.875*std::pow(10.,drive/20)*std::sin(997*phase);
            else if(signal=="imd")value=1.6*std::sin(60*phase)+.4*std::sin(7000*phase);
            else if(signal=="impulse"||signal=="short")value=s==(signal=="short"?0:rate/2)?2:0;
            else if(signal=="burst"){const double time=t-.5;if(time>=0&&time<.1)value=2*std::sin(80*phase)*std::pow(std::sin(juce::MathConstants<double>::pi*time/.1),2);}
            else if(signal=="dense")value=.8*(std::sin(997*phase)+std::sin(rate*.381*phase));
            else if(signal=="complex")value=(.2+.8*std::pow(std::sin(phase*.3),2))*(.8*std::sin(53*phase)+.5*std::sin(211*phase)+.4*std::sin(997*phase)+.3*std::sin(3101*phase));
            source.setSample(0,s,(float)value);source.setSample(1,s,(float)(value*.37));
        }
        const auto prefix=signal+"-"+juce::String(rate);const auto input=directory.getChildFile(prefix+"-input.wav");write(input,source,rate,32);
        auto inputReader=open(input);require(inputReader&&inputReader->usesFloatingPointData,"float staging preserves headroom");const auto original=measure(*inputReader,frequencies);inputReader.reset();
        for(auto mode:{AudioExportMode::transparent,AudioExportMode::loud,AudioExportMode::legacy}){
            const auto label=mode==AudioExportMode::transparent?"transparent":mode==AudioExportMode::loud?"loud":"legacy";
            const auto output=directory.getChildFile(prefix+"-"+label+"-24.wav");OfflineExportStats stats;
            const double started=juce::Time::getMillisecondCounterHiRes();
            if(mode!=AudioExportMode::legacy){
                const auto result=finishOfflineExport(input,output,24,mode,-1,[]{return false;},[](double){},&stats);require(result.wasOk(),result.getErrorMessage().toRawUTF8());
            }else{
                ExportPeakGuard guard;guard.prepare(512);const int latency=guard.latency();juce::AudioBuffer<float> all(2,frames+latency),block(2,512);all.clear();
                for(int at=0;at<all.getNumSamples();at+=512){const int n=std::min(512,all.getNumSamples()-at);block.clear();if(at<frames)for(int ch=0;ch<2;++ch)block.copyFrom(ch,0,source,ch,at,std::min(n,frames-at));guard.process(block,n);for(int ch=0;ch<2;++ch)all.copyFrom(ch,at,block,ch,0,n);}
                juce::AudioBuffer<float> legacy(2,frames);for(int ch=0;ch<2;++ch)legacy.copyFrom(ch,0,all,ch,latency,frames);
                for(int ch=0;ch<2;++ch)for(int s=0;s<frames;++s)legacy.setSample(ch,s,std::clamp(legacy.getSample(ch,s),-.999f,.999f));
                write(output,legacy,rate,24);require(ExportPeakGuard::verifyFile(output,24,[]{return false;}).wasOk(),"legacy fixture verification");
            }
            const double processMs=juce::Time::getMillisecondCounterHiRes()-started;
            auto reader=open(output);require(reader&&reader->lengthInSamples==frames&&reader->sampleRate==rate&&reader->bitsPerSample==24&&reader->metadataValues.getValue("ICMT",{})=="offline export QA","offline encoding metadata/duration");
            const auto metrics=measure(*reader,frequencies);
            if(mode==AudioExportMode::loud&&signal=="imd")worstLoudImd=std::max(worstLoudImd,metrics.distortion);
            juce::AudioBuffer<float> samples(2,frames);require(reader->read(&samples,0,frames,0,true,true),"stereo fixture read");
            double stereo=0;for(int s=0;s<frames;++s)stereo=std::max(stereo,std::abs(samples.getSample(1,s)-.37*samples.getSample(0,s)));
            // Legacy's independent PCM clamp can break this ratio on dense overloads;
            // retain it as a measured comparator, never as the new mode's acceptance target.
            require(mode==AudioExportMode::legacy||stereo<3e-7,"new offline modes must retain the linked stereo image");
            if(mode==AudioExportMode::transparent){
                juce::AudioBuffer<float> result(2,frames);reader->read(&result,0,frames,0,true,true);
                double error=0,stereo=0;for(int s=0;s<frames;++s){error=std::max(error,std::abs(result.getSample(0,s)-source.getSample(0,s)*stats.gain));stereo=std::max(stereo,std::abs(result.getSample(1,s)-.37*result.getSample(0,s)));}
                require(error<2e-7&&stereo<3e-7,"Transparent must preserve the entire waveform and stereo ratio with one gain");
                if(tone||signal=="imd"){worstTransparent=std::max(worstTransparent,metrics.distortion);require(metrics.distortion<.01,"Transparent added harmonic/intermodulation distortion");}
            }
            csv+=juce::String(rate)+","+signal+","+label+",24,"+(mode==AudioExportMode::legacy?juce::String():juce::String(stats.outputPeak>0?20*std::log10(stats.outputPeak):-100,8))+","+juce::String(metrics.lufs,8)+","+juce::String(metrics.crest,8)+","+juce::String(metrics.distortion,8)+","+juce::String(original.distortion,8)+","+juce::String(stats.gain,10)+","+juce::String(processMs,3)+","+juce::String(stereo,10)+"\n";++count;
        }
        if(signal=="tone7"){
            const auto output=directory.getChildFile(prefix+"-transparent-16.wav");require(finishOfflineExport(input,output,16,AudioExportMode::transparent,-3,[]{return false;},[](double){}).wasOk(),"16-bit configurable ceiling export");
            const auto cancelled=directory.getChildFile("cancelled-staging.wav");
            for(double stop:{.1,.6,.95}){bool cancel=false;const auto result=finishOfflineExport(input,cancelled,24,AudioExportMode::transparent,-1,[&]{return cancel;},[&](double p){if(p>=stop)cancel=true;});require(cancel&&result.failed(),"cancel offline analysis/write/verification");cancelled.deleteFile();}
        }
    }
    require(directory.getChildFile("metrics.csv").replaceWithText(csv),"offline metrics output");
    // Admission target fixed before candidate evaluation: <=0.1% added two-tone residual.
    require(!offlineLoudReady||worstLoudImd<=.1,"Loud cannot be admitted above the IMD quality gate");
    std::cout<<"Loud quality gate: "<<(worstLoudImd<=.1?"numeric PASS, listening pending":"FAIL, not exposed")<<"; IMD="<<worstLoudImd<<"%, target <=0.1%\n";
    {
        AudioEngine engine(AudioEngine::Offline{});auto project=newProject(0);project.sections={{Section::main,1,"Export QA"}};project.settings.bpm=120;project.settings.freeTempo=true;project.normalise();
        project.masterGain=1;project.masterAutomation.enabled=false;
        for(auto& track:project.tracks){track.gain=0;track.volume.enabled=false;track.engine.reverb=track.engine.chorus=0;for(auto& clip:track.clips)clip.notes.clear();}
        auto& track=project.tracks[0];track.gain=2;track.instrument="sample";track.sampleRoot=60;track.sampler.mode=1;track.sampler.attack=track.sampler.decay=0;track.sampler.sustain=1;track.sampler.velocity=0;
        const auto input=directory.getChildFile("tone7-48000-input.wav");track.instrumentPath=input.getFullPathName().toStdString();track.clips[0].notes={{0,4,60,127,0}};
        require(engine.loadSample(0,input,60,track.sampler).wasOk(),"integration load float headroom sample");engine.publish(project);
        const auto target=directory.getChildFile("engine-transparent.wav");double rawPeak=0;
        require(engine.exportAudio(project,target,0,false,[]{return false;},[](double){},&rawPeak,{48000,24,AudioExportMode::transparent}).wasOk(),"Transparent full engine export");
        auto reader=open(target);require(reader&&rawPeak>1,"render staging must preserve values above full scale");
        const auto metrics=measure(*reader,{997});require(metrics.distortion<.01,"full engine Transparent must not clip before quantization");reader.reset();
        require(engine.exportAudio(project,target,0,false,[]{return false;},[](double){},nullptr,{48000,24,AudioExportMode::transparent,-3}).wasOk(),"successful atomic overwrite with another ceiling");
        reader=open(target);require(reader&&reader->bitsPerSample==24,"overwritten WAV remains valid");reader.reset();
        for(bool mp3:{false,true})for(double stop:{.3,.6,.98,1.}){
            const auto file=directory.getChildFile(mp3?"cancel-modern.mp3":"cancel-modern.wav");require(file.replaceWithText("previous export"),"modern overwrite fixture");bool cancel=false;
            const int before=directory.findChildFiles(juce::File::findFiles,false).size();
            const auto result=engine.exportAudio(project,file,0,mp3,[&]{return cancel;},[&](double p){if(p>=stop)cancel=true;},nullptr,{48000,24,AudioExportMode::transparent});
            require(cancel&&result.failed()&&!engine.isExporting()&&file.loadFileAsString()=="previous export","modern cancellation must preserve target and release engine");
            require(before==directory.findChildFiles(juce::File::findFiles,false).size(),"cancelled float/PCM staging must be removed");file.deleteFile();
        }
        const auto blocked=directory.getChildFile("blocked-output");blocked.createDirectory();
        require(engine.exportAudio(project,blocked,0,false,[]{return false;},[](double){}).failed()&&!engine.isExporting(),"failed output releases export ownership");blocked.deleteRecursively();
        for(double ceiling:{0.,-13.,std::numeric_limits<double>::quiet_NaN()})require(engine.exportAudio(project,target,0,false,[]{return false;},[](double){},nullptr,{48000,24,AudioExportMode::transparent,ceiling}).failed(),"invalid ceiling must be rejected");
        if(!offlineLoudReady)require(engine.exportAudio(project,target,0,false,[]{return false;},[](double){},nullptr,{48000,24,AudioExportMode::loud}).failed(),"unvalidated Loud must not be available through the product API");
        engine.play(true);juce::AudioBuffer<float> audio(2,512);juce::AudioIODeviceCallbackContext context;engine.audioDeviceIOCallbackWithContext(nullptr,0,audio.getArrayOfWritePointers(),2,512,context);
        require(engine.isPlaying()&&engine.position()>0,"playback after successful/cancelled/failed offline export");engine.stop();
    }
    std::cout<<"PASS offline modes "<<count<<" cases; maximum Transparent THD+N/IMD="<<worstTransparent<<"%; engine export/cancel/overwrite/error/restart\n";
}
