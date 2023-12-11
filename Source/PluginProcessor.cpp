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
    memset(circularBufferLeft, 0, sizeof(float) * maxBufferSize);
    memset(circularBufferRight, 0, sizeof(float) * maxBufferSize);
}

DelayAudioProcessor::~DelayAudioProcessor()
{
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
    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = 2;
    spec.sampleRate = sampleRate;

    leftChain.prepare(spec);
    rightChain.prepare(spec);

    coeff = 1.0f - std::exp( -1.0f / (0.05f * sampleRate)); // tape delay effect

    auto chainsettings = getChainSettings(apvts);
    delayTimeLeft = chainsettings.delayTimeLeft;
    delayTimeRight = chainsettings.delayTimeRight;
    smoothedDelayTimeLeft.reset(sampleRate, 0.05);
    smoothedDelayTimeRight.reset(sampleRate, 0.05);
    smoothedFeedback.reset(sampleRate, 0.05);
}


void DelayAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
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

    auto chainsettings = getChainSettings(apvts);
    feedbackTime = chainsettings.feedbackTime;
    float newDelayTimeLeft = chainsettings.delayTimeLeft;
    float newDelayTimeRight = chainsettings.delayTimeRight;

    for (int channel = 0; channel < numChannels; ++channel)
    {
    const float* inData = buffer.getReadPointer(channel);
    float* outData = buffer.getWritePointer(channel);

        for (int sample = 0; sample < numSamples; ++sample)
        {

            if (channel == 0)
            {
                smoothedDelayTimeLeft.setTargetValue(newDelayTimeLeft);
                smoothedFeedback.setTargetValue(feedbackTime);

                delayTimeLeft = smoothedDelayTimeLeft.getNextValue();
                feedbackTime = smoothedFeedback.getNextValue();

                delayTimeLeft += (smoothedDelayTimeLeft.getNextValue() - delayTimeLeft) * coeff;
                //delayTimeLeft += (newDelayTimeLeft - delayTimeLeft) * coeff;

                int readIndexLeft = (writeIndexLeft - int(delayTimeLeft * getSampleRate() / 1000) + maxBufferSize) % maxBufferSize;
                float delayedSample = circularBufferLeft[readIndexLeft];
                circularBufferLeft[writeIndexLeft] = inData[sample] + feedbackTime * delayedSample;
                outData[sample] = delayedSample;
                writeIndexLeft = (writeIndexLeft + 1) % maxBufferSize;
            }
            else if (channel == 1)
            {

                smoothedDelayTimeRight.setTargetValue(newDelayTimeRight);
                smoothedFeedback.setTargetValue(feedbackTime);

                delayTimeRight = smoothedDelayTimeRight.getNextValue();
                feedbackTime = smoothedFeedback.getNextValue();

                delayTimeRight += (smoothedDelayTimeRight.getNextValue() - delayTimeRight) * coeff;
                //delayTimeRight += (newDelayTimeRight - delayTimeRight) * coeff;

                int readIndexRight = (writeIndexRight - int(delayTimeRight * getSampleRate() / 1000) + maxBufferSize) % maxBufferSize;
                float delayedSample = circularBufferRight[readIndexRight];
                circularBufferRight[writeIndexRight] = inData[sample] + feedbackTime * delayedSample;
                outData[sample] = delayedSample;
                writeIndexRight = (writeIndexRight + 1) % maxBufferSize;
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
}

void DelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts) {
    ChainSettings settings;

    settings.delayTimeLeft = apvts.getRawParameterValue("Delay Left")->load();
    settings.delayTimeRight = apvts.getRawParameterValue("Delay Right")->load();
    settings.feedbackTime = apvts.getRawParameterValue("Feedback")->load();

    return settings;
}

void DelayAudioProcessor::updateFilters(double sampleRate)
{
    auto chainSettings = getChainSettings(apvts);

    //auto delaySamples = (chainSettings.delayTime * sampleRate);
}


juce::AudioProcessorValueTreeState::ParameterLayout DelayAudioProcessor::createParameters()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterInt>("Delay Left", "Delay Left", 1, 1500, 320));
    params.push_back(std::make_unique<juce::AudioParameterInt>("Delay Right", "Delay Right", 1, 1500, 320));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("Feedback", "Feedback", juce::NormalisableRange<float>(0.f, 0.98f, 0.01f, 1.f), 0.25f));

    return { params.begin(), params.end() };
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DelayAudioProcessor();
}
