#include "SamplePlayer.h"
#include "WaveformSVGRenderer.h"

SamplePlayer::SamplePlayer (int newId)
{
    state.id = newId;
    state.waveformSVG = WaveformSVGRenderer::generateBlankWaveformSVG();
    vuBuffer.assign ((size_t) vuBufferSize, 0.0f);
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

    if (channel == 0)
        pushVuSample (sample);

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
    state.waveformSVG = WaveformSVGRenderer::generateWaveformSVG (sampleBuffer, 320);
    vuBuffer.assign ((size_t) vuBufferSize, 0.0f);
    vuWritePos = 0;
    vuSum = 0.0f;
    lastVuDb = -60.0f;
    return true;
}

void SamplePlayer::markError (const juce::String& path, const juce::String& message)
{
    sampleBuffer.setSize (0, 0);
    state.status = "error";
    state.filePath = path;
    state.fileName = message.isNotEmpty() ? message : juce::File (path).getFileName();
    state.waveformSVG = WaveformSVGRenderer::generateBlankWaveformSVG();
    vuBuffer.assign ((size_t) vuBufferSize, 0.0f);
    vuWritePos = 0;
    vuSum = 0.0f;
    lastVuDb = -60.0f;
}

void SamplePlayer::beginBlock() noexcept
{
    // no-op for now; samples are pushed per-sample
}

void SamplePlayer::endBlock() noexcept
{
    const float average = vuBufferSize > 0 ? (vuSum / (float) vuBufferSize) : 0.0f;
    float db = juce::Decibels::gainToDecibels (average + 1.0e-6f, -80.0f);
    if (db < lastVuDb) {// hold peaks a bit
        db = (lastVuDb + db) / 2.0f; 
    }
    lastVuDb = juce::jlimit (-60.0f, 6.0f, db);
}

void SamplePlayer::pushVuSample (float sample) noexcept
{
    if (vuBuffer.empty())
        return;

    const float mag = std::abs (sample);
    vuSum -= vuBuffer[(size_t) vuWritePos];
    vuBuffer[(size_t) vuWritePos] = mag;
    vuSum += mag;
    vuWritePos = (vuWritePos + 1) % vuBufferSize;
}
