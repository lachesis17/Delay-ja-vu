#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DelayLine.h"

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
    leftDelay = std::make_unique<DelayLine>(currentSampleRate);
    rightDelay = std::make_unique<DelayLine>(currentSampleRate);

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
    float g = 0.5f;
    auto allPassCoefficients = new juce::dsp::IIR::Coefficients<float>(-g, 1.0f, 1.0f, -g);

    leftLowPass.coefficients = coefficientsLow;
    rightLowPass.coefficients = coefficientsLow;
    leftHighPass.coefficients = coefficientsHigh;
    rightHighPass.coefficients = coefficientsHigh;
    leftLowAll.coefficients = coefficientsLowAll;
    rightLowAll.coefficients = coefficientsLowAll;

    //== REVERB LINES
    for (size_t i = 0; i < reverbDelaysLeft.size(); ++i) {
        reverbDelaysLeft[i] = std::make_unique<DelayLine>(currentSampleRate);
        reverbDelaysRight[i] = std::make_unique<DelayLine>(currentSampleRate);
        reverbDelaysLeft[i]->resetSmoothedValue(0.7f);
        reverbDelaysRight[i]->resetSmoothedValue(0.7f);
        reverbDelaysLeft[i]->makeBuffer();
        reverbDelaysRight[i]->makeBuffer();
        reverbLowPassLeft[i].reset();
        reverbLowPassRight[i].reset();
        reverbLowPassLeft[i].coefficients = coefficientsLow;
        reverbLowPassRight[i].coefficients = coefficientsLow;
        reverbAllPassLeft[i].reset();
        reverbAllPassRight[i].reset();
        reverbAllPassLeft[i].coefficients = allPassCoefficients;
        reverbAllPassRight[i].coefficients = allPassCoefficients;
    }

    //== ONE-POLE FILTER COEFFICENTS
    coeff = 1.0f - std::exp( -1.0f / (0.1f * currentSampleRate));
    coeff_sml = 1.0f - std::exp( -1.0f / (0.01f * currentSampleRate));
    coeff_lrg = 1.0f - std::exp( -1.0f / (0.5f * currentSampleRate));

    //== SMOOTHING
    leftDelay->resetSmoothedValue(0.7f);
    rightDelay->resetSmoothedValue(0.7f);
    smoothedFeedback.reset(currentSampleRate, 0.005f);
    smoothedDryWet.reset(currentSampleRate, 0.005f);
    smoothedLowPass.reset(currentSampleRate, 0.7f);
    smoothedHighPass.reset(currentSampleRate, 0.7f);
    smoothedChorus.reset(currentSampleRate, 77.7f);

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
    bool reverb = false;

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

    //== REVERB DELAY TIMES
    for (size_t i = 0; i < reverbDelaysLeft.size(); ++i)
    {
        reverbDelaysLeft[i]->setNewTarget(fixedDelayTimesLeft[i]);
        reverbDelaysRight[i]->setNewTarget(fixedDelayTimesRight[i]);
    }

    //== PROCESSING LOOP
    for (int channel = 0; channel < numChannels; ++channel)
    {
    const float* inData = buffer.getReadPointer(channel);
    float* outData = buffer.getWritePointer(channel);

        for (int sample = 0; sample < numSamples; ++sample)
        {

            if (channel == 0) //== LEFT CHANNEL
            {
                //== CHORUS & DELAY
                smoothedChorus.skip(sample); 
                float newDelay = applyChorus(sample, smoothedChorus.getCurrentValue(), *leftDelay, newDelayTimeLeft);
                leftDelay->updateDelayTime(newDelay);

                float delayedSample = leftDelay->readBufferDelayedSample();

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
                leftDelay->writeDelayBuffer(inData[sample], feedbackTime, delayedSample);
                float wetScale = (1.0f - dryWetLeft) + dryWetLeft * 0.5;  // making this to control the volume changes when mixing dry/wet signals
                outData[sample] = wetScale * inData[sample] + dryWetLeft * delayedSample;  // dry / wet   //outData[sample] = delayedSample; // 100% wet  // outData[sample] = (1.0f - dryWet) * inData[sample] + dryWet * delayedSample; // original
                leftDelay->updateWriteIndex();

                //== REVERB
                if (reverb)
                {
                    float combinedReverb = applyReverb(reverbDelaysLeft, reverbLowPassLeft, reverbAllPassLeft, inData[sample], dryWetLeft);
                    combinedReverb /= (reverbDelaysLeft.size() / 2); // pseudo normalising
                    outData[sample] += combinedReverb;
                }
            }
            else if (channel == 1) //== RIGHT CHANNEL
            {
                //== CHORUS & DELAY
                smoothedChorus.skip(sample); 
                float newDelay = applyChorus(sample, smoothedChorus.getCurrentValue(), *rightDelay, newDelayTimeRight);
                rightDelay->updateDelayTime(newDelay);

                float delayedSample = rightDelay->readBufferDelayedSample();

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
                rightDelay->writeDelayBuffer(inData[sample], feedbackTime, delayedSample);
                float wetScale = (1.0f - dryWetRight) + dryWetRight * 0.5;
                outData[sample] = wetScale * inData[sample] + dryWetRight * delayedSample; 
                rightDelay->updateWriteIndex();

                //== REVERB
                if (reverb)
                {
                    float combinedReverb = applyReverb(reverbDelaysRight, reverbLowPassRight, reverbAllPassRight, inData[sample], dryWetRight);
                    combinedReverb /= (reverbDelaysRight.size() / 2); // pseudo normalising
                    outData[sample] += combinedReverb;
                }
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

float DelayAudioProcessor::applyChorus(int sample, float currentMixValue, DelayLine& delayLine, float newDelayTime)
{
    chorusModulation = chorusDepth * std::sin(2.0 * juce::MathConstants<float>::pi * chorusRate * sample / currentSampleRate + chorusPhase);
    chorusPhase += 2.0 * juce::MathConstants<float>::pi * chorusRate / currentSampleRate;
    if (chorusPhase > 2.0 * juce::MathConstants<float>::pi)
    {
        chorusPhase -= 2.0 * juce::MathConstants<float>::pi;
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

float DelayAudioProcessor::applyReverb(std::array<std::unique_ptr<DelayLine>, 10>& reverbDelays,
                                       std::array<juce::dsp::IIR::Filter<float>, 10>& reverbLowPass,
                                       std::array<juce::dsp::IIR::Filter<float>, 10>& reverbAllPass,
                                       float sample,
                                       float& dryWet)
{
    float reverbDecay = 0.8f;
    float combinedReverb = 0.0f;

    for (size_t i = 0; i < reverbDelays.size(); ++i)
        {
            float reverb = reverbDelays[i]->getCurrentDelayTime();
            reverb = applyOnePoleFilter(reverb, reverbDelays[i]->getSmoothedNext(), coeff_sml);
            reverbDelays[i]->updateDelayTime(reverb);

            float delayedReverbSample = reverbDelays[i]->readBufferDelayedSample();

            delayedReverbSample = reverbLowPass[i].processSample(delayedReverbSample);
            delayedReverbSample = reverbAllPass[i].processSample(delayedReverbSample);

            reverbDelays[i]->writeDelayBuffer(sample, reverbDecay, delayedReverbSample);
            combinedReverb += dryWet * delayedReverbSample;
            reverbDelays[i]->updateWriteIndex();
            reverbDecay -= 0.025f;
        }

    return combinedReverb;
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

float DelayAudioProcessor::getCurrentBPM()
{
    AudioPlayHead* playHead = getPlayHead();
    AudioPlayHead::CurrentPositionInfo positionInfo;

    if (playHead && playHead->getCurrentPosition(positionInfo)) {
        if (positionInfo.bpm > 0) {
            return positionInfo.bpm;
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
    settings.lowPass = apvts.getRawParameterValue("Low Pass")->load() > 0.5f;
    settings.highPass = apvts.getRawParameterValue("High Pass")->load() > 0.5f;

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
    params.push_back(std::make_unique<juce::AudioParameterBool>("Low Pass", "Low Pass", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("High Pass", "High Pass", false));

    //params.push_back(std::make_unique<juce::AudioParameterChoice>("Divisions", "Divisions", noteStringArray, 0));

    return { params.begin(), params.end() };
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DelayAudioProcessor();
}
