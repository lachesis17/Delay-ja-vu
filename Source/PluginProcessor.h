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

private:
	ApplicationProperties appProperties;

	float applyChorus(int sample, float currentMixValue, DelayLine& delayLine, float newDelayTime);
	float applyOnePoleFilter(float current, float next, float coefficient);
	float setDryWetMix(float newDelayTime, float dryWet, float newDryWet, SmoothedValue<float, ValueSmoothingTypes::Linear>& smoothedDryWet);
	void toggleButtonStateMixes(bool lowPass, bool highPass, bool chorus, bool reverb);

	juce::LinearSmoothedValue<float> smoothedFeedback, smoothedDryWet, smoothedLowPassMix, smoothedHighPassMix, smoothedChorus, smoothedReverb, smoothedReverbLevel;

	std::unique_ptr<DelayLine> leftDelay, rightDelay;
	std::unique_ptr<ReverbLines> reverbLines;
	std::unique_ptr<Filters> filters;
	double currentSampleRate;
	
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
	float reverbLevel;

	float chorusRate = 0.45f; 
	float chorusDepth = 0.33f;
	float chorusPhase = 0.f;
	float chorusModulation = 0.f;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayAudioProcessor)
};
