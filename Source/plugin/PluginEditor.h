#pragma once
#include "PluginProcessor.h"
#include "../ui/RigUI.h"

class DaniRigEditor : public juce::AudioProcessorEditor
{
public:
    explicit DaniRigEditor (DaniRigProcessor& p);
    void resized() override { ui.setBounds (getLocalBounds()); }

private:
    dr::RigUI ui;
};
