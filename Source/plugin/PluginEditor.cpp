#include "PluginEditor.h"

DaniRigEditor::DaniRigEditor (DaniRigProcessor& p) : AudioProcessorEditor (&p), ui (p.engine)
{
    addAndMakeVisible (ui);
    setResizable (true, true);
    setResizeLimits (550, 344, 2560, 1600);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (1280.0 / 800.0);
    setSize (1100, 688);
}

juce::AudioProcessorEditor* DaniRigProcessor::createEditor() { return new DaniRigEditor (*this); }
