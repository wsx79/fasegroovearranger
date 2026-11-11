// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/AudioEngine.h"
#include "../Source/SamplePreviewAnalysis.h"
#include <stdexcept>
#include <limits>
#include <iostream>
#include <complex>
namespace {void require(bool b,const char* s){if(!b)throw std::runtime_error(s);}}
namespace {
void meterReferenceChecks(){
    // ITU-R BS.1770-5, Annex 1, Tables 1/2: independent fixed 48 kHz reference.
    const std::array<double,5> shelf{1.53512485958697,-2.69169618940638,1.19839281085285,-1.69065929318241,.73248077421585};
    const std::array<double,5> highPass{1,-2,1,-1.99004745483398,.99007225036621};
    for(int rate:{44100,48000,96000})for(double frequency:{100.,997.,1682.,10000.}){
        constexpr int blockSize=257;fga::LoudnessMeter meter;meter.prepare(rate);
        juce::AudioBuffer<float> block(2,blockSize);
        for(int at=0;at<rate*4;at+=blockSize){
            const int n=std::min(blockSize,rate*4-at);
            for(int s=0;s<n;++s)for(int ch=0;ch<2;++ch)
                block.setSample(ch,s,.1f*(float)std::sin(2*juce::MathConstants<double>::pi*frequency*(at+s)/rate));
            meter.process(block.getArrayOfWritePointers(),2,n);
        }
        const auto z=std::polar(1.,-2*juce::MathConstants<double>::pi*frequency/48000.);
        auto response=[&](const auto& c){return (c[0]+c[1]*z+c[2]*z*z)/(1.+c[3]*z+c[4]*z*z);};
        const double expected=-20-.691+20*std::log10(std::abs(response(shelf)*response(highPass)));
        std::cout<<"Meter reference "<<rate<<" Hz / "<<frequency<<" Hz: expected_48k="<<expected<<", short="<<meter.shortTerm.load()<<", integrated="<<meter.integrated.load()<<" LUFS\n";
        require(std::abs(meter.shortTerm.load()-expected)<(rate==48000?.01:.1)&&std::abs(meter.integrated.load()-expected)<(rate==48000?.025:.1),
                "BS.1770 K weighting differs from the reference frequency response");
        if(frequency==997){
            const float steady=meter.integrated.load();
            for(int at=0;at<rate*4;at+=blockSize){
                const int n=std::min(blockSize,rate*4-at);
                for(int s=0;s<n;++s)for(int ch=0;ch<2;++ch)block.setSample(ch,s,.003f*(float)std::sin(2*juce::MathConstants<double>::pi*997*(at+s)/rate));
                meter.process(block.getArrayOfWritePointers(),2,n);
            }
            require(std::abs(meter.integrated.load()-steady)<.25&&std::abs(meter.shortTerm.load()-(steady+20*std::log10(.03)))<.01,
                    "relative gating must exclude the quiet passage while short-term follows its last three seconds");
            meter.reset();block.clear();meter.process(block.getArrayOfWritePointers(),2,17);
            require(meter.integrated==-100&&meter.shortTerm==-100,"silence/short audio must not invent a gated loudness");
            for(int at=0;at<rate*4;at+=blockSize){
                const int n=std::min(blockSize,rate*4-at);
                for(int s=0;s<n;++s)block.setSample(0,s,.1f*(float)std::sin(2*juce::MathConstants<double>::pi*997*(at+s)/rate));
                meter.process(block.getArrayOfWritePointers(),1,n);
            }
            require(std::abs(meter.integrated.load()-(steady-3.0103))<.025,"mono loudness must be 3.01 LU below equal stereo channels");
        }
    }
    std::cout<<"PASS BS.1770 reference response, mono/stereo, short silence and relative/short-term gates\n";
}
void peakGuardChecks(const juce::File& directory){
    const auto archive=juce::SystemStats::getEnvironmentVariable("FGA_EXPORT_AUDIT_DIR",{});
    if(archive.isNotEmpty())require(juce::File::isAbsolutePath(archive)&&juce::File(archive).createDirectory().wasOk(),"create optional export audit directory");
    for(int rate:{44100,48000,96000})for(const auto* signal:{"silence","short","quiet","hot","impulse","dense"}){
        const bool shortAudio=juce::String(signal)=="short",quiet=juce::String(signal)=="quiet",hot=juce::String(signal)=="hot",impulse=juce::String(signal)=="impulse",dense=juce::String(signal)=="dense";
        const int frames=shortAudio?17:rate*2;
        fga::ExportPeakGuard guard;guard.prepare(512);
        juce::AudioBuffer<float> processed(2,frames+512),block(2,512);processed.clear();
        for(int at=0;at<processed.getNumSamples();at+=257){
            const int n=std::min(257,processed.getNumSamples()-at);block.clear();
            for(int s=0;s<n;++s){
                const int t=at+s;float value=0;
                if(t<frames){
                    const auto phase=2*juce::MathConstants<double>::pi*t/rate;
                    if(quiet||hot)value=(quiet?.25f:2.f)*(float)std::sin(997*phase);
                    else if(impulse)value=t==rate/2?2.f:0.f;
                    else if(shortAudio)value=t==0?.9f:0.f;
                    else if(dense)value=.8f*(float)(std::sin(997*phase)+std::sin(rate*.381*phase));
                }
                for(int ch=0;ch<2;++ch)block.setSample(ch,s,value);
            }
            guard.process(block,n);
            for(int ch=0;ch<2;++ch)processed.copyFrom(ch,at,block,ch,0,n);
        }
        juce::AudioBuffer<float> audio(2,frames);
        for(int ch=0;ch<2;++ch)audio.copyFrom(ch,0,processed,ch,guard.latency(),frames);
        if(quiet||hot){
            double energy=0,sine=0,cosine=0;
            for(int s=rate/2;s<rate/2+rate;++s){
                const double value=audio.getSample(0,s),phase=2*juce::MathConstants<double>::pi*997*s/rate;
                energy+=value*value;sine+=value*std::sin(phase);cosine+=value*std::cos(phase);
            }
            const double fundamental=2*std::hypot(sine,cosine)/rate;
            const double residual=std::max(0.,energy/rate-fundamental*fundamental*.5);
            const double thdn=std::sqrt(residual/(fundamental*fundamental*.5));
            std::cout<<"Guard "<<signal<<" "<<rate<<" Hz: fundamental="<<fundamental<<", THD+N="<<100*thdn<<"%\n";
            require(std::isfinite(thdn)&&(!quiet||(thdn<.001&&std::abs(fundamental-.25)<.0001)),"guard must preserve an unclipped reference tone");
        }
        if(impulse){
            int peakAt=0;double energy=0,cumulative=0;int first=-1,last=-1;
            for(int s=0;s<frames;++s){const double value=audio.getSample(0,s);energy+=value*value;if(std::abs(value)>std::abs(audio.getSample(0,peakAt)))peakAt=s;}
            for(int s=0;s<frames;++s){const double value=audio.getSample(0,s);cumulative+=value*value;if(first<0&&cumulative>=energy*.05)first=s;if(last<0&&cumulative>=energy*.95)last=s;}
            std::cout<<"Guard impulse "<<rate<<" Hz: peak="<<audio.getMagnitude(0,frames)<<", shift="<<peakAt-rate/2<<" frames, 90% energy width="<<last-first+1<<" frames\n";
            require(std::abs(peakAt-rate/2)<=1,"guard delay compensation must align transients within one sample");
        }
        for(int bits:{16,24}){
            const auto file=directory.getChildFile(juce::String(signal)+"-"+juce::String(rate)+"-"+juce::String(bits)+".wav");
            juce::WavAudioFormat wav;juce::StringPairArray metadata;metadata.set("ICMT","phase04 reference");
            {auto stream=file.createOutputStream();std::unique_ptr<juce::AudioFormatWriter> writer(stream?wav.createWriterFor(stream.get(),rate,2,bits,metadata,0):nullptr);require(writer!=nullptr,"create guard reference WAV");stream.release();require(writer->writeFromAudioSampleBuffer(audio,0,frames),"write guard reference WAV");}
            require(fga::ExportPeakGuard::verifyFile(file,bits,[]{return false;}).wasOk(),"verify guard reference WAV");
            std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(file.createInputStream().release(),true));
            require(reader&&reader->sampleRate==rate&&reader->bitsPerSample==(unsigned)bits&&reader->lengthInSamples==frames&&reader->metadataValues.getValue("ICMT",{})=="phase04 reference","guard verification must preserve encoding, length and metadata");
            juce::dsp::Oversampling<float> reference(2,4,juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,true);reference.initProcessing(512);float peak=0;
            for(int at=0;at<frames+512;at+=512){block.clear();if(at<frames)require(reader->read(&block,0,512,at,true,true),"read guard reference WAV");const auto high=reference.processSamplesUp(juce::dsp::AudioBlock<float>(block));for(size_t ch=0;ch<2;++ch)for(size_t s=0;s<high.getNumSamples();++s)peak=std::max(peak,std::abs(high.getChannelPointer(ch)[s]));}
            if(peak>.8913f)std::cout<<"True peak regression "<<file.getFileName()<<": 16x peak="<<peak<<" ("<<juce::Decibels::gainToDecibels(peak)<<" dBTP)\n";
            require(peak<=.8913f,"guarded PCM exceeds -1 dBTP with the higher resolution 16x estimator");reader.reset();
            if(archive.isNotEmpty())require(file.copyFileTo(juce::File(archive).getChildFile(file.getFileName())),"archive optional guard reference WAV");
        }
    }
    std::cout<<"PASS guard PCM: silence/17-frame audio/quiet/hot/impulse/dense, 44.1/48/96 kHz, 16/24 bit\n";
}
}
void runImprovementEngineTests(){using namespace fga;
    meterReferenceChecks();
    {fga::Instrument delayed(0);delayed.delay=6;delayed.delayLine.setSize(2,8);juce::AudioBuffer<float> block(2,4);block.clear();block.setSample(0,0,1);delayed.applyDelay(block,4);require(block.getMagnitude(0,4)==0,"PDC must wait for delayed samples");delayed.flushDelay();block.clear();delayed.applyDelay(block,4);require(block.getMagnitude(0,4)==0,"PDC flush replays old audio");block.clear();delayed.applyDelay(block,4);require(block.getMagnitude(0,4)==0,"PDC stale sample appears after flush");}
    {fga::LoudnessMeter meter;meter.prepare(16000);juce::AudioBuffer<float> bad(2,1600);for(int c=0;c<2;++c)for(int n=0;n<1600;++n)bad.setSample(c,n,n%2?std::numeric_limits<float>::infinity():std::numeric_limits<float>::quiet_NaN());for(int i=0;i<8;++i)meter.process(bad.getArrayOfWritePointers(),2,1600);require(std::isfinite(meter.shortTerm.load())&&std::isfinite(meter.integrated.load()),"nonfinite audio corrupts LUFS or integer bins");}
    auto asset=std::make_shared<SampleAsset>();asset->rate=16000;asset->frames=16000;asset->audio.setSize(2,16004);asset->audio.clear();for(int ch=0;ch<2;++ch)for(int n=0;n<16000;++n)asset->audio.setSample(ch,n,.5f);
    SamplerSettings settings;settings.mode=1;settings.attack=0;settings.decay=0;settings.sustain=1;settings.velocity=0;settings.fadeIn=settings.fadeOut=0;SampleSynth synth(asset,settings,60,false);synth.setCurrentPlaybackSampleRate(0);synth.setCurrentPlaybackSampleRate(16000);juce::AudioBuffer<float> audio(2,16000);audio.clear();juce::MidiBuffer midi;SampleSynth::addRegionEvent(midi,0,60,127,.1,.5,.02,.03);synth.renderNextBlock(audio,midi,0,16000);require(std::abs(audio.getSample(0,0))<1e-6&&audio.getSample(0,80)<audio.getSample(0,400),"region fade-in absent");require(audio.getSample(0,7980)<audio.getSample(0,7400)&&audio.getMagnitude(8000,8000)==0,"region fade-out/end absent");synth.renderNextBlock(audio,midi,16000,500); // external range clamped before touching AudioBuffer.
    MixerBuses buses;buses.prepare(16000,512);auto mix=defaultBuses();mix[0].sidechainAmount=1;buses.configure(mix,120,1,true);juce::AudioBuffer<float> output(2,512);output.clear();buses.begin(512);for(int s=0;s<512;++s)for(int ch=0;ch<2;++ch)buses.add(ch,s,.5f,0,{});buses.finish(output.getArrayOfWritePointers(),2,512,nullptr);const float unducked=output.getSample(0,0);output.clear();buses.begin(512);for(int s=0;s<512;++s)for(int ch=0;ch<2;++ch)buses.add(ch,s,.5f,0,{});buses.kick(0,.7f);buses.finish(output.getArrayOfWritePointers(),2,512,nullptr);require(unducked>.4&&output.getSample(0,0)<.01,"kick sidechain did not duck group");
    ChannelStrip strip;EqSettings eq;juce::AudioBuffer<float> tone(2,16000);for(int ch=0;ch<2;++ch)for(int n=0;n<16000;++n)tone.setSample(ch,n,.2f*std::sin(n*juce::MathConstants<double>::twoPi*50/16000));const auto original=tone;strip.configure(eq,16000);strip.process(tone,16000);require(tone.getSample(0,100)==original.getSample(0,100),"bypassed EQ is not transparent");eq.enabled=true;eq.highPass=300;strip.configure(eq,16000);strip.process(tone,16000);require(tone.getRMSLevel(0,8000,8000)<original.getRMSLevel(0,8000,8000)*.1,"high-pass does not reject bass");
    LoudnessMeter meter;meter.prepare(48000);juce::AudioBuffer<float> sine(2,480);for(int ch=0;ch<2;++ch)for(int n=0;n<480;++n)sine.setSample(ch,n,.1f*std::sin(n*juce::MathConstants<double>::twoPi*1000/48000));for(int b=0;b<400;++b)meter.process(sine.getArrayOfWritePointers(),2,480);const float measured=meter.integrated.load();require(measured> -21&&measured< -19,"LUFS K-weighted reference outside tolerance");sine.clear();for(int b=0;b<400;++b)meter.process(sine.getArrayOfWritePointers(),2,480);require(std::abs(meter.integrated.load()-measured)<.6,"LUFS silence gate failed");
    juce::AudioBuffer<float> click(1,16000*4);click.clear();for(int n=0;n<click.getNumSamples();n+=8000)for(int i=0;i<200;++i)click.setSample(0,n+i,.8f*(1-i/200.f));auto estimate=analysePreview(click,16000);require(std::abs(estimate.bpm-120)<3,"preview BPM estimation failed on 120 BPM transients");
    const auto directory=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fga-improvement-audio","",false);directory.createDirectory();struct Cleanup{juce::File file;~Cleanup(){file.deleteRecursively();}}cleanup{directory};peakGuardChecks(directory);const auto source=directory.getChildFile("source.wav"),guarded=directory.getChildFile("guarded.wav");juce::WavAudioFormat wav;{auto stream=source.createOutputStream();std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(),16000,2,24,{},0));require(writer!=nullptr,"create guard fixture");stream.release();for(int ch=0;ch<2;++ch)for(int n=0;n<16000;++n)audio.setSample(ch,n,.99f*std::sin(n*juce::MathConstants<double>::twoPi*6100/16000+.7));require(writer->writeFromAudioSampleBuffer(audio,0,16000),"write guard fixture");}
    AudioEngine engine(AudioEngine::Offline{});auto project=newProject(0);project.sections={{Section::main,1,"Test"}};project.normalise();project.masterGain=1;project.masterAutomation={};project.masterEffects.effects.clear();for(auto& track:project.tracks){track.volume={};for(auto& clip:track.clips)clip.notes.clear();}auto& track=project.tracks[0];track.instrument="sample";track.instrumentPath=source.getFullPathName().toStdString();track.sampleRoot=60;track.sampler=settings;track.gain=2;track.clips[0].notes={{0,2,60,127,0}};require(engine.loadSample(0,source,60,settings).wasOk(),"load guard fixture");require(engine.exportAudio(project,guarded,0,false,[]{return false;},[](double){},nullptr,{16000,24,true}).wasOk(),"guarded export failed");std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(guarded.createInputStream().release(),true));require(reader!=nullptr,"read guarded export");juce::dsp::Oversampling<float> oversampling(2,2,juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,true);oversampling.initProcessing(512);juce::AudioBuffer<float> block(2,512);float truePeak=0;for(juce::int64 at=0;at<reader->lengthInSamples+512;at+=512){block.clear();if(at<reader->lengthInSamples)reader->read(&block,0,512,at,true,true);const auto up=oversampling.processSamplesUp(juce::dsp::AudioBlock<float>(block));for(size_t ch=0;ch<2;++ch)for(size_t n=0;n<up.getNumSamples();++n)truePeak=std::max(truePeak,std::abs(up.getChannelPointer(ch)[n]));}require(truePeak<=.8913f,"guarded WAV exceeds -1 dBTP at 4x");reader.reset();track.gain=1;track.clips[0].expression[2].enabled=true;track.clips[0].expression[2].points={{0,.5,0},{1,1,0}};engine.publish(project);engine.seek(.5);engine.play(true);juce::AudioIODeviceCallbackContext expressionContext;engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,expressionContext);track.clips[0].expression[2]={};engine.publish(project);engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,expressionContext);engine.stop();engine.publish(project);engine.freezeTrack(0,guarded,project.settings.bpm,track.sessionId);require(engine.trackFrozen(0),"freeze cache not adopted");engine.play(true);juce::AudioIODeviceCallbackContext context;engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);require(block.getMagnitude(0,512)>.01,"frozen cache does not play");engine.stop();engine.audioDeviceIOCallbackWithContext(nullptr,0,block.getArrayOfWritePointers(),2,512,context);require(block.getMagnitude(0,512)<1e-6,"frozen cache sounds while stopped");engine.unfreezeTrack(0);require(!engine.trackFrozen(0),"unfreeze did not restore live path");
    std::cout<<"PASS audio fades/bounds, kick detector, transparent/native EQ, K-weighted/gated LUFS, 4x true-peak ceiling, freeze/play/stop/unfreeze, preview tempo\n";
}
