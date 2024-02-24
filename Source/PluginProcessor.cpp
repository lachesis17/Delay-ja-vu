/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

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

void DelayAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String DelayAudioProcessor::getProgramName (int index)
{
    return {};
}

void DelayAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void DelayAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const juce::dsp::ProcessSpec spec{sampleRate, static_cast<juce::uint32>(samplesPerBlock), 2};

    leftChain.prepare(spec);
    rightChain.prepare(spec);

    currentSampleRate = getSampleRate();

    //== LOW PASS & HIGH PASS
    leftLowPass.reset(); 
    rightLowPass.reset();
    leftHighPass.reset(); 
    rightHighPass.reset();
    leftLowAll.reset();
    rightLowAll.reset();

    juce::dsp::IIR::Coefficients<float>::Ptr coefficientsLow = juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, 2000);     //const double highSampleRate = 1e6; // 1mil hz
    juce::dsp::IIR::Coefficients<float>::Ptr coefficientsHigh = juce::dsp::IIR::Coefficients<float>::makeHighPass(currentSampleRate, 500); 
    juce::dsp::IIR::Coefficients<float>::Ptr coefficientsLowAll = juce::dsp::IIR::Coefficients<float>::makeLowPass(currentSampleRate, 10000);

    leftLowPass.coefficients = coefficientsLow;
    rightLowPass.coefficients = coefficientsLow;
    leftHighPass.coefficients = coefficientsHigh;
    rightHighPass.coefficients = coefficientsHigh;
    leftLowAll.coefficients = coefficientsLowAll;
    rightLowAll.coefficients = coefficientsLowAll;

    //== ONE-POLE FILTER COEFFICENTS
    coeff = 1.0f - std::exp( -1.0f / (0.1f * currentSampleRate));
    coeff_sml = 1.0f - std::exp( -1.0f / (0.01f * currentSampleRate));
    coeff_lrg = 1.0f - std::exp( -1.0f / (0.5f * currentSampleRate));

    //== SMOOTHING
    smoothedDelayTimeLeft.reset(currentSampleRate, 0.77f);
    smoothedDelayTimeRight.reset(currentSampleRate, 0.77f);
    smoothedFeedback.reset(currentSampleRate, 0.005);
    smoothedDryWet.reset(currentSampleRate, 0.005);
    smoothedLowPass.reset(currentSampleRate, 0.77f);
    smoothedHighPass.reset(currentSampleRate, 0.77f);
    smoothedChorus.reset(currentSampleRate, 0.05f);

    //== CIRCULAR BUFFER
    circBuffLeft.createCircularBuffer(2 * currentSampleRate);   // doubled or limited to 1365ms @ 48k
    circBuffRight.createCircularBuffer(2 * currentSampleRate);
    circBuffLeft.flushBuffer();
    circBuffRight.flushBuffer();
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
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
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

void DelayAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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
    bool lowPass = chainsettings.lowPass;
    bool highPass = chainsettings.highPass;
    float newDelayTimeLeft = chainsettings.delayTimeLeft;
    float newDelayTimeRight = dualDelay ? chainsettings.delayTimeRight : chainsettings.delayTimeLeft;

    //== TOGGLE MIXES
    toggleButtonStateMixes(lowPass, highPass, chorus);
    smoothedLowPass.setTargetValue(targetLowPassMix);
    smoothedHighPass.setTargetValue(targetHighPassMix);
    smoothedChorus.setTargetValue(targetChorusMix);

    //== SMOOTHING
    smoothedFeedback.setTargetValue(newFeedbackTime);
    feedbackTime = applyOnePoleFilter(feedbackTime, smoothedFeedback.getNextValue(), coeff_sml);

    //== MIXING & BYPASS
    dryWetLeft = setDryWetMix(newDelayTimeLeft, dryWet, newDryWet, smoothedDryWet);
    dryWetRight = setDryWetMix(newDelayTimeRight, dryWet, newDryWet, smoothedDryWet);

    //== PROCESSING LOOP
    for (int channel = 0; channel < numChannels; ++channel)
    {
    const float* inData = buffer.getReadPointer(channel);
    float* outData = buffer.getWritePointer(channel);

        for (int sample = 0; sample < numSamples; ++sample)
        {

            if (channel == 0) //== LEFT CHANNEL
            {
                smoothedDelayTimeLeft.setTargetValue(newDelayTimeLeft);
                delayTimeLeft = applyOnePoleFilter(delayTimeLeft, smoothedDelayTimeLeft.getNextValue(), coeff);

                //== CHORUS
                smoothedChorus.skip(sample); 
                delayTimeLeft = applyChorus(sample, smoothedChorus.getCurrentValue(), delayTimeLeft, smoothedDelayTimeLeft.getNextValue(), newDelayTimeLeft);

                float delayedSample = circBuffLeft.readBuffer(delayTimeLeft * currentSampleRate / 1000.0);

                //== LOW PASS
                currentLowPassMix = smoothedLowPass.getNextValue();
                float lowPassSample = leftLowPass.processSample(delayedSample);          
                delayedSample = (1.0f - currentLowPassMix) * delayedSample + currentLowPassMix * lowPassSample;

                //== HIGH PASS
                currentHighPassMix = smoothedHighPass.getNextValue();
                float highPassSample = leftHighPass.processSample(delayedSample);          
                delayedSample = (1.0f - currentHighPassMix) * delayedSample + currentHighPassMix * highPassSample;

                //== GENERAL LOW PASS
                delayedSample = leftLowAll.processSample(delayedSample);
                
                //== MIXING
                circBuffLeft.writeBuffer(inData[sample] + feedbackTime * delayedSample);
                float wetScale = (1.0f - dryWetLeft) + dryWetLeft * 0.5;  // making this to control the volume changes when mixing dry/wet signals
                outData[sample] = wetScale * inData[sample] + dryWetLeft * delayedSample;  // dry / wet   //outData[sample] = delayedSample; // 100% wet  // outData[sample] = (1.0f - dryWet) * inData[sample] + dryWet * delayedSample; // original
                writeIndexLeft = (writeIndexLeft + 1) % circBuffLeft.getBufferLength();
            }
            else if (channel == 1) //== RIGHT CHANNEL
            {
                smoothedDelayTimeRight.setTargetValue(newDelayTimeRight);
                delayTimeRight = applyOnePoleFilter(delayTimeRight, smoothedDelayTimeRight.getNextValue(), coeff);

                //== CHORUS
                smoothedChorus.skip(sample); 
                delayTimeRight = applyChorus(sample, smoothedChorus.getCurrentValue(), delayTimeRight, smoothedDelayTimeRight.getNextValue(), newDelayTimeRight);

                float delayedSample = circBuffRight.readBuffer(delayTimeRight * currentSampleRate / 1000.0);

                //== LOW PASS
                currentLowPassMix = smoothedLowPass.getNextValue(); 
                float lowPassSample = rightLowPass.processSample(delayedSample);  
                delayedSample = (1.0f - currentLowPassMix) * delayedSample + currentLowPassMix * lowPassSample;

                //== HIGH PASS
                currentHighPassMix = smoothedHighPass.getNextValue(); 
                float highPassSample = rightHighPass.processSample(delayedSample);  
                delayedSample = (1.0f - currentHighPassMix) * delayedSample + currentHighPassMix * highPassSample;

                ///== GENERAL LOW PASS
                delayedSample = rightLowAll.processSample(delayedSample);

                //== MIXING
                circBuffRight.writeBuffer(inData[sample] + feedbackTime * delayedSample);
                float wetScale = (1.0f - dryWetRight) + dryWetRight * 0.5;
                outData[sample] = wetScale * inData[sample] + dryWetRight * delayedSample; 
                writeIndexRight = (writeIndexRight + 1) % circBuffRight.getBufferLength();
            }
        }
    }

    juce::dsp::AudioBlock<float> block(buffer);
    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);
    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);
    leftChain.process(leftContext);
    rightChain.process(rightContext);

    lastDelayTimeLeft = newDelayTimeLeft;
    lastDelayTimeRight = newDelayTimeRight;
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
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    //auto editor = apvts.state.getOrCreateChildWithName ("editor", nullptr);
    juce::MemoryOutputStream mos(destData, true);
    apvts.state.writeToStream(mos);
}

void DelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    auto tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if (tree.isValid())
    {
        apvts.replaceState(tree);
    }
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts) {
    ChainSettings settings;

    settings.delayTimeLeft = apvts.getRawParameterValue("Delay Left")->load();
    settings.delayTimeRight = apvts.getRawParameterValue("Delay Right")->load();
    settings.feedbackTime = apvts.getRawParameterValue("Feedback")->load();
    settings.dryWet = apvts.getRawParameterValue("Dry Wet")->load();
    settings.dualDelay = apvts.getRawParameterValue("Dual Delay")->load() < 0.5f;
    settings.chorus = apvts.getRawParameterValue("Chorus")->load() < 0.5f;
    settings.lowPass = apvts.getRawParameterValue("Low Pass")->load() < 0.5f;
    settings.highPass = apvts.getRawParameterValue("High Pass")->load() < 0.5f;

    return settings;
}

float DelayAudioProcessor::applyChorus(int sample, float currentMixValue, float delayedSample, float nextDelayedSample, float newDelayTime)
{
    chorusModulation = currentMixValue * chorusDepth * std::sin(2.0 * juce::MathConstants<float>::pi * chorusRate * sample / currentSampleRate + chorusPhase);      //float randomMod = 0.f;      // randomMod += 0.05f * (rand() / static_cast<float>(RAND_MAX) - 0.5f); // randomised fun, with 0.05 as smoothing factor      // chorusModulation = chorusDepth * std::sin(2.0 * juce::MathConstants<float>::pi * chorusRate * sample / currentSampleRate + chorusPhase) + 0.1f * randomMod;
    chorusPhase += 2.0 * juce::MathConstants<float>::pi * chorusRate / currentSampleRate;
    if(chorusPhase > 2.0 * juce::MathConstants<float>::pi) {
        chorusPhase -= 2.0 * juce::MathConstants<float>::pi;
    }

    if (newDelayTime == 0.f) {
        delayedSample = applyOnePoleFilter(delayedSample, nextDelayedSample, coeff);
        return delayedSample;
    }

    float chorusSample = delayedSample + chorusModulation;
    delayedSample = applyOnePoleFilter(delayedSample, chorusSample, coeff);
    return delayedSample;
}

float DelayAudioProcessor::applyOnePoleFilter(float current, float next, float coefficient)
{
    return next + ((next - current) * coefficient);
}

void DelayAudioProcessor::toggleButtonStateMixes(bool lowPass, bool highPass, bool chorus) {
    targetLowPassMix = lowPass ? 1.0f : 0.0f;
    targetHighPassMix = highPass ? 1.0f : 0.0f;
    targetChorusMix = chorus ? 1.0f : 0.0f;
}

float DelayAudioProcessor::setDryWetMix(float newDelayTime, float dryWet, float newDryWet, SmoothedValue<float, ValueSmoothingTypes::Linear>& smoothedDryWet) 
{
    smoothedDryWet.setTargetValue(newDelayTime == 0.f ? 0.f : newDryWet);
    return applyOnePoleFilter(dryWet, smoothedDryWet.getNextValue(), newDelayTime == 0.f ? coeff_lrg : coeff_sml);
}

juce::AudioProcessorValueTreeState::ParameterLayout DelayAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterInt>("Delay Left", "Delay Left", 0, 2000, 320));
    params.push_back(std::make_unique<juce::AudioParameterInt>("Delay Right", "Delay Right", 0, 2000, 320));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("Feedback", "Feedback", juce::NormalisableRange<float>(0.f, 1.f, 0.01f, 1.f), 0.25f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("Dry Wet", "Dry Wet", juce::NormalisableRange<float>(0.f, 1.f, 0.02f, 1.f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("Dual Delay", "Dual Delay", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("Chorus", "Chorus", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("Low Pass", "Low Pass", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("High Pass", "High Pass", true));

    return { params.begin(), params.end() };
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DelayAudioProcessor();
}
