// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AudioEngine.h"
#include "core/Mp3Encoder.h"
#include <cmath>
namespace fga {
Instrument::Instrument(int track):kind(track){
    for(int i=0;i<fgd::numLanes;++i)drumNotes[(size_t)i]=fgd::defaultNote(i);
    drumLevels.fill(1);guitarLevels.fill(1);guitarEvents.reserve(8192);midi.ensureSize(262144);
    if(kind<3){for(int i=0;i<48;++i)synth.addVoice(new GuideVoice(tone,sound));if(kind==2)synth.addSound(new GuideStringsTag(1));else synth.addSound(new GuideSoundTag);}
}
void Instrument::prepare(double rate,int block){
    synth.setCurrentPlaybackSampleRate(rate);if(kind>=4)drums.prepare(rate);if(kind==3)guitar.prepare(rate);
    int channels=2;if(plugin){plugin->releaseResources();plugin->setRateAndBufferSizeDetails(rate,block);plugin->prepareToPlay(rate,block);channels=std::max({2,plugin->getTotalNumOutputChannels(),plugin->getTotalNumInputChannels()});}
    volumeGains.resize((size_t)block);scratch.setSize(channels,block);reverb.setSampleRate(rate);chorus.prepare({rate,(juce::uint32)block,2});reset();
}
void Instrument::reset(){reverb.reset();chorus.reset();synth.allNotesOff(0,false);drums.allNotesOff();guitar.allNotesOff();if(plugin)plugin->reset();midi.clear();guitarEvents.clear();}
AudioEngine::AudioEngine(){
    formats.addDefaultFormats();for(int i=0;i<trackCount;++i){instruments[(size_t)i]=std::make_unique<Instrument>(i);instruments[(size_t)i]->prepare(sampleRate,blockSize);meters[(size_t)i]=0;}
    devices.initialise(0,2,nullptr,true);devices.addAudioCallback(this);
}
AudioEngine::~AudioEngine(){lifetime.reset();devices.removeAudioCallback(this);const juce::ScopedLock lock(mutex);for(auto& i:instruments)if(i->plugin)i->plugin->releaseResources();}
void AudioEngine::publish(const Project& source){
    auto canonical=source;canonical.normalise();const auto& p=canonical;
    std::array<std::vector<TimedEvent>,trackCount> schedules;
    for(auto e:events(canonical))schedules[(size_t)e.track].push_back(e);
    const juce::ScopedLock lock(mutex);
    tempo=p.settings.bpm;length=p.length();master=p.masterGain;looping=p.loop;loopFrom=p.loopStart;loopTo=p.loopEnd;
    for(size_t ti=0;ti<instruments.size();++ti){auto& i=*instruments[ti];const auto& t=p.tracks[ti];i.schedule=std::move(schedules[ti]);i.volume=t.volume;i.gain=t.gain;i.pan=t.pan;i.mute=t.mute;i.solo=t.solo;i.sound=(float)t.sound;i.tone=(float)t.engine.tone;i.drums.setKit(t.sound);i.drums.setTone((float)t.engine.tone);i.guitar.setSound(t.sound);i.guitar.setTone((float)t.engine.tone);for(size_t lane=0;lane<i.guitarLevels.size();++lane){i.guitarLevels[lane]=(float)t.engine.laneVolume[lane];if(i.guitarMuted[lane]!=t.engine.laneMute[lane]){needsReset=true;needsChase=playing;}i.guitarMuted[lane]=t.engine.laneMute[lane];}juce::Reverb::Parameters rp;rp.roomSize=.4f;rp.wetLevel=(float)t.engine.reverb*.4f;rp.dryLevel=.5f;rp.width=1;i.reverb.setParameters(rp);i.chorus.setMix((float)t.engine.chorus*.5f);i.chorus.setRate(.35f);i.chorus.setDepth(.25f);i.pump=t.engine.pump;i.pumpRelease=t.engine.pumpRelease;i.pumpRate=t.engine.pumpRate;i.pumpSource=t.engine.pumpSource;if(ti>=4)i.drumNotes[ti-4]=t.engine.midiNote>=0?t.engine.midiNote:fgd::defaultNote((int)ti-4);}
    currentBeat=std::min(currentBeat.load(),length);needsReset=true;needsChase=playing;
}
void AudioEngine::play(bool enabled){const juce::ScopedLock lock(mutex);if(enabled&&currentBeat>=length)currentBeat=looping?loopFrom:0;playing=enabled;needsReset=true;needsChase=enabled;}
void AudioEngine::updateMix(const Project& p){const juce::ScopedLock lock(mutex);tempo=p.settings.bpm;master=p.masterGain;for(size_t ti=0;ti<instruments.size();++ti){auto& i=*instruments[ti];const auto& t=p.tracks[ti];i.volume=t.volume;i.gain=t.gain;i.pan=t.pan;i.mute=t.mute;i.solo=t.solo;i.sound=(float)t.sound;i.tone=(float)t.engine.tone;i.drums.setKit(t.sound);i.drums.setTone((float)t.engine.tone);i.guitar.setSound(t.sound);i.guitar.setTone((float)t.engine.tone);for(size_t lane=0;lane<i.guitarLevels.size();++lane){i.guitarLevels[lane]=(float)t.engine.laneVolume[lane];if(i.guitarMuted[lane]!=t.engine.laneMute[lane]){needsReset=true;needsChase=playing;}i.guitarMuted[lane]=t.engine.laneMute[lane];}juce::Reverb::Parameters rp;rp.roomSize=.4f;rp.wetLevel=(float)t.engine.reverb*.4f;rp.dryLevel=.5f;rp.width=1;i.reverb.setParameters(rp);i.chorus.setMix((float)t.engine.chorus*.5f);i.chorus.setRate(.35f);i.chorus.setDepth(.25f);i.pump=t.engine.pump;i.pumpRelease=t.engine.pumpRelease;i.pumpRate=t.engine.pumpRate;i.pumpSource=t.engine.pumpSource;if(ti>=4)i.drumNotes[ti-4]=t.engine.midiNote>=0?t.engine.midiNote:fgd::defaultNote((int)ti-4);}}
void AudioEngine::stop(){play(false);seek(0);}
void AudioEngine::seek(double beat){const juce::ScopedLock lock(mutex);currentBeat=std::clamp(beat,0.0,length);needsReset=true;needsChase=playing;}
void AudioEngine::setLoop(bool enabled,double from,double to){const juce::ScopedLock lock(mutex);looping=enabled;loopFrom=std::clamp(from,0.0,length);loopTo=std::clamp(to,loopFrom,length);}
void AudioEngine::resetAt(double beat){
    for(auto& ptr:instruments){auto& i=*ptr;i.reset();i.cursor=(size_t)(std::lower_bound(i.schedule.begin(),i.schedule.end(),beat,[](const TimedEvent& e,double b){return e.beat<b;})-i.schedule.begin());
        i.midi.addEvent(juce::MidiMessage::controllerEvent(1,64,0),0);i.midi.addEvent(juce::MidiMessage::controllerEvent(1,11,127),0);i.midi.addEvent(juce::MidiMessage::allNotesOff(1),0);
        if(needsChase){std::array<int,128> vel{},lane{},cc;cc.fill(-1);std::array<bool,128> pressed{};bool sustain=false;
            for(size_t k=0;k<i.cursor;++k){const auto& e=i.schedule[k];if(i.kind==3&&e.controller<0&&i.guitarMuted[(size_t)std::clamp(e.lane,0,fgg::numLanes-1)])continue;if(e.controller>=0){cc[(size_t)e.controller]=e.velocity;if(e.controller==64){sustain=e.velocity>=64;if(!sustain)for(size_t n=0;n<128;++n)if(!pressed[n])vel[n]=0;}}else{auto n=(size_t)e.pitch;pressed[n]=e.velocity>0;if(e.velocity>0||!sustain)vel[n]=e.velocity;lane[n]=e.lane;}}
            for(int c=0;c<128;++c)if(cc[(size_t)c]>=0)i.midi.addEvent(juce::MidiMessage::controllerEvent(1,c,cc[(size_t)c]),0);
            for(int n=0;n<128;++n)if(vel[(size_t)n]>0){i.midi.addEvent(juce::MidiMessage::noteOn(1,n,(juce::uint8)vel[(size_t)n]),0);if(!pressed[(size_t)n])i.midi.addEvent(juce::MidiMessage::noteOff(1,n),0);if(i.kind==3)i.guitarEvents.push_back({0,n,vel[(size_t)n]/127.f,lane[(size_t)n]});}}

    }needsReset=false;needsChase=false;
}
void AudioEngine::render(float* const* output,int channels,int count,double beat,int onlyTrack){
    const double samplesPerBeat=sampleRate*60./tempo.load(),end=beat+count/samplesPerBeat;
    bool anySolo=false;for(auto& i:instruments)anySolo=anySolo||i->solo;
    for(int ti=0;ti<trackCount;++ti){auto& i=*instruments[(size_t)ti];
        while(i.cursor<i.schedule.size()&&i.schedule[i.cursor].beat<end){const auto& e=i.schedule[i.cursor++];if(e.beat<beat-1e-8||(ti==3&&e.controller<0&&i.guitarMuted[(size_t)std::clamp(e.lane,0,fgg::numLanes-1)]))continue;int at=std::clamp((int)std::llround((e.beat-beat)*samplesPerBeat),0,count-1);
            i.midi.addEvent(e.controller>=0?juce::MidiMessage::controllerEvent(1,e.controller,e.velocity):e.velocity?juce::MidiMessage::noteOn(1,e.pitch,(juce::uint8)e.velocity):juce::MidiMessage::noteOff(1,e.pitch),at);
            if(ti==3&&e.controller<0)i.guitarEvents.push_back({at,e.pitch,e.velocity/127.f,e.lane});
        }
        juce::AudioBuffer<float> block(i.scratch.getArrayOfWritePointers(),i.scratch.getNumChannels(),count);block.clear();
        if(i.mode==2&&i.plugin)i.plugin->processBlock(block,i.midi);
        else if(i.mode==1)i.synth.renderNextBlock(block,i.midi,0,count);
        else if(i.mode==0){if(ti<3)i.synth.renderNextBlock(block,i.midi,0,count);else if(ti==3)i.guitar.render(block,i.guitarEvents,i.guitarLevels);else i.drums.render(block,i.midi,i.drumNotes,i.drumLevels,i.drumSamples);}
        if(i.mode==0){
            juce::dsp::AudioBlock<float> stereo(block.getArrayOfWritePointers(),2,(size_t)count);juce::dsp::ProcessContextReplacing<float> context(stereo);i.chorus.process(context);
            i.reverb.processStereo(block.getWritePointer(0),block.getWritePointer(1),count);
            if(i.pump>0)for(int sample=0;sample<count;++sample){const double time=beat+sample/samplesPerBeat;static const double rates[]{1,.5,2,.25};double phase=std::fmod(time,rates[i.pumpRate]);if(i.pumpSource==1){const auto& kick=instruments[4]->schedule;auto it=std::upper_bound(kick.begin(),kick.end(),time,[](double t,const TimedEvent& e){return t<e.beat;});phase=100;while(it!=kick.begin()){--it;if(it->controller<0&&it->velocity>0){phase=time-it->beat;break;}}}const float gain=(float)(1-i.pump*std::exp(-phase/std::max(.05,i.pumpRelease)));for(int ch=0;ch<2;++ch)block.setSample(ch,sample,block.getSample(ch,sample)*gain);}
        }
        i.midi.clear();i.guitarEvents.clear();
        const bool audible=onlyTrack>=0?ti==onlyTrack:(!i.mute&&(!anySolo||i.solo));
        float meter=0;
        if(audible){for(int s=0;s<count;++s)i.volumeGains[(size_t)s]=(float)i.volume.valueAt(beat+s/samplesPerBeat);float left=(float)(i.gain*master*std::min(1.,1.-i.pan)),right=(float)(i.gain*master*std::min(1.,1.+i.pan));
            for(int ch=0;ch<std::min(2,channels);++ch){auto* in=block.getReadPointer(std::min(ch,block.getNumChannels()-1));float gain=ch==0?left:right;for(int s=0;s<count;++s){const float v=in[s]*gain*i.volumeGains[(size_t)s];output[ch][s]+=v;meter=std::max(meter,std::abs(v));}}
        }meters[(size_t)ti]=meter;
    }
}
void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* d){const juce::ScopedLock lock(mutex);deviceRate=d->getCurrentSampleRate()>0?d->getCurrentSampleRate():44100;deviceBlock=std::max(16,d->getCurrentBufferSizeSamples());if(exporting)return;sampleRate=deviceRate;blockSize=deviceBlock;for(auto& i:instruments)i->prepare(sampleRate,blockSize);needsReset=true;needsChase=playing;}
void AudioEngine::audioDeviceStopped(){const juce::ScopedLock lock(mutex);if(exporting)return;for(auto& i:instruments)i->reset();}
void AudioEngine::audioDeviceIOCallbackWithContext(const float* const*,int,float* const* output,int channels,int count,const juce::AudioIODeviceCallbackContext&){
    for(int ch=0;ch<channels;++ch)if(output[ch])juce::FloatVectorOperations::clear(output[ch],count);
    if(exporting)return;
    const juce::ScopedTryLock lock(mutex);if(!lock.isLocked()||exporting)return;
    if(needsReset)resetAt(currentBeat);
    if(!playing)return;
    int done=0;
    while(done<count&&playing){double beat=currentBeat;const bool hasLoop=looping&&loopTo-loopFrom>=1./960.;double boundary=hasLoop?loopTo:length;
        if(beat>=boundary-1e-9){if(hasLoop){currentBeat=loopFrom;needsChase=true;resetAt(loopFrom);continue;}playing=false;needsReset=true;break;}
        const int remain=std::max(1,(int)std::ceil((boundary-beat)*sampleRate*60./tempo.load()));int n=std::min({count-done,blockSize,remain});
        float* pointers[2]{};int used=std::min(2,channels);for(int ch=0;ch<used;++ch)pointers[ch]=output[ch]+done;
        render(pointers,used,n,beat);currentBeat=beat+n*tempo.load()/(sampleRate*60.);done+=n;
    }
    // Final safety ceiling; export reports and preserves the floating-point mix before encoding.
    for(int ch=0;ch<channels;++ch)if(output[ch])for(int s=0;s<count;++s)output[ch][s]=std::clamp(output[ch][s],-.999f,.999f);
}
juce::Optional<juce::AudioPlayHead::PositionInfo> AudioEngine::getPosition() const{
    PositionInfo p;p.setBpm(tempo.load());p.setPpqPosition(currentBeat.load());p.setTimeInSeconds(currentBeat.load()*60./tempo.load());p.setIsPlaying(playing.load()||exporting.load());p.setTimeSignature(juce::AudioPlayHead::TimeSignature{4,4});return p;
}
void AudioEngine::useInternal(int track){const juce::ScopedLock lock(mutex);auto& i=*instruments[(size_t)track];if(i.plugin)i.plugin->releaseResources();i.plugin.reset();i.synth.clearSounds();i.synth.clearVoices();if(track<3){for(int v=0;v<48;++v)i.synth.addVoice(new GuideVoice(i.tone,i.sound));if(track==2)i.synth.addSound(new GuideStringsTag(1));else i.synth.addSound(new GuideSoundTag);}
    i.mode=0;i.prepare(sampleRate,blockSize);needsReset=true;needsChase=playing;
}
juce::Result AudioEngine::loadSample(int track,const juce::File& file,int root){
    juce::AudioFormatManager fm;fm.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(file));
    if(!r||r->lengthInSamples<=0)return juce::Result::fail("Campione audio non leggibile.");
    if(r->lengthInSamples>r->sampleRate*60)return juce::Result::fail("Usare campioni fino a 60 secondi.");
    juce::BigInteger range;range.setRange(0,128,true);auto sound=std::make_unique<juce::SamplerSound>(file.getFileName(),*r,range,std::clamp(root,0,127),.002,.1,60.);
    const juce::ScopedLock lock(mutex);auto& i=*instruments[(size_t)track];if(i.plugin)i.plugin->releaseResources();i.plugin.reset();i.synth.clearSounds();i.synth.clearVoices();for(int v=0;v<32;++v)i.synth.addVoice(new juce::SamplerVoice);i.synth.addSound(sound.release());i.mode=1;i.prepare(sampleRate,blockSize);needsReset=true;needsChase=playing;return juce::Result::ok();
}
void AudioEngine::loadPlugin(int track,const juce::PluginDescription& description,std::function<void(juce::Result)> callback){
    formats.createPluginInstanceAsync(description,sampleRate,blockSize,[this,track,callback,weak=std::weak_ptr<int>(lifetime)](std::unique_ptr<juce::AudioPluginInstance> next,const juce::String& error){
        if(weak.expired())return;
        if(!next){callback(juce::Result::fail(error));return;}
        next->enableAllBuses();auto layout=next->getBusesLayout();for(auto& b:layout.inputBuses)b=juce::AudioChannelSet::disabled();if(!layout.outputBuses.isEmpty())layout.outputBuses.getReference(0)=juce::AudioChannelSet::stereo();
        if(next->checkBusesLayoutSupported(layout))next->setBusesLayout(layout);
        if(next->getTotalNumOutputChannels()<1||next->getTotalNumOutputChannels()>64){callback(juce::Result::fail("Il plugin deve avere da 1 a 64 uscite audio."));return;}
        const juce::ScopedLock lock(mutex);auto& i=*instruments[(size_t)track];if(i.plugin)i.plugin->releaseResources();i.plugin=std::move(next);i.plugin->setPlayHead(this);i.mode=2;i.prepare(sampleRate,blockSize);needsReset=true;needsChase=playing;callback(juce::Result::ok());
    });
}
juce::AudioPluginInstance* AudioEngine::plugin(int track){return instruments[(size_t)track]->plugin.get();}
void AudioEngine::capturePluginStates(Project& p){const juce::ScopedLock lock(mutex);for(size_t ti=0;ti<instruments.size();++ti)if(auto* plugin=instruments[ti]->plugin.get()){juce::MemoryBlock state;plugin->getStateInformation(state);p.tracks[ti].instrumentState=state.toBase64Encoding().toStdString();}}
void AudioEngine::restorePluginState(int track,const juce::String& state){juce::MemoryBlock data;if(data.fromBase64Encoding(state)){const juce::ScopedLock lock(mutex);if(auto* p=instruments[(size_t)track]->plugin.get())p->setStateInformation(data.getData(),(int)data.getSize());}}
void AudioEngine::markMissing(int track){const juce::ScopedLock lock(mutex);instruments[(size_t)track]->mode=3;}
juce::Result AudioEngine::exportAudio(const Project& source,const juce::File& file,int onlyTrack,bool mp3,std::function<bool()> cancelled,std::function<void(double)> progress,double* rawPeak){
    if(rawPeak)*rawPeak=0;
    if(onlyTrack < -1 || onlyTrack>=trackCount)return juce::Result::fail("Invalid export track");
    auto p=source;p.normalise();
    bool expected=false;if(!exporting.compare_exchange_strong(expected,true))return juce::Result::fail("Esportazione gia in corso.");
    struct Guard{AudioEngine& e;~Guard(){const juce::ScopedLock lock(e.mutex);e.sampleRate=e.deviceRate;e.blockSize=e.deviceBlock;for(auto& i:e.instruments){if(i->plugin)i->plugin->setNonRealtime(false);i->prepare(e.sampleRate,e.blockSize);}e.playing=false;e.currentBeat=0;e.needsReset=true;e.exporting=false;}} guard{*this};
    publish(p);
    {const juce::ScopedLock lock(mutex);for(int ti=0;ti<trackCount;++ti)if(instruments[(size_t)ti]->mode==3&&(onlyTrack<0||ti==onlyTrack))return juce::Result::fail("Ricollega gli strumenti mancanti prima di esportare.");
        playing=false;currentBeat=0;needsChase=false;if(mp3)sampleRate=44100;
        for(auto& i:instruments){if(i->plugin)i->plugin->setNonRealtime(true);i->prepare(sampleRate,blockSize);}resetAt(0);}
    juce::TemporaryFile temp(file);auto stream=temp.getFile().createOutputStream();if(!stream)return juce::Result::fail("Impossibile scrivere il file audio.");
    juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(),sampleRate,2,24,{},0));if(!writer)return juce::Result::fail("Encoder WAV non disponibile.");stream.release();
    juce::AudioBuffer<float> buffer(2,blockSize);const auto total=(juce::int64)std::ceil((p.length()*60./p.settings.bpm+3.)*sampleRate);
    for(juce::int64 at=0;at<total;){if(cancelled())return juce::Result::fail("Esportazione annullata.");int n=(int)std::min<juce::int64>(blockSize,total-at);buffer.clear();
        {const juce::ScopedLock lock(mutex);currentBeat=at*p.settings.bpm/(sampleRate*60.);render(buffer.getArrayOfWritePointers(),2,n,currentBeat,onlyTrack);}
        for(int ch=0;ch<2;++ch)for(int sample=0;sample<n;++sample){
            const float value=buffer.getSample(ch,sample);
            if(!std::isfinite(value))return juce::Result::fail("Audio non finito: controlla lo strumento e i livelli.");
            if(rawPeak)*rawPeak=std::max(*rawPeak,(double)std::abs(value));
            buffer.setSample(ch,sample,std::clamp(value,-.999f,.999f));
        }
        if(!writer->writeFromAudioSampleBuffer(buffer,0,n))return juce::Result::fail("Scrittura WAV fallita (spazio disco?).");at+=n;progress((double)at/(double)total*(mp3?.8:1.));
    }writer.reset();
    if(!mp3)return temp.overwriteTargetFileWithTemporary()?juce::Result::ok():juce::Result::fail("Impossibile completare il WAV.");
    auto wavStream=temp.getFile().createInputStream();if(!wavStream)return juce::Result::fail("WAV temporaneo non leggibile.");
    std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(wavStream.release(),true));if(!reader)return juce::Result::fail("WAV temporaneo non valido.");
    Mp3Encoder encoder;if(!encoder.valid())return juce::Result::fail("Encoder MP3 non disponibile.");
    juce::TemporaryFile encoded(file);auto out=encoded.getFile().createOutputStream();if(!out)return juce::Result::fail("Impossibile scrivere MP3.");
    const int frame=encoder.frameSize();juce::AudioBuffer<float> audio(2,frame);std::vector<std::int16_t> pcm((size_t)frame*2);
    for(juce::int64 at=0;at<total;at+=frame){if(cancelled())return juce::Result::fail("Esportazione annullata.");audio.clear();reader->read(&audio,0,(int)std::min<juce::int64>(frame,total-at),at,true,true);
        for(int i=0;i<frame;++i)for(int ch=0;ch<2;++ch)pcm[(size_t)i*2+(size_t)ch]=(std::int16_t)std::lround(std::clamp(audio.getSample(ch,i),-.999f,.999f)*32767.f);
        auto bytes=encoder.encode(pcm.data());if(!out->write(bytes.data(),bytes.size()))return juce::Result::fail("Scrittura MP3 fallita.");progress(.8+.2*std::min(1.,(double)(at+frame)/(double)total));
    }auto bytes=encoder.flush();if(!out->write(bytes.data(),bytes.size()))return juce::Result::fail("Scrittura MP3 fallita.");out.reset();
    return encoded.overwriteTargetFileWithTemporary()?juce::Result::ok():juce::Result::fail("Impossibile completare MP3.");
}
}
