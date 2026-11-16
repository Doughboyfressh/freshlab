#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "SystemSpeech.h"
#include <BinaryData.h>

using namespace juce;
namespace {
int parameterVersion(const String& id){
    static const StringArray addedIn03{"mpe","legato","bendRange","speechEngine","compAttack","compRelease","roomSize","roomDecay","roomDamping","dynamicAttack","dynamicRelease","limiterCeiling","limiterRelease","vocalFormant","vocalQuality","tuneAmount","tuneSpeed","tuneKey","tuneScale","externalSidechain","delaySync","delayDivision"};
    return addedIn03.contains(id)?2:1;
}
}
AudioProcessorValueTreeState::ParameterLayout FreshLabProcessor::layout() {
    AudioProcessorValueTreeState::ParameterLayout p;
    auto f=[&](const char* id,const char* name,float lo,float hi,float def,float skew=1.0f){
        p.add(std::make_unique<AudioParameterFloat>(ParameterID{id,parameterVersion(id)},name,NormalisableRange<float>{lo,hi,0,skew},def));
    };
    auto choice=[&](const char* id,const char* name,StringArray labels,int def){p.add(std::make_unique<AudioParameterChoice>(ParameterID{id,parameterVersion(id)},name,labels,def));};
    auto b=[&](const char* id,const char* name,bool def){p.add(std::make_unique<AudioParameterBool>(ParameterID{id,parameterVersion(id)},name,def));};
    choice("mode","Engine",{"808","Synth","Adlib"},0);choice("wave","Waveform",{"Sine","Saw","Square","Triangle"},1);
    f("attack","Attack (s)",.001f,2,.005f,.35f);f("decay","Decay (s)",.03f,8,1.2f,.4f);
    f("sustain","Sustain",0,1,.65f);f("release","Release (s)",.005f,4,.15f,.4f);
    f("glide","808 glide (s)",0,.5f,.04f);f("drop","808 pitch drop (st)",0,36,12);
    f("detune","Synth detune (ct)",0,40,7);f("cutoff","Synth lowpass (Hz)",40,20000,8000,.3f);
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"root",1},"Sample root MIDI note",24,96,60));
    f("samplePitch","Adlib transpose (st)",-24,24,0);f("sampleStart","Adlib start",0,.95f,0);
    b("reverse","Adlib reverse",false);b("oneShot","Adlib one shot",true);b("bypass","Effects bypass",false);
    f("input","Input (dB)",-24,24,0);f("output","Output (dB)",-30,6,-6);f("mix","Effects mix",0,1,1);
    f("drive","Drive (dB)",0,30,0);f("distMix","Distortion mix",0,1,0);
    choice("distType","Distortion",{"Soft clip","Hard clip","Foldback"},0);
    f("highPass","High pass (Hz)",20,1500,20,.35f);
    f("lowHz","Low shelf (Hz)",30,500,100,.4f);f("lowGain","Low shelf (dB)",-18,18,0);
    f("mid1Hz","Low mid (Hz)",80,4000,400,.35f);f("mid1Gain","Low mid (dB)",-18,18,0);f("mid1Q","Low mid Q",.2f,8,1,.5f);
    f("mid2Hz","High mid (Hz)",500,12000,2000,.4f);f("mid2Gain","High mid (dB)",-18,18,0);f("mid2Q","High mid Q",.2f,8,1,.5f);
    f("highHz","High shelf (Hz)",1500,18000,8000,.4f);f("highGain","High shelf (dB)",-18,18,0);
    f("threshold","Compressor threshold (dB)",-48,0,0);f("ratio","Compressor ratio",1,12,3);
    f("deess","De ess",0,1,0);f("delayMs","Delay (ms)",30,1500,280,.5f);
    f("feedback","Delay feedback",0,.85f,.25f);f("delayMix","Delay send",0,1,0);f("reverb","Room send",0,1,0);
    f("bassSub","808 sub layer",0,1,1);f("bassPunch","808 punch layer",0,1,0);f("bassClick","808 click layer",0,1,0);
    f("bassBuzz","808 buzz layer",0,1,0);f("bassDrive","808 saturation (dB)",0,24,0);f("curveTime","808 pitch curve length (s)",.03f,1,.25f);
    b("curveEnabled","808 custom pitch curve",false);
    const float curve[]{24,12,5,1,0,0,0,0};for(int i=0;i<8;++i)f(("curve"+String(i)).toRawUTF8(),("808 pitch point "+String(i+1)).toRawUTF8(),-24,36,curve[i]);
    choice("voiceStyle","Voice character",{"Sub","Spark","Haze","Circuit"},0);
    f("voicePitch","Voice design pitch (st)",-24,24,0);f("voiceFormant","Voice formant",.65f,1.6f,1);
    f("voiceBreath","Voice breath",0,1,.08f);f("voiceGrit","Voice grit",0,1,0);f("voiceRobot","Voice robot",0,1,0);
    f("voiceSpeed","Voice speech speed",.6f,1.6f,1);f("voiceWidth","Voice stereo spread",0,1,.25f);
    f("doubles","Vocal doubler",0,1,0);f("harmony","Vocal harmony",0,1,0);f("width","Stereo width",0,2,1);
    p.add(std::make_unique<AudioParameterInt>(ParameterID{"harmonyPitch",1},"Harmony semitones",-12,12,7));
    f("dynamicHz","Dynamic EQ center (Hz)",100,14000,3200,.35f);f("dynamicQ","Dynamic EQ Q",.3f,8,1.5f);
    f("dynamicThreshold","Dynamic EQ threshold (dB)",-60,0,-20);f("dynamicRange","Dynamic EQ max cut (dB)",0,18,0);
    b("seqEnabled","Run adlib sequencer",false);b("seqSync","Sequencer follows host",true);
    f("seqBpm","Sequencer internal BPM",40,240,140);f("seqSwing","Sequencer swing",0,.45f,0);
    choice("seqDivision","Step duration",{"1/8","1/16","1/32"},1);choice("seqLength","Pattern length",{"4 steps","8 steps","16 steps"},2);
    for(int i=0;i<16;++i){const String prefix="step"+String(i);
        choice((prefix+"Action").toRawUTF8(),("Step "+String(i+1)+" action").toRawUTF8(),{"Off","Hit","Double","Triple","Reverse","Echo throw"},i%4==0?1:0);
        p.add(std::make_unique<AudioParameterInt>(ParameterID{prefix+"Pitch",1},"Step "+String(i+1)+" pitch",-24,24,0));
        f((prefix+"Velocity").toRawUTF8(),("Step "+String(i+1)+" velocity").toRawUTF8(),0,1,.85f);
    }
    b("mpe","MPE lower zone",false);b("legato","Legato portamento",false);f("bendRange","Pitch bend range (st)",1,48,2);
    choice("speechEngine","Speech engine",{"Original electronic","Installed system voice"},0);
    f("compAttack","Compressor attack (ms)",.1f,200,5,.4f);f("compRelease","Compressor release (ms)",5,2000,80,.4f);
    f("roomSize","Room size",.25f,1.8f,1);f("roomDecay","Room decay (s)",.2f,8,1.5f,.5f);f("roomDamping","Room damping",0,.95f,.7f);
    f("dynamicAttack","Dynamic EQ attack (ms)",.1f,200,6,.4f);f("dynamicRelease","Dynamic EQ release (ms)",5,2000,120,.4f);
    f("limiterCeiling","Limiter ceiling (dB)",-12,0,-.3f);f("limiterRelease","Limiter release (ms)",10,1000,80,.4f);
    p.add(std::make_unique<AudioParameterChoice>(ParameterID{"vocalQuality",2},"Vocal quality",StringArray{"Live / creative","Studio / formants + tuning"},0,AudioParameterChoiceAttributes{}.withAutomatable(false)));
    f("vocalFormant","Vocal formant",.65f,1.6f,1);
    f("tuneAmount","Pitch correction amount",0,1,0);f("tuneSpeed","Pitch correction response (ms)",1,300,60,.5f);
    choice("tuneKey","Pitch correction root",{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"},0);
    choice("tuneScale","Pitch correction scale",{"Chromatic","Major","Minor","Major pentatonic"},0);
    b("externalSidechain","External compressor sidechain",false);b("delaySync","Delay follows host tempo",false);
    choice("delayDivision","Synced delay length",{"1/32","1/16","1/8 triplet","1/8","1/8 dotted","1/4","1/4 dotted","1/2"},5);
    b("padMidi","MIDI notes 36-43 trigger pads",false);
    auto names=voiceNames();names.insert(0,"Current custom voice");const int notes[]{33,36,40,43,60,64,67,72};
    for(int i=0;i<8;++i){const String prefix="pad"+String(i);
        choice((prefix+"Engine").toRawUTF8(),("Pad "+String(i+1)+" engine").toRawUTF8(),{"808","Synth","Voice"},i<4?0:2);
        p.add(std::make_unique<AudioParameterInt>(ParameterID{prefix+"Note",1},"Pad "+String(i+1)+" MIDI note",24,96,notes[i]));
        choice((prefix+"Voice").toRawUTF8(),("Pad "+String(i+1)+" voice").toRawUTF8(),names,i<4?0:(i-4)*6+1);
        choice((prefix+"Macro").toRawUTF8(),("Pad "+String(i+1)+" effect").toRawUTF8(),{"Clean","Drive","Echo","Space"},i<5?0:i-4);
    }
    return p;
}

FreshLabProcessor::FreshLabProcessor()
 : AudioProcessor(BusesProperties()
 #if !JucePlugin_IsSynth
    .withInput("Input",AudioChannelSet::stereo(),true).withInput("Sidechain",AudioChannelSet::stereo(),false)
 #endif
    .withOutput("Output",AudioChannelSet::stereo(),true)),state(*this,nullptr,"FreshLab",layout()) {
    formats.registerBasicFormats();
    for(auto* parameter:getParameters())if(auto* p=dynamic_cast<AudioProcessorParameterWithID*>(parameter))
        values.emplace(p->paramID.toStdString(),state.getRawParameterValue(p->paramID));
    brand.setProperty("title","FRESHLAB",nullptr);brand.setProperty("tagline","YOUR SOUND. YOUR SIGNATURE.",nullptr);
    brand.setProperty("accent","96f2cc",nullptr);brand.setProperty("theme",0,nullptr);brand.setProperty("knobStyle",0,nullptr);brand.setProperty("animate",true,nullptr);
    if(instrument){
        const char* styles[]={"sub","spark","haze","circuit"};const char* words[]={"yeah","hey","whoa","go","ah","brr"};
        for(int i=0;i<24;++i){const String resource=String(styles[i/6])+"_"+words[i%6]+"_wav";int size=0;const auto* bytes=BinaryData::getNamedResource(resource.toRawUTF8(),size);
            if(bytes)bank[size_t(i)]=readAudio(std::unique_ptr<AudioFormatReader>(formats.createReaderFor(std::make_unique<MemoryInputStream>(bytes,size,false))));
        }
        loadFactory(0);
    }
    startTimerHz(30);
}
bool FreshLabProcessor::isBusesLayoutSupported(const BusesLayout& l) const {
    const auto out=l.getMainOutputChannelSet();
    if(out!=AudioChannelSet::mono()&&out!=AudioChannelSet::stereo())return false;
    if(!instrument&&l.getMainInputChannelSet()!=out)return false;
    if(!instrument&&l.inputBuses.size()>1){const auto side=l.inputBuses[1];if(!side.isDisabled()&&side!=AudioChannelSet::mono()&&side!=AudioChannelSet::stereo())return false;}
    return true;
}
void FreshLabProcessor::prepareToPlay(double sr,int) { synth.prepare(sr);for(auto& s:padSynths)s.prepare(sr);sequenceSynth.prepare(sr);effects.prepare(sr);auto fx=fxPatch();effects.configure(fx);appliedQuality=fx.vocalQuality;pendingLatency=effects.latency();setLatencySamples(effects.latency());sequencer.reset();localBeats=0; }
void FreshLabProcessor::stopVoices(){synth.allOff();for(auto& s:padSynths)s.allOff();sequenceSynth.allOff();sequenceGate=-1;activeStep=-1;}
void FreshLabProcessor::reset(){stopVoices();synth.reset();effects.reset();keyboard.reset();sequencer.reset();localBeats=0;meterL=0;meterR=0;macroSeconds=0;pendingPads=0;}
fresh::Patch FreshLabProcessor::patch() const {
    fresh::Patch p;
    p.mpe=value("mpe")>.5f;p.legato=value("legato")>.5f;p.bendRange=value("bendRange");
    p.mode=int(value("mode"));p.wave=int(value("wave"));p.attack=value("attack");p.decay=value("decay");
    p.sustain=value("sustain");p.release=value("release");p.glide=value("glide");p.drop=value("drop");
    p.detune=value("detune");p.cutoff=value("cutoff");p.root=int(value("root"));p.samplePitch=value("samplePitch");
    p.sampleStart=value("sampleStart");p.reverse=value("reverse")>.5f;p.oneShot=value("oneShot")>.5f;
    p.bassSub=value("bassSub");p.bassPunch=value("bassPunch");p.bassClick=value("bassClick");p.bassBuzz=value("bassBuzz");p.bassDrive=value("bassDrive");
    p.curveTime=value("curveTime");p.curveEnabled=value("curveEnabled")>.5f;
    for(int i=0;i<8;++i){const std::string id="curve"+std::to_string(i);p.curve[size_t(i)]=values.find(id)->second->load();}return p;
}
fresh::FxPatch FreshLabProcessor::fxPatch() const {
    fresh::FxPatch p;
    #define GET(x) p.x=value(#x)
    GET(input);GET(output);GET(mix);GET(drive);GET(distMix);GET(highPass);GET(lowHz);GET(lowGain);
    GET(mid1Hz);GET(mid1Gain);GET(mid1Q);GET(mid2Hz);GET(mid2Gain);GET(mid2Q);GET(highHz);GET(highGain);
    GET(threshold);GET(ratio);GET(deess);GET(delayMs);GET(feedback);GET(delayMix);GET(reverb);
    GET(doubles);GET(harmony);GET(harmonyPitch);GET(width);GET(vocalFormant);GET(tuneAmount);GET(tuneSpeed);
    GET(compAttack);GET(compRelease);GET(roomSize);GET(roomDecay);GET(roomDamping);GET(dynamicAttack);GET(dynamicRelease);GET(limiterCeiling);GET(limiterRelease);GET(dynamicHz);GET(dynamicQ);GET(dynamicThreshold);GET(dynamicRange);
    #undef GET
    p.vocalQuality=int(value("vocalQuality"));p.tuneKey=int(value("tuneKey"));p.tuneScale=int(value("tuneScale"));p.externalSidechain=value("externalSidechain")>.5f;p.distType=int(value("distType"));p.bypass=value("bypass")>.5f;return p;
}
fresh::SequencePatch FreshLabProcessor::sequencePatch()const {
    fresh::SequencePatch p;p.division=2<<int(value("seqDivision"));p.length=4<<int(value("seqLength"));p.swing=value("seqSwing");
    for(int i=0;i<16;++i){const std::string key="step"+std::to_string(i);auto get=[&](const char* suffix){return values.find(key+suffix)->second->load();};p.steps[size_t(i)]={int(get("Action")),int(get("Pitch")),get("Velocity")};}return p;
}
void FreshLabProcessor::setStep(int i,int action,int pitch,float velocity){if(i<0||i>=16)return;const String id="step"+String(i);setValue(id+"Action",float(action));setValue(id+"Pitch",float(pitch));setValue(id+"Velocity",velocity);}
void FreshLabProcessor::playPad(int index,const fresh::Patch& source,float velocity){
    const std::string key="pad"+std::to_string(index);auto get=[&](const char* suffix){return values.find(key+suffix)->second->load();};
    auto p=source;p.mode=int(get("Engine"));p.oneShot=p.mode==2;
    const int voice=int(get("Voice"));const auto* audio=voice>0?&bank[size_t(voice-1)]:currentAudioSample;
    padSynths[size_t(index)].allOff();padSynths[size_t(index)].noteOn(int(get("Note")),velocity,1,p,audio);
    padNotes[size_t(index)]=int(get("Note"));padGates[size_t(index)]=p.mode==1?.5:-1;
    padMeters[size_t(index)]=1;macro=int(get("Macro"));macroSeconds=2;
}
void FreshLabProcessor::midi(const MidiMessage& m,const fresh::Patch& p){
    if(value("padMidi")>.5f&&(m.isNoteOn()||m.isNoteOff())&&m.getNoteNumber()>=36&&m.getNoteNumber()<44){
        const int index=m.getNoteNumber()-36;if(m.isNoteOn())playPad(index,p,m.getFloatVelocity());
        else {auto pp=p;const auto id="pad"+std::to_string(index);pp.mode=int(values.find(id+"Engine")->second->load());pp.oneShot=pp.mode==2;padSynths[size_t(index)].noteOff(int(values.find(id+"Note")->second->load()),1,pp);}return;
    }
    if(m.isNoteOn())synth.noteOn(m.getNoteNumber(),m.getFloatVelocity(),m.getChannel(),p,currentAudioSample);
    else if(m.isNoteOff())synth.noteOff(m.getNoteNumber(),m.getChannel(),p);
    else if(m.isController()){
        if(p.mpe&&m.getChannel()==1&&m.getControllerNumber()==64){for(int channel=1;channel<=16;++channel)synth.controller(64,m.getControllerValue(),channel,p);}
        else synth.controller(m.getControllerNumber(),m.getControllerValue(),m.getChannel(),p);
        if((m.getControllerNumber()==120||m.getControllerNumber()==123)&&(!p.mpe||m.getChannel()==1))stopVoices();
    }
    else if(m.isPitchWheel())synth.pitchBend(m.getPitchWheelValue(),m.getChannel(),p.mpe,p.bendRange);
    else if(m.isChannelPressure())synth.channelPressure(m.getChannelPressureValue()/127.f,m.getChannel());
    else if(m.isAftertouch())synth.polyPressure(m.getAfterTouchValue()/127.f,m.getNoteNumber(),m.getChannel());
    else if(m.isAllSoundOff())synth.allOff(m.getChannel());
}
void FreshLabProcessor::processBlock(AudioBuffer<float>& buffer,MidiBuffer& messages){processAudio(buffer,messages,false);}
void FreshLabProcessor::processBlockBypassed(AudioBuffer<float>& buffer,MidiBuffer& messages){
    if(instrument){buffer.clear();messages.clear();stopVoices();}processAudio(buffer,messages,true);
}
void FreshLabProcessor::processAudio(AudioBuffer<float>& buffer,MidiBuffer& messages,bool hostBypass) {
    ScopedNoDenormals noDenormals;
    if(instrument&&!hostBypass)keyboard.processNextMidiBuffer(messages,0,buffer.getNumSamples(),true);
    if(resetRequested.exchange(false)){stopVoices();effects.reset();sequencer.reset();macroSeconds=0;}
    if(panicRequested.exchange(false)){stopVoices();effects.reset();messages.clear();pendingPads=0;macroSeconds=0;sequencer.reset();}
    const auto p=patch();auto fx=fxPatch();fx.bypass=fx.bypass||hostBypass;
    struct SampleGuard {std::atomic<const fresh::Sample*>& hazard;~SampleGuard(){hazard.store(nullptr);}}sampleGuard{sampleHazard};
    currentAudioSample=nullptr;
    for(int attempt=0;attempt<3;++attempt){const auto* candidate=audioSample.load();sampleHazard.store(candidate);
        if(candidate==audioSample.load()){currentAudioSample=candidate;break;}}
    if(!currentAudioSample)sampleHazard.store(nullptr);
    const double sr=getSampleRate()>0?getSampleRate():44100;
    const bool enabled=instrument&&value("seqEnabled")>.5f,sync=value("seqSync")>.5f;
    bool hostPlaying=false;bool running=enabled&&!sync;double bpm=instrument?value("seqBpm"):120.,beats=localBeats;
    if(auto* head=getPlayHead())if(auto position=head->getPosition()){
        hostPlaying=position->getIsPlaying();if(enabled&&sync)running=position->getIsPlaying()&&position->getPpqPosition().hasValue();
        bpm=position->getBpm().orFallback(120.0);beats=position->getPpqPosition().orFallback(0.0);
    }
    bpm=std::isfinite(bpm)?std::clamp(bpm,20.0,400.0):120.;if(!std::isfinite(beats)){beats=0;running=false;}
    if(value("delaySync")>.5f){const double beatsPerEcho[]{.125,.25,1./3,.5,.75,1,1.5,2};fx.delayMs=float(std::clamp(60000/bpm*beatsPerEcho[std::clamp(int(value("delayDivision")),0,7)],1.,7990.));}
    if(hostPlaying)fx.vocalQuality=appliedQuality.load();else appliedQuality=fx.vocalQuality;
    effects.configure(fx);pendingLatency=effects.latency();delayTimeMs=fx.delayMs;
    AudioBuffer<float> sidechain;
    if(!instrument&&getBusCount(true)>1&&getBus(true,1)->isEnabled())sidechain=getBusBuffer(buffer,true,1);
    const double beatStep=bpm/(60*sr);const auto pattern=sequencePatch();
    if(!running){sequencer.reset();sequenceSynth.allOff();sequenceGate=-1;activeStep=-1;}
    std::array<fresh::Patch,8> pads;std::array<const fresh::Sample*,8> audio{};
    for(int index=0;index<8;++index){const std::string key="pad"+std::to_string(index);pads[size_t(index)]=p;
        pads[size_t(index)].mode=int(values.find(key+"Engine")->second->load());pads[size_t(index)].oneShot=pads[size_t(index)].mode==2;
        const int voice=int(values.find(key+"Voice")->second->load());audio[size_t(index)]=voice>0?&bank[size_t(voice-1)]:currentAudioSample;
    }
    const unsigned queued=pendingPads.exchange(0);if(instrument)for(int i=0;i<8;++i)if(queued&(1u<<i))playPad(i,p);
    auto event=messages.cbegin();const auto end=messages.cend();float maxL=0,maxR=0;
    for(int i=0;i<buffer.getNumSamples();++i){
        if(instrument)while(event!=end&&(*event).samplePosition<=i){midi((*event).getMessage(),p);++event;}
        float l=0,r=0;
        if(instrument&&!hostBypass){
            synth.render(l,r,p,currentAudioSample);
            for(int index=0;index<8;++index)if(padSynths[size_t(index)].isActive()){
                if(padGates[size_t(index)]>0){padGates[size_t(index)]-=1/sr;if(padGates[size_t(index)]<=0)padSynths[size_t(index)].noteOff(padNotes[size_t(index)],1,pads[size_t(index)]);}
                float a,b;padSynths[size_t(index)].render(a,b,pads[size_t(index)],audio[size_t(index)]);l+=a;r+=b;
            }
            if(running){
                const auto e=sequencer.tick(beats,pattern);
                if(e.trigger){sequenceVoice=p;sequenceVoice.mode=2;sequenceVoice.reverse=p.reverse!=e.reverse;sequenceVoice.oneShot=false;sequenceVoice.attack=.003f;sequenceVoice.release=.012f;
                    sequenceNote=std::clamp(p.root+e.pitch,0,127);sequenceSynth.allOff();sequenceSynth.noteOn(sequenceNote,e.velocity,1,sequenceVoice,currentAudioSample);
                    sequenceGate=beats+e.gateBeats;activeStep=e.step;
                    if(e.throwDelay){macro=2;macroSeconds=60/bpm;}
                }
                if(sequenceGate>=0&&beats>=sequenceGate){sequenceSynth.noteOff(sequenceNote,1,sequenceVoice);sequenceGate=-1;}
                float a,b;sequenceSynth.render(a,b,sequenceVoice,currentAudioSample);l+=a;r+=b;
            }
        }
        else if(!instrument){l=buffer.getSample(0,i);r=getMainBusNumOutputChannels()>1?buffer.getSample(1,i):l;}
        auto effective=fx;
        if(macroSeconds>0){const float fade=float(std::min(1.0,macroSeconds/.05));
            if(macro==1){effective.drive=std::max(fx.drive,18*fade);effective.distMix=std::max(fx.distMix,.6f*fade);}
            if(macro==2)effective.delayMix=std::max(fx.delayMix,.6f*fade);
            if(macro==3)effective.reverb=std::max(fx.reverb,.65f*fade);
            macroSeconds-=1/sr;
        }
        float detector=-1;if(sidechain.getNumChannels()>0){detector=0;for(int c=0;c<sidechain.getNumChannels();++c)detector=std::max(detector,std::abs(sidechain.getSample(c,i)));}
        effects.process(l,r,effective,detector);
        buffer.setSample(0,i,l);if(getMainBusNumOutputChannels()>1)buffer.setSample(1,i,r);
        maxL=std::max(maxL,std::abs(l));maxR=std::max(maxR,std::abs(r));
        if(running)beats+=beatStep;
    }
    if(running&&!sync)localBeats=beats;else if(!enabled)localBeats=0;
    for(auto& level:padMeters)level.store(level.load()*.88f);
    dynamicMeter=effects.reduction();meterL.store(maxL);meterR.store(maxR);messages.clear();
}
void FreshLabProcessor::setValue(const String& id,float v){if(auto* p=state.getParameter(id)){p->beginChangeGesture();p->setValueNotifyingHost(p->convertTo0to1(v));p->endChangeGesture();}}
StringArray FreshLabProcessor::voiceNames(){StringArray a;for(auto s:{"Sub","Spark","Haze","Circuit"})for(auto p:{"Yeah","Hey","Whoa","Go","Ah","Brr"})a.add(String(s)+" / "+p);return a;}
String FreshLabProcessor::loadFactory(int index){
    if(index<0||index>=24)return "Unknown voice";
    if(!bank[size_t(index)].valid())return "Voice resource missing";
    installSample(bank[size_t(index)],voiceNames()[index]);return {};
}
String FreshLabProcessor::loadSample(const File& file){
    if(file.getSize()>32*1024*1024)return "Choose a file smaller than 32 MB.";
    return decode(std::unique_ptr<AudioFormatReader>(formats.createReaderFor(file)),file.getFileNameWithoutExtension());
}
fresh::Sample FreshLabProcessor::readAudio(std::unique_ptr<AudioFormatReader> reader){
    if(!reader)throw std::invalid_argument("Could not read this audio file. Try WAV or AIFF.");
    if(reader->sampleRate<8000||reader->sampleRate>192000||reader->lengthInSamples<2)throw std::invalid_argument("Unsupported sample rate or empty file.");
    if(reader->lengthInSamples>reader->sampleRate*12)throw std::invalid_argument("Choose an adlib 12 seconds or shorter.");
    AudioBuffer<float> b(2,int(reader->lengthInSamples));
    if(!reader->read(&b,0,b.getNumSamples(),0,true,true))throw std::invalid_argument("Audio read failed.");
    if(reader->numChannels==1)b.copyFrom(1,0,b,0,0,b.getNumSamples());
    fresh::Sample replacement;replacement.rate=reader->sampleRate;
    replacement.left.assign(b.getReadPointer(0),b.getReadPointer(0)+b.getNumSamples());
    replacement.right.assign(b.getReadPointer(1),b.getReadPointer(1)+b.getNumSamples());
    for(auto& v:replacement.left)v=std::clamp(fresh::finite(v),-1.0f,1.0f);
    for(auto& v:replacement.right)v=std::clamp(fresh::finite(v),-1.0f,1.0f);
    return replacement;
}
void FreshLabProcessor::publishAudioSample(){
    auto next=std::make_unique<const fresh::Sample>(sample);
    retiredSamples.reserve(retiredSamples.size()+1);
    if(audioSampleOwner)retiredSamples.push_back(std::move(audioSampleOwner));
    audioSampleOwner=std::move(next);audioSample.store(audioSampleOwner.get());
    const auto* protectedSample=sampleHazard.load();
    retiredSamples.erase(std::remove_if(retiredSamples.begin(),retiredSamples.end(),[protectedSample](const auto& p){return p.get()!=protectedSample;}),retiredSamples.end());
}
void FreshLabProcessor::installSample(fresh::Sample replacement,const String& title){const ScopedLock lock(sampleMutex);sample=std::move(replacement);sampleTitle=title.substring(0,64);publishAudioSample();}
String FreshLabProcessor::decode(std::unique_ptr<AudioFormatReader> reader,const String& title){try{installSample(readAudio(std::move(reader)),title);return {};}catch(const std::exception& e){return e.what();}}
String FreshLabProcessor::generateVoice(const String& text,const String& name){
    fresh::VoiceDesign d;d.character=int(value("voiceStyle"));d.pitch=value("voicePitch");d.formant=value("voiceFormant");d.breath=value("voiceBreath");d.grit=value("voiceGrit");d.robot=value("voiceRobot");d.speed=value("voiceSpeed");d.width=value("voiceWidth");
    try {fresh::Sample generated;
        if(value("speechEngine")>.5f){
            #if JUCE_MAC
            const char* extension=".aiff";
           #else
            const char* extension=".wav";
           #endif
            const auto rendered=File::getSpecialLocation(File::tempDirectory).getNonexistentChildFile("freshlab-installed-voice",extension,false);
            struct Cleanup {File file;~Cleanup(){file.deleteFile();}}cleanup{rendered};
            const auto error=fresh::renderSystemSpeech(text,d.speed,rendered);if(error.isNotEmpty())return error;
            generated=readAudio(std::unique_ptr<AudioFormatReader>(formats.createReaderFor(rendered)));
        }else generated=fresh::Speech::render(text.toStdString(),d);{const ScopedLock lock(sampleMutex);sample=std::move(generated);sampleTitle=name.isNotEmpty()?name.substring(0,64):text.substring(0,48);voiceText=text;publishAudioSample();}
        setValue("mode",2);setValue("root",60);setValue("samplePitch",0);setValue("sampleStart",0);setValue("reverse",0);return {};
    }catch(const std::exception& e){return e.what();}
}
String FreshLabProcessor::exportVoice(const File& file)const{
    fresh::Sample copy;{const ScopedLock lock(sampleMutex);copy=sample;}if(!copy.valid())return "Load or generate a voice first.";
    TemporaryFile temporary(file);
    {FileOutputStream out(temporary.getFile());if(!out.openedOk())return "Could not write the WAV file.";
        const int bytes=int(copy.left.size()*4);out.write("RIFF",4);out.writeInt(36+bytes);out.write("WAVEfmt ",8);out.writeInt(16);out.writeShort(1);out.writeShort(2);
        out.writeInt(int(copy.rate));out.writeInt(int(copy.rate)*4);out.writeShort(4);out.writeShort(16);out.write("data",4);out.writeInt(bytes);
        for(size_t i=0;i<copy.left.size();++i){out.writeShort(short(std::clamp(copy.left[i],-1.0f,1.0f)*32767));out.writeShort(short(std::clamp(copy.right[i],-1.0f,1.0f)*32767));}out.flush();
        if(out.getStatus().failed())return "WAV export failed.";
    }
    return temporary.overwriteTargetFileWithTemporary()?String{}:String("Could not finish saving the WAV file.");
}
String FreshLabProcessor::phrase()const{const ScopedLock lock(sampleMutex);return voiceText;}
ValueTree FreshLabProcessor::branding()const{const ScopedLock lock(sampleMutex);return brand.createCopy();}
void FreshLabProcessor::setBranding(const ValueTree& input){if(!input.hasType("Branding"))return;const ScopedLock lock(sampleMutex);brand=input.createCopy();brand.setProperty("title",brand["title"].toString().substring(0,32),nullptr);brand.setProperty("tagline",brand["tagline"].toString().substring(0,64),nullptr);appearanceRevision.fetch_add(1);}
String FreshLabProcessor::importLogo(const File& file){
    if(file.getSize()>2*1024*1024)return "Choose a logo under 2 MB.";auto image=ImageFileFormat::loadFrom(file);if(!image.isValid())return "Choose a PNG or JPEG image.";
    image=image.rescaled(std::min(256,image.getWidth()),std::min(256,image.getHeight()),Graphics::highResamplingQuality);MemoryOutputStream out;PNGImageFormat png;
    if(!png.writeImageToStream(image,out))return "Logo encoding failed.";const ScopedLock lock(sampleMutex);brand.setProperty("logo",out.getMemoryBlock(),nullptr);appearanceRevision.fetch_add(1);return {};
}
String FreshLabProcessor::sampleName() const {const ScopedLock lock(sampleMutex);return sampleTitle;}
void FreshLabProcessor::getStateInformation(MemoryBlock& dest){
    auto copy=state.copyState();
    const ScopedLock lock(sampleMutex);
    copy.addChild(brand.createCopy(),-1,nullptr);copy.setProperty("voiceText",voiceText,nullptr);copy.setProperty("schema",3,nullptr);copy.setProperty("editorWidth",editorWidth.load(),nullptr);
    if(instrument){
        if(sample.valid()){
            // Store interleaved float samples explicitly little-endian for portable projects.
            MemoryOutputStream stream;stream.writeDouble(sample.rate);stream.writeInt(int(sample.left.size()));
            for(size_t i=0;i<sample.left.size();++i){stream.writeFloat(sample.left[i]);stream.writeFloat(sample.right[i]);}
            copy.setProperty("voiceData",stream.getMemoryBlock(),nullptr);copy.setProperty("voiceName",sampleTitle,nullptr);
        }
    }
    MemoryOutputStream out(dest,false);copy.writeToStream(out);
}
void FreshLabProcessor::setStateInformation(const void* data,int size){
    if(size<=0||size>24*1024*1024)return;
    auto copy=ValueTree::readFromData(data,static_cast<size_t>(size));if(!copy.isValid()||!copy.hasType("FreshLab"))return;
    editorWidth=std::clamp(int(copy.getProperty("editorWidth",editorWidth.load())),868,1860);editorHeight=int(std::round(editorWidth.load()*940.0/1240));copy.removeProperty("editorWidth",nullptr);
    if(auto appearance=copy.getChildWithName("Branding");appearance.isValid()){setBranding(appearance);copy.removeChild(appearance,nullptr);}
    {const ScopedLock lock(sampleMutex);voiceText=copy.getProperty("voiceText",voiceText).toString().substring(0,100);}
    if(instrument){
        if(auto* blob=copy.getProperty("voiceData").getBinaryData()){
            MemoryInputStream stream(*blob,false);const double sr=stream.readDouble();const int count=stream.readInt();
            if(sr>=8000&&sr<=192000&&count>=2&&count<=sr*12&&blob->getSize()==12+static_cast<size_t>(count)*8){
                fresh::Sample replacement;replacement.rate=sr;replacement.left.resize(size_t(count));replacement.right.resize(size_t(count));
                for(int i=0;i<count;++i){replacement.left[size_t(i)]=std::clamp(fresh::finite(stream.readFloat()),-1.0f,1.0f);replacement.right[size_t(i)]=std::clamp(fresh::finite(stream.readFloat()),-1.0f,1.0f);}
                const ScopedLock lock(sampleMutex);sample=std::move(replacement);sampleTitle=copy["voiceName"].toString();publishAudioSample();
            }
        }
    }
    copy.removeProperty("voiceData",nullptr);copy.removeProperty("voiceName",nullptr);copy.removeProperty("voiceText",nullptr);copy.removeProperty("schema",nullptr);state.replaceState(copy);
    resetRequested=true;
}
File FreshLabProcessor::soundDirectory(){return File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("FreshLab").getChildFile("Sounds");}
String FreshLabProcessor::saveSound(const File& file,int scope){
    MemoryBlock data;getStateInformation(data);auto tree=ValueTree::readFromData(data.getData(),data.getSize());ValueTree entry("FreshLabSound");entry.setProperty("scope",std::clamp(scope,0,4),nullptr);entry.addChild(tree,-1,nullptr);
    if(scope>1){tree.removeProperty("voiceData",nullptr);tree.removeProperty("voiceName",nullptr);tree.removeProperty("voiceText",nullptr);}
    if(file.getParentDirectory().createDirectory().failed())return "Could not create the sound folder.";
    TemporaryFile temp(file);{FileOutputStream out(temp.getFile());if(!out.openedOk())return "Could not open the sound file.";entry.writeToStream(out);out.flush();if(out.getStatus().failed())return "Sound write failed.";}
    return temp.overwriteTargetFileWithTemporary()?String{}:String("Could not finish saving this sound.");
}
String FreshLabProcessor::loadSound(const File& file){
    if(file.getSize()<=0||file.getSize()>24*1024*1024)return "Invalid or oversized sound file.";
    MemoryBlock data;if(!file.loadFileAsData(data))return "Could not read this sound.";auto entry=ValueTree::readFromData(data.getData(),data.getSize());
    if(!entry.hasType("FreshLabSound")||!entry.getChild(0).hasType("FreshLab"))return "This is not a FreshLab sound.";
    auto incoming=entry.getChild(0);const int scope=int(entry["scope"]);
    if(scope<0||scope>4)return "Unknown sound category.";
    if(scope==0){MemoryOutputStream out;incoming.writeToStream(out);setStateInformation(out.getData(),int(out.getDataSize()));return {};}
    const StringArray voices{"root","samplePitch","sampleStart","reverse","oneShot","voiceStyle","voicePitch","voiceFormant","voiceBreath","voiceGrit","voiceRobot","voiceSpeed","voiceWidth","speechEngine"};
    const StringArray instruments{"mode","wave","attack","decay","sustain","release","glide","drop","detune","cutoff","bassSub","bassPunch","bassClick","bassBuzz","bassDrive","curveTime","curveEnabled","mpe","legato","bendRange"};
    const StringArray eq{"highPass","lowHz","lowGain","mid1Hz","mid1Gain","mid1Q","mid2Hz","mid2Gain","mid2Q","highHz","highGain","dynamicHz","dynamicQ","dynamicThreshold","dynamicRange","dynamicAttack","dynamicRelease"};
    for(auto child:incoming){const String id=child["id"].toString();if(id.isEmpty())continue;bool selected=false;
        if(scope==1)selected=voices.contains(id);if(scope==2)selected=instruments.contains(id)||id.startsWith("curve");
        if(scope==3)selected=eq.contains(id);if(scope==4)selected=!voices.contains(id)&&!instruments.contains(id)&&!id.startsWith("curve")&&!id.startsWith("step")&&!id.startsWith("seq")&&!id.startsWith("pad");
        if(selected)setValue(id,float(child["value"]));
    }
    if(scope==1){MemoryBlock current;getStateInformation(current);auto merged=ValueTree::readFromData(current.getData(),current.getSize());
        for(auto key:{"voiceData","voiceName","voiceText"})if(incoming.hasProperty(key))merged.setProperty(key,incoming[key],nullptr);
        MemoryOutputStream out;merged.writeToStream(out);setStateInformation(out.getData(),int(out.getDataSize()));setValue("mode",2);
    }
    return {};
}
void FreshLabProcessor::loadPreset(int index){
    // Every preset is complete: restore every parameter to its default first.
    for(auto* param:getParameters())param->setValueNotifyingHost(param->getDefaultValue());
    switch(index){
        case 1:setValue("mode",0);setValue("decay",2.8f);setValue("drop",19);setValue("drive",14);setValue("distMix",.48f);setValue("output",-9);break;
        case 2:setValue("mode",0);setValue("decay",.5f);setValue("drop",30);setValue("glide",0);setValue("drive",8);setValue("distMix",.3f);break;
        case 3:setValue("mode",1);setValue("attack",.45f);setValue("release",1.6f);setValue("cutoff",3400);setValue("detune",13);setValue("reverb",.55f);setValue("delayMix",.2f);break;
        case 4:setValue("mode",1);setValue("wave",2);setValue("cutoff",1400);setValue("sustain",.3f);setValue("decay",.35f);setValue("drive",9);setValue("distMix",.35f);break;
        case 5:setValue("mode",2);loadFactory(0);setValue("delayMix",.2f);setValue("reverb",.2f);break;
        case 6:setValue("mode",2);loadFactory(18);setValue("samplePitch",-5);setValue("distMix",.3f);setValue("drive",12);setValue("delayMix",.35f);setValue("feedback",.45f);break;
        case 7:setValue("highPass",100);setValue("mid1Gain",-3);setValue("highGain",3);setValue("threshold",-18);setValue("ratio",3);setValue("deess",.4f);setValue("output",-1);break;
        case 8:setValue("highPass",400);setValue("lowGain",-12);setValue("highGain",-15);setValue("highHz",2800);setValue("drive",20);setValue("distMix",.7f);setValue("output",-9);break;
        case 9:setValue("delayMix",.38f);setValue("delayMs",375);setValue("feedback",.5f);setValue("reverb",.4f);setValue("output",-6);break;
        default:break;
    }
    resetRequested=true;
}
AudioProcessorEditor* FreshLabProcessor::createEditor(){return new FreshLabEditor(*this);}
AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new FreshLabProcessor;}
