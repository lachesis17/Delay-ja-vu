/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

void RotaryLookAndFeel::drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height, float currentValue,
float rotaryStartAngle, float rotaryEndAngle, juce::Slider & slider)
{
    using namespace juce;

    auto bounds = Rectangle<float>(x, y, width, height);
    bounds = bounds * JUCE_LIVE_CONSTANT(0.975); // fix this properly to adjust for window width/height, just reducing it to fit valuearc...

    auto enabled = slider.isEnabled();

    g.setGradientFill(enabled ? 
                    ColourGradient (Colour(Colour(43u, 52u, 177u)), 0.175*(float) width, 0.175*(float) height,
                    Colour(10u, 12u, 68u), 0.75*(float) width, 0.75*(float) height, true) 
                    : 
                    ColourGradient (Colour(Colours::lightgrey), 0.175*(float) width, 0.175*(float) height,
                    Colour(Colours::darkgrey), 0.75*(float) width, 0.75*(float) height, true) );
    g.fillEllipse(bounds.reduced(JUCE_LIVE_CONSTANT(15)));

    auto endValueAngle = rotaryStartAngle + currentValue * (rotaryEndAngle - rotaryStartAngle); // set the max angle of the value arc to the current value
    auto valueLineWidth = jmin(4.0f, width * 0.05f);
    auto valueArcOuterRadius = (width * 0.5f + valueLineWidth) * JUCE_LIVE_CONSTANT(0.9); // value arc outside slider bounds, bigger than half the width

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
        r.setTop(bounds.getY() * JUCE_LIVE_CONSTANT(9)); // adjust to set the value line to fit inside reduced bounds
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

void RotaryLookAndFeel::drawToggleButton(juce::Graphics &g,
                        juce::ToggleButton &toggleButton, 
                        bool shouldDrawButtonAsHighlighted, 
                        bool shouldDrawButtonAsDown)
{
    using namespace juce;

    // if (auto *pb = dynamic_cast<EnableButton*>(&toggleButton)) // if you can cast one of the widgets to this class, do this
    // {
        Path powerButton;

        auto bounds = toggleButton.getLocalBounds();
        // g.setColour(Colours::yellow);
        // g.drawRect(bounds);
        
        bool scale = bounds.getWidth() > bounds.getHeight();
        auto size = scale ?  bounds.getHeight() * JUCE_LIVE_CONSTANT(0.33f) : bounds.getHeight() * JUCE_LIVE_CONSTANT(0.33f); // toggle button size
        auto r = bounds.withSizeKeepingCentre(size, size).toFloat();

        float ang = 25.f;

        size -= 7;

        powerButton.addCentredArc(r.getCentreX(), r.getCentreY(), size * 0.5, size * 0.5, 0.f, degreesToRadians(ang), degreesToRadians(360.f - ang), true);

        powerButton.startNewSubPath(r.getCentreX(), r.getY());
        powerButton.lineTo(r.getCentre());

        PathStrokeType pst(2.f, PathStrokeType::JointStyle::curved);

        auto color = toggleButton.getToggleState() ? Colours::dimgrey : Colour(63u, 72u, 204u);

        g.setColour(color);
        g.strokePath(powerButton, pst);
        g.drawEllipse(r, 2);
    //}
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
    chorusButtonAttachment(audioProcessor.apvts, "Chorus", chorusButton)
{

    delayTimeSliderLeft.setTextValueSuffix(" (ms)");
    delayTimeSliderRight.setTextValueSuffix(" (ms)");

    bool delayToggled = !dualDelayButton.getToggleState(); // making sure its state and paint is correct on loading GUI outside of onClick event of togglebutton
    delayTimeSliderRight.setEnabled(delayToggled);

    auto safePtr = juce::Component::SafePointer<DelayAudioProcessorEditor>(this);
    dualDelayButton.onClick = [safePtr]()
    {
        if (auto *comp = safePtr.getComponent())
        {
            auto bypassed = comp->dualDelayButton.getToggleState();
            
            comp->delayTimeSliderRight.setEnabled(!bypassed);
        }
    };

    for( auto* comp: getComps())
    {
        addAndMakeVisible(comp);
    }

    dualDelayButton.setLookAndFeel(&lnf);
    chorusButton.setLookAndFeel(&lnf);

    int width = audioProcessor.getAppProperties().getUserSettings()->getIntValue("WindowWidth", 700);
    int height = audioProcessor.getAppProperties().getUserSettings()->getIntValue("WindowHeight", 500);

    setSize(width, height);
    setResizable(true,true);
    juce::Rectangle<int> r = Desktop::getInstance().getDisplays().getMainDisplay().userArea;
    int x = r.getWidth();
    int y = r.getHeight();
    setResizeLimits(420, 300, x, y);
}

DelayAudioProcessorEditor::~DelayAudioProcessorEditor()
{
    dualDelayButton.setLookAndFeel(nullptr);
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

    // g.setColour(juce::Colours::red);
    // g.drawRect(delayToggleButtonBounds); // just used for drawing bbox rects for ui layout

    float windowHeight = static_cast<float>(getHeight());

    delayTimeSliderLeftBounds.setY(delayTimeSliderLeftBounds.getY() + windowHeight * JUCE_LIVE_CONSTANT(-0.275f));
    delayTimeSliderRightBounds.setY(delayTimeSliderRightBounds.getY() + windowHeight * JUCE_LIVE_CONSTANT(-0.275f));
    feedbackSliderBounds.setY(feedbackSliderBounds.getBottom() + windowHeight * JUCE_LIVE_CONSTANT(-0.145f));
    dryWetBounds.setY(dryWetBounds.getBottom() + windowHeight * JUCE_LIVE_CONSTANT(-0.145f));
    delayToggleButtonBounds.setY(delayToggleButtonBounds.getY() + windowHeight * JUCE_LIVE_CONSTANT(-0.1f));
    chorusToggleButtonBounds.setY(chorusToggleButtonBounds.getY() + windowHeight * JUCE_LIVE_CONSTANT(0.1f));

    g.drawFittedText("Delay Time Left", delayTimeSliderLeftBounds, juce::Justification::centred, 1);
    g.drawFittedText("Delay Time Right", delayTimeSliderRightBounds, juce::Justification::centred, 1);
    g.drawFittedText("Feedback", feedbackSliderBounds, juce::Justification::centred, 1);
    g.drawFittedText("Dry / Wet", dryWetBounds, juce::Justification::centred, 1);
    g.drawFittedText("Single / Dual", delayToggleButtonBounds, juce::Justification::centred, 1);
    g.drawFittedText("Chorus", chorusToggleButtonBounds, juce::Justification::centred, 1);
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

    toggleArea.setWidth(windowWidth * 0.15);
    toggleArea.setX(getLocalBounds().getCentreX() - toggleArea.getWidth() * 0.5f);
    auto delayToggleArea = toggleArea;
    auto chorusToggleArea = toggleArea;
    delayToggleArea.setHeight(windowHeight * JUCE_LIVE_CONSTANT(0.2f));
    chorusToggleArea.setHeight(windowHeight * JUCE_LIVE_CONSTANT(0.2f));
    delayToggleArea.setY(delayToggleArea.getY() + windowHeight * JUCE_LIVE_CONSTANT(0.1f));
    chorusToggleArea.setY(chorusToggleArea.getY() + windowHeight * JUCE_LIVE_CONSTANT(0.5f));

    delayTimeSliderLeft.setBounds(delayArea.removeFromLeft(delayArea.getWidth() * JUCE_LIVE_CONSTANT(0.33f)));
    delayTimeSliderRight.setBounds(delayArea.removeFromRight(delayArea.getWidth() * JUCE_LIVE_CONSTANT(0.5f)));
    feedbackSlider.setBounds(feedbackArea.removeFromLeft(feedbackArea.getWidth() * JUCE_LIVE_CONSTANT(0.4f)));
    dryWetSlider.setBounds(feedbackArea.removeFromRight(feedbackArea.getWidth() * JUCE_LIVE_CONSTANT(0.7f)));

    dualDelayButton.setBounds(delayToggleArea.removeFromRight(delayToggleArea.getWidth() * JUCE_LIVE_CONSTANT(1.f)));
    chorusButton.setBounds(chorusToggleArea.removeFromRight(chorusToggleArea.getWidth() * JUCE_LIVE_CONSTANT(1.f)));

    int width = getWidth();
    int height = getHeight();
    audioProcessor.getAppProperties().getUserSettings()->setValue("WindowWidth", width);
    audioProcessor.getAppProperties().getUserSettings()->setValue("WindowHeight", height);
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
    &chorusButton
  };
}