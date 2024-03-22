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
	float chorusRate = {0.45f};
	bool lowPass {false};
	float lowPassFreq {2000};
	float highPassFreq {500};
	bool highPass {false};
	bool reverb {false};
	float reverbLevel {0};
};

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts);

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
					  std::array<juce::dsp::IIR::Filter<float>, 10>& reverbAllPass1, 
					  std::array<juce::dsp::IIR::Filter<float>, 10>& reverbAllPass2,
					  std::array<juce::dsp::IIR::Filter<float>, 10>& reverbAllPass3,
					  std::array<juce::dsp::IIR::Filter<float>, 10>& reverbAllPass4,
					  std::array<juce::dsp::IIR::Filter<float>, 10>& reverbAllPass5,
					  float sample, 
					  float& dryWet);
	void toggleButtonStateMixes(bool lowPass, bool highPass, bool chorus, bool reverb);
	float applyOnePoleFilter(float current, float next, float coefficient);
	float setDryWetMix(float newDelayTime, float dryWet, float newDryWet, SmoothedValue<float, ValueSmoothingTypes::Linear>& smoothedDryWet);
	void DelayAudioProcessor::updateLowPassFilter(juce::dsp::IIR::Filter<float>& filter, float frequency, double sampleRate);
	void DelayAudioProcessor::updateHighPassFilter(juce::dsp::IIR::Filter<float>& filter, float frequency, double sampleRate);

	juce::LinearSmoothedValue<float> smoothedFeedback, smoothedDryWet, smoothedLowPassMix, smoothedHighPassMix, smoothedChorus, smoothedReverb, smoothedReverbLevel,
									 smoothedLowPassFreq, smoothedHighPassFreq;

	std::unique_ptr<DelayLine> leftDelay, rightDelay;
	double currentSampleRate;

	juce::dsp::IIR::Filter<float> leftLowPass, rightLowPass, leftHighPass, rightHighPass, leftLowAll, rightLowAll;
	
	float currentLowPassMix, targetLowPassMix, currentHighPassMix, targetHighPassMix, targetChorusMix, currentReverbMix, targetReverbMix = 0.f;

	int writeIndexLeft = 0;
	int writeIndexRight = 0;
	float coeff;
	float coeff_sml;
	float coeff_lrg;
	float feedbackTime;
	float dryWet;
	float dryWetLeft;
	float dryWetRight;
	float lastLowPassFreq;
	float lastHighPassFreq;
	float reverbLevel;

	float chorusRate = 0.45f; 
	float chorusDepth = 0.33f;
	float chorusPhase = 0.f;
	float chorusModulation = 0.f;

	float reverbModRate = 0.005f;
	float reverbModDepth = 0.005f;
	float reverbModPhase = 0.f;
	float reverbModulation = 0.f;

	std::array<std::unique_ptr<DelayLine>, 10> reverbDelaysLeft;
	std::array<std::unique_ptr<DelayLine>, 10> reverbDelaysRight;
	//const std::array<float, 10> fixedDelayTimesLeft = {33.0f, 42.0f, 55.0f, 77.0f, 86.0f, 121.0f, 133.0f, 143.0f, 152.0f, 168.0f};
	//const std::array<float, 10> fixedDelayTimesRight = {43.0f, 64.0f, 72.0f, 89.0f, 101.0f, 117.0f, 125.0f, 130.0f, 142.0f, 158.0f};
	const std::array<float, 10> fixedDelayTimesLeft = {182.20f, 164.17f, 149.06f, 136.87f, 127.59f, 121.24f, 117.80f, 117.29f, 119.69f, 125.02f};
	const std::array<float, 10> fixedDelayTimesRight = {184.03f, 165.81f, 150.55f, 138.24f, 128.87f, 122.45f, 118.98f, 118.46f, 120.89f, 126.27f};
	std::array<juce::dsp::IIR::Filter<float>, 10> reverbLowPassLeft;
	std::array<juce::dsp::IIR::Filter<float>, 10> reverbLowPassRight;
	std::array<juce::dsp::IIR::Filter<float>, 10> reverbAllPassLeft1;
	std::array<juce::dsp::IIR::Filter<float>, 10> reverbAllPassRight1;
	std::array<juce::dsp::IIR::Filter<float>, 10> reverbAllPassLeft2;
	std::array<juce::dsp::IIR::Filter<float>, 10> reverbAllPassRight2;
	std::array<juce::dsp::IIR::Filter<float>, 10> reverbAllPassLeft3;
	std::array<juce::dsp::IIR::Filter<float>, 10> reverbAllPassRight3;
	std::array<juce::dsp::IIR::Filter<float>, 10> reverbAllPassLeft4;
	std::array<juce::dsp::IIR::Filter<float>, 10> reverbAllPassRight4;
	std::array<juce::dsp::IIR::Filter<float>, 10> reverbAllPassLeft5;
	std::array<juce::dsp::IIR::Filter<float>, 10> reverbAllPassRight5;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayAudioProcessor)
};
