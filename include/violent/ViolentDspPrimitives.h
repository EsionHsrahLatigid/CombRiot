#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace violent
{

/** A stereo sample produced by a DSP engine. */
struct StereoFrame
{
    float left = 0.0f;
    float right = 0.0f;
};

/** Returns a finite value constrained to the supplied interval. */
inline float clampFinite (float value, float low, float high, float fallback) noexcept
{
    if (! std::isfinite (value))
        value = fallback;
    return std::clamp (value, low, high);
}

/** Cheap bounded nonlinearity used only as a final realtime safety stage. */
inline float boundedDrive (float input, float drive = 1.0f) noexcept
{
    const auto safeInput = std::isfinite (input) ? input : 0.0f;
    const auto safeDrive = clampFinite (drive, 0.0f, 32.0f, 1.0f);
    return std::tanh (safeInput * safeDrive);
}

/** Small deterministic generator for repeatable realtime-safe testable noise. */
class DeterministicNoise
{
public:
    void reset (std::uint32_t seed) noexcept
    {
        state = seed != 0u ? seed : 0x6d2b79f5u;
    }

    [[nodiscard]] std::uint32_t nextWord() noexcept
    {
        auto value = state;
        value ^= value << 13;
        value ^= value >> 17;
        value ^= value << 5;
        state = value != 0u ? value : 0x6d2b79f5u;
        return state;
    }

    [[nodiscard]] float nextFloat() noexcept
    {
        constexpr auto scale = 1.0 / 2147483648.0;
        return static_cast<float> (static_cast<double> (nextWord()) * scale - 1.0);
    }

private:
    std::uint32_t state = 0x6d2b79f5u;
};

/** One-pole DC blocker for feedback and asymmetric-noise paths. */
class DcBlocker
{
public:
    void prepare (double sampleRate, float cutoffHz = 5.0f) noexcept
    {
        const auto safeRate = std::isfinite (sampleRate) && sampleRate > 1.0 ? sampleRate : 44100.0;
        const auto safeCutoff = clampFinite (cutoffHz, 1.0f, 40.0f, 5.0f);
        coefficient = std::exp (-6.28318530718f * safeCutoff / static_cast<float> (safeRate));
        reset();
    }

    void reset() noexcept
    {
        previousInput = 0.0f;
        previousOutput = 0.0f;
    }

    [[nodiscard]] float process (float input) noexcept
    {
        const auto safeInput = std::isfinite (input) ? input : 0.0f;
        const auto output = safeInput - previousInput + coefficient * previousOutput;
        previousInput = safeInput;
        previousOutput = std::isfinite (output) ? output : 0.0f;
        return previousOutput;
    }

private:
    float coefficient = 0.999f;
    float previousInput = 0.0f;
    float previousOutput = 0.0f;
};

} // namespace violent

