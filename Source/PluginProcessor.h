#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Core/Dsp.h"
#include "Core/Speech.h"
#include "Core/Sequencer.h"
#include <atomic>
#include <map>

class FreshLabProcessor final : public juce::AudioProcessor {
public:
    FreshLabProcessor();
    ~FreshLabProcessor() override = default;
    void prepareToPlay(double,int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    void reset() override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return JucePlugin_IsSynth != 0; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    void setValue(const juce::String&,float);
    void loadPreset(int);
    juce::String loadSample(const juce::File&);
    juce::String loadFactory(int);
    juce::String sampleName() const;
    juce::String generateVoice(const juce::String& text,const juce::String& name);
    juce::String exportVoice(const juce::File&)const;
    void triggerPad(int index){if(index>=0&&index<8)pendingPads.fetch_or(1u<<index);}
    void setStep(int index,int action,int pitch,float velocity);
    juce::String saveSound(const juce::File&,int scope);
    juce::String loadSound(const juce::File&);
    static juce::File soundDirectory();
    juce::ValueTree branding()const;
    void setBranding(const juce::ValueTree&);
    juce::String importLogo(const juce::File&);
    juce::String phrase()const;
    static juce::StringArray voiceNames();
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    juce::AudioProcessorValueTreeState state;
    juce::MidiKeyboardState keyboard;
    std::atomic<float> meterL{0},meterR{0};
    std::atomic<bool> panicRequested{false};
    std::atomic<bool> resetRequested{false};
    std::atomic<int> activeStep{-1};
    std::atomic<float> dynamicMeter{0};
    std::atomic<unsigned> appearanceRevision{0};
    std::array<std::atomic<float>,8> padMeters{};
    static constexpr bool instrument = JucePlugin_IsSynth != 0;
private:
    fresh::Synth synth;
    std::array<fresh::Synth,8> padSynths;
    fresh::Synth sequenceSynth;
    fresh::Sequencer sequencer;
    std::array<fresh::Sample,24> bank;
    std::atomic<unsigned> pendingPads{0};
    double localBeats=0,sequenceGate=-1,macroSeconds=0;
    int macro=0,sequenceNote=60;
    std::array<double,8> padGates{};
    std::array<int,8> padNotes{};
    fresh::Patch sequenceVoice;
    fresh::Effects effects;
    // Audio callback only uses try-lock; import/state writes cannot block playback.
    mutable juce::CriticalSection sampleMutex;
    fresh::Sample sample;
    juce::String sampleTitle;
    juce::String voiceText="Let's go";
    juce::ValueTree brand{"Branding"};
    juce::AudioFormatManager formats;
    std::map<std::string,std::atomic<float>*,std::less<>> values;
    float value(const char* id) const { return values.find(id)->second->load(std::memory_order_relaxed); }
    fresh::Patch patch() const;
    fresh::FxPatch fxPatch() const;
    fresh::SequencePatch sequencePatch()const;
    void playPad(int index,const fresh::Patch&,float velocity=.95f);
    void stopVoices();
    void installSample(fresh::Sample,const juce::String&);
    fresh::Sample readAudio(std::unique_ptr<juce::AudioFormatReader>);
    juce::String decode(std::unique_ptr<juce::AudioFormatReader>,const juce::String&);
    void midi(const juce::MidiMessage&,const fresh::Patch&);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FreshLabProcessor)
};
