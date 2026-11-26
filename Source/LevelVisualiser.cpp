#include "LevelVisualiser.h"

// faded gradient for glow
static juce::ColourGradient fadeGradient(juce::ColourGradient gradient, float alpha)
{
    for (int i = 0; i < gradient.getNumColours(); ++i)
        gradient.setColour(i, gradient.getColour(i).withMultipliedAlpha(alpha));
    return gradient;
}

LevelVisualiser::LevelVisualiser(SignalLevels& levels, const juce::String& labelText)
    : signalLevels(levels), label(labelText)
{
    setInterceptsMouseClicks(false, false);     // don't steal knob drags
    lastTimerMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz(60);
}

//==============================================================================

void LevelVisualiser::timerCallback()
{
    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    const float elapsedSeconds = juce::jlimit(0.f, 0.1f, static_cast<float>((nowMs - lastTimerMs) * 0.001));
    lastTimerMs = nowMs;

    const bool leftChanged = updateBar(true, elapsedSeconds);
    const bool rightChanged = updateBar(false, elapsedSeconds);

    if (leftChanged || rightChanged)
        repaint();      // idle when silent
}

bool LevelVisualiser::updateBar(bool left, float elapsedSeconds)
{
    BarLevels& bar = left ? leftBar : rightBar;
    const BarLevels lastBar = bar;

    const float newPeakDb = juce::Decibels::gainToDecibels(signalLevels.readPeakLevel(left), minDb);
    const float newRmsDb = juce::Decibels::gainToDecibels(signalLevels.readRMSLevel(left), minDb);

    //== PEAK     (instant rise, steady fall)
    bar.peakDb = juce::jmax(newPeakDb, bar.peakDb - peakFallDbPerSecond * elapsedSeconds);

    //== RMS      (smoothed, faster rise than fall)
    const float smoothingSeconds = newRmsDb > bar.rmsDb ? rmsRiseSeconds : rmsFallSeconds;
    bar.rmsDb += (newRmsDb - bar.rmsDb) * (1.f - std::exp(-elapsedSeconds / smoothingSeconds));
    if (std::abs(newRmsDb - bar.rmsDb) < 0.01f)
        bar.rmsDb = newRmsDb;

    //== PEAK HOLD    (stick, then fall)
    if (bar.peakDb >= bar.holdDb)
    {
        bar.holdDb = bar.peakDb;
        bar.holdTime = 0.f;
    }
    else
    {
        bar.holdTime += elapsedSeconds;
        if (bar.holdTime > peakHoldSeconds)
            bar.holdDb = juce::jmax(bar.peakDb, bar.holdDb - holdFallDbPerSecond * elapsedSeconds);
    }

    return bar.peakDb != lastBar.peakDb || bar.rmsDb != lastBar.rmsDb || bar.holdDb != lastBar.holdDb;
}

//==============================================================================

juce::ColourGradient LevelVisualiser::getBarGradient(float bottomY, float topY)
{
    using namespace juce;
    // knob blue -> violet -> white at 0 dB -> red above
    ColourGradient gradient(Colour(43u, 52u, 177u), 0.f, bottomY, Colour(255u, 60u, 100u), 0.f, topY, false);
    gradient.addColour(dbToProportion(-30.f), Colour(70u, 96u, 240u));
    gradient.addColour(dbToProportion(-12.f), Colour(150u, 112u, 255u));
    gradient.addColour(dbToProportion(-3.f), Colour(225u, 190u, 255u));
    gradient.addColour(dbToProportion(0.f), Colour(250u, 238u, 255u));
    gradient.addColour(dbToProportion(0.f) + 0.002f, Colour(255u, 70u, 110u));
    return gradient;
}

juce::Rectangle<int> LevelVisualiser::getBarBounds() const
{
    auto bounds = getLocalBounds();
    bounds.removeFromBottom(footerHeight);
    bounds.removeFromTop(topPadding);
    return bounds.reduced(sidePadding, 0);
}

void LevelVisualiser::paintBar(juce::Graphics& g, juce::Rectangle<float> bounds, const BarLevels& bar, const juce::ColourGradient& gradient) const
{
    using namespace juce;

    //== TRACK
    g.setColour(Colours::black.withAlpha(0.45f));
    g.fillRoundedRectangle(bounds.expanded(2.f), 3.f);
    g.setColour(Colours::white.withAlpha(0.07f));
    g.drawRoundedRectangle(bounds.expanded(2.f), 3.f, 1.f);

    const float peakProportion = dbToProportion(bar.peakDb);
    const float rmsProportion = jmin(dbToProportion(bar.rmsDb), peakProportion);
    const float holdProportion = dbToProportion(bar.holdDb);

    //== GLOW
    if (peakProportion > 0.f)
    {
        const auto litBounds = bounds.withTop(bounds.getBottom() - peakProportion * bounds.getHeight());
        g.setGradientFill(fadeGradient(gradient, 0.10f));
        g.fillRoundedRectangle(litBounds.expanded(5.f, 3.f), 5.f);
        g.setGradientFill(fadeGradient(gradient, 0.18f));
        g.fillRoundedRectangle(litBounds.expanded(2.5f, 1.5f), 3.5f);
    }

    //== SEGMENTS     (solid to rms, dim to peak, faint above)
    constexpr int segmentSpacing = 5;
    constexpr int segmentHeight = 3;
    const int numSegments = jmax(1, (static_cast<int>(bounds.getHeight()) + segmentSpacing - segmentHeight) / segmentSpacing);

    for (int i = 0; i < numSegments; ++i)
    {
        const float segmentBottom = bounds.getBottom() - static_cast<float>(i * segmentSpacing);
        const Rectangle<float> segment(bounds.getX(), segmentBottom - segmentHeight, bounds.getWidth(), static_cast<float>(segmentHeight));
        const float segmentProportion = (bounds.getBottom() - segment.getCentreY()) / bounds.getHeight();
        const float alpha = segmentProportion <= rmsProportion ? 1.f : segmentProportion <= peakProportion ? 0.55f : 0.09f;
        g.setColour(gradient.getColourAtPosition(segmentProportion).withAlpha(alpha));
        g.fillRoundedRectangle(segment, 1.f);
    }

    //== PEAK HOLD
    if (holdProportion > 0.f)
    {
        const float holdY = jmax(bounds.getY() + 1.f, bounds.getBottom() - holdProportion * bounds.getHeight());
        const Colour holdColour = gradient.getColourAtPosition(holdProportion);
        g.setColour(holdColour.withAlpha(0.35f));
        g.fillRoundedRectangle(bounds.getX() - 2.f, holdY - 2.5f, bounds.getWidth() + 4.f, 5.f, 2.f);
        g.setColour(holdColour.brighter(0.5f));
        g.fillRoundedRectangle(bounds.getX(), holdY - 1.f, bounds.getWidth(), 2.f, 1.f);
    }
}

void LevelVisualiser::paint(juce::Graphics& g)
{
    using namespace juce;

    const auto barBounds = getBarBounds().toFloat();
    if (barBounds.getHeight() <= 0.f)
        return;

    const auto gradient = getBarGradient(barBounds.getBottom(), barBounds.getY());
    const auto leftBarBounds = barBounds.withWidth(barWidth);
    const auto rightBarBounds = barBounds.withLeft(barBounds.getRight() - barWidth);
    const auto scaleBounds = barBounds.withTrimmedLeft(barWidth).withTrimmedRight(barWidth);

    paintBar(g, leftBarBounds, leftBar, gradient);
    paintBar(g, rightBarBounds, rightBar, gradient);

    //== DB SCALE
    g.setFont(Font(FontOptions(typeface).withHeight(8.5f).withStyle("plain")));
    for (int scaleDb : { 0, -6, -12, -18, -24, -36, -48 })
    {
        const float scaleY = barBounds.getBottom() - dbToProportion(static_cast<float>(scaleDb)) * barBounds.getHeight();
        g.setColour(Colours::white.withAlpha(scaleDb == 0 ? 0.8f : 0.4f));
        g.drawText(String(std::abs(scaleDb)), Rectangle<float>(scaleBounds.getX(), scaleY - 5.f, scaleBounds.getWidth(), 10.f), Justification::centred, false);
    }

    //== PEAK READOUT + LABEL
    auto footerBounds = getLocalBounds().withTop(getHeight() - footerHeight);
    const auto readoutBounds = footerBounds.removeFromTop(18).withTrimmedTop(4);

    const float loudestHoldDb = jmax(leftBar.holdDb, rightBar.holdDb);
    const String readoutText = loudestHoldDb <= minDb ? String("-inf") : (loudestHoldDb > 0.f ? "+" : "") + String(loudestHoldDb, 1);

    g.setColour(loudestHoldDb > 0.f ? Colour(255u, 70u, 110u) : Colours::white.withAlpha(0.7f));
    g.setFont(Font(FontOptions(typeface).withHeight(9.5f).withStyle("plain")));
    g.drawFittedText(readoutText, readoutBounds, Justification::centred, 1);

    g.setColour(Colours::white);
    g.setFont(Font(FontOptions(typeface).withHeight(15.5f).withStyle("plain")));
    g.drawFittedText(label, footerBounds, Justification::centred, 1);
}
