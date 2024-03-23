#pragma once

#include "PluginProcessor.h"

class Filters {
public:
    float lastLowPassFreq;
    float lastHighPassFreq;

    Filters(double sampleRate) : currentSampleRate(sampleRate)
    {
        setupFilters();
    }

    void setupFilters()
    {
        leftLowPass.reset(); 
        rightLowPass.reset();
        leftHighPass.reset(); 
        rightHighPass.reset();
        leftLowAll.reset();
        rightLowAll.reset();

        juce::dsp::IIR::Coefficients<float>::Ptr coefficientsLow = juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, 2000);     //const double highSampleRate = 1e6; // 1mil hz
        juce::dsp::IIR::Coefficients<float>::Ptr coefficientsHigh = juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, 500); 
        juce::dsp::IIR::Coefficients<float>::Ptr coefficientsLowAll = juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, 7000);

        leftLowPass.coefficients = coefficientsLow;
        rightLowPass.coefficients = coefficientsLow;
        leftHighPass.coefficients = coefficientsHigh;
        rightHighPass.coefficients = coefficientsHigh;
        leftLowAll.coefficients = coefficientsLowAll;
        rightLowAll.coefficients = coefficientsLowAll;
    }

    void resetSmoothing()
    {
        smoothedLowPassFreq.reset(currentSampleRate, 0.0075f);
        smoothedHighPassFreq.reset(currentSampleRate, 0.0075f);
    }

    float processLowFilter(bool left, float sample)
    {
        if (left)
            return leftLowPass.processSample(sample);

        return rightLowPass.processSample(sample);
    }

    float processHighFilter(bool left, float sample)
    {
        if (left)
            return leftHighPass.processSample(sample);
            
        return rightHighPass.processSample(sample);
    }

    float processGeneralLowFilter(bool left, float sample)
    {
        if (left)
            return leftLowAll.processSample(sample);

        return rightLowAll.processSample(sample);
    }

    void updateLowPassFilter(float newLowPassFreq, float coeff)
    {
        smoothedLowPassFreq.setTargetValue(newLowPassFreq);
        newLowPassFreq = applyOnePoleFilter(smoothedLowPassFreq.getCurrentValue(), smoothedLowPassFreq.getNextValue(), coeff);
        updateLowCoefficients(leftLowPass, newLowPassFreq, currentSampleRate);
        updateLowCoefficients(rightLowPass, newLowPassFreq, currentSampleRate);
        lastLowPassFreq = newLowPassFreq;
    }

    void updateHighPassFilter(float newHighPassFreq, float coeff)
    {
        smoothedHighPassFreq.setTargetValue(newHighPassFreq);
        newHighPassFreq = applyOnePoleFilter(smoothedHighPassFreq.getCurrentValue(), smoothedHighPassFreq.getNextValue(), coeff);
        updateHighCoefficients(leftHighPass, newHighPassFreq, currentSampleRate);
        updateHighCoefficients(rightHighPass, newHighPassFreq, currentSampleRate);
        lastHighPassFreq = newHighPassFreq;
    }

    void updateLowCoefficients(juce::dsp::IIR::Filter<float>& filter, float frequency, double sampleRate)
    {
        auto coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, frequency);
        filter.coefficients = coefficients;
    }

    void updateHighCoefficients(juce::dsp::IIR::Filter<float>& filter, float frequency, double sampleRate)
    {
        auto coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, frequency);
        filter.coefficients = coefficients;
    }

    float applyOnePoleFilter(float current, float next, float coefficient)
    {
        return next + ((next - current) * coefficient);
    }

private:
    double currentSampleRate;

    juce::dsp::IIR::Filter<float> leftLowPass, rightLowPass, leftHighPass, rightHighPass, leftLowAll, rightLowAll;
    juce::LinearSmoothedValue<float> smoothedLowPassFreq, smoothedHighPassFreq;
};
