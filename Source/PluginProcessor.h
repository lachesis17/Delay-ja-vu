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

	float applyChorus(int sample, float currentMixValue, DelayLine& delayLine, float newDelayTime);
	float applyReverb(std::array<std::unique_ptr<DelayLine>, 10>& reverbDelays,
					  std::array<juce::dsp::IIR::Filter<float>, 10>& reverbLowPass, 
					  std::array<juce::dsp::IIR::Filter<float>, 10>& reverbAllPass, 
					  float sample, 
					  float& dryWet);
	void toggleButtonStateMixes(bool lowPass, bool highPass, bool chorus);
	float applyOnePoleFilter(float current, float next, float coefficient);
	float setDryWetMix(float newDelayTime, float dryWet, float newDryWet, SmoothedValue<float, ValueSmoothingTypes::Linear>& smoothedDryWet);

	MonoChain leftChain, rightChain;
	juce::LinearSmoothedValue<float> smoothedFeedback, smoothedDryWet, smoothedLowPass, smoothedHighPass, smoothedChorus;

	std::unique_ptr<DelayLine> leftDelay, rightDelay;
	double currentSampleRate;

	juce::dsp::IIR::Filter<float> leftLowPass, rightLowPass, leftHighPass, rightHighPass, leftLowAll, rightLowAll;
	
	float currentLowPassMix, targetLowPassMix, currentHighPassMix, targetHighPassMix, currentChorusMix, targetChorusMix = 0.f;

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

	std::array<std::unique_ptr<DelayLine>, 10> reverbDelaysLeft;
	std::array<std::unique_ptr<DelayLine>, 10> reverbDelaysRight;
	const std::array<float, 10> fixedDelayTimesLeft = {33.0f, 42.0f, 55.0f, 77.0f, 86.0f, 121.0f, 133.0f, 143.0f, 152.0f, 168.0f};
	const std::array<float, 10> fixedDelayTimesRight = {43.0f, 64.0f, 72.0f, 89.0f, 101.0f, 117.0f, 125.0f, 130.0f, 142.0f, 158.0f};
	std::array<juce::dsp::IIR::Filter<float>, 10> reverbLowPassLeft;
	std::array<juce::dsp::IIR::Filter<float>, 10> reverbLowPassRight;
	std::array<juce::dsp::IIR::Filter<float>, 10> reverbAllPassLeft;
	std::array<juce::dsp::IIR::Filter<float>, 10> reverbAllPassRight;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayAudioProcessor)
};
