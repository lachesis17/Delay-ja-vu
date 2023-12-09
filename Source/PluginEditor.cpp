/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

void RotaryLookAndFeel::drawRotarySlider(juce::Graphics &g, int x, int y, int width, int height, float sliderPosProportional,
float rotaryStartAngle, float rotaryEndAngle, juce::Slider & slider)
{
    using namespace juce;

    auto bounds = Rectangle<float>(x, y, width, height);

    auto enabled = slider.isEnabled();

    g.setGradientFill(enabled ? 
                    ColourGradient (Colour(Colour(43u, 52u, 177u)), 0.175*(float) width, 0.175*(float) height,
                    Colour(10u, 12u, 68u), 0.75*(float) width, 0.75*(float) height, true) 
                    : 
                    ColourGradient (Colour(Colours::lightgrey), 0.175*(float) width, 0.175*(float) height,
                    Colour(Colours::darkgrey), 0.75*(float) width, 0.75*(float) height, true) );
    g.fillEllipse(bounds);

    g.setColour(enabled ? Colour(250u, 250u, 250u) : Colour(Colours::black));
    g.drawEllipse(bounds, 1.f);    

    if(auto* rswl = dynamic_cast<RotarySliderWithLabels*>(&slider))
    {
        auto center = bounds.getCentre();
        Path p;

        Rectangle<float> r;
        r.setLeft(center.getX() - 2.5);
        r.setRight(center.getX() + 2.5);
        r.setTop(bounds.getY());
        r.setBottom(center.getY() - rswl->getTextHeight() * 1.5);

        p.addRoundedRectangle(r, 2.f);
        jassert(rotaryStartAngle < rotaryEndAngle);

        auto sliderAngRad = jmap(sliderPosProportional, 0.f, 1.f, rotaryStartAngle, rotaryEndAngle);

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

    if(auto* floatParam = dynamic_cast<juce::AudioParameterFloat*>(param))
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
BlankAudioProcessorEditor::BlankAudioProcessorEditor (BlankAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p),
    delayTimeSlider(*audioProcessor.apvts.getParameter("Delay Time"), "ms"),
    delayTimeSliderAttachment(audioProcessor.apvts, "Delay Time", delayTimeSlider)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    
    delayTimeSlider.setSliderStyle(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    delayTimeSlider.setTextValueSuffix(" (ms)");
    addAndMakeVisible(delayTimeSlider);
    addAndMakeVisible(delayTimeLabel);
    delayTimeLabel.setFont(juce::Font(typeface).withHeight(15.5f));
    delayTimeLabel.setText("Delay Time", juce::dontSendNotification);
    delayTimeLabel.attachToComponent(&delayTimeSlider, true);
    //delayTimeSliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.apvts, "Delay Time", delayTimeSlider);

    setSize (650, 650);
    setResizable(true,false);
    // juce::Rectangle<int> r = Desktop::getInstance().getDisplays().getMainDisplay().userArea;
    // int x = r.getWidth();
    // int y = r.getHeight();
    // setResizeLimits(50, 0, x, y);
}

BlankAudioProcessorEditor::~BlankAudioProcessorEditor()
{
}

//==============================================================================
void BlankAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)

    g.setGradientFill(ColourGradient (Colour(23, 0, 62), 0.125f*(float) getWidth(), 0.125f*(float) getHeight(),
                                       Colour(0, 0, 0), 0.875f*(float) getWidth(), 0.875f*(float) getHeight(), true));
    g.fillAll();

//     g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

//     g.setColour (juce::Colours::white);
//     g.setFont (15.0f);
//     g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void BlankAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    auto bounds = getLocalBounds();
    bounds.reduced(10);
    bounds.removeFromTop(bounds.getHeight() * 0.5f);

    auto delayArea = bounds.removeFromRight(bounds.getWidth() * 0.5f);

    delayTimeSlider.setBounds(delayArea * 0.67f);
    //delayTimeSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 100, 50);
    //delayTimeSlider.setBounds(getWidth() / 2 - 50, getHeight() / 2 - 50, 500, 75);
}


std::vector<juce::Component*> BlankAudioProcessorEditor::getComps()
{
  return
  {
    &delayTimeSlider
  };
}