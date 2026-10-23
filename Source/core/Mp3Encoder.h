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
    // Shine's encoder delay in samples (measured; the decoder adds its own 529).
    static constexpr int encoderDelay=528;
    static constexpr int decoderDelay=529;
    static constexpr int infoFrameBytes=1044; // one 320 kbps / 44.1 kHz frame
    // Xing "Info" + LAME tag frame for gapless playback: decoders skip the encoder
    // delay and the final padding, so the MP3 lines up sample-exactly with the WAV.
    // audioFrames = encode() calls (one frame each), sourceSamples = real input length.
    static std::vector<std::uint8_t> infoFrame(std::uint32_t audioFrames,std::uint32_t totalBytes,std::int64_t sourceSamples);
private:void* handle=nullptr;
};
}
