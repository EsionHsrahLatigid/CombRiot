#pragma once

#include "violent/ViolentDspPrimitives.h"

#include <array>
#include <cstdint>
#include <memory>

namespace violent
{

/** Parameters for CombRiotEngine.

    All fields are sanitized by setParameters().

    burstSeconds controls the seeded noise-burst exciter duration.
    decaySeconds controls the comb feedback decay target.
    feedback scales the decay-derived feedback amount and is clamped below unity.
    polarity morphs the comb loop from negative feedback (-1) through neutral (0)
    to positive feedback (+1).
    structure changes the deterministic delay layout selected at the next noteOn,
    and while delaySlew is nonzero.
    delaySlew controls sample-wise movement toward new delay targets. At 0, delays
    are fixed at trigger time until the next noteOn.
    stereoSpread widens the fixed voice pan matrix.
    outputGain is a linear post-bank gain.
    ceiling is the always-on absolute output peak limit.
*/
struct CombRiotParameters
{
    float burstSeconds = 0.025f;
    float decaySeconds = 1.2f;
    float feedback = 0.82f;
    float polarity = 1.0f;
    float structure = 0.5f;
    float delaySlew = 0.15f;
    float stereoSpread = 0.8f;
    float outputGain = 0.45f;
    float ceiling = 0.95f;
};

/** Monophonic MIDI-triggered deterministic comb-resonator swarm.

    CombRiotEngine is a pure C++20 stereo instrument core. It does not track
    conventional pitch: note number selects the deterministic exciter seed and
    eight-comb delay structure. The render path performs no dynamic allocation.
*/
class CombRiotEngine
{
public:
    CombRiotEngine();

    /** Sets sample rate, clears state, and keeps the current sanitized parameters. */
    void prepare (double sampleRate) noexcept;

    /** Clears all delay lines, filters, and excitation state. */
    void reset() noexcept;

    /** Sanitizes and applies parameters. Delay targets update during a held note only when delaySlew > 0. */
    void setParameters (const CombRiotParameters& parameters) noexcept;

    /** Starts a deterministic burst. Note selects seed/structure; velocity scales excitation only. */
    void noteOn (int midiNote, float velocity) noexcept;

    /** Stops new excitation. Existing comb tails decay naturally. */
    void noteOff() noexcept;

    /** Renders one bounded stereo sample. */
    [[nodiscard]] StereoFrame processSample() noexcept;

    /** Renders numSamples stereo frames into non-null output buffers. */
    void process (float* left, float* right, int numSamples) noexcept;

    [[nodiscard]] bool isExciting() const noexcept;

private:
    static constexpr int voiceCount = 8;
    static constexpr int maxDelaySamples = 32768;
    static constexpr int delayMask = maxDelaySamples - 1;

    struct Voice
    {
        std::unique_ptr<std::array<float, maxDelaySamples>> buffer = std::make_unique<std::array<float, maxDelaySamples>>();
        float delaySamples = 240.0f;
        float targetDelaySamples = 240.0f;
        float panLeft = 0.70710678f;
        float panRight = 0.70710678f;
        int writeIndex = 0;
    };

    struct ClampedParameters
    {
        float burstSeconds = 0.025f;
        float decaySeconds = 1.2f;
        float feedback = 0.82f;
        float polarity = 1.0f;
        float structure = 0.5f;
        float delaySlew = 0.15f;
        float stereoSpread = 0.8f;
        float outputGain = 0.45f;
        float ceiling = 0.95f;
    };

    static std::uint32_t mixSeed (std::uint32_t value) noexcept;
    static float readFractionalDelay (const Voice& voice) noexcept;

    void updateDelayTargets (bool forceCurrentDelays) noexcept;
    void updatePans() noexcept;
    [[nodiscard]] float nextExciterSample() noexcept;
    [[nodiscard]] float voiceFeedbackCoefficient (const Voice& voice) const noexcept;

    std::array<Voice, voiceCount> voices {};
    DeterministicNoise noise;
    DcBlocker dcLeft;
    DcBlocker dcRight;
    ClampedParameters params;

    double sampleRate = 44100.0;
    std::uint32_t noteSeed = 1u;
    int currentNote = 60;
    int burstSamplesRemaining = 0;
    int burstTotalSamples = 1;
    float triggerVelocity = 0.0f;
};

} // namespace violent
