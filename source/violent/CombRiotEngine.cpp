#include "violent/CombRiotEngine.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace violent
{

CombRiotEngine::CombRiotEngine()
{
    prepare (44100.0);
}

void CombRiotEngine::prepare (double newSampleRate) noexcept
{
    sampleRate = std::isfinite (newSampleRate) && newSampleRate > 1.0 ? newSampleRate : 44100.0;
    dcLeft.prepare (sampleRate);
    dcRight.prepare (sampleRate);
    reset();
    updatePans();
    updateDelayTargets (true);
}

void CombRiotEngine::reset() noexcept
{
    for (auto& voice : voices)
    {
        voice.buffer.fill (0.0f);
        voice.writeIndex = 0;
    }

    noise.reset (noteSeed);
    dcLeft.reset();
    dcRight.reset();
    burstSamplesRemaining = 0;
    burstTotalSamples = 1;
    triggerVelocity = 0.0f;
}

void CombRiotEngine::setParameters (const CombRiotParameters& parameters) noexcept
{
    const auto oldStructure = params.structure;
    const auto oldStereoSpread = params.stereoSpread;

    params.burstSeconds = clampFinite (parameters.burstSeconds, 0.001f, 0.250f, 0.025f);
    params.decaySeconds = clampFinite (parameters.decaySeconds, 0.020f, 12.0f, 1.2f);
    params.feedback = clampFinite (parameters.feedback, 0.0f, 0.985f, 0.82f);
    params.polarity = clampFinite (parameters.polarity, -1.0f, 1.0f, 1.0f);
    params.structure = clampFinite (parameters.structure, 0.0f, 1.0f, 0.5f);
    params.delaySlew = clampFinite (parameters.delaySlew, 0.0f, 1.0f, 0.15f);
    params.stereoSpread = clampFinite (parameters.stereoSpread, 0.0f, 1.0f, 0.8f);
    params.outputGain = clampFinite (parameters.outputGain, 0.0f, 2.0f, 0.45f);
    params.ceiling = clampFinite (parameters.ceiling, 0.05f, 1.0f, 0.95f);

    if (params.stereoSpread != oldStereoSpread)
        updatePans();

    if (params.structure != oldStructure && params.delaySlew > 0.0f)
        updateDelayTargets (false);
}

void CombRiotEngine::noteOn (int midiNote, float velocity) noexcept
{
    currentNote = std::clamp (midiNote, 0, 127);
    triggerVelocity = clampFinite (velocity, 0.0f, 1.0f, 1.0f);
    noteSeed = mixSeed (static_cast<std::uint32_t> (currentNote + 1)
                      ^ static_cast<std::uint32_t> (params.structure * 65535.0f)
                      ^ 0x6352696fu);
    noise.reset (noteSeed);
    updateDelayTargets (true);

    burstTotalSamples = std::max (1, static_cast<int> (std::round (params.burstSeconds * static_cast<float> (sampleRate))));
    burstSamplesRemaining = triggerVelocity > 0.0f ? burstTotalSamples : 0;
}

void CombRiotEngine::noteOff() noexcept
{
    burstSamplesRemaining = 0;
    triggerVelocity = 0.0f;
}

StereoFrame CombRiotEngine::processSample() noexcept
{
    const auto exciter = nextExciterSample();
    const auto slewCoefficient = params.delaySlew <= 0.0f
                               ? 1.0f
                               : (0.00002f + params.delaySlew * params.delaySlew * 0.004f);

    float left = 0.0f;
    float right = 0.0f;

    for (auto& voice : voices)
    {
        if (params.delaySlew > 0.0f)
            voice.delaySamples += (voice.targetDelaySamples - voice.delaySamples) * slewCoefficient;

        const auto delayed = readFractionalDelay (voice);
        const auto feedback = std::clamp (delayed * voiceFeedbackCoefficient (voice), -0.98f, 0.98f);
        const auto writeSample = std::clamp (exciter + feedback, -1.0f, 1.0f);
        voice.buffer[static_cast<std::size_t> (voice.writeIndex)] = writeSample;
        voice.writeIndex = (voice.writeIndex + 1) & delayMask;

        const auto contribution = boundedDrive (delayed, 1.2f) * 0.36f;
        left += contribution * voice.panLeft;
        right += contribution * voice.panRight;
    }

    left = dcLeft.process (left * params.outputGain);
    right = dcRight.process (right * params.outputGain);

    left = std::clamp (boundedDrive (left, 1.05f), -params.ceiling, params.ceiling);
    right = std::clamp (boundedDrive (right, 1.05f), -params.ceiling, params.ceiling);
    return { left, right };
}

void CombRiotEngine::process (float* left, float* right, int numSamples) noexcept
{
    if (left == nullptr || right == nullptr || numSamples <= 0)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto frame = processSample();
        left[i] = frame.left;
        right[i] = frame.right;
    }
}

bool CombRiotEngine::isExciting() const noexcept
{
    return burstSamplesRemaining > 0;
}

std::uint32_t CombRiotEngine::mixSeed (std::uint32_t value) noexcept
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value != 0u ? value : 0x6d2b79f5u;
}

float CombRiotEngine::readFractionalDelay (const Voice& voice) noexcept
{
    const auto safeDelay = clampFinite (voice.delaySamples, 2.0f, static_cast<float> (maxDelaySamples - 3), 240.0f);
    const auto readPosition = static_cast<float> (voice.writeIndex) - safeDelay;
    auto index0 = static_cast<int> (std::floor (readPosition));
    const auto fraction = readPosition - static_cast<float> (index0);
    index0 &= delayMask;
    const auto index1 = (index0 + 1) & delayMask;
    const auto sample0 = voice.buffer[static_cast<std::size_t> (index0)];
    const auto sample1 = voice.buffer[static_cast<std::size_t> (index1)];
    return sample0 + (sample1 - sample0) * fraction;
}

void CombRiotEngine::updateDelayTargets (bool forceCurrentDelays) noexcept
{
    auto state = mixSeed (noteSeed ^ static_cast<std::uint32_t> (params.structure * 16777215.0f));
    const auto minDelay = static_cast<float> (sampleRate) * 0.0035f;
    const auto maxDelay = std::min (static_cast<float> (maxDelaySamples - 4), static_cast<float> (sampleRate) * 0.075f);
    const auto span = std::max (4.0f, maxDelay - minDelay);

    for (int i = 0; i < voiceCount; ++i)
    {
        state = mixSeed (state + 0x9e3779b9u + static_cast<std::uint32_t> (i * 0x45d9f3bu));
        const auto randomUnit = static_cast<float> (state & 0x00ffffffu) / 16777215.0f;
        const auto lane = (static_cast<float> (i) + 0.5f) / static_cast<float> (voiceCount);
        const auto structureSkew = std::fmod (lane * (0.63f + params.structure * 1.71f) + randomUnit * 0.42f, 1.0f);
        auto& voice = voices[static_cast<std::size_t> (i)];
        voice.targetDelaySamples = minDelay + span * structureSkew;
        if (forceCurrentDelays || params.delaySlew <= 0.0f)
            voice.delaySamples = voice.targetDelaySamples;
    }
}

void CombRiotEngine::updatePans() noexcept
{
    for (int i = 0; i < voiceCount; ++i)
    {
        const auto unit = voiceCount == 1 ? 0.5f : static_cast<float> (i) / static_cast<float> (voiceCount - 1);
        const auto pan = 0.5f + (unit - 0.5f) * params.stereoSpread;
        const auto angle = pan * std::numbers::pi_v<float> * 0.5f;
        auto& voice = voices[static_cast<std::size_t> (i)];
        voice.panLeft = std::cos (angle);
        voice.panRight = std::sin (angle);
    }
}

float CombRiotEngine::nextExciterSample() noexcept
{
    if (burstSamplesRemaining <= 0)
        return 0.0f;

    const auto age = burstTotalSamples - burstSamplesRemaining;
    const auto position = static_cast<float> (age) / static_cast<float> (std::max (1, burstTotalSamples));
    const auto envelope = (1.0f - position) * (1.0f - position);
    const auto initialClick = age == 0 ? 1.0f : 0.0f;
    --burstSamplesRemaining;

    const auto noiseSample = noise.nextFloat() * 0.72f + initialClick;
    return std::clamp (noiseSample * envelope * triggerVelocity, -1.0f, 1.0f);
}

float CombRiotEngine::voiceFeedbackCoefficient (const Voice& voice) const noexcept
{
    const auto delaySeconds = voice.delaySamples / static_cast<float> (sampleRate);
    const auto decayCoefficient = std::exp (std::log (0.001f) * delaySeconds / params.decaySeconds);
    return params.polarity * params.feedback * std::clamp (decayCoefficient, 0.0f, 0.985f);
}

} // namespace violent

