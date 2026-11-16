#include "PluginProcessor.h"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace juce;
static int checks=0;
void require(bool x,const char* label){++checks;if(!x)throw std::runtime_error(label);}
// Continuous parameters round-trip through JUCE's normalized float range.
// Allow a few ULPs across compilers, while still rejecting changed values.
void requireParameter(FreshLabProcessor& p,const char* id,float expected,const char* label){
    const float actual=p.state.getRawParameterValue(id)->load();
    const bool ok=std::isfinite(actual)&&std::abs(actual-expected)<=1.0e-5f;
    if(!ok)std::cerr<<id<<": expected "<<std::setprecision(9)<<expected<<", got "<<actual<<'\n';
    require(ok,label);
}
float peak(const AudioBuffer<float>& b,int start=0,int count=-1){return b.getMagnitude(start,count<0?b.getNumSamples()-start:count);}
struct TestPlayHead final : AudioPlayHead {PositionInfo position;Optional<PositionInfo> getPosition()const override{return position;}};
int main(){try{
    ScopedJuceInitialiser_GUI initialise;
    const File scratch=File::getSpecialLocation(File::tempDirectory).getNonexistentChildFile("freshlab-tests","",false);scratch.createDirectory();
    struct Cleanup {File dir;~Cleanup(){dir.deleteRecursively();}}cleanup{scratch};
    FreshLabProcessor p;p.prepareToPlay(44100,512);AudioBuffer<float>b(2,512);MidiBuffer m;
    if(FreshLabProcessor::instrument){
        b.clear();m.addEvent(MidiMessage::noteOn(1,45,.9f),128);p.processBlock(b,m);
        require(peak(b,0,128)==0,"MIDI position is sample accurate");require(peak(b,128)>.01f,"instrument responds");
        p.setValue("mode",2);require(p.loadFactory(23).isEmpty(),"last factory voice loads");
        b.clear();m.addEvent(MidiMessage::noteOn(1,60,.9f),0);p.processBlock(b,m);require(peak(b)>.001f,"factory adlib audible");
        MemoryBlock saved;p.getStateInformation(saved);require(saved.getSize()>1000,"state embeds sample");
        p.loadFactory(0);p.setValue("samplePitch",12);p.setStateInformation(saved.getData(),int(saved.getSize()));
        require(p.sampleName().contains("Circuit"),"sample restored");requireParameter(p,"samplePitch",0,"parameter restored");
        for(int i=0;i<24;++i)require(p.loadFactory(i).isEmpty(),"every voice resource available");
        require(p.loadSample(File("/does/not/exist.wav")).isNotEmpty(),"bad import rejected");
        p.panicRequested=true;m.clear();b.clear();p.processBlock(b,m);require(peak(b)==0,"panic clears voices and tails");
        p.setValue("voiceFormant",1.3f);p.setValue("voiceRobot",.5f);require(p.generateVoice("FreshLab let's go","My own voice").isEmpty(),"type to adlib generates a voice");
        require(p.sampleName()=="My own voice"&&p.phrase()=="FreshLab let's go","voice name and text stored");
        const File voice=scratch.getChildFile("voice.wav");require(p.exportVoice(voice).isEmpty()&&voice.getSize()>1000,"WAV export works");
        AudioFormatManager fm;fm.registerBasicFormats();std::unique_ptr<AudioFormatReader> reader(fm.createReaderFor(voice));require(reader&&reader->numChannels==2&&reader->sampleRate==44100,"export has expected WAV format");
        require(p.generateVoice("","bad").isNotEmpty(),"empty typed phrase rejected");
        const File voicePreset=scratch.getChildFile("voice.freshlab");p.setValue("highGain",5);require(p.saveSound(voicePreset,1).isEmpty(),"voice library save");
        p.loadFactory(0);p.setValue("highGain",-3);require(p.loadSound(voicePreset).isEmpty(),"voice library load");require(p.sampleName()=="My own voice","saved voice embeds custom audio");requireParameter(p,"highGain",-3,"voice loading preserves EQ");
        p.loadPreset(0);b.clear();m.clear();m.addEvent(MidiMessage::noteOn(1,45,.9f),128);p.processBlock(b,m);
        require(peak(b,0,128)==0&&peak(b,128)>.01f,"first MIDI note after preset load is preserved");
        p.reset();b.clear();m.clear();p.processBlock(b,m);
        p.triggerPad(0);b.clear();p.processBlock(b,m);require(peak(b)>.001f,"UI pad produces 808 audio");
        p.panicRequested=true;b.clear();p.processBlock(b,m);p.setValue("padMidi",1);m.addEvent(MidiMessage::noteOn(1,40,.9f),100);b.clear();p.processBlock(b,m);
        require(peak(b,0,100)==0&&peak(b,100)>.0001f,"MIDI pad is sample accurate and audible");
        p.loadPreset(0);p.reset();p.setValue("seqEnabled",1);p.setValue("seqSync",1);for(int i=0;i<16;++i)p.setStep(i,0,0,.9f);p.setStep(1,1,0,.9f);
        TestPlayHead head;head.position.setBpm(120);head.position.setPpqPosition(.125);head.position.setIsPlaying(true);p.setPlayHead(&head);p.prepareToPlay(44100,4096);
        AudioBuffer<float> sequence(2,4096);sequence.clear();m.clear();p.processBlock(sequence,m);
        require(peak(sequence,0,2756)==0,"host sequencer waits for exact step boundary");require(peak(sequence,2800)>.0001f&&p.activeStep.load()==1,"host-synced sequence triggers correct step");
        head.position.setIsPlaying(false);sequence.clear();p.processBlock(sequence,m);require(p.activeStep.load()==-1,"stopped host stops pattern");p.setPlayHead(nullptr);
        p.setValue("seqSync",0);p.setStep(0,2,7,.85f);p.setStep(1,4,-5,.7f);p.setValue("curve0",31);p.setValue("bassBuzz",.7f);p.setValue("pad3Macro",2);
        MemoryBlock upgraded;p.getStateInformation(upgraded);p.loadPreset(0);p.setStateInformation(upgraded.getData(),int(upgraded.getSize()));
        require(p.state.getRawParameterValue("step0Action")->load()==2&&p.state.getRawParameterValue("pad3Macro")->load()==2,"new features survive project recall");
        requireParameter(p,"curve0",31,"808 pitch curve survives project recall");
        p.loadPreset(0);p.prepareToPlay(44100,512);
    }else{
        p.setValue("bypass",1);for(int i=0;i<512;++i){b.setSample(0,i,.12f);b.setSample(1,i,-.4f);}p.processBlock(b,m);
        require(b.getSample(0,200)==.12f&&b.getSample(1,200)==-.4f,"FX passes stereo audio in bypass");
        p.setValue("bypass",0);p.loadPreset(7);p.processBlock(b,m);require(peak(b)>0,"FX processes input");
        p.setValue("harmony",.7f);p.setValue("harmonyPitch",12);p.setValue("doubles",.4f);p.setValue("dynamicRange",9);
        MemoryBlock saved;p.getStateInformation(saved);p.setValue("output",-20);p.setValue("harmony",0);p.setValue("harmonyPitch",-12);p.setValue("doubles",0);p.setValue("dynamicRange",0);p.setStateInformation(saved.getData(),int(saved.getSize()));
        requireParameter(p,"output",-1,"FX state restores output gain");
        require(p.state.getRawParameterValue("harmonyPitch")->load()==12,"FX state restores harmony interval");
        requireParameter(p,"harmony",.7f,"FX state restores harmony level");
        requireParameter(p,"doubles",.4f,"FX state restores doubler level");
        requireParameter(p,"dynamicRange",9,"FX state restores dynamic EQ range");
    }
    p.setValue("highGain",7);const File eq=scratch.getChildFile("eq.freshlab");require(p.saveSound(eq,3).isEmpty(),"EQ library save");p.setValue("highGain",-7);p.setValue("mode",1);require(p.loadSound(eq).isEmpty(),"EQ library load");requireParameter(p,"highGain",7,"EQ gain restored");require(p.state.getRawParameterValue("mode")->load()==1,"EQ load preserves instrument");
    p.setValue("doubles",.6f);const File chain=scratch.getChildFile("chain.freshlab");require(p.saveSound(chain,4).isEmpty(),"effects chain save");p.setValue("doubles",0);require(p.loadSound(chain).isEmpty(),"effects chain load");requireParameter(p,"doubles",.6f,"effects chain restore");
    auto branding=p.branding();branding.setProperty("title","CHRISTOPHER'S LAB",nullptr);branding.setProperty("accent","d9b8ff",nullptr);branding.setProperty("theme",1,nullptr);branding.setProperty("knobStyle",2,nullptr);p.setBranding(branding);
    const File complete=scratch.getChildFile("complete.freshlab");require(p.saveSound(complete,0).isEmpty(),"complete library save");branding.setProperty("title","Other",nullptr);p.setBranding(branding);require(p.loadSound(complete).isEmpty()&&p.branding()["title"].toString()=="CHRISTOPHER'S LAB","branding survives complete sound recall");
    Image logo(Image::ARGB,24,24,true);{Graphics g(logo);g.fillAll(Colours::orange);}const File logoFile=scratch.getChildFile("logo.png");{FileOutputStream out(logoFile);PNGImageFormat png;png.writeImageToStream(logo,out);}require(p.importLogo(logoFile).isEmpty(),"custom logo import");
    MemoryBlock branded;p.getStateInformation(branded);p.setStateInformation(branded.getData(),int(branded.getSize()));require(p.branding().hasProperty("logo"),"logo embedded in project");
    require(p.loadSound(scratch.getChildFile("missing.freshlab")).isNotEmpty(),"missing sound rejected");p.setStateInformation("bad",3);require(p.getName().isNotEmpty(),"bad state ignored");
    AudioProcessor::BusesLayout layout=p.getBusesLayout();layout.outputBuses.set(0,AudioChannelSet::mono());if(!FreshLabProcessor::instrument)layout.inputBuses.set(0,AudioChannelSet::mono());require(p.isBusesLayoutSupported(layout),"mono supported");
    p.keyboard.reset();p.loadPreset(0);auto display=p.branding();display.setProperty("title","FRESHLAB",nullptr);display.setProperty("accent","96f2cc",nullptr);display.setProperty("theme",0,nullptr);display.setProperty("knobStyle",0,nullptr);display.removeProperty("logo",nullptr);p.setBranding(display);
    std::unique_ptr<AudioProcessorEditor> editor(p.createEditor());require(editor&&editor->getWidth()==1240&&editor->getHeight()==940,"expanded editor creates");
    if(const auto* dir=std::getenv("FRESHLAB_SNAPSHOT_DIR")){
        File folder=File::getCurrentWorkingDirectory().getChildFile(dir);folder.createDirectory();
        for(auto name:{"SOURCE","VOICE DESIGN","808 BUILDER","SEQUENCER","VOCAL FX","EQUALIZER","PERFORMANCE","SOUND LIBRARY","APPEARANCE"}){
            if(!FreshLabProcessor::instrument&&(String(name)=="SOURCE"||String(name)=="VOICE DESIGN"||String(name)=="808 BUILDER"||String(name)=="SEQUENCER"||String(name)=="PERFORMANCE"))continue;
            for(auto* child:editor->getChildren())if(auto* button=dynamic_cast<TextButton*>(child))if(button->getButtonText()==name)button->onClick();
            auto image=editor->createComponentSnapshot(editor->getLocalBounds());FileOutputStream out(folder.getChildFile(String(FreshLabProcessor::instrument?"instrument-":"fx-")+String(name).toLowerCase().replace(" ","-")+".png"));out.setPosition(0);out.truncate();PNGImageFormat png;require(png.writeImageToStream(image,out),"UI snapshot rendered");
        }
    }
    std::cout<<"PASS: "<<checks<<" "<<(FreshLabProcessor::instrument?"instrument":"FX")<<" integration checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
