#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DelayLine.h"
#include "ReverbLines.h"
#include "Filters.h"

//==============================================================================
DelayAudioProcessor::DelayAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), apvts (*this, nullptr, "Parameters", createParameters())
#endif
{
    PropertiesFile::Options options;
    options.applicationName = "Delay-Plugin";
    options.folderName = "lachesis17";
    appProperties.setStorageParameters(options);
}

DelayAudioProcessor::~DelayAudioProcessor()
{
    // juce::File Log("build/Delay_artefacts/Debug/Standalone/feedback.txt"); // log making
    // juce::FileLogger Logger(feedbackLog, "Log Message");
    // Logger.logMessage("Value: " + juce::String(delayTimeLeft));
}

//==============================================================================
const juce::String DelayAudioProcessor::getName() const
{
    return "";
}

bool DelayAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool DelayAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool DelayAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double DelayAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int DelayAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int DelayAudioProcessor::getCurrentProgram()
{
    return 0;
}

void DelayAudioProcessor::setCurrentProgram ([[maybe_unused]] int index)
{
}

const juce::String DelayAudioProcessor::getProgramName ([[maybe_unused]] int index)
{
    return {};
}

void DelayAudioProcessor::changeProgramName ([[maybe_unused]] int index, [[maybe_unused]] const juce::String& newName)
{
}

//==============================================================================
void DelayAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const juce::dsp::ProcessSpec spec{sampleRate, static_cast<juce::uint32>(samplesPerBlock), 2};

    currentSampleRate = getSampleRate();
    leftDelay = std::make_unique<DelayLine>(currentSampleRate);
    rightDelay = std::make_unique<DelayLine>(currentSampleRate);

    //== LOW PASS & HIGH PASS
    filters = std::make_unique<Filters>(currentSampleRate);

    //== REVERB LINES
    reverbLines = std::make_unique<ReverbLines>(currentSampleRate);

    //== ONE-POLE FILTER COEFFICIENTS
    coeff = 1.0f - static_cast<float>(std::exp( -1.0f / (0.1f * currentSampleRate)));
    coeff_sml = 1.0f - static_cast<float>(std::exp( -1.0f / (0.01f * currentSampleRate)));
    coeff_lrg = 1.0f - static_cast<float>(std::exp( -1.0f / (0.5f * currentSampleRate)));

    //== SMOOTHING
    leftDelay->resetSmoothedValue(0.7f);
    rightDelay->resetSmoothedValue(0.7f);
    smoothedFeedback.reset(currentSampleRate, 0.005f);
    smoothedDryWet.reset(currentSampleRate, 0.005f);
    smoothedChorus.reset(currentSampleRate, 77.7f);
    smoothedReverb.reset(currentSampleRate, 0.7f);
    smoothedReverbLevel.reset(currentSampleRate, 0.0075f);
    smoothedLowPassMix.reset(currentSampleRate, 0.7f);
    smoothedHighPassMix.reset(currentSampleRate, 0.7f);
    filters->resetSmoothing();

    //== CIRCULAR BUFFER
    leftDelay->makeBuffer();
    rightDelay->makeBuffer();
}


void DelayAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool DelayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported. In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void DelayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, [[maybe_unused]] juce::MidiBuffer& midiMessages)
{

    for (auto i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    //== PARAMETERS
    auto chainsettings = getChainSettings(apvts);
    float newFeedbackTime = chainsettings.feedbackTime;
    float newDryWet = chainsettings.dryWet;
    bool dualDelay = chainsettings.dualDelay;
    bool chorus = chainsettings.chorus;
    float newChorusRate = chainsettings.chorusRate;
    bool lowPass = chainsettings.lowPass;
    float newLowPassFreq = chainsettings.lowPassFreq;
    bool highPass = chainsettings.highPass;
    float newHighPassFreq = chainsettings.highPassFreq;
    bool reverb = chainsettings.reverb;
    float newReverbLevel = chainsettings.reverbLevel;
    float newDelayTimeLeft = chainsettings.delayTimeLeft;
    float newDelayTimeRight = dualDelay ? chainsettings.delayTimeRight : chainsettings.delayTimeLeft;
    float newInputSignalLevel = 0.f;
    float newOutputSignalLevel = 0.f;

    //== TOGGLE MIXES
    toggleButtonStateMixes(lowPass, highPass, chorus, reverb);

    //== COEFFICIENTS
    if (newLowPassFreq != filters->lastLowPassFreq)
        filters->updateLowPassFilter(newLowPassFreq, coeff);

    if (newHighPassFreq != filters->lastHighPassFreq)
        filters->updateHighPassFilter(newHighPassFreq, coeff);

    //== CHORUS RATE
    if (newChorusRate != chorusRate)
        chorusRate = newChorusRate;

    //== SMOOTHING
    smoothedFeedback.setTargetValue(newFeedbackTime);
    feedbackTime = applyOnePoleFilter(smoothedFeedback.getCurrentValue(), smoothedFeedback.getNextValue(), coeff_sml);

    smoothedReverbLevel.setTargetValue(newReverbLevel);
    reverbLevel = applyOnePoleFilter(smoothedReverbLevel.getCurrentValue(), smoothedReverbLevel.getNextValue(), coeff_sml);

    //== MIXING & BYPASS
    dryWetLeft = setDryWetMix(newDelayTimeLeft, dryWet, newDryWet, smoothedDryWet);
    dryWetRight = setDryWetMix(newDelayTimeRight, dryWet, newDryWet, smoothedDryWet);

    //== REVERB DELAY TIMES
    reverbLines->updateTargetDelayTimes();

    //== PROCESSING LOOP
    for (int channel = 0; channel < numChannels; ++channel)
    {
    const float* inData = buffer.getReadPointer(channel);
    float* outData = buffer.getWritePointer(channel);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            newInputSignalLevel = fmaxf(newInputSignalLevel, fabsf(inData[sample]));
            inputSignalLevel = newInputSignalLevel * 1.25f;
            inputSignalLevel = fminf(inputSignalLevel, 1.0f);       // keep it below 1

            if (channel == 0) //== LEFT CHANNEL
            {
                //== CHORUS & DELAY
                smoothedChorus.skip(sample); 
                float newDelay = applyChorus(sample, smoothedChorus.getCurrentValue(), *leftDelay, newDelayTimeLeft);
                leftDelay->updateDelayTime(newDelay);

                float delayedSample = leftDelay->readBufferDelayedSample();

                //== LOW PASS
                currentLowPassMix = smoothedLowPassMix.getNextValue();
                float lowPassSample = filters->processLowFilter(true, delayedSample);          
                delayedSample = (1.0f - currentLowPassMix) * delayedSample + currentLowPassMix * lowPassSample;

                //== HIGH PASS
                currentHighPassMix = smoothedHighPassMix.getNextValue();
                float highPassSample = filters->processHighFilter(true, delayedSample);
                delayedSample = (1.0f - currentHighPassMix) * delayedSample + currentHighPassMix * highPassSample;

                //== GENERAL LOW PASS
                delayedSample = filters->processGeneralLowFilter(true, delayedSample);
                
                //== MIXING
                leftDelay->writeDelayBuffer(inData[sample], feedbackTime, delayedSample);
                float wetScale = (1.0f - dryWetLeft) + dryWetLeft * 0.5f;  // making this to control the volume changes when mixing dry/wet signals
                outData[sample] = wetScale * inData[sample] + dryWetLeft * delayedSample;  // dry / wet   //outData[sample] = delayedSample; // 100% wet  // outData[sample] = (1.0f - dryWet) * inData[sample] + dryWet * delayedSample; // original
                leftDelay->updateWriteIndex();

                //== REVERB
                float combinedReverb = reverbLines->applyReverb(true, inData[sample], reverbLevel);
                float wetReverb = (1.0f - reverbLevel) + reverbLevel * 0.5f;
                combinedReverb = wetReverb * outData[sample] + reverbLevel * combinedReverb;
                currentReverbMix = smoothedReverb.getNextValue();       
                outData[sample] += (1.0f - currentReverbMix) * outData[sample] + currentReverbMix * combinedReverb;
            }
            else if (channel == 1) //== RIGHT CHANNEL
            {
                //== CHORUS & DELAY
                smoothedChorus.skip(sample); 
                float newDelay = applyChorus(sample, smoothedChorus.getCurrentValue(), *rightDelay, newDelayTimeRight);
                rightDelay->updateDelayTime(newDelay);

                float delayedSample = rightDelay->readBufferDelayedSample();

                //== LOW PASS
                currentLowPassMix = smoothedLowPassMix.getNextValue(); 
                float lowPassSample = filters->processLowFilter(false, delayedSample);
                delayedSample = (1.0f - currentLowPassMix) * delayedSample + currentLowPassMix * lowPassSample;

                //== HIGH PASS
                currentHighPassMix = smoothedHighPassMix.getNextValue(); 
                float highPassSample = filters->processHighFilter(false, delayedSample);
                delayedSample = (1.0f - currentHighPassMix) * delayedSample + currentHighPassMix * highPassSample;

                ///== GENERAL LOW PASS
                delayedSample = filters->processGeneralLowFilter(false, delayedSample);

                //== MIXING
                rightDelay->writeDelayBuffer(inData[sample], feedbackTime, delayedSample);
                float wetScale = (1.0f - dryWetRight) + dryWetRight * 0.5f;
                outData[sample] = wetScale * inData[sample] + dryWetRight * delayedSample; 
                rightDelay->updateWriteIndex();

                //== REVERB;
                float combinedReverb = reverbLines->applyReverb(false, inData[sample], reverbLevel);
                float wetReverb = (1.0f - reverbLevel) + reverbLevel * 0.5f;
                combinedReverb = wetReverb * outData[sample] + reverbLevel * combinedReverb;
                currentReverbMix = smoothedReverb.getNextValue();       
                outData[sample] += (1.0f - currentReverbMix) * outData[sample] + currentReverbMix * combinedReverb;
            }
            newOutputSignalLevel = fmaxf(newOutputSignalLevel, fabsf(outData[sample]));
            outputSignalLevel = newOutputSignalLevel * 1.25f;
            outputSignalLevel = fminf(outputSignalLevel, 1.0f);       // keep it below 1
        }
    }
}

//==============================================================================
bool DelayAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* DelayAudioProcessor::createEditor()
{
    return new DelayAudioProcessorEditor (*this);
    // return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void DelayAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void DelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if (tree.isValid())
    {
        apvts.replaceState(tree);
    }
}

[[nodiscard]] float DelayAudioProcessor::applyChorus(int sample, float currentMixValue, DelayLine& delayLine, float newDelayTime)
{
    chorusModulation = chorusDepth * static_cast<float>(std::sin(2.0f * juce::MathConstants<float>::pi * chorusRate * sample / currentSampleRate + chorusPhase));
    chorusPhase += 2.0f * static_cast<float>(juce::MathConstants<float>::pi * chorusRate / currentSampleRate);
    chorusPhase = std::fmod(chorusPhase, 2.0f * juce::MathConstants<float>::pi);
    if (chorusPhase < 0)
    {
        chorusPhase += 2.0f * juce::MathConstants<float>::pi;
    }

    if (newDelayTime != delayLine.getSmoothedCurrent() && newDelayTime != 0.f) 
    {
        delayLine.setNewTarget(newDelayTime + chorusModulation); // target + chorus + large ramp = yes
    } 
    else
    {
        delayLine.setNewTarget(newDelayTime);
    }

    float delayedSample = delayLine.getCurrentDelayTime();
    delayedSample = applyOnePoleFilter(delayedSample, delayLine.getSmoothedNext(), coeff_sml); // need to apply chorus to sample after this, even though it pops

    if (newDelayTime != 0.f) // bypass chorus even when enabled
    {
        delayedSample += chorusModulation * currentMixValue;
    }

    return delayedSample;
}

[[nodiscard]] float DelayAudioProcessor::applyOnePoleFilter(float current, float next, float coefficient)
{
    return next + ((next - current) * coefficient);
}

void DelayAudioProcessor::toggleButtonStateMixes(bool lowPass, bool highPass, bool chorus, bool reverb)
{
    targetLowPassMix = lowPass ? 1.0f : 0.0f;
    targetHighPassMix = highPass ? 1.0f : 0.0f;
    targetChorusMix = chorus ? 1.0f : 0.0f;
    targetReverbMix = reverb ? 1.0f : 0.0f;
    smoothedLowPassMix.setTargetValue(targetLowPassMix);
    smoothedHighPassMix.setTargetValue(targetHighPassMix);
    smoothedChorus.setTargetValue(targetChorusMix);
    smoothedReverb.setTargetValue(targetReverbMix);
}

[[nodiscard]] float DelayAudioProcessor::setDryWetMix(float newDelayTime, float drywet, float newDryWet, SmoothedValue<float, ValueSmoothingTypes::Linear>& smoothedwrywet)
{
    smoothedwrywet.setTargetValue(newDelayTime == 0.f ? 0.f : newDryWet);
    return applyOnePoleFilter(drywet, smoothedwrywet.getNextValue(), newDelayTime == 0.f ? coeff_lrg : coeff_sml);
}

[[nodiscard]] float DelayAudioProcessor::getCurrentBPM()
{
    AudioPlayHead* playhead = getPlayHead();
    AudioPlayHead::CurrentPositionInfo positionInfo;

    if (playhead && playhead->getPosition())
    {
        if (positionInfo.bpm > 0)
        {
            return static_cast<float>(positionInfo.bpm);
        }
    }
    return 120.0f;
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts) {
    ChainSettings settings;

    settings.delayTimeLeft = apvts.getRawParameterValue("Delay Left")->load();
    settings.delayTimeRight = apvts.getRawParameterValue("Delay Right")->load();
    settings.feedbackTime = apvts.getRawParameterValue("Feedback")->load();
    settings.dryWet = apvts.getRawParameterValue("Dry Wet")->load();
    settings.dualDelay = apvts.getRawParameterValue("Dual Delay")->load() > 0.5f;
    settings.chorus = apvts.getRawParameterValue("Chorus")->load() > 0.5f;
    settings.chorusRate = apvts.getRawParameterValue("Chorus Rate")->load();
    settings.lowPass = apvts.getRawParameterValue("Low Pass")->load() > 0.5f;
    settings.lowPassFreq = apvts.getRawParameterValue("Low Pass Freq")->load();
    settings.highPassFreq = apvts.getRawParameterValue("High Pass Freq")->load();
    settings.highPass = apvts.getRawParameterValue("High Pass")->load() > 0.5f;
    settings.reverb = apvts.getRawParameterValue("Reverb")->load() > 0.5f;
    settings.reverbLevel = apvts.getRawParameterValue("Reverb Level")->load();

    return settings;
}

juce::AudioProcessorValueTreeState::ParameterLayout DelayAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // juce::StringArray noteStringArray;
    // std::vector<juce::String> divArray = {"1/64", "1/64D", "1/32", "1/32D", "1/16", "1/16D", "1/8", "1/8D", "1/4", "1/4D", "1/2", "1/2D", "1/1"};
    // for (const auto& div : divArray) {
    //     noteStringArray.add(div);
    // }

    params.push_back(std::make_unique<juce::AudioParameterInt>("Delay Left", "Delay Left", 0, 2000, 320));
    params.push_back(std::make_unique<juce::AudioParameterInt>("Delay Right", "Delay Right", 0, 2000, 320));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("Feedback", "Feedback", juce::NormalisableRange<float>(0.f, 1.f, 0.01f, 1.f), 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("Dry Wet", "Dry Wet", juce::NormalisableRange<float>(0.f, 1.f, 0.02f, 1.f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("Dual Delay", "Dual Delay", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("Chorus", "Chorus", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("Chorus Rate", "Chorus Rate", juce::NormalisableRange<float>(0.1f, 3.f, 0.01f, 1.f), 0.45f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("Low Pass", "Low Pass", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("Low Pass Freq", "Low Pass Freq", juce::NormalisableRange<float>(20.f, 7000.f, 1.f, 0.25f), 2000.f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("High Pass", "High Pass", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("High Pass Freq", "High Pass Freq", juce::NormalisableRange<float>(20.f, 1000.f, 1.f, 0.25f), 500.f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("Reverb", "Reverb", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("Reverb Level", "Reverb Level", juce::NormalisableRange<float>(0.f, 1.f, 0.02f, 1.f), 0.5f));

    //params.push_back(std::make_unique<juce::AudioParameterChoice>("Divisions", "Divisions", noteStringArray, 0));

    return { params.begin(), params.end() };
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DelayAudioProcessor();
}
