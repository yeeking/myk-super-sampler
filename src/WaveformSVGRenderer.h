#pragma once

#include <JuceHeader.h>

// Utility to turn an AudioBuffer into a compact SVG waveform preview string.
class WaveformSVGRenderer
{
public:
    static juce::String generateWaveformSVG (const juce::AudioBuffer<float>& buffer,
                                             int numPlotPoints,
                                             float width = 520.0f,
                                             float height = 120.0f);

    static juce::String generateBlankWaveformSVG (float width = 520.0f,
                                                  float height = 120.0f);

private:
    WaveformSVGRenderer() = delete;
};
