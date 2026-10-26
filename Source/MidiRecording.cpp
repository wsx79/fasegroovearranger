// Copyright (C) 2026 Fabio 'Pitch' Serra
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MidiRecording.h"
namespace fga {
void MidiCapture::push(juce::AbstractFifo& fifo,std::array<LiveMidiEvent,inputCapacity>& events,LiveMidiEvent e){auto slot=fifo.write(1);if(slot.blockSize1)events[(size_t)slot.startIndex1]=e;else if(slot.blockSize2)events[(size_t)slot.startIndex2]=e;else {overflow=true;panic=true;}}
void MidiCapture::handleIncomingMidiMessage(juce::MidiInput*,const juce::MidiMessage& message){if(message.getRawDataSize()>3||message.getRawDataSize()<1)return;LiveMidiEvent e;e.size=message.getRawDataSize();std::copy_n(message.getRawData(),e.size,e.bytes.begin());e.time=message.getTimeStamp()>0?message.getTimeStamp():juce::Time::getMillisecondCounterHiRes()*.001;e.offset=-1;push(deviceFifo,*deviceEvents,e);}
void MidiCapture::enqueue(int track,const juce::MidiMessage& message,bool recordable,int offset){if(message.getRawDataSize()>3||message.getRawDataSize()<1)return;LiveMidiEvent e;e.track=track;e.size=message.getRawDataSize();std::copy_n(message.getRawData(),e.size,e.bytes.begin());e.time=juce::Time::getMillisecondCounterHiRes()*.001;e.recordable=recordable;e.offset=offset;push(uiFifo,*uiEvents,e);}
bool MidiCapture::pop(LiveMidiEvent& e){for(auto* fifo:{&deviceFifo,&uiFifo}){auto read=fifo->read(1);auto& events=*(fifo==&deviceFifo?deviceEvents:uiEvents);if(read.blockSize1){e=events[(size_t)read.startIndex1];return true;}if(read.blockSize2){e=events[(size_t)read.startIndex2];return true;}}return false;}
void MidiCapture::append(CapturedMidi e){auto write=recordFifo.write(1);if(write.blockSize1)(*recorded)[(size_t)write.startIndex1]=e;else if(write.blockSize2)(*recorded)[(size_t)write.startIndex2]=e;else overflow=true;}
void MidiCapture::drain(std::vector<CapturedMidi>& events){auto read=recordFifo.read(recordFifo.getNumReady());for(int i=0;i<read.blockSize1;++i)events.push_back((*recorded)[(size_t)(read.startIndex1+i)]);for(int i=0;i<read.blockSize2;++i)events.push_back((*recorded)[(size_t)(read.startIndex2+i)]);}
void MidiCapture::resetRecordQueue(){recordFifo.reset();overflow=false;elapsed=0;}
namespace {
struct Held {double beat=0,elapsed=0;int velocity=0;bool released=false;};
void appendNote(MidiTake& take,const Held& h,int pitch,double end,double grid){if(h.velocity==0)return;double duration=std::max(1./960.,end-h.elapsed),position=grid>0?std::round(h.beat/grid)*grid:h.beat;position=std::max(0.,position);
    if(take.looping&&take.loopEnd>take.loopStart&&position>=take.loopEnd)position=take.loopStart+std::fmod(position-take.loopStart,take.loopEnd-take.loopStart);
    for(int segments=0;duration>1.e-8&&segments<8192;++segments){double length=duration;if(take.looping&&position<take.loopEnd)length=std::min(length,take.loopEnd-position);if(length>1.e-8)take.notes.push_back({position,length,pitch,h.velocity});duration-=length;if(!take.looping||length<=1.e-8)break;position=take.loopStart;}
}
}
void assembleMidiTake(MidiTake& take,double grid){take.notes.clear();std::array<Held,16*128> held{};std::array<bool,16> pedal{};
    auto finish=[&](int ch,int note,double time){auto& h=held[(size_t)(ch*128+note)];appendNote(take,h,note,time,grid);h={};};
    std::stable_sort(take.events.begin(),take.events.end(),[](const auto& a,const auto& b){return a.elapsed<b.elapsed;});
    for(const auto& e:take.events){int ch=e.status&15,type=e.status&0xf0,note=std::clamp(e.note,0,127);auto& h=held[(size_t)(ch*128+note)];
        if(type==0x90&&e.value>0){if(h.velocity)finish(ch,note,e.elapsed);h={e.beat,std::max(0.,e.elapsed),e.value,false};}
        else if(type==0x80||(type==0x90&&e.value==0)){if(pedal[(size_t)ch])h.released=true;else finish(ch,note,e.elapsed);}
        else if(type==0xb0&&note==64){pedal[(size_t)ch]=e.value>=64;if(!pedal[(size_t)ch])for(int n=0;n<128;++n)if(held[(size_t)(ch*128+n)].released)finish(ch,n,e.elapsed);}
        else if(type==0xb0&&(note==120||note==123)){for(int n=0;n<128;++n)finish(ch,n,e.elapsed);pedal[(size_t)ch]=false;}
    }
    for(int c=0;c<16;++c)for(int n=0;n<128;++n)finish(c,n,take.duration);
    std::sort(take.notes.begin(),take.notes.end(),[](const auto& a,const auto& b){return a.beat<b.beat;});
}
std::vector<std::pair<double,double>> recordedRanges(const MidiTake& take){std::vector<std::pair<double,double>> ranges;if(take.duration<=0)return ranges;double start=take.start,length=take.duration;if(!take.looping||take.loopEnd<=take.loopStart){ranges.push_back({start,start+length});return ranges;}const double first=std::max(0.,take.loopEnd-start);ranges.push_back({start,start+std::min(first,length)});length-=std::min(first,length);if(length>0)ranges.push_back({take.loopStart,take.loopStart+std::min(length,take.loopEnd-take.loopStart)});return ranges;}
bool applyMidiTake(Project& p,const MidiTake& take,bool overdub){if(take.track<0||take.track>=(int)p.tracks.size()||p.tracks[(size_t)take.track].locked)return false;auto& track=p.tracks[(size_t)take.track];bool edited=false;const auto ranges=recordedRanges(take);
    if(!overdub)for(size_t ci=0;ci<track.clips.size();++ci){auto& clip=track.clips[ci];if(clip.locked)continue;const double offset=p.clipStart(take.track,(int)ci);std::vector<Note> notes;bool clipEdited=false;
        for(auto note:clip.notes){std::vector<Note> parts{note};for(auto [a,b]:ranges){std::vector<Note> next;for(auto part:parts){double begin=part.start+offset,end=begin+part.length;if(end<=a||begin>=b){next.push_back(part);continue;}edited=true;clipEdited=true;if(begin<a){auto left=part;left.length=a-begin;next.push_back(left);}if(end>b){auto right=part;right.start=b-offset;right.length=end-b;next.push_back(right);}}parts=std::move(next);}notes.insert(notes.end(),parts.begin(),parts.end());}
        clip.notes=std::move(notes);if(clipEdited){clip.loopSource.clear();clip.loopLength=0;}}
    for(const auto& recorded:take.notes){double position=std::clamp(recorded.beat,0.,std::max(0.,p.length()-1./960.));double remaining=recorded.length;
        for(int segments=0;remaining>1.e-7&&segments<4096;++segments){int ci=p.clipAtBeat(take.track,position+1.e-8);if(ci<0&&track.freeTimeline){Clip c;c.position=position;double end=std::min(p.length(),position+16);for(size_t existing=0;existing<track.clips.size();++existing){const double start=p.clipStart(take.track,(int)existing);if(start>position+1.e-8)end=std::min(end,start);}c.span=std::max(1./960.,end-position);c.seed=1;if(track.clips.size()>=4096)break;auto insert=std::lower_bound(track.clips.begin(),track.clips.end(),position,[](const Clip& clip,double beat){return clip.position<beat;});ci=(int)(insert-track.clips.begin());track.clips.insert(insert,std::move(c));}if(ci<0)break;
            const double offset=p.clipStart(take.track,ci),length=p.clipLength(take.track,ci),n=std::min(remaining,offset+length-position);if(n<=1.e-8)break;auto& clip=track.clips[(size_t)ci];
            if(!clip.locked){Note note{position-offset,n,recorded.pitch,recorded.velocity,isDrumTrack(take.track)?take.track-4:0};auto same=std::find_if(clip.notes.begin(),clip.notes.end(),[&](const Note& old){return old.pitch==note.pitch&&std::abs(old.start-note.start)<1./1920.;});if(same==clip.notes.end())clip.notes.push_back(note);else *same=note;clip.loopSource.clear();clip.loopLength=0;edited=true;}
            remaining-=n;position+=n;if(position>=p.length()-1.e-8)break;
        }
    }
    if(edited)p.normalise();return edited;
}
}
