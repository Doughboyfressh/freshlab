#include "PluginEditor.h"
using namespace juce;
namespace {
const Colour ink{0xffeaf0f4},muted{0xff98a6b5},edge{0xff34404c};
const StringArray pageNames{"SOURCE","VOICE DESIGN","808 BUILDER","SEQUENCER","VOCAL FX","EQUALIZER","PERFORMANCE","SOUND LIBRARY","APPEARANCE"};
}
FreshLook::FreshLook(){configure(accent,0,0);}
void FreshLook::configure(Colour colour,int theme,int style){
    accent=colour;knobStyle=style;const uint32 colours[]{0xff101419,0xff111426,0xff192329};background=Colour(colours[std::clamp(theme,0,2)]);panel=background.brighter(.055f);
    setColour(Slider::textBoxTextColourId,ink);setColour(Slider::textBoxBackgroundColourId,background);setColour(Slider::textBoxOutlineColourId,Colours::transparentBlack);
    setColour(Slider::thumbColourId,accent);setColour(Slider::trackColourId,accent);setColour(Label::textColourId,muted);
    setColour(ComboBox::backgroundColourId,panel);setColour(ComboBox::textColourId,ink);setColour(ComboBox::outlineColourId,edge);
    setColour(PopupMenu::backgroundColourId,panel);setColour(PopupMenu::textColourId,ink);setColour(PopupMenu::highlightedBackgroundColourId,edge);
    setColour(TextButton::buttonColourId,panel);setColour(TextButton::buttonOnColourId,accent.darker(.55f));setColour(TextButton::textColourOffId,ink);setColour(TextButton::textColourOnId,ink);
    setColour(ToggleButton::textColourId,ink);setColour(ToggleButton::tickColourId,accent);
    setColour(TextEditor::backgroundColourId,background);setColour(TextEditor::textColourId,ink);setColour(TextEditor::outlineColourId,edge);setColour(TextEditor::focusedOutlineColourId,accent);
    setColour(MidiKeyboardComponent::keyDownOverlayColourId,accent.withAlpha(.6f));
}
void FreshLook::drawRotarySlider(Graphics& g,int x,int y,int w,int h,float pos,float start,float end,Slider& s){
    auto area=Rectangle<float>(float(x),float(y),float(w),float(h)).reduced(9);const float radius=std::min(area.getWidth(),area.getHeight())/2,cx=area.getCentreX(),cy=area.getCentreY();
    Path track;track.addCentredArc(cx,cy,radius-2,radius-2,0,start,end,true);g.setColour(edge);g.strokePath(track,PathStrokeType(3));
    const auto colour=s.isEnabled()?accent:muted;const float angle=start+pos*(end-start);
    if(knobStyle==2){Path wedge;wedge.addPieSegment(cx-radius+5,cy-radius+5,(radius-5)*2,(radius-5)*2,start,angle,.2f);g.setColour(colour.withAlpha(.5f));g.fillPath(wedge);}
    else {Path fill;fill.addCentredArc(cx,cy,radius-2,radius-2,0,start,angle,true);g.setColour(colour);g.strokePath(fill,PathStrokeType(knobStyle==1?1.5f:3.0f));}
    g.setColour(panel.brighter(.05f));g.fillEllipse(cx-radius+8,cy-radius+8,(radius-8)*2,(radius-8)*2);
    g.setColour(ink);const float inside=knobStyle==1?0:radius-21;g.drawLine(cx+std::sin(angle)*inside,cy-std::cos(angle)*inside,cx+std::sin(angle)*(radius-12),cy-std::cos(angle)*(radius-12),2);
}
FreshLabEditor::Knob::Knob(FreshLabProcessor& p,const String& id,const String& text){
    title.setText(text,dontSendNotification);title.setJustificationType(Justification::centred);title.setFont(Font{FontOptions{12.5f}});addAndMakeVisible(title);
    slider.setSliderStyle(Slider::RotaryHorizontalVerticalDrag);slider.setTextBoxStyle(Slider::TextBoxBelow,false,90,24);slider.setName(text);slider.setTooltip(p.state.getParameter(id)->getName(100));
    addAndMakeVisible(slider);attachment=std::make_unique<SliderAttachment>(p.state,id,slider);
    slider.textFromValueFunction=[id](double v){int n=2;if(id=="root"||id=="harmonyPitch"||id=="cutoff"||id.endsWith("Hz")||id=="highPass"||id=="delayMs"||id=="seqBpm")n=0;
        else if(id=="attack"||id=="decay"||id=="release"||id=="glide"||id=="curveTime")n=3;
        else if(id.endsWith("Gain")||id=="drive"||id=="bassDrive"||id=="input"||id=="output"||id=="threshold"||id=="dynamicThreshold"||id=="dynamicRange")n=1;return String(v,n);};
    slider.valueFromTextFunction=[](const String& text){return text.getDoubleValue();};slider.updateText();
}
void FreshLabEditor::Knob::resized(){auto r=getLocalBounds();title.setBounds(r.removeFromTop(23));slider.setBounds(r);}
FreshLabEditor::Menu::Menu(FreshLabProcessor& p,const String& id,const String& text,StringArray labels){
    title.setText(text,dontSendNotification);title.setFont(Font{FontOptions{12.0f}});addAndMakeVisible(title);combo.addItemList(labels,1);combo.setName(text);addAndMakeVisible(combo);attachment=std::make_unique<ComboAttachment>(p.state,id,combo);
}
void FreshLabEditor::Menu::resized(){auto r=getLocalBounds();title.setBounds(r.removeFromTop(20));combo.setBounds(r.reduced(0,3));}
FreshLabEditor::StepControl::StepControl(FreshLabProcessor& p,int i):processor(p),index(i){
    const String id="step"+String(i);addAndMakeVisible(action);action.setTooltip("Click: Off, Hit, Double, Triple, Reverse, Echo throw");
    action.onClick=[this,id]{const int next=(int(processor.state.getRawParameterValue(id+"Action")->load())+1)%6;processor.setValue(id+"Action",float(next));refresh(-1);};
    pitch.setSliderStyle(Slider::LinearHorizontal);pitch.setTextBoxStyle(Slider::TextBoxRight,false,43,23);pitch.setName("Step "+String(i+1)+" pitch");pitch.setTooltip("Transpose this step in semitones");addAndMakeVisible(pitch);
    velocity.setSliderStyle(Slider::LinearHorizontal);velocity.setTextBoxStyle(Slider::TextBoxRight,false,43,23);velocity.setName("Step "+String(i+1)+" velocity");velocity.setTooltip("Step velocity, 0 to 1");addAndMakeVisible(velocity);
    pitchAttachment=std::make_unique<SliderAttachment>(p.state,id+"Pitch",pitch);velocityAttachment=std::make_unique<SliderAttachment>(p.state,id+"Velocity",velocity);
    velocity.textFromValueFunction=[](double v){return String(v,2);};velocity.updateText();refresh(-1);
}
void FreshLabEditor::StepControl::resized(){action.setBounds(2,4,getWidth()-4,36);pitch.setBounds(0,49,getWidth(),32);velocity.setBounds(0,88,getWidth(),32);}
void FreshLabEditor::StepControl::refresh(int playing){const StringArray names{"OFF","HIT","2X","3X","REV","ECHO"};const int actionIndex=std::clamp(int(processor.state.getRawParameterValue("step"+String(index)+"Action")->load()),0,5);action.setButtonText(String(index+1).paddedLeft('0',2)+" / "+names[actionIndex]);action.setToggleState(index==playing,dontSendNotification);}
FreshLabEditor::PadControl::PadControl(FreshLabProcessor& p,int i):processor(p),index(i){
    const String id="pad"+String(i);hit.setButtonText("PAD "+String(i+1)+" / MIDI "+String(36+i));hit.onClick=[this]{processor.triggerPad(index);};addAndMakeVisible(hit);
    engine.addItemList({"808","Synth","Voice"},1);engine.setName("Pad engine");addAndMakeVisible(engine);
    auto names=FreshLabProcessor::voiceNames();names.insert(0,"Current custom voice");voice.addItemList(names,1);voice.setName("Pad voice bank");addAndMakeVisible(voice);
    macro.addItemList({"Clean","Drive","Echo","Space"},1);macro.setName("Pad effect combination");addAndMakeVisible(macro);
    note.setSliderStyle(Slider::LinearHorizontal);note.setTextBoxStyle(Slider::TextBoxRight,false,45,22);note.setName("Pad MIDI note");note.setTooltip("Sound pitch as a MIDI note number");addAndMakeVisible(note);
    engineAttachment=std::make_unique<ComboAttachment>(p.state,id+"Engine",engine);voiceAttachment=std::make_unique<ComboAttachment>(p.state,id+"Voice",voice);macroAttachment=std::make_unique<ComboAttachment>(p.state,id+"Macro",macro);noteAttachment=std::make_unique<SliderAttachment>(p.state,id+"Note",note);
}
void FreshLabEditor::PadControl::resized(){const int w=getWidth();hit.setBounds(4,4,w-8,42);engine.setBounds(4,54,w-8,28);note.setBounds(0,86,w,29);voice.setBounds(4,123,w-8,28);macro.setBounds(4,158,w-8,28);}
void FreshLabEditor::CurveEditor::paint(Graphics& g){
    auto r=getLocalBounds().toFloat().reduced(24);auto* look=dynamic_cast<FreshLook*>(&getLookAndFeel());const auto accent=look?look->accent:Colours::aquamarine;
    g.setColour(edge);for(int i=0;i<5;++i)g.drawHorizontalLine(int(r.getY()+r.getHeight()*i/4),r.getX(),r.getRight());
    Path line;g.setFont(11);for(int i=0;i<8;++i){const float value=processor.state.getRawParameterValue("curve"+String(i))->load();const float x=r.getX()+r.getWidth()*i/7,y=r.getBottom()-(value+24)/60*r.getHeight();
        if(i==0)line.startNewSubPath(x,y);else line.lineTo(x,y);g.setColour(accent);g.fillEllipse(x-5,y-5,10,10);}
    g.setColour(accent);g.strokePath(line,PathStrokeType(2));g.setColour(muted);g.drawText("+36 st",2,0,70,20,Justification::left);g.drawText("-24 st",2,getHeight()-42,70,20,Justification::left);
    g.drawText("0 s",24,getHeight()-21,100,20,Justification::left);g.drawText(String(processor.state.getRawParameterValue("curveTime")->load(),3)+" s",getWidth()-120,getHeight()-21,100,20,Justification::right);
}
void FreshLabEditor::CurveEditor::mouseDrag(const MouseEvent& e){auto r=getLocalBounds().toFloat().reduced(24);const int index=std::clamp(int(std::round((e.position.x-r.getX())/r.getWidth()*7)),0,7);const float pitch=std::clamp((r.getBottom()-e.position.y)/r.getHeight()*60-24,-24.0f,36.0f);processor.setValue("curve"+String(index),pitch);processor.setValue("curveEnabled",1);repaint();}
void FreshLabEditor::addPage(int p,Component& c){addAndMakeVisible(c);pages.emplace_back(p,&c);}
void FreshLabEditor::addKnob(int p,const char* id,const char* name){auto k=std::make_unique<Knob>(processor,id,name);addPage(p,*k);knobs.push_back(std::move(k));knobPages.push_back(p);}
void FreshLabEditor::addMenu(int p,const char* id,const char* name,StringArray labels){auto m=std::make_unique<Menu>(processor,id,name,labels);addPage(p,*m);menus.push_back(std::move(m));menuPages.push_back(p);}
void FreshLabEditor::bindToggle(int p,const char* id,ToggleButton& b){addPage(p,b);toggleAttachments.push_back(std::make_unique<ButtonAttachment>(processor.state,id,b));}
void FreshLabEditor::fileDialog(const String& title,const String& extensions,bool save,std::function<void(File)> fn){
    chooser=std::make_unique<FileChooser>(title,File{},extensions);Component::SafePointer<FreshLabEditor> safe(this);
    chooser->launchAsync((save?FileBrowserComponent::saveMode|FileBrowserComponent::warnAboutOverwriting:FileBrowserComponent::openMode)|FileBrowserComponent::canSelectFiles,[safe,fn=std::move(fn)](const FileChooser& c){if(safe&&c.getResult()!=File{})fn(c.getResult());});
}
FreshLabEditor::FreshLabEditor(FreshLabProcessor& p):AudioProcessorEditor(p),processor(p),curve(p),keyboard(p.keyboard,MidiKeyboardComponent::horizontalKeyboard){
    setLookAndFeel(&look);
    for(int i=0;i<9;++i){auto& tab=tabs[size_t(i)];tab.setButtonText(pageNames[i]);tab.onClick=[this,i]{showPage(i);};addAndMakeVisible(tab);}
    if(FreshLabProcessor::instrument)presets.addItemList({"Init / Clean","808 / Heavy","808 / Short knock","Synth / Night pad","Synth / Square bass","Adlib / Sub yeah","Adlib / Circuit echo","Vocal / Clear","Vocal / Telephone","FX / Wide echo"},1);
    else {presets.addItem("Init / Clean",1);presets.addItem("Vocal / Clear",8);presets.addItem("Vocal / Telephone",9);presets.addItem("FX / Wide echo",10);}
    presets.setTextWhenNothingSelected("Factory presets");presets.onChange=[this]{processor.loadPreset(presets.getSelectedId()-1);status="Factory preset loaded";};addAndMakeVisible(presets);
    panic.onClick=[this]{processor.keyboard.allNotesOff(0);processor.panicRequested=true;status="Stopped voices, pattern notes and effect tails";};addAndMakeVisible(panic);
    auto text=[this](int page,TextEditor& e,const String& placeholder,const String& value,int limit){e.setTextToShowWhenEmpty(placeholder,muted);e.setInputRestrictions(limit);e.setText(value);e.setName(placeholder);addPage(page,e);};
    if(FreshLabProcessor::instrument){
        addMenu(0,"mode","SOUND ENGINE",{"808 bass","Poly synth","Adlib sampler"});addMenu(0,"wave","SYNTH WAVE",{"Sine","Saw","Square","Triangle"});
        addKnob(0,"attack","Attack / sec");addKnob(0,"decay","Decay / sec");addKnob(0,"sustain","Sustain");addKnob(0,"release","Release / sec");addKnob(0,"glide","808 glide / sec");addKnob(0,"drop","808 drop / st");addKnob(0,"detune","Synth detune / ct");addKnob(0,"cutoff","Synth filter / Hz");
        text(1,voiceName,"Voice name","Custom Fresh voice",64);text(1,phraseText,"Type an adlib in English",p.phrase(),100);
        voices.addItemList(FreshLabProcessor::voiceNames(),1);voices.setTextWhenNothingSelected("Choose an original voice");voices.setName("Original voice bank");voices.onChange=[this]{status=processor.loadFactory(voices.getSelectedId()-1);if(status.isEmpty())status="Original voice loaded";processor.setValue("mode",2);};addPage(1,voices);
        generate.onClick=[this]{status=processor.generateVoice(phraseText.getText(),voiceName.getText());if(status.isEmpty()){status="New voice generated / original CC0 audio";voices.setSelectedId(0,dontSendNotification);}};addPage(1,generate);
        importVoice.onClick=[this]{fileDialog("Load an adlib up to 12 seconds","*.wav;*.aif;*.aiff;*.flac",false,[this](File f){status=processor.loadSample(f);if(status.isEmpty()){status="Recording loaded";processor.setValue("mode",2);voices.setSelectedId(0,dontSendNotification);}});};addPage(1,importVoice);
        exportVoice.onClick=[this]{fileDialog("Export the current voice","*.wav",true,[this](File f){status=processor.exportVoice(f.withFileExtension("wav"));if(status.isEmpty())status="Voice WAV exported";});};addPage(1,exportVoice);
        audition.onClick=[this]{processor.setValue("mode",2);processor.keyboard.noteOn(1,60,.9f);previewTicks=24;};addPage(1,audition);
        addMenu(1,"voiceStyle","SYNTHETIC CHARACTER",{"Sub","Spark","Haze","Circuit"});bindToggle(1,"reverse",reverse);bindToggle(1,"oneShot",oneShot);
        addKnob(1,"voicePitch","Design pitch / st");addKnob(1,"voiceFormant","Formant size");addKnob(1,"voiceBreath","Breath");addKnob(1,"voiceGrit","Grit");addKnob(1,"voiceRobot","Robot texture");addKnob(1,"voiceSpeed","Speech speed");addKnob(1,"voiceWidth","Stereo spread");addKnob(1,"root","Root / MIDI note");addKnob(1,"samplePitch","Playback pitch / st");addKnob(1,"sampleStart","Playback start");
        addKnob(2,"bassSub","Sub layer");addKnob(2,"bassPunch","Punch layer");addKnob(2,"bassClick","Click layer");addKnob(2,"bassBuzz","Buzz layer");addKnob(2,"bassDrive","808 saturation / dB");addKnob(2,"curveTime","Pitch curve / sec");bindToggle(2,"curveEnabled",curveEnabled);addPage(2,curve);
        bindToggle(3,"seqEnabled",seqEnabled);bindToggle(3,"seqSync",seqSync);addKnob(3,"seqBpm","Internal tempo / BPM");addKnob(3,"seqSwing","Swing");
        addMenu(3,"seqDivision","STEP DURATION",{"1/8","1/16","1/32"});addMenu(3,"seqLength","PATTERN LENGTH",{"4 steps","8 steps","16 steps"});
        clearSteps.onClick=[this]{for(int i=0;i<16;++i)processor.setStep(i,0,0,.85f);status="Pattern cleared";};addPage(3,clearSteps);
        fillSteps.onClick=[this]{for(int i=0;i<16;++i)processor.setStep(i,i%4==0?1:(i==7?2:(i==15?5:0)),i==8?7:0,.85f);status="Starter pattern loaded";};addPage(3,fillSteps);
        for(int i=0;i<16;++i){steps[size_t(i)]=std::make_unique<StepControl>(p,i);addPage(3,*steps[size_t(i)]);}
        bindToggle(6,"padMidi",padMidi);for(int i=0;i<8;++i){pads[size_t(i)]=std::make_unique<PadControl>(p,i);addPage(6,*pads[size_t(i)]);}
        addAndMakeVisible(keyboard);keyboard.setOctaveForMiddleC(4);keyboard.setAvailableRange(24,96);keyboard.setKeyWidth(23);
    }
    addMenu(4,"distType","DISTORTION",{"Soft clip","Hard clip","Foldback"});bindToggle(4,"bypass",bypass);
    addKnob(4,"input","Input / dB");addKnob(4,"output","Output / dB");addKnob(4,"mix","Dry / wet");addKnob(4,"drive","Drive / dB");addKnob(4,"distMix","Distortion mix");addKnob(4,"threshold","Comp threshold / dB");addKnob(4,"ratio","Comp ratio");addKnob(4,"deess","De esser");addKnob(4,"delayMs","Delay / ms");addKnob(4,"feedback","Feedback");addKnob(4,"delayMix","Delay send");addKnob(4,"reverb","Room send");addKnob(4,"doubles","Doubler level");addKnob(4,"harmony","Harmony level");addKnob(4,"harmonyPitch","Harmony / st");addKnob(4,"width","Stereo width");
    addKnob(5,"highPass","High pass / Hz");addKnob(5,"lowHz","Low shelf / Hz");addKnob(5,"lowGain","Low shelf / dB");addKnob(5,"mid1Hz","Low mid / Hz");addKnob(5,"mid1Gain","Low mid / dB");addKnob(5,"mid1Q","Low mid / Q");addKnob(5,"mid2Hz","High mid / Hz");addKnob(5,"mid2Gain","High mid / dB");addKnob(5,"mid2Q","High mid / Q");addKnob(5,"highHz","High shelf / Hz");addKnob(5,"highGain","High shelf / dB");addKnob(5,"dynamicHz","Dynamic center / Hz");addKnob(5,"dynamicQ","Dynamic bandwidth / Q");addKnob(5,"dynamicThreshold","Dynamic threshold / dB");addKnob(5,"dynamicRange","Maximum cut / dB");
    text(7,libraryName,"Name your saved sound","Fresh sound",60);if(FreshLabProcessor::instrument)libraryScope.addItemList({"Complete sound","Voice","Instrument","EQ","Effects chain"},1);else {libraryScope.addItem("Complete sound",1);libraryScope.addItem("EQ",4);libraryScope.addItem("Effects chain",5);}libraryScope.setSelectedId(1);addPage(7,libraryScope);
    libraryEntries.setTextWhenNothingSelected("No saved sounds yet");addPage(7,libraryEntries);
    saveSound.onClick=[this]{const int scope=libraryScope.getSelectedId()-1;const StringArray categories{"Complete","Voices","Instruments","EQ","Effects"};const String name=File::createLegalFileName(libraryName.getText().trim());if(name.isEmpty()){status="Name this sound before saving.";return;}
        const File dir=FreshLabProcessor::soundDirectory().getChildFile(categories[scope]);const File file=dir.getNonexistentChildFile(name,".freshlab",false);status=processor.saveSound(file,scope);if(status.isEmpty()){status="Saved "+file.getFileNameWithoutExtension();refreshSounds();}};addPage(7,saveSound);
    loadSound.onClick=[this]{const int i=libraryEntries.getSelectedId()-1;if(i<0||i>=soundFiles.size()){status="Select a saved sound first.";return;}status=processor.loadSound(soundFiles[i]);if(status.isEmpty()){status="Sound loaded";applyAppearance();phraseText.setText(processor.phrase());}};addPage(7,loadSound);
    refreshLibrary.onClick=[this]{refreshSounds();status="Sound library refreshed";};addPage(7,refreshLibrary);
    importSound.onClick=[this]{fileDialog("Import a FreshLab sound","*.freshlab",false,[this](File f){status=processor.loadSound(f);if(status.isEmpty()){status="Imported sound loaded; save it to keep a library copy";applyAppearance();phraseText.setText(processor.phrase());}});};addPage(7,importSound);refreshSounds();
    text(8,brandTitle,"Plugin name","FRESHLAB",32);text(8,tagline,"Your tagline","YOUR SOUND. YOUR SIGNATURE.",64);text(8,accentText,"Accent hex colour","96f2cc",6);
    theme.addItemList({"Midnight","Indigo","Slate"},1);addPage(8,theme);knobStyle.addItemList({"Ring knobs","Needle knobs","Filled knobs"},1);addPage(8,knobStyle);addPage(8,animation);
    applyBrand.onClick=[this]{const auto hex=accentText.getText().retainCharacters("0123456789abcdefABCDEF");if(hex.length()!=6){status="Use six hexadecimal digits for the accent colour.";return;}auto b=processor.branding();b.setProperty("title",brandTitle.getText(),nullptr);b.setProperty("tagline",tagline.getText(),nullptr);b.setProperty("accent",hex,nullptr);b.setProperty("theme",theme.getSelectedId()-1,nullptr);b.setProperty("knobStyle",knobStyle.getSelectedId()-1,nullptr);b.setProperty("animate",animation.getToggleState(),nullptr);processor.setBranding(b);applyAppearance();status="Appearance saved with this project";};addPage(8,applyBrand);
    importLogo.onClick=[this]{fileDialog("Choose a PNG or JPEG logo","*.png;*.jpg;*.jpeg",false,[this](File f){status=processor.importLogo(f);if(status.isEmpty()){applyAppearance();status="Custom logo loaded";}});};addPage(8,importLogo);
    clearLogo.onClick=[this]{auto b=processor.branding();b.removeProperty("logo",nullptr);processor.setBranding(b);applyAppearance();};addPage(8,clearLogo);
    setSize(1240,940);applyAppearance();showPage(FreshLabProcessor::instrument?0:4);startTimerHz(30);
}
FreshLabEditor::~FreshLabEditor(){stopTimer();setLookAndFeel(nullptr);}
void FreshLabEditor::refreshSounds(){soundFiles.clear();auto dir=FreshLabProcessor::soundDirectory();dir.findChildFiles(soundFiles,File::findFiles,true,"*.freshlab");soundFiles.sort();libraryEntries.clear(dontSendNotification);for(int i=0;i<soundFiles.size();++i)libraryEntries.addItem(soundFiles[i].getParentDirectory().getFileName()+" / "+soundFiles[i].getFileNameWithoutExtension(),i+1);}
void FreshLabEditor::applyAppearance(){
    auto b=processor.branding();brandVersion=processor.appearanceRevision.load();titleText=b.getProperty("title","FRESHLAB").toString();taglineText=b.getProperty("tagline","YOUR SOUND. YOUR SIGNATURE.").toString();
    auto hex=b.getProperty("accent","96f2cc").toString().retainCharacters("0123456789abcdefABCDEF");if(hex.length()!=6)hex="96f2cc";
    look.configure(Colour::fromString("ff"+hex),int(b.getProperty("theme",0)),int(b.getProperty("knobStyle",0)));animated=bool(b.getProperty("animate",true));
    brandTitle.setText(titleText);tagline.setText(taglineText);accentText.setText(hex);theme.setSelectedId(std::clamp(int(b.getProperty("theme",0)),0,2)+1,dontSendNotification);knobStyle.setSelectedId(std::clamp(int(b.getProperty("knobStyle",0)),0,2)+1,dontSendNotification);animation.setToggleState(animated,dontSendNotification);
    logo={};if(auto* data=b.getProperty("logo").getBinaryData())if(data->getSize()<1024*1024)logo=ImageFileFormat::loadFrom(data->getData(),data->getSize());sendLookAndFeelChange();repaint();
}
void FreshLabEditor::showPage(int p){page=p;if(!FreshLabProcessor::instrument&&(p<4||p==6))page=4;
    for(auto pair:pages)pair.second->setVisible(pair.first==page);for(int i=0;i<9;++i){tabs[size_t(i)].setToggleState(i==page,dontSendNotification);tabs[size_t(i)].setEnabled(FreshLabProcessor::instrument||(i>=4&&i!=6));}resized();repaint();}
void FreshLabEditor::resized(){
    presets.setBounds(820,26,250,35);panic.setBounds(1085,26,125,35);for(int i=0;i<9;++i)tabs[size_t(i)].setBounds(24,122+i*66,146,52);keyboard.setBounds(190,840,1020,70);
    int n=0;for(size_t i=0;i<knobs.size();++i)if(knobPages[i]==page){const int columns=page==0?4:(page==1||page==5?5:6);const int top=page==0?240:(page==1?330:(page==2?160:(page==5?140:230)));const int width=948/columns;knobs[i]->setBounds(216+(n%columns)*width,top+(n/columns)*145,width-10,136);++n;}
    int m=0;for(size_t i=0;i<menus.size();++i)if(menuPages[i]==page){int x=216+m*300,y=148,w=280;if(page==1){y=260;w=210;}if(page==3){x=610+m*270;y=222;w=250;}menus[i]->setBounds(x,y,w,61);++m;}
    voiceName.setBounds(216,153,235,36);phraseText.setBounds(466,153,544,36);generate.setBounds(1024,153,162,36);
    voices.setBounds(216,211,310,36);importVoice.setBounds(540,211,158,36);exportVoice.setBounds(712,211,158,36);audition.setBounds(884,211,158,36);reverse.setBounds(460,280,140,30);oneShot.setBounds(620,280,140,30);
    curveEnabled.setBounds(216,341,300,35);curve.setBounds(216,398,948,276);
    seqEnabled.setBounds(216,153,200,35);seqSync.setBounds(438,153,250,35);clearSteps.setBounds(828,153,162,35);fillSteps.setBounds(1005,153,180,35);
    if(page==3){int k=0;for(size_t i=0;i<knobs.size();++i)if(knobPages[i]==3)knobs[i]->setBounds(216+k++*178,209,160,126);}
    for(int i=0;i<16;++i)if(steps[size_t(i)])steps[size_t(i)]->setBounds(214+(i%8)*121,365+(i/8)*153,108,130);
    bypass.setBounds(530,165,250,35);padMidi.setBounds(216,153,450,35);for(int i=0;i<8;++i)if(pads[size_t(i)])pads[size_t(i)]->setBounds(212+(i%4)*246,224+(i/4)*225,226,195);
    libraryName.setBounds(216,180,340,38);libraryScope.setBounds(574,180,255,38);saveSound.setBounds(847,180,300,38);libraryEntries.setBounds(216,280,694,40);loadSound.setBounds(930,280,217,40);refreshLibrary.setBounds(216,345,180,36);importSound.setBounds(415,345,230,36);
    brandTitle.setBounds(216,180,450,38);tagline.setBounds(216,260,670,38);accentText.setBounds(216,340,190,38);theme.setBounds(430,340,220,38);knobStyle.setBounds(674,340,220,38);animation.setBounds(216,405,250,35);applyBrand.setBounds(216,470,250,40);importLogo.setBounds(486,470,200,40);clearLogo.setBounds(706,470,190,40);
}
void FreshLabEditor::timerCallback(){
    if(brandVersion!=processor.appearanceRevision.load())applyAppearance();
    left=animated?std::max(processor.meterL.load(),left*.84f):processor.meterL.load();right=animated?std::max(processor.meterR.load(),right*.84f):processor.meterR.load();
    if(previewTicks>0&&--previewTicks==0)processor.keyboard.noteOff(1,60,0);
    if(page==3)for(auto& s:steps)if(s)s->refresh(processor.activeStep.load());
    if(page==6)for(int i=0;i<8;++i)if(pads[size_t(i)])pads[size_t(i)]->hit.setToggleState(processor.padMeters[size_t(i)].load()>.1f,dontSendNotification);
    repaint();
}
void FreshLabEditor::drawEq(Graphics& g,Rectangle<float> a){
    g.setColour(look.background);g.fillRoundedRectangle(a,7);for(int i=0;i<=4;++i){g.setColour(edge);g.drawHorizontalLine(int(a.getY()+a.getHeight()*i/4),a.getX(),a.getRight());}
    std::array<fresh::Biquad,5> eq;auto v=[this](const char* id){return processor.state.getRawParameterValue(id)->load();};const double rate=processor.getSampleRate()>0?processor.getSampleRate():44100;
    eq[0].configure(fresh::Biquad::HighPass,rate,v("highPass"));eq[1].configure(fresh::Biquad::LowShelf,rate,v("lowHz"),.707,v("lowGain"));eq[2].configure(fresh::Biquad::Peak,rate,v("mid1Hz"),v("mid1Q"),v("mid1Gain"));eq[3].configure(fresh::Biquad::Peak,rate,v("mid2Hz"),v("mid2Q"),v("mid2Gain"));eq[4].configure(fresh::Biquad::HighShelf,rate,v("highHz"),.707,v("highGain"));Path path;
    for(int i=0;i<=500;++i){const double hz=20*std::pow(1000.0,i/500.0);const auto z=std::polar(1.0,-2*fresh::pi*hz/rate);double mag=1;for(auto& b:eq)mag*=std::abs((b.b0+b.b1*z+b.b2*z*z)/(1.0+b.a1*z+b.a2*z*z));const float y=a.getCentreY()-std::clamp(float(20*std::log10(std::max(mag,1e-6))),-24.0f,24.0f)*a.getHeight()/48,x=a.getX()+float(i)/500*a.getWidth();if(i==0)path.startNewSubPath(x,y);else path.lineTo(x,y);}
    g.setColour(look.accent);g.strokePath(path,PathStrokeType(2));g.setFont(11);g.setColour(muted);g.drawText("20 Hz",a.getX()+8,a.getBottom()-20,80,18,Justification::left);g.drawText("20 kHz",a.getRight()-88,a.getBottom()-20,80,18,Justification::right);
    const float x=a.getX()+a.getWidth()*float(std::log(50.0)/std::log(1000.0));g.drawText("1 kHz",x-40,a.getBottom()-20,80,18,Justification::centred);g.drawText("Static EQ / +/-24 dB",a.getX()+8,a.getY()+3,230,18,Justification::left);
    g.drawText("Dynamic cut: "+String(processor.dynamicMeter.load(),1)+" dB",a.getRight()-260,a.getY()+3,250,18,Justification::right);
}
void FreshLabEditor::paint(Graphics& g){
    g.fillAll(look.background);const int logoOffset=logo.isValid()?60:0;if(logo.isValid())g.drawImageWithin(logo,26,22,48,48,RectanglePlacement::centred);
    g.setColour(look.accent);g.setFont(32);g.drawFittedText(titleText,28+logoOffset,20,680-logoOffset,45,Justification::left,1);g.setFont(12);g.setColour(muted);g.drawText(taglineText,30+logoOffset,67,710-logoOffset,20,Justification::left);
    g.setColour(look.panel);g.fillRoundedRectangle(190,112,1020,672,12);
    auto label=[&](const String& text,int x,int y,int w=940,int h=25,float size=13){g.setColour(muted);g.setFont(size);g.drawFittedText(text,x,y,w,h,Justification::left,2);};
    if(page==0){label("Choose an engine, then play the Piano roll or keyboard below. Shape the 808 layers on 808 BUILDER.",216,630);label("Vocal FX and Equalizer process every engine. The original voice bank and generated phrases can be exported as WAV.",216,671);}
    if(page==1){label("Name",216,126,220,22,12);label("Type to adlib / original formant synthesis",466,126,600,22,12);label("Loaded: "+processor.sampleName(),216,656,950,28,17);label("Change the design knobs, then Generate phrase. Playback pitch, start, root and reverse affect the current sample immediately.",216,703,950,30,12);label("Generated voices are electronic, with rule-based English pronunciation. Original generated audio: CC0.",216,742,950,25,12);}
    if(page==2){label("Draw the pitch shape by dragging any of the eight points. Curve values are semitones relative to the played note.",216,705);label("Sub, punch, click and buzz layers combine before the 808 filter and effects rack.",216,742);}
    if(page==3){label("Each step: click its action, then set pitch (upper slider) and velocity (lower slider). Reverse and echo throws are per step.",216,692,950,32,12);label("Follow FL transport uses host tempo and position. Turn it off to run independently at Internal tempo.",216,738,950,25,12);}
    if(page==4){label("EQ > dynamic EQ > de ess > compressor > distortion > delay + room > doubler + harmony > width > output.",216,701,950,30,12);label("Harmony uses a creative delay-based pitch effect; its short delay adds to the original vocal.",216,741,950,24,12);}
    if(page==5)drawEq(g,Rectangle<float>(216,601,948,144));
    if(page==6){label("Each pad chooses an engine, sound pitch, original/custom voice and a two-second effect combination.",216,705);label("Click a pad or enable MIDI notes 36-43. The synth pads release automatically; 808s and adlibs decay or finish.",216,742,950,24,12);}
    if(page==7){label("SAVE A SOUND",216,145,600,24,14);label("YOUR SAVED SOUNDS",216,244,600,24,14);label("Save a complete patch or only a voice, instrument, EQ, or effects chain. Voice entries embed their audio.",216,426);label("Loading an EQ keeps your instrument and voice. Loading a voice keeps your current EQ and effects.",216,470);label("Complete sounds also remember your sequencer, performance pads, logo and appearance.",216,514);label("Your sound files can be copied between Windows and Mac. Use Import .freshlab to load a file from another folder.",216,580,950,40,12);}
    if(page==8){label("PLUGIN NAME",216,147,450,22,12);label("TAGLINE",216,227,670,22,12);label("ACCENT / HEX",216,307,190,22,12);label("BACKGROUND",430,307,220,22,12);label("KNOB STYLE",674,307,220,22,12);label("Add your own PNG/JPEG logo, choose any accent colour and change how the knobs and meters look.",216,570);label("Appearance is saved in the FL Studio project and complete sounds. Scan names stay FreshLab / FreshLab FX.",216,618,950,35,12);}
    g.setColour(muted);g.setFont(12);g.drawFittedText(status.isEmpty()?"Ready / FreshLab 0.2 / all sound controls support host automation":status,192,799,740,30,Justification::left,2);
    for(int c=0;c<2;++c){const float level=c==0?left:right;g.setColour(edge);g.fillRoundedRectangle(974,float(799+c*14),236,8,3);g.setColour(level>.96f?Colour(0xffff987d):look.accent);g.fillRoundedRectangle(974,float(799+c*14),std::clamp(level,0.0f,1.0f)*236,8,3);}
    if(!FreshLabProcessor::instrument){g.setColour(look.panel);g.fillRoundedRectangle(190,840,1020,70,8);label("FRESHLAB FX / Insert on an FL Studio Mixer channel to process vocals, drums, synths and recorded adlibs.",216,860);}
}
