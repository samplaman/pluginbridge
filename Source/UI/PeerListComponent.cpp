#include "PeerListComponent.h"
#include "OmniLookAndFeel.h"
#include <cmath>

namespace pluginbridge
{

class PeerRowComponent : public juce::Component
{
public:
    explicit PeerRowComponent(PeerListComponent::ConnectCallback callback)
        : callback_(std::move(callback))
    {
        addAndMakeVisible(connectBtn_);
        connectBtn_.onClick = [this] {
            if (callback_)
            {
                isCurrentlyConnected_ = !isCurrentlyConnected_;
                updateButtonState();
                callback_(peer_, isCurrentlyConnected_);
            }
        };
    }

    void update(const DiscoveredPeer& peer, bool isConnected)
    {
        peer_ = peer;
        isCurrentlyConnected_ = isConnected;
        updateButtonState();
        repaint();
    }

    void updateButtonState()
    {
        if (isCurrentlyConnected_)
        {
            connectBtn_.setButtonText("LINKED");
            connectBtn_.setColour(juce::TextButton::buttonColourId, OmniLookAndFeel::getAccentGreen().withAlpha(0.25f));
            connectBtn_.setColour(juce::TextButton::textColourOffId, OmniLookAndFeel::getAccentGreen());
        }
        else
        {
            connectBtn_.setButtonText("CONNECT");
            connectBtn_.setColour(juce::TextButton::buttonColourId, OmniLookAndFeel::getSurfaceAlt());
            connectBtn_.setColour(juce::TextButton::textColourOffId, OmniLookAndFeel::getAccentCyan());
        }
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced(4.0f, 3.0f);

        // Card body
        g.setColour(OmniLookAndFeel::getSurface());
        g.fillRoundedRectangle(bounds, 6.0f);

        g.setColour(isCurrentlyConnected_ ? OmniLookAndFeel::getAccentGreen().withAlpha(0.5f) : OmniLookAndFeel::getBorder());
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

        // Active status dot
        float dotX = bounds.getX() + 14.0f;
        float dotY = bounds.getY() + 16.0f;
        g.setColour(OmniLookAndFeel::getAccentGreen().withAlpha(0.3f));
        g.fillEllipse(dotX - 5.0f, dotY - 5.0f, 10.0f, 10.0f);
        g.setColour(OmniLookAndFeel::getAccentGreen());
        g.fillEllipse(dotX - 3.0f, dotY - 3.0f, 6.0f, 6.0f);

        // Instance Name (Bold White)
        g.setFont(juce::Font(juce::FontOptions().withHeight(13.0f).withStyle("Bold")));
        g.setColour(OmniLookAndFeel::getTextPrimary());
        g.drawText(peer_.instanceName, static_cast<int>(bounds.getX() + 26.0f), static_cast<int>(bounds.getY() + 6.0f),
                   static_cast<int>(bounds.getWidth() - 110.0f), 18, juce::Justification::centredLeft);

        // Host machine badge
        g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        g.setColour(OmniLookAndFeel::getAccentCyan().withAlpha(0.85f));
        g.drawText(peer_.hostName, static_cast<int>(bounds.getX() + 26.0f), static_cast<int>(bounds.getY() + 25.0f),
                   static_cast<int>(bounds.getWidth() - 110.0f), 14, juce::Justification::centredLeft);

        // Details line (IP : Port | Channels)
        juce::String details = juce::String(peer_.ipAddress) + ":" + juce::String(peer_.audioPort)
                             + "  |  " + juce::String(peer_.numChannels) + " CH"
                             + " (" + juce::String(peer_.sampleRate / 1000) + "k)";
        g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f)));
        g.setColour(OmniLookAndFeel::getTextSecondary());
        g.drawText(details, static_cast<int>(bounds.getX() + 26.0f), static_cast<int>(bounds.getY() + 41.0f),
                   static_cast<int>(bounds.getWidth() - 110.0f), 14, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(8, 12);
        connectBtn_.setBounds(bounds.removeFromRight(74).withSizeKeepingCentre(74, 28));
    }

private:
    DiscoveredPeer peer_;
    bool isCurrentlyConnected_ { false };
    PeerListComponent::ConnectCallback callback_;
    juce::TextButton connectBtn_;
};

PeerListComponent::PeerListComponent()
{
    addAndMakeVisible(listBox_);
    listBox_.setModel(this);
    listBox_.setRowHeight(64);
    listBox_.setColour(juce::ListBox::backgroundColourId, OmniLookAndFeel::getBgDark());

    addAndMakeVisible(manualLabel_);
    manualLabel_.setText("MANUAL PEER IP", juce::dontSendNotification);
    manualLabel_.setFont(juce::Font(juce::FontOptions().withHeight(10.0f).withStyle("Bold")));
    manualLabel_.setColour(juce::Label::textColourId, OmniLookAndFeel::getTextSecondary());

    addAndMakeVisible(ipEditor_);
    ipEditor_.setText("192.168.1.50");
    ipEditor_.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));

    addAndMakeVisible(portEditor_);
    portEditor_.setText("52801");
    portEditor_.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));

    addAndMakeVisible(connectManualBtn_);
    connectManualBtn_.setButtonText("+ ADD");
    connectManualBtn_.setColour(juce::TextButton::buttonColourId, OmniLookAndFeel::getAccentCyan().withAlpha(0.2f));
    connectManualBtn_.setColour(juce::TextButton::textColourOffId, OmniLookAndFeel::getAccentCyan());
    connectManualBtn_.onClick = [this] {
        DiscoveredPeer manualPeer;
        manualPeer.instanceName = "Manual Peer";
        manualPeer.hostName = ipEditor_.getText().toStdString();
        manualPeer.ipAddress = ipEditor_.getText().toStdString();
        manualPeer.audioPort = static_cast<uint16_t>(portEditor_.getText().getIntValue());
        manualPeer.numChannels = 16;
        manualPeer.sampleRate = 48000;
        manualPeer.streamName = "Direct";

        peers_.push_back(manualPeer);
        listBox_.updateContent();

        if (onConnectCallback_)
            onConnectCallback_(manualPeer, true);
    };

    startTimerHz(20); // 20fps for subtle scanner pulse
}

PeerListComponent::~PeerListComponent()
{
    stopTimer();
}

void PeerListComponent::timerCallback()
{
    pulsePhase_ += 0.08f;
    if (pulsePhase_ > 6.283185f)
        pulsePhase_ -= 6.283185f;

    if (peers_.empty())
        repaint();
}

void PeerListComponent::updatePeers(const std::vector<DiscoveredPeer>& peers)
{
    peers_ = peers;
    listBox_.updateContent();
    repaint();
}

void PeerListComponent::setOnConnectCallback(ConnectCallback callback)
{
    onConnectCallback_ = std::move(callback);
}

int PeerListComponent::getNumRows()
{
    return static_cast<int>(peers_.size());
}

void PeerListComponent::paintListBoxItem(int /*rowNumber*/, juce::Graphics& /*g*/, int /*width*/, int /*height*/, bool /*rowIsSelected*/)
{
}

juce::Component* PeerListComponent::refreshComponentForRow(int rowNumber, bool /*isRowSelected*/, juce::Component* existingComponentToUpdate)
{
    auto* rowComp = dynamic_cast<PeerRowComponent*>(existingComponentToUpdate);
    if (rowComp == nullptr)
    {
        rowComp = new PeerRowComponent(onConnectCallback_);
    }

    if (rowNumber >= 0 && rowNumber < static_cast<int>(peers_.size()))
    {
        rowComp->update(peers_[static_cast<size_t>(rowNumber)], peers_[static_cast<size_t>(rowNumber)].isConnected);
    }
    return rowComp;
}

void PeerListComponent::paint(juce::Graphics& g)
{
    g.fillAll(OmniLookAndFeel::getBgDark());

    // Header banner
    auto headerRect = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(getWidth()), 32.0f);
    g.setColour(OmniLookAndFeel::getSurface());
    g.fillRect(headerRect);
    g.setColour(OmniLookAndFeel::getBorder());
    g.drawHorizontalLine(32, 0.0f, static_cast<float>(getWidth()));
    g.drawVerticalLine(getWidth() - 1, 0.0f, static_cast<float>(getHeight()));

    // Title text
    g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f).withStyle("Bold")));
    g.setColour(OmniLookAndFeel::getTextPrimary());
    g.drawText("LAN PEER DISCOVERY", 12, 0, getWidth() - 70, 32, juce::Justification::centredLeft);

    // Count badge
    auto badgeRect = juce::Rectangle<float>(static_cast<float>(getWidth() - 36), 7.0f, 24.0f, 18.0f);
    g.setColour(OmniLookAndFeel::getSurfaceAlt());
    g.fillRoundedRectangle(badgeRect, 3.0f);
    g.setColour(OmniLookAndFeel::getAccentCyan());
    g.drawText(juce::String(peers_.size()), badgeRect.toNearestInt(), juce::Justification::centred);

    // Empty state animation when searching
    if (peers_.empty())
    {
        float cx = static_cast<float>(getWidth()) * 0.5f;
        float cy = (static_cast<float>(getHeight()) - 70.0f) * 0.45f;

        // Concentric sonar rings
        for (int ring = 1; ring <= 3; ++ring)
        {
            float fRing = static_cast<float>(ring);
            float baseR = fRing * 24.0f;
            float pulse = std::sin(pulsePhase_ + fRing * 0.8f) * 3.0f;
            float r = baseR + pulse;
            float alpha = 0.18f / fRing;
            g.setColour(OmniLookAndFeel::getAccentCyan().withAlpha(alpha));
            g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 1.0f);
        }

        g.setColour(OmniLookAndFeel::getAccentCyan().withAlpha(0.8f));
        g.fillEllipse(cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);

        g.setFont(juce::Font(juce::FontOptions().withHeight(12.0f).withStyle("Bold")));
        g.setColour(OmniLookAndFeel::getTextPrimary());
        g.drawText("SCANNING LOCAL NETWORK", 0, static_cast<int>(cy + 40.0f), getWidth(), 20, juce::Justification::centred);

        g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f)));
        g.setColour(OmniLookAndFeel::getTextSecondary());
        g.drawText("Searching for DAWs running PluginBridge...", 10, static_cast<int>(cy + 60.0f), getWidth() - 20, 20, juce::Justification::centred);
    }
}

void PeerListComponent::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromTop(32); // Header banner

    // Manual connect bar at bottom
    auto bottomSection = bounds.removeFromBottom(60);
    auto bottomBar = bottomSection.reduced(6, 4);

    manualLabel_.setBounds(bottomBar.removeFromTop(16));

    auto inputsBar = bottomBar.reduced(0, 2);
    connectManualBtn_.setBounds(inputsBar.removeFromRight(56));
    portEditor_.setBounds(inputsBar.removeFromRight(54).reduced(2, 0));
    ipEditor_.setBounds(inputsBar.reduced(2, 0));

    listBox_.setBounds(bounds);
}

} // namespace pluginbridge
