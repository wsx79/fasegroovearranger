// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../Source/core/Mp3Encoder.h"
#include <cmath>
#include <fstream>
#include <iostream>
int main(int argc,char** argv){fga::Mp3Encoder encoder;if(!encoder.valid())return 1;std::vector<std::uint8_t> output;std::vector<std::int16_t> pcm((size_t)encoder.frameSize()*2);int frames=0;for(int at=0;at<44100*2;at+=encoder.frameSize()){for(int i=0;i<encoder.frameSize();++i){double t=(at+i)/44100.;pcm[(size_t)i*2]=(std::int16_t)(8000*std::sin(6.283185307179586*440*t));pcm[(size_t)i*2+1]=(std::int16_t)(8000*std::sin(6.283185307179586*880*t));}auto bytes=encoder.encode(pcm.data());output.insert(output.end(),bytes.begin(),bytes.end());frames++;}auto bytes=encoder.flush();output.insert(output.end(),bytes.begin(),bytes.end());if(output.size()<70000||frames<70)return 1;if(argc>1){std::ofstream f(argv[1],std::ios::binary);f.write((const char*)output.data(),(std::streamsize)output.size());}std::cout<<"Bundled MP3 encoder: "<<output.size()<<" bytes\n";}
