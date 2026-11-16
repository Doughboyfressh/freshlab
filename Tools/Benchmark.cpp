#include "Core/Dsp.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using Clock=std::chrono::steady_clock;
int main(int argc,char** argv){try{
    std::ofstream file;if(argc>1)file.open(argv[1]);std::ostream& out=argc>1?file:std::cout;if(!out)throw std::runtime_error("Cannot write benchmark report");
    out<<std::setprecision(8)<<"{\n  \"note\": \"Synthetic single-instance measurements; not a host or hardware certification\",\n  \"results\": [\n";bool first=true;
    for(double rate:{44100.,48000.,96000.})for(int block:{32,64,256})for(int mode=0;mode<3;++mode){
        fresh::Effects fx;fx.prepare(rate);fresh::FxPatch p;fresh::Synth synth;fresh::Patch instrument;instrument.mode=1;instrument.attack=.001f;instrument.decay=8;
        if(mode==1){p.drive=20;p.distMix=.7f;p.reverb=.4f;p.dynamicRange=9;p.lowGain=6;}
        if(mode==2){p.vocalQuality=1;p.harmony=.8f;p.doubles=.5f;p.tuneAmount=.7f;p.drive=12;p.distMix=.3f;}
        synth.prepare(rate);for(int note=48;note<64;++note)synth.noteOn(note,.6f,1,instrument,nullptr);
        fx.configure(p);std::vector<double> times;times.reserve(size_t(rate*1.0/block)+1);double energy=0;
        for(int frame=0;frame<int(rate*1.25);frame+=block){const auto begin=Clock::now();
            for(int i=0;i<block;++i){float l,r;synth.render(l,r,instrument,nullptr);fx.process(l,r,p);if(!std::isfinite(l)||!std::isfinite(r))throw std::runtime_error("Non-finite output");energy+=l*l+r*r;}
            const double seconds=std::chrono::duration<double>(Clock::now()-begin).count();if(frame>rate*.25)times.push_back(seconds);}
        double sum=0;for(auto t:times)sum+=t;std::sort(times.begin(),times.end());const double budget=block/rate;
        if(!first)out<<",\n";first=false;
        out<<"    {\"sample_rate\": "<<rate<<", \"block_size\": "<<block<<", \"scenario\": \""<<(mode==0?"16_voice_clean":(mode==1?"16_voice_fx":"16_voice_spectral"))<<"\", \"latency_samples\": "<<fx.latency()<<", \"average_budget_fraction\": "<<sum/times.size()/budget<<", \"p99_budget_fraction\": "<<times[std::min(times.size()-1,size_t(times.size()*.99))]/budget<<", \"max_budget_fraction\": "<<times.back()/budget<<", \"energy\": "<<energy<<"}";
    }
    out<<"\n  ]\n}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
