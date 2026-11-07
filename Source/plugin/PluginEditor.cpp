#include "PluginEditor.h"

DaniRigEditor::DaniRigEditor (DaniRigProcessor& p) : AudioProcessorEditor (&p), ui (p.engine)
{
    addAndMakeVisible (ui);
    setResizable (true, true);
    setResizeLimits (550, 344, 2560, 1600);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio (1280.0 / 890.0);
    setSize (1100, 765);
}

juce::AudioProcessorEditor* DaniRigProcessor::createEditor() { return new DaniRigEditor (*this); }
