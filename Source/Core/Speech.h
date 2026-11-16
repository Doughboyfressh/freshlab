#pragma once
#include "Dsp.h"
#include <cctype>
#include <map>
#include <string>
#include <stdexcept>

namespace fresh {
struct VoiceDesign {
    int character=0;
    float pitch=0,formant=1,breath=.08f,grit=0,robot=0,speed=1,width=.25f;
};

// Original rule-based formant speech. No speech recordings or model weights.
// Deliberately electronic; spelling-to-sound fallback is not a linguistic model.
class Speech {
    struct Phone { double f1=500,f2=1500,f3=2500,duration=.09;float voiced=1,noise=0,gain=1; };
    static Phone phone(const std::string& s) {
        if(s=="a")return {760,1150,2500,.13,1,0,1};
        if(s=="ae")return {720,1750,2450,.12,1,0,1};
        if(s=="e")return {530,1840,2500,.11,1,0,1};
        if(s=="i")return {330,2150,2900,.11,1,0,1};
        if(s=="o")return {480,880,2450,.14,1,0,1};
        if(s=="u")return {330,680,2250,.13,1,0,1};
        if(s=="r")return {340,1100,1650,.075,1,.06f,.8f};
        if(s=="l")return {400,1050,2700,.07,1,0,.8f};
        if(s=="m"||s=="n")return {250,950,2200,.07,1,.04f,.65f};
        if(s=="s"||s=="z")return {3800,5700,7100,.075,s=="z"?.25f:0,1,.25f};
        if(s=="sh"||s=="ch")return {1900,3200,4900,.08,0,1,.3f};
        if(s=="f"||s=="v"||s=="th")return {1100,3900,6200,.065,s=="v"?.45f:0,.8f,.23f};
        if(s=="h")return {600,1700,2800,.055,0,.8f,.35f};
        if(s=="w")return {290,650,2100,.065,1,0,.7f};
        if(s=="y")return {270,2300,3000,.055,1,0,.8f};
        if(s=="b"||s=="p")return {350,1000,2200,.045,s=="b"?.5f:0,.6f,.6f};
        if(s=="d"||s=="t")return {600,2400,4100,.04,s=="d"?.4f:0,.8f,.4f};
        if(s=="g"||s=="k")return {500,1600,3000,.045,s=="g"?.5f:0,.8f,.5f};
        if(s=="_")return {500,1500,2500,.065,0,0,0};
        return {500,1500,2500,.07,1,0,.7f};
    }
    static std::vector<std::string> spell(std::string text) {
        static const std::map<std::string,std::vector<std::string>> dictionary {
            {"yeah",{"y","ae","e"}},{"hey",{"h","e","i"}},{"whoa",{"w","u","o","a"}},
            {"go",{"g","o","u"}},{"oh",{"o","u"}},{"ah",{"a"}},{"brr",{"b","r","r","u"}},
            {"fresh",{"f","r","e","sh"}},{"freshlab",{"f","r","e","sh","l","ae","b"}},
            {"lets",{"l","e","t","s"}},{"let's",{"l","e","t","s"}},
            {"turn",{"t","u","r","n"}},{"it",{"i","t"}},{"up",{"a","p"}},
            {"we",{"w","i"}},{"outside",{"a","u","t","s","a","i","d"}},
            {"okay",{"o","k","e","i"}},{"money",{"m","a","n","i"}},
            {"one",{"w","a","n"}},{"two",{"t","u"}},{"three",{"th","r","i"}},
            {"four",{"f","o","r"}},{"five",{"f","a","i","v"}},
            {"six",{"s","i","k","s"}},{"seven",{"s","e","v","e","n"}},
            {"eight",{"e","i","t"}},{"nine",{"n","a","i","n"}},{"zero",{"z","i","r","o"}}
        };
        std::vector<std::string> result;std::string word;
        auto flush=[&]{
            if(word.empty())return;
            if(auto it=dictionary.find(word);it!=dictionary.end())result.insert(result.end(),it->second.begin(),it->second.end());
            else for(size_t i=0;i<word.size();++i){
                const auto pair=word.substr(i,2);
                if(pair=="sh"||pair=="ch"||pair=="th"){result.push_back(pair);++i;}
                else if(pair=="ee"||pair=="ea"){result.push_back("i");++i;}
                else if(pair=="oo"){result.push_back("u");++i;}
                else if(pair=="ow"||pair=="ou"){result.push_back("a");result.push_back("u");++i;}
                else if(pair=="ai"||pair=="ay"){result.push_back("e");result.push_back("i");++i;}
                else {char c=word[i];if(c=='c'||c=='q')c='k';if(c=='x'){result.push_back("k");c='s';}if(c=='j'){result.push_back("d");result.push_back("sh");}else if(c!='\'')result.push_back(std::string(1,c));}
            }
            result.push_back("_");word.clear();
        };
        for(unsigned char c:text){
            if(c>=128)throw std::invalid_argument("Use English letters, numbers, spaces and punctuation.");
            if(std::isalpha(c)||c=='\'')word.push_back(char(std::tolower(c)));
            else if(std::isdigit(c)){flush();static const char* names[]={"zero","one","two","three","four","five","six","seven","eight","nine"};word=names[c-'0'];flush();}
            else {flush();if(c=='.'||c=='!'||c=='?'||c==',')result.push_back("_");}
        }
        flush();return result;
    }
public:
    static Sample render(const std::string& text,const VoiceDesign& design) {
        if(text.empty()||text.size()>100)throw std::invalid_argument("Type between 1 and 100 characters.");
        auto phonemes=spell(text);
        if(phonemes.empty()||std::none_of(phonemes.begin(),phonemes.end(),[](const auto& s){return s!="_";}))throw std::invalid_argument("Type a phrase containing letters or numbers.");
        const float speed=std::clamp(design.speed,.6f,1.6f);
        double total=.06;for(auto&v:phonemes)total+=phone(v).duration/speed;
        if(total>12)throw std::invalid_argument("Phrase is over 12 seconds. Shorten it or increase Speed.");
        Sample sample;sample.rate=44100;const int count=int(total*sample.rate);
        sample.left.reserve(size_t(count));sample.right.reserve(size_t(count));
        const double bases[]{91,225,164,124},scales[]{.86,1.17,1.03,1.0};
        const int style=std::clamp(design.character,0,3);
        const double base=bases[style]*std::exp2(std::clamp(design.pitch,-24.0f,24.0f)/12);
        const double formant=scales[style]*std::clamp(design.formant,.65f,1.6f);
        double phase=0,time=0;uint32_t random=0x91ef7a3u;
        std::array<std::array<double,2>,3> memory{};
        auto noise=[&]{random^=random<<13;random^=random>>17;random^=random<<5;return (random/4294967295.0)*2-1;};
        for(size_t k=0;k<phonemes.size();++k){
            const Phone current=phone(phonemes[k]);const Phone next=phone(phonemes[std::min(k+1,phonemes.size()-1)]);
            const int frames=int(current.duration/speed*sample.rate);
            for(int n=0;n<frames;++n){
                const double t=double(n)/std::max(1,frames-1),blend=std::clamp((t-.65)/.35,0.0,1.0);
                const double f0=base*(1.08-.15*std::min(1.0,time/total))*(1+.012*std::sin(2*pi*5.3*time)*(1-design.robot));
                phase+=f0/sample.rate;phase-=std::floor(phase);
                // A harmonic glottal source with a bounded number of harmonics.
                double excitation=0;for(int h=1;h<=18;++h)excitation+=std::sin(2*pi*phase*h)/std::pow(double(h),1.4);
                const double voiced=current.voiced+(next.voiced-current.voiced)*blend;
                const double air=current.noise+(next.noise-current.noise)*blend;
                const double source=excitation*.3*voiced+noise()*(air*.6+std::clamp(design.breath,0.0f,1.0f)*.23)*current.gain;
                const double freq[]{current.f1+(next.f1-current.f1)*blend,current.f2+(next.f2-current.f2)*blend,current.f3+(next.f3-current.f3)*blend};
                double output=0;
                for(int b=0;b<3;++b){const double radius=std::exp(-pi*(85+b*55)/sample.rate);
                    const double y=source*(1-radius)+2*radius*std::cos(2*pi*freq[b]*formant/sample.rate)*memory[b][0]-radius*radius*memory[b][1];
                    memory[b][1]=memory[b][0];memory[b][0]=y;output+=y*(b==0?1:(b==1?.65:.35));
                }
                output*=1-design.robot*.55+design.robot*.55*std::cos(2*pi*base*1.47*time);
                output=std::tanh(output*(2.0+design.grit*12));
                const double envelope=std::min(1.0,n/(sample.rate*.006))*std::min(1.0,(frames-n-1)/(sample.rate*.008))*current.gain;
                sample.left.push_back(float(output*envelope));time+=1/sample.rate;
            }
        }
        sample.left.resize(sample.left.size()+2205,0);sample.right.resize(sample.left.size());
        float peak=.001f;for(auto x:sample.left)peak=std::max(peak,std::abs(x));
        const size_t spread=size_t(sample.rate*.012*std::clamp(design.width,0.0f,1.0f));
        for(size_t i=0;i<sample.left.size();++i){sample.left[i]*=.82f/peak;sample.right[i]=sample.left[i];}
        if(spread)for(size_t i=sample.left.size();i-->0;)sample.right[i]=i>=spread?sample.left[i-spread]:0;
        return sample;
    }
};
} // namespace fresh
