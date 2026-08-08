#pragma once

#include "violent/CombRiotEngine.h"

#include <yup_audio_processors/yup_audio_processors.h>

#include <array>
#include <atomic>
#include <cstdint>

namespace violent::plugin
{

class CombRiotPlugin final : public yup::AudioProcessor
{
public:
    CombRiotPlugin();

    void prepareToPlay (const yup::AudioSpec& spec) override;
    void releaseResources() override;
    void processBlock (yup::AudioProcessContext<float>& context) override;
    void flush() override;

    bool acceptsMidi() const noexcept override;
    int getNumVoices() const override;

    int getCurrentPreset() const noexcept override;
    void setCurrentPreset (int index) noexcept override;
    int getNumPresets() const override;
    yup::String getPresetName (int index) const override;
    void setPresetName (int index, yup::StringRef newName) override;

    yup::Result loadStateFromMemory (const yup::MemoryBlock& data) override;
    yup::Result saveStateIntoMemory (yup::MemoryBlock& data) override;

    bool hasEditor() const override;
    yup::AudioProcessorEditor* createEditor() override;

    void setStandaloneTriggerGate (bool shouldBeOn) noexcept;
    [[nodiscard]] bool getStandaloneTriggerGate() const noexcept;
    [[nodiscard]] float consumeOutputPeak() noexcept;

private:
    enum ParameterIndex
    {
        burst,
        decay,
        feedback,
        polarity,
        structure,
        delaySlew,
        stereoSpread,
        output,
        ceiling,
        parameterCount
    };

    void updateEngineParameters (int samplePosition, int samplesSinceLastUpdate);
    void resetControlCadence() noexcept;
    void applyStandaloneTriggerGate() noexcept;
    void advanceStandaloneTriggerClock() noexcept;

    std::array<yup::AudioParameter::Ptr, parameterCount> parameters;
    std::array<yup::AudioParameterHandle, parameterCount> parameterHandles;
    CombRiotEngine engine;

    int lastNote = -1;
    bool standaloneGateActive = false;
    bool pendingStandaloneRelease = false;
    int samplesSinceStandaloneTrigger = 0;
    std::uint32_t consumedStandalonePressCount = 0;
    std::uint32_t consumedStandaloneReleaseCount = 0;
    int controlSamplesUntilUpdate = 0;
    int samplesSinceControlUpdate = 1;
    std::atomic<bool> standaloneGateRequested { false };
    std::atomic<std::uint32_t> standalonePressCount { 0 };
    std::atomic<std::uint32_t> standaloneReleaseCount { 0 };
    std::atomic<std::uint32_t> outputPeakQuantized { 0 };
    std::atomic<bool> controlRefreshPending { false };
    std::atomic<int> currentPreset { 0 };
    std::array<yup::String, 4> presetNames {
        "Glass Swarm",
        "Negative Hive",
        "Wire Storm",
        "Short Fuse"
    };
};

} // namespace violent::plugin
