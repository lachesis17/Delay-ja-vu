#pragma once

#include <JuceHeader.h>
#include "DelayLine.h"
#include "ReverbLines.h"
#include "Filters.h"

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
	float getInputSignalLevel() const { return inputSignalLevel; }
	float getOutputSignalLevel() const { return outputSignalLevel; }

private:
	ApplicationProperties appProperties;

	[[nodiscard]] float applyChorus(int sample, float currentMixValue, DelayLine& delayLine, float newDelayTime);
	[[nodiscard]] float applyOnePoleFilter(float current, float next, float coefficient);
	[[nodiscard]] float setDryWetMix(float newDelayTime, float dryWet, float newDryWet, SmoothedValue<float, ValueSmoothingTypes::Linear>& smoothedDryWet);
	void toggleButtonStateMixes(bool lowPass, bool highPass, bool chorus, bool reverb);

	juce::LinearSmoothedValue<float> smoothedFeedback, smoothedDryWet, smoothedLowPassMix, smoothedHighPassMix, smoothedChorus, smoothedReverb, smoothedReverbLevel;

	std::unique_ptr<DelayLine> leftDelay, rightDelay;
	std::unique_ptr<ReverbLines> reverbLines;
	std::unique_ptr<Filters> filters;
	double currentSampleRate = 44100.0;

	// dont uninit floats since they can hold NaN (do not want in feedback!)
	float currentLowPassMix = 0.f, targetLowPassMix = 0.f, currentHighPassMix = 0.f, targetHighPassMix = 0.f, targetChorusMix = 0.f, currentReverbMix = 0.f, targetReverbMix = 0.f;

	int writeIndexLeft = 0;
	int writeIndexRight = 0;
	float coeff = 0.f;
	float coeff_sml = 0.f;
	float coeff_lrg = 0.f;
	float feedbackTime = 0.f;
	float dryWet = 0.f;
	float dryWetLeft = 0.f;
	float dryWetRight = 0.f;
	float reverbLevel = 0.f;

	float chorusRate = 0.45f; 
	float chorusDepth = 0.33f;
	float chorusPhase = 0.f;
	float chorusModulation = 0.f;

	float inputSignalLevel = 0.f;
	float outputSignalLevel = 0.f;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayAudioProcessor)
};
