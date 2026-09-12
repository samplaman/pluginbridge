#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../DSP/RoutingMatrix.h"
#include <array>
#include <memory>

namespace pluginbridge
{

class ChannelMappingComponent : public juce::Component
{
public:
    enum class MapDirection
    {
        Inputs,  // Matrix DAW/HW Inputs
        Outputs  // Matrix DAW/HW Outputs
    };

    ChannelMappingComponent(RoutingMatrix& matrix, MapDirection dir, std::function<void()> onDismiss);
    ~ChannelMappingComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    RoutingMatrix& matrix_;
    MapDirection direction_;
    std::function<void()> onDismiss_;

    juce::Label titleLabel_;
    juce::TextButton closeBtn_;

    // Quick presets
    juce::TextButton btnReset1To1_;
    juce::TextButton btnOffsetPlus8_;
    juce::TextButton btnOffsetPlus16_;

    // 48 channel row widgets
    struct ChannelRow
    {
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::ComboBox> combo;
    };
    std::array<ChannelRow, RoutingMatrix::MATRIX_SIZE> rows_;

    juce::Viewport viewport_;
    std::unique_ptr<juce::Component> contentContainer_;

    void refreshCombos();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelMappingComponent)
};

} // namespace pluginbridge

