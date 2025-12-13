#pragma once

#include <JuceHeader.h>
#include <mutex>
#include <vector>
#include "SamplePlayer.h"

// Coordinates multiple SamplePlayer instances and exposes a thread-safe API.
class SamplerEngine
{
public:
    SamplerEngine();

    int addSamplePlayer();

    void processBlock (juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi);

    juce::var toVar() const;

    void loadSampleAsync (int playerId, const juce::File& file, std::function<void (bool, juce::String)> onComplete);
    bool setMidiRange (int playerId, int low, int high);
    bool setGain (int playerId, float gain);
    bool trigger (int playerId);

    juce::ValueTree exportToValueTree() const;
    void importFromValueTree (const juce::ValueTree& tree);

private:
    bool loadSampleInternal (int playerId, const juce::File& file, juce::String& error);
    SamplePlayer* getPlayer (int playerId) const;

    std::vector<std::unique_ptr<SamplePlayer>> players;
    mutable std::mutex playerMutex;
    int nextId { 1 };
    juce::AudioFormatManager formatManager;
};
