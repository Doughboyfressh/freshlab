#include "Core/Dsp.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace fresh;
static int checks=0;
void require(bool condition,const char* label){++checks;if(!condition)throw std::runtime_error(label);}
double energy(Synth& s,Patch p,int frames,const Sample* sample=nullptr){double sum=0;for(int i=0;i<frames;++i){float l,r;s.render(l,r,p,sample);require(std::isfinite(l)&&std::isfinite(r),"synth finite");sum+=l*l+r*r;}return sum/frames;}
void wav(const std::string& path,const std::vector<float>& data){
    std::ofstream f(path,std::ios::binary);auto put=[&](uint32_t x,int n){for(int i=0;i<n;++i)f.put(char((x>>(8*i))&255));};
    f.write("RIFF",4);put(36+uint32_t(data.size()*2),4);f.write("WAVEfmt ",8);put(16,4);put(1,2);put(2,2);put(44100,4);put(44100*4,4);put(4,2);put(16,2);f.write("data",4);put(uint32_t(data.size()*2),4);
    for(float x:data)put(uint16_t(int16_t(std::clamp(x,-1.0f,1.0f)*32767)),2);
}
int main(int argc,char** argv){try{
    require(std::abs(noteHz(69)-440)<1e-9,"MIDI tuning");
    for(double rate:{22050,44100,48000,96000,192000}){
        Synth s;s.prepare(rate);Patch p;p.drop=0;
        require(energy(s,p,100)==0,"idle silence");s.noteOn(33,1,1,p,nullptr);
        require(energy(s,p,int(rate*.1))>.001,"808 audible");
        s.allOff();require(energy(s,p,100)==0,"panic silence");
        p.mode=1;p.attack=.001f;s.noteOn(60,1,1,p,nullptr);s.noteOn(67,1,1,p,nullptr);
        require(energy(s,p,int(rate*.05))>.0001,"poly synth audible");
        s.noteOff(60,1,p);s.noteOff(67,1,p);require(energy(s,p,int(rate*3))<.004,"release decays");
        require(energy(s,p,100)==0,"release becomes silence");
        Effects fx;fx.prepare(rate);FxPatch fp;fp.bypass=true;float l=.12f,r=-.45f;for(int i=0;i<=fx.latency();++i){l=.12f;r=-.45f;fx.process(l,r,fp);}require(l==.12f&&r==-.45f,"latency-aligned bypass exact");
        fp.bypass=false;fp.drive=30;fp.distMix=1;fp.feedback=.85f;fp.delayMix=1;fp.reverb=1;
        fp.lowGain=18;fp.mid1Gain=18;fp.mid1Q=8;fp.mid2Gain=18;fp.highGain=18;fp.output=6;
        fx.configure(fp);
        for(int shape=0;shape<3;++shape){fp.distType=shape;for(int i=0;i<int(rate*.3);++i){l=std::sin(float(i)*.2f)*10;r=-l;fx.process(l,r,fp);require(std::isfinite(l)&&std::isfinite(r)&&std::abs(l)<=1&&std::abs(r)<=1,"FX stable bounded");}}
    }
    {
        Synth s;s.prepare(44100);Patch p;p.mode=2;p.attack=.001f;
        s.noteOn(60,1,1,p,nullptr);require(energy(s,p,100)==0,"missing sample safe");
        Sample a;a.left.resize(4410);a.right.resize(4410);for(int i=0;i<4410;++i)a.left[size_t(i)]=a.right[size_t(i)]=std::sin(float(i)*.05f)*.6f;
        s.noteOn(60,1,1,p,&a);require(energy(s,p,200,&a)>.001,"sampler plays");
        s.noteOff(60,1,p);require(energy(s,p,100,&a)>.001,"one-shot ignores note off");
        energy(s,p,5000,&a);require(energy(s,p,100,&a)==0,"sample ends safely");
        p.reverse=true;s.noteOn(60,1,1,p,&a);require(energy(s,p,300,&a)>.001,"reverse plays");
        s.allOff();p.mode=1;p.attack=.001f;p.release=.02f;s.noteOn(60,1,1,p,nullptr);s.controller(64,127,1,p);s.noteOff(60,1,p);
        require(energy(s,p,10000)>.0001,"sustain holds");s.controller(64,0,1,p);energy(s,p,15000);require(energy(s,p,100)==0,"sustain release");
        s.noteOn(60,1,1,p,nullptr);energy(s,p,1000);p.mode=0;require(energy(s,p,100)==0,"mode switch clears old notes");
        p.drop=0;p.attack=.001f;s.noteOn(69,1,1,p,nullptr);s.pitchBend(16383,1);
        float l,r;int crossings=0;float previous=0;for(int i=0;i<44100;++i){s.render(l,r,p,nullptr);if(l>0&&previous<=0)++crossings;previous=l;}
        require(crossings>=492&&crossings<=496,"pitch bend two semitones");
    }
    if(argc>1){
        Synth synth;synth.prepare(44100);Effects fx;fx.prepare(44100);Patch p;FxPatch fp;std::vector<float> audio;audio.reserve(44100*12*2);
        const int notes[]{33,33,36,31,33,40,36,31};
        for(int i=0;i<44100*12;++i){
            if(i==44100*6){synth.allOff();p.mode=1;p.attack=.1f;p.release=.8f;p.cutoff=4200;fp.reverb=.5f;fp.delayMix=.2f;}
            if(i<44100*6&&i%33075==0){p.decay=1.3f;fp.drive=13;fp.distMix=.35f;synth.noteOn(notes[(i/33075)%8],.95f,1,p,nullptr);}
            if(i==44100*6||i==44100*9){synth.allOff();for(int note:{48,55,60,63})synth.noteOn(note+(i==44100*9?2:0),.7f,1,p,nullptr);}
            fx.configure(fp);float l,r;synth.render(l,r,p,nullptr);fx.process(l,r,fp);audio.push_back(l);audio.push_back(r);
        }
        wav(argv[1],audio);
    }
    std::cout<<"PASS: "<<checks<<" DSP assertions across 5 sample rates\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
