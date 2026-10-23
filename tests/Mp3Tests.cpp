// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/core/Mp3Encoder.h"
#include <cmath>
#include <fstream>
#include <iostream>
static bool gaplessInfoFrame(){
    // 30000 source samples need ceil((30000+528+529)/1152)=27 frames; padding = 27*1152-528-30000.
    const auto f=fga::Mp3Encoder::infoFrame(27,123456,30000);
    if((int)f.size()!=fga::Mp3Encoder::infoFrameBytes||f[0]!=0xff||f[1]!=0xfb||f[2]!=0xe0)return false;
    if(std::string(f.begin()+36,f.begin()+40)!="Info"||std::string(f.begin()+156,f.begin()+165)!="LAME3.100")return false;
    const int frames=(f[44]<<24)|(f[45]<<16)|(f[46]<<8)|f[47],delay=(f[177]<<4)|(f[178]>>4),padding=((f[178]&15)<<8)|f[179];
    if(frames!=27||delay!=fga::Mp3Encoder::encoderDelay||padding!=27*1152-528-30000)return false;
    std::uint16_t crc=0;for(int i=0;i<190;++i){crc^=f[(size_t)i];for(int b=0;b<8;++b)crc=(crc&1)?(std::uint16_t)((crc>>1)^0xa001):(std::uint16_t)(crc>>1);}
    return f[190]==(crc>>8)&&f[191]==(crc&255);
}
int main(int argc,char** argv){if(!gaplessInfoFrame()){std::cerr<<"gapless Info/LAME frame\n";return 1;}fga::Mp3Encoder encoder;if(!encoder.valid())return 1;std::vector<std::uint8_t> output;std::vector<std::int16_t> pcm((size_t)encoder.frameSize()*2);int frames=0;for(int at=0;at<44100*2;at+=encoder.frameSize()){for(int i=0;i<encoder.frameSize();++i){double t=(at+i)/44100.;pcm[(size_t)i*2]=(std::int16_t)(8000*std::sin(6.283185307179586*440*t));pcm[(size_t)i*2+1]=(std::int16_t)(8000*std::sin(6.283185307179586*880*t));}auto bytes=encoder.encode(pcm.data());output.insert(output.end(),bytes.begin(),bytes.end());frames++;}auto bytes=encoder.flush();output.insert(output.end(),bytes.begin(),bytes.end());if(output.size()<70000||frames<70)return 1;if(argc>1){std::ofstream f(argv[1],std::ios::binary);f.write((const char*)output.data(),(std::streamsize)output.size());}std::cout<<"Bundled MP3 encoder: "<<output.size()<<" bytes\n";}
