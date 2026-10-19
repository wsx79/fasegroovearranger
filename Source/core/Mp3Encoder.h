// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <cstdint>
#include <vector>
namespace fga {
// Bundled Shine encoder: 320 kbps, stereo, 44.1 kHz. Each input is exactly frameSize frames.
class Mp3Encoder {
public:
    Mp3Encoder();
    ~Mp3Encoder();
    Mp3Encoder(const Mp3Encoder&)=delete;
    Mp3Encoder& operator=(const Mp3Encoder&)=delete;
    bool valid()const{return handle!=nullptr;}
    int frameSize()const;
    std::vector<std::uint8_t> encode(std::int16_t* interleaved);
    std::vector<std::uint8_t> flush();
private:void* handle=nullptr;
};
}
