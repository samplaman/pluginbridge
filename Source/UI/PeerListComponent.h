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

    PeerListComponent();
    ~PeerListComponent() override;

    void updatePeers(const std::vector<DiscoveredPeer>& peers);
    void setOnConnectCallback(ConnectCallback callback);

    // ListBoxModel methods
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForRow(int rowNumber, bool isRowSelected, juce::Component* existingComponentToUpdate) override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    std::vector<DiscoveredPeer> peers_;
    juce::ListBox listBox_;

    // Manual connect UI
    juce::Label manualLabel_;
    juce::TextEditor ipEditor_;
    juce::TextEditor portEditor_;
    juce::TextButton connectManualBtn_;

    ConnectCallback onConnectCallback_;
    float pulsePhase_ { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PeerListComponent)
};

} // namespace pluginbridge
