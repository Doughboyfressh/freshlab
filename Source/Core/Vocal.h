#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
namespace fresh {
// Dual moving delay taps; all buffers are allocated in prepare().
class VocalLayers {
    double rate=44100;std::array<std::vector<float>,2> history;size_t write=0;
    std::array<double,3> phase{.25,.15,.65};
    double windowSamples=2646,periodSamples=275.625;int sinceCrossing=0;float previous=0;
    float read(int channel,double delay)const {
        const auto& h=history[size_t(channel)];double index=double(write)+h.size()-delay;
        index=std::fmod(index,double(h.size()));if(index<0)index+=h.size();
        const auto n=size_t(index);const float f=float(index-n);return h[n]*(1-f)+h[(n+1)%h.size()]*f;
    }
    float shifted(int channel,int tap,double base)const {
        const double p=phase[size_t(tap)],q=p<.5?p+.5:p-.5;
        const double w=std::pow(std::sin(3.141592653589793*p),2);
        return float(read(channel,base+p*windowSamples)*w+read(channel,base+q*windowSamples)*(1-w));
    }
public:
    void prepare(double sr){rate=sr;for(auto& h:history)h.assign(size_t(sr*.16)+8,0);reset();}
    void reset(){for(auto& h:history)std::fill(h.begin(),h.end(),0);write=0;phase={.25,.15,.65};windowSamples=rate*.06;periodSamples=rate/160;sinceCrossing=0;previous=0;}
    void process(float& l,float& r,float doubles,float harmony,float semitones,float width){
        history[0][write]=l;history[1][write]=r;
        // Align the grain length to an estimated input period for steadier monophonic notes.
        const float mono=(l+r)*.5f;if(sinceCrossing<int(rate))++sinceCrossing;
        if(mono>0&&previous<=0){if(sinceCrossing>rate/1200&&sinceCrossing<rate/35)periodSamples+=.2*(sinceCrossing-periodSamples);sinceCrossing=0;}
        previous=mono;
        const double wanted=periodSamples*std::max(1.0,std::round(rate*.06/periodSamples));windowSamples+=.0005*(wanted-windowSamples);
        if(doubles>0){l+=shifted(0,1,rate*.015)*doubles*.65f;r+=shifted(1,2,rate*.023)*doubles*.65f;}
        if(harmony>0){l+=shifted(0,0,rate*.008)*harmony*.7f;r+=shifted(1,0,rate*.008)*harmony*.7f;}
        const float mid=(l+r)*.5f,side=(l-r)*.5f*std::clamp(width,0.0f,2.0f);l=mid+side;r=mid-side;
        const double ratios[]{std::exp2(semitones/12),std::exp2(.07/12),std::exp2(-.09/12)};
        for(int i=0;i<3;++i){phase[size_t(i)]+=(1-ratios[i])/windowSamples;phase[size_t(i)]-=std::floor(phase[size_t(i)]);}
        if(++write==history[0].size())write=0;
    }
};
} // namespace fresh
