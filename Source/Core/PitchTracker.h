#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace fresh {
// YIN-style normalized difference detector. It rejects unvoiced/noisy frames;
// analysis runs off a decimated, low-pass-filtered mono signal.
class PitchTracker {
    std::array<float,512> history{},analysis{};std::array<double,257> difference{};
    int write=0,count=0,decimate=4,phase=0,lagCursor=0;double running=0;double rate=11025;
    float low1=0,low2=0,coefficient=.2f,hz=0,confidence=0;
public:
    void prepare(double sampleRate){decimate=std::max(1,int(std::round(sampleRate/12000)));rate=sampleRate/decimate;
        coefficient=float(1-std::exp(-2*3.141592653589793*3000/sampleRate));reset();}
    void reset(){history={};analysis={};difference={};write=count=phase=lagCursor=0;running=0;low1=low2=hz=confidence=0;}
    float frequency()const{return hz;}
    float certainty()const{return confidence;}
    void process(float x){low1+=coefficient*(x-low1);low2+=coefficient*(low1-low2);if(++phase<decimate)return;phase=0;
        history[size_t(write)]=low2;write=(write+1)%512;if(count<512)++count;if(count<512)return;
        const int minimum=std::max(2,int(rate/1200)),maximum=std::min(255,int(rate/50));
        if(lagCursor==0){double rms=0;for(int j=0;j<512;++j)analysis[size_t(j)]=history[size_t((write+j)%512)];
            for(int j=0;j<256;++j)rms+=analysis[size_t(j)]*analysis[size_t(j)];
            if(rms/256<1.e-6){hz=0;confidence=0;return;}lagCursor=1;running=0;difference[0]=1;}
        // Spread the difference analysis over a hop instead of doing all lags
        // in one small host block. The snapshot remains immutable for that hop.
        for(int work=0;work<4&&lagCursor<=maximum+1;++work,++lagCursor){double value=0;
            for(int j=0;j<256;++j){const double d=analysis[size_t(j)]-analysis[size_t(j+lagCursor)];value+=d*d;}
            running+=value;difference[size_t(lagCursor)]=value*lagCursor/std::max(1.e-20,running);}
        if(lagCursor<=maximum+1)return;lagCursor=0;
        int best=0;for(int lag=minimum;lag<=maximum;++lag)if(difference[size_t(lag)]<.15){
            while(lag<maximum&&difference[size_t(lag+1)]<difference[size_t(lag)])++lag;best=lag;break;}
        if(!best){hz=0;confidence=0;return;}
        const double a=difference[size_t(best-1)],b=difference[size_t(best)],c=difference[size_t(best+1)],denom=a-2*b+c;
        const double offset=std::abs(denom)>1.e-12?.5*(a-c)/denom:0;
        hz=float(rate/(best+std::clamp(offset,-.5,.5)));confidence=float(std::clamp(1-b,0.,1.));
    }
    static float correction(float hz,int root,int scale){
        if(hz<50||hz>1200)return 0;
        const double note=69+12*std::log2(hz/440.0);double closest=std::round(note),distance=100;
        const unsigned masks[]{0xfff,0xab5,0x5ad,0x295}; // chromatic, major, natural minor, major pentatonic
        const unsigned mask=masks[std::clamp(scale,0,3)];
        for(int n=int(std::floor(note))-2;n<=int(std::ceil(note))+2;++n){const int key=((n-root)%12+12)%12;
            if((mask&(1u<<key))&&std::abs(n-note)<distance){closest=n;distance=std::abs(n-note);}}
        return float(std::clamp(closest-note,-2.,2.));
    }
};
}
