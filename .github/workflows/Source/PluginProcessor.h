#pragma once
#include <JuceHeader.h>
#include "Reverb.h"

class RollXProcessor : public juce::AudioProcessor
{
public:
    struct Sample
    {
        juce::AudioBuffer<float> data;
        double sr = 44100.0;
    };

    static inline const juce::StringArray rateNames { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/64" };
    // lunghezza di ogni step del roll, in beat (1 beat = 1/4)
    static constexpr double rateBeats[7] { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125, 0.0625 };

    RollXProcessor();
    ~RollXProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ROLL-X"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // thread messaggi
    bool loadSample (const juce::File& file);
    std::shared_ptr<const Sample> getSample() const;
    juce::String getSampleName() const { return sampleName; }
    int getSampleVersion() const { return sampleVersion.load(); }

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<bool>  guiRoll   { false };  // pulsante HOLD TO ROLL
    std::atomic<float> rollPhase { -1.0f };  // 0..1 posizione nella battuta, -1 = fermo

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct Voice
    {
        bool active = false;
        double pos = 0.0, ratio = 1.0;
        float gain = 1.0f;
    };

    void triggerVoice (double rollPos, double rampLen, float rampSemitones);

    juce::AudioFormatManager formatManager;

    // campione condiviso tra thread messaggi e audio
    mutable juce::SpinLock sampleLock;
    std::shared_ptr<const Sample> sample;
    std::shared_ptr<const Sample> audioSample;
    int audioVersion = -1;
    std::atomic<int> sampleVersion { 0 };
    juce::String sampleName, samplePath;

    std::array<Voice, 16> voices;
    size_t nextVoice = 0;

    double sampleRateHz = 44100.0;
    int heldNotes = 0;
    bool wasActive = false;
    double samplesToNext = 0.0, rollSamples = 0.0;

    float lastGain = 1.0f;
    SizeDecayReverb reverb;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RollXProcessor)
};
