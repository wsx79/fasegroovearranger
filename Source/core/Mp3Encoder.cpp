// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Mp3Encoder.h"
extern "C" {
#include "../../ThirdParty/shine/src/lib/layer3.h"
}
namespace fga {
Mp3Encoder::Mp3Encoder(){shine_config_t c{};c.wave.channels=PCM_STEREO;c.wave.samplerate=44100;shine_set_config_mpeg_defaults(&c.mpeg);c.mpeg.bitr=320;c.mpeg.mode=STEREO;handle=shine_initialise(&c);}
Mp3Encoder::~Mp3Encoder(){if(handle)shine_close((shine_t)handle);}
int Mp3Encoder::frameSize()const{return handle?shine_samples_per_pass((shine_t)handle):0;}
std::vector<std::uint8_t> Mp3Encoder::encode(std::int16_t* pcm){int size=0;auto* p=shine_encode_buffer_interleaved((shine_t)handle,pcm,&size);return size>0?std::vector<std::uint8_t>(p,p+size):std::vector<std::uint8_t>{};}
std::vector<std::uint8_t> Mp3Encoder::flush(){int size=0;auto* p=shine_flush((shine_t)handle,&size);return size>0?std::vector<std::uint8_t>(p,p+size):std::vector<std::uint8_t>{};}
}
