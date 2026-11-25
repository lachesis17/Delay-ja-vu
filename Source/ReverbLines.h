#pragma once

#include "PluginProcessor.h"
#include "DelayLine.h"

class ReverbLines {
public:
    //== REVERB SETTINGS
    static constexpr float decaySeconds = 8.0f;     // time for tail to fall 60dB
    static constexpr float outputGain = 3.0f;       // wet gain
    static constexpr float dampingHz = 6000.f;      // lower = darker tail
    static constexpr float preDelayMs = 20.f;       // gap between dry and reverb
    static constexpr float inputHighPassHz = 120.f; // keep bass out of tail
    static constexpr float width = 1.0f;            // 0 = mono tail, 1 = full stereo
    static constexpr float reverbModDepth = 0.25f;  // ms of comb wobble

    // damping coeff with same exp. smoothing, with dampingHz for decay
    ReverbLines(double sampleRate) : currentSampleRate(sampleRate),
    coeff(1.0f - static_cast<float>(std::exp(-2.0 * juce::MathConstants<double>::pi * dampingHz / sampleRate)))
    {
        setupDelaysAndFilters();
    }

    void setupDelaysAndFilters()
    {
        for (size_t i = 0; i < reverbDelaysLeft.size(); ++i) {
            reverbDelaysLeft[i] = std::make_unique<DelayLine>(currentSampleRate);
            reverbDelaysRight[i] = std::make_unique<DelayLine>(currentSampleRate);
            reverbDelaysLeft[i]->makeBuffer();
            reverbDelaysRight[i]->makeBuffer();

            // feedback so each comb falls 60 dB (x0.001) in decaySeconds. longer combs loop fewer times, so they keep more per loop
            reverbDecayLeft[i] = std::pow(10.f, -3.f * fixedDelayTimesLeft[i] / (decaySeconds * 1000.f));
            reverbDecayRight[i] = std::pow(10.f, -3.f * fixedDelayTimesRight[i] / (decaySeconds * 1000.f));

            reverbLowPassLeft[i] = 0.f;
            reverbLowPassRight[i] = 0.f;

            // LFO per comb
            const float twoPi = juce::MathConstants<float>::twoPi;
            const float sampleRate = static_cast<float>(currentSampleRate);
            reverbModRateLeft[i] = twoPi * (0.31f + 0.083f * static_cast<float>(i)) / sampleRate;      // phase per sample
            reverbModRateRight[i] = twoPi * (0.37f + 0.077f * static_cast<float>(i)) / sampleRate;
            reverbModPhaseLeft[i] = twoPi * static_cast<float>(i) / static_cast<float>(numCombs);
            reverbModPhaseRight[i] = reverbModPhaseLeft[i] + 0.25f * twoPi;
        }

        // ALL PASS
        for (size_t i = 0; i < reverbAllPassLeft.size(); ++i) {
            allPassSamplesLeft[i] = static_cast<int>(std::lround(fixedAllPassTimesLeft[i] * currentSampleRate / 1000.0));
            allPassSamplesRight[i] = static_cast<int>(std::lround(fixedAllPassTimesRight[i] * currentSampleRate / 1000.0));
            reverbAllPassLeft[i].createCircularBuffer(static_cast<unsigned int>(allPassSamplesLeft[i]));
            reverbAllPassRight[i].createCircularBuffer(static_cast<unsigned int>(allPassSamplesRight[i]));
        }

        // PRE-DELAY + HIGH PASS
        reverbPreDelay = std::make_unique<DelayLine>(currentSampleRate);
        reverbPreDelay->makeBuffer();
        reverbPreDelay->updateDelayTime(preDelayMs);
        highPassCoeff = 1.0f - static_cast<float>(std::exp(-2.0 * juce::MathConstants<double>::pi * inputHighPassHz / currentSampleRate));
        reverbHighPass = 0.f;
    }

    float applyOnePoleFilter(float current, float next, float coefficient)
    {
        return current + ((next - current) * coefficient);
    }

    // stereo in, wet stereo out
    void applyReverb(float inLeft, float inRight, float& outLeft, float& outRight)
    {
        //== MONO IN + HIGH PASS
        float input = 0.5f * (inLeft + inRight) * inputGain;
        reverbHighPass = applyOnePoleFilter(reverbHighPass, input, highPassCoeff);     // low pass of input...
        input -= reverbHighPass;                                                        // ...taken away leaves highs

        //== PRE-DELAY
        float delayedInput = reverbPreDelay->readBufferDelayedSample();
        reverbPreDelay->writeDelayBuffer(input, 0.f, 0.f);      // no feedback, just delay
        reverbPreDelay->updateWriteIndex();

        //== COMBS + ALL PASS
        float left = applyReverbSide(true, delayedInput);
        float right = applyReverbSide(false, delayedInput);

        //== WIDTH  (blend some of each side into the other)
        const float wet1 = 0.5f + 0.5f * width;
        const float wet2 = 0.5f - 0.5f * width;
        outLeft = left * wet1 + right * wet2;
        outRight = right * wet1 + left * wet2;
    }

private:
    float applyReverbSide(bool left, float input)
    {
        std::array<std::unique_ptr<DelayLine>, numCombs>* reverbDelays;
        const std::array<float, numCombs>* fixedDelayTimes;
        const std::array<float, numCombs>* reverbDecay;
        std::array<float, numCombs>* reverbLowPass;
        std::array<float, numCombs>* reverbModPhase;
        const std::array<float, numCombs>* reverbModRate;
        std::array<CircularBuffer<float>, numAllPasses>* reverbAllPass;
        const std::array<int, numAllPasses>* allPassSamples;

        if (left)
        {
            reverbDelays = &reverbDelaysLeft;
            fixedDelayTimes = &fixedDelayTimesLeft;
            reverbDecay = &reverbDecayLeft;
            reverbLowPass = &reverbLowPassLeft;
            reverbModPhase = &reverbModPhaseLeft;
            reverbModRate = &reverbModRateLeft;
            reverbAllPass = &reverbAllPassLeft;
            allPassSamples = &allPassSamplesLeft;
        }
        else
        {
            reverbDelays = &reverbDelaysRight;
            fixedDelayTimes = &fixedDelayTimesRight;
            reverbDecay = &reverbDecayRight;
            reverbLowPass = &reverbLowPassRight;
            reverbModPhase = &reverbModPhaseRight;
            reverbModRate = &reverbModRateRight;
            reverbAllPass = &reverbAllPassRight;
            allPassSamples = &allPassSamplesRight;
        }

        float combinedReverb = 0.0f;

        for (size_t i = 0; i < reverbDelays->size(); ++i)
            {
                //== LFO
                float reverb = (*fixedDelayTimes)[i] + reverbModDepth * std::sin((*reverbModPhase)[i]);
                (*reverbModPhase)[i] += (*reverbModRate)[i];
                if ((*reverbModPhase)[i] >= juce::MathConstants<float>::twoPi)
                    (*reverbModPhase)[i] -= juce::MathConstants<float>::twoPi;

                (*reverbDelays)[i]->updateDelayTime(reverb);
                float delayedReverbSample = (*reverbDelays)[i]->readBufferDelayedSample();

                // damping highs lose a little every loop, so tail darkens
                (*reverbLowPass)[i] = applyOnePoleFilter((*reverbLowPass)[i], delayedReverbSample, coeff);

                (*reverbDelays)[i]->writeDelayBuffer(input, (*reverbDecay)[i], (*reverbLowPass)[i]);
                combinedReverb += delayedReverbSample;
                (*reverbDelays)[i]->updateWriteIndex();
            }

        //== ALL PASS (schroeder)
        for (size_t i = 0; i < reverbAllPass->size(); ++i)
            {
                float delayedAllPass = (*reverbAllPass)[i].readBuffer((*allPassSamples)[i] - 1);     // readBuffer(n) is n + 1 samples ago
                float toBuffer = combinedReverb + allPassGain * delayedAllPass;
                (*reverbAllPass)[i].writeBuffer(toBuffer);
                combinedReverb = delayedAllPass - allPassGain * toBuffer;
            }

        return combinedReverb * outputGain;
    }

    static constexpr size_t numCombs = 8;
    static constexpr size_t numAllPasses = 4;
    static constexpr float inputGain = 0.03f;   // each comb stacks up so feed quietly
    static constexpr float allPassGain = 0.5f;  // how much each allpass smears

    double currentSampleRate;
    float coeff;

    std::array<std::unique_ptr<DelayLine>, numCombs> reverbDelaysLeft;
    std::array<std::unique_ptr<DelayLine>, numCombs> reverbDelaysRight;
    const std::array<float, numCombs> fixedDelayTimesLeft = {25.306f, 26.939f, 28.957f, 30.748f, 32.245f, 33.810f, 35.306f, 36.667f};
    const std::array<float, numCombs> fixedDelayTimesRight = {25.828f, 27.460f, 29.478f, 31.270f, 32.766f, 34.331f, 35.828f, 37.188f};
    std::array<float, numCombs> reverbDecayLeft;
    std::array<float, numCombs> reverbDecayRight;
    std::array<float, numCombs> reverbLowPassLeft;
    std::array<float, numCombs> reverbLowPassRight;
    std::array<float, numCombs> reverbModPhaseLeft;     // LFO position per comb (radians)
    std::array<float, numCombs> reverbModPhaseRight;
    std::array<float, numCombs> reverbModRateLeft;      // LFO phase per sample
    std::array<float, numCombs> reverbModRateRight;

    const std::array<float, numAllPasses> fixedAllPassTimesLeft = {12.608f, 10.000f, 7.732f, 5.102f};
    const std::array<float, numAllPasses> fixedAllPassTimesRight = {13.129f, 10.522f, 8.254f, 5.624f};
    std::array<int, numAllPasses> allPassSamplesLeft;
    std::array<int, numAllPasses> allPassSamplesRight;
    std::array<CircularBuffer<float>, numAllPasses> reverbAllPassLeft;
    std::array<CircularBuffer<float>, numAllPasses> reverbAllPassRight;

    std::unique_ptr<DelayLine> reverbPreDelay;
    float highPassCoeff = 0.f;
    float reverbHighPass = 0.f;
};
