#pragma once

#include <JuceHeader.h>

// A lightweight sample player placeholder that will later own audio data.
class SamplePlayer
{
public:
    struct State
    {
        int id {};
        int midiLow { 36 };   // default C2
        int midiHigh { 60 };  // default C4
        float gain { 1.0f };
        bool isPlaying { false };
        juce::String status { "empty" };
        juce::String fileName;
        juce::String filePath;
    };

    explicit SamplePlayer (int newId);

    int getId() const noexcept { return state.id; }

    void setMidiRange (int low, int high) noexcept;
    void setGain (float g) noexcept;
    void setFilePathAndStatus (const juce::String& path, const juce::String& statusLabel, const juce::String& displayName = {});
    State getState() const noexcept;

    bool acceptsNote (int midiNote) const noexcept;
    void trigger();
    void triggerNote (int midiNote);
    float getNextSampleForChannel (int channel);

    bool setLoadedBuffer (juce::AudioBuffer<float>&& newBuffer, const juce::String& name);
    void markError (const juce::String& path, const juce::String& message);

private:
    State state;
    juce::AudioBuffer<float> sampleBuffer;
    int playHead { 0 };
};
