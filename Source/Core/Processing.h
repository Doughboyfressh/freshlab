#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
namespace fresh {
class StereoDelay {
    std::array<std::vector<float>,2> memory;
    size_t position=0;
public:
    void prepare(int samples){for(auto& c:memory)c.assign(size_t(std::max(0,samples)),0);position=0;}
    void reset(){for(auto& c:memory)std::fill(c.begin(),c.end(),0);position=0;}
    void process(float& l,float& r){if(memory[0].empty())return;
        const float a=memory[0][position],b=memory[1][position];memory[0][position]=l;memory[1][position]=r;l=a;r=b;
        if(++position==memory[0].size())position=0;}
};
// Polyphase 4x interpolation, waveshaping, then low-pass/decimation.
// Two 65-tap FIRs contribute 16 base-rate samples of delay.
class OversampledDistortion {
    std::array<float,65> kernel{},high{};std::array<float,17> input{};
    size_t inputWrite=0,highWrite=0;
    static float shape(float x,int kind){if(kind==0)return std::tanh(x);if(kind==1)return std::clamp(x,-1.0f,1.0f);
        x=std::fmod(x+1,4.0f);if(x<0)x+=4;return 1-std::abs(x-2);}
public:
    static constexpr int latency=16;
    OversampledDistortion(){double sum=0;for(int i=0;i<65;++i){const double x=i-32,pi=3.14159265358979323846;
        const double sinc=x==0?.24:std::sin(2*pi*.12*x)/(pi*x);
        const double window=.42-.5*std::cos(2*pi*i/64)+.08*std::cos(4*pi*i/64);
        kernel[size_t(i)]=float(sinc*window);sum+=sinc*window;}for(auto& k:kernel)k/=float(sum);}
    void reset(){input={};high={};inputWrite=highWrite=0;}
    float process(float x,float drive,int kind){input[inputWrite]=x;float out=0;
        for(int phase=0;phase<4;++phase){float up=0;
            for(int tap=phase,j=0;tap<65;tap+=4,++j)up+=kernel[size_t(tap)]*input[(inputWrite+input.size()-size_t(j))%input.size()]*4;
            high[highWrite]=shape(up*drive,kind);
            if(phase==0)for(size_t tap=0;tap<kernel.size();++tap)out+=kernel[tap]*high[(highWrite+high.size()-tap)%high.size()];
            if(++highWrite==high.size())highWrite=0;}
        if(++inputWrite==input.size())inputWrite=0;return out;}
};
// Stereo-linked sample-peak limiter. A preallocated maximum queue anticipates
// peaks by 3 ms, with instant reduction and smooth release. Not a true-peak meter.
class LookaheadLimiter {
    StereoDelay audio;std::vector<float> peaks;std::vector<unsigned long long> times;
    size_t head=0,tail=0;unsigned long long clock=0;int lookahead=0;float gain=1,release=0;
public:
    void prepare(double rate){lookahead=std::max(1,int(std::round(rate*.003)));audio.prepare(lookahead);
        peaks.assign(size_t(lookahead+2),0);times.assign(peaks.size(),0);setRelease(80,rate);reset();}
    int latency()const{return lookahead;}
    void setRelease(float ms,double rate){release=float(std::exp(-1/(std::max(1.0f,ms)*.001*rate)));}
    void reset(){audio.reset();head=tail=0;clock=0;gain=1;}
    void process(float& l,float& r,float ceiling){const float peak=std::max(std::abs(l),std::abs(r));const auto cap=peaks.size();
        while(head!=tail&&times[head]+static_cast<unsigned>(lookahead)<clock)head=(head+1)%cap;
        while(head!=tail&&peaks[(tail+cap-1)%cap]<=peak)tail=(tail+cap-1)%cap;
        peaks[tail]=peak;times[tail]=clock++;tail=(tail+1)%cap;
        const float wanted=std::min(1.0f,ceiling/std::max(1.0e-12f,peaks[head]));gain=wanted<gain?wanted:wanted+(gain-wanted)*release;
        audio.process(l,r);l*=gain;r*=gain;}
};
}
