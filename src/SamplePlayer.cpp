#include "SamplePlayer.h"

SamplePlayer::SamplePlayer (int newId)
{
    state.id = newId;
}

void SamplePlayer::setMidiRange (int low, int high) noexcept
{
    low = juce::jlimit (0, 127, low);
    high = juce::jlimit (0, 127, high);
    state.midiLow = juce::jmin (low, high);
    state.midiHigh = juce::jmax (low, high);
}

void SamplePlayer::setGain (float g) noexcept
{
    state.gain = juce::jlimit (0.0f, 2.0f, g);
}

void SamplePlayer::setFilePathAndStatus (const juce::String& path, const juce::String& statusLabel, const juce::String& displayName)
{
    state.filePath = path;
    state.fileName = displayName.isNotEmpty() ? displayName : juce::File (path).getFileName();
    state.status = statusLabel;
}

SamplePlayer::State SamplePlayer::getState() const noexcept
{
    return state;
}

bool SamplePlayer::acceptsNote (int midiNote) const noexcept
{
    return midiNote >= state.midiLow && midiNote <= state.midiHigh && sampleBuffer.getNumSamples() > 0;
}

void SamplePlayer::trigger()
{
    if (sampleBuffer.getNumSamples() > 0)
    {
        playHead = 0;
        state.isPlaying = true;
    }
}

void SamplePlayer::triggerNote (int midiNote)
{
    juce::ignoreUnused (midiNote);
    trigger();
}

float SamplePlayer::getNextSampleForChannel (int channel)
{
    if (! state.isPlaying || sampleBuffer.getNumSamples() == 0)
        return 0.0f;

    const int totalSamples = sampleBuffer.getNumSamples();
    if (playHead >= totalSamples)
    {
        state.isPlaying = false;
        return 0.0f;
    }

    const int numSampleChans = sampleBuffer.getNumChannels();
    const float* src = sampleBuffer.getReadPointer (juce::jmin (channel, numSampleChans - 1), playHead);
    float sample = src[0] * state.gain;

    ++playHead;
    if (playHead >= totalSamples)
        state.isPlaying = false;

    return sample;
}

bool SamplePlayer::setLoadedBuffer (juce::AudioBuffer<float>&& newBuffer, const juce::String& name)
{
    sampleBuffer = std::move (newBuffer);
    state.status = "loaded";
    state.fileName = name;
    // Preserve path if already set, otherwise infer from name.
    if (state.filePath.isEmpty())
        state.filePath = name;
    playHead = 0;
    state.isPlaying = false;
    return true;
}

void SamplePlayer::markError (const juce::String& path, const juce::String& message)
{
    sampleBuffer.setSize (0, 0);
    state.status = "error";
    state.filePath = path;
    state.fileName = message.isNotEmpty() ? message : juce::File (path).getFileName();
}
