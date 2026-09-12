#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>

namespace pluginbridge
{

class MeterComponent : public juce::Component
{
public:
    enum class Orientation
    {
        Vertical,
        Horizontal
    };

    MeterComponent(Orientation orientation = Orientation::Vertical);
    ~MeterComponent() override = default;

    void setLevel(float levelLinear);
    void paint(juce::Graphics& g) override;

private:
    Orientation orientation_;
    float currentLevel_ { 0.0f };
    float peakHold_ { 0.0f };
    int peakHoldTimer_ { 0 };
    bool isClipped_ { false };
    int clipHoldTimer_ { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeterComponent)
};

} // namespace pluginbridge
