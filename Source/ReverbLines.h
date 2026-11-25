#pragma once

#include "PluginProcessor.h"
#include "DelayLine.h"

class ReverbLines {
public:
    //== REVERB SETTINGS
    static constexpr float decaySeconds = 3.0f;     // time for tail to fall 60dB

    ReverbLines(double sampleRate) : currentSampleRate(sampleRate),
    coeff(1.0f - static_cast<float>(std::exp(-1.0f / (0.01f * sampleRate))))
    {
        setupDelaysAndFilters();
    }

    void setupDelaysAndFilters()
    {
        juce::dsp::IIR::Coefficients<float>::Ptr coefficientsLowReverb = juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, 3277);  
        juce::dsp::IIR::Coefficients<float>::Ptr coefficientsAllPass1 = juce::dsp::IIR::Coefficients<float>::makeAllPass(currentSampleRate, 500, 0.55f);
        juce::dsp::IIR::Coefficients<float>::Ptr coefficientsAllPass2 = juce::dsp::IIR::Coefficients<float>::makeAllPass(currentSampleRate, 1500, 0.575f);
        juce::dsp::IIR::Coefficients<float>::Ptr coefficientsAllPass3 = juce::dsp::IIR::Coefficients<float>::makeAllPass(currentSampleRate, 2500, 0.6f);
        juce::dsp::IIR::Coefficients<float>::Ptr coefficientsAllPass4 = juce::dsp::IIR::Coefficients<float>::makeAllPass(currentSampleRate, 4000, 0.65f);
        juce::dsp::IIR::Coefficients<float>::Ptr coefficientsAllPass5 = juce::dsp::IIR::Coefficients<float>::makeAllPass(currentSampleRate, 5000, 0.7f);

        for (size_t i = 0; i < reverbDelaysLeft.size(); ++i) {
            reverbDelaysLeft[i] = std::make_unique<DelayLine>(currentSampleRate);
            reverbDelaysRight[i] = std::make_unique<DelayLine>(currentSampleRate);
            reverbDelaysLeft[i]->makeBuffer();
            reverbDelaysRight[i]->makeBuffer();

            // feedback so each comb falls 60 dB (x0.001) in decaySeconds. longer combs loop fewer times, so they keep more per loop
            reverbDecayLeft[i] = std::pow(10.f, -3.f * fixedDelayTimesLeft[i] / (decaySeconds * 1000.f));
            reverbDecayRight[i] = std::pow(10.f, -3.f * fixedDelayTimesRight[i] / (decaySeconds * 1000.f));

            reverbLowPassLeft[i].reset();
            reverbLowPassRight[i].reset();
            reverbLowPassLeft[i].coefficients = coefficientsLowReverb;
            reverbLowPassRight[i].coefficients = coefficientsLowReverb;
            reverbAllPassLeft1[i].reset();
            reverbAllPassRight1[i].reset();
            reverbAllPassLeft1[i].coefficients = coefficientsAllPass1;
            reverbAllPassRight1[i].coefficients = coefficientsAllPass1;
            reverbAllPassLeft2[i].reset();
            reverbAllPassRight2[i].reset();
            reverbAllPassLeft2[i].coefficients = coefficientsAllPass2;
            reverbAllPassRight2[i].coefficients = coefficientsAllPass2;
            reverbAllPassLeft3[i].reset();
            reverbAllPassRight3[i].reset();
            reverbAllPassLeft3[i].coefficients = coefficientsAllPass3;
            reverbAllPassRight3[i].coefficients = coefficientsAllPass3;
            reverbAllPassLeft4[i].reset();
            reverbAllPassRight4[i].reset();
            reverbAllPassLeft4[i].coefficients = coefficientsAllPass4;
            reverbAllPassRight4[i].coefficients = coefficientsAllPass4;
            reverbAllPassLeft5[i].reset();
            reverbAllPassRight5[i].reset();
            reverbAllPassLeft5[i].coefficients = coefficientsAllPass5;
            reverbAllPassRight5[i].coefficients = coefficientsAllPass5;
        }
    }

    float applyOnePoleFilter(float current, float next, float coefficient)
    {
        return next + ((next - current) * coefficient);
    }

    float applyReverb(bool left, float sample, float& drywet)
    {
        std::array<std::unique_ptr<DelayLine>, numCombs>* reverbDelays;
        const std::array<float, numCombs>* fixedDelayTimes;
        const std::array<float, numCombs>* reverbDecay;
        std::array<juce::dsp::IIR::Filter<float>, numCombs>* reverbLowPass;
        std::array<juce::dsp::IIR::Filter<float>, numCombs>* reverbAllPass1;
        std::array<juce::dsp::IIR::Filter<float>, numCombs>* reverbAllPass2;
        std::array<juce::dsp::IIR::Filter<float>, numCombs>* reverbAllPass3;
        std::array<juce::dsp::IIR::Filter<float>, numCombs>* reverbAllPass4;
        std::array<juce::dsp::IIR::Filter<float>, numCombs>* reverbAllPass5;

        if (left)
        {
            reverbDelays = &reverbDelaysLeft;
            fixedDelayTimes = &fixedDelayTimesLeft;
            reverbDecay = &reverbDecayLeft;
            reverbLowPass = &reverbLowPassLeft;
            reverbAllPass1 = &reverbAllPassLeft1;
            reverbAllPass2 = &reverbAllPassLeft2;
            reverbAllPass3 = &reverbAllPassLeft3;
            reverbAllPass4 = &reverbAllPassLeft4;
            reverbAllPass5 = &reverbAllPassLeft5;
        }
        else
        {
            reverbDelays = &reverbDelaysRight;
            fixedDelayTimes = &fixedDelayTimesRight;
            reverbDecay = &reverbDecayRight;
            reverbLowPass = &reverbLowPassRight;
            reverbAllPass1 = &reverbAllPassRight1;
            reverbAllPass2 = &reverbAllPassRight2;
            reverbAllPass3 = &reverbAllPassRight3;
            reverbAllPass4 = &reverbAllPassRight4;
            reverbAllPass5 = &reverbAllPassRight5;
        }

        float combinedReverb = 0.0f;

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

                delayedReverbSample = (*reverbAllPass1)[i].processSample(delayedReverbSample);
                delayedReverbSample = (*reverbAllPass2)[i].processSample(delayedReverbSample);
                delayedReverbSample = (*reverbAllPass3)[i].processSample(delayedReverbSample);
                delayedReverbSample = (*reverbAllPass4)[i].processSample(delayedReverbSample);
                delayedReverbSample = (*reverbAllPass5)[i].processSample(delayedReverbSample);
                delayedReverbSample = (*reverbLowPass)[i].processSample(delayedReverbSample);

                (*reverbDelays)[i]->writeDelayBuffer(sample, (*reverbDecay)[i], delayedReverbSample);
                combinedReverb += drywet * delayedReverbSample;
                (*reverbDelays)[i]->updateWriteIndex();
            }

        return combinedReverb;
    }

private:
    static constexpr size_t numCombs = 8;

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
    std::array<juce::dsp::IIR::Filter<float>, numCombs> reverbLowPassLeft;
    std::array<juce::dsp::IIR::Filter<float>, numCombs> reverbLowPassRight;
    std::array<juce::dsp::IIR::Filter<float>, numCombs> reverbAllPassLeft1;
    std::array<juce::dsp::IIR::Filter<float>, numCombs> reverbAllPassRight1;
    std::array<juce::dsp::IIR::Filter<float>, numCombs> reverbAllPassLeft2;
    std::array<juce::dsp::IIR::Filter<float>, numCombs> reverbAllPassRight2;
    std::array<juce::dsp::IIR::Filter<float>, numCombs> reverbAllPassLeft3;
    std::array<juce::dsp::IIR::Filter<float>, numCombs> reverbAllPassRight3;
    std::array<juce::dsp::IIR::Filter<float>, numCombs> reverbAllPassLeft4;
    std::array<juce::dsp::IIR::Filter<float>, numCombs> reverbAllPassRight4;
    std::array<juce::dsp::IIR::Filter<float>, numCombs> reverbAllPassLeft5;
    std::array<juce::dsp::IIR::Filter<float>, numCombs> reverbAllPassRight5;
};
