#include "PluginEditor.h"

using namespace juce;

namespace
{
    const Colour colText  { 0xffd6d9de };
    const Colour colDim   { 0xff8a8f99 };
    const Colour colTick  { 0xffaeb3bc };
    constexpr float kStart = MathConstants<float>::pi * 1.25f;
    constexpr float kEnd   = MathConstants<float>::pi * 2.75f;

    Point<float> onCircle (Point<float> c, float ang, float rad)
    {
        return { c.x + rad * std::sin (ang), c.y - rad * std::cos (ang) };
    }
}

//==============================================================================
void KnobLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h,
                                        float pos, float a0, float a1, Slider& s)
{
    const auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float r = jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const int ticks = (int) s.getProperties().getWithDefault ("ticks", 10);

    // tacche
    g.setColour (Colour (0xff6b707a));
    for (int i = 0; i <= ticks; ++i)
    {
        const float a = a0 + (a1 - a0) * (float) i / (float) ticks;
        g.drawLine (Line<float> (onCircle (c, a, r * 0.90f), onCircle (c, a, r)), jmax (1.5f, r * 0.03f));
    }

    // binario + arco del valore
    const float ar = r * 0.80f, th = jmax (3.0f, r * 0.05f);
    const float av = a0 + pos * (a1 - a0);

    Path track;
    track.addCentredArc (c.x, c.y, ar, ar, 0.0f, a0, a1, true);
    g.setColour (Colour (0xff25272b));
    g.strokePath (track, PathStrokeType (th, PathStrokeType::curved, PathStrokeType::rounded));

    Path val;
    val.addCentredArc (c.x, c.y, ar, ar, 0.0f, a0, av, true);
    g.setColour (accent);
    g.strokePath (val, PathStrokeType (th, PathStrokeType::curved, PathStrokeType::rounded));

    // corpo
    const float rb = r * 0.68f;
    g.setGradientFill (ColourGradient (Colour (0xff3f4248), c.x, c.y - rb, Colour (0xff0d0e10), c.x, c.y + rb, false));
    g.fillEllipse (c.x - rb, c.y - rb, rb * 2.0f, rb * 2.0f);
    g.setColour (Colours::black);
    g.drawEllipse (c.x - rb, c.y - rb, rb * 2.0f, rb * 2.0f, 1.5f);

    // zigrinatura
    g.setColour (Colour (0xff0b0c0e));
    for (int i = 0; i < 72; ++i)
    {
        const float a = MathConstants<float>::twoPi * (float) i / 72.0f;
        g.drawLine (Line<float> (onCircle (c, a, rb * 0.93f), onCircle (c, a, rb * 0.99f)), 1.0f);
    }

    // cappuccio
    const float rc = r * 0.52f;
    g.setGradientFill (ColourGradient (Colour (0xff585c63), c.x - rc * 0.4f, c.y - rc * 0.5f,
                                       Colour (0xff17181b), c.x + rc, c.y + rc, true));
    g.fillEllipse (c.x - rc, c.y - rc, rc * 2.0f, rc * 2.0f);
    g.setColour (Colour (0xff575b62));
    g.drawEllipse (c.x - rc, c.y - rc, rc * 2.0f, rc * 2.0f, 1.0f);

    // indicatore
    const float wd = jmax (3.0f, r * 0.045f);
    Path p;
    p.addRoundedRectangle (-wd * 0.5f, -rc * 0.92f, wd, rc * 0.5f, wd * 0.5f);
    g.setColour (accent);
    g.fillPath (p, AffineTransform::rotation (av).translated (c.x, c.y));
}

//==============================================================================
RollXEditor::RollXEditor (RollXProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setSize (1080, 500);

    auto setup = [this] (Slider& s, int ticks)
    {
        s.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (kStart, kEnd, true);
        s.getProperties().set ("ticks", ticks);
        s.setLookAndFeel (&lnf);
        s.onValueChange = [this] { repaint(); };
        addAndMakeVisible (s);
    };
    setup (rate, 6);
    setup (pitch, 10);
    setup (mix, 10);
    setup (size, 10);
    setup (decay, 10);
    setup (out, 10);

    rateA  = std::make_unique<SliderAttachment> (proc.apvts, "rate",      rate);
    pitchA = std::make_unique<SliderAttachment> (proc.apvts, "pitchRamp", pitch);
    mixA   = std::make_unique<SliderAttachment> (proc.apvts, "revMix",    mix);
    sizeA  = std::make_unique<SliderAttachment> (proc.apvts, "revSize",   size);
    decayA = std::make_unique<SliderAttachment> (proc.apvts, "revDecay",  decay);
    outA   = std::make_unique<SliderAttachment> (proc.apvts, "outGain",   out);

    importBtn.setColour (TextButton::buttonColourId, Colour (0xff1f2125));
    importBtn.setColour (TextButton::textColourOffId, colText);
    importBtn.onClick = [this] { chooseSample(); };
    addAndMakeVisible (importBtn);

    rollBtn.setColour (TextButton::buttonColourId, lnf.accent);
    rollBtn.setColour (TextButton::buttonOnColourId, lnf.accent.brighter (0.3f));
    rollBtn.setColour (TextButton::textColourOffId, Colour (0xff1a0a05));
    rollBtn.setColour (TextButton::textColourOnId, Colour (0xff1a0a05));
    rollBtn.onStateChange = [this] { proc.guiRoll = rollBtn.isDown(); };
    addAndMakeVisible (rollBtn);

    startTimerHz (30);
}

RollXEditor::~RollXEditor()
{
    proc.guiRoll = false;
    for (auto* s : { &rate, &pitch, &mix, &size, &decay, &out })
        s->setLookAndFeel (nullptr);
}

void RollXEditor::resized()
{
    rate.setBounds  (68, 92, 200, 200);
    pitch.setBounds (425, 292, 84, 84);
    mix.setBounds   (822, 76, 180, 180);
    size.setBounds  (827, 312, 58, 58);
    decay.setBounds (939, 312, 58, 58);
    out.setBounds   (990, 5, 62, 62);

    displayArea = { 328, 84, 424, 176 };
    importBtn.setBounds (28, 438, 250, 46);
    rollBtn.setBounds   (380, 434, 320, 54);
}

void RollXEditor::chooseSample()
{
    chooser = std::make_unique<FileChooser> ("Select a sample", File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3");
    Component::SafePointer<RollXEditor> safe (this);
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [safe] (const FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (safe != nullptr && f.existsAsFile())
                                  safe->proc.loadSample (f);
                          });
}

void RollXEditor::refreshPeaks()
{
    peaks.clear();
    const auto s = proc.getSample();
    if (s == nullptr) return;

    const int N = 256, len = s->data.getNumSamples();
    const float* p = s->data.getReadPointer (0);
    peaks.assign ((size_t) N, 0.0f);
    float mx = 1.0e-6f;

    for (int b = 0; b < N; ++b)
    {
        const int i0 = (int) ((int64) len * b / N);
        const int i1 = jmax (i0 + 1, (int) ((int64) len * (b + 1) / N));
        float m = 0.0f;
        for (int i = i0; i < i1 && i < len; ++i) m = jmax (m, std::abs (p[i]));
        peaks[(size_t) b] = m;
        mx = jmax (mx, m);
    }
    for (auto& v : peaks) v /= mx;
}

void RollXEditor::timerCallback()
{
    if (proc.getSampleVersion() != shownVersion)
    {
        shownVersion = proc.getSampleVersion();
        refreshPeaks();
    }
    repaint (displayArea);
}

void RollXEditor::drawText (Graphics& g, const String& text, Rectangle<int> area, float sz,
                            Colour col, Justification j, bool bold)
{
    g.setFont (FontOptions (sz, bold ? Font::bold : Font::plain));
    g.setColour (col);
    g.drawText (text, area, j, false);
}

void RollXEditor::drawDisplay (Graphics& g, Rectangle<int> r)
{
    g.setColour (Colour (0xff08090a));
    g.fillRoundedRectangle (r.toFloat(), 8.0f);
    g.setColour (Colour (0xff2d3035));
    g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 8.0f, 1.0f);

    auto inner = r.reduced (10);
    auto top = inner.removeFromTop (16);
    const int idx = jlimit (0, 6, roundToInt (rate.getValue()));
    const int steps = roundToInt (4.0 / RollXProcessor::rateBeats[idx]);

    drawText (g, proc.getSampleName().isEmpty() ? String ("no sample - press IMPORT") : proc.getSampleName(),
              top, 11.0f, colText, Justification::centredLeft, false);
    drawText (g, String (steps) + " steps / bar", top, 11.0f, colDim, Justification::centredRight, false);

    inner.removeFromTop (6);
    const auto wave = inner.removeFromTop (130);
    const float cy = (float) wave.getCentreY(), half = (float) wave.getHeight() * 0.5f - 4.0f;
    const float stepW = (float) wave.getWidth() / (float) steps;

    // griglia
    for (int s = 0; s <= steps; ++s)
    {
        const bool beat = (s % (steps / 4)) == 0;
        g.setColour (Colour (beat ? 0xff34373d : 0xff1b1d21));
        g.drawVerticalLine (wave.getX() + (int) ((float) s * stepW), (float) wave.getY(), (float) wave.getBottom());
    }
    g.setColour (Colour (0xff2b2e33));
    g.drawHorizontalLine ((int) cy, (float) wave.getX(), (float) wave.getRight());

    // colpi del roll con crescendo
    const float phase = proc.rollPhase.load();
    if (! peaks.empty())
    {
        for (int s = 0; s < steps; ++s)
        {
            const float t = (float) s / (float) steps;
            const float amp = 0.35f + 0.65f * t;
            if (phase < 0.0f)               g.setColour (lnf.accent.withAlpha (0.6f));
            else if (t <= phase)            g.setColour (lnf.accent);
            else                            g.setColour (Colour (0xff5b5f66));

            const float x0 = (float) wave.getX() + (float) s * stepW;
            const int w = jmax (1, (int) stepW - 1);
            for (int j = 0; j < w; ++j)
            {
                const float pk = peaks[(size_t) j * peaks.size() / (size_t) w] * amp;
                g.drawVerticalLine ((int) (x0 + (float) j), cy - pk * half, cy + pk * half);
            }
        }
    }

    if (phase >= 0.0f)
    {
        g.setColour (lnf.accent);
        g.drawVerticalLine (wave.getX() + (int) (phase * (float) wave.getWidth()),
                            (float) wave.getY(), (float) wave.getBottom());
    }
}

void RollXEditor::paint (Graphics& g)
{
    const auto accent = lnf.accent;
    const int W = getWidth(), H = getHeight();

    g.setGradientFill (ColourGradient (Colour (0xff2a2c30), 0.0f, 0.0f, Colour (0xff17181b), 0.0f, (float) H, false));
    g.fillAll();
    g.setColour (Colour (0xff0a0a0b));
    g.fillRect (0, 72, W, 1);
    g.setColour (Colour (0xff121315));
    g.fillRect (0, 422, W, H - 422);
    g.setColour (Colour (0xff2b2e33));
    g.fillRect (0, 422, W, 1);

    // header
    drawText (g, "ROLL", { 28, 12, 90, 34 }, 30.0f, Colour (0xfff2f3f5), Justification::centredLeft);
    drawText (g, "-X",   { 106, 12, 50, 34 }, 30.0f, accent, Justification::centredLeft);
    drawText (g, "SAMPLE ROLL PROCESSOR", { 28, 46, 260, 14 }, 10.0f, colDim, Justification::centredLeft, false);

    drawText (g, "OUT", { 880, 16, 102, 18 }, 14.0f, colText, Justification::centredRight);
    drawText (g, String (out.getValue(), 1) + " dB", { 880, 36, 102, 16 }, 12.0f, colDim, Justification::centredRight, false);

    // ROLL RATE (sinistra)
    {
        const auto c = rate.getBounds().getCentre().toFloat();
        const float rad = (float) rate.getWidth() * 0.5f + 16.0f;
        const int cur = jlimit (0, 6, roundToInt (rate.getValue()));
        for (int i = 0; i < 7; ++i)
        {
            const float a = kStart + (kEnd - kStart) * (float) i / 6.0f;
            drawText (g, RollXProcessor::rateNames[i],
                      Rectangle<int> (44, 18).withCentre (onCircle (c, a, rad).toInt()),
                      13.0f, i == cur ? accent : colTick);
        }
        drawText (g, "ROLL RATE", { 28, 308, 280, 24 }, 18.0f, colText);
        drawText (g, RollXProcessor::rateNames[cur], { 28, 332, 280, 26 }, 20.0f, accent);
    }

    // display centrale + PITCH MOD
    drawDisplay (g, displayArea);
    drawText (g, "PITCH MOD", { 525, 306, 200, 22 }, 17.0f, colText, Justification::centredLeft);
    drawText (g, "RAMP +" + String (pitch.getValue(), 1) + " st", { 525, 330, 200, 22 }, 15.0f, accent,
              Justification::centredLeft);

    // REVERB (destra)
    {
        const auto c = mix.getBounds().getCentre().toFloat();
        const float rad = (float) mix.getWidth() * 0.5f + 12.0f;
        const char* nums[3] = { "0", "5", "10" };
        for (int i = 0; i < 3; ++i)
        {
            const float a = kStart + (kEnd - kStart) * (float) i * 0.5f;
            drawText (g, nums[i], Rectangle<int> (30, 16).withCentre (onCircle (c, a, rad).toInt()), 12.0f, colTick);
        }
        drawText (g, "REVERB", { 772, 266, 280, 20 }, 18.0f, colText);
        drawText (g, "MIX " + String (roundToInt ((float) mix.getValue() * 100.0f)) + "%",
                  { 772, 286, 280, 20 }, 15.0f, accent);
        drawText (g, "SIZE " + String (roundToInt ((float) size.getValue() * 100.0f)) + "%",
                  { 796, 372, 120, 18 }, 13.0f, colText);
        drawText (g, "DECAY " + String (roundToInt ((float) decay.getValue() * 100.0f)) + "%",
                  { 908, 372, 120, 18 }, 13.0f, colText);
    }

    drawText (g, "HOLD ANY MIDI NOTE TO ROLL", { 740, 444, 312, 16 }, 13.0f, colDim, Justification::centredRight);
    drawText (g, "FOLLOWS HOST BPM", { 740, 464, 312, 16 }, 13.0f, colDim, Justification::centredRight);
}
