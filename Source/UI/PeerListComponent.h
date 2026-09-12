#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../Network/NetworkProtocol.h"
#include <vector>
#include <functional>

namespace pluginbridge
{

class PeerListComponent : public juce::Component,
                          public juce::ListBoxModel,
                          public juce::Timer
{
public:
    using ConnectCallback = std::function<void(const DiscoveredPeer&, bool connect)>;
    using IsConnectedCallback = std::function<bool(const DiscoveredPeer&)>;
    using PingCallback = std::function<void(const std::string& ip, uint16_t port)>;
    using ScanCallback = std::function<void()>;

    PeerListComponent();
    ~PeerListComponent() override;

    void updatePeers(const std::vector<DiscoveredPeer>& peers);
    void setOnConnectCallback(ConnectCallback callback);
    void setIsConnectedCallback(IsConnectedCallback callback);
    void setOnPingCallback(PingCallback callback);
    void setOnScanCallback(ScanCallback callback);
    void setLocalDeviceIp(const juce::String& ip, uint16_t port);

    // ListBoxModel methods
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForRow(int rowNumber, bool isRowSelected, juce::Component* existingComponentToUpdate) override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    std::vector<DiscoveredPeer> peers_;
    std::vector<DiscoveredPeer> manualPeers_;
    juce::ListBox listBox_;

    // Local device info banner
    juce::Label localIpBannerLabel_;

    // Manual connect UI
    juce::Label manualLabel_;
    juce::TextEditor ipEditor_;
    juce::TextEditor portEditor_;
    juce::TextButton connectManualBtn_;

    juce::TextButton scanSubnetBtn_;

    ConnectCallback onConnectCallback_;
    IsConnectedCallback isConnectedCallback_;
    PingCallback onPingCallback_;
    ScanCallback onScanCallback_;
    float pulsePhase_ { 0.0f };
    juce::String localIpString_ { "127.0.0.1" };
    uint16_t localPort_ { DEFAULT_AUDIO_PORT };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PeerListComponent)
};

} // namespace pluginbridge
