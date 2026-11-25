#pragma once

#include "PluginProcessor.h"
#include "DelayLine.h"

//==============================================================================
// stereo reverb:
// mono in -> high pass -> pre-delay -> 8 combs dampened -> 4 series all pass -> width -> out
// one set of combs + all pass per side, right side 0.52 ms longer so tails differ
//==============================================================================

class ReverbLines {
public:
    //== REVERB SETTINGS
    static constexpr float decaySeconds = 8.0f;         // time for tail to fall 60dB (setDecay() changes)
    static constexpr float maxDecaySeconds = 16.0f;     // also tail length told to host
    static constexpr float roomSize = 1.15f;            // scales comb lengths, bigger = larger space, slower = grainier build-up
    static constexpr float decayCompensation = 0.25f;   // 0 = longer decays get louder on sustained sound, 0.5 = same loudness
    static constexpr float outputGain = 3.0f;           // wet gain
    static constexpr float dampingHz = 6000.f;          // lower = darker tail
    static constexpr float preDelayMs = 20.f;           // gap between dry and reverb
    static constexpr float inputHighPassHz = 120.f;     // keep bass out of tail
    static constexpr float width = 1.0f;                // 0 = mono tail, 1 = full stereo
    static constexpr float reverbModDepth = 0.25f;      // ms of comb wobble

    // damping coeff with same exp. smoothing, with dampingHz for decay
    ReverbLines(double sampleRate) : currentSampleRate(sampleRate),
    coeff(1.0f - static_cast<float>(std::exp(-2.0 * juce::MathConstants<double>::pi * dampingHz / sampleRate)))
    {
        setupDelaysAndFilters();
    }

    void setupDelaysAndFilters()
    {
        // COMBS
        for (size_t i = 0; i < reverbCombsLeft.size(); ++i) {
            reverbCombsLeft[i].setup(currentSampleRate, fixedDelayTimesLeft[i] * roomSize, 0.31f + 0.083f * static_cast<float>(i));
            reverbCombsRight[i].setup(currentSampleRate, fixedDelayTimesRight[i] * roomSize, 0.37f + 0.077f * static_cast<float>(i));
        }
        setDecay(decaySeconds);

        // ALL PASS
        for (size_t i = 0; i < reverbAllPassLeft.size(); ++i) {
            reverbAllPassLeft[i].setup(currentSampleRate, fixedAllPassTimesLeft[i]);
            reverbAllPassRight[i].setup(currentSampleRate, fixedAllPassTimesRight[i]);
        }

        // PRE-DELAY + HIGH PASS
        preDelaySamples = preDelayMs * currentSampleRate / 1000.0;
        reverbPreDelay.createCircularBuffer(static_cast<unsigned int>(preDelaySamples) + 4);
        highPassCoeff = 1.0f - static_cast<float>(std::exp(-2.0 * juce::MathConstants<double>::pi * inputHighPassHz / currentSampleRate));

        reset();
    }

    void reset()
    {
        const float twoPi = juce::MathConstants<float>::twoPi;
        for (size_t i = 0; i < reverbCombsLeft.size(); ++i) {
            const float startPhase = twoPi * static_cast<float>(i) / static_cast<float>(numCombs);
            reverbCombsLeft[i].reset(startPhase);
            reverbCombsRight[i].reset(startPhase + 0.25f * twoPi);
        }

        for (size_t i = 0; i < reverbAllPassLeft.size(); ++i) {
            reverbAllPassLeft[i].reset();
            reverbAllPassRight[i].reset();
        }

        reverbPreDelay.flushBuffer();
        reverbHighPass = 0.f;
    }

    void setDecay(float seconds)
    {
        seconds = std::clamp(seconds, 0.1f, maxDecaySeconds);
        if (seconds == currentDecaySeconds)
            return;

        currentDecaySeconds = seconds;
        for (size_t i = 0; i < reverbCombsLeft.size(); ++i) {
            reverbCombsLeft[i].setDecay(seconds);
            reverbCombsRight[i].setDecay(seconds);
        }

        currentInputGain = inputGain * std::pow(referenceDecaySeconds / seconds, decayCompensation);
    }

    static float applyOnePoleFilter(float current, float next, float coefficient)
    {
        return current + ((next - current) * coefficient);
    }

    // stereo in, wet stereo out
    void applyReverb(float inLeft, float inRight, float& outLeft, float& outRight)
    {
        //== MONO IN + HIGH PASS
        float input = 0.5f * (inLeft + inRight) * currentInputGain;
        reverbHighPass = applyOnePoleFilter(reverbHighPass, input, highPassCoeff);     // low pass of input...
        input -= reverbHighPass;                                                        // ...taken away leaves highs

        //== PRE-DELAY
        float delayedInput = reverbPreDelay.readBuffer(preDelaySamples);
        reverbPreDelay.writeBuffer(input);      // no feedback, just delay

        //== COMBS + ALL PASS
        float left = applyReverbSide(true, delayedInput);
        float right = applyReverbSide(false, delayedInput);

        //== WIDTH  (blend some of each side into the other)
        const float wet1 = 0.5f + 0.5f * width;
        const float wet2 = 0.5f - 0.5f * width;
        outLeft = left * wet1 + right * wet2;
        outRight = right * wet1 + left * wet2;

        // yeet NaN out of feedback loop
        if (! std::isfinite(outLeft) || ! std::isfinite(outRight))
        {
            reset();
            outLeft = 0.f;
            outRight = 0.f;
        }
    }

private:
    float applyReverbSide(bool left, float input)
    {
        std::array<Comb, numCombs>& reverbCombs = left ? reverbCombsLeft : reverbCombsRight;
        std::array<AllPass, numAllPasses>& reverbAllPass = left ? reverbAllPassLeft : reverbAllPassRight;

        float combinedReverb = 0.0f;
        for (Comb& comb : reverbCombs)
            combinedReverb += comb.process(input, coeff);

        //== ALL PASS (schroeder)
        for (AllPass& allPass : reverbAllPass)
            combinedReverb = allPass.process(combinedReverb);

        return combinedReverb * outputGain;
    }

    static constexpr size_t numCombs = 8;
    static constexpr size_t numAllPasses = 4;
    static constexpr float inputGain = 0.03f;   // each comb stacks up so feed quietly
    static constexpr float referenceDecaySeconds = 3.f;
    static constexpr float allPassGain = 0.5f;  // how much each allpass smears

    //==============================================================================
    struct Comb
    {
        CircularBuffer<float> buffer;
        double sampleRate = 44100.0;
        float delayTime = 0.f;          // ms
        float reverbDecay = 0.f;        // feedback per loop
        float reverbLowPass = 0.f;      // damping filter memory
        float reverbModPhase = 0.f;     // LFO position (radians)
        float reverbModRate = 0.f;      // LFO phase per sample

        void setup(double newSampleRate, float timeMs, float modRateHz)
        {
            sampleRate = newSampleRate;
            delayTime = timeMs;
            reverbModRate = juce::MathConstants<float>::twoPi * modRateHz / static_cast<float>(sampleRate);

            // longest LFO can, plus interpolation room
            buffer.createCircularBuffer(static_cast<unsigned int>((delayTime + reverbModDepth) * sampleRate / 1000.0) + 4);
        }

        // feedback so the comb falls 60 dB (x0.001) in seconds. longer combs loop fewer times, so they keep more per loop
        void setDecay(float seconds)
        {
            reverbDecay = std::pow(10.f, -3.f * delayTime / (seconds * 1000.f));
        }

        void reset(float startPhase)
        {
            buffer.flushBuffer();
            reverbLowPass = 0.f;
            reverbModPhase = startPhase;
        }

        float process(float input, float dampingCoeff)
        {
            //== LFO
            float reverb = delayTime + reverbModDepth * std::sin(reverbModPhase);
            reverbModPhase += reverbModRate;
            if (reverbModPhase >= juce::MathConstants<float>::twoPi)
                reverbModPhase -= juce::MathConstants<float>::twoPi;

            float delayedReverbSample = buffer.readBuffer(reverb * sampleRate / 1000.0);

            // damping highs lose a little every loop, so tail darkens
            reverbLowPass = applyOnePoleFilter(reverbLowPass, delayedReverbSample, dampingCoeff);

            buffer.writeBuffer(input + reverbDecay * reverbLowPass);
            return delayedReverbSample;
        }
    };

    //==============================================================================
    struct AllPass
    {
        CircularBuffer<float> buffer;
        int allPassSamples = 1;

        void setup(double sampleRate, float timeMs)
        {
            allPassSamples = static_cast<int>(std::lround(timeMs * sampleRate / 1000.0));
            buffer.createCircularBuffer(static_cast<unsigned int>(allPassSamples));
        }

        void reset()
        {
            buffer.flushBuffer();
        }

        float process(float input)
        {
            float delayedAllPass = buffer.readBuffer(allPassSamples - 1);     // readBuffer(n) is n + 1 samples ago
            float toBuffer = input + allPassGain * delayedAllPass;
            buffer.writeBuffer(toBuffer);
            return delayedAllPass - allPassGain * toBuffer;
        }
    };

    //==============================================================================
    double currentSampleRate;
    float coeff;
    float currentDecaySeconds = 0.f;
    float currentInputGain = inputGain;

    const std::array<float, numCombs> fixedDelayTimesLeft = {25.306f, 26.939f, 28.957f, 30.748f, 32.245f, 33.810f, 35.306f, 36.667f};
    const std::array<float, numCombs> fixedDelayTimesRight = {25.828f, 27.460f, 29.478f, 31.270f, 32.766f, 34.331f, 35.828f, 37.188f};
    std::array<Comb, numCombs> reverbCombsLeft;
    std::array<Comb, numCombs> reverbCombsRight;

    const std::array<float, numAllPasses> fixedAllPassTimesLeft = {12.608f, 10.000f, 7.732f, 5.102f};
    const std::array<float, numAllPasses> fixedAllPassTimesRight = {13.129f, 10.522f, 8.254f, 5.624f};
    std::array<AllPass, numAllPasses> reverbAllPassLeft;
    std::array<AllPass, numAllPasses> reverbAllPassRight;

    CircularBuffer<float> reverbPreDelay;
    double preDelaySamples = 0.0;
    float highPassCoeff = 0.f;
    float reverbHighPass = 0.f;
};
