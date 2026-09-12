#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "UI/PatchbayComponent.h"
#include "UI/PeerListComponent.h"
#include "UI/OmniLookAndFeel.h"

namespace pluginbridge
{

class PluginBridgeAudioProcessorEditor : public juce::AudioProcessorEditor,
                                         public juce::Timer
{
public:
    explicit PluginBridgeAudioProcessorEditor(PluginBridgeAudioProcessor&);
    ~PluginBridgeAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    PluginBridgeAudioProcessor& processorRef_;
    OmniLookAndFeel omniLookAndFeel_;

    // Header controls
    juce::Label titleLabel_;
    juce::Label subtitleLabel_;
    juce::Label instanceNameLabel_;
    juce::TextEditor instanceNameEditor_;
    juce::Label portLabel_;
    juce::TextEditor portEditor_;

    juce::ComboBox roleBox_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> roleAttachment_;

    juce::ComboBox latencyBox_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> latencyAttachment_;

    juce::TextButton btnAudioDevice_;
    juce::ToggleButton btnTestTone_;

    // Mode tabs
    juce::TextButton tabTxBtn_;
    juce::TextButton tabRxBtn_;
    juce::TextButton tabThruBtn_;

    // Matrix quick presets
    juce::TextButton btnPreset1To1_;
    juce::TextButton btnPresetStereo_;
    juce::TextButton btnClearMatrix_;

    // Main Components
    PatchbayComponent patchbay_;
    PeerListComponent peerList_;

    // Bottom Diagnostics and Master controls
    juce::Slider masterGainSlider_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterGainAttachment_;

    juce::ToggleButton masterMuteBtn_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> masterMuteAttachment_;

    // Diagnostic readouts
    float currentTxMbps_ { 0.0f };
    float currentRxMbps_ { 0.0f };
    float currentLoss_ { 0.0f };
    float currentBufferPkts_ { 0.0f };
    float currentDriftRatio_ { 1.0f };

    void updateTabStyles();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginBridgeAudioProcessorEditor)
};

} // namespace pluginbridge
