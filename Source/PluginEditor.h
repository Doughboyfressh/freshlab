#pragma once
#include "PluginProcessor.h"
#include <complex>

struct FreshLook final : juce::LookAndFeel_V4 {
    juce::Colour accent{0xff96f2cc},background{0xff101419},panel{0xff1a2028};
    int knobStyle=0;
    FreshLook();
    void configure(juce::Colour,int,int);
    void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
};

class FreshLabEditor final : public juce::AudioProcessorEditor,private juce::Timer {
public:
    explicit FreshLabEditor(FreshLabProcessor&);
    ~FreshLabEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    using SliderAttachment=juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment=juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment=juce::AudioProcessorValueTreeState::ButtonAttachment;
    struct Knob final : juce::Component {
        juce::Slider slider;juce::Label title;std::unique_ptr<SliderAttachment> attachment;
        Knob(FreshLabProcessor&,const juce::String&,const juce::String&);
        void resized() override;
    };
    struct Menu final : juce::Component {
        juce::ComboBox combo;juce::Label title;std::unique_ptr<ComboAttachment> attachment;
        Menu(FreshLabProcessor&,const juce::String&,const juce::String&,juce::StringArray);
        void resized() override;
    };
    struct StepControl final : juce::Component {
        FreshLabProcessor& processor;int index;juce::TextButton action;juce::Slider pitch,velocity;
        std::unique_ptr<SliderAttachment> pitchAttachment,velocityAttachment;
        StepControl(FreshLabProcessor&,int);void resized()override;void refresh(int);
    };
    struct PadControl final : juce::Component {
        FreshLabProcessor& processor;int index;juce::TextButton hit;juce::ComboBox engine,voice,macro;juce::Slider note;
        std::unique_ptr<ComboAttachment> engineAttachment,voiceAttachment,macroAttachment;
        std::unique_ptr<SliderAttachment> noteAttachment;
        PadControl(FreshLabProcessor&,int);void resized()override;
    };
    struct CurveEditor final : juce::Component {
        FreshLabProcessor& processor;explicit CurveEditor(FreshLabProcessor& p):processor(p){}
        void paint(juce::Graphics&)override;void mouseDown(const juce::MouseEvent& e)override{mouseDrag(e);}
        void mouseDrag(const juce::MouseEvent&)override;
    };
    FreshLabProcessor& processor;FreshLook look;
    std::array<juce::TextButton,9> tabs;
    std::vector<std::pair<int,juce::Component*>> pages;
    std::vector<std::unique_ptr<Knob>> knobs;std::vector<int> knobPages;
    std::vector<std::unique_ptr<Menu>> menus;std::vector<int> menuPages;
    std::vector<std::unique_ptr<ButtonAttachment>> toggleAttachments;
    std::array<std::unique_ptr<StepControl>,16> steps;
    std::array<std::unique_ptr<PadControl>,8> pads;
    CurveEditor curve;
    juce::ComboBox presets,voices,libraryEntries,libraryScope,theme,knobStyle;
    juce::TextEditor voiceName,phraseText,libraryName,brandTitle,tagline,accentText;
    juce::TextButton importVoice{"Import audio"},generate{"Generate phrase"},exportVoice{"Export WAV"},audition{"Play voice"},panic{"Panic"};
    juce::TextButton saveSound{"Save current sound"},loadSound{"Load selected"},refreshLibrary{"Refresh"},importSound{"Import .freshlab"};
    juce::TextButton applyBrand{"Apply appearance"},importLogo{"Import logo"},clearLogo{"Clear logo"},clearSteps{"Clear pattern"},fillSteps{"Starter pattern"};
    juce::ToggleButton reverse{"Reverse"},oneShot{"One shot"},bypass{"Bypass effects"},seqEnabled{"Run pattern"},seqSync{"Follow FL transport"},curveEnabled{"Use drawn pitch curve"},padMidi{"MIDI 36-43 play the pads"},animation{"Animate meters"};
    juce::MidiKeyboardComponent keyboard;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Array<juce::File> soundFiles;
    juce::Image logo;
    juce::String status,titleText,taglineText;
    unsigned brandVersion=0;int page=0,previewTicks=0;float left=0,right=0;bool animated=true;
    void addPage(int,juce::Component&);
    void addKnob(int,const char*,const char*);
    void addMenu(int,const char*,const char*,juce::StringArray);
    void bindToggle(int,const char*,juce::ToggleButton&);
    void showPage(int);void timerCallback() override;void drawEq(juce::Graphics&,juce::Rectangle<float>);
    void refreshSounds();void applyAppearance();void fileDialog(const juce::String&,const juce::String&,bool,std::function<void(juce::File)>);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FreshLabEditor)
};
