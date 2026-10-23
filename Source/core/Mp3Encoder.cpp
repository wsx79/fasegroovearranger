// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Mp3Encoder.h"
#include <algorithm>
extern "C" {
#include "../../ThirdParty/shine/src/lib/layer3.h"
}
namespace fga {
Mp3Encoder::Mp3Encoder(){shine_config_t c{};c.wave.channels=PCM_STEREO;c.wave.samplerate=44100;shine_set_config_mpeg_defaults(&c.mpeg);c.mpeg.bitr=320;c.mpeg.mode=STEREO;handle=shine_initialise(&c);}
Mp3Encoder::~Mp3Encoder(){if(handle)shine_close((shine_t)handle);}
int Mp3Encoder::frameSize()const{return handle?shine_samples_per_pass((shine_t)handle):0;}
std::vector<std::uint8_t> Mp3Encoder::encode(std::int16_t* pcm){int size=0;auto* p=shine_encode_buffer_interleaved((shine_t)handle,pcm,&size);return size>0?std::vector<std::uint8_t>(p,p+size):std::vector<std::uint8_t>{};}
std::vector<std::uint8_t> Mp3Encoder::infoFrame(std::uint32_t audioFrames,std::uint32_t totalBytes,std::int64_t sourceSamples){
    std::vector<std::uint8_t> f((size_t)infoFrameBytes,0);
    f[0]=0xff;f[1]=0xfb;f[2]=0xe0;f[3]=0x04;              // MPEG-1 L3, 320 kbps, 44.1 kHz, stereo, as the stream
    auto be32=[&f](size_t at,std::uint32_t v){for(int i=0;i<4;++i)f[at+(size_t)i]=(std::uint8_t)(v>>(24-8*i));};
    size_t x=4+32;                                         // after the stereo side info
    f[x]='I';f[x+1]='n';f[x+2]='f';f[x+3]='o';be32(x+4,0x0f);be32(x+8,audioFrames);be32(x+12,totalBytes);
    for(int i=0;i<100;++i)f[x+16+(size_t)i]=(std::uint8_t)(i*256/100);   // linear TOC (CBR)
    be32(x+116,0);
    const size_t l=x+120;const char version[]="LAME3.100";for(int i=0;i<9;++i)f[l+(size_t)i]=(std::uint8_t)version[i];
    f[l+9]=0x01;f[l+20]=255;                               // CBR, 320 kbps
    const std::int64_t padding=std::clamp<std::int64_t>((std::int64_t)audioFrames*1152-encoderDelay-sourceSamples,0,4095);
    f[l+21]=(std::uint8_t)(encoderDelay>>4);f[l+22]=(std::uint8_t)(((encoderDelay&15)<<4)|(int)(padding>>8));f[l+23]=(std::uint8_t)(padding&255);
    be32(l+28,totalBytes);
    std::uint16_t crc=0;for(size_t i=0;i<190;++i){crc^=f[i];for(int b=0;b<8;++b)crc=(crc&1)?(std::uint16_t)((crc>>1)^0xa001):(std::uint16_t)(crc>>1);}
    f[190]=(std::uint8_t)(crc>>8);f[191]=(std::uint8_t)crc;
    return f;
}
std::vector<std::uint8_t> Mp3Encoder::flush(){int size=0;auto* p=shine_flush((shine_t)handle,&size);return size>0?std::vector<std::uint8_t>(p,p+size):std::vector<std::uint8_t>{};}
}
