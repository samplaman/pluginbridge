#include "ChannelMappingComponent.h"
#include "OmniLookAndFeel.h"

namespace pluginbridge
{

ChannelMappingComponent::ChannelMappingComponent(RoutingMatrix& matrix, MapDirection dir, std::function<void()> onDismiss)
    : matrix_(matrix), direction_(dir), onDismiss_(std::move(onDismiss))
{
    juce::String dirTitle = (direction_ == MapDirection::Inputs) ? "INPUT CHANNEL REMAPPER" : "OUTPUT CHANNEL REMAPPER";
    titleLabel_.setText(dirTitle + " (48 CH)", juce::dontSendNotification);
    titleLabel_.setFont(juce::Font(juce::FontOptions().withHeight(14.0f).withStyle("Bold")));
    titleLabel_.setColour(juce::Label::textColourId,
        (direction_ == MapDirection::Inputs) ? OmniLookAndFeel::getAccentCyan() : OmniLookAndFeel::getAccentAmber());
    addAndMakeVisible(titleLabel_);

    closeBtn_.setButtonText("DONE");
    closeBtn_.onClick = [this] {
        if (onDismiss_) onDismiss_();
    };
    addAndMakeVisible(closeBtn_);

    btnReset1To1_.setButtonText("RESET 1:1");
    btnReset1To1_.onClick = [this] {
        if (direction_ == MapDirection::Inputs)
            matrix_.resetInputChannelMap();
        else
            matrix_.resetOutputChannelMap();
        refreshCombos();
    };
    addAndMakeVisible(btnReset1To1_);

    btnOffsetPlus8_.setButtonText("OFFSET +8");
    btnOffsetPlus8_.onClick = [this] {
        if (direction_ == MapDirection::Inputs)
            matrix_.setInputChannelOffset(8);
        else
            matrix_.setOutputChannelOffset(8);
        refreshCombos();
    };
    addAndMakeVisible(btnOffsetPlus8_);

    btnOffsetPlus16_.setButtonText("OFFSET +16");
    btnOffsetPlus16_.onClick = [this] {
        if (direction_ == MapDirection::Inputs)
            matrix_.setInputChannelOffset(16);
        else
            matrix_.setOutputChannelOffset(16);
        refreshCombos();
    };
    addAndMakeVisible(btnOffsetPlus16_);

    contentContainer_ = std::make_unique<juce::Component>();

    juce::String prefix = (direction_ == MapDirection::Inputs) ? "IN " : "OUT ";

    for (size_t i = 0; i < static_cast<size_t>(RoutingMatrix::MATRIX_SIZE); ++i)
    {
        rows_[i].label = std::make_unique<juce::Label>();
        rows_[i].label->setText(prefix + juce::String::formatted("%02d", static_cast<int>(i + 1)) + ":", juce::dontSendNotification);
        rows_[i].label->setFont(juce::Font(juce::FontOptions().withHeight(11.0f).withStyle("Bold")));
        rows_[i].label->setColour(juce::Label::textColourId, OmniLookAndFeel::getTextSecondary());
        contentContainer_->addAndMakeVisible(*rows_[i].label);

        rows_[i].combo = std::make_unique<juce::ComboBox>();
        for (int h = 0; h < RoutingMatrix::MATRIX_SIZE; ++h)
        {
            rows_[i].combo->addItem("Interface Ch " + juce::String(h + 1), h + 1);
        }

        size_t chIndex = i;
        rows_[i].combo->onChange = [this, chIndex] {
            int selectedHw = rows_[chIndex].combo->getSelectedId() - 1;
            if (selectedHw >= 0 && selectedHw < RoutingMatrix::MATRIX_SIZE)
            {
                if (direction_ == MapDirection::Inputs)
                    matrix_.setInputChannelMap(static_cast<int>(chIndex), selectedHw);
                else
                    matrix_.setOutputChannelMap(static_cast<int>(chIndex), selectedHw);
            }
        };
        contentContainer_->addAndMakeVisible(*rows_[i].combo);
    }

    refreshCombos();

    viewport_.setViewedComponent(contentContainer_.get(), false);
    viewport_.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport_);
}

void ChannelMappingComponent::refreshCombos()
{
    for (size_t i = 0; i < static_cast<size_t>(RoutingMatrix::MATRIX_SIZE); ++i)
    {
        int hw = (direction_ == MapDirection::Inputs)
            ? matrix_.getInputChannelMap(static_cast<int>(i))
            : matrix_.getOutputChannelMap(static_cast<int>(i));
        rows_[i].combo->setSelectedId(hw + 1, juce::dontSendNotification);
    }
}

void ChannelMappingComponent::paint(juce::Graphics& g)
{
    g.fillAll(OmniLookAndFeel::getBgDark().withAlpha(0.96f));

    auto bounds = getLocalBounds().toFloat();
    g.setColour(OmniLookAndFeel::getBorder());
    g.drawRoundedRectangle(bounds.reduced(1.0f), 6.0f, 1.5f);

    // Header divider
    g.drawHorizontalLine(44, 0.0f, static_cast<float>(getWidth()));
}

void ChannelMappingComponent::resized()
{
    titleLabel_.setBounds(14, 10, 180, 24);
    closeBtn_.setBounds(getWidth() - 68, 10, 56, 24);

    btnOffsetPlus16_.setBounds(getWidth() - 146, 10, 74, 24);
    btnOffsetPlus8_.setBounds(getWidth() - 218, 10, 68, 24);
    btnReset1To1_.setBounds(getWidth() - 288, 10, 66, 24);

    auto vpBounds = getLocalBounds().reduced(12);
    vpBounds.removeFromTop(36);
    viewport_.setBounds(vpBounds);

    // Arrange 48 channels into 3 columns of 16 rows inside the content container
    int colWidth = (vpBounds.getWidth() - 30) / 3;
    int rowHeight = 26;
    int totalH = 16 * rowHeight + 10;
    contentContainer_->setBounds(0, 0, vpBounds.getWidth() - 16, totalH);

    for (size_t i = 0; i < static_cast<size_t>(RoutingMatrix::MATRIX_SIZE); ++i)
    {
        int col = static_cast<int>(i / 16);
        int row = static_cast<int>(i % 16);

        int x = col * colWidth + 6;
        int y = row * rowHeight + 4;

        rows_[i].label->setBounds(x, y, 54, 22);
        rows_[i].combo->setBounds(x + 56, y, colWidth - 62, 22);
    }
}

} // namespace pluginbridge

