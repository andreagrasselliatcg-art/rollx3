#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>
#include <cmath>

// Riverbero stereo a combs + allpass (stile Schroeder/Freeverb) con
// SIZE e DECAY indipendenti:
//  - size  -> scala le lunghezze dei delay (0.5x .. 2.0x)
//  - decay -> RT60 da 0.2 s a 8 s, calcolato per ogni comb in base al suo delay
class SizeDecayReverb
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        const int maxComb = (int) (sr * 0.2) + 8;
        const int maxAp   = (int) (sr * 0.03) + 8;

        for (int c = 0; c < 2; ++c)
        {
            for (auto& cb : combs[(size_t) c])   cb.init (maxComb);
            for (auto& ap : allpass[(size_t) c]) ap.init (maxAp);
        }

        sizeScale.reset (sr, 0.08);
        sizeScale.setCurrentAndTargetValue (1.0f);
        reset();
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            for (auto& cb : combs[(size_t) c])   cb.clear();
            for (auto& ap : allpass[(size_t) c]) ap.clear();
        }
    }

    void setParams (float size01, float decay01, float mix01)
    {
        mix = mix01;
        sizeScale.setTargetValue (0.5f + size01 * 1.5f);
        rt60 = 0.2f * std::pow (40.0f, decay01);
    }

    // L/R contengono il segnale dry; in uscita dry + wet * mix
    void process (float* L, float* R, int n)
    {
        if (mix <= 0.0001f && ! tailActive)
            return;

        // feedback per comb, calcolato sul valore target di size (una volta per blocco)
        const float target = sizeScale.getTargetValue();
        for (size_t c = 0; c < 2; ++c)
            for (size_t k = 0; k < numCombs; ++k)
            {
                const float delaySec = combMs[k] * 0.001f * target * spread[c];
                fb[c][k] = juce::jmin (0.98f, std::pow (10.0f, -3.0f * delaySec / rt60));
            }

        for (int i = 0; i < n; ++i)
        {
            const float scale = sizeScale.getNextValue();
            const float in = (L[i] + R[i]) * 0.5f * inputGain;
            float wet[2];

            for (size_t c = 0; c < 2; ++c)
            {
                float sum = 0.0f;
                for (size_t k = 0; k < numCombs; ++k)
                {
                    const float d = combMs[k] * 0.001f * (float) sampleRate * scale * spread[c];
                    sum += combs[c][k].process (in, d, fb[c][k], damping);
                }
                for (size_t a = 0; a < numAllpass; ++a)
                {
                    const float d = apMs[a] * 0.001f * (float) sampleRate * spread[c];
                    sum = allpass[c][a].process (sum, d);
                }
                wet[c] = sum * wetGain;
            }

            L[i] += wet[0] * mix;
            R[i] += wet[1] * mix;
        }

        tailActive = true; // semplice: il tail resta attivo finche' mix > 0
        if (mix <= 0.0001f) tailActive = false;
    }

private:
    struct DelayLine
    {
        std::vector<float> buf;
        int w = 0, size = 0;
        void init (int n) { buf.assign ((size_t) n, 0.0f); size = n; w = 0; }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
        float read (float d) const
        {
            d = juce::jlimit (1.0f, (float) (size - 2), d);
            float rp = (float) w - d;
            if (rp < 0.0f) rp += (float) size;
            const int i0 = (int) rp;
            const int i1 = (i0 + 1) % size;
            const float f = rp - (float) i0;
            return buf[(size_t) i0] + f * (buf[(size_t) i1] - buf[(size_t) i0]);
        }
        void push (float x) { buf[(size_t) w] = x; if (++w >= size) w = 0; }
    };

    struct Comb : DelayLine
    {
        float z = 0.0f;
        void clear() { DelayLine::clear(); z = 0.0f; }
        float process (float x, float delaySamples, float feedback, float damp)
        {
            const float y = read (delaySamples);
            z = y * (1.0f - damp) + z * damp;
            push (x + z * feedback);
            return y;
        }
    };

    struct Allpass : DelayLine
    {
        float process (float x, float delaySamples)
        {
            constexpr float g = 0.5f;
            const float d = read (delaySamples);
            const float v = x - g * d;
            push (v);
            return d + g * v;
        }
    };

    static constexpr size_t numCombs = 8, numAllpass = 4;
    static constexpr std::array<float, numCombs>   combMs { 29.7f, 37.1f, 41.1f, 43.7f, 50.3f, 53.2f, 58.9f, 61.9f };
    static constexpr std::array<float, numAllpass> apMs   { 5.0f, 6.7f, 8.9f, 11.3f };
    static constexpr std::array<float, 2> spread { 1.0f, 1.013f };

    std::array<std::array<Comb, numCombs>, 2>     combs;
    std::array<std::array<Allpass, numAllpass>, 2> allpass;
    float fb[2][numCombs] {};

    juce::SmoothedValue<float> sizeScale;
    double sampleRate = 44100.0;
    float mix = 0.4f, rt60 = 1.0f;
    float damping = 0.35f, inputGain = 0.6f, wetGain = 0.08f;
    bool tailActive = false;
};
