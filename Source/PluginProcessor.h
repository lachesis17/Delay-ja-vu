#pragma once

#include <JuceHeader.h>
#include "DelayLine.h"

struct ChainSettings {
	float delayTimeLeft {0};
	float delayTimeRight {0};
	float feedbackTime {0};
	float dryWet {0};
	bool dualDelay {false};
	bool chorus {false};
	bool lowPass {false};
	bool highPass {false};
};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts);

using Filter = juce::dsp::IIR::Filter<float>;

using CutFilter = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;

using MonoChain = juce::dsp::ProcessorChain<CutFilter, Filter, CutFilter>;

//==============================================================================
/**
*/
class DelayAudioProcessor  : public juce::AudioProcessor
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
{
public:
    //==============================================================================
    DelayAudioProcessor();
    ~DelayAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // custom layout
    juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    juce::AudioProcessorValueTreeState apvts;

	ApplicationProperties& getAppProperties() { return appProperties; }
	float getCurrentBPM();

private:
	ApplicationProperties appProperties;

	float applyChorus(int sample, float currentMixValue, float delayedSample, SmoothedValue<float, ValueSmoothingTypes::Linear>& smoothedDelayTime, float newDelayTime);
	void toggleButtonStateMixes(bool lowPass, bool highPass, bool chorus);
	float applyOnePoleFilter(float current, float next, float coefficient);
	float setDryWetMix(float newDelayTime, float dryWet, float newDryWet, SmoothedValue<float, ValueSmoothingTypes::Linear>& smoothedDryWet);

	MonoChain leftChain, rightChain;
	juce::LinearSmoothedValue<float> smoothedFeedback, smoothedDryWet, smoothedLowPass, smoothedHighPass, smoothedChorus;

	std::unique_ptr<DelayLine> leftDelay;
	std::unique_ptr<DelayLine> rightDelay;
	double currentSampleRate;

	juce::dsp::IIR::Filter<float> leftLowPass;
	juce::dsp::IIR::Filter<float> rightLowPass;
	juce::dsp::IIR::Filter<float> leftHighPass;
	juce::dsp::IIR::Filter<float> rightHighPass;
	juce::dsp::IIR::Filter<float> leftLowAll;
	juce::dsp::IIR::Filter<float> rightLowAll;

	float currentLowPassMix = 0.f;
	float targetLowPassMix = 0.f;
	float currentHighPassMix = 0.f;
	float targetHighPassMix = 0.f;
	float currentChorusMix = 0.f;
	float targetChorusMix = 0.f;

	int writeIndexLeft = 0;
	int writeIndexRight = 0;
	float coeff;
	float coeff_sml;
	float coeff_lrg;
	float feedbackTime;
	float dryWet;
	float dryWetLeft;
	float dryWetRight;

	float chorusRate = 0.45f; 
	float chorusDepth = 0.75f;
	float chorusPhase = 0.f;
	float chorusModulation = 0.f;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayAudioProcessor)
};
