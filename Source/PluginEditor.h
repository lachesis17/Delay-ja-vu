#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

struct RotaryLookAndFeel : juce::LookAndFeel_V4
{
  void drawRotarySlider (juce::Graphics&, // from juce::LookAndFeel_V4 class line 206
                      int x, int y, int width, int height,
                      float sliderPosProportional,
                      float rotaryStartAngle,
                      float rotaryEndAngle,
                      juce::Slider&) override;

  void drawToggleButton (juce::Graphics &g, // line 119
                      juce::ToggleButton &toggleButton, 
                      bool shouldDrawButtonAsHighlighted, 
                      bool shouldDrawButtonAsDown) override;

  juce::ColourGradient getSliderGradient(int width, int height, bool enabled) const;

  //Font getLabelFont (Label&) override;
    
private:

const juce::Typeface::Ptr typeface = juce::Typeface::createSystemTypefaceFor(BinaryData::Orbitron_ttf, BinaryData::Orbitron_ttfSize);
    // g.setFont(juce::Font(typeface).withHeight(15.5f)); // slider labels
};

//==============================================================================

struct RotarySliderWithLabels : juce::Slider, juce::Timer
{
  RotarySliderWithLabels(juce::RangedAudioParameter& rap, const juce::String& unitSuffix) : 
  juce::Slider(juce::Slider::SliderStyle::RotaryHorizontalVerticalDrag, juce::Slider::TextEntryBoxPosition::NoTextBox),
  param(&rap),
  suffix(unitSuffix)
  {
    setLookAndFeel(&lnf);
    setComponentProperty("labelFontSize", labelFontSize);
  }

  ~RotarySliderWithLabels() // setLookAndFeel, unset with destructor
  {
    setLookAndFeel(nullptr);
  }

void setSliderEnabled(bool state)
{
  sliderEnabled = state;
}

bool getSliderState() { return sliderEnabled; }

  //=======================================
  float alpha = 1.0f;
  float targetAlpha = 0.0f;
  float animationSpeed = 0.04f;

void animateColor()
  {
    targetAlpha = getSliderState() ? 1.0f : 0.0f;
    startTimerHz(120);
  }

  void timerCallback() override
  {
    alpha = alpha < targetAlpha ? std::min(alpha + animationSpeed, targetAlpha) : alpha = std::max(alpha - animationSpeed, targetAlpha);
    repaint();
    if (alpha == targetAlpha) { stopTimer(); }
  }

  float getAlpha() const { return alpha; }
  //=======================================

  struct LabelPos
  {
    float pos;
    juce::String label;
  };

  juce::Array<LabelPos> labels;

  void paint(juce::Graphics& g) override;
  juce::Rectangle<int> getSliderBounds() const;
  float getTextHeight() const { return labelFontSize; }
  juce::String getDisplayString() const;
private:
  RotaryLookAndFeel lnf; // Calling this "LookAndFeel" throws ambiguous symbol error as could be juce::LookAndFeel
  float labelFontSize = 17.f;
  bool sliderEnabled = true;

  void setComponentProperty(const juce::Identifier& propertyName, float value)
  {
    getProperties().set(propertyName, value);
  }

  juce::RangedAudioParameter* param;
  juce::String suffix;
};

//==============================================================================

struct RotarySliderToggle : RotarySliderWithLabels {
    RotarySliderToggle(juce::RangedAudioParameter& rap, const juce::String& unitSuffix,
                       juce::AudioProcessorValueTreeState& apvts, const juce::String& paramId, float labelFontSize = 12.5f)
    : RotarySliderWithLabels(rap, unitSuffix), apvts(apvts), paramId(paramId), labelFontSize(labelFontSize)
    {
      setComponentProperty("labelFontSize", labelFontSize);
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    juce::String paramId;
    float labelFontSize = 14.f;

void setComponentProperty(const juce::Identifier& propertyName, float value)
  {
    getProperties().set(propertyName, value);
  }

void mouseDoubleClick(const juce::MouseEvent& event) override
  {
    RotarySliderWithLabels::mouseDoubleClick(event);

    bool state = !getSliderState();
    apvts.getParameter(paramId)->setValueNotifyingHost(state ? 1.0f : 0.0f);
    setSliderEnabled(state);
    animateColor();
  }
};

//==============================================================================

struct EnableButton : juce::ToggleButton, juce::Timer {
  float alpha = 0.0f;
  float targetAlpha = 0.0f;
  float animationSpeed = 0.04f;

  EnableButton() {
    onClick = [this] { animateColor(); };
  }

  void animateColor() {
    targetAlpha = getToggleState() ? 1.0f : 0.0f;
    startTimerHz(120);
  }

  void timerCallback() override
  {
    alpha = alpha < targetAlpha ? std::min(alpha + animationSpeed, targetAlpha) : alpha = std::max(alpha - animationSpeed, targetAlpha);
    repaint();
    if (alpha == targetAlpha) { stopTimer(); }
  }

  float getAlpha() const { return alpha; }
};

//==============================================================================

struct BPMLabel : juce::Label
{
  BPMLabel()
  {
    auto typeface = juce::Typeface::createSystemTypefaceFor(BinaryData::Orbitron_ttf, BinaryData::Orbitron_ttfSize);
    juce::FontOptions fontOptions = juce::FontOptions(typeface).withHeight(11.5f).withStyle("plain");
    setFont(juce::Font(fontOptions));
    setColour(juce::Label::textColourId, juce::Colours::white);
  }   
};

//==============================================================================

class DelayAudioProcessorEditor  : public juce::AudioProcessorEditor, juce::Timer
{
public:
    DelayAudioProcessorEditor (DelayAudioProcessor&);
    ~DelayAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
  void DelayAudioProcessorEditor::timerCallback()
  {
    updateBPMLabel();
    repaint();
  }
  void DelayAudioProcessorEditor::setSliderState(bool state, RotarySliderWithLabels &slider);

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    DelayAudioProcessor& audioProcessor;

    const juce::Typeface::Ptr typeface = juce::Typeface::createSystemTypefaceFor(BinaryData::Orbitron_ttf, BinaryData::Orbitron_ttfSize);
    // g.setFont(juce::Font(typeface).withHeight(15.5f)); // slider labels

    RotarySliderToggle
    delayTimeSliderRight,
    lowPassSlider,
    highPassSlider,
    chorusSlider,
    reverbSlider;

    RotarySliderWithLabels
    delayTimeSliderLeft,
    feedbackSlider,
    dryWetSlider;

    using APVTS = juce::AudioProcessorValueTreeState;
    using Attachment = APVTS::SliderAttachment;

    Attachment
    delayTimeSliderAttachmentLeft,
    delayTimeSliderAttachmentRight,
    feedbackSliderAttachment,
    dryWetSliderAttachment,
    lowPassSliderAttachement,
    highPassSliderAttachement,
    chorusSliderAttachement,
    reverbSliderAttachement;

    // using ButtonAttachment = APVTS::ButtonAttachment; // don't need buttons with double-click sliders, but save for later
    // EnableButton dualDelayButton;
    // ButtonAttachment dualDelayButtonAttachment;

    BPMLabel bpmLabel;
    void updateBPMLabel();
    float lastBPM = 120.f;

    std::vector<juce::Component*> getComps();

    //juce::Image background; // just used for drawing bbox rects for ui layout

    RotaryLookAndFeel lnf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayAudioProcessorEditor)
};
