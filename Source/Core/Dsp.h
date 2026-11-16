#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include "Vocal.h"

namespace fresh {
constexpr double pi = 3.14159265358979323846;
inline float db(float x) { return std::pow(10.0f, x / 20.0f); }
inline double noteHz(int note) { return 440.0 * std::exp2((note - 69) / 12.0); }
inline float finite(float x) { return std::isfinite(x) ? x : 0.0f; }

struct Patch {
    int mode = 0, wave = 1, root = 60;
    float attack = 0.005f, decay = 1.2f, sustain = 0.65f, release = 0.15f;
    float glide = 0.04f, drop = 12.0f, detune = 7.0f, cutoff = 8000.0f;
    float samplePitch = 0.0f, sampleStart = 0.0f;
    bool reverse = false, oneShot = true;
    float bassSub=1,bassPunch=0,bassClick=0,bassBuzz=0,bassDrive=0,curveTime=.25f;
    bool curveEnabled=false;
    std::array<float,8> curve{24,12,5,1,0,0,0,0};
};

// Immutable sample storage; ownership and replacement are handled off the audio thread.
struct Sample {
    std::vector<float> left, right;
    double rate = 44100.0;
    bool valid() const { return left.size() > 1 && left.size() == right.size() && rate >= 8000; }
};

struct Biquad {
    double b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
    void reset() { z1=z2=0; }
    float tick(float input) {
        const double out=b0*input+z1;
        z1=b1*input-a1*out+z2; z2=b2*input-a2*out;
        if (std::abs(z1)<1e-24) z1=0;
        if (std::abs(z2)<1e-24) z2=0;
        return finite(static_cast<float>(out));
    }
    enum Type { HighPass, LowPass, Peak, LowShelf, HighShelf, BandPass };
    void configure(Type type, double rate, double hz, double q=0.707, double gain=0) {
        hz=std::clamp(hz,10.0,rate*0.45); q=std::clamp(q,0.1,12.0);
        const double w=2*pi*hz/rate,c=std::cos(w),s=std::sin(w),al=s/(2*q);
        const double A=std::pow(10.0,gain/40.0),rt=2*std::sqrt(A)*al;
        double a0=1;
        if(type==HighPass) { b0=(1+c)/2;b1=-(1+c);b2=b0;a0=1+al;a1=-2*c;a2=1-al; }
        else if(type==LowPass) { b0=(1-c)/2;b1=1-c;b2=b0;a0=1+al;a1=-2*c;a2=1-al; }
        else if(type==BandPass) { b0=al;b1=0;b2=-al;a0=1+al;a1=-2*c;a2=1-al; }
        else if(type==Peak) { b0=1+al*A;b1=-2*c;b2=1-al*A;a0=1+al/A;a1=-2*c;a2=1-al/A; }
        else if(type==LowShelf) {
            b0=A*((A+1)-(A-1)*c+rt);b1=2*A*((A-1)-(A+1)*c);b2=A*((A+1)-(A-1)*c-rt);
            a0=(A+1)+(A-1)*c+rt;a1=-2*((A-1)+(A+1)*c);a2=(A+1)+(A-1)*c-rt;
        } else {
            b0=A*((A+1)+(A-1)*c+rt);b1=-2*A*((A-1)+(A+1)*c);b2=A*((A+1)+(A-1)*c-rt);
            a0=(A+1)-(A-1)*c+rt;a1=2*((A-1)-(A+1)*c);a2=(A+1)-(A-1)*c-rt;
        }
        b0/=a0;b1/=a0;b2/=a0;a1/=a0;a2/=a0;
    }
};

class Synth {
    struct Voice {
        bool active=false,releasing=false; int note=60,channel=1;
        double phase=0,phase2=0,hz=440,target=440,pos=0,age=0;
        float level=0,velocity=0,low=0; uint64_t order=0;uint32_t noise=0x1237a8u;
    };
    std::array<Voice,16> voices{};
    std::array<bool,16> sustainPedal{};
    std::array<double,16> bend{};
    std::array<std::array<bool,128>,16> held{};
    double rate=44100; uint64_t counter=0; int previousMode=-1;
    static double blep(double t,double dt) {
        if(t<dt){t/=dt;return 2*t-t*t-1;}
        if(t>1-dt){t=(t-1)/dt;return t*t+2*t+1;}
        return 0;
    }
    static double oscillator(double t,double dt,int wave) {
        if(wave==0) return std::sin(2*pi*t);
        if(wave==1) return 2*t-1-blep(t,dt);
        if(wave==2){ double u=t+0.5;if(u>=1)u-=1;return (t<0.5?1:-1)+blep(t,dt)-blep(u,dt); }
        return 2*std::abs(2*t-1)-1;
    }
public:
    bool isActive()const {for(const auto& v:voices)if(v.active)return true;return false;}
    void prepare(double sr) { rate=sr;reset(); }
    void reset() { voices={};held={};sustainPedal={};bend.fill(1);previousMode=-1; }
    void allOff(int channel=0,bool immediate=true) {
        for(auto&v:voices) if(channel==0||v.channel==channel){if(immediate)v.active=false;else v.releasing=true;}
        for(int c=0;c<16;++c)if(channel==0||c+1==channel){held[c].fill(false);sustainPedal[c]=false;}
    }
    void noteOn(int note,float velocity,int channel,const Patch& p,const Sample* sample) {
        if(note<0||note>127||channel<1||channel>16)return;
        if(p.mode!=previousMode){allOff();previousMode=p.mode;}
        if(p.mode==2 && (!sample||!sample->valid()))return;
        held[channel-1][note]=true;
        Voice* slot=&voices[0];
        if(p.mode!=0){
            for(auto&v:voices)if(!v.active){slot=&v;break;}else if(v.order<slot->order)slot=&v;
        }
        const double oldHz=slot->hz;const bool glide=slot->active&&p.mode==0&&p.glide>0;
        *slot={};slot->active=true;slot->note=note;slot->channel=channel;
        slot->target=noteHz(note);slot->hz=glide?oldHz:slot->target;
        slot->velocity=std::clamp(velocity,0.0f,1.0f);slot->order=++counter;
        if(sample && sample->valid())slot->pos=(p.reverse?1-p.sampleStart:p.sampleStart)*(sample->left.size()-2);
    }
    void noteOff(int note,int channel,const Patch& p) {
        if(note<0||note>127||channel<1||channel>16)return;
        held[channel-1][note]=false;
        for(auto&v:voices) if(v.active&&v.note==note&&v.channel==channel&&!sustainPedal[channel-1])
            if(p.mode!=2||!p.oneShot)v.releasing=true;
    }
    void controller(int number,int value,int channel,const Patch& p) {
        if(channel<1||channel>16)return;
        if(number==120){allOff(channel);return;}
        if(number==123){allOff(channel,false);return;}
        if(number==64){sustainPedal[channel-1]=value>=64;
            if(value<64)for(auto&v:voices)if(v.channel==channel&&!held[channel-1][v.note]&&(p.mode!=2||!p.oneShot))v.releasing=true;
        }
    }
    void pitchBend(int value,int channel) { if(channel>=1&&channel<=16)bend[channel-1]=std::exp2(((value-8192)/8192.0)*2/12); }
    void render(float& l,float& r,const Patch& p,const Sample* sample) {
        l=r=0;
        if(p.mode!=previousMode){allOff();previousMode=p.mode;}
        const float atk=1.0f/static_cast<float>(std::max(0.001f,p.attack)*rate);
        const float rel=static_cast<float>(std::exp(-6.91/(std::max(0.005f,p.release)*rate)));
        const float dec=static_cast<float>(std::exp(-6.91/(std::max(0.03f,p.decay)*rate)));
        const float filt=static_cast<float>(1-std::exp(-2*pi*std::clamp(double(p.cutoff),20.0,rate*0.4)/rate));
        for(auto&v:voices){
            if(!v.active)continue;
            if(v.releasing)v.level*=rel;
            else if(v.age<p.attack)v.level=std::min(1.0f,v.level+atk);
            else if(p.mode==0)v.level*=dec;
            else if(p.mode==1)v.level=p.sustain+(v.level-p.sustain)*dec;
            else v.level=1;
            if(v.age>p.attack && v.level<0.00003f){v.active=false;continue;}
            if(p.glide>0)v.hz+=(v.target-v.hz)*(1-std::exp(-1/(p.glide*rate)));else v.hz=v.target;
            const double freq=v.hz*bend[v.channel-1];
            float a=0,b=0;
            if(p.mode==2){
                if(!sample||!sample->valid()||v.pos<0||v.pos>=double(sample->left.size()-1)){v.active=false;continue;}
                const auto i=static_cast<size_t>(v.pos);const float t=static_cast<float>(v.pos-i);
                a=sample->left[i]*(1-t)+sample->left[i+1]*t;b=sample->right[i]*(1-t)+sample->right[i+1]*t;
                const double step=sample->rate/rate*std::exp2((v.note-p.root+p.samplePitch)/12.0)*bend[v.channel-1];
                v.pos+=p.reverse?-step:step;
                // Smooth the end of a one-shot even when the imported file has a hard boundary.
                const float fade=static_cast<float>(std::min(1.0,std::min(v.pos,double(sample->left.size()-1)-v.pos)/(rate*0.003*step)));
                a*=std::max(0.0f,fade);b*=std::max(0.0f,fade);
            } else {
                double hz=freq;
                if(p.mode==0){
                    double drop=p.drop*std::exp(-v.age/0.035);
                    if(p.curveEnabled){const double at=std::clamp(v.age/std::max(.01f,p.curveTime)*7,0.0,7.0);const int index=std::min(6,int(at));const double t=at-index;drop=p.curve[size_t(index)]*(1-t)+p.curve[size_t(index+1)]*t;}
                    hz*=std::exp2(drop/12.0);
                }
                const double dt=std::min(0.45,hz/rate),dt2=std::min(0.45,dt*(p.mode==0?2:std::exp2(p.detune/1200.0)));
                a=static_cast<float>(oscillator(v.phase,dt,p.mode==0?0:p.wave));
                if(p.mode==1)a=0.5f*(a+static_cast<float>(oscillator(v.phase2,dt2,p.wave)));
                if(p.mode==0){
                    v.noise^=v.noise<<13;v.noise^=v.noise>>17;v.noise^=v.noise<<5;
                    const float noise=float(v.noise/4294967295.0*2-1);
                    a=a*p.bassSub+float(std::sin(2*pi*v.phase2)*std::exp(-v.age/.035))*p.bassPunch;
                    a+=noise*float(std::exp(-v.age/.004))*p.bassClick;
                    a+=float(oscillator(v.phase,dt,1))*.45f*p.bassBuzz;
                    if(p.bassDrive>0){const float amount=db(p.bassDrive);a=std::tanh(a*amount)/std::tanh(amount);}
                }
                v.phase+=dt;v.phase-=std::floor(v.phase);v.phase2+=dt2;v.phase2-=std::floor(v.phase2);
                v.low+=filt*(a-v.low);a=v.low;b=a;
            }
            const float gain=v.level*v.velocity*(p.mode==0?0.75f:0.23f);
            l+=a*gain;r+=b*gain;v.age+=1/rate;
        }
        l=finite(l);r=finite(r);
    }
};

struct FxPatch {
    bool bypass=false;
    float input=0,output=-6,mix=1,drive=0,distMix=0;
    int distType=0;
    float highPass=20,lowHz=100,lowGain=0,mid1Hz=400,mid1Gain=0,mid1Q=1;
    float mid2Hz=2000,mid2Gain=0,mid2Q=1,highHz=8000,highGain=0;
    float threshold=0,ratio=3,deess=0,delayMs=280,feedback=0.25f,delayMix=0,reverb=0;
    float doubles=0,harmony=0,harmonyPitch=7,width=1;
    float dynamicHz=3200,dynamicQ=1.5f,dynamicThreshold=-20,dynamicRange=0;
};

class Effects {
    double rate=44100;std::array<std::array<Biquad,5>,2> eq{};
    std::array<Biquad,2> sibilance{};
    std::array<Biquad,2> dynamicBand{};
    std::array<float,2> dynamicEnvelope{};
    VocalLayers vocal;
    float dynamicReduction=0;
    std::array<std::vector<float>,2> delay;
    struct Comb {std::vector<float> data;size_t pos=0;float damp=0;
        float tick(float x,float feedback){float y=data[pos];damp=0.3f*y+0.7f*damp;data[pos]=x+damp*feedback;if(++pos==data.size())pos=0;return y;}
    };
    std::array<std::array<Comb,4>,2> combs;
    size_t write=0;float envelope=0,inputGain=1,outputGain=0.5f,wet=1,driveGain=1,distWet=0,delayWet=0,roomWet=0,delaySamples=12348;
    bool coefficientsValid=false;FxPatch old{};
public:
    float reduction()const{return dynamicReduction;}
    void prepare(double sr){
        vocal.prepare(sr);
        rate=sr;for(auto&v:delay)v.assign(static_cast<size_t>(sr*2)+4,0);
        const std::array<double,4> times{0.0297,0.0371,0.0411,0.0437};
        for(int c=0;c<2;++c)for(int i=0;i<4;++i){auto&b=combs[c][i];b.data.assign(static_cast<size_t>(sr*(times[i]+c*0.0013)),0);b.pos=0;b.damp=0;}
        reset();
    }
    void reset(){for(auto&channel:eq)for(auto&b:channel)b.reset();for(auto&b:sibilance)b.reset();
        for(auto& b:dynamicBand)b.reset();
        dynamicEnvelope={};dynamicReduction=0;vocal.reset();
        for(auto&v:delay)std::fill(v.begin(),v.end(),0);
        for(auto&c:combs)for(auto&b:c){std::fill(b.data.begin(),b.data.end(),0);b.pos=0;b.damp=0;}
        write=0;envelope=0;coefficientsValid=false;
    }
    void configure(const FxPatch& p){
        // Recalculate only changed filters, avoiding repeated trigonometry at tiny host block sizes.
        for(int c=0;c<2;++c){
            if(!coefficientsValid||p.highPass!=old.highPass)eq[c][0].configure(Biquad::HighPass,rate,p.highPass);
            if(!coefficientsValid||p.lowHz!=old.lowHz||p.lowGain!=old.lowGain)eq[c][1].configure(Biquad::LowShelf,rate,p.lowHz,0.707,p.lowGain);
            if(!coefficientsValid||p.mid1Hz!=old.mid1Hz||p.mid1Gain!=old.mid1Gain||p.mid1Q!=old.mid1Q)eq[c][2].configure(Biquad::Peak,rate,p.mid1Hz,p.mid1Q,p.mid1Gain);
            if(!coefficientsValid||p.mid2Hz!=old.mid2Hz||p.mid2Gain!=old.mid2Gain||p.mid2Q!=old.mid2Q)eq[c][3].configure(Biquad::Peak,rate,p.mid2Hz,p.mid2Q,p.mid2Gain);
            if(!coefficientsValid||p.highHz!=old.highHz||p.highGain!=old.highGain)eq[c][4].configure(Biquad::HighShelf,rate,p.highHz,0.707,p.highGain);
            if(!coefficientsValid)sibilance[c].configure(Biquad::HighPass,rate,5500);
            if(!coefficientsValid||p.dynamicHz!=old.dynamicHz||p.dynamicQ!=old.dynamicQ)dynamicBand[c].configure(Biquad::BandPass,rate,p.dynamicHz,p.dynamicQ);
        }
        old=p;coefficientsValid=true;
    }
    void process(float& l,float& r,const FxPatch& p){
        float dry[2]={finite(l),finite(r)};if(p.bypass){l=dry[0];r=dry[1];return;}
        constexpr float slew=0.002f;
        inputGain+=slew*(db(p.input)-inputGain);outputGain+=slew*(db(p.output)-outputGain);
        wet+=slew*(p.mix-wet);driveGain+=slew*(db(p.drive)-driveGain);distWet+=slew*(p.distMix-distWet);
        delayWet+=slew*(p.delayMix-delayWet);roomWet+=slew*(p.reverb-roomWet);
        delaySamples+=0.0005f*(static_cast<float>(p.delayMs*rate/1000)-delaySamples);
        const float peak=std::max(std::abs(dry[0]),std::abs(dry[1]))*inputGain;
        const float ec=static_cast<float>(std::exp(-1/((peak>envelope?0.005:0.08)*rate)));
        envelope=peak+(envelope-peak)*ec;
        const float over=std::max(0.0f,20*std::log10(std::max(1e-8f,envelope))-p.threshold);
        const float compression=db(-over*(1-1/std::max(1.0f,p.ratio)));
        float out[2];dynamicReduction=0;
        for(int c=0;c<2;++c){
            float x=dry[c]*inputGain;for(auto&b:eq[c])x=b.tick(x);
            const float probe=dynamicBand[c].tick(x);
            const float dc=float(std::exp(-1/((std::abs(probe)>dynamicEnvelope[c]?.006:.120)*rate)));
            dynamicEnvelope[c]=std::abs(probe)+(dynamicEnvelope[c]-std::abs(probe))*dc;
            const float reduction=std::clamp((20*std::log10(std::max(1e-8f,dynamicEnvelope[c]))-p.dynamicThreshold)*.75f,0.0f,p.dynamicRange);
            dynamicReduction=std::max(dynamicReduction,reduction);x+=(db(-reduction)-1)*probe;
            const float high=sibilance[c].tick(x);x-=high*std::clamp(std::abs(high)*p.deess*3.0f,0.0f,0.85f);
            x*=compression;
            float driven=x*driveGain;
            if(p.distType==0)driven=std::tanh(driven);
            else if(p.distType==1)driven=std::clamp(driven,-1.0f,1.0f);
            else {driven=std::fmod(driven+1.0f,4.0f);if(driven<0)driven+=4;driven=1-std::abs(driven-2);}
            x+=distWet*(driven-x);
            const double read=std::fmod(write+delay[c].size()-std::clamp(double(delaySamples),1.0,double(delay[c].size()-2)),double(delay[c].size()));
            const auto i=static_cast<size_t>(read);const float f=static_cast<float>(read-i);
            const float echo=delay[c][i]*(1-f)+delay[c][(i+1)%delay[c].size()]*f;
            delay[c][write]=finite(std::clamp(x,-4.0f,4.0f)+echo*std::clamp(p.feedback,0.0f,0.85f));
            float room=0;for(auto&b:combs[c])room+=b.tick(x*0.15f,0.72f);
            x+=echo*delayWet+room*roomWet;
            // Output protection; no lookahead or reported latency. This is a hard safety ceiling.
            out[c]=x;
        }
        vocal.process(out[0],out[1],p.doubles,p.harmony,p.harmonyPitch,p.width);
        for(int c=0;c<2;++c)out[c]=std::clamp(finite(dry[c]+wet*(out[c]*outputGain-dry[c])),-1.0f,1.0f);
        if(++write==delay[0].size())write=0;
        l=out[0];r=out[1];
    }
};
} // namespace fresh
