#include "PluginProcessor.h"

DaniRigProcessor::DaniRigProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    startTimerHz (2);
}

DaniRigProcessor::~DaniRigProcessor() { stopTimer(); }

bool DaniRigProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo());
}

void DaniRigProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int nIn = std::min (getTotalNumInputChannels(), 2), nOut = std::min (getTotalNumOutputChannels(), 2);
    const int n = buffer.getNumSamples();

    const float* ins[2] = { nullptr, nullptr };
    float* outs[2] = { nullptr, nullptr };
    for (int c = 0; c < nIn; ++c)  ins[c]  = buffer.getReadPointer (c);
    for (int c = 0; c < nOut; ++c) outs[c] = buffer.getWritePointer (c);

    engine.process (ins, nIn, outs, nOut, n);
    for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, n);
}

void DaniRigProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = engine.saveState()) copyXmlToBinary (*xml, destData);
}

void DaniRigProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes)) engine.loadState (*xml);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DaniRigProcessor(); }
