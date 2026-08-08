#include "violent/plugins/CombRiotPlugin.h"

#include "violent/ProductState.h"
#ifndef COMBRIOT_PLUGIN_BRIDGE_TEST
#include "violent/plugins/CombRiotEditor.h"
#endif

#include <algorithm>
#include <array>
#include <cmath>

namespace violent::plugin
{
namespace
{
constexpr std::array<char, 4> stateMagic {{ 'C', 'R', 'T', '1' }};
constexpr int stateVersion = 1;
constexpr int controlIntervalSamples = 16;
constexpr std::size_t presetParameterCount = 9;

yup::NormalisableRange<float> makeBurstRange()
{
    auto range = yup::NormalisableRange<float> (0.001f, 0.250f);
    range.setSkewForCentre (0.025f);
    return range;
}

yup::NormalisableRange<float> makeDecayRange()
{
    auto range = yup::NormalisableRange<float> (0.020f, 12.0f);
    range.setSkewForCentre (1.2f);
    return range;
}

constexpr std::array<std::array<float, presetParameterCount>, 4> presetValues {{
    {{ 0.025f, 1.20f, 0.82f,  1.00f, 0.50f, 0.15f, 0.80f,  -7.0f, 0.95f }},
    {{ 0.018f, 2.80f, 0.90f, -0.75f, 0.72f, 0.28f, 1.00f, -10.0f, 0.88f }},
    {{ 0.055f, 4.60f, 0.96f,  0.35f, 0.90f, 0.55f, 0.65f, -12.0f, 0.82f }},
    {{ 0.006f, 0.24f, 0.72f,  1.00f, 0.18f, 0.03f, 0.95f,  -5.0f, 0.90f }}
}};

float decibelsToLinear (float decibels) noexcept
{
    return std::pow (10.0f, decibels / 20.0f);
}
} // namespace

CombRiotPlugin::CombRiotPlugin()
    : yup::AudioProcessor ("CombRiot",
                           yup::AudioBusLayout ({
                                                     yup::AudioBus ("midi", yup::AudioBus::Midi, yup::AudioBus::Input, 1),
                                                 },
                                                 {
                                                     yup::AudioBus ("main", yup::AudioBus::Audio, yup::AudioBus::Output, 2),
                                                 }))
{
    parameters[burst] = yup::AudioParameterBuilder()
                            .withID ("burst")
                            .withName ("Burst")
                            .withHostID (burst)
                            .withRange (makeBurstRange())
                            .withDefault (0.025f)
                            .withSmoothing (10.0f)
                            .withModulatable (true)
                            .withUnit (yup::AudioParameter::ParameterUnit::Seconds)
                            .build();
    parameters[decay] = yup::AudioParameterBuilder()
                            .withID ("decay")
                            .withName ("Decay")
                            .withHostID (decay)
                            .withRange (makeDecayRange())
                            .withDefault (1.2f)
                            .withSmoothing (35.0f)
                            .withModulatable (true)
                            .withUnit (yup::AudioParameter::ParameterUnit::Seconds)
                            .build();
    parameters[feedback] = yup::AudioParameterBuilder()
                               .withID ("feedback")
                               .withName ("Feedback")
                               .withHostID (feedback)
                               .withRange (0.0f, 0.985f)
                               .withDefault (0.82f)
                               .withSmoothing (25.0f)
                               .withModulatable (true)
                               .build();
    parameters[polarity] = yup::AudioParameterBuilder()
                               .withID ("polarity")
                               .withName ("Polarity")
                               .withHostID (polarity)
                               .withRange (-1.0f, 1.0f)
                               .withDefault (1.0f)
                               .withSmoothing (20.0f)
                               .withModulatable (true)
                               .build();
    parameters[structure] = yup::AudioParameterBuilder()
                                .withID ("structure")
                                .withName ("Structure")
                                .withHostID (structure)
                                .withRange (0.0f, 1.0f)
                                .withDefault (0.5f)
                                .withSmoothing (20.0f)
                                .withModulatable (true)
                                .build();
    parameters[delaySlew] = yup::AudioParameterBuilder()
                                .withID ("delay_slew")
                                .withName ("Delay Slew")
                                .withHostID (delaySlew)
                                .withRange (0.0f, 1.0f)
                                .withDefault (0.15f)
                                .withSmoothing (20.0f)
                                .withModulatable (true)
                                .build();
    parameters[stereoSpread] = yup::AudioParameterBuilder()
                                   .withID ("stereo_spread")
                                   .withName ("Stereo Spread")
                                   .withHostID (stereoSpread)
                                   .withRange (0.0f, 1.0f)
                                   .withDefault (0.8f)
                                   .withSmoothing (20.0f)
                                   .withModulatable (true)
                                   .withUnit (yup::AudioParameter::ParameterUnit::Pan)
                                   .build();
    parameters[output] = yup::AudioParameterBuilder()
                             .withID ("output")
                             .withName ("Output")
                             .withHostID (output)
                             .withRange (-60.0f, 6.0f)
                             .withDefault (-7.0f)
                             .withSmoothing (30.0f)
                             .withModulatable (true)
                             .withUnit (yup::AudioParameter::ParameterUnit::Decibels)
                             .build();
    parameters[ceiling] = yup::AudioParameterBuilder()
                              .withID ("ceiling")
                              .withName ("Ceiling")
                              .withHostID (ceiling)
                              .withRange (0.05f, 1.0f)
                              .withDefault (0.95f)
                              .withSmoothing (10.0f)
                              .withModulatable (true)
                              .withUnit (yup::AudioParameter::ParameterUnit::LinearGain)
                              .build();

    for (const auto& parameter : parameters)
        addParameter (parameter);
}

void CombRiotPlugin::prepareToPlay (const yup::AudioSpec& spec)
{
    engine.prepare (spec.sampleRate);

    for (std::size_t i = 0; i < parameterHandles.size(); ++i)
        parameterHandles[i] = yup::AudioParameterHandle (*parameters[i], spec.sampleRate);

    updateEngineParameters (0, 1);
    resetControlCadence();
}

void CombRiotPlugin::releaseResources()
{
}

void CombRiotPlugin::processBlock (yup::AudioProcessContext<float>& context)
{
    auto& audio = context.audio;
    const auto numSamples = audio.getNumSamples();
    const auto numChannels = audio.getNumChannels();

    for (std::size_t i = 0; i < parameterHandles.size(); ++i)
        parameterHandles[i].prepareBlock (context.params, parameters[i]->getIndexInContainer());

    if (controlRefreshPending.exchange (false, std::memory_order_acq_rel))
        resetControlCadence();

    auto midi = context.midi.begin();
    const auto midiEnd = context.midi.end();
    auto* left = numChannels > 0 ? audio.getWritePointer (0) : nullptr;
    auto* right = numChannels > 1 ? audio.getWritePointer (1) : nullptr;
    float blockPeak = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        while (midi != midiEnd && (*midi).samplePosition <= sample)
        {
            const auto& message = (*midi).getMessage();
            if (message.isNoteOn())
            {
                lastNote = std::clamp (message.getNoteNumber(), 0, 127);
                standaloneGateActive = false;
                engine.noteOn (lastNote, std::clamp (message.getFloatVelocity(), 0.0f, 1.0f));
            }
            else if (message.isNoteOff())
            {
                const auto note = std::clamp (message.getNoteNumber(), 0, 127);
                if (note == lastNote)
                {
                    engine.noteOff();
                    lastNote = -1;
                }
            }

            ++midi;
        }

        if (controlSamplesUntilUpdate <= 0)
        {
            updateEngineParameters (sample, samplesSinceControlUpdate);
            controlSamplesUntilUpdate = controlIntervalSamples;
            samplesSinceControlUpdate = 0;
        }

        applyStandaloneTriggerGate();

        const auto frame = engine.processSample();
        blockPeak = std::max (blockPeak, std::max (std::fabs (frame.left), std::fabs (frame.right)));

        if (left != nullptr)
            left[sample] = frame.left;
        if (right != nullptr)
            right[sample] = frame.right;

        for (int channel = 2; channel < numChannels; ++channel)
            audio.getWritePointer (channel)[sample] = 0.0f;

        --controlSamplesUntilUpdate;
        ++samplesSinceControlUpdate;
        advanceStandaloneTriggerClock();
    }

    const auto quantizedPeak = static_cast<std::uint32_t> (std::clamp (blockPeak, 0.0f, 1.0f) * 1000000.0f);
    auto observedPeak = outputPeakQuantized.load (std::memory_order_relaxed);
    while (quantizedPeak > observedPeak
           && ! outputPeakQuantized.compare_exchange_weak (observedPeak,
                                                           quantizedPeak,
                                                           std::memory_order_release,
                                                           std::memory_order_relaxed))
    {
    }
    context.midi.clear();
}

void CombRiotPlugin::flush()
{
    engine.noteOff();
    engine.reset();
    lastNote = -1;
    standaloneGateActive = false;
    pendingStandaloneRelease = false;
    samplesSinceStandaloneTrigger = 0;
    consumedStandalonePressCount = standalonePressCount.load (std::memory_order_acquire);
    consumedStandaloneReleaseCount = standaloneReleaseCount.load (std::memory_order_acquire);
    outputPeakQuantized.store (0, std::memory_order_release);
    controlRefreshPending.store (true, std::memory_order_release);
}

bool CombRiotPlugin::acceptsMidi() const noexcept
{
    return true;
}

int CombRiotPlugin::getNumVoices() const
{
    return 1;
}

int CombRiotPlugin::getCurrentPreset() const noexcept
{
    return currentPreset.load (std::memory_order_relaxed);
}

void CombRiotPlugin::setCurrentPreset (int index) noexcept
{
    if (! yup::isPositiveAndBelow (index, static_cast<int> (presetValues.size())))
        return;

    currentPreset.store (index, std::memory_order_relaxed);
    for (std::size_t i = 0; i < parameters.size(); ++i)
        parameters[i]->setValue (presetValues[static_cast<std::size_t> (index)][i]);

    controlRefreshPending.store (true, std::memory_order_release);
}

int CombRiotPlugin::getNumPresets() const
{
    return static_cast<int> (presetNames.size());
}

yup::String CombRiotPlugin::getPresetName (int index) const
{
    if (yup::isPositiveAndBelow (index, static_cast<int> (presetNames.size())))
        return presetNames[static_cast<std::size_t> (index)];
    return "Invalid Preset";
}

void CombRiotPlugin::setPresetName (int index, yup::StringRef newName)
{
    if (yup::isPositiveAndBelow (index, static_cast<int> (presetNames.size())))
        presetNames[static_cast<std::size_t> (index)] = newName;
}

yup::Result CombRiotPlugin::loadStateFromMemory (const yup::MemoryBlock& data)
{
    auto loadedPreset = currentPreset.load (std::memory_order_relaxed);
    if (const auto result = loadProductState (*this, data, stateMagic, stateVersion, getNumPresets(), loadedPreset);
        result.failed())
        return result;

    currentPreset.store (loadedPreset, std::memory_order_relaxed);
    controlRefreshPending.store (true, std::memory_order_release);
    return yup::Result::ok();
}

yup::Result CombRiotPlugin::saveStateIntoMemory (yup::MemoryBlock& data)
{
    return saveProductState (*this,
                             data,
                             stateMagic,
                             stateVersion,
                             currentPreset.load (std::memory_order_relaxed));
}

bool CombRiotPlugin::hasEditor() const
{
    return true;
}

yup::AudioProcessorEditor* CombRiotPlugin::createEditor()
{
#ifdef COMBRIOT_PLUGIN_BRIDGE_TEST
    return nullptr;
#else
    return new CombRiotEditor (*this);
#endif
}

void CombRiotPlugin::setStandaloneTriggerGate (bool shouldBeOn) noexcept
{
    const auto wasOn = standaloneGateRequested.exchange (shouldBeOn, std::memory_order_acq_rel);
    if (shouldBeOn == wasOn)
        return;

    auto& counter = shouldBeOn ? standalonePressCount : standaloneReleaseCount;
    counter.fetch_add (1, std::memory_order_release);
}

bool CombRiotPlugin::getStandaloneTriggerGate() const noexcept
{
    return standaloneGateRequested.load (std::memory_order_acquire);
}

float CombRiotPlugin::consumeOutputPeak() noexcept
{
    const auto quantizedPeak = outputPeakQuantized.exchange (0, std::memory_order_acq_rel);
    return static_cast<float> (quantizedPeak) / 1000000.0f;
}

void CombRiotPlugin::updateEngineParameters (int samplePosition, int samplesSinceLastUpdate)
{
    for (auto& handle : parameterHandles)
        handle.advanceToSample (samplePosition);

    auto nextValue = [samplesSinceLastUpdate] (yup::AudioParameterHandle& handle)
    {
        return samplesSinceLastUpdate > 1 ? handle.skip (samplesSinceLastUpdate)
                                          : handle.getNextValue();
    };

    CombRiotParameters engineParameters;
    engineParameters.burstSeconds = nextValue (parameterHandles[burst]);
    engineParameters.decaySeconds = nextValue (parameterHandles[decay]);
    engineParameters.feedback = nextValue (parameterHandles[feedback]);
    engineParameters.polarity = nextValue (parameterHandles[polarity]);
    engineParameters.structure = nextValue (parameterHandles[structure]);
    engineParameters.delaySlew = nextValue (parameterHandles[delaySlew]);
    engineParameters.stereoSpread = nextValue (parameterHandles[stereoSpread]);
    engineParameters.outputGain = std::clamp (decibelsToLinear (nextValue (parameterHandles[output])), 0.0f, 2.0f);
    engineParameters.ceiling = nextValue (parameterHandles[ceiling]);

    engine.setParameters (engineParameters);
}

void CombRiotPlugin::resetControlCadence() noexcept
{
    controlSamplesUntilUpdate = 0;
    samplesSinceControlUpdate = 1;
}

void CombRiotPlugin::applyStandaloneTriggerGate() noexcept
{
    const auto shouldGate = standaloneGateRequested.load (std::memory_order_acquire);
    const auto pressCount = standalonePressCount.load (std::memory_order_acquire);
    const auto releaseCount = standaloneReleaseCount.load (std::memory_order_acquire);
    const auto hasPressEdge = pressCount != consumedStandalonePressCount;
    const auto hasReleaseEdge = releaseCount != consumedStandaloneReleaseCount;

    if (lastNote >= 0)
    {
        standaloneGateActive = false;
        pendingStandaloneRelease = false;
        consumedStandalonePressCount = pressCount;
        consumedStandaloneReleaseCount = releaseCount;
        return;
    }

    if ((hasPressEdge || shouldGate) && ! standaloneGateActive)
    {
        engine.noteOn (60, 1.0f);
        standaloneGateActive = true;
        pendingStandaloneRelease = false;
        samplesSinceStandaloneTrigger = 0;
    }

    if (hasPressEdge)
        consumedStandalonePressCount = pressCount;

    if (hasReleaseEdge)
    {
        consumedStandaloneReleaseCount = releaseCount;
        if (standaloneGateActive)
            pendingStandaloneRelease = true;
    }

    if ((! shouldGate || pendingStandaloneRelease) && standaloneGateActive && samplesSinceStandaloneTrigger > 0)
    {
        engine.noteOff();
        standaloneGateActive = false;
        pendingStandaloneRelease = false;
    }
}

void CombRiotPlugin::advanceStandaloneTriggerClock() noexcept
{
    if (standaloneGateActive && samplesSinceStandaloneTrigger < 0x3fffffff)
        ++samplesSinceStandaloneTrigger;
}

} // namespace violent::plugin

extern "C" yup::AudioProcessor* createPluginProcessor()
{
    return new violent::plugin::CombRiotPlugin();
}
