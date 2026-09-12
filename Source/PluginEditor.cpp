#include "PluginEditor.h"

namespace pluginbridge
{

PluginBridgeAudioProcessorEditor::PluginBridgeAudioProcessorEditor(PluginBridgeAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processorRef_(p),
      patchbay_(p.getRoutingMatrix())
{
    setLookAndFeel(&omniLookAndFeel_);

    setSize(1040, 680);
    setResizable(true, true);
    setResizeLimits(880, 560, 1600, 1100);

    // Title label
    addAndMakeVisible(titleLabel_);
    titleLabel_.setText("PLUGINBRIDGE", juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions().withHeight(17.0f).withStyle("Bold")));
    titleLabel_.setColour(juce::Label::textColourId, OmniLookAndFeel::getTextPrimary());

    // Subtitle label
    addAndMakeVisible(subtitleLabel_);
    subtitleLabel_.setText("OMNIBUS LAN AUDIO BUS", juce::dontSendNotification);
    subtitleLabel_.setFont(juce::Font(juce::FontOptions().withHeight(9.0f).withStyle("Bold")));
    subtitleLabel_.setColour(juce::Label::textColourId, OmniLookAndFeel::getAccentCyan().withAlpha(0.8f));

    // Instance Name
    addAndMakeVisible(instanceNameLabel_);
    instanceNameLabel_.setText("INSTANCE", juce::dontSendNotification);
    instanceNameLabel_.setFont(juce::Font(juce::FontOptions().withHeight(9.5f).withStyle("Bold")));
    instanceNameLabel_.setColour(juce::Label::textColourId, OmniLookAndFeel::getTextSecondary());

    addAndMakeVisible(instanceNameEditor_);
    instanceNameEditor_.setText(processorRef_.getInstanceName());
    instanceNameEditor_.onReturnKey = [this] {
        processorRef_.setInstanceName(instanceNameEditor_.getText().toStdString());
    };
    instanceNameEditor_.onFocusLost = [this] {
        processorRef_.setInstanceName(instanceNameEditor_.getText().toStdString());
    };

    // Port Editor
    addAndMakeVisible(portLabel_);
    portLabel_.setText("UDP PORT", juce::dontSendNotification);
    portLabel_.setFont(juce::Font(juce::FontOptions().withHeight(9.5f).withStyle("Bold")));
    portLabel_.setColour(juce::Label::textColourId, OmniLookAndFeel::getTextSecondary());

    addAndMakeVisible(portEditor_);
    portEditor_.setText(juce::String(processorRef_.getAudioPort()));
    portEditor_.onReturnKey = [this] {
        processorRef_.setAudioPort(static_cast<uint16_t>(portEditor_.getText().getIntValue()));
    };

    // Role Box
    addAndMakeVisible(roleBox_);
    roleBox_.addItem("Matrix / Duplex", 1);
    roleBox_.addItem("Sender Only", 2);
    roleBox_.addItem("Receiver Only", 3);
    roleAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef_.getAPVTS(), "role", roleBox_);

    // Latency Box
    addAndMakeVisible(latencyBox_);
    latencyBox_.addItem("64 smp (1.3ms)", 1);
    latencyBox_.addItem("128 smp (2.7ms)", 2);
    latencyBox_.addItem("256 smp (5.3ms)", 3);
    latencyAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processorRef_.getAPVTS(), "packetFrames", latencyBox_);

    // Audio I/O & Device Button
    addAndMakeVisible(btnAudioDevice_);
    btnAudioDevice_.setButtonText("AUDIO I/O");
    btnAudioDevice_.setColour(juce::TextButton::buttonColourId, OmniLookAndFeel::getSurfaceAlt());
    btnAudioDevice_.setColour(juce::TextButton::textColourOffId, OmniLookAndFeel::getAccentCyan());
    btnAudioDevice_.onClick = [this] {
        juce::PopupMenu menu;
        menu.addSectionHeader("AUDIO HARDWARE & HOST STATUS");

        double sr = processorRef_.getSampleRate();
        int bs = processorRef_.getBlockSize();
        menu.addItem(1, "Host Sample Rate: " + (sr > 0 ? juce::String(static_cast<int>(sr)) + " Hz" : "48000 Hz"), false);
        menu.addItem(2, "Host Buffer Size: " + (bs > 0 ? juce::String(bs) + " samples" : "Auto"), false);
        menu.addItem(3, "Bus Configuration: 48 In / 48 Out (Discrete)", false);

        menu.addSeparator();
        menu.addSectionHeader("QUICK AUDIO INTERFACE MAPPING");
        menu.addItem(10, "Reset All Channels to 1:1");
        menu.addItem(11, "Map Inputs to HW Channels 17-32 (Bank 2)");
        menu.addItem(12, "Map Inputs to HW Channels 33-48 (Bank 3)");
        menu.addItem(13, "Map Outputs to HW Channels 17-32 (Bank 2)");
        menu.addItem(14, "Map Outputs to HW Channels 33-48 (Bank 3)");

        menu.addSeparator();
        menu.addItem(20, "Configure Audio Hardware Device...");

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&btnAudioDevice_), [this](int result) {
            if (result == 10)
            {
                processorRef_.getRoutingMatrix().resetInputChannelMap();
                processorRef_.getRoutingMatrix().resetOutputChannelMap();
                patchbay_.repaint();
            }
            else if (result == 11)
            {
                processorRef_.getRoutingMatrix().setInputBank(0, 16, 16);
                patchbay_.repaint();
            }
            else if (result == 12)
            {
                processorRef_.getRoutingMatrix().setInputBank(0, 32, 16);
                patchbay_.repaint();
            }
            else if (result == 13)
            {
                processorRef_.getRoutingMatrix().setOutputBank(0, 16, 16);
                patchbay_.repaint();
            }
            else if (result == 14)
            {
                processorRef_.getRoutingMatrix().setOutputBank(0, 32, 16);
                patchbay_.repaint();
            }
            else if (result == 20)
            {
                bool triggered = false;
                if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
                {
                    for (int i = 0; i < dw->getNumChildComponents(); ++i)
                    {
                        if (auto* btn = dynamic_cast<juce::Button*>(dw->getChildComponent(i)))
                        {
                            if (btn->getButtonText().containsIgnoreCase("Options"))
                            {
                                btn->triggerClick();
                                triggered = true;
                                break;
                            }
                        }
                    }
                }
                if (!triggered)
                {
                    juce::AlertWindow::showMessageBoxAsync(
                        juce::AlertWindow::InfoIcon,
                        "Audio Interface Selection",
                        "When hosted inside a DAW (VST3/AU), hardware soundcards, drivers, and master sample rates are managed by your DAW's Audio Preferences.\n\nUse the patchbay's channel headers (e.g. IN 01 [IF 01], OUT 01 [IF 01]) or the AUDIO IF MAP button to map matrix channels directly to physical audio interface channels.",
                        "OK"
                    );
                }
            }
        });
    };

    // Mode tabs
    addAndMakeVisible(tabTxBtn_);
    tabTxBtn_.setButtonText("DAW IN -> LAN TX");
    tabTxBtn_.onClick = [this] {
        patchbay_.setMode(PatchbayComponent::MatrixMode::DawInToNetworkTx);
        updateTabStyles();
    };

    addAndMakeVisible(tabRxBtn_);
    tabRxBtn_.setButtonText("LAN RX -> DAW OUT");
    tabRxBtn_.onClick = [this] {
        patchbay_.setMode(PatchbayComponent::MatrixMode::NetworkRxToDawOut);
        updateTabStyles();
    };

    addAndMakeVisible(tabThruBtn_);
    tabThruBtn_.setButtonText("DIRECT MONITOR (THRU)");
    tabThruBtn_.onClick = [this] {
        patchbay_.setMode(PatchbayComponent::MatrixMode::DawInToDawOut);
        updateTabStyles();
    };

    // Quick Matrix Presets
    addAndMakeVisible(btnPreset1To1_);
    btnPreset1To1_.setButtonText("1:1 PATCH");
    btnPreset1To1_.onClick = [this] {
        for (int i = 0; i < RoutingMatrix::MATRIX_SIZE; ++i)
        {
            for (int j = 0; j < RoutingMatrix::MATRIX_SIZE; ++j)
            {
                if (patchbay_.getMode() == PatchbayComponent::MatrixMode::DawInToNetworkTx)
                    processorRef_.getRoutingMatrix().setInputToTx(i, j, i == j);
                else if (patchbay_.getMode() == PatchbayComponent::MatrixMode::NetworkRxToDawOut)
                    processorRef_.getRoutingMatrix().setRxToOutput(i, j, i == j);
                else
                    processorRef_.getRoutingMatrix().setInputToOutput(i, j, i == j);
            }
        }
        patchbay_.repaint();
    };

    addAndMakeVisible(btnPresetStereo_);
    btnPresetStereo_.setButtonText("STEREO 1-2");
    btnPresetStereo_.onClick = [this] {
        for (int i = 0; i < RoutingMatrix::MATRIX_SIZE; ++i)
        {
            for (int j = 0; j < RoutingMatrix::MATRIX_SIZE; ++j)
            {
                bool connect = (i < 2 && i == j);
                if (patchbay_.getMode() == PatchbayComponent::MatrixMode::DawInToNetworkTx)
                    processorRef_.getRoutingMatrix().setInputToTx(i, j, connect);
                else if (patchbay_.getMode() == PatchbayComponent::MatrixMode::NetworkRxToDawOut)
                    processorRef_.getRoutingMatrix().setRxToOutput(i, j, connect);
                else
                    processorRef_.getRoutingMatrix().setInputToOutput(i, j, connect);
            }
        }
        patchbay_.repaint();
    };

    addAndMakeVisible(btnClearMatrix_);
    btnClearMatrix_.setButtonText("CLEAR ALL");
    btnClearMatrix_.onClick = [this] {
        for (int i = 0; i < RoutingMatrix::MATRIX_SIZE; ++i)
        {
            for (int j = 0; j < RoutingMatrix::MATRIX_SIZE; ++j)
            {
                if (patchbay_.getMode() == PatchbayComponent::MatrixMode::DawInToNetworkTx)
                    processorRef_.getRoutingMatrix().setInputToTx(i, j, false);
                else if (patchbay_.getMode() == PatchbayComponent::MatrixMode::NetworkRxToDawOut)
                    processorRef_.getRoutingMatrix().setRxToOutput(i, j, false);
                else
                    processorRef_.getRoutingMatrix().setInputToOutput(i, j, false);
            }
        }
        patchbay_.repaint();
    };

    // Central components
    addAndMakeVisible(peerList_);
    peerList_.setLocalDeviceIp(processorRef_.getBeaconService().getLocalIp(), processorRef_.getAudioPort());
    peerList_.setIsConnectedCallback([this](const DiscoveredPeer& peer) {
        return processorRef_.isPeerConnected(peer);
    });
    peerList_.setOnConnectCallback([this](const DiscoveredPeer& peer, bool connect) {
        if (connect)
            processorRef_.connectToPeer(peer);
        else
            processorRef_.disconnectPeer(peer);
    });
    peerList_.setOnPingCallback([this](const std::string& ip, uint16_t port) {
        processorRef_.getBeaconService().addUnicastTarget(ip, port);
        processorRef_.getBeaconService().pingPeer(ip, port);
    });
    peerList_.setOnScanCallback([this] {
        processorRef_.getBeaconService().scanSubnet();
    });

    addAndMakeVisible(patchbay_);

    // Master controls
    addAndMakeVisible(masterGainSlider_);
    masterGainSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
    masterGainSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    masterGainSlider_.setTextValueSuffix(" dB");
    masterGainAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef_.getAPVTS(), "masterGain", masterGainSlider_);

    addAndMakeVisible(masterMuteBtn_);
    masterMuteBtn_.setButtonText("MUTE");
    masterMuteAttachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processorRef_.getAPVTS(), "masterMute", masterMuteBtn_);

    updateTabStyles();

    // 30 Hz refresh timer
    startTimerHz(30);
}

PluginBridgeAudioProcessorEditor::~PluginBridgeAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void PluginBridgeAudioProcessorEditor::updateTabStyles()
{
    auto mode = patchbay_.getMode();

    tabTxBtn_.setColour(juce::TextButton::buttonColourId,
        (mode == PatchbayComponent::MatrixMode::DawInToNetworkTx) ? OmniLookAndFeel::getAccentCyan().withAlpha(0.25f) : OmniLookAndFeel::getSurface());
    tabTxBtn_.setColour(juce::TextButton::textColourOffId,
        (mode == PatchbayComponent::MatrixMode::DawInToNetworkTx) ? OmniLookAndFeel::getAccentCyan() : OmniLookAndFeel::getTextSecondary());

    tabRxBtn_.setColour(juce::TextButton::buttonColourId,
        (mode == PatchbayComponent::MatrixMode::NetworkRxToDawOut) ? OmniLookAndFeel::getAccentAmber().withAlpha(0.25f) : OmniLookAndFeel::getSurface());
    tabRxBtn_.setColour(juce::TextButton::textColourOffId,
        (mode == PatchbayComponent::MatrixMode::NetworkRxToDawOut) ? OmniLookAndFeel::getAccentAmber() : OmniLookAndFeel::getTextSecondary());

    tabThruBtn_.setColour(juce::TextButton::buttonColourId,
        (mode == PatchbayComponent::MatrixMode::DawInToDawOut) ? OmniLookAndFeel::getAccentGreen().withAlpha(0.25f) : OmniLookAndFeel::getSurface());
    tabThruBtn_.setColour(juce::TextButton::textColourOffId,
        (mode == PatchbayComponent::MatrixMode::DawInToDawOut) ? OmniLookAndFeel::getAccentGreen() : OmniLookAndFeel::getTextSecondary());
}

void PluginBridgeAudioProcessorEditor::timerCallback()
{
    patchbay_.updateMeters();
    peerList_.setLocalDeviceIp(processorRef_.getBeaconService().getLocalIp(), processorRef_.getAudioPort());
    peerList_.updatePeers(processorRef_.getBeaconService().getActivePeers());

    currentTxMbps_ = processorRef_.getAudioSender().getCurrentBitrateKbps() / 1000.0f;
    currentRxMbps_ = processorRef_.getAudioReceiver().getBitrateKbps() / 1000.0f;
    currentLoss_ = processorRef_.getAudioReceiver().getPacketLossPercentage();
    currentBufferPkts_ = processorRef_.getAudioReceiver().getBufferFillPackets();
    currentDriftRatio_ = processorRef_.getAudioReceiver().getCurrentSpeedRatio();

    repaint(0, getHeight() - 44, getWidth(), 44);
}

void PluginBridgeAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(OmniLookAndFeel::getBgDark());

    // Top Header Surface
    auto headerRect = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(getWidth()), 56.0f);
    g.setColour(OmniLookAndFeel::getSurface());
    g.fillRect(headerRect);
    g.setColour(OmniLookAndFeel::getBorder());
    g.drawHorizontalLine(56, 0.0f, static_cast<float>(getWidth()));

    // Logo mark (Sleek interconnected waveforms icon)
    float iconX = 14.0f;
    float iconY = 16.0f;
    g.setColour(OmniLookAndFeel::getAccentCyan());
    g.fillRoundedRectangle(iconX, iconY, 24.0f, 24.0f, 5.0f);
    g.setColour(juce::Colour(0xff06080b));
    g.drawLine(iconX + 6.0f, iconY + 12.0f, iconX + 18.0f, iconY + 12.0f, 2.0f);
    g.drawLine(iconX + 12.0f, iconY + 6.0f, iconX + 12.0f, iconY + 18.0f, 2.0f);
    g.drawEllipse(iconX + 8.0f, iconY + 8.0f, 8.0f, 8.0f, 1.5f);

    // Network Status Pill in Header
    auto statusPill = juce::Rectangle<float>(static_cast<float>(getWidth() - 114), 16.0f, 98.0f, 24.0f);
    g.setColour(OmniLookAndFeel::getSurfaceAlt());
    g.fillRoundedRectangle(statusPill, 12.0f);
    g.setColour(OmniLookAndFeel::getBorder());
    g.drawRoundedRectangle(statusPill, 12.0f, 1.0f);

    g.setColour(OmniLookAndFeel::getAccentGreen());
    g.fillEllipse(statusPill.getX() + 8.0f, statusPill.getCentreY() - 3.5f, 7.0f, 7.0f);

    g.setFont(juce::Font(juce::FontOptions().withHeight(10.0f).withStyle("Bold")));
    g.setColour(OmniLookAndFeel::getTextPrimary());
    g.drawText("LAN READY", static_cast<int>(statusPill.getX() + 18.0f), static_cast<int>(statusPill.getY()), 74, 24, juce::Justification::centred);

    // Tab bar divider
    g.setColour(OmniLookAndFeel::getBorder());
    g.drawHorizontalLine(96, 0.0f, static_cast<float>(getWidth()));

    // Bottom Diagnostics HUD Bar
    auto hudRect = juce::Rectangle<float>(0.0f, static_cast<float>(getHeight() - 44), static_cast<float>(getWidth()), 44.0f);
    g.setColour(OmniLookAndFeel::getSurface());
    g.fillRect(hudRect);
    g.setColour(OmniLookAndFeel::getBorder());
    g.drawHorizontalLine(getHeight() - 44, 0.0f, static_cast<float>(getWidth()));

    // Render Diagnostic HUD metrics (compact & clean)
    float x = 12.0f;
    auto drawHudCard = [&](const juce::String& title, const juce::String& val, const juce::Colour& valCol, float cardW) {
        auto card = juce::Rectangle<float>(x, static_cast<float>(getHeight() - 37), cardW, 30.0f);
        g.setColour(OmniLookAndFeel::getSurfaceAlt());
        g.fillRoundedRectangle(card, 4.0f);
        g.setColour(OmniLookAndFeel::getBorder());
        g.drawRoundedRectangle(card, 4.0f, 1.0f);

        g.setFont(juce::Font(juce::FontOptions().withHeight(8.0f).withStyle("Bold")));
        g.setColour(OmniLookAndFeel::getTextSecondary());
        g.drawText(title, card.reduced(5, 2).removeFromTop(11).toNearestInt(), juce::Justification::topLeft);

        g.setFont(juce::Font(juce::FontOptions().withHeight(10.5f).withStyle("Bold")));
        g.setColour(valCol);
        g.drawText(val, card.reduced(5, 2).removeFromBottom(15).toNearestInt(), juce::Justification::bottomLeft);

        x += cardW + 6.0f;
    };

    drawHudCard("LAN TX STREAM", juce::String(currentTxMbps_, 1) + " Mbps", OmniLookAndFeel::getAccentCyan(), 86.0f);
    drawHudCard("LAN RX STREAM", juce::String(currentRxMbps_, 1) + " Mbps", OmniLookAndFeel::getAccentAmber(), 86.0f);
    drawHudCard("BUFFER DEPTH", juce::String(currentBufferPkts_, 1) + " pkts", OmniLookAndFeel::getTextPrimary(), 84.0f);

    juce::String driftText = (std::abs(currentDriftRatio_ - 1.0f) < 0.0005f) ? "LOCKED" : juce::String(currentDriftRatio_, 4) + "x";
    drawHudCard("CLOCK DRIFT", driftText, OmniLookAndFeel::getAccentGreen(), 80.0f);

    juce::Colour lossCol = (currentLoss_ < 0.1f) ? OmniLookAndFeel::getAccentGreen() : OmniLookAndFeel::getAccentRed();
    drawHudCard("PACKET LOSS", juce::String(currentLoss_, 1) + "%", lossCol, 76.0f);
}

void PluginBridgeAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Top Header (56px)
    auto header = bounds.removeFromTop(56).reduced(12, 8);
    header.removeFromRight(116); // Protected space for status pill

    header.removeFromLeft(34); // Space for logo icon

    titleLabel_.setBounds(header.removeFromLeft(115).removeFromTop(20));
    subtitleLabel_.setBounds(12 + 34, 32, 130, 14);

    header.removeFromLeft(14);

    // Instance label + editor
    auto instSection = header.removeFromLeft(110).reduced(2, 0);
    instanceNameLabel_.setBounds(instSection.removeFromTop(12));
    instanceNameEditor_.setBounds(instSection.reduced(0, 2));

    // Port label + editor
    auto portSection = header.removeFromLeft(66).reduced(2, 0);
    portLabel_.setBounds(portSection.removeFromTop(12));
    portEditor_.setBounds(portSection.reduced(0, 2));

    header.removeFromLeft(8);

    // Role & Latency combo boxes
    roleBox_.setBounds(header.removeFromLeft(120).withSizeKeepingCentre(120, 28));
    header.removeFromLeft(6);
    latencyBox_.setBounds(header.removeFromLeft(120).withSizeKeepingCentre(120, 28));

    header.removeFromLeft(8);
    btnAudioDevice_.setBounds(header.removeFromLeft(84).withSizeKeepingCentre(84, 26));

    // Tab Bar (40px)
    auto tabBounds = bounds.removeFromTop(40).reduced(8, 4);
    tabTxBtn_.setBounds(tabBounds.removeFromLeft(135).reduced(2));
    tabRxBtn_.setBounds(tabBounds.removeFromLeft(145).reduced(2));
    tabThruBtn_.setBounds(tabBounds.removeFromLeft(155).reduced(2));

    // Quick presets on right of tab bar
    btnClearMatrix_.setBounds(tabBounds.removeFromRight(72).reduced(2));
    btnPresetStereo_.setBounds(tabBounds.removeFromRight(82).reduced(2));
    btnPreset1To1_.setBounds(tabBounds.removeFromRight(78).reduced(2));

    // Bottom Bar (44px)
    auto bottomBar = bounds.removeFromBottom(44).reduced(10, 4);
    masterMuteBtn_.setBounds(bottomBar.removeFromRight(62).reduced(2, 3));
    bottomBar.removeFromRight(8);
    masterGainSlider_.setBounds(bottomBar.removeFromRight(150));

    // Center Area: Peer List (left ~300px) and Patchbay (rest)
    int peerWidth = 300;
    peerList_.setBounds(bounds.removeFromLeft(peerWidth));
    patchbay_.setBounds(bounds);
}

} // namespace pluginbridge
