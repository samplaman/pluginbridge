#include "MeterComponent.h"
#include <cmath>

namespace pluginbridge
{

MeterComponent::MeterComponent(Orientation orientation)
    : orientation_(orientation)
{
    setOpaque(false);
}

void MeterComponent::setLevel(float levelLinear)
{
    currentLevel_ = levelLinear;

    // Detect clipping
    if (currentLevel_ >= 1.0f)
    {
        isClipped_ = true;
        clipHoldTimer_ = 60; // hold clip LED for ~2 seconds at 30fps
    }
    else if (clipHoldTimer_ > 0)
    {
        --clipHoldTimer_;
        if (clipHoldTimer_ == 0)
            isClipped_ = false;
    }

    // Peak hold decay
    if (currentLevel_ > peakHold_)
    {
        peakHold_ = currentLevel_;
        peakHoldTimer_ = 25;
    }
    else if (peakHoldTimer_ > 0)
    {
        --peakHoldTimer_;
    }
    else
    {
        peakHold_ *= 0.91f;
    }
    repaint();
}

void MeterComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Dark recessed meter trench
    g.setColour(juce::Colour(0xff090c10));
    g.fillRoundedRectangle(bounds, 2.0f);
    g.setColour(juce::Colour(0xff1b222d));
    g.drawRoundedRectangle(bounds, 2.0f, 1.0f);

    auto meterArea = bounds.reduced(1.5f);

    // Convert linear to normalized [0, 1] using dB range -60dB to +6dB
    float db = juce::Decibels::gainToDecibels(currentLevel_, -60.0f);
    float norm = juce::jmap(juce::jlimit(-60.0f, 6.0f, db), -60.0f, 6.0f, 0.0f, 1.0f);

    float peakDb = juce::Decibels::gainToDecibels(peakHold_, -60.0f);
    float peakNorm = juce::jmap(juce::jlimit(-60.0f, 6.0f, peakDb), -60.0f, 6.0f, 0.0f, 1.0f);

    if (orientation_ == Orientation::Vertical)
    {
        // Clip LED at top
        auto clipArea = meterArea.removeFromTop(4.0f);
        meterArea.removeFromTop(1.5f); // gap

        if (isClipped_)
        {
            g.setColour(juce::Colour(0xffff2222));
            g.fillRoundedRectangle(clipArea, 1.0f);
        }
        else
        {
            g.setColour(juce::Colour(0xff2d1214));
            g.fillRoundedRectangle(clipArea, 1.0f);
        }

        // Active level bar
        if (norm > 0.01f)
        {
            float barH = meterArea.getHeight() * norm;
            auto activeRect = meterArea.removeFromBottom(barH);

            juce::ColourGradient grad(juce::Colour(0xff10b981), meterArea.getX(), meterArea.getBottom(),
                                      juce::Colour(0xffef4444), meterArea.getX(), meterArea.getY(), false);
            grad.addColour(0.65, juce::Colour(0xff84cc16));
            grad.addColour(0.85, juce::Colour(0xfff59e0b));

            g.setGradientFill(grad);
            g.fillRoundedRectangle(activeRect, 1.0f);
        }

        // Peak tick
        if (peakNorm > 0.05f)
        {
            float tickY = meterArea.getBottom() - (meterArea.getHeight() * peakNorm);
            g.setColour(peakDb >= 0.0f ? juce::Colour(0xffff4444) : juce::Colours::white.withAlpha(0.9f));
            g.drawHorizontalLine(static_cast<int>(tickY), meterArea.getX(), meterArea.getRight());
        }
    }
    else
    {
        // Horizontal orientation
        auto clipArea = meterArea.removeFromRight(4.0f);
        meterArea.removeFromRight(1.5f); // gap

        if (isClipped_)
        {
            g.setColour(juce::Colour(0xffff2222));
            g.fillRoundedRectangle(clipArea, 1.0f);
        }
        else
        {
            g.setColour(juce::Colour(0xff2d1214));
            g.fillRoundedRectangle(clipArea, 1.0f);
        }

        if (norm > 0.01f)
        {
            float barW = meterArea.getWidth() * norm;
            auto activeRect = meterArea.removeFromLeft(barW);

            juce::ColourGradient grad(juce::Colour(0xff10b981), meterArea.getX(), meterArea.getY(),
                                      juce::Colour(0xffef4444), meterArea.getRight(), meterArea.getY(), false);
            grad.addColour(0.65, juce::Colour(0xff84cc16));
            grad.addColour(0.85, juce::Colour(0xfff59e0b));

            g.setGradientFill(grad);
            g.fillRoundedRectangle(activeRect, 1.0f);
        }

        if (peakNorm > 0.05f)
        {
            float tickX = meterArea.getX() + (meterArea.getWidth() * peakNorm);
            g.setColour(peakDb >= 0.0f ? juce::Colour(0xffff4444) : juce::Colours::white.withAlpha(0.9f));
            g.drawVerticalLine(static_cast<int>(tickX), meterArea.getY(), meterArea.getBottom());
        }
    }
}

} // namespace pluginbridge
