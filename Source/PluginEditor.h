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

class RollXEditor : public juce::AudioProcessorEditor,
                    public juce::FileDragAndDropTarget,
                    private juce::Timer
{
public:
    explicit RollXEditor (RollXProcessor&);
    ~RollXEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

    // drag & drop di file audio sul plug-in
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override;
    void chooseSample();
    void loadFile (const juce::File&);
    void refreshPeaks();
    void drawDisplay (juce::Graphics&, juce::Rectangle<int>);
    void drawText (juce::Graphics&, const juce::String&, juce::Rectangle<int>, float size,
                   juce::Colour, juce::Justification = juce::Justification::centred, bool bold = true);

    RollXProcessor& proc;
    KnobLookAndFeel lnf;

    juce::Slider rate, pitch, mix, size, decay, out;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> rateA, pitchA, mixA, sizeA, decayA, outA;

    juce::TextButton rollBtn { "HOLD TO ROLL" };
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Rectangle<int> displayArea;
    std::vector<float> peaks;
    int shownVersion = -1;
    bool dragOver = false;
    juce::String statusMsg;
    int statusTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RollXEditor)
};
