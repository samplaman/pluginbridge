#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Network/NetworkProtocol.h"
#include "Network/BeaconService.h"
#include "Network/AudioSender.h"
#include "Network/AudioReceiver.h"
#include "DSP/RoutingMatrix.h"

namespace pluginbridge
{

class PluginBridgeAudioProcessor : public juce::AudioProcessor
{
public:
    PluginBridgeAudioProcessor();
    ~PluginBridgeAudioProcessor() override;

    // AudioProcessor lifecycle
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // Editor
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // Plugin Info
    const juce::String getName() const override { return "PluginBridge"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // Programs
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    // State persistence
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Subsystems access for UI
    RoutingMatrix& getRoutingMatrix() { return matrix_; }
    AudioSender& getAudioSender() { return sender_; }
    AudioReceiver& getAudioReceiver() { return receiver_; }
    BeaconService& getBeaconService() { return beacon_; }

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts_; }

    // Connection helpers
    void connectToPeer(const DiscoveredPeer& peer);
    void disconnectPeer(const DiscoveredPeer& peer);

    std::string getInstanceName() const { return instanceName_; }
    void setInstanceName(const std::string& name);

    uint16_t getAudioPort() const { return audioPort_; }
    void setAudioPort(uint16_t port);

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts_;

    RoutingMatrix matrix_;
    AudioSender sender_;
    AudioReceiver receiver_;
    BeaconService beacon_;

    std::string instanceUuid_;
    std::string instanceName_ { "Workstation" };
    std::string streamName_ { "Main-LAN" };
    uint16_t audioPort_ { DEFAULT_AUDIO_PORT };

    // Scratch audio buffers for real-time DSP routing
    std::vector<std::vector<float>> netTxBuffers_;
    std::vector<float*> netTxPointers_;

    std::vector<std::vector<float>> netRxBuffers_;
    std::vector<float*> netRxPointers_;

    std::vector<const float*> dawInPointers_;
    std::vector<float*> dawOutPointers_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginBridgeAudioProcessor)
};

} // namespace pluginbridge

