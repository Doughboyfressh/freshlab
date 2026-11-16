#include "Core/Dsp.h"
#include <atomic>
#include <complex>
#include <cstdlib>
#include <iostream>
#include <new>
#include <stdexcept>
static std::atomic<bool> track{false};static std::atomic<int> allocations{0};
void* operator new(std::size_t n){if(track)++allocations;if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
using namespace fresh;
static int checks=0;
void check(bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);}
double amplitude(const std::vector<float>& audio,double hz,double rate=44100){std::complex<double> sum{};
    for(size_t i=0;i<audio.size();++i)sum+=double(audio[i])*std::polar(1.,-2*pi*hz*i/rate);return 2*std::abs(sum)/audio.size();}
int main(){try{
    for(double rate:{22050.,44100.,48000.,96000.}){
        for(int quality:{0,1}){
            VocalLayers voice;voice.prepare(rate);const int d=voice.latency(quality);
            for(int i=0;i<d+10;++i){float l=i==0?.3f:0,r=i==0?-.2f:0;voice.process(l,r,0,0,7,1,1,true,0,0,0,60,quality);
                check(l==(i==d?.3f:0)&&r==(i==d?-.2f:0),"vocal dry delay matches declared latency");}
            Effects effects;effects.prepare(rate);FxPatch p;p.mix=0;p.vocalQuality=quality;effects.configure(p);
            for(int i=0;i<effects.latency()+10;++i){float l=i==0?.3f:0,r=i==0?-.2f:0;effects.process(l,r,p);
                check(l==(i==effects.latency()?.3f:0)&&r==(i==effects.latency()?-.2f:0),"complete dry path matches host latency");}
            effects.reset();p.bypass=true;
            for(int i=0;i<effects.latency()+10;++i){float l=i==0?1.3f:0,r=i==0?-.8f:0;effects.process(l,r,p);
                check(l==(i==effects.latency()?1.3f:0)&&r==(i==effects.latency()?-.8f:0),"bypass preserves original samples and timing");}
        }
        LookaheadLimiter limiter;limiter.prepare(rate);const float ceiling=db(-.3f);
        for(int i=0;i<int(rate*.3);++i){float l=i%97==0?8:float(std::sin(i*.11))*2,r=l*.5f;limiter.process(l,r,ceiling);
            check(std::isfinite(l)&&std::abs(l)<=ceiling+1.e-6f&&r==l*.5f,"limiter bounds peaks and preserves stereo ratio");}
        PitchTracker tracker;tracker.prepare(rate);
        for(int i=0;i<int(rate*.4);++i)tracker.process(float(std::sin(2*pi*445*i/rate)*.3+.08*std::sin(2*pi*890*i/rate)));
        check(std::abs(tracker.frequency()-445)<2&&tracker.certainty()>.9f,"pitch detector tracks a voiced harmonic signal");
    }
    check(std::abs(PitchTracker::correction(440*std::exp2(.3/12),0,0)+.3)<.01,"chromatic correction targets the nearest note");
    {
        OversampledDistortion distortion;std::vector<float> plain,filtered;
        for(int i=0;i<44100;++i){const float x=float(std::sin(2*pi*8000*i/44100))*.8f;
            const float y=distortion.process(x,4,1);if(i>4410){plain.push_back(std::clamp(x*4,-1.f,1.f));filtered.push_back(y);}}
        const auto before=amplitude(plain,20100),after=amplitude(filtered,20100);
        std::cout<<"8 kHz hard-clip alias amplitude: "<<before<<" -> "<<after<<'\n';check(after<before*.5,"4x distortion reduces folded third harmonic");
    }
    {
        Biquad smooth,instant;smooth.configure(Biquad::Peak,44100,1000,3,0);instant=smooth;double a=0,b=0;
        for(int i=0;i<4410;++i){const float x=float(std::sin(2*pi*1000*i/44100))*.1f;smooth.tick(x);instant.tick(x);}
        smooth.configureSmooth(Biquad::Peak,44100,1000,3,18);instant.configure(Biquad::Peak,44100,1000,3,18);
        for(int i=0;i<64;++i){const float x=float(std::sin(2*pi*1000*i/44100))*.1f;a+=std::abs(smooth.tick(x)-x);b+=std::abs(instant.tick(x)-x);}
        check(a<b*.5,"EQ coefficient ramp softens a sudden gain change");
    }
    {
        Effects external,quiet;external.prepare(44100);quiet.prepare(44100);FxPatch p;p.output=0;p.ratio=10;p.threshold=-20;p.externalSidechain=true;
        double loudEnergy=0,quietEnergy=0;
        for(int i=0;i<22050;++i){float a=float(std::sin(2*pi*1000*i/44100))*.1f,b=a,c=a,d=a;
            external.process(a,b,p,1);quiet.process(c,d,p,0);if(i>11025){loudEnergy+=a*a;quietEnergy+=c*c;}}
        check(loudEnergy<quietEnergy*.1,"external sidechain ducks the main signal");
    }
    {
        Synth synth;synth.prepare(44100);Patch p;p.mode=1;p.wave=0;p.attack=.001f;p.decay=8;p.sustain=1;p.cutoff=14000;p.mpe=true;p.legato=true;
        synth.noteOn(69,1,2,p,nullptr);synth.noteOn(76,1,3,p,nullptr);synth.pitchBend(12288,2,true);
        std::vector<float> audio;for(int i=0;i<44100;++i){float l,r;synth.render(l,r,p,nullptr);if(i>4410)audio.push_back(l);}
        check(amplitude(audio,1760)>.05&&amplitude(audio,noteHz(76))>.05,"MPE bends one member independently of another");
        synth.channelPressure(0,2);audio.clear();for(int i=0;i<22050;++i){float l,r;synth.render(l,r,p,nullptr);audio.push_back(l);}
        check(amplitude(audio,1760)<.001&&amplitude(audio,noteHz(76))>.05,"MPE pressure applies only to its member");
    }
    {
        VocalLayers tuner;tuner.prepare(44100);std::vector<float> audio;
        for(int i=0;i<88200;++i){float l=float(std::sin(2*pi*448*i/44100))*.2f,r=l;tuner.process(l,r,0,0,7,1,1,false,1,0,0,10,1);if(i>44100)audio.push_back(l);}
        std::cout<<"Corrected 448 Hz: 440="<<amplitude(audio,440)<<" / 448="<<amplitude(audio,448)<<'\n';
        check(amplitude(audio,440)>.08&&amplitude(audio,440)>amplitude(audio,448)*3,"monophonic pitch correction pulls a detuned tone to A");
    }
    {
        Effects fx;fx.prepare(44100);FxPatch p;p.vocalQuality=1;p.harmony=.7f;p.doubles=.4f;p.tuneAmount=.7f;p.drive=16;p.distMix=.6f;
        for(int i=0;i<22050;++i){float l=float(std::sin(i*.035))*.2f,r=l;fx.process(l,r,p);}
        allocations=0;track=true;bool finite=true;
        for(int i=0;i<11025;++i){float l=float(std::sin(i*.035))*.2f,r=l;fx.process(l,r,p);finite=finite&&std::isfinite(l)&&std::isfinite(r);}
        track=false;check(finite,"full spectral rack stays finite");check(allocations==0,"full rack makes no C++ heap allocations while processing");
    }
    std::cout<<"PASS: "<<checks<<" readiness assertions\n";return 0;
}catch(const std::exception& e){track=false;std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
