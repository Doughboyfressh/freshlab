#pragma once
#include "Processing.h"
#include "PitchTracker.h"
#include "LiveVocal.h"
#include "../ThirdParty/signalsmith-stretch.h"
namespace fresh {
// Phase-coherent spectral layers with formant compensation and aligned dry audio.
// FFT plans and buffers are allocated before audio processing.
class VocalLayers {
    using Shifter=signalsmith::stretch::SignalsmithStretch<float>;
    static constexpr int chunk=32;
    std::array<Shifter,4> shifts{{Shifter(101),Shifter(102),Shifter(103),Shifter(104)}};
    std::array<std::array<float,chunk>,2> input{};
    std::array<std::array<std::array<float,chunk>,2>,4> output{};
    LiveVocalLayers live;int mode=0;
    PitchTracker tracker;double sampleRate=44100;float tune=0,correctionMix=0;
    StereoDelay dry;int position=0,delaySamples=0;float doubleLevel=0,harmonyLevel=0,slew=.002f;
public:
    int latency(int quality=0)const{return quality==1?delaySamples:0;}
    void prepare(double rate){sampleRate=rate;live.prepare(rate);tracker.prepare(rate);int block=512;while(block<rate*.07)block*=2;
        for(auto& shift:shifts)shift.configure(2,block,block/4,true);
        delaySamples=shifts[0].inputLatency()+shifts[0].outputLatency()+chunk;
        dry.prepare(delaySamples);slew=float(1-std::exp(-1/(rate*.015)));reset();}
    void reset(){live.reset();for(auto& shift:shifts)shift.reset();input={};output={};position=0;
        if(mode==1){std::array<std::array<float,chunk>,2> silence{},discard{};
            for(size_t i=1;i<shifts.size();++i){int remaining=int(i)*shifts[i].intervalSamples()/4;
                while(remaining>0){const int n=std::min(chunk,remaining);shifts[i].process(silence,n,discard,n);remaining-=n;}}}doubleLevel=harmonyLevel=0;tune=correctionMix=0;tracker.reset();dry.reset();}
    void process(float& l,float& r,float doubles,float harmony,float semitones,float width,float formant=1,bool preserve=true,float tuneAmount=0,int key=0,int scale=0,float tuneSpeed=60,int quality=0){
        if(quality!=mode){mode=quality;reset();}
        if(mode==0){doubleLevel+=slew*(doubles-doubleLevel);harmonyLevel+=slew*(harmony-harmonyLevel);live.process(l,r,doubleLevel,harmonyLevel,semitones,width);return;}
        if(tuneAmount>0||harmony>0||doubles>0)tracker.process((l+r)*.5f);
        input[0][size_t(position)]=l;input[1][size_t(position)]=r;dry.process(l,r);
        correctionMix+=slew*((tuneAmount>0?1.f:0.f)-correctionMix);l+=(output[3][0][size_t(position)]-l)*correctionMix;r+=(output[3][1][size_t(position)]-r)*correctionMix;
        doubleLevel+=slew*(doubles-doubleLevel);harmonyLevel+=slew*(harmony-harmonyLevel);
        l+=harmonyLevel*.7f*output[0][0][size_t(position)]+doubleLevel*.325f*(output[1][0][size_t(position)]+output[2][0][size_t(position)]);
        r+=harmonyLevel*.7f*output[0][1][size_t(position)]+doubleLevel*.325f*(output[1][1][size_t(position)]+output[2][1][size_t(position)]);
        if(width!=1){const float mid=(l+r)*.5f,side=(l-r)*.5f*std::clamp(width,0.0f,2.0f);l=mid+side;r=mid-side;}
        if(++position==chunk){position=0;
            const float wanted=tracker.certainty()>.8f?PitchTracker::correction(tracker.frequency(),key,scale)*tuneAmount:0;
            tune+=float(1-std::exp(-chunk/(sampleRate*std::max(1.f,tuneSpeed)*.001)))*(wanted-tune);
            const float pitches[]{semitones+tune,.07f+tune,-.09f+tune,tune};
            for(size_t i=0;i<shifts.size();++i){if((i==0?harmonyLevel:(i==3?correctionMix:doubleLevel))<1.0e-6f){output[i]={};continue;}
                shifts[i].setTransposeSemitones(pitches[i]);if(tracker.frequency()>0)shifts[i].setFormantBase(float(tracker.frequency()/sampleRate));else shifts[i].setFormantBase(0);shifts[i].setFormantFactor(std::clamp(formant,.65f,1.6f),preserve);
                shifts[i].process(input,chunk,output[i],chunk);}}
    }
};
}
