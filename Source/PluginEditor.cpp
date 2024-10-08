#include "PluginProcessor.h"
#include "PluginEditor.h"

void RotaryLookAndFeel::drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height, float currentValue,
float rotaryStartAngle, float rotaryEndAngle, juce::Slider &slider)
{
    using namespace juce;

    auto yOffset = JUCE_LIVE_CONSTANT(10);
    auto bounds = Rectangle<float>(static_cast<float>(x), static_cast<float>(y) + static_cast<float>(yOffset), static_cast<float>(width), static_cast<float>(height));

    // gradient colors
    auto enabled = dynamic_cast<RotarySliderToggle*>(&slider) ? dynamic_cast<RotarySliderToggle*>(&slider)->getSliderState() : true;
    float alpha = dynamic_cast<RotarySliderToggle*>(&slider) ? dynamic_cast<RotarySliderToggle*>(&slider)->getAlpha() : 1.0f;

    ColourGradient enabledGradient = getSliderGradient(width, height, true);
    ColourGradient disabledGradient = getSliderGradient(width, height, false);
    ColourGradient currentGradient = enabled ? enabledGradient : disabledGradient;

    currentGradient.clearColours();
    for (int i = 0; i < enabledGradient.getNumColours(); ++i)
    {
        Colour col = enabledGradient.getColour(i).interpolatedWith(disabledGradient.getColour(i), 1.0f - alpha);
        currentGradient.addColour(enabledGradient.getColourPosition(i), col);
    }

    g.setGradientFill(currentGradient);
    g.fillEllipse(bounds.reduced(JUCE_LIVE_CONSTANT(15.f)));

    // value arcs
    auto endValueAngle = rotaryStartAngle + currentValue * (rotaryEndAngle - rotaryStartAngle); // set the max angle of the value arc to the current value
    auto valueLineWidth = jmin(4.0f, width * 0.05f);
    auto valueArcOuterRadius = (width * 0.55f + valueLineWidth) * JUCE_LIVE_CONSTANT(0.9f); // value arc outside slider bounds, bigger than half the width

    Path backgroundArc;
    backgroundArc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(), valueArcOuterRadius, valueArcOuterRadius,
                                0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(enabled ? Colour(63u, 72u, 204u).withMultipliedAlpha(JUCE_LIVE_CONSTANT(0.35f)) : Colours::black);
    g.strokePath(backgroundArc, PathStrokeType(valueLineWidth, PathStrokeType::curved));

    Path valueArc;
    valueArc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(), valueArcOuterRadius, valueArcOuterRadius,
                           0.0f, rotaryStartAngle, endValueAngle, true);
    g.setColour(enabled ? Colour(250u, 250u, 250u) : Colours::grey);
    g.strokePath(valueArc, PathStrokeType(valueLineWidth, PathStrokeType::curved));

    
    g.setColour(enabled ? Colour(250u, 250u, 250u) : Colour(Colours::black));
    g.drawEllipse(bounds.reduced(JUCE_LIVE_CONSTANT(15.f)), 2.5f); // white border for all the sliders

    if(auto* rswl = dynamic_cast<RotarySliderWithLabels*>(&slider))
    {
        auto center = bounds.getCentre();
        Path p;

        Rectangle<float> r;
        r.setLeft(center.getX() - 3.5f);
        r.setRight(center.getX() + 3.5f);
        r.setTop((bounds.getY() + yOffset) * JUCE_LIVE_CONSTANT(1.2f)); // adjust to set the value line to fit inside reduced bounds
        r.setBottom(center.getY() - rswl->getTextHeight() * 1.5f);

        p.addRoundedRectangle(r, 2.f);
        jassert(rotaryStartAngle < rotaryEndAngle);

        auto sliderAngRad = jmap(currentValue, 0.f, 1.f, rotaryStartAngle, rotaryEndAngle);

        p.applyTransform(AffineTransform().rotated(sliderAngRad, center.getX(), center.getY()));

        g.fillPath(p);

        g.setFont(rswl->getTextHeight());
        auto text = rswl->getDisplayString();
        juce::TextLayout layout;
        layout.createLayout(juce::AttributedString(text), 200.f);
        auto strWidth = layout.getWidth();
        
        r.setSize(strWidth + 30.f, (rswl->getTextHeight() + 8.75f));
        r.setCentre(center);

        g.setColour(enabled ? Colours::black : Colours::white);
        g.drawRoundedRectangle(r, 12,1);
        g.fillRoundedRectangle(r, 12);
        //g.fillRect(r);

        g.setColour(enabled ? Colours::white : Colours::black);
        const juce::Typeface::Ptr font = juce::Typeface::createSystemTypefaceFor(BinaryData::Orbitron_ttf, BinaryData::Orbitron_ttfSize);
        float labelFontSize = slider.getProperties()["labelFontSize"]; // use the private member vars of each slider class
        juce::FontOptions fontOptions = juce::FontOptions(font).withHeight(labelFontSize).withStyle("plain");
        g.setFont(juce::Font(fontOptions)); // slider labels
        g.drawFittedText(text, r.toNearestInt(), Justification::centred, 1);
    }
}

juce::ColourGradient RotaryLookAndFeel::getSliderGradient(int width, int height, bool enabled) const
{
    if (enabled)
    {
    return juce::ColourGradient(
    juce::Colour(43u, 52u, 177u), 0.175f * width, 0.175f * height,
    juce::Colour(10u, 12u, 68u), 0.75f * width, 0.75f * height, true);
    }
    else
    {
    return juce::ColourGradient(
    juce::Colour(juce::Colours::lightgrey), 0.175f * width, 0.175f * height,
    juce::Colour(juce::Colours::darkgrey), 0.75f * width, 0.75f * height, true);
    }
}

void RotaryLookAndFeel::drawToggleButton(juce::Graphics &g,
                        juce::ToggleButton &toggleButton, 
                        [[maybe_unused]] bool shouldDrawButtonAsHighlighted, 
                        [[maybe_unused]] bool shouldDrawButtonAsDown)
{
    using namespace juce;
    Path powerButton, shadowPath;

    auto bounds = toggleButton.getLocalBounds();
    // g.setColour(Colours::yellow);
    // g.drawRect(bounds);
    
    bool scale = bounds.getWidth() > bounds.getHeight();
    auto size = scale ?  bounds.getHeight() * 0.2f : bounds.getHeight() * JUCE_LIVE_CONSTANT(0.2f); // toggle button size
    auto r = bounds.withSizeKeepingCentre(static_cast<int>(size), static_cast<int>(size)).toFloat();

    float ang = 25.f;
    size -= 7;;

    powerButton.addCentredArc(r.getCentreX(), r.getCentreY(), size * 0.5f, size * 0.5f, 0.f, degreesToRadians(ang), degreesToRadians(360.f - ang), true);
    powerButton.startNewSubPath(r.getCentreX(), r.getY());
    powerButton.lineTo(r.getCentre());

    PathStrokeType pst(2.f, PathStrokeType::JointStyle::curved);

    // auto shadowRadius = size * 0.5f + (size * 0.1f); // make a drop shadow that's slightly larger than the button
    // auto shadowTop = r.getCentre() - juce::Point<float>(shadowRadius, shadowRadius);
    // shadowPath.addEllipse(shadowTop.x, shadowTop.y, shadowRadius * 2, shadowRadius * 2);
    // if (auto *eb = dynamic_cast<EnableButton*>(&toggleButton))  // if you can cast one of the widgets to this class, do this (dynamic_cast)
    // {
    //     DropShadow shadow(Colour(30u, 20u, 75u).withAlpha(eb->getAlpha()), 10, Point<int>(0, 0));
    //     shadow.drawForPath(g, shadowPath);
    // }

    float alpha = (dynamic_cast<EnableButton*>(&toggleButton))->getAlpha();
    Colour blendedColour = Colours::dimgrey.interpolatedWith(Colour(63u, 72u, 204u), alpha);

    g.setColour(blendedColour);
    g.strokePath(powerButton, pst);
    g.drawEllipse(r, 2);
}

void RotarySliderWithLabels::paint(juce::Graphics &g)
{
    using namespace juce;

    auto startAng = degreesToRadians(180.f + 45.f);
    auto endAng = degreesToRadians(180.f - 45.f) + MathConstants<float>::twoPi;

    auto range = getRange();

    auto sliderBounds = getSliderBounds();

    // g.setColour(Colours::red);
    // g.drawRect(getLocalBounds());
    // g.setColour(Colours::yellow);
    // g.drawRect(sliderBounds);

    getLookAndFeel().drawRotarySlider(g, sliderBounds.getX(), sliderBounds.getY(), sliderBounds.getWidth(), sliderBounds.getHeight(), 
    static_cast<float>(jmap(getValue(), range.getStart(), range.getEnd(), 0.0, 1.0)), startAng, endAng, *this);
}

juce::Rectangle<int> RotarySliderWithLabels::getSliderBounds() const
{
    auto bounds = getLocalBounds();

    auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());

    size -= static_cast<int>(getTextHeight() * 2);

    juce::Rectangle<int> r;
    r.setSize(size, size);
    r.setCentre(static_cast<int>(bounds.getCentreX()), 0);
    r.setY(2);

    return r;
}

juce::String RotarySliderWithLabels::getDisplayString() const
{
    //return juce::String(getValue());
    if(auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(param))
        return choiceParam->getCurrentChoiceName();

    juce::String str;

    if(auto* intParam = dynamic_cast<juce::AudioParameterInt*>(param))
    {
        auto val = getValue();
        str = juce::String(val, 0);
    }
    else if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
    {
        float val = static_cast<float>(getValue());
        str = juce::String(val, 0);
    }
    else
    {
        jassertfalse;
    }

    if(suffix.isNotEmpty())
    {
        str << " ";
        str << suffix;
    }

    return str;
}

//==============================================================================
DelayAudioProcessorEditor::DelayAudioProcessorEditor (DelayAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
    delayTimeSliderLeft(*audioProcessor.apvts.getParameter("Delay Left"), "ms"),
    delayTimeSliderAttachmentLeft(audioProcessor.apvts, "Delay Left", delayTimeSliderLeft),
    delayTimeSliderRight(*audioProcessor.apvts.getParameter("Delay Right"), "ms", audioProcessor.apvts, "Dual Delay", 15.5f),
    delayTimeSliderAttachmentRight(audioProcessor.apvts, "Delay Right", delayTimeSliderRight),
    feedbackSlider(*audioProcessor.apvts.getParameter("Feedback"), ""),
    feedbackSliderAttachment(audioProcessor.apvts, "Feedback", feedbackSlider),
    dryWetSlider(*audioProcessor.apvts.getParameter("Dry Wet"), ""),
    dryWetSliderAttachment(audioProcessor.apvts, "Dry Wet", dryWetSlider),
    lowPassSlider(*audioProcessor.apvts.getParameter("Low Pass Freq"), "", audioProcessor.apvts, "Low Pass"),
    lowPassSliderAttachement(audioProcessor.apvts, "Low Pass Freq", lowPassSlider),
    highPassSlider(*audioProcessor.apvts.getParameter("High Pass Freq"), "", audioProcessor.apvts, "High Pass"),
    highPassSliderAttachement(audioProcessor.apvts, "High Pass Freq", highPassSlider),
    reverbSlider(*audioProcessor.apvts.getParameter("Reverb Level"), "", audioProcessor.apvts, "Reverb"),
    reverbSliderAttachement(audioProcessor.apvts, "Reverb Level", reverbSlider),
    chorusSlider(*audioProcessor.apvts.getParameter("Chorus Rate"), "", audioProcessor.apvts, "Chorus"),
    chorusSliderAttachement(audioProcessor.apvts, "Chorus Rate", chorusSlider)
{

    bpmLabel.setText("120 BPM", juce::dontSendNotification);     //bpmLabel.onClick = [this] { this->updateBPMLabel(); };

    bool dualDelayToggled = audioProcessor.apvts.getRawParameterValue("Dual Delay")->load() > 0.5f;
    setSliderState(dualDelayToggled, delayTimeSliderRight);

    bool lowPass = audioProcessor.apvts.getRawParameterValue("Low Pass")->load() > 0.5f;
    setSliderState(lowPass, lowPassSlider);

    bool highPass = audioProcessor.apvts.getRawParameterValue("High Pass")->load() > 0.5f;
    setSliderState(highPass, highPassSlider);

    bool chorus = audioProcessor.apvts.getRawParameterValue("Chorus")->load() > 0.5f;
    setSliderState(chorus, chorusSlider);

    bool reverb = audioProcessor.apvts.getRawParameterValue("Reverb")->load() > 0.5f;
    setSliderState(reverb, reverbSlider);

    for( auto* comp: getComps())
    {
        addAndMakeVisible(comp);
    }

    int width = audioProcessor.getAppProperties().getUserSettings()->getIntValue("WindowWidth", 1100);
    int height = audioProcessor.getAppProperties().getUserSettings()->getIntValue("WindowHeight", 575);

    setSize(width, height);
    setResizable(true,true);
    auto& displays = juce::Desktop::getInstance().getDisplays();
    auto displayOpt = displays.getPrimaryDisplay();

    if (displayOpt) { // Ensure we have a display
        auto& r = *displayOpt;
        int x = r.totalArea.getWidth(); // Use totalArea or userArea depending on your needs
        int y = r.totalArea.getHeight();
        setResizeLimits(800, 550, x, y);
    }

    startTimer(60); // to update bpm
}

DelayAudioProcessorEditor::~DelayAudioProcessorEditor()
{
}

//==============================================================================
void DelayAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setGradientFill(ColourGradient(Colour(23, 0, 62), 0.125f * (float)getWidth(), 0.125f * (float)getHeight(),
                                     Colour(0, 0, 0), 0.875f * (float)getWidth(), 0.875f * (float)getHeight(), true));
    g.fillAll();
    
    g.setColour(juce::Colours::white);
    juce::FontOptions fontOptions = juce::FontOptions(typeface).withHeight(15.5f).withStyle("plain");
    g.setFont(juce::Font(fontOptions)); // slider labels

    juce::Rectangle<int> delayTimeSliderLeftBounds = delayTimeSliderLeft.getBounds();
    juce::Rectangle<int> delayTimeSliderRightBounds = delayTimeSliderRight.getBounds();
    juce::Rectangle<int> feedbackSliderBounds = feedbackSlider.getBounds();
    juce::Rectangle<int> dryWetBounds = dryWetSlider.getBounds();
    juce::Rectangle<int> chorusLabelBounds = chorusSlider.getSliderBounds();
    juce::Rectangle<int> lowPassLabelBounds = lowPassSlider.getSliderBounds();
    juce::Rectangle<int> highPassLabelBounds = highPassSlider.getSliderBounds();
    juce::Rectangle<int> reverbLabelBounds = reverbSlider.getSliderBounds();

    // g.setColour(juce::Colours::red);
    // g.drawRect(dryWetBounds); // just used for drawing bbox rects for ui layout

    float windowHeight = static_cast<float>(getHeight());

    delayTimeSliderLeftBounds.setY(static_cast<int>(delayTimeSliderLeftBounds.getY() + windowHeight * JUCE_LIVE_CONSTANT(-0.275f)));
    delayTimeSliderRightBounds.setY(static_cast<int>(delayTimeSliderRightBounds.getY() + windowHeight * JUCE_LIVE_CONSTANT(-0.275f)));
    delayTimeSliderLeftBounds.setX(static_cast<int>(delayTimeSliderLeftBounds.getX()));
    delayTimeSliderRightBounds.setX(static_cast<int>(delayTimeSliderRightBounds.getX()));
    feedbackSliderBounds.setY(static_cast<int>(feedbackSliderBounds.getBottom() + windowHeight * JUCE_LIVE_CONSTANT(-0.145f)));
    dryWetBounds.setY(static_cast<int>(dryWetBounds.getBottom() + windowHeight * JUCE_LIVE_CONSTANT(-0.145f)));
    feedbackSliderBounds.setX(static_cast<int>(feedbackSlider.getX()));
    dryWetBounds.setX(static_cast<int>(dryWetSlider.getX()));

    chorusLabelBounds.setX(static_cast<int>(chorusSlider.getBounds().getCentreX() - (chorusLabelBounds.getWidth() / 2.f)));
    chorusLabelBounds.setY(static_cast<int>(chorusSlider.getBounds().getY() + (chorusLabelBounds.getHeight() / 2.f) + JUCE_LIVE_CONSTANT(20.f)));
    lowPassLabelBounds.setX(static_cast<int>(lowPassSlider.getBounds().getCentreX() - (lowPassLabelBounds.getWidth() / 2.f)));
    lowPassLabelBounds.setY(static_cast<int>(lowPassSlider.getBounds().getY() + (lowPassLabelBounds.getHeight() / 2.f) + JUCE_LIVE_CONSTANT(20.f)));
    highPassLabelBounds.setX(static_cast<int>(highPassSlider.getBounds().getCentreX() - (highPassLabelBounds.getWidth() / 2.f)));
    highPassLabelBounds.setY(static_cast<int>(highPassSlider.getBounds().getY() + (highPassLabelBounds.getHeight() / 2.f) + JUCE_LIVE_CONSTANT(20.f)));
    reverbLabelBounds.setX(static_cast<int>(reverbSlider.getBounds().getCentreX() - (reverbLabelBounds.getWidth() / 2.f)));
    reverbLabelBounds.setY(static_cast<int>(reverbSlider.getBounds().getY() + (reverbLabelBounds.getHeight() / 2.f) + JUCE_LIVE_CONSTANT(20.f)));

    g.drawFittedText("Delay Time Left", delayTimeSliderLeftBounds, juce::Justification::centred, 1);
    g.drawFittedText("Delay Time Right", delayTimeSliderRightBounds, juce::Justification::centred, 1);
    g.drawFittedText("Feedback", feedbackSliderBounds, juce::Justification::centred, 1);
    g.drawFittedText("Dry / Wet", dryWetBounds, juce::Justification::centred, 1);
    g.drawFittedText("Chorus", chorusLabelBounds, juce::Justification::centred, 1);
    g.drawFittedText("Low Pass", lowPassLabelBounds, juce::Justification::centred, 1);
    g.drawFittedText("High Pass", highPassLabelBounds, juce::Justification::centred, 1);
    g.drawFittedText("Reverb", reverbLabelBounds, juce::Justification::centred, 1);

    // input and output level visualisers
    float visualiserLevels[2] =
    {
        audioProcessor.getInputSignalLevel(),
        audioProcessor.getOutputSignalLevel()
    };
    
    for (auto& level : visualiserLevels)
    {
        level = juce::jlimit(0.0f, 1.0f, level);
    }

    int visualiserPosX[2] =
    {
        static_cast<int>(getWidth() - (getWidth() * JUCE_LIVE_CONSTANT(0.98f))),
        static_cast<int>(getWidth() * JUCE_LIVE_CONSTANT(0.9633f))
    };

    int visualiserPosY = 100;
    int visualiserWidth = 20;
    int visualiserHeight = getHeight() - 200;
    int segmentGap = 5;
    const int numSegments = 40;
    float segmentHeight = (visualiserHeight - (numSegments - 1) * segmentGap) / (float)numSegments;

    for (int j = 0; j < 2; ++j)
    {
        // g.setColour(juce::Colours::darkgrey.withAlpha(0.5f));        // meter background
        // g.fillRect(visualiserPosX[j], visualiserPosY, visualiserWidth, visualiserHeight);

        int bottom = visualiserPosY + visualiserHeight;
        juce::Rectangle<int> labelBounds((visualiserPosX[j] + visualiserWidth / 2) - 30 / 2, bottom + 10, 30, 20);

        for (int i = 0; i < numSegments; ++i)
        {
            int segmentTop = visualiserPosY + static_cast<int>((segmentHeight + segmentGap) * i);
            int segmentBottom = visualiserPosY + visualiserHeight - (segmentTop - visualiserPosY) - static_cast<int>(segmentHeight);

            if (visualiserLevels[j] * numSegments > i || (i == 0 && visualiserLevels[j] > 1.0f / numSegments))
            {
                g.setColour(juce::Colours::white.withAlpha(0.8f));      // active
                g.fillRoundedRectangle(static_cast<float>(visualiserPosX[j]), static_cast<float>(segmentBottom), static_cast<float>(visualiserWidth), static_cast<float>(segmentHeight), 3.0f); // 3.0f == rounded corners
            }
            else 
            {
                g.setColour(juce::Colours::white.withAlpha(0.2f));      // inactive
                g.fillRoundedRectangle(static_cast<float>(visualiserPosX[j]), static_cast<float>(segmentBottom), static_cast<float>(visualiserWidth), static_cast<float>(segmentHeight), 3.0f); // 3.0f == rounded corners
            }
        }
        g.setColour(juce::Colours::white);
        g.drawFittedText(j == 0 ? "In" : "Out", labelBounds, juce::Justification::centred, 1);
        // // drop shadow
        // int activeHeight = static_cast<int>(visualiserLevels[j] * numSegments * (segmentHeight + segmentGap));
        // if (activeHeight > 0) activeHeight -= static_cast<int>(segmentGap);
        // int activeY = visualiserPosY + visualiserHeight - activeHeight;
        // if (activeHeight > 0)
        // {
        //     juce::DropShadow shadow(juce::Colour(43u, 52u, 177u).withAlpha(0.5f), 25, juce::Point<int>(0, -5));
        //     shadow.drawForRectangle(g, juce::Rectangle<int>(visualiserPosX[j], activeY, visualiserWidth, activeHeight));
        // }
    }
}

void DelayAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(JUCE_LIVE_CONSTANT(50));
    auto delayArea = bounds.removeFromRight(bounds.getWidth());
    delayArea.removeFromTop(static_cast<int>(std::round(delayArea.getHeight() * JUCE_LIVE_CONSTANT(0.1f))));
    auto feedbackArea = delayArea.removeFromBottom(static_cast<int>(std::round(delayArea.getHeight() * JUCE_LIVE_CONSTANT(0.4f))));
    feedbackSlider.setBounds(feedbackArea.removeFromLeft(static_cast<int>(std::round(feedbackArea.getWidth() * JUCE_LIVE_CONSTANT(0.4f)))));
    dryWetSlider.setBounds(feedbackArea.removeFromRight(static_cast<int>(std::round(feedbackArea.getWidth() * JUCE_LIVE_CONSTANT(0.7f)))));

    float windowHeight = static_cast<float>(getHeight());
    float windowWidth = static_cast<float>(getWidth());
    float toggleButtonHeight = windowHeight * JUCE_LIVE_CONSTANT(0.24f);
    float toggleButtonWidth = windowWidth * JUCE_LIVE_CONSTANT(0.15f);

    if (windowHeight >= windowWidth) //== shitty responsiveness attempt
    {
        delayTimeSliderLeft.setBounds(delayArea.removeFromLeft(static_cast<int>(std::round(delayArea.getWidth() * JUCE_LIVE_CONSTANT(0.38f)))));
        delayTimeSliderRight.setBounds(delayArea.removeFromRight(static_cast<int>(std::round(delayArea.getWidth() * JUCE_LIVE_CONSTANT(0.61f)))));
        float lowPassButtonX = windowWidth * JUCE_LIVE_CONSTANT(0.42f);
        float highPassButtonX = windowWidth * JUCE_LIVE_CONSTANT(0.58f);
        float lowPassButtonY = windowHeight * JUCE_LIVE_CONSTANT(0.375f);
        float highPassButtonY = windowHeight * JUCE_LIVE_CONSTANT(0.375f);

        lowPassSlider.setBounds(static_cast<int>(lowPassButtonX - toggleButtonWidth * 0.5f),static_cast<int>(lowPassButtonY), static_cast<int>(toggleButtonWidth), static_cast<int>(toggleButtonHeight));
        float lowPassHeight = static_cast<float>(lowPassSlider.getSliderBounds().getHeight());
        lowPassSlider.setBounds(lowPassSlider.getBounds().getX(), lowPassSlider.getBounds().getY(), lowPassSlider.getBounds().getWidth(), static_cast<int>(lowPassHeight * 1.5));

        highPassSlider.setBounds(static_cast<int>(highPassButtonX - toggleButtonWidth * 0.5f), static_cast<int>(highPassButtonY), static_cast<int>(toggleButtonWidth), static_cast<int>(toggleButtonHeight));
        highPassSlider.setBounds(highPassSlider.getBounds().getX(), highPassSlider.getBounds().getY(), highPassSlider.getBounds().getWidth(), static_cast<int>(lowPassHeight * 1.5));
    }
    else
    {
        delayTimeSliderLeft.setBounds(delayArea.removeFromLeft(static_cast<int>(delayArea.getWidth() * JUCE_LIVE_CONSTANT(0.3f))));
        delayTimeSliderRight.setBounds(delayArea.removeFromRight(static_cast<int>(delayArea.getWidth() * JUCE_LIVE_CONSTANT(0.43f))));
        float lowPassButtonX = windowWidth * JUCE_LIVE_CONSTANT(0.4f);
        float highPassButtonX = windowWidth * JUCE_LIVE_CONSTANT(0.6f);
        float lowPassButtonY = windowHeight * JUCE_LIVE_CONSTANT(0.3f);
        float highPassButtonY = windowHeight * JUCE_LIVE_CONSTANT(0.3f);

        lowPassSlider.setBounds(static_cast<int>(lowPassButtonX - toggleButtonWidth * 0.5f), static_cast<int>(lowPassButtonY), static_cast<int>(toggleButtonWidth), static_cast<int>(toggleButtonHeight));
        float lowPassHeight = static_cast<float>(lowPassSlider.getSliderBounds().getHeight());
        lowPassSlider.setBounds(lowPassSlider.getBounds().getX(), lowPassSlider.getBounds().getY(), lowPassSlider.getBounds().getWidth(), static_cast<int>(lowPassHeight * 1.17));

        highPassSlider.setBounds(static_cast<int>(highPassButtonX - toggleButtonWidth * 0.5f), static_cast<int>(highPassButtonY), static_cast<int>(toggleButtonWidth), static_cast<int>(toggleButtonHeight));
        highPassSlider.setBounds(highPassSlider.getBounds().getX(), highPassSlider.getBounds().getY(), highPassSlider.getBounds().getWidth(), static_cast<int>(lowPassHeight * 1.17));
    }

    float bpmY = windowHeight * JUCE_LIVE_CONSTANT(1.3f);
    bpmLabel.setBounds(static_cast<int>((windowWidth * 0.5f) - 25.f), static_cast<int>(bpmY * 0.15f), 100, 30);

    float chorusButtonX = windowWidth * JUCE_LIVE_CONSTANT(0.42f);
    float reverbButtonX = windowWidth * JUCE_LIVE_CONSTANT(0.58f);
    float chorusButtonY = windowHeight * JUCE_LIVE_CONSTANT(0.555f);
    float reverbButtonY = windowHeight * JUCE_LIVE_CONSTANT(0.555f);

    chorusSlider.setBounds(static_cast<int>(chorusButtonX - toggleButtonWidth * 0.5f), static_cast<int>(chorusButtonY), static_cast<int>(toggleButtonWidth), static_cast<int>(toggleButtonHeight));
    float chorusSliderHeight = static_cast<float>(chorusSlider.getSliderBounds().getHeight());
    chorusSlider.setBounds(chorusSlider.getBounds().getX(), chorusSlider.getBounds().getY(), chorusSlider.getBounds().getWidth(), static_cast<int>(chorusSliderHeight * 1.17));

    reverbSlider.setBounds(static_cast<int>(reverbButtonX - toggleButtonWidth * 0.5f), static_cast<int>(reverbButtonY), static_cast<int>(toggleButtonWidth), static_cast<int>(toggleButtonHeight));
    float reverbSliderHeight = static_cast<float>(reverbSlider.getSliderBounds().getHeight());
    reverbSlider.setBounds(reverbSlider.getBounds().getX(), reverbSlider.getBounds().getY(), reverbSlider.getBounds().getWidth(), static_cast<int>(reverbSliderHeight * 1.17));

    int width = getWidth();
    int height = getHeight();
    audioProcessor.getAppProperties().getUserSettings()->setValue("WindowWidth", width);
    audioProcessor.getAppProperties().getUserSettings()->setValue("WindowHeight", height);
}

void DelayAudioProcessorEditor::setSliderState(bool state, RotarySliderWithLabels &slider)
{
    slider.setSliderEnabled(state);
    slider.animateColor();
    slider.setDoubleClickReturnValue(false, 0);
}

void DelayAudioProcessorEditor::updateBPMLabel()
{
    float currentBPM = audioProcessor.getCurrentBPM();
    if (currentBPM != lastBPM)
    {
        lastBPM = currentBPM;
        bpmLabel.setText(juce::String(currentBPM) + " BPM", juce::dontSendNotification);
    }
}

std::vector<juce::Component*> DelayAudioProcessorEditor::getComps()
{
  return
  {
    &delayTimeSliderLeft,
    &delayTimeSliderRight,
    &feedbackSlider,
    &dryWetSlider,
    &chorusSlider,
    &lowPassSlider,
    &highPassSlider,
    &reverbSlider,
    &bpmLabel
  };
}