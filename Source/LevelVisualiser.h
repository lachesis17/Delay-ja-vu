#pragma once

#include <JuceHeader.h>
#include "SignalLevels.h"

//==============================================================================
// stereo LED level visualiser:
// dB scale, rms core + dimmer peak, peak hold, glow
//==============================================================================

struct LevelVisualiser : juce::Component, juce::Timer
{
    //== VISUALISER SETTINGS
    static constexpr float minDb = -60.f;
    static constexpr float maxDb = 6.f;                     // red above 0 dB
    static constexpr float peakFallDbPerSecond = 24.f;
    static constexpr float peakHoldSeconds = 1.5f;
    static constexpr float holdFallDbPerSecond = 14.f;
    static constexpr float rmsRiseSeconds = 0.05f;
    static constexpr float rmsFallSeconds = 0.3f;

    //== LAYOUT
    static constexpr int preferredWidth = 48;
    static constexpr int topPadding = 4;
    static constexpr int footerHeight = 42;                 // peak readout + label

    LevelVisualiser(SignalLevels& levels, const juce::String& labelText);

    void paint(juce::Graphics& g) override;
    void timerCallback() override;

private:
    static constexpr int sidePadding = 5;                   // glow room
    static constexpr float barWidth = 11.f;

    struct BarLevels
    {
        float peakDb = minDb;
        float rmsDb = minDb;
        float holdDb = minDb;
        float holdTime = 0.f;       // seconds since hold last moved up
    };

    bool updateBar(bool left, float elapsedSeconds);
    void paintBar(juce::Graphics& g, juce::Rectangle<float> bounds, const BarLevels& bar, const juce::ColourGradient& gradient) const;
    juce::Rectangle<int> getBarBounds() const;
    static juce::ColourGradient getBarGradient(float bottomY, float topY);
    static float dbToProportion(float db) { return juce::jlimit(0.f, 1.f, (db - minDb) / (maxDb - minDb)); }

    SignalLevels& signalLevels;
    juce::String label;
    BarLevels leftBar, rightBar;
    double lastTimerMs = 0.0;

    const juce::Typeface::Ptr typeface = juce::Typeface::createSystemTypefaceFor(BinaryData::Orbitron_ttf, BinaryData::Orbitron_ttfSize);
};
