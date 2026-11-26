#pragma once

#include <JuceHeader.h>

//==============================================================================
// peak + rms for the visualisers
// holds loudest value until GUI reads it (read resets), so hits between frames still show
//==============================================================================

class SignalLevels {
public:
    void updateLevels(const juce::AudioBuffer<float>& buffer, int numChannels)
    {
        const int numChannelsToRead = juce::jmin(numChannels, buffer.getNumChannels());
        const int numSamples = buffer.getNumSamples();
        if (numChannelsToRead <= 0 || numSamples <= 0)
            return;

        const int rightChannel = numChannelsToRead > 1 ? 1 : 0;     // mono feeds both sides

        storeIfLouder(peakLevelLeft, buffer.getMagnitude(0, 0, numSamples));
        storeIfLouder(peakLevelRight, buffer.getMagnitude(rightChannel, 0, numSamples));
        storeIfLouder(rmsLevelLeft, buffer.getRMSLevel(0, 0, numSamples));
        storeIfLouder(rmsLevelRight, buffer.getRMSLevel(rightChannel, 0, numSamples));
    }

    float readPeakLevel(bool left) { return (left ? peakLevelLeft : peakLevelRight).exchange(0.f, std::memory_order_relaxed); }
    float readRMSLevel(bool left)  { return (left ? rmsLevelLeft : rmsLevelRight).exchange(0.f, std::memory_order_relaxed); }

private:
    static void storeIfLouder(std::atomic<float>& level, float newLevel)
    {
        float currentLevel = level.load(std::memory_order_relaxed);
        while (newLevel > currentLevel && ! level.compare_exchange_weak(currentLevel, newLevel, std::memory_order_relaxed)) {}
    }

    std::atomic<float> peakLevelLeft { 0.f }, peakLevelRight { 0.f };
    std::atomic<float> rmsLevelLeft { 0.f }, rmsLevelRight { 0.f };
};
