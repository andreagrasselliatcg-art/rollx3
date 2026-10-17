#include "PluginProcessor.h"
#include "PluginEditor.h"

RollXProcessor::RollXProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    formatManager.registerBasicFormats();
}

juce::AudioProcessorValueTreeState::ParameterLayout RollXProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "rate", 1 }, "Roll Rate", rateNames, 3));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "pitchRamp", 1 }, "Pitch Ramp (st)",
                                                      NormalisableRange<float> (0.0f, 12.0f, 0.1f), 2.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "revMix", 1 }, "Reverb Mix",
                                                      NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "revSize", 1 }, "Reverb Size",
                                                      NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.55f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "revDecay", 1 }, "Reverb Decay",
                                                      NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "outGain", 1 }, "Output (dB)",
                                                      NormalisableRange<float> (-24.0f, 6.0f, 0.1f), -3.0f));
    return layout;
}

bool RollXProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void RollXProcessor::prepareToPlay (double sr, int)
{
    sampleRateHz = sr;
    reverb.prepare (sr);
    for (auto& v : voices) v.active = false;
    heldNotes = 0;
    wasActive = false;
    lastGain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("outGain")->load());
}

void RollXProcessor::triggerVoice (double rollPos, double rampLen, float rampSemitones)
{
    // rampa crescente: 0 -> rampSemitones nell'arco di una battuta, poi resta al massimo
    const double t = juce::jlimit (0.0, 1.0, rollPos / rampLen);

    Voice* v = nullptr;
    for (auto& cand : voices)
        if (! cand.active) { v = &cand; break; }
    if (v == nullptr) { v = &voices[nextVoice]; nextVoice = (nextVoice + 1) % voices.size(); }

    v->active = true;
    v->pos    = 0.0;
    v->ratio  = std::pow (2.0, (double) rampSemitones * t / 12.0) * audioSample->sr / sampleRateHz;
    v->gain   = 0.45f + 0.55f * (float) t;   // crescendo insieme alla rampa
}

void RollXProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    // nuovo campione dal thread messaggi (try-lock: mai bloccare l'audio)
    if (sampleVersion.load() != audioVersion)
    {
        const juce::SpinLock::ScopedTryLockType lock (sampleLock);
        if (lock.isLocked())
        {
            audioSample = sample;
            audioVersion = sampleVersion.load();
            for (auto& v : voices) v.active = false;
        }
    }

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                if (*b > 20.0) bpm = *b;

    const int rateIdx = juce::jlimit (0, 6, (int) apvts.getRawParameterValue ("rate")->load());
    const double beatSamples = sampleRateHz * 60.0 / bpm;
    const double stepSamples = beatSamples * rateBeats[rateIdx];
    const double rampSamples = beatSamples * 4.0;
    const float  rampSt      = apvts.getRawParameterValue ("pitchRamp")->load();

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);

    auto it = midi.begin();
    for (int i = 0; i < n; ++i)
    {
        while (it != midi.end() && (*it).samplePosition <= i)
        {
            const auto m = (*it).getMessage();
            if (m.isNoteOn())                                  ++heldNotes;
            else if (m.isNoteOff() && heldNotes > 0)           --heldNotes;
            else if (m.isAllNotesOff() || m.isAllSoundOff())   heldNotes = 0;
            ++it;
        }

        const bool active = guiRoll.load() || heldNotes > 0;
        if (active && ! wasActive) { samplesToNext = 0.0; rollSamples = 0.0; }
        wasActive = active;

        if (active && audioSample != nullptr)
        {
            if (samplesToNext <= 0.0)
            {
                triggerVoice (rollSamples, rampSamples, rampSt);
                samplesToNext += stepSamples;
            }
            samplesToNext -= 1.0;
            rollSamples   += 1.0;
        }

        float l = 0.0f, r = 0.0f;
        if (audioSample != nullptr)
        {
            const auto& d = audioSample->data;
            const int len = d.getNumSamples();
            const int chans = d.getNumChannels();

            for (auto& v : voices)
            {
                if (! v.active) continue;
                const int i0 = (int) v.pos;
                if (i0 >= len - 1) { v.active = false; continue; }
                const float f = (float) (v.pos - (double) i0);

                const float* p0 = d.getReadPointer (0);
                const float* p1 = d.getReadPointer (juce::jmin (1, chans - 1));
                l += (p0[i0] + f * (p0[i0 + 1] - p0[i0])) * v.gain;
                r += (p1[i0] + f * (p1[i0 + 1] - p1[i0])) * v.gain;
                v.pos += v.ratio;
            }
        }
        L[i] = l;
        R[i] = r;
    }

    // riverbero + volume di uscita
    reverb.setParams (apvts.getRawParameterValue ("revSize")->load(),
                      apvts.getRawParameterValue ("revDecay")->load(),
                      apvts.getRawParameterValue ("revMix")->load());
    reverb.process (L, R, n);

    const float gain = juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("outGain")->load());
    buffer.applyGainRamp (0, n, lastGain, gain);
    lastGain = gain;

    rollPhase.store (wasActive ? (float) (std::fmod (rollSamples, rampSamples) / rampSamples) : -1.0f);
}

bool RollXProcessor::loadSample (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0
        || reader->lengthInSamples > (juce::int64) (reader->sampleRate * 30.0))
        return false;

    auto s = std::make_shared<Sample>();
    s->sr = reader->sampleRate;
    s->data.setSize (juce::jmin (2, (int) reader->numChannels), (int) reader->lengthInSamples);
    reader->read (&s->data, 0, (int) reader->lengthInSamples, 0, true, true);

    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);
        sample = s;
    }
    samplePath = file.getFullPathName();
    sampleName = file.getFileName();
    ++sampleVersion;
    return true;
}

std::shared_ptr<const RollXProcessor::Sample> RollXProcessor::getSample() const
{
    const juce::SpinLock::ScopedLockType lock (sampleLock);
    return sample;
}

void RollXProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("samplePath", samplePath, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void RollXProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (vt);
            const juce::File f (vt["samplePath"].toString());
            if (f.existsAsFile()) loadSample (f);
        }
}

juce::AudioProcessorEditor* RollXProcessor::createEditor()
{
    return new RollXEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RollXProcessor();
}
