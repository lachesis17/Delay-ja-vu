#pragma once

#include "PluginProcessor.h"
#include "DelayLine.h"

class ReverbLines {
public:
    //== REVERB SETTINGS
    static constexpr float decaySeconds = 8.0f;     // time for tail to fall 60dB
    static constexpr float outputGain = 3.0f;       // wet gain
    static constexpr float dampingHz = 6000.f;      // lower = darker tail

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
        }
    }

    float applyOnePoleFilter(float current, float next, float coefficient)
    {
        return current + ((next - current) * coefficient);
    }

    float applyReverb(bool left, float sample)
    {
        std::array<std::unique_ptr<DelayLine>, numCombs>* reverbDelays;
        const std::array<float, numCombs>* fixedDelayTimes;
        const std::array<float, numCombs>* reverbDecay;
        std::array<float, numCombs>* reverbLowPass;

        if (left)
        {
            reverbDelays = &reverbDelaysLeft;
            fixedDelayTimes = &fixedDelayTimesLeft;
            reverbDecay = &reverbDecayLeft;
            reverbLowPass = &reverbLowPassLeft;
        }
        else
        {
            reverbDelays = &reverbDelaysRight;
            fixedDelayTimes = &fixedDelayTimesRight;
            reverbDecay = &reverbDecayRight;
            reverbLowPass = &reverbLowPassRight;
        }

        float combinedReverb = 0.0f;
        float input = sample * inputGain;

        //== LFO
        reverbModPhase += (2.0f * juce::MathConstants<float>::pi * reverbModRate) / static_cast<float>(currentSampleRate);
        reverbModPhase = std::fmod(reverbModPhase, 2.0f * juce::MathConstants<float>::pi);
        if (reverbModPhase < 0)
        {
            reverbModPhase += 2.0 * juce::MathConstants<float>::pi;
        }
        float lfo = std::sin(reverbModPhase);
        float modDepthInSamples = (reverbModDepth / 1000.0f) * static_cast<float>(currentSampleRate);
        float modAmount = lfo * modDepthInSamples;

        for (size_t i = 0; i < reverbDelays->size(); ++i)
            {
                float reverb = (*fixedDelayTimes)[i] + modAmount;       // fixed length, no glide from 0 at start
                (*reverbDelays)[i]->updateDelayTime(reverb);
                float delayedReverbSample = (*reverbDelays)[i]->readBufferDelayedSample();

                // damping highs lose a little every loop, so tail darkens
                (*reverbLowPass)[i] = applyOnePoleFilter((*reverbLowPass)[i], delayedReverbSample, coeff);

                (*reverbDelays)[i]->writeDelayBuffer(input, (*reverbDecay)[i], (*reverbLowPass)[i]);
                combinedReverb += delayedReverbSample;
                (*reverbDelays)[i]->updateWriteIndex();
            }

        return combinedReverb * outputGain;
    }

private:
    static constexpr size_t numCombs = 8;
    static constexpr float inputGain = 0.03f;   // each comb stacks up so feed quietly

    double currentSampleRate;
    float coeff;

    float reverbModRate = 0.005f;
    float reverbModDepth = 0.005f;
    float reverbModPhase = 0.f;
    float reverbModulation = 0.f;

    std::array<std::unique_ptr<DelayLine>, numCombs> reverbDelaysLeft;
    std::array<std::unique_ptr<DelayLine>, numCombs> reverbDelaysRight;
    const std::array<float, numCombs> fixedDelayTimesLeft = {25.306f, 26.939f, 28.957f, 30.748f, 32.245f, 33.810f, 35.306f, 36.667f};
    const std::array<float, numCombs> fixedDelayTimesRight = {25.828f, 27.460f, 29.478f, 31.270f, 32.766f, 34.331f, 35.828f, 37.188f};
    std::array<float, numCombs> reverbDecayLeft;
    std::array<float, numCombs> reverbDecayRight;
    std::array<float, numCombs> reverbLowPassLeft;      // damping filter memory, one per comb
    std::array<float, numCombs> reverbLowPassRight;
};
