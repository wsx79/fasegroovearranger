// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Sampler.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_cryptography/juce_cryptography.h>
#include "signalsmith-stretch.h"
#include <limits>
#include <cstring>
namespace fga {
namespace {std::atomic<std::int64_t> reservedSampleMemory{0};}
void SampleLoadLimits::normalise(){
    constexpr std::int64_t mib=1024*1024;
    fileBytes=std::clamp(fileBytes,mib,512*mib);decodedBytes=std::clamp(decodedBytes,mib,maxDecodedSampleBytes);
    totalBytes=std::clamp(totalBytes,64*mib,2048*mib);seconds=bounded(seconds,maxSampleSeconds,1,maxSampleSeconds);
}
std::int64_t sampleMemoryInUse(){return reservedSampleMemory.load(std::memory_order_acquire);}
SampleAsset::~SampleAsset(){
    // Release accounting only after the large buffers have actually been freed.
    audio.setSize(0,0);std::vector<std::array<float,2>>().swap(peaks);
    if(reservedBytes)reservedSampleMemory.fetch_sub(reservedBytes,std::memory_order_release);
}
juce::Result readSampleAsset(const juce::File& file,std::shared_ptr<const SampleAsset>& result,
    SampleLoadLimits limits,const std::function<bool()>& cancelled,const std::function<void(double)>& progress){
    limits.normalise();auto stopped=[&]{return cancelled&&cancelled();};
    auto report=[&](double value){if(progress)progress(value);};
    if(stopped())return juce::Result::fail("Sample import cancelled.");
    if(file.getSize()>limits.fileBytes)return juce::Result::fail("Sample file exceeds the configured file limit ("+juce::String(limits.fileBytes/(1024*1024))+" MiB).");
    juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if(!reader||!fitsSampleMemory(reader->lengthInSamples,reader->sampleRate,(int)reader->numChannels))return juce::Result::fail("Sample audio is invalid or exceeds the hard 30 minute / 512 MiB decoded limit.");
    if(reader->lengthInSamples/reader->sampleRate>limits.seconds)return juce::Result::fail("Sample exceeds the configured duration limit ("+juce::String(limits.seconds,0)+" seconds).");
    const int frames=(int)reader->lengthInSamples,channels=std::min(2,(int)reader->numChannels),bins=std::min(8192,frames);
    const std::int64_t bytes=(std::int64_t)(frames+4)*channels*sizeof(float)+(std::int64_t)bins*sizeof(std::array<float,2>);
    if(bytes>limits.decodedBytes)return juce::Result::fail("Sample exceeds the configured decoded-memory limit ("+juce::String(limits.decodedBytes/(1024*1024))+" MiB).");
    auto asset=std::make_shared<SampleAsset>();auto used=sampleMemoryInUse();
    do{if(bytes>limits.totalBytes-used)return juce::Result::fail("Sample exceeds the total decoded-memory budget ("+juce::String(limits.totalBytes/(1024*1024))+" MiB); the previous sample remains reserved during replacement.");}
    while(!reservedSampleMemory.compare_exchange_weak(used,used+bytes,std::memory_order_acq_rel));
    asset->reservedBytes=bytes;asset->file=file;asset->rate=reader->sampleRate;asset->frames=frames;
    asset->audio.setSize(channels,frames+4);asset->audio.clear();
    for(int at=0;at<frames;at+=16384){
        if(stopped())return juce::Result::fail("Sample import cancelled.");
        const int n=std::min(16384,frames-at);
        if(!reader->read(&asset->audio,at,n,at,true,true))return juce::Result::fail("Sample decoding failed.");
        for(int c=0;c<channels;++c)for(int i=at;i<at+n;++i)if(!std::isfinite(asset->audio.getSample(c,i)))asset->audio.setSample(c,i,0);
        report(.6*(at+n)/frames);
    }
    auto in=file.createInputStream();if(!in)return juce::Result::fail("Cannot read sample fingerprint.");
    // InputStream adapter lets JUCE SHA256 read bounded, cancellable blocks.
    struct Input final:juce::InputStream {
        juce::InputStream& source;const std::function<bool()>& stop;
        Input(juce::InputStream& s,const std::function<bool()>& c):source(s),stop(c){}
        bool stopped()const{return stop&&stop();}
        juce::int64 getTotalLength()override{return source.getTotalLength();}
        juce::int64 getPosition()override{return source.getPosition();}
        bool setPosition(juce::int64 p)override{return !stopped()&&source.setPosition(p);}
        bool isExhausted()override{return stopped()||source.isExhausted();}
        int read(void* data,int size)override{return stopped()?0:source.read(data,std::min(size,65536));}
    } checked(*in,cancelled);
    asset->hash=juce::SHA256(checked).toHexString();
    if(stopped())return juce::Result::fail("Sample import cancelled.");
    if(in->getStatus().failed()||in->getPosition()!=file.getSize())return juce::Result::fail("Cannot read complete sample fingerprint.");
    report(.8);asset->peaks.resize((size_t)bins);
    for(int bin=0;bin<bins;++bin){
        if((bin%128)==0&&stopped())return juce::Result::fail("Sample import cancelled.");
        float lo=0,hi=0;const int a=(int)((int64_t)bin*frames/bins),b=(int)((int64_t)(bin+1)*frames/bins);
        for(int c=0;c<channels;++c)for(int i=a;i<b;++i){auto x=asset->audio.getSample(c,i);lo=std::min(lo,x);hi=std::max(hi,x);}asset->peaks[(size_t)bin]={lo,hi};
    }
    if(stopped())return juce::Result::fail("Sample import cancelled.");
    result=std::move(asset);report(1);return juce::Result::ok();
}
double snapSlicePoint(const SampleAsset& asset,double point,const SamplerSettings& settings,double bpm){
    point=bounded(point,settings.start,settings.start,settings.end);
    if(settings.sliceSnapBeats<=0)return settings.snap?snapSamplePoint(asset,point):point;
    if(!std::isfinite(bpm)||bpm<=0||asset.seconds()<=0)return point;
    const double speed=settings.sync?bpm/settings.sourceBpm:std::pow(2.,(settings.transpose+settings.fine*.01)/12.);
    const double step=settings.sliceSnapBeats*60./bpm*speed/asset.seconds();
    if(!std::isfinite(step)||step<=0)return point;
    return std::clamp(settings.start+std::round((point-settings.start)/step)*step,settings.start,settings.end);
}
double snapSamplePoint(const SampleAsset& a,double p){const int centre=std::clamp((int)std::llround(p*a.frames),0,a.frames-1),radius=(int)std::min(512.,a.rate*.01);int best=centre;double score=std::numeric_limits<double>::max();const auto* x=a.audio.getReadPointer(0);
    for(int i=std::max(1,centre-radius);i<std::min(a.frames-1,centre+radius);++i)if((x[i-1]<=0&&x[i]>=0)||(x[i-1]>=0&&x[i]<=0)){double v=std::abs(i-centre)+std::abs(x[i])*8;if(v<score){score=v;best=i;}}return best/(double)a.frames;}
std::vector<double> detectSampleSlices(const SampleAsset& a,double start,double end,double sensitivity,int limit){
    std::vector<double> points;start=std::clamp(start,0.,.99999);end=std::clamp(end,start+.00001,1.);limit=std::clamp(limit,1,128);points.push_back(start);if(limit==1)return points;
    const int first=(int)(start*a.frames),last=(int)(end*a.frames),window=std::max(16,(int)(a.rate*.005));double previous=0,average=0;int lastHit=first;
    for(int at=first;at<last;at+=window){double energy=0;for(int i=at;i<std::min(at+window,last);++i)for(int c=0;c<a.audio.getNumChannels();++c){auto v=a.audio.getSample(c,i);energy+=v*v;}energy=std::sqrt(energy/(window*a.audio.getNumChannels()));
        const double floor=.002+(1-sensitivity)*.035,ratio=1.25+(1-sensitivity)*3.;
        if(energy>floor&&energy>std::max(previous*ratio,average*(1.4+(1-sensitivity)*2))&&at-lastHit>a.rate*(.03+(1-sensitivity)*.08)){
            auto p=snapSamplePoint(a,at/(double)a.frames);if(p>points.back()+.00001&&p<end-.00001){points.push_back(p);lastHit=at;if((int)points.size()>=limit)break;}}
        previous=energy;average=average*.85+energy*.15;
    }return points;
}
namespace {
struct SampleSound : juce::SynthesiserSound {
    explicit SampleSound(std::shared_ptr<const SampleAsset> p):data(std::move(p)){}
    std::shared_ptr<const SampleAsset> data;
    bool appliesToNote(int)override{return true;}bool appliesToChannel(int)override{return true;}
};
class SampleVoice : public juce::SynthesiserVoice {
public:
    explicit SampleVoice(SampleSynth& s):owner(s){}
    bool canPlaySound(juce::SynthesiserSound* s)override{return dynamic_cast<SampleSound*>(s)!=nullptr;}
    void setCurrentPlaybackSampleRate(double rate)override{
        rate=std::isfinite(rate)&&rate>=8000?rate:44100;juce::SynthesiserVoice::setCurrentPlaybackSampleRate(rate);envelope.setSampleRate(rate);
        const int size=juce::nextPowerOfTwo(std::max(256,(int)(2048*rate/48000.)));warp.configure(2,size,size/4,false);owner.latency=warp.outputLatency();
        filter.prepare({rate,512,2});filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);filter.reset();
    }
    bool retiring()const{return retriggerLeft>0;}
    // Release on the output clock, when the replacement voice becomes audible.
    void fadeForRetrigger(){retriggerDelay=owner.latencySamples();retriggerTotal=retriggerLeft=std::max(1,(int)std::lround(getSampleRate()*.0025));}
    void startNote(int note,float velocity,juce::SynthesiserSound*,int wheel)override{
        const auto& p=owner.settings();retriggerDelay=retriggerLeft=0;data=&owner.asset();pitch=note;level=1-p.velocity+p.velocity*velocity;pitchWheel=wheel;sourcePos=0;inputFraction=0;playedSource=0;elapsed=0;releaseAt=-1;released=false;filter.reset();
        begin=p.start*data->frames;finish=p.end*data->frames;
        reverseRegion=owner.regionReverse||owner.settings().reverse;region=owner.regionOffset>=0;fadeIn=owner.regionFadeIn;fadeOut=owner.regionFadeOut;
        if(region){begin=std::clamp(owner.regionOffset*data->rate,0.,(double)data->frames);finish=std::min((double)data->frames,begin+owner.regionDuration*data->rate);}
        if(owner.mode()==2){int index=note-p.sliceBase;if(index<0||index>=p.sliceCount){clearCurrentNote();data=nullptr;return;}begin=std::round(p.slices[(size_t)index]*data->frames);finish=std::round((index+1<p.sliceCount?p.slices[(size_t)index+1]:p.end)*data->frames);}
        updateParameters();envelope.reset();envelope.noteOn();shaped.noteOn(p,getSampleRate());warp.reset();
        if(p.sync){Reader reader{*this,0};const int seek=warp.inputLatency();warp.seek(reader,seek,speed());sourcePos=seek;}
    }
    void stopNote(float,bool allowTailOff)override{
        if(!allowTailOff||region){retriggerDelay=retriggerLeft=0;envelope.reset();shaped.reset();clearCurrentNote();data=nullptr;return;}
        if(owner.mode()==0&&releaseAt<0)releaseAt=elapsed+owner.latencySamples();
    }
    void pitchWheelMoved(int value)override{pitchWheel=value;}
    void controllerMoved(int,int)override{}
    void renderNextBlock(juce::AudioBuffer<float>& output,int start,int count)override{
        if(!data||!isVoiceActive())return;updateParameters();const auto& p=owner.settings();
        // Fixed stack buffers bound work and avoid allocation at any host block size.
        for(int done=0;done<count&&data;){const int n=std::min(256,count-done);float left[256]{},right[256]{};float* target[]{left,right};
            int consumedInput=0;
            if(p.sync){const double need=n*speed()+inputFraction;consumedInput=(int)need;inputFraction=need-consumedInput;Reader reader{*this,sourcePos};warp.process(reader,consumedInput,target,n);sourcePos+=consumedInput;}
            else{const double ratio=data->rate/getSampleRate()*std::pow(2.,semitones()/12.);for(int i=0;i<n;++i){left[i]=read(0,sourcePos);right[i]=read(1,sourcePos);sourcePos+=ratio;}}
            const int lag=owner.latencySamples();
            const bool looping=!region&&p.loop&&owner.mode()==0;
            // Sync input maps through rate/sr. Slice duration starts on the audible
            // output clock; regions/legacy modes retain their source-clock behavior.
            const double sourceStep=p.sync?(double)consumedInput*(data->rate/getSampleRate())/std::max(1,n):data->rate/getSampleRate()*speed();
            for(int i=0;i<n&&data;++i,++elapsed){
                if(releaseAt>=0&&elapsed>=releaseAt&&!released){envelope.noteOff();shaped.noteOff();released=true;}
                if(elapsed<lag){if(region||p.mode!=2)playedSource+=sourceStep;if(retriggerDelay>0)--retriggerDelay;continue;}
                const auto at=elapsed-lag;
                if(!looping&&playedSource>=finish-begin){envelope.reset();shaped.reset();clearCurrentNote();data=nullptr;break;}
                const float linear=envelope.getNextSample(),bent=shaped.next();
                float gain=(shaped.curved()?bent:linear)*level;
                if(retriggerLeft>0&&retriggerDelay==0)gain*=(float)retriggerLeft/retriggerTotal;
                if(region){const double remaining=std::max(0.,finish-begin-playedSource)/(data->rate*std::max(.001,speed()));const double elapsedSeconds=at/getSampleRate();gain*=(float)(std::clamp(elapsedSeconds/std::max(.0025,fadeIn),0.,1.)*std::clamp(remaining/std::max(.0025,fadeOut),0.,1.));}
                if(p.fadeIn>0){const float lin=(float)std::clamp(at/(p.fadeIn*getSampleRate()),0.,1.);gain*=std::abs(p.fadeInCurve)<1e-7f?lin:(float)sampleEnvelopeCurve(lin,p.fadeInCurve);}
                if(!looping&&p.fadeOut>0){const float lin=(float)std::clamp((finish-begin-playedSource)/(p.fadeOut*data->rate*speed()),0.,1.);gain*=std::abs(p.fadeOutCurve)<1e-7f?lin:(float)sampleEnvelopeCurve(lin,p.fadeOutCurve);}
                float l=left[i],r=right[i];if(p.filter){l=filter.processSample(0,l);r=filter.processSample(1,r);}l*=gain;r*=gain;
                output.addSample(0,start+done+i,l);if(output.getNumChannels()>1)output.addSample(1,start+done+i,r);
                playedSource+=sourceStep;
                if(retriggerDelay>0)--retriggerDelay;
                else if(retriggerLeft>0&&--retriggerLeft==0){clearCurrentNote();data=nullptr;}
                if(!envelope.isActive()){clearCurrentNote();data=nullptr;}
            }done+=n;
        }
    }
private:
    struct ShapedEnvelope {
        enum class Stage { idle, attack, decay, sustain, release };
        Stage stage=Stage::idle;
        double sr=44100,phase=0,level=0,releaseFrom=0,attack=0,decay=0,sustain=1,release=.1,attackCurve=0,decayCurve=0,releaseCurve=0;
        void reset(){stage=Stage::idle;phase=0;level=0;}
        void apply(const SamplerSettings& p,double rate){if(rate>0)sr=rate;attack=p.attack;decay=p.decay;sustain=p.sustain;release=p.release;attackCurve=p.attackCurve;decayCurve=p.decayCurve;releaseCurve=p.releaseCurve;}
        void noteOn(const SamplerSettings& p,double rate){apply(p,rate);phase=0;if(attack>0){stage=Stage::attack;level=0;}else if(decay>0&&sustain<1){stage=Stage::decay;level=1;}else{stage=Stage::sustain;level=sustain;}}
        void noteOff(){if(stage==Stage::idle)return;releaseFrom=level;phase=0;if(release>0)stage=Stage::release;else{stage=Stage::idle;level=0;}}
        bool curved()const{return std::abs(attackCurve)>1e-6||std::abs(decayCurve)>1e-6||std::abs(releaseCurve)>1e-6;}
        float next(){
            const double dt=1./std::max(1.,sr);
            switch(stage){
            case Stage::attack:{phase+=dt;level=sampleEnvelopeCurve(attack>0?std::min(1.,phase/attack):1.,attackCurve);if(phase>=attack){phase=0;level=1;stage=(decay>0&&sustain<1)?Stage::decay:Stage::sustain;if(stage==Stage::sustain)level=sustain;}break;}
            case Stage::decay:{phase+=dt;level=1.+(sustain-1.)*sampleEnvelopeCurve(decay>0?std::min(1.,phase/decay):1.,decayCurve);if(phase>=decay){stage=Stage::sustain;level=sustain;}break;}
            case Stage::sustain:level=sustain;break;
            case Stage::release:{phase+=dt;level=releaseFrom*(1.-sampleEnvelopeCurve(release>0?std::min(1.,phase/release):1.,releaseCurve));if(phase>=release||level<=0){level=0;stage=Stage::idle;}break;}
            case Stage::idle:level=0;break;
            }
            return (float)level;
        }
    };
    SampleSynth& owner;const SampleAsset* data=nullptr;
    signalsmith::stretch::SignalsmithStretch<float> warp{0};
    juce::ADSR envelope;ShapedEnvelope shaped;juce::dsp::StateVariableTPTFilter<float> filter;
    double sourcePos=0,begin=0,finish=0,inputFraction=0,playedSource=0,fadeIn=0,fadeOut=0;int pitch=60,pitchWheel=8192;
    int retriggerDelay=0,retriggerLeft=0,retriggerTotal=1;
    int64_t elapsed=0,releaseAt=-1;float level=1;bool released=false,region=false,reverseRegion=false;
    double semitones()const{return (owner.mode()==2?0:pitch-owner.root())+owner.settings().transpose+owner.settings().fine*.01+(pitchWheel-8192)/8192.*2.;}
    double speed()const{return owner.settings().sync?owner.tempo()/owner.settings().sourceBpm:std::pow(2.,semitones()/12.);}
    void updateParameters(){const auto& p=owner.settings();envelope.setParameters({(float)p.attack,(float)p.decay,(float)p.sustain,(float)p.release});shaped.apply(p,getSampleRate());filter.setCutoffFrequency((float)std::clamp(p.cutoff,20.,getSampleRate()*.45));filter.setResonance((float)(.5+p.resonance*8));warp.setTransposeSemitones((float)semitones());}
    float raw(int channel,double pos)const{
        if(pos<begin||pos>=finish)return 0;
        const int i=std::clamp((int)pos,0,data->frames-1);
        const float frac=(float)(pos-i);
        const auto* a=data->audio.getReadPointer(std::min(channel,data->audio.getNumChannels()-1));
        // Keep interpolation inside the crop/slice and the actual asset frames.
        const int next=i+1<finish?std::min(i+1,data->frames-1):i;
        return a[i]+frac*(a[next]-a[i]);
    }
    float read(int channel,double input)const{
        const auto& p=owner.settings();double progress=input*(p.sync?data->rate/getSampleRate():1.);bool wrapped=false;
        double pos=(region?reverseRegion:p.reverse)?finish-1-progress:begin+progress;
        const bool loop=!region&&p.loop&&owner.mode()==0;const double lo=std::max(begin,p.loopStart*data->frames),hi=std::min(finish,p.loopEnd*data->frames),length=hi-lo;
        if(loop&&length>1){if(!p.reverse&&pos>=hi){pos=lo+std::fmod(pos-hi,length);wrapped=true;}if(p.reverse&&pos<lo){pos=hi-1-std::fmod(lo-1-pos,length);wrapped=true;}}
        auto value=raw(channel,pos);if(!loop||length<=1)return value;
        const double fade=std::min(p.crossfade*data->rate,length*.5);if(fade<1)return value;
        // Crossfade adjoining source regions without reflecting their time direction.
        // Boundary extension keeps full-crop loops continuous and preserves the loop period.
        double distance=fade,other=pos;
        if(!p.reverse&&pos>hi-fade){distance=hi-pos;other=pos-length;}
        else if(!p.reverse&&wrapped&&pos<lo+fade){distance=pos-lo;other=pos+length;}
        else if(p.reverse&&pos<lo+fade){distance=pos-lo;other=pos+length;}
        else if(p.reverse&&wrapped&&pos>hi-fade){distance=hi-pos;other=pos-length;}
        if(distance>=fade)return value;const float weight=(float)(.5+.5*std::sin(std::clamp(distance/fade,0.,1.)*juce::MathConstants<double>::halfPi));return weight*value+(1-weight)*raw(channel,std::clamp(other,begin,finish-1));
    }
    struct Reader {
        SampleVoice& voice;double position;
        struct Channel {const Reader& reader;int channel;float operator[](int index)const{return reader.voice.read(channel,reader.position+index);}};
        Channel operator[](int channel)const{return {*this,channel};}
    };
};
}
SampleSynth::SampleSynth(std::shared_ptr<const SampleAsset> a,const SamplerSettings& p,int root,bool drum):data(std::move(a)),fallback(p),rootNote(root),drumTrack(drum){fallback.normalise();setCurrentPlaybackSampleRate(44100);for(int i=0;i<32;++i)addVoice(new SampleVoice(*this));addSound(new SampleSound(data));setNoteStealingEnabled(true);setMinimumRenderingSubdivisionSize(1,true);}
SampleSynth::~SampleSynth()=default;
void SampleSynth::addRegionEvent(juce::MidiBuffer& midi,int sample,int note,int velocity,double offset,double duration,double fadeIn,double fadeOut,bool reverse){
    // Non-commercial manufacturer ID, signature/version, pitch/velocity, two
    // doubles encoded in 7-bit groups. No heap-backed MidiMessage on the callback.
    unsigned char bytes[49]{0xf0,0x7d,'F','G','A',1,(unsigned char)note,(unsigned char)velocity};
    for(int field=0;field<4;++field){const double value=field==0?offset:field==1?duration:field==2?fadeIn:fadeOut;std::uint64_t bits;std::memcpy(&bits,&value,sizeof bits);for(int j=0;j<10;++j){bytes[8+field*10+j]=(unsigned char)(bits&127);bits>>=7;}}
    bytes[5]=(unsigned char)(reverse?2:1);bytes[48]=0xf7;midi.addEvent(bytes,49,sample);
}
bool SampleSynth::isRegionEvent(const unsigned char* bytes,int size){return size==49&&bytes[0]==0xf0&&bytes[1]==0x7d&&bytes[2]=='F'&&bytes[3]=='G'&&bytes[4]=='A'&&(bytes[5]==1||bytes[5]==2)&&bytes[48]==0xf7;}
bool SampleSynth::handleRegion(const unsigned char* bytes,int size){
    if(isRegionEvent(bytes,size)){
        double values[4]{};for(int field=0;field<4;++field){std::uint64_t bits=0;for(int j=0;j<10;++j)bits|=(std::uint64_t)(bytes[8+field*10+j]&127)<<(j*7);std::memcpy(&values[field],&bits,sizeof bits);}
        if(std::isfinite(values[0])&&values[0]>=0&&std::isfinite(values[1])&&values[1]>0&&std::isfinite(values[2])&&std::isfinite(values[3])){regionReverse=bytes[5]==2;regionOffset=values[0];regionDuration=values[1];regionFadeIn=std::clamp(values[2],0.,30.);regionFadeOut=std::clamp(values[3],0.,30.);noteOn(1,bytes[6],bytes[7]/127.f);regionOffset=-1;regionDuration=0;}
        return true;
    }
    return false;
}
void SampleSynth::handleMidiEvent(const juce::MidiMessage& message){
    // JUCE treats CC120 like a note release; one-shot/slice voices ignore releases.
    // Panic must stop all sounding audio on its channel, including retiring voices.
    if(message.isAllSoundOff()){allNotesOff(message.getChannel(),false);return;}
    if(!handleRegion(message.getRawData(),message.getRawDataSize()))juce::Synthesiser::handleMidiEvent(message);
}
void SampleSynth::renderNextBlock(juce::AudioBuffer<float>& output,const juce::MidiBuffer& midi,int start,int count){
    const juce::ScopedLock guard(lock);start=std::clamp(start,0,output.getNumSamples());count=std::clamp(count,0,output.getNumSamples()-start);const int end=start+count;int cursor=start;
    for(auto it=midi.findNextSamplePosition(start);it!=midi.cend();++it){const auto event=*it;if(event.samplePosition>=end)break;
        const int at=std::max(cursor,event.samplePosition);if(at>cursor)renderVoices(output,cursor,at-cursor);cursor=at;
        // Read the internal region message in place: constructing a MidiMessage
        // for this long payload would allocate on the audio thread.
        if(!handleRegion(event.data,event.numBytes))handleMidiEvent(event.getMessage());
    }
    if(cursor<end)renderVoices(output,cursor,end-cursor);
}
bool SampleSynth::configure(const SamplerSettings& p,int root,double tempo){const auto& old=*parameters;const bool restart=old.mode!=p.mode||old.sync!=p.sync||old.reverse!=p.reverse||old.start!=p.start||old.end!=p.end||old.sliceCount!=p.sliceCount||old.slices!=p.slices;if(restart)allNotesOff(0,false);fallback=p;parameters=&fallback;rootNote=root;bpm=tempo;return restart;}
void SampleSynth::setCurrentPlaybackSampleRate(double rate){juce::Synthesiser::setCurrentPlaybackSampleRate(std::isfinite(rate)&&rate>=8000?rate:44100);}
void SampleSynth::noteOn(int channel,int note,float velocity){
    if(mode()==2&&(note<settings().sliceBase||note>=settings().sliceBase+settings().sliceCount))return;
    // Slices can use a spare preallocated voice for a short release. Retiring
    // voices do not consume the playable polyphony budget; total voices stay 32.
    for(int tries=0;tries<32;++tries){
        int active=0;SampleVoice* oldest=nullptr;
        for(int i=0;i<getNumVoices();++i){auto* voice=dynamic_cast<SampleVoice*>(getVoice(i));if(!voice||!voice->isVoiceActive()||voice->retiring())continue;++active;if(!oldest||voice->wasStartedBefore(*oldest))oldest=voice;}
        if(active<settings().polyphony||!oldest)break;
        if(mode()==2&&settings().polyphony<getNumVoices())oldest->fadeForRetrigger();else oldest->stopNote(0,false);
    }
    juce::Synthesiser::noteOn(channel,note,velocity);
}
}
