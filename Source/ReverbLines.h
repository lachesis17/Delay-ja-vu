#pragma once

#include "PluginProcessor.h"
#include "DelayLine.h"

class ReverbLines {
public:
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
            reverbDelaysLeft[i]->resetSmoothedValue(0.7f);
            reverbDelaysRight[i]->resetSmoothedValue(0.7f);
            reverbDelaysLeft[i]->makeBuffer();
            reverbDelaysRight[i]->makeBuffer();
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

    void updateTargetDelayTimes()
    {
        for (size_t i = 0; i < reverbDelaysLeft.size(); ++i)
        {
            reverbDelaysLeft[i]->setNewTarget(fixedDelayTimesLeft[i]);
            reverbDelaysRight[i]->setNewTarget(fixedDelayTimesRight[i]);
        }
    }

    float applyOnePoleFilter(float current, float next, float coefficient)
    {
        return next + ((next - current) * coefficient);
    }

    float applyReverb(bool left, float sample, float& drywet)
    {
        std::array<std::unique_ptr<DelayLine>, 10>* reverbDelays;
        std::array<juce::dsp::IIR::Filter<float>, 10>* reverbLowPass;
        std::array<juce::dsp::IIR::Filter<float>, 10>* reverbAllPass1;
        std::array<juce::dsp::IIR::Filter<float>, 10>* reverbAllPass2;
        std::array<juce::dsp::IIR::Filter<float>, 10>* reverbAllPass3;
        std::array<juce::dsp::IIR::Filter<float>, 10>* reverbAllPass4;
        std::array<juce::dsp::IIR::Filter<float>, 10>* reverbAllPass5;

        if (left)
        {
            reverbDelays = &reverbDelaysLeft;
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
            reverbLowPass = &reverbLowPassRight;
            reverbAllPass1 = &reverbAllPassRight1;
            reverbAllPass2 = &reverbAllPassRight2;
            reverbAllPass3 = &reverbAllPassRight3;
            reverbAllPass4 = &reverbAllPassRight4;
            reverbAllPass5 = &reverbAllPassRight5;
        }

        float reverbDecay = 0.9f;
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
                float reverb = (*reverbDelays)[i]->getCurrentDelayTime();
                reverb += modAmount;
                reverb = applyOnePoleFilter(reverb, (*reverbDelays)[i]->getSmoothedNext(), coeff);
                (*reverbDelays)[i]->updateDelayTime(reverb);
                float delayedReverbSample = (*reverbDelays)[i]->readBufferDelayedSample();

                delayedReverbSample = (*reverbAllPass1)[i].processSample(delayedReverbSample);
                delayedReverbSample = (*reverbAllPass2)[i].processSample(delayedReverbSample);
                delayedReverbSample = (*reverbAllPass3)[i].processSample(delayedReverbSample);
                delayedReverbSample = (*reverbAllPass4)[i].processSample(delayedReverbSample);
                delayedReverbSample = (*reverbAllPass5)[i].processSample(delayedReverbSample);
                delayedReverbSample = (*reverbLowPass)[i].processSample(delayedReverbSample);

                (*reverbDelays)[i]->writeDelayBuffer(sample, reverbDecay, delayedReverbSample);
                combinedReverb += drywet * delayedReverbSample;
                (*reverbDelays)[i]->updateWriteIndex();
                reverbDecay -= 0.01f;
            }

        return combinedReverb;
    }

private:
    double currentSampleRate;
    float coeff;

    float reverbModRate = 0.005f;
    float reverbModDepth = 0.005f;
    float reverbModPhase = 0.f;
    float reverbModulation = 0.f;

    std::array<std::unique_ptr<DelayLine>, 10> reverbDelaysLeft;
    std::array<std::unique_ptr<DelayLine>, 10> reverbDelaysRight;
    //const std::array<float, 10> fixedDelayTimesLeft = {33.0f, 42.0f, 55.0f, 77.0f, 86.0f, 121.0f, 133.0f, 143.0f, 152.0f, 168.0f};
    //const std::array<float, 10> fixedDelayTimesRight = {43.0f, 64.0f, 72.0f, 89.0f, 101.0f, 117.0f, 125.0f, 130.0f, 142.0f, 158.0f};
    const std::array<float, 10> fixedDelayTimesLeft = {182.20f, 164.17f, 149.06f, 136.87f, 127.59f, 121.24f, 117.80f, 117.29f, 119.69f, 125.02f};
    const std::array<float, 10> fixedDelayTimesRight = {184.03f, 165.81f, 150.55f, 138.24f, 128.87f, 122.45f, 118.98f, 118.46f, 120.89f, 126.27f};
    std::array<juce::dsp::IIR::Filter<float>, 10> reverbLowPassLeft;
    std::array<juce::dsp::IIR::Filter<float>, 10> reverbLowPassRight;
    std::array<juce::dsp::IIR::Filter<float>, 10> reverbAllPassLeft1;
    std::array<juce::dsp::IIR::Filter<float>, 10> reverbAllPassRight1;
    std::array<juce::dsp::IIR::Filter<float>, 10> reverbAllPassLeft2;
    std::array<juce::dsp::IIR::Filter<float>, 10> reverbAllPassRight2;
    std::array<juce::dsp::IIR::Filter<float>, 10> reverbAllPassLeft3;
    std::array<juce::dsp::IIR::Filter<float>, 10> reverbAllPassRight3;
    std::array<juce::dsp::IIR::Filter<float>, 10> reverbAllPassLeft4;
    std::array<juce::dsp::IIR::Filter<float>, 10> reverbAllPassRight4;
    std::array<juce::dsp::IIR::Filter<float>, 10> reverbAllPassLeft5;
    std::array<juce::dsp::IIR::Filter<float>, 10> reverbAllPassRight5;
};
