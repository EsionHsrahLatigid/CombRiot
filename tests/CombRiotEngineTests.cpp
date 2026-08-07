#include "violent/CombRiotEngine.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <vector>

using violent::CombRiotEngine;
using violent::CombRiotParameters;

static_assert (sizeof (CombRiotEngine) < 65536,
               "CombRiotEngine must stay small enough for stack-local tests on Windows");

namespace
{

std::vector<float> renderLeft (int note, float velocity, CombRiotParameters params, int samples)
{
    CombRiotEngine engine;
    engine.prepare (48000.0);
    engine.setParameters (params);
    engine.noteOn (note, velocity);

    std::vector<float> output;
    output.reserve (static_cast<std::size_t> (samples));
    for (int i = 0; i < samples; ++i)
        output.push_back (engine.processSample().left);
    return output;
}

float sumAbs (const std::vector<float>& samples, int begin, int end)
{
    float total = 0.0f;
    for (int i = begin; i < end; ++i)
        total += std::fabs (samples[static_cast<std::size_t> (i)]);
    return total;
}

void assertFiniteBounded (const violent::StereoFrame& frame, float ceiling = 0.9501f)
{
    assert (std::isfinite (frame.left));
    assert (std::isfinite (frame.right));
    assert (frame.left >= -ceiling && frame.left <= ceiling);
    assert (frame.right >= -ceiling && frame.right <= ceiling);
}

void testDeterministicSameNote()
{
    CombRiotParameters params;
    params.structure = 0.31f;
    params.polarity = 1.0f;

    assert (renderLeft (38, 0.8f, params, 4096) == renderLeft (38, 0.8f, params, 4096));
}

void testNoteSelectsStructure()
{
    CombRiotParameters params;
    const auto a = renderLeft (36, 0.8f, params, 4096);
    const auto b = renderLeft (37, 0.8f, params, 4096);

    int different = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        different += a[i] != b[i] ? 1 : 0;

    assert (different > 3000);
}

void testSilenceBeforeAndVelocityZeroTrigger()
{
    CombRiotEngine engine;
    engine.prepare (48000.0);

    for (int i = 0; i < 1024; ++i)
    {
        const auto frame = engine.processSample();
        assert (frame.left == 0.0f);
        assert (frame.right == 0.0f);
    }

    engine.noteOn (60, 0.0f);
    for (int i = 0; i < 2048; ++i)
    {
        const auto frame = engine.processSample();
        assert (frame.left == 0.0f);
        assert (frame.right == 0.0f);
    }
}

void testImpulseNoiseTailAndDecay()
{
    CombRiotParameters params;
    params.burstSeconds = 0.004f;
    params.decaySeconds = 0.35f;
    params.feedback = 0.9f;

    const auto output = renderLeft (41, 1.0f, params, 48000);
    const auto early = sumAbs (output, 128, 4096);
    const auto middle = sumAbs (output, 6000, 12000);
    const auto late = sumAbs (output, 36000, 48000);

    assert (early > 1.0f);
    assert (middle > 0.05f);
    assert (late < middle * 0.65f);
}

void testLongerDecayHasMoreTailEnergy()
{
    CombRiotParameters shortDecay;
    shortDecay.decaySeconds = 0.10f;
    shortDecay.feedback = 0.95f;
    shortDecay.burstSeconds = 0.003f;

    auto longDecay = shortDecay;
    longDecay.decaySeconds = 2.0f;

    const auto shortOutput = renderLeft (48, 1.0f, shortDecay, 36000);
    const auto longOutput = renderLeft (48, 1.0f, longDecay, 36000);

    assert (sumAbs (longOutput, 16000, 36000) > sumAbs (shortOutput, 16000, 36000) * 2.0f);
}

void testPolarityChangesOutput()
{
    CombRiotParameters positive;
    positive.polarity = 1.0f;
    positive.feedback = 0.92f;

    auto negative = positive;
    negative.polarity = -1.0f;

    const auto a = renderLeft (55, 0.9f, positive, 8192);
    const auto b = renderLeft (55, 0.9f, negative, 8192);

    float difference = 0.0f;
    for (std::size_t i = 0; i < a.size(); ++i)
        difference += std::fabs (a[i] - b[i]);

    assert (difference > 8.0f);
}

void testExtremeParametersStayFiniteAndBounded()
{
    CombRiotParameters params;
    params.burstSeconds = std::numeric_limits<float>::infinity();
    params.decaySeconds = std::numeric_limits<float>::quiet_NaN();
    params.feedback = 10.0f;
    params.polarity = -10.0f;
    params.structure = std::numeric_limits<float>::infinity();
    params.delaySlew = 10.0f;
    params.stereoSpread = 10.0f;
    params.outputGain = 10.0f;
    params.ceiling = 0.42f;

    CombRiotEngine engine;
    engine.prepare (0.0);
    engine.setParameters (params);
    engine.noteOn (999, 100.0f);

    for (int i = 0; i < 200000; ++i)
        assertFiniteBounded (engine.processSample(), 0.4201f);
}

void testProcessBuffers()
{
    CombRiotEngine engine;
    engine.prepare (44100.0);
    engine.noteOn (64, 0.7f);

    std::vector<float> left (512);
    std::vector<float> right (512);
    engine.process (left.data(), right.data(), static_cast<int> (left.size()));

    assert (sumAbs (left, 0, static_cast<int> (left.size())) > 0.01f);
    assert (sumAbs (right, 0, static_cast<int> (right.size())) > 0.01f);
}

} // namespace

int main()
{
    testDeterministicSameNote();
    testNoteSelectsStructure();
    testSilenceBeforeAndVelocityZeroTrigger();
    testImpulseNoiseTailAndDecay();
    testLongerDecayHasMoreTailEnergy();
    testPolarityChangesOutput();
    testExtremeParametersStayFiniteAndBounded();
    testProcessBuffers();

    std::cout << "CombRiotEngineTests passed\n";
    return 0;
}
