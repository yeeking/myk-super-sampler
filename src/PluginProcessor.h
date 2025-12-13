#pragma once
// #include <juce_audio_processors/juce_audio_processors.h>
#include <JuceHeader.h>


// #define CPPHTTPLIB_OPENSSL_SUPPORT
#include "HTTPServer.h"
#include "SamplerEngine.h"


//==============================================================================
class PluginProcessor final : public juce::AudioProcessor
{
public:
    //==============================================================================
    PluginProcessor();
    ~PluginProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // things for the api server to call
    // really you should make an 'interface' for this 
    // but for simplicity in this demo
    void messageReceivedFromWebAPI(std::string msg);
    void addSamplePlayerFromWeb();
    void sendSamplerStateToUI();
    void requestSampleLoadFromWeb (int playerId);
    juce::var getSamplerState() const;
    void setSampleRangeFromWeb (int playerId, int low, int high);
    void triggerFromWeb (int playerId);

private:
    HttpServerThread apiServer;
    SamplerEngine sampler;
    juce::AudioProcessorValueTreeState apvts;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void broadcastMessage (const juce::String& msg);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
