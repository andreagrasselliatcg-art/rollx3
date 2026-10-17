#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Manopola custom: anello di tacche, arco del valore, corpo metallico, indicatore.
class KnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    juce::Colour accent { 0xffff4d1c };
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle, juce::Slider&) override;
};

class RollXEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RollXEditor (RollXProcessor&);
    ~RollXEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void chooseSample();
    void refreshPeaks();
    void drawDisplay (juce::Graphics&, juce::Rectangle<int>);
    void drawText (juce::Graphics&, const juce::String&, juce::Rectangle<int>, float size,
                   juce::Colour, juce::Justification = juce::Justification::centred, bool bold = true);

    RollXProcessor& proc;
    KnobLookAndFeel lnf;

    juce::Slider rate, pitch, mix, size, decay, out;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> rateA, pitchA, mixA, sizeA, decayA, outA;

    juce::TextButton importBtn { "IMPORT SAMPLE" }, rollBtn { "HOLD TO ROLL" };
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Rectangle<int> displayArea;
    std::vector<float> peaks;
    int shownVersion = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RollXEditor)
};
