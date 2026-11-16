#include "Core/Speech.h"
#include <fstream>
#include <iostream>
void writeWav(const std::string& path,const fresh::Sample& s){
    std::ofstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Cannot open output WAV");
    auto put=[&](uint32_t x,int n){for(int i=0;i<n;++i)f.put(char((x>>(8*i))&255));};
    f.write("RIFF",4);put(36+uint32_t(s.left.size()*4),4);f.write("WAVEfmt ",8);put(16,4);put(1,2);put(2,2);put(uint32_t(s.rate),4);put(uint32_t(s.rate)*4,4);put(4,2);put(16,2);f.write("data",4);put(uint32_t(s.left.size()*4),4);
    for(size_t i=0;i<s.left.size();++i)for(float v:{s.left[i],s.right[i]})put(uint16_t(int16_t(std::clamp(v,-1.0f,1.0f)*32767)),2);
    if(!f)throw std::runtime_error("WAV write failed");
}
int main(int argc,char** argv){try{
    if(argc<3){std::cerr<<"Usage: freshlab-render-voice output.wav \"phrase\" [character 0..3] [pitch st] [formant .65..1.6] [robot 0..1]\n";return 2;}
    fresh::VoiceDesign p;if(argc>3)p.character=std::clamp(std::stoi(argv[3]),0,3);if(argc>4)p.pitch=std::clamp(std::stof(argv[4]),-24.0f,24.0f);if(argc>5)p.formant=std::clamp(std::stof(argv[5]),.65f,1.6f);if(argc>6)p.robot=std::clamp(std::stof(argv[6]),0.0f,1.0f);
    const auto s=fresh::Speech::render(argv[2],p);writeWav(argv[1],s);std::cout<<"Wrote "<<argv[1]<<" / "<<s.left.size()/s.rate<<" seconds / original CC0 voice\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
