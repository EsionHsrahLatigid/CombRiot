#include "violent/plugins/CombRiotPlugin.h"
#include "violent/plugins/StandaloneGateState.h"

#include <yup_audio_processors/yup_audio_processors.h>

#include <cassert>
#include <cmath>
#include <iostream>

namespace
{

float renderBlockPeak (violent::plugin::CombRiotPlugin& plugin, int numSamples, yup::MidiBuffer* midiToRender = nullptr)
{
    yup::AudioBuffer<float> audio (2, numSamples);
    yup::MidiBuffer midi;
    if (midiToRender != nullptr)
        midi = *midiToRender;
    yup::ParameterChangeBuffer params;
    params.reserve (0);
    yup::AudioProcessContext<float> context { audio, midi, params, nullptr, {}, {} };

    plugin.processBlock (context);

    float peak = 0.0f;
    for (int channel = 0; channel < audio.getNumChannels(); ++channel)
    {
        const auto* samples = audio.getReadPointer (channel);
        for (int i = 0; i < audio.getNumSamples(); ++i)
            peak = std::max (peak, std::fabs (samples[i]));
    }
    return peak;
}

void testStandaloneSyntheticTriggerProducesOutput()
{
    violent::plugin::CombRiotPlugin plugin;
    plugin.prepareToPlay ({ 48000.0f, 256, 2 });

    assert (renderBlockPeak (plugin, 256) == 0.0f);

    plugin.setStandaloneTriggerGate (true);

    float peak = 0.0f;
    for (int i = 0; i < 8; ++i)
        peak = std::max (peak, renderBlockPeak (plugin, 256));

    assert (peak > 0.001f);
    assert (plugin.consumeOutputPeak() > 0.001f);

    plugin.setStandaloneTriggerGate (false);
    renderBlockPeak (plugin, 256);
}

void testRapidStandalonePulseProducesOutput()
{
    violent::plugin::CombRiotPlugin plugin;
    plugin.prepareToPlay ({ 48000.0f, 256, 2 });

    plugin.setStandaloneTriggerGate (true);
    plugin.setStandaloneTriggerGate (false);

    float peak = 0.0f;
    for (int i = 0; i < 12; ++i)
        peak = std::max (peak, renderBlockPeak (plugin, 256));

    assert (peak > 0.001f);
    assert (! plugin.getStandaloneTriggerGate());
}

void testCombinedMouseAndSpaceGateState()
{
    violent::plugin::StandaloneGateState gate;

    assert (gate.setMouseGateHeld (true));
    assert (gate.setSpaceGateHeld (true));
    assert (gate.setMouseGateHeld (false));
    assert (! gate.setSpaceGateHeld (false));
}

void testMidiOverlapHandsBackToHeldStandalone()
{
    violent::plugin::CombRiotPlugin plugin;
    plugin.prepareToPlay ({ 48000.0f, 256, 2 });
    plugin.setStandaloneTriggerGate (true);

    assert (renderBlockPeak (plugin, 256) > 0.001f);

    yup::MidiBuffer noteOn;
    noteOn.addEvent (yup::MidiMessage::noteOn (1, 67, 1.0f), 0);
    auto midiPeak = renderBlockPeak (plugin, 256, &noteOn);
    for (int i = 0; i < 8; ++i)
        midiPeak = std::max (midiPeak, renderBlockPeak (plugin, 256));
    assert (midiPeak > 0.001f);

    yup::MidiBuffer noteOff;
    noteOff.addEvent (yup::MidiMessage::noteOff (1, 67), 0);
    renderBlockPeak (plugin, 256, &noteOff);

    float handoffPeak = 0.0f;
    for (int i = 0; i < 8; ++i)
        handoffPeak = std::max (handoffPeak, renderBlockPeak (plugin, 256));

    assert (handoffPeak > 0.001f);
    assert (plugin.getStandaloneTriggerGate());
}

void testFlushPreservesHeldRequestAndReleaseClearsIt()
{
    violent::plugin::CombRiotPlugin plugin;
    plugin.prepareToPlay ({ 48000.0f, 256, 2 });
    plugin.setStandaloneTriggerGate (true);
    assert (renderBlockPeak (plugin, 256) > 0.001f);

    plugin.flush();

    float retriggerPeak = 0.0f;
    for (int i = 0; i < 8; ++i)
        retriggerPeak = std::max (retriggerPeak, renderBlockPeak (plugin, 256));

    assert (retriggerPeak > 0.001f);
    assert (plugin.getStandaloneTriggerGate());

    plugin.setStandaloneTriggerGate (false);
    plugin.flush();
    assert (renderBlockPeak (plugin, 256) == 0.0f);
    assert (! plugin.getStandaloneTriggerGate());
}

} // namespace

int main()
{
    testStandaloneSyntheticTriggerProducesOutput();
    testRapidStandalonePulseProducesOutput();
    testCombinedMouseAndSpaceGateState();
    testMidiOverlapHandsBackToHeldStandalone();
    testFlushPreservesHeldRequestAndReleaseClearsIt();

    std::cout << "CombRiotPluginBridgeTests passed\n";
    return 0;
}
