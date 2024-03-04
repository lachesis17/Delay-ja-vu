#pragma once

#include "PluginProcessor.h"

template <typename T>
class CircularBuffer
{
public:
	CircularBuffer() {}
	~CircularBuffer() {}

	void flushBuffer(){ memset(&buffer[0], 0, bufferLength * sizeof(T)); }

	void createCircularBuffer(unsigned int _bufferLength)
	{
		createCircularBufferPowerOfTwo((unsigned int)(pow(2, ceil(log(_bufferLength) / log(2)))));
	}

	void createCircularBufferPowerOfTwo(unsigned int _bufferLengthPowerOfTwo)
	{
		writeIndex = 0;
		bufferLength = _bufferLengthPowerOfTwo;
		wrapMask = bufferLength - 1;
		buffer.reset(new T[bufferLength]);
		flushBuffer();
	}

	void writeBuffer(T input)
	{
		buffer[writeIndex++] = input;
		writeIndex &= wrapMask;
	}

	T readBuffer(int delayInSamples)
	{
		int readIndex = (writeIndex - 1) - delayInSamples;
		readIndex &= wrapMask;
		return buffer[readIndex];
	}

	T readBuffer(double delayInFractionalSamples)
	{
		T y1 = readBuffer((int)delayInFractionalSamples);
		if (!interpolate) return y1;
		T y2 = readBuffer((int)delayInFractionalSamples + 1);
		double fraction = delayInFractionalSamples - (int)delayInFractionalSamples;
		return doLinearInterpolation(y1, y2, fraction);
	}

	inline double doLinearInterpolation(double y1, double y2, double fractional_X)
    {
        if (fractional_X >= 1.0) return y2;
        return fractional_X*y2 + (1.0 - fractional_X)*y1;
    }

	void setInterpolate(bool b) { interpolate = b; }

  unsigned int getBufferLength() { return bufferLength; }

private:
	std::unique_ptr<T[]> buffer = nullptr;		///< smart pointer will auto-delete
	unsigned int writeIndex = 0;				///> write index
	unsigned int bufferLength = 1024;			///< must be nearest power of 2
	unsigned int wrapMask = bufferLength - 1;	///< must be (bufferLength - 1)
	bool interpolate = true;					///< interpolation (default is ON)
};

//==============================================================================

class DelayLine
{
public:
	explicit DelayLine(double sampleRate)
	: currentSampleRate(sampleRate), 
	coeff(1.0f - std::exp(-1.0f / (0.1f * sampleRate))) {}

	//==============================================================================

	float getCurrentDelayTime()
	{
		return delayTime;
	}

	void updateDelayTime(float newDelayTime)
	{
		delayTime = newDelayTime;
	}

	void setSampleRate(double newSampleRate)
	{
		currentSampleRate = newSampleRate;
		coeff = 1.0f - std::exp(-1.0f / (0.1f * newSampleRate));
	}

	//==============================================================================

	void resetSmoothedValue(double factor)
	{
		smoothedDelayTime.reset(currentSampleRate, factor);
	}

	float getSmoothedCurrent()
	{
		return smoothedDelayTime.getCurrentValue();
	}

	float getSmoothedNext()
	{
		return smoothedDelayTime.getNextValue();
	}

	float setNewTargetWithSmooth(float delayedSample, SmoothedValue<float, ValueSmoothingTypes::Linear>& smoothedDelayTime, float newDelayTime)
	{
		smoothedDelayTime.setTargetValue(newDelayTime);
		delayedSample = applyOnePoleFilter(delayedSample, smoothedDelayTime.getNextValue(), coeff);
		return delayedSample;
	}

	void setNewTarget(float newDelayTime)
	{
		smoothedDelayTime.setTargetValue(newDelayTime);
	}

	//==============================================================================

	float applyOnePoleFilter(float current, float next, float coefficient)
	{
		return next + ((next - current) * coefficient);
	}

	float applyChorus(int sample, float currentMixValue, float delayedSample, SmoothedValue<float, ValueSmoothingTypes::Linear>& smoothedDelayTime, float newDelayTime)
	{
		chorusModulation = chorusDepth * std::sin(2.0 * juce::MathConstants<float>::pi * chorusRate * sample / currentSampleRate + chorusPhase);
		chorusPhase += 2.0 * juce::MathConstants<float>::pi * chorusRate / currentSampleRate;
		if (chorusPhase > 2.0 * juce::MathConstants<float>::pi)
		{
			chorusPhase -= 2.0 * juce::MathConstants<float>::pi;
		}

		if (newDelayTime != smoothedDelayTime.getCurrentValue() && newDelayTime != 0.f) 
		{
			smoothedDelayTime.setTargetValue(newDelayTime + chorusModulation);
		} 
		else
		{
			smoothedDelayTime.setTargetValue(newDelayTime);
		}

		delayedSample = applyOnePoleFilter(delayedSample, smoothedDelayTime.getNextValue(), coeff);

		if (newDelayTime != 0.f && currentMixValue != 0.f)
		{
			delayedSample += chorusModulation * currentMixValue;
		}

		return delayedSample;
	};

	//==============================================================================

	void makeBuffer()
	{
		circBuff.createCircularBuffer(2 * currentSampleRate);		// double samplerate or limited to 1365ms @ 48k
		circBuff.flushBuffer();
	}

	float readBufferDelayedSample()
	{
		float delayedSample = circBuff.readBuffer(delayTime * currentSampleRate / 1000.0);
		return delayedSample;
	}

	void writeDelayBuffer(float readPointer, float feedback, float delayedSample)
	{
		circBuff.writeBuffer(readPointer + feedback * delayedSample);
	}

	void updateWriteIndex()
	{
		writeIndex = (writeIndex + 1) % circBuff.getBufferLength();
	}

	//==============================================================================

private:
	CircularBuffer<float> circBuff;
	juce::LinearSmoothedValue<float> smoothedDelayTime;
	float delayTime;

	float coeff;
	double currentSampleRate;
	int writeIndex = 0;

	float chorusRate = 0.45f; 
	float chorusDepth = 0.75f;
	float chorusPhase = 0.f;
	float chorusModulation = 0.f;
};