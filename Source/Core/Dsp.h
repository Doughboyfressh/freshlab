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
    bool reverse = false, oneShot = true, mpe = false, legato = false;
    float bendRange=2;
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
    std::array<double,5> destination{},increment{};int ramp=0;
    void reset() { z1=z2=0;ramp=0; }
    float tick(float input) {
        if(ramp>0){double* coefficients[]{&b0,&b1,&b2,&a1,&a2};--ramp;
            for(size_t i=0;i<5;++i)*coefficients[i]=ramp?*coefficients[i]+increment[i]:destination[i];}
        const double out=b0*input+z1;
        z1=b1*input-a1*out+z2; z2=b2*input-a2*out;
        if (std::abs(z1)<1e-24) z1=0;
        if (std::abs(z2)<1e-24) z2=0;
        return finite(static_cast<float>(out));
    }
    enum Type { HighPass, LowPass, Peak, LowShelf, HighShelf, BandPass };
    void configure(Type type, double rate, double hz, double q=0.707, double gain=0) {
        ramp=0;hz=std::clamp(hz,10.0,rate*0.45); q=std::clamp(q,0.1,12.0);
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
    void configureSmooth(Type type,double rate,double hz,double q=.707,double gain=0){
        const std::array<double,5> previous{b0,b1,b2,a1,a2};configure(type,rate,hz,q,gain);
        destination={b0,b1,b2,a1,a2};ramp=std::max(1,int(rate*.015));double* coefficients[]{&b0,&b1,&b2,&a1,&a2};
        for(size_t i=0;i<5;++i){increment[i]=(destination[i]-previous[i])/ramp;*coefficients[i]=previous[i];}
    }
};

class Synth {
    struct Voice {
        bool active=false,releasing=false; int note=60,channel=1;
        double phase=0,phase2=0,hz=440,target=440,pos=0,age=0;
        float level=0,velocity=0,pressure=1,low=0; uint64_t order=0;uint32_t noise=0x1237a8u;
    };
    std::array<Voice,16> voices{};
    std::array<bool,16> sustainPedal{};
    std::array<double,16> bend{};
    std::array<float,16> pressure{},timbre{},bendRanges{};
    std::array<int,16> wheels{},rpnMsb{},rpnLsb{};
    std::array<std::array<uint64_t,128>,16> heldOrder{};
    float attackStep=0,releaseMul=0,decayMul=0,filterCoeff=0,glideCoeff=0;
    Patch cached;bool cacheValid=false;
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
    void reset() { voices={};held={};heldOrder={};sustainPedal={};bend.fill(1);pressure.fill(1);timbre.fill(.5f);
        bendRanges.fill(-1);wheels.fill(8192);rpnMsb.fill(127);rpnLsb.fill(127);previousMode=-1;cacheValid=false; }
    void allOff(int channel=0,bool immediate=true) {
        for(auto&v:voices) if(channel==0||v.channel==channel){if(immediate)v.active=false;else v.releasing=true;}
        for(int c=0;c<16;++c)if(channel==0||c+1==channel){held[c].fill(false);sustainPedal[c]=false;}
    }
    void noteOn(int note,float velocity,int channel,const Patch& p,const Sample* sample) {
        if(note<0||note>127||channel<1||channel>16)return;
        if(p.mode!=previousMode){allOff();previousMode=p.mode;}
        if(p.mode==2 && (!sample||!sample->valid()))return;
        held[channel-1][note]=true;heldOrder[channel-1][note]=++counter;
        Voice* slot=&voices[0];
        if(p.mode!=0&&(!p.legato||p.mode==2||p.mpe)){
            for(auto&v:voices)if(!v.active){slot=&v;break;}else if(v.order<slot->order)slot=&v;
        }
        if(p.mode==1&&p.legato&&p.mpe)for(auto& v:voices)if(v.active&&v.channel==channel){slot=&v;break;}
        const double oldHz=slot->hz;const bool glide=slot->active&&(p.mode==0||p.legato)&&p.glide>0;
        *slot={};slot->active=true;slot->note=note;slot->channel=channel;
        slot->target=noteHz(note);slot->hz=glide?oldHz:slot->target;
        slot->velocity=std::clamp(velocity,0.0f,1.0f);slot->order=++counter;
        if(sample && sample->valid())slot->pos=(p.reverse?1-p.sampleStart:p.sampleStart)*(sample->left.size()-2);
    }
    void noteOff(int note,int channel,const Patch& p) {
        if(note<0||note>127||channel<1||channel>16)return;
        held[channel-1][note]=false;
        if(p.legato&&p.mode!=2){for(auto& v:voices)if(v.active&&v.note==note&&v.channel==channel){
            int newest=-1;uint64_t order=0;for(int n=0;n<128;++n)if(held[channel-1][n]&&heldOrder[channel-1][n]>order){newest=n;order=heldOrder[channel-1][n];}
            if(newest>=0){v.note=newest;v.target=noteHz(newest);return;}}}
        for(auto&v:voices) if(v.active&&v.note==note&&v.channel==channel&&!sustainPedal[channel-1])
            if(p.mode!=2||!p.oneShot)v.releasing=true;
    }
    void controller(int number,int value,int channel,const Patch& p) {
        if(channel<1||channel>16)return;
        if(number==101)rpnMsb[channel-1]=value;
        if(number==100)rpnLsb[channel-1]=value;
        if(number==74)timbre[channel-1]=std::clamp(value/127.0f,0.0f,1.0f);
        if(number==6&&rpnMsb[channel-1]==0&&rpnLsb[channel-1]==0){bendRanges[channel-1]=float(std::clamp(value,0,96));pitchBend(wheels[channel-1],channel,p.mpe,p.bendRange);}
        if(number==120){allOff(channel);return;}
        if(number==123){allOff(channel,false);return;}
        if(number==64){sustainPedal[channel-1]=value>=64;
            if(value<64)for(auto&v:voices)if(v.channel==channel&&!held[channel-1][v.note]&&(p.mode!=2||!p.oneShot))v.releasing=true;
        }
    }
    void pitchBend(int value,int channel,bool mpe=false,float range=2) { if(channel>=1&&channel<=16){
        wheels[channel-1]=value;const float semitones=bendRanges[channel-1]>=0?bendRanges[channel-1]:(mpe&&channel>1?48:range);
        bend[channel-1]=std::exp2(((value-8192)/8192.0)*semitones/12);}}
    void channelPressure(float value,int channel){if(channel>=1&&channel<=16)pressure[channel-1]=std::clamp(value,0.0f,1.0f);}
    void polyPressure(float value,int note,int channel){for(auto& v:voices)if(v.note==note&&v.channel==channel)v.pressure=std::clamp(value,0.0f,1.0f);}
    void render(float& l,float& r,const Patch& p,const Sample* sample) {
        l=r=0;
        if(p.mode!=previousMode){allOff();previousMode=p.mode;}
        if(!cacheValid||p.attack!=cached.attack||p.release!=cached.release||p.decay!=cached.decay||p.cutoff!=cached.cutoff||p.glide!=cached.glide){
            attackStep=1.0f/float(std::max(.001f,p.attack)*rate);releaseMul=float(std::exp(-6.91/(std::max(.005f,p.release)*rate)));
            decayMul=float(std::exp(-6.91/(std::max(.03f,p.decay)*rate)));filterCoeff=float(1-std::exp(-2*pi*std::clamp(double(p.cutoff),20.0,rate*.4)/rate));
            glideCoeff=p.glide>0?float(1-std::exp(-1/(p.glide*rate))):1;cached=p;cacheValid=true;
        }
        const float atk=attackStep,rel=releaseMul,dec=decayMul;
        for(auto&v:voices){
            if(!v.active)continue;
            if(v.releasing)v.level*=rel;
            else if(v.age<p.attack)v.level=std::min(1.0f,v.level+atk);
            else if(p.mode==0)v.level*=dec;
            else if(p.mode==1)v.level=p.sustain+(v.level-p.sustain)*dec;
            else v.level=1;
            if(v.age>p.attack && v.level<0.00003f){v.active=false;continue;}
            v.hz+=(v.target-v.hz)*glideCoeff;
            const double freq=v.hz*bend[v.channel-1]*(p.mpe&&v.channel>1?bend[0]:1);
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
                const float filt=std::min(.99f,filterCoeff*(p.mpe?(.5f+timbre[v.channel-1])*(v.channel>1?.5f+timbre[0]:1.f):1.f));v.low+=filt*(a-v.low);a=v.low;b=a;
            }
            const float gain=v.level*v.velocity*v.pressure*(p.mpe?pressure[v.channel-1]*(v.channel>1?pressure[0]:1.f):1.0f)*(p.mode==0?0.75f:0.23f);
            l+=a*gain;r+=b*gain;v.age+=1/rate;
        }
        l=finite(l);r=finite(r);
    }
};

struct FxPatch {
    bool bypass=false,externalSidechain=false;
    float input=0,output=-6,mix=1,drive=0,distMix=0;int distType=0;
    float highPass=20,lowHz=100,lowGain=0,mid1Hz=400,mid1Gain=0,mid1Q=1;
    float mid2Hz=2000,mid2Gain=0,mid2Q=1,highHz=8000,highGain=0;
    float threshold=0,ratio=3,deess=0,delayMs=280,feedback=.25f,delayMix=0,reverb=0;
    float compAttack=5,compRelease=80,roomSize=1,roomDecay=1.5f,roomDamping=.7f;
    float doubles=0,harmony=0,harmonyPitch=7,width=1,vocalFormant=1;
    float dynamicHz=3200,dynamicQ=1.5f,dynamicThreshold=-20,dynamicRange=0,dynamicAttack=6,dynamicRelease=120;
    float limiterCeiling=-.3f,limiterRelease=80,tuneAmount=0,tuneSpeed=60;int tuneKey=0,tuneScale=0,vocalQuality=0;
};

class Effects {
    double rate=44100;std::array<std::array<Biquad,5>,2> eq{};
    std::array<Biquad,2> sibilance{},dynamicBand{};std::array<float,2> dynamicEnvelope{};
    VocalLayers vocal;LookaheadLimiter limiter;std::array<StereoDelay,2> original,beforeLimit,dryMix;StereoDelay dryDist;int qualityMode=0;
    std::array<OversampledDistortion,2> distortions;
    std::array<std::vector<float>,2> delay;
    struct Comb {std::vector<float> data;size_t pos=0;float damp=0,seconds=.03f,feedback=.72f;
        void reset(){std::fill(data.begin(),data.end(),0);pos=0;damp=0;}
        float tick(float x,float samples,float damping){
            double at=double(pos)+data.size()-std::clamp(double(samples),1.,double(data.size()-2));if(at>=data.size())at-=data.size();
            const size_t i=size_t(at);const float t=float(at-i),y=data[i]*(1-t)+data[(i+1)%data.size()]*t;
            damp=(1-damping)*y+damping*damp;data[pos]=finite(x+damp*feedback);if(++pos==data.size())pos=0;return y;}
    };
    std::array<std::array<Comb,4>,2> combs;
    size_t write=0;uint64_t controlPhase=0;float envelope=0,dynamicReduction=0;
    float inputGain=1,outputGain=.5f,wet=1,driveGain=1,distWet=0,delayWet=0,roomWet=0,delaySamples=12348;
    float targetInput=1,targetOutput=.5f,targetDrive=1,targetCeiling=.966f;
    float threshold=0,ratio=3,roomSize=1,width=1,bypassBlend=0;
    float slew=.002f,delaySlew=.0005f,attack=0,release=0,dynAttack=0,dynRelease=0;
    bool coefficientsValid=false;FxPatch old{};
    static void smooth(float& x,float target,float amount){x+=(target-x)*amount;}
public:
    float reduction()const{return dynamicReduction;}
    int latencyForQuality(int mode)const{return OversampledDistortion::latency+vocal.latency(mode)+limiter.latency();}
    int latency()const{return latencyForQuality(qualityMode);}
private:
    void delayPaths(std::array<StereoDelay,2>& paths,float& l,float& r){float a=l,b=r;paths[0].process(l,r);paths[1].process(a,b);if(qualityMode==1){l=a;r=b;}}
public:
    void prepare(double sr){rate=std::clamp(sr,8000.,384000.);vocal.prepare(rate);limiter.prepare(rate);
        qualityMode=0;for(int mode=0;mode<2;++mode){original[size_t(mode)].prepare(latencyForQuality(mode));
            beforeLimit[size_t(mode)].prepare(OversampledDistortion::latency+vocal.latency(mode));dryMix[size_t(mode)].prepare(OversampledDistortion::latency+vocal.latency(mode));}
        dryDist.prepare(OversampledDistortion::latency);
        for(auto& v:delay)v.assign(size_t(rate*8)+4,0);
        const double times[]{.0297,.0371,.0411,.0437};
        for(int c=0;c<2;++c)for(int i=0;i<4;++i){auto& b=combs[c][i];b.seconds=float(times[i]+c*.0013);b.data.assign(size_t(rate*b.seconds*1.8)+4,0);}
        slew=float(1-std::exp(-1/(rate*.015)));delaySlew=float(1-std::exp(-1/(rate*.04)));reset();}
    void reset(){for(auto& channel:eq)for(auto& b:channel)b.reset();for(auto& b:sibilance)b.reset();for(auto& b:dynamicBand)b.reset();
        dynamicEnvelope={};dynamicReduction=0;vocal.reset();limiter.reset();for(auto& d:original)d.reset();for(auto& d:beforeLimit)d.reset();dryDist.reset();for(auto& d:dryMix)d.reset();
        for(auto& d:distortions)d.reset();for(auto& v:delay)std::fill(v.begin(),v.end(),0);for(auto& c:combs)for(auto& b:c)b.reset();
        write=0;envelope=0;controlPhase=0;coefficientsValid=false;}
    void configure(const FxPatch& p){
        if(coefficientsValid&&p.vocalQuality!=qualityMode)reset();qualityMode=std::clamp(p.vocalQuality,0,1);
        for(int c=0;c<2;++c){
            auto configure=[&](Biquad& b,Biquad::Type type,double hz,double q=.707,double gain=0){
                if(coefficientsValid)b.configureSmooth(type,rate,hz,q,gain);else b.configure(type,rate,hz,q,gain);};
            if(!coefficientsValid||p.highPass!=old.highPass)configure(eq[c][0],Biquad::HighPass,p.highPass);
            if(!coefficientsValid||p.lowHz!=old.lowHz||p.lowGain!=old.lowGain)configure(eq[c][1],Biquad::LowShelf,p.lowHz,.707,p.lowGain);
            if(!coefficientsValid||p.mid1Hz!=old.mid1Hz||p.mid1Gain!=old.mid1Gain||p.mid1Q!=old.mid1Q)configure(eq[c][2],Biquad::Peak,p.mid1Hz,p.mid1Q,p.mid1Gain);
            if(!coefficientsValid||p.mid2Hz!=old.mid2Hz||p.mid2Gain!=old.mid2Gain||p.mid2Q!=old.mid2Q)configure(eq[c][3],Biquad::Peak,p.mid2Hz,p.mid2Q,p.mid2Gain);
            if(!coefficientsValid||p.highHz!=old.highHz||p.highGain!=old.highGain)configure(eq[c][4],Biquad::HighShelf,p.highHz,.707,p.highGain);
            if(!coefficientsValid)sibilance[c].configure(Biquad::HighPass,rate,5500);
            if(!coefficientsValid||p.dynamicHz!=old.dynamicHz||p.dynamicQ!=old.dynamicQ)configure(dynamicBand[c],Biquad::BandPass,p.dynamicHz,p.dynamicQ);
        }
        targetInput=db(p.input);targetOutput=db(p.output);targetDrive=db(p.drive);targetCeiling=db(std::clamp(p.limiterCeiling,-12.f,0.f));
        attack=float(std::exp(-1/(std::max(.1f,p.compAttack)*.001*rate)));release=float(std::exp(-1/(std::max(1.f,p.compRelease)*.001*rate)));
        dynAttack=float(std::exp(-1/(std::max(.1f,p.dynamicAttack)*.001*rate)));dynRelease=float(std::exp(-1/(std::max(1.f,p.dynamicRelease)*.001*rate)));
        if(!coefficientsValid||p.limiterRelease!=old.limiterRelease)limiter.setRelease(p.limiterRelease,rate);
        if(!coefficientsValid||p.roomSize!=old.roomSize||p.roomDecay!=old.roomDecay)
            for(auto& c:combs)for(auto& b:c)b.feedback=float(std::pow(.001,b.seconds*p.roomSize/std::max(.2f,p.roomDecay)));
        if(!coefficientsValid){inputGain=targetInput;outputGain=targetOutput;driveGain=targetDrive;wet=p.mix;distWet=p.distMix;
            delayWet=p.delayMix;roomWet=p.reverb;delaySamples=float(p.delayMs*rate/1000);threshold=p.threshold;ratio=p.ratio;roomSize=p.roomSize;width=p.width;bypassBlend=p.bypass?1.f:0.f;}
        old=p;coefficientsValid=true;
    }
    void process(float& l,float& r,const FxPatch& p,float sidechain=-1){
        if(!coefficientsValid||p.vocalQuality!=qualityMode||(controlPhase&31u)==0)configure(p);
        if(controlPhase==0)bypassBlend=p.bypass?1.f:0.f;++controlPhase;
        float rawL=finite(l),rawR=finite(r),bypassL=rawL,bypassR=rawR;delayPaths(original,bypassL,bypassR);float cleanL=rawL,cleanR=rawR;delayPaths(beforeLimit,cleanL,cleanR);
        smooth(inputGain,targetInput,slew);smooth(outputGain,targetOutput,slew);smooth(driveGain,targetDrive,slew);
        smooth(wet,p.mix,slew);smooth(distWet,p.distMix,slew);smooth(delayWet,p.delayMix,slew);smooth(roomWet,p.reverb,slew);
        smooth(delaySamples,float(p.delayMs*rate/1000),delaySlew);smooth(threshold,p.threshold,slew);smooth(ratio,p.ratio,slew);smooth(roomSize,p.roomSize,slew);smooth(width,p.width,slew);
        const float peak=(p.externalSidechain&&sidechain>=0&&std::isfinite(sidechain)?sidechain:std::max(std::abs(rawL),std::abs(rawR)))*inputGain;
        const float ec=peak>envelope?attack:release;envelope=peak+(envelope-peak)*ec;
        const float over=std::max(0.f,20*std::log10(std::max(1.e-8f,envelope))-threshold),compression=db(-over*(1-1/std::max(1.f,ratio)));
        float out[]{rawL*inputGain,rawR*inputGain};dynamicReduction=0;
        for(int c=0;c<2;++c){float& x=out[c];for(auto& b:eq[c])x=b.tick(x);
            const float probe=dynamicBand[c].tick(x),dc=std::abs(probe)>dynamicEnvelope[c]?dynAttack:dynRelease;
            dynamicEnvelope[c]=std::abs(probe)+(dynamicEnvelope[c]-std::abs(probe))*dc;
            const float cut=std::clamp((20*std::log10(std::max(1.e-8f,dynamicEnvelope[c]))-p.dynamicThreshold)*.75f,0.f,p.dynamicRange);
            dynamicReduction=std::max(dynamicReduction,cut);x+=(db(-cut)-1)*probe;
            const float high=sibilance[c].tick(x);x-=high*std::clamp(std::abs(high)*p.deess*3.f,0.f,.85f);x*=compression;
        }
        float shaped[]{0,0};if(distWet>1.e-6f)for(int c=0;c<2;++c)shaped[c]=distortions[c].process(out[c],driveGain,p.distType);
        dryDist.process(out[0],out[1]);for(int c=0;c<2;++c){float& x=out[c];x+=distWet*(shaped[c]-x);
            const double read=std::fmod(write+delay[c].size()-std::clamp(double(delaySamples),1.,double(delay[c].size()-2)),double(delay[c].size()));
            const size_t i=size_t(read);const float f=float(read-i),echo=delay[c][i]*(1-f)+delay[c][(i+1)%delay[c].size()]*f;
            delay[c][write]=finite(std::clamp(x,-4.f,4.f)+echo*std::clamp(p.feedback,0.f,.85f));
            float room=0;for(auto& b:combs[c])room+=b.tick(x*.15f,float(rate*b.seconds*roomSize),std::clamp(p.roomDamping,0.f,.95f));
            x+=echo*delayWet+room*roomWet;
        }
        vocal.process(out[0],out[1],p.doubles,p.harmony,p.harmonyPitch,width,p.vocalFormant,true,p.tuneAmount,p.tuneKey,p.tuneScale,p.tuneSpeed,p.vocalQuality);
        delayPaths(dryMix,rawL,rawR);out[0]=finite(rawL+wet*(out[0]*outputGain-rawL));out[1]=finite(rawR+wet*(out[1]*outputGain-rawR));
        smooth(bypassBlend,p.bypass?1.f:0.f,slew);if(std::abs(bypassBlend-(p.bypass?1.f:0.f))<1.e-5f)bypassBlend=p.bypass?1.f:0.f;
        out[0]+=bypassBlend*(cleanL-out[0]);out[1]+=bypassBlend*(cleanR-out[1]);limiter.process(out[0],out[1],targetCeiling);
        l=p.bypass&&bypassBlend==1?bypassL:out[0];r=p.bypass&&bypassBlend==1?bypassR:out[1];
        if(++write==delay[0].size())write=0;
    }
};
} // namespace fresh
