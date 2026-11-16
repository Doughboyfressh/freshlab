#include "Core/Speech.h"
#include "Core/Sequencer.h"
#include <iostream>
#include <stdexcept>
#include <complex>
using namespace fresh;
static int checks=0;
void check(bool ok,const char* label){++checks;if(!ok)throw std::runtime_error(label);}
float signalEnergy(const Sample& a){double sum=0;for(auto x:a.left){check(std::isfinite(x)&&std::abs(x)<=.821f,"voice finite and normalized");sum+=x*x;}return float(sum/a.left.size());}
double toneAmplitude(const std::vector<float>& data,double hz,double rate){std::complex<double> sum{};for(size_t i=0;i<data.size();++i)sum+=double(data[i])*std::polar(1.0,-2*pi*hz*i/rate);return std::abs(sum)*2/data.size();}
int main(){try{
    VoiceDesign d;auto a=Speech::render("Let's go",d);check(a.valid()&&signalEnergy(a)>.001f,"typed adlib audible");check(a.left.front()==0&&a.left.back()==0&&a.right.back()==0,"voice boundaries clean");
    auto b=Speech::render("Let's go",d);check(a.left==b.left&&a.right==b.right,"speech deterministic");
    auto c=Speech::render("FreshLab 123",d);check(c.left!=a.left,"different text changes voice");
    d.character=3;d.formant=1.35f;d.grit=.6f;d.robot=.8f;d.width=1;auto custom=Speech::render("Let's go",d);check(custom.left!=a.left&&custom.right!=custom.left,"designer changes timbre and stereo");signalEnergy(custom);
    d.speed=1.6f;auto fast=Speech::render("Let's go",d);check(fast.left.size()<custom.left.size(),"speech speed changes duration");
    for(auto text:{"","!!!", "\xc3\xa9"}){bool threw=false;try{Speech::render(text,d);}catch(const std::invalid_argument&){threw=true;}check(threw,"invalid speech text rejected");}
    bool tooLong=false;try{Speech::render(std::string(100,'a'),VoiceDesign{});}catch(const std::invalid_argument&){tooLong=true;}check(tooLong,"overlong rendered phrase rejected");
    {
        Sequencer s;SequencePatch p;for(auto& step:p.steps)step.action=1;
        int events=0;for(int i=0;i<88200;++i)if(s.tick(i/22050.0,p).trigger)++events;check(events==16,"one bar produces 16 hits");
        s.reset();p.steps[0].action=2;p.steps[1].action=3;events=0;for(int i=0;i<88200;++i)if(s.tick(i/22050.0,p).trigger)++events;check(events==19,"double and triple repeats count");
        s.reset();p={};p.steps[1].action=4;p.steps[1].pitch=-5;p.steps[1].velocity=.4f;p.swing=.3;
        int first=-1;for(int i=0;i<22050;++i){auto e=s.tick(i/22050.0,p);if(e.trigger){first=i;check(e.reverse&&e.pitch==-5&&e.velocity==.4f,"step articulation preserved");break;}}
        check(std::abs(first-int(.325*22050))<=1,"swing is sample accurate");s.reset();check(!s.tick(-1,p).trigger,"negative timeline ignored");
        p.steps[0].action=5;check(s.tick(0,p).throwDelay,"echo throw event");check(!s.tick(0,p).trigger,"same sample does not retrigger");
        s.tick(2,p);check(s.tick(0,p).trigger,"loop restart retriggers");
    }
    for(double rate:{22050,44100,96000}){
        VocalLayers v;v.prepare(rate);std::vector<float> wet;
        for(int i=0;i<int(rate);++i){float l=float(std::sin(2*pi*440*i/rate))*.2f,r=l,original=l;v.process(l,r,0,1,7,1,1,false);if(i>rate*.4)wet.push_back(l-original);check(std::isfinite(l)&&std::isfinite(r),"harmonizer stable");}
        check(toneAmplitude(wet,440*std::exp2(7.0/12),rate)>.01,"harmony contains requested interval");
        Effects dry,dynamic;dry.prepare(rate);dynamic.prepare(rate);FxPatch plain;plain.output=0;plain.ratio=1;
        FxPatch cut=plain;cut.dynamicHz=3200;cut.dynamicQ=2;cut.dynamicRange=12;cut.dynamicThreshold=-30;dry.configure(plain);dynamic.configure(cut);double e0=0,e1=0;
        for(int i=0;i<int(rate*.6);++i){float l=float(std::sin(2*pi*3200*i/rate))*.4f,r=l,a=l,b=r;dry.process(l,r,plain);dynamic.process(a,b,cut);if(i>rate*.3){e0+=l*l;e1+=a*a;}}
        check(e1<e0*.35,"dynamic EQ reduces hot center frequency");check(dynamic.reduction()>8,"dynamic EQ reports gain reduction");
        Synth synth;Patch p;p.drop=0;p.bassSub=0;synth.prepare(rate);synth.noteOn(33,1,1,p,nullptr);float l,r;synth.render(l,r,p,nullptr);check(l==0,"808 layers can be muted");p.bassBuzz=1;double energy=0;for(int i=0;i<2000;++i){synth.render(l,r,p,nullptr);energy+=l*l;}check(energy>.01,"808 buzz layer produces sound");
        p.curveEnabled=true;p.curve={36,24,12,0,-12,-24,-24,-24};p.bassSub=1;p.bassPunch=1;p.bassClick=1;p.bassDrive=24;synth.allOff();synth.noteOn(33,1,1,p,nullptr);for(int i=0;i<5000;++i){synth.render(l,r,p,nullptr);check(std::isfinite(l)&&std::isfinite(r),"drawn 808 curve and layers stable");}
    }
    std::cout<<"PASS: "<<checks<<" extension assertions / speech, sequencer, harmony, dynamic EQ, 808 layers\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
