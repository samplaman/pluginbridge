#include "OmniLookAndFeel.h"

namespace pluginbridge
{

OmniLookAndFeel::OmniLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, getBgDark());
    setColour(juce::ListBox::backgroundColourId, getSurface());
    setColour(juce::Label::textColourId, getTextPrimary());
    setColour(juce::TextEditor::textColourId, getTextPrimary());
    setColour(juce::TextEditor::backgroundColourId, getSurfaceAlt());
    setColour(juce::TextEditor::outlineColourId, getBorder());
    setColour(juce::TextEditor::focusedOutlineColourId, getAccentCyan());
    setColour(juce::PopupMenu::backgroundColourId, getSurface());
    setColour(juce::PopupMenu::textColourId, getTextPrimary());
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff182230));
    setColour(juce::PopupMenu::highlightedTextColourId, getAccentCyan());
}

void OmniLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                          const juce::Colour& backgroundColour,
                                          bool shouldDrawButtonAsHighlighted,
                                          bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
    float corner = 5.0f;

    juce::Colour base = backgroundColour;
    if (shouldDrawButtonAsDown)
        base = base.darker(0.2f);
    else if (shouldDrawButtonAsHighlighted)
        base = base.brighter(0.15f);

    // Gradient fill for sleek depth
    juce::ColourGradient grad(base.brighter(0.08f), bounds.getX(), bounds.getY(),
                              base.darker(0.12f), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(bounds, corner);

    // Subtle outline
    juce::Colour borderCol = shouldDrawButtonAsHighlighted ? base.brighter(0.35f) : getBorder();
    g.setColour(borderCol);
    g.drawRoundedRectangle(bounds, corner, 1.0f);

    // Soft highlight line at top
    g.setColour(juce::Colours::white.withAlpha(shouldDrawButtonAsDown ? 0.03f : 0.08f));
    g.drawHorizontalLine(static_cast<int>(bounds.getY() + 1.0f), bounds.getX() + 2.0f, bounds.getRight() - 2.0f);
}

void OmniLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                    bool /*shouldDrawButtonAsHighlighted*/,
                                    bool /*shouldDrawButtonAsDown*/)
{
    auto font = juce::Font(juce::FontOptions().withHeight(12.0f).withStyle("Bold"));
    g.setFont(font);

    juce::Colour textCol = button.isEnabled() ? button.findColour(juce::TextButton::textColourOffId)
                                             : getTextSecondary();
    g.setColour(textCol);
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(4, 1),
                     juce::Justification::centred, 1);
}

void OmniLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                      bool shouldDrawButtonAsHighlighted,
                                      bool /*shouldDrawButtonAsDown*/)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(2.0f);
    bool toggled = button.getToggleState();

    juce::Colour bg = toggled ? getAccentRed() : getSurfaceAlt();
    if (shouldDrawButtonAsHighlighted)
        bg = bg.brighter(0.15f);

    g.setColour(bg);
    g.fillRoundedRectangle(bounds, 4.0f);

    g.setColour(toggled ? getAccentRed().brighter(0.3f) : getBorder());
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

    // Text
    g.setFont(juce::Font(juce::FontOptions().withHeight(11.0f).withStyle("Bold")));
    g.setColour(toggled ? juce::Colours::white : getTextSecondary());
    g.drawFittedText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, 1);
}

void OmniLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                   int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                                   juce::ComboBox& box)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(1.0f);
    float corner = 5.0f;

    // Body
    g.setColour(getSurface());
    g.fillRoundedRectangle(bounds, corner);

    // Border
    bool focused = box.hasKeyboardFocus(false);
    g.setColour(focused ? getAccentCyan() : getBorder());
    g.drawRoundedRectangle(bounds, corner, 1.0f);

    // Arrow icon on right
    auto arrowZone = bounds.removeFromRight(20.0f).reduced(4.0f);
    juce::Path arrow;
    float ax = arrowZone.getCentreX();
    float ay = arrowZone.getCentreY();
    arrow.addTriangle(ax - 4.0f, ay - 2.0f,
                      ax + 4.0f, ay - 2.0f,
                      ax, ay + 3.0f);
    g.setColour(getTextSecondary());
    g.fillPath(arrow);
}

void OmniLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(4, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont(juce::Font(juce::FontOptions().withHeight(12.0f)));
}

void OmniLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                                       juce::Slider::SliderStyle /*style*/, juce::Slider& /*slider*/)
{
    auto trackRect = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y + height / 2 - 2),
                                            static_cast<float>(width), 4.0f);

    // Track background
    g.setColour(getSurfaceAlt());
    g.fillRoundedRectangle(trackRect, 2.0f);

    // Filled portion
    auto filledRect = trackRect.withWidth(sliderPos - static_cast<float>(x));
    g.setColour(getAccentCyan().withAlpha(0.7f));
    g.fillRoundedRectangle(filledRect, 2.0f);

    // Thumb thumb circle
    float thumbRadius = 6.5f;
    juce::Point<float> thumbCenter(sliderPos, static_cast<float>(y + height / 2));

    // Glow halo
    g.setColour(getAccentCyan().withAlpha(0.25f));
    g.fillEllipse(thumbCenter.x - thumbRadius - 2.0f, thumbCenter.y - thumbRadius - 2.0f,
                  (thumbRadius + 2.0f) * 2.0f, (thumbRadius + 2.0f) * 2.0f);

    // Core thumb
    g.setColour(juce::Colours::white);
    g.fillEllipse(thumbCenter.x - thumbRadius, thumbCenter.y - thumbRadius,
                  thumbRadius * 2.0f, thumbRadius * 2.0f);

    g.setColour(getAccentCyan());
    g.drawEllipse(thumbCenter.x - thumbRadius, thumbCenter.y - thumbRadius,
                  thumbRadius * 2.0f, thumbRadius * 2.0f, 1.5f);
}

void OmniLookAndFeel::fillTextEditorBackground(juce::Graphics& g, int width, int height,
                                              juce::TextEditor& textEditor)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(1.0f);
    g.setColour(textEditor.findColour(juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle(bounds, 4.0f);
}

void OmniLookAndFeel::drawTextEditorOutline(juce::Graphics& g, int width, int height,
                                           juce::TextEditor& textEditor)
{
    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(1.0f);
    bool focused = textEditor.hasKeyboardFocus(true);
    g.setColour(focused ? getAccentCyan() : getBorder());
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
}

void OmniLookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar& /*scrollbar*/,
                                    int x, int y, int width, int height,
                                    bool isScrollbarVertical, int thumbStartPosition,
                                    int thumbSize, bool isMouseOver, bool /*isMouseDown*/)
{
    juce::Rectangle<float> thumbBounds;
    if (isScrollbarVertical)
    {
        thumbBounds = juce::Rectangle<float>(static_cast<float>(x + 2), static_cast<float>(thumbStartPosition),
                                             static_cast<float>(width - 4), static_cast<float>(thumbSize));
    }
    else
    {
        thumbBounds = juce::Rectangle<float>(static_cast<float>(thumbStartPosition), static_cast<float>(y + 2),
                                             static_cast<float>(thumbSize), static_cast<float>(height - 4));
    }

    g.setColour(isMouseOver ? getBorderLight() : getBorder());
    g.fillRoundedRectangle(thumbBounds, 3.0f);
}

} // namespace pluginbridge

