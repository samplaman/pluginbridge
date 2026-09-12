#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace pluginbridge
{

class OmniLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OmniLookAndFeel();
    ~OmniLookAndFeel() override = default;

    // Buttons
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                            const juce::Colour& backgroundColour,
                            bool shouldDrawButtonAsHighlighted,
                            bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    // Toggle Buttons
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;

    // ComboBox
    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;

    // Sliders
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle style, juce::Slider& slider) override;

    // TextEditor
    void fillTextEditorBackground(juce::Graphics& g, int width, int height,
                                  juce::TextEditor& textEditor) override;
    void drawTextEditorOutline(juce::Graphics& g, int width, int height,
                               juce::TextEditor& textEditor) override;

    // Scrollbars
    void drawScrollbar(juce::Graphics& g, juce::ScrollBar& scrollbar,
                       int x, int y, int width, int height,
                       bool isScrollbarVertical, int thumbStartPosition,
                       int thumbSize, bool isMouseOver, bool isMouseDown) override;

    // Colors - Stealth Pro Audio Dark Palette
    static juce::Colour getBgDark()      { return juce::Colour(0xff07080b); }
    static juce::Colour getSurface()     { return juce::Colour(0xff0e1116); }
    static juce::Colour getSurfaceAlt()  { return juce::Colour(0xff14171f); }
    static juce::Colour getBorder()      { return juce::Colour(0xff1b2029); }
    static juce::Colour getBorderLight() { return juce::Colour(0xff262e3b); }

    static juce::Colour getAccentCyan()  { return juce::Colour(0xff00d9ff); }
    static juce::Colour getAccentAmber() { return juce::Colour(0xffff9900); }
    static juce::Colour getAccentGreen() { return juce::Colour(0xff10b981); }
    static juce::Colour getAccentRed()   { return juce::Colour(0xffef4444); }

    static juce::Colour getTextPrimary()   { return juce::Colour(0xfff0f6fc); }
    static juce::Colour getTextSecondary() { return juce::Colour(0xff798394); }
};

} // namespace pluginbridge

