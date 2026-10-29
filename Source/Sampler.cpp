// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Sampler.h"
#include <juce_dsp/juce_dsp.h>
#include <juce_cryptography/juce_cryptography.h>
#include "signalsmith-stretch.h"
#include <limits>
namespace fga {
juce::Result readSampleAsset(const juce::File& file,std::shared_ptr<const SampleAsset>& result){
    juce::AudioFormatManager formats;formats.registerBasicFormats();std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if(!reader||reader->lengthInSamples<2||reader->sampleRate<8000||reader->sampleRate>384000)return juce::Result::fail("Sample audio cannot be read.");
    if(!fitsSampleMemory(reader->lengthInSamples,reader->sampleRate,(int)reader->numChannels))return juce::Result::fail("Sampler limit: 30 minutes and 512 MiB of decoded audio per file.");
    auto asset=std::make_shared<SampleAsset>();asset->file=file;asset->rate=reader->sampleRate;asset->frames=(int)reader->lengthInSamples;
    asset->audio.setSize(std::min(2,(int)reader->numChannels),asset->frames+4);asset->audio.clear();
    if(!reader->read(&asset->audio,0,asset->frames,0,true,true))return juce::Result::fail("Sample decoding failed.");
    for(int c=0;c<asset->audio.getNumChannels();++c)for(int i=0;i<asset->frames;++i){auto x=asset->audio.getSample(c,i);if(!std::isfinite(x))asset->audio.setSample(c,i,0);}
    auto in=file.createInputStream();if(!in)return juce::Result::fail("Cannot read sample fingerprint.");asset->hash=juce::SHA256(*in).toHexString();
    const int bins=std::min(8192,asset->frames);asset->peaks.resize((size_t)bins);
    for(int bin=0;bin<bins;++bin){float lo=0,hi=0;const int a=(int)((int64_t)bin*asset->frames/bins),b=(int)((int64_t)(bin+1)*asset->frames/bins);for(int c=0;c<asset->audio.getNumChannels();++c)for(int i=a;i<b;++i){auto x=asset->audio.getSample(c,i);lo=std::min(lo,x);hi=std::max(hi,x);}asset->peaks[(size_t)bin]={lo,hi};}
    result=std::move(asset);return juce::Result::ok();
}
double snapSamplePoint(const SampleAsset& a,double p){const int centre=std::clamp((int)std::llround(p*a.frames),0,a.frames-1),radius=(int)std::min(512.,a.rate*.01);int best=centre;double score=std::numeric_limits<double>::max();const auto* x=a.audio.getReadPointer(0);
    for(int i=std::max(1,centre-radius);i<std::min(a.frames-1,centre+radius);++i)if((x[i-1]<=0&&x[i]>=0)||(x[i-1]>=0&&x[i]<=0)){double v=std::abs(i-centre)+std::abs(x[i])*8;if(v<score){score=v;best=i;}}return best/(double)a.frames;}
std::vector<double> detectSampleSlices(const SampleAsset& a,double start,double end,double sensitivity,int limit){
    std::vector<double> points;start=std::clamp(start,0.,.99999);end=std::clamp(end,start+.00001,1.);limit=std::clamp(limit,1,128);points.push_back(start);
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
        juce::SynthesiserVoice::setCurrentPlaybackSampleRate(rate);envelope.setSampleRate(rate);
        const int size=juce::nextPowerOfTwo(std::max(256,(int)(2048*rate/48000.)));warp.configure(2,size,size/4,false);owner.latency=warp.outputLatency();
        filter.prepare({rate,512,2});filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);filter.reset();
    }
    void startNote(int note,float velocity,juce::SynthesiserSound*,int wheel)override{
        const auto& p=owner.settings();data=&owner.asset();pitch=note;level=1-p.velocity+p.velocity*velocity;pitchWheel=wheel;sourcePos=0;inputFraction=0;playedSource=0;elapsed=0;releaseAt=-1;released=false;filter.reset();
        begin=p.start*data->frames;finish=p.end*data->frames;
        if(owner.mode()==2){int index=note-p.sliceBase;if(index<0||index>=p.sliceCount){clearCurrentNote();data=nullptr;return;}begin=p.slices[(size_t)index]*data->frames;finish=(index+1<p.sliceCount?p.slices[(size_t)index+1]:p.end)*data->frames;}
        updateParameters();envelope.reset();envelope.noteOn();shaped.noteOn(p,getSampleRate());warp.reset();
        if(p.sync){Reader reader{*this,0};const int seek=warp.inputLatency();warp.seek(reader,seek,speed());sourcePos=seek;}
    }
    void stopNote(float,bool allowTailOff)override{
        if(!allowTailOff){envelope.reset();shaped.reset();clearCurrentNote();data=nullptr;return;}
        if(owner.mode()==0&&releaseAt<0)releaseAt=elapsed+owner.latencySamples();
    }
    void pitchWheelMoved(int value)override{pitchWheel=value;}
    void controllerMoved(int,int)override{}
    void renderNextBlock(juce::AudioBuffer<float>& output,int start,int count)override{
        if(!data||!isVoiceActive())return;updateParameters();const auto& p=owner.settings();
        // Fixed stack buffers bound work and avoid allocation at any host block size.
        for(int done=0;done<count&&data;){const int n=std::min(256,count-done);float left[256]{},right[256]{};float* target[]{left,right};
            if(p.sync){const double need=n*speed()+inputFraction;const int input=(int)need;inputFraction=need-input;Reader reader{*this,sourcePos};warp.process(reader,input,target,n);sourcePos+=input;}
            else{const double ratio=data->rate/getSampleRate()*std::pow(2.,semitones()/12.);for(int i=0;i<n;++i){left[i]=read(0,sourcePos);right[i]=read(1,sourcePos);sourcePos+=ratio;}}
            const int lag=owner.latencySamples();
            const bool looping=p.loop&&owner.mode()==0;
            for(int i=0;i<n&&data;++i,++elapsed){
                if(releaseAt>=0&&elapsed>=releaseAt&&!released){envelope.noteOff();shaped.noteOff();released=true;}
                if(elapsed<lag)continue;
                const auto at=elapsed-lag;
                if(!looping&&playedSource>=finish-begin){envelope.reset();shaped.reset();clearCurrentNote();data=nullptr;break;}
                const float linear=envelope.getNextSample(),bent=shaped.next();
                float gain=(shaped.curved()?bent:linear)*level;
                if(p.fadeIn>0){const float lin=(float)std::clamp(at/(p.fadeIn*getSampleRate()),0.,1.);gain*=std::abs(p.fadeInCurve)<1e-7f?lin:(float)sampleEnvelopeCurve(lin,p.fadeInCurve);}
                if(!looping&&p.fadeOut>0){const float lin=(float)std::clamp((finish-begin-playedSource)/(p.fadeOut*data->rate*speed()),0.,1.);gain*=std::abs(p.fadeOutCurve)<1e-7f?lin:(float)sampleEnvelopeCurve(lin,p.fadeOutCurve);}
                float l=left[i],r=right[i];if(p.filter){l=filter.processSample(0,l);r=filter.processSample(1,r);}l*=gain;r*=gain;
                output.addSample(0,start+done+i,l);if(output.getNumChannels()>1)output.addSample(1,start+done+i,r);
                playedSource+=data->rate/getSampleRate()*speed();
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
    double sourcePos=0,begin=0,finish=0,inputFraction=0,playedSource=0;int pitch=60,pitchWheel=8192;
    int64_t elapsed=0,releaseAt=-1;float level=1;bool released=false;
    double semitones()const{return (owner.mode()==2?0:pitch-owner.root())+owner.settings().transpose+owner.settings().fine*.01+(pitchWheel-8192)/8192.*2.;}
    double speed()const{return owner.settings().sync?owner.tempo()/owner.settings().sourceBpm:std::pow(2.,semitones()/12.);}
    void updateParameters(){const auto& p=owner.settings();envelope.setParameters({(float)p.attack,(float)p.decay,(float)p.sustain,(float)p.release});shaped.apply(p,getSampleRate());filter.setCutoffFrequency((float)std::clamp(p.cutoff,20.,getSampleRate()*.45));filter.setResonance((float)(.5+p.resonance*8));warp.setTransposeSemitones((float)semitones());}
    float raw(int channel,double pos)const{if(pos<begin||pos>=finish)return 0;const int i=std::clamp((int)pos,0,data->frames-1);const float frac=(float)(pos-i);auto* a=data->audio.getReadPointer(std::min(channel,data->audio.getNumChannels()-1));return a[i]+frac*(a[i+1]-a[i]);}
    float read(int channel,double input)const{
        const auto& p=owner.settings();double progress=input*(p.sync?data->rate/getSampleRate():1.);bool wrapped=false;
        double pos=p.reverse?finish-1-progress:begin+progress;
        const bool loop=p.loop&&owner.mode()==0;const double lo=std::max(begin,p.loopStart*data->frames),hi=std::min(finish,p.loopEnd*data->frames),length=hi-lo;
        if(loop&&length>1){if(!p.reverse&&pos>=hi){pos=lo+std::fmod(pos-hi,length);wrapped=true;}if(p.reverse&&pos<lo){pos=hi-1-std::fmod(lo-1-pos,length);wrapped=true;}}
        auto value=raw(channel,pos);if(!loop||length<=1)return value;
        const double fade=std::min(p.crossfade*data->rate,length*.5);if(fade<1)return value;
        // Symmetric crossfade keeps the exact loop period (no hidden shortening).
        double distance=fade,other=pos;
        if(!p.reverse&&pos>hi-fade){distance=hi-pos;other=lo+distance;}
        else if(!p.reverse&&wrapped&&pos<lo+fade){distance=pos-lo;other=hi-distance-1;}
        else if(p.reverse&&pos<lo+fade){distance=pos-lo;other=hi-distance-1;}
        else if(p.reverse&&wrapped&&pos>hi-fade){distance=hi-pos;other=lo+distance;}
        if(distance>=fade)return value;const float weight=(float)(.5+.5*std::sin(std::clamp(distance/fade,0.,1.)*juce::MathConstants<double>::halfPi));return weight*value+(1-weight)*raw(channel,other);
    }
    struct Reader {
        SampleVoice& voice;double position;
        struct Channel {const Reader& reader;int channel;float operator[](int index)const{return reader.voice.read(channel,reader.position+index);}};
        Channel operator[](int channel)const{return {*this,channel};}
    };
};
}
SampleSynth::SampleSynth(std::shared_ptr<const SampleAsset> a,const SamplerSettings& p,int root,bool drum):data(std::move(a)),fallback(p),rootNote(root),drumTrack(drum){fallback.normalise();for(int i=0;i<32;++i)addVoice(new SampleVoice(*this));addSound(new SampleSound(data));setNoteStealingEnabled(true);}
SampleSynth::~SampleSynth()=default;
bool SampleSynth::configure(const SamplerSettings& p,int root,double tempo){const auto& old=*parameters;const bool restart=old.mode!=p.mode||old.sync!=p.sync||old.reverse!=p.reverse||old.start!=p.start||old.end!=p.end||old.sliceCount!=p.sliceCount||old.slices!=p.slices;if(restart)allNotesOff(0,false);fallback=p;parameters=&fallback;rootNote=root;bpm=tempo;return restart;}
void SampleSynth::setCurrentPlaybackSampleRate(double rate){juce::Synthesiser::setCurrentPlaybackSampleRate(rate);}
void SampleSynth::noteOn(int channel,int note,float velocity){
    if(mode()==2&&(note<settings().sliceBase||note>=settings().sliceBase+settings().sliceCount))return;
    for(int tries=0;tries<32;++tries){int active=0;juce::SynthesiserVoice* oldest=nullptr;for(int i=0;i<getNumVoices();++i){auto* voice=getVoice(i);if(!voice->isVoiceActive())continue;++active;if(!oldest||voice->wasStartedBefore(*oldest))oldest=voice;}if(active<settings().polyphony||!oldest)break;oldest->stopNote(0,false);}
    juce::Synthesiser::noteOn(channel,note,velocity);
}
}
