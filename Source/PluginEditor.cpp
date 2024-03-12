#include "PluginProcessor.h"
#include "PluginEditor.h"

void RotaryLookAndFeel::drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height, float currentValue,
float rotaryStartAngle, float rotaryEndAngle, juce::Slider &slider)
{
    using namespace juce;

    auto yOffset = JUCE_LIVE_CONSTANT(10);
    auto bounds = Rectangle<float>(x, y + yOffset, width, height);

    auto enabled = slider.isEnabled();

    auto enabledGradient = getSliderGradient(width, height, true);
    auto disabledGradient = getSliderGradient(width, height, false);
    float alpha = dynamic_cast<RotarySliderWithLabels*>(&slider) ? dynamic_cast<RotarySliderWithLabels*>(&slider)->getAlpha() : 1.0f;

    ColourGradient currentGradient;
    for (int i = 0; i < enabledGradient.getNumColours(); ++i)
    {
        Colour col = enabledGradient.getColour(i).interpolatedWith(disabledGradient.getColour(i), 1.0f - alpha);
        currentGradient.addColour(enabledGradient.getColourPosition(i), col);
    }

    g.setGradientFill(currentGradient);
    g.fillEllipse(bounds.reduced(JUCE_LIVE_CONSTANT(15)));

    auto endValueAngle = rotaryStartAngle + currentValue * (rotaryEndAngle - rotaryStartAngle); // set the max angle of the value arc to the current value
    auto valueLineWidth = jmin(4.0f, width * 0.05f);
    auto valueArcOuterRadius = (width * 0.55f + valueLineWidth) * JUCE_LIVE_CONSTANT(0.9); // value arc outside slider bounds, bigger than half the width

    Path backgroundArc;
    backgroundArc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(), valueArcOuterRadius, valueArcOuterRadius,
                                0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(Colour(63u, 72u, 204u).withMultipliedAlpha(JUCE_LIVE_CONSTANT(0.35)));
    g.strokePath(backgroundArc, PathStrokeType(valueLineWidth, PathStrokeType::curved));

    Path valueArc;
    valueArc.addCentredArc(bounds.getCentreX(), bounds.getCentreY(), valueArcOuterRadius, valueArcOuterRadius,
                           0.0f, rotaryStartAngle, endValueAngle, true);
    g.setColour(enabled ? Colour(250u, 250u, 250u) : Colours::grey);
    g.strokePath(valueArc, PathStrokeType(valueLineWidth, PathStrokeType::curved));

    
    g.setColour(enabled ? Colour(250u, 250u, 250u) : Colour(Colours::black));
    g.drawEllipse(bounds.reduced(JUCE_LIVE_CONSTANT(15)), 2.5f); // white border for all the sliders

    if(auto* rswl = dynamic_cast<RotarySliderWithLabels*>(&slider))
    {
        auto center = bounds.getCentre();
        Path p;

        Rectangle<float> r;
        r.setLeft(center.getX() - 3.5f);
        r.setRight(center.getX() + 3.5f);
        r.setTop((bounds.getY() + yOffset) * JUCE_LIVE_CONSTANT(1.2)); // adjust to set the value line to fit inside reduced bounds
        r.setBottom(center.getY() - rswl->getTextHeight() * 1.5);

        p.addRoundedRectangle(r, 2.f);
        jassert(rotaryStartAngle < rotaryEndAngle);

        auto sliderAngRad = jmap(currentValue, 0.f, 1.f, rotaryStartAngle, rotaryEndAngle);

        p.applyTransform(AffineTransform().rotated(sliderAngRad, center.getX(), center.getY()));

        g.fillPath(p);

        g.setFont(rswl->getTextHeight());
        auto text = rswl->getDisplayString();
        auto strWidth = g.getCurrentFont().getStringWidth(text);
        
        r.setSize(strWidth + 22, rswl->getTextHeight() + 10);
        r.setCentre(center);

        g.setColour(enabled ? Colours::black : Colours::white);
        g.drawRoundedRectangle(r, 12,1);
        g.fillRoundedRectangle(r, 12);
        //g.fillRect(r);

        g.setColour(enabled ? Colours::white : Colours::black);
        const juce::Typeface::Ptr typeface = juce::Typeface::createSystemTypefaceFor(BinaryData::Orbitron_ttf, BinaryData::Orbitron_ttfSize);
        g.setFont(juce::Font(typeface).withHeight(15.5f)); // slider labels
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
                        bool shouldDrawButtonAsHighlighted, 
                        bool shouldDrawButtonAsDown)
{
    using namespace juce;
    Path powerButton, shadowPath;

    auto bounds = toggleButton.getLocalBounds();
    // g.setColour(Colours::yellow);
    // g.drawRect(bounds);
    
    bool scale = bounds.getWidth() > bounds.getHeight();
    auto size = scale ?  bounds.getHeight() * 0.5f : bounds.getHeight() * JUCE_LIVE_CONSTANT(0.5f); // toggle button size
    auto r = bounds.withSizeKeepingCentre(size, size).toFloat();

    float ang = 25.f;
    size -= 7;;

    powerButton.addCentredArc(r.getCentreX(), r.getCentreY(), size * 0.5, size * 0.5, 0.f, degreesToRadians(ang), degreesToRadians(360.f - ang), true);
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
    jmap(getValue(), range.getStart(), range.getEnd(), 0.0, 1.0), startAng, endAng, *this);
}

juce::Rectangle<int> RotarySliderWithLabels::getSliderBounds() const
{
    auto bounds = getLocalBounds();

    auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());

    size -= getTextHeight() * 2;

    juce::Rectangle<int> r;
    r.setSize(size, size);
    r.setCentre(bounds.getCentreX(), 0);
    r.setY(2);

    return r;
}

juce::String RotarySliderWithLabels::getDisplayString() const
{
    //return juce::String(getValue());
    if(auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(param))
        return choiceParam->getCurrentChoiceName();

    juce::String str;

    if(auto* floatParam = dynamic_cast<juce::AudioParameterInt*>(param))
    {
        float val = getValue();
        str = juce::String(val, 0);
    }
    else if (auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
    {
        float val = getValue();
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
    delayTimeSliderRight(*audioProcessor.apvts.getParameter("Delay Right"), "ms"),
    delayTimeSliderAttachmentRight(audioProcessor.apvts, "Delay Right", delayTimeSliderRight),
    feedbackSlider(*audioProcessor.apvts.getParameter("Feedback"), ""),
    feedbackSliderAttachment(audioProcessor.apvts, "Feedback", feedbackSlider),
    dryWetSlider(*audioProcessor.apvts.getParameter("Dry Wet"), ""),
    dryWetSliderAttachment(audioProcessor.apvts, "Dry Wet", dryWetSlider),

    dualDelayButtonAttachment(audioProcessor.apvts, "Dual Delay", dualDelayButton),
    chorusButtonAttachment(audioProcessor.apvts, "Chorus", chorusButton),
    lowPassButtonAttachment(audioProcessor.apvts, "Low Pass", lowPassButton),
    highPassButtonAttachment(audioProcessor.apvts, "High Pass", highPassButton),
    reverbButtonAttachment(audioProcessor.apvts, "Reverb", reverbButton)
{

    delayTimeSliderLeft.setTextValueSuffix(" (ms)");
    delayTimeSliderRight.setTextValueSuffix(" (ms)");

    bpmLabel.setText("120 BPM", juce::dontSendNotification);     //bpmLabel.onClick = [this] { this->updateBPMLabel(); };

    bool dualDelayToggled = dualDelayButton.getToggleState(); // making sure its state and paint is correct on loading GUI outside of onClick event of togglebutton
    delayTimeSliderRight.setEnabled(dualDelayToggled);
    delayTimeSliderRight.animateColor();

    auto safePtr = juce::Component::SafePointer<DelayAudioProcessorEditor>(this);
    dualDelayButton.onClick = [safePtr]()
    {
        if (auto *comp = safePtr.getComponent())
        {
            auto dualDelayState = comp->dualDelayButton.getToggleState();
            
            comp->delayTimeSliderRight.setEnabled(dualDelayState);
            comp->delayTimeSliderRight.animateColor();
            comp->dualDelayButton.animateColor();
        }
    };

    for( auto* comp: getComps())
    {
        addAndMakeVisible(comp);
    }

    dualDelayButton.setLookAndFeel(&lnf);
    chorusButton.setLookAndFeel(&lnf);
    lowPassButton.setLookAndFeel(&lnf);
    highPassButton.setLookAndFeel(&lnf);
    reverbButton.setLookAndFeel(&lnf);

    int width = audioProcessor.getAppProperties().getUserSettings()->getIntValue("WindowWidth", 1100);
    int height = audioProcessor.getAppProperties().getUserSettings()->getIntValue("WindowHeight", 575);

    setSize(width, height);
    setResizable(true,true);
    juce::Rectangle<int> r = Desktop::getInstance().getDisplays().getMainDisplay().userArea;
    int x = r.getWidth();
    int y = r.getHeight();
    setResizeLimits(475, 350, x, y);

    startTimer(500); // to update bpm
}

DelayAudioProcessorEditor::~DelayAudioProcessorEditor()
{
    dualDelayButton.setLookAndFeel(nullptr);
    chorusButton.setLookAndFeel(nullptr);
    lowPassButton.setLookAndFeel(nullptr);
    highPassButton.setLookAndFeel(nullptr);
    reverbButton.setLookAndFeel(nullptr);
}

//==============================================================================
void DelayAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setGradientFill(ColourGradient(Colour(23, 0, 62), 0.125f * (float)getWidth(), 0.125f * (float)getHeight(),
                                     Colour(0, 0, 0), 0.875f * (float)getWidth(), 0.875f * (float)getHeight(), true));
    g.fillAll();
    
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(typeface).withHeight(15.5f)); // slider labels

    juce::Rectangle<int> delayTimeSliderLeftBounds = delayTimeSliderLeft.getBounds();
    juce::Rectangle<int> delayTimeSliderRightBounds = delayTimeSliderRight.getBounds();
    juce::Rectangle<int> feedbackSliderBounds = feedbackSlider.getBounds();
    juce::Rectangle<int> dryWetBounds = dryWetSlider.getBounds();
    juce::Rectangle<int> delayToggleButtonBounds = dualDelayButton.getBounds();
    juce::Rectangle<int> chorusToggleButtonBounds = chorusButton.getBounds();
    juce::Rectangle<int> lowPassButtonBounds = lowPassButton.getBounds();
    juce::Rectangle<int> highPassButtonBounds = highPassButton.getBounds();
    juce::Rectangle<int> reverbButtonBounds = reverbButton.getBounds();

    // g.setColour(juce::Colours::red);
    // g.drawRect(delayToggleButtonBounds); // just used for drawing bbox rects for ui layout

    float windowHeight = static_cast<float>(getHeight());

    delayTimeSliderLeftBounds.setY(delayTimeSliderLeftBounds.getY() + windowHeight * JUCE_LIVE_CONSTANT(-0.275f));
    delayTimeSliderRightBounds.setY(delayTimeSliderRightBounds.getY() + windowHeight * JUCE_LIVE_CONSTANT(-0.275f));
    delayTimeSliderLeftBounds.setX(delayTimeSliderLeftBounds.getX());
    delayTimeSliderRightBounds.setX(delayTimeSliderRightBounds.getX());
    feedbackSliderBounds.setY(feedbackSliderBounds.getBottom() + windowHeight * JUCE_LIVE_CONSTANT(-0.145f));
    dryWetBounds.setY(dryWetBounds.getBottom() + windowHeight * JUCE_LIVE_CONSTANT(-0.145f));
    feedbackSliderBounds.setX(feedbackSlider.getX());
    dryWetBounds.setX(dryWetSlider.getX());
    delayToggleButtonBounds.setY(delayToggleButtonBounds.getY() + windowHeight * JUCE_LIVE_CONSTANT(-0.1f));
    chorusToggleButtonBounds.setY(chorusToggleButtonBounds.getY() + windowHeight * JUCE_LIVE_CONSTANT(0.1f));
    reverbButtonBounds.setY(reverbButtonBounds.getY() + windowHeight * JUCE_LIVE_CONSTANT(0.1f));
    lowPassButtonBounds.setY(lowPassButtonBounds.getY() + windowHeight * JUCE_LIVE_CONSTANT(0.1f));
    highPassButtonBounds.setY(highPassButtonBounds.getY() + windowHeight * JUCE_LIVE_CONSTANT(0.1f));

    g.drawFittedText("Delay Time Left", delayTimeSliderLeftBounds, juce::Justification::centred, 1);
    g.drawFittedText("Delay Time Right", delayTimeSliderRightBounds, juce::Justification::centred, 1);
    g.drawFittedText("Feedback", feedbackSliderBounds, juce::Justification::centred, 1);
    g.drawFittedText("Dry / Wet", dryWetBounds, juce::Justification::centred, 1);
    g.drawFittedText("Single / Dual", delayToggleButtonBounds, juce::Justification::centred, 1);
    g.drawFittedText("Chorus", chorusToggleButtonBounds, juce::Justification::centred, 1);
    g.drawFittedText("Low Pass", lowPassButtonBounds, juce::Justification::centred, 1);
    g.drawFittedText("High Pass", highPassButtonBounds, juce::Justification::centred, 1);
    g.drawFittedText("Reverb", reverbButtonBounds, juce::Justification::centred, 1);
}

void DelayAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(JUCE_LIVE_CONSTANT(50));

    auto delayArea = bounds.removeFromRight(bounds.getWidth() * JUCE_LIVE_CONSTANT(1.f));
    auto delayAreaTop = delayArea.removeFromTop(delayArea.getHeight() * JUCE_LIVE_CONSTANT(0.1f));
    auto feedbackArea = delayArea.removeFromBottom(delayArea.getHeight() * JUCE_LIVE_CONSTANT(0.4f));
    auto toggleArea = bounds;

    float windowHeight = static_cast<float>(getHeight());
    float windowWidth = static_cast<float>(getWidth());

    delayTimeSliderLeft.setBounds(delayArea.removeFromLeft(delayArea.getWidth() * JUCE_LIVE_CONSTANT(0.33f)));
    delayTimeSliderRight.setBounds(delayArea.removeFromRight(delayArea.getWidth() * JUCE_LIVE_CONSTANT(0.5f)));
    feedbackSlider.setBounds(feedbackArea.removeFromLeft(feedbackArea.getWidth() * JUCE_LIVE_CONSTANT(0.4f)));
    dryWetSlider.setBounds(feedbackArea.removeFromRight(feedbackArea.getWidth() * JUCE_LIVE_CONSTANT(0.7f)));

    float toggleButtonWidth = windowWidth * JUCE_LIVE_CONSTANT(0.15f);
    float toggleButtonHeight = windowHeight * 0.10f;
    float lowPassButtonX = windowWidth * JUCE_LIVE_CONSTANT(0.4f);
    float highPassButtonX = windowWidth * JUCE_LIVE_CONSTANT(0.6f);
    float chorusButtonX = windowWidth * JUCE_LIVE_CONSTANT(0.43f);
    float reverbButtonX = windowWidth * JUCE_LIVE_CONSTANT(0.57f);
    
    float dualButtonY = windowHeight * JUCE_LIVE_CONSTANT(0.25f);
    float lowPassButtonY = windowHeight * JUCE_LIVE_CONSTANT(0.425f);   
    float highPassButtonY = windowHeight * JUCE_LIVE_CONSTANT(0.425f); 
    float chorusButtonY = windowHeight * JUCE_LIVE_CONSTANT(0.675f);
    float reverbButtonY = windowHeight * JUCE_LIVE_CONSTANT(0.675f);

    dualDelayButton.setBounds(windowWidth * 0.5f - toggleButtonWidth * 0.5f, dualButtonY, toggleButtonWidth, toggleButtonHeight);
    lowPassButton.setBounds(lowPassButtonX - toggleButtonWidth * 0.5f, lowPassButtonY, toggleButtonWidth, toggleButtonHeight);
    highPassButton.setBounds(highPassButtonX - toggleButtonWidth * 0.5f, highPassButtonY, toggleButtonWidth, toggleButtonHeight);
    chorusButton.setBounds(chorusButtonX - toggleButtonWidth * 0.5f, chorusButtonY, toggleButtonWidth, toggleButtonHeight);
    reverbButton.setBounds(reverbButtonX - toggleButtonWidth * 0.5f, reverbButtonY, toggleButtonWidth, toggleButtonHeight);
    //chorusButton.setBounds(windowWidth * 0.5f - toggleButtonWidth * 0.5f, chorusButtonY, toggleButtonWidth, toggleButtonHeight);
    bpmLabel.setBounds(dualDelayButton.getBounds().getCentreX() - 25, dualDelayButton.getBounds().getY() * 0.15, 100, 30);

    int width = getWidth();
    int height = getHeight();
    audioProcessor.getAppProperties().getUserSettings()->setValue("WindowWidth", width);
    audioProcessor.getAppProperties().getUserSettings()->setValue("WindowHeight", height);
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
    &dualDelayButton,
    &chorusButton,
    &lowPassButton,
    &highPassButton,
    &reverbButton,
    &bpmLabel
  };
}