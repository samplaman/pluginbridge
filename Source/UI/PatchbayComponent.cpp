#include "PatchbayComponent.h"
#include "OmniLookAndFeel.h"

namespace pluginbridge
{

PatchbayComponent::PatchbayComponent(RoutingMatrix& matrix)
    : matrix_(matrix)
{
    for (size_t i = 0; i < RoutingMatrix::MATRIX_SIZE; ++i)
    {
        sourceMeters_[i] = std::make_unique<MeterComponent>(MeterComponent::Orientation::Vertical);
        addAndMakeVisible(*sourceMeters_[i]);

        destMeters_[i] = std::make_unique<MeterComponent>(MeterComponent::Orientation::Vertical);
        addAndMakeVisible(*destMeters_[i]);
    }

    // Bank buttons
    addAndMakeVisible(bank1Btn_);
    bank1Btn_.setButtonText("1-16");
    bank1Btn_.onClick = [this] { setBankView(BankView::Bank1_16); };

    addAndMakeVisible(bank2Btn_);
    bank2Btn_.setButtonText("17-32");
    bank2Btn_.onClick = [this] { setBankView(BankView::Bank17_32); };

    addAndMakeVisible(bank3Btn_);
    bank3Btn_.setButtonText("33-48");
    bank3Btn_.onClick = [this] { setBankView(BankView::Bank33_48); };

    addAndMakeVisible(bankAllBtn_);
    bankAllBtn_.setButtonText("ALL 48");
    bankAllBtn_.onClick = [this] { setBankView(BankView::All_48); };

    // Hardware Audio Interface Channel Mapping button
    addAndMakeVisible(btnAudioIfMap_);
    btnAudioIfMap_.setButtonText("AUDIO IF MAP");
    btnAudioIfMap_.setColour(juce::TextButton::buttonColourId, OmniLookAndFeel::getSurfaceAlt());
    btnAudioIfMap_.setColour(juce::TextButton::textColourOffId, OmniLookAndFeel::getTextPrimary());
    btnAudioIfMap_.onClick = [this] { showGlobalChannelMapMenu(); };

    updateBankButtonStyles();
}

void PatchbayComponent::setMode(MatrixMode mode)
{
    currentMode_ = mode;
    repaint();
}

void PatchbayComponent::setBankView(BankView bank)
{
    currentBank_ = bank;
    updateBankButtonStyles();
    resized();
    repaint();
}

void PatchbayComponent::updateBankButtonStyles()
{
    auto setBtnStyle = [](juce::TextButton& btn, bool active) {
        btn.setColour(juce::TextButton::buttonColourId, active ? OmniLookAndFeel::getAccentCyan().withAlpha(0.25f) : OmniLookAndFeel::getSurfaceAlt());
        btn.setColour(juce::TextButton::textColourOffId, active ? OmniLookAndFeel::getAccentCyan() : OmniLookAndFeel::getTextSecondary());
    };

    setBtnStyle(bank1Btn_, currentBank_ == BankView::Bank1_16);
    setBtnStyle(bank2Btn_, currentBank_ == BankView::Bank17_32);
    setBtnStyle(bank3Btn_, currentBank_ == BankView::Bank33_48);
    setBtnStyle(bankAllBtn_, currentBank_ == BankView::All_48);
}

int PatchbayComponent::getVisibleChannels() const
{
    return (currentBank_ == BankView::All_48) ? 48 : 16;
}

int PatchbayComponent::getStartChannel() const
{
    switch (currentBank_)
    {
        case BankView::Bank1_16:  return 0;
        case BankView::Bank17_32: return 16;
        case BankView::Bank33_48: return 32;
        case BankView::All_48:    return 0;
    }
    return 0;
}

void PatchbayComponent::updateMeters()
{
    for (size_t i = 0; i < RoutingMatrix::MATRIX_SIZE; ++i)
    {
        int ch = static_cast<int>(i);
        float srcVal = 0.0f;
        float dstVal = 0.0f;

        switch (currentMode_)
        {
            case MatrixMode::DawInToNetworkTx:
                srcVal = matrix_.getDawInPeak(ch);
                dstVal = matrix_.getNetworkTxPeak(ch);
                break;
            case MatrixMode::NetworkRxToDawOut:
                srcVal = matrix_.getNetworkRxPeak(ch);
                dstVal = matrix_.getDawOutPeak(ch);
                break;
            case MatrixMode::DawInToDawOut:
                srcVal = matrix_.getDawInPeak(ch);
                dstVal = matrix_.getDawOutPeak(ch);
                break;
        }

        if (sourceMeters_[i]) sourceMeters_[i]->setLevel(srcVal);
        if (destMeters_[i]) destMeters_[i]->setLevel(dstVal);
    }
}

juce::Rectangle<int> PatchbayComponent::getGridBounds() const
{
    int leftMargin = (currentBank_ == BankView::All_48) ? 46 : 124;
    int topMargin = 58;
    return juce::Rectangle<int>(leftMargin, topMargin, getWidth() - leftMargin - 10, getHeight() - topMargin - 8);
}

juce::Rectangle<int> PatchbayComponent::getCellBounds(int localRow, int localCol) const
{
    auto grid = getGridBounds();
    float visibleCount = static_cast<float>(getVisibleChannels());
    float cellW = static_cast<float>(grid.getWidth()) / visibleCount;
    float cellH = static_cast<float>(grid.getHeight()) / visibleCount;

    return juce::Rectangle<int>(
        grid.getX() + static_cast<int>(static_cast<float>(localCol) * cellW),
        grid.getY() + static_cast<int>(static_cast<float>(localRow) * cellH),
        static_cast<int>(cellW),
        static_cast<int>(cellH)
    );
}

juce::Rectangle<int> PatchbayComponent::getRowHeaderBounds(int localRow) const
{
    auto grid = getGridBounds();
    float visibleCount = static_cast<float>(getVisibleChannels());
    float cellH = static_cast<float>(grid.getHeight()) / visibleCount;
    int y = grid.getY() + static_cast<int>(static_cast<float>(localRow) * cellH);
    return juce::Rectangle<int>(4, y, grid.getX() - 20, static_cast<int>(cellH));
}

juce::Rectangle<int> PatchbayComponent::getColHeaderBounds(int localCol) const
{
    auto grid = getGridBounds();
    float visibleCount = static_cast<float>(getVisibleChannels());
    float cellW = static_cast<float>(grid.getWidth()) / visibleCount;
    int x = grid.getX() + static_cast<int>(static_cast<float>(localCol) * cellW);
    return juce::Rectangle<int>(x - 3, 10, static_cast<int>(cellW) + 6, 32);
}

void PatchbayComponent::resized()
{
    auto grid = getGridBounds();
    int visibleCount = getVisibleChannels();
    float fVisibleCount = static_cast<float>(visibleCount);
    int startCh = getStartChannel();
    float cellW = static_cast<float>(grid.getWidth()) / fVisibleCount;
    float cellH = static_cast<float>(grid.getHeight()) / fVisibleCount;

    // Bank buttons at top left
    bank1Btn_.setBounds(10, 8, 44, 22);
    bank2Btn_.setBounds(58, 8, 44, 22);
    bank3Btn_.setBounds(106, 8, 44, 22);
    bankAllBtn_.setBounds(154, 8, 52, 22);
    btnAudioIfMap_.setBounds(212, 8, 92, 22);

    // Position source & destination meters
    for (size_t i = 0; i < RoutingMatrix::MATRIX_SIZE; ++i)
    {
        int chIdx = static_cast<int>(i);
        bool isVisible = (chIdx >= startCh && chIdx < startCh + visibleCount);
        if (currentBank_ == BankView::All_48)
        {
            if (sourceMeters_[i])
            {
                int localIdx = chIdx - startCh;
                int y = grid.getY() + static_cast<int>(static_cast<float>(localIdx) * cellH);
                sourceMeters_[i]->setVisible(true);
                sourceMeters_[i]->setBounds(grid.getX() - 8, y + 1, 4, std::max(2, static_cast<int>(cellH) - 2));
            }
            if (destMeters_[i])
            {
                destMeters_[i]->setVisible(false);
            }
        }
        else
        {
            if (sourceMeters_[i])
            {
                sourceMeters_[i]->setVisible(isVisible);
                if (isVisible)
                {
                    int localIdx = chIdx - startCh;
                    int y = grid.getY() + static_cast<int>(static_cast<float>(localIdx) * cellH);
                    sourceMeters_[i]->setBounds(grid.getX() - 13, y + 2, 5, static_cast<int>(cellH) - 4);
                }
            }

            if (destMeters_[i])
            {
                destMeters_[i]->setVisible(isVisible);
                if (isVisible)
                {
                    int localIdx = chIdx - startCh;
                    int x = grid.getX() + static_cast<int>(static_cast<float>(localIdx) * cellW);
                    destMeters_[i]->setBounds(x + 2, grid.getY() - 11, static_cast<int>(cellW) - 4, 5);
                }
            }
        }
    }

    if (mappingOverlay_)
        mappingOverlay_->setBounds(getLocalBounds().reduced(16));
}

void PatchbayComponent::paint(juce::Graphics& g)
{
    auto grid = getGridBounds();
    int visibleCount = getVisibleChannels();
    float fVisibleCount = static_cast<float>(visibleCount);
    int startCh = getStartChannel();
    float cellW = static_cast<float>(grid.getWidth()) / fVisibleCount;
    float cellH = static_cast<float>(grid.getHeight()) / fVisibleCount;

    // Background
    g.fillAll(OmniLookAndFeel::getSurface());

    // Active mode badge on right
    juce::Colour activeColor;
    juce::String srcLabel, dstLabel, modeName;
    bool rowsAreInputs = true;
    bool colsAreOutputs = false;

    switch (currentMode_)
    {
        case MatrixMode::DawInToNetworkTx:
            activeColor = OmniLookAndFeel::getAccentCyan();
            srcLabel = "IN";
            dstLabel = "TX";
            modeName = "TX MATRIX (48 CH)";
            rowsAreInputs = true;
            colsAreOutputs = false;
            break;
        case MatrixMode::NetworkRxToDawOut:
            activeColor = OmniLookAndFeel::getAccentAmber();
            srcLabel = "RX";
            dstLabel = "OUT";
            modeName = "RX MATRIX (48 CH)";
            rowsAreInputs = false;
            colsAreOutputs = true;
            break;
        case MatrixMode::DawInToDawOut:
            activeColor = OmniLookAndFeel::getAccentGreen();
            srcLabel = "IN";
            dstLabel = "OUT";
            modeName = "THRU MATRIX (48 CH)";
            rowsAreInputs = true;
            colsAreOutputs = true;
            break;
    }

    auto modeBadge = juce::Rectangle<float>(static_cast<float>(getWidth() - 134), 8.0f, 124.0f, 22.0f);
    g.setColour(OmniLookAndFeel::getSurfaceAlt());
    g.fillRoundedRectangle(modeBadge, 3.0f);
    g.setColour(OmniLookAndFeel::getBorder());
    g.drawRoundedRectangle(modeBadge, 3.0f, 1.0f);

    g.setFont(juce::Font(juce::FontOptions().withHeight(9.5f).withStyle("Bold")));
    g.setColour(activeColor);
    g.drawText(modeName, modeBadge.toNearestInt(), juce::Justification::centred);

    // Alternating stereo pair banding (subtle carbon contrast)
    for (int pair = 0; pair < visibleCount / 2; ++pair)
    {
        if (pair % 2 == 1)
        {
            float y = static_cast<float>(grid.getY()) + static_cast<float>(pair * 2) * cellH;
            g.setColour(juce::Colour(0x06ffffff));
            g.fillRect(grid.getX(), static_cast<int>(y), grid.getWidth(), static_cast<int>(cellH * 2.0f));
        }
    }

    // Hover crosshair guide beams
    if (hoveredCol_ >= 0 && hoveredCol_ < visibleCount)
    {
        int x = grid.getX() + static_cast<int>(static_cast<float>(hoveredCol_) * cellW);
        g.setColour(activeColor.withAlpha(0.06f));
        g.fillRect(x, grid.getY(), static_cast<int>(cellW), grid.getHeight());
    }
    if (hoveredRow_ >= 0 && hoveredRow_ < visibleCount)
    {
        int y = grid.getY() + static_cast<int>(static_cast<float>(hoveredRow_) * cellH);
        g.setColour(activeColor.withAlpha(0.06f));
        g.fillRect(grid.getX(), y, grid.getWidth(), static_cast<int>(cellH));
    }

    // Column Headers (Destinations)
    float headerFontH = (currentBank_ == BankView::All_48) ? 8.0f : 9.5f;
    g.setFont(juce::Font(juce::FontOptions().withHeight(headerFontH).withStyle("Bold")));

    for (int localCol = 0; localCol < visibleCount; ++localCol)
    {
        int globalCol = startCh + localCol;
        int x = grid.getX() + static_cast<int>(static_cast<float>(localCol) * cellW);
        bool isHovered = (localCol == hoveredCol_ || localCol == hoveredHeaderCol_);
        auto colHdrBounds = getColHeaderBounds(localCol);

        if (isHovered && colsAreOutputs)
        {
            g.setColour(activeColor.withAlpha(0.12f));
            g.fillRoundedRectangle(colHdrBounds.toFloat(), 3.0f);
        }

        g.setColour(isHovered ? activeColor : OmniLookAndFeel::getTextSecondary());

        if (currentBank_ != BankView::All_48)
        {
            juce::String colText = dstLabel + " " + juce::String::formatted("%02d", globalCol + 1);
            g.drawText(colText, x - 4, 12, static_cast<int>(cellW) + 8, 14, juce::Justification::centred);

            if (colsAreOutputs)
            {
                int hwOut = matrix_.getOutputChannelMap(globalCol) + 1;
                juce::String ifText = "[IF " + juce::String::formatted("%02d", hwOut) + "]";
                g.setFont(juce::Font(juce::FontOptions().withHeight(8.0f).withStyle("Bold")));
                g.setColour(activeColor.withAlpha(0.85f));
                g.drawText(ifText, x - 4, 26, static_cast<int>(cellW) + 8, 12, juce::Justification::centred);
                g.setFont(juce::Font(juce::FontOptions().withHeight(headerFontH).withStyle("Bold")));
            }
        }
        else
        {
            int chNum = globalCol + 1;
            if (chNum == 1 || chNum % 8 == 1)
            {
                g.setColour(activeColor);
                g.drawText(juce::String(chNum), x - 2, 22, static_cast<int>(cellW * 2.0f) + 6, 13, juce::Justification::left);
            }
            else if (chNum % 4 == 1)
            {
                g.setColour(OmniLookAndFeel::getTextSecondary());
                g.drawText(juce::String(chNum), x - 2, 22, static_cast<int>(cellW * 2.0f) + 6, 13, juce::Justification::left);
            }
        }
    }

    // Row Headers (Sources)
    for (int localRow = 0; localRow < visibleCount; ++localRow)
    {
        int globalRow = startCh + localRow;
        int y = grid.getY() + static_cast<int>(static_cast<float>(localRow) * cellH);
        bool isHovered = (localRow == hoveredRow_ || localRow == hoveredHeaderRow_);
        auto rowHdrBounds = getRowHeaderBounds(localRow);

        if (isHovered && rowsAreInputs)
        {
            g.setColour(activeColor.withAlpha(0.12f));
            g.fillRoundedRectangle(rowHdrBounds.toFloat(), 3.0f);
        }

        g.setColour(isHovered ? activeColor : OmniLookAndFeel::getTextSecondary());

        if (currentBank_ != BankView::All_48)
        {
            juce::String rowText = srcLabel + " " + juce::String::formatted("%02d", globalRow + 1);
            if (rowsAreInputs)
            {
                int hwIn = matrix_.getInputChannelMap(globalRow) + 1;
                rowText += " [IF " + juce::String::formatted("%02d", hwIn) + "]";
            }
            g.drawText(rowText, 4, y, grid.getX() - 20, static_cast<int>(cellH), juce::Justification::centredRight);
        }
        else
        {
            int chNum = globalRow + 1;
            if (chNum == 1 || chNum % 8 == 1)
                g.setColour(activeColor);
            else
                g.setColour(OmniLookAndFeel::getTextSecondary());

            if (chNum == 1 || chNum % 4 == 1)
            {
                juce::String rowText = juce::String(chNum);
                g.drawText(rowText, 2, y, grid.getX() - 12, static_cast<int>(cellH), juce::Justification::centredRight);
            }
        }
    }

    // Grid lines
    for (int i = 0; i <= visibleCount; ++i)
    {
        int chNum = startCh + i;
        float x = static_cast<float>(grid.getX()) + static_cast<float>(i) * cellW;
        bool isBlock8 = (chNum % 8 == 0);
        bool isPair = (chNum % 2 == 0);

        juce::Colour lineCol = isBlock8 ? activeColor.withAlpha(0.35f)
                             : isPair   ? OmniLookAndFeel::getBorderLight().withAlpha(0.25f)
                                        : OmniLookAndFeel::getBorder().withAlpha(0.15f);
        g.setColour(lineCol);
        g.drawLine(x, static_cast<float>(grid.getY()), x, static_cast<float>(grid.getBottom()), isBlock8 ? 1.5f : (isPair ? 1.0f : 0.5f));

        float y = static_cast<float>(grid.getY()) + static_cast<float>(i) * cellH;
        g.drawLine(static_cast<float>(grid.getX()), y, static_cast<float>(grid.getRight()), y, isBlock8 ? 1.5f : (isPair ? 1.0f : 0.5f));
    }

    // Matrix Crosspoints
    for (int localRow = 0; localRow < visibleCount; ++localRow)
    {
        int globalRow = startCh + localRow;
        for (int localCol = 0; localCol < visibleCount; ++localCol)
        {
            int globalCol = startCh + localCol;
            auto cell = getCellBounds(localRow, localCol);
            bool isConnected = false;

            switch (currentMode_)
            {
                case MatrixMode::DawInToNetworkTx:
                    isConnected = matrix_.getInputToTx(globalRow, globalCol);
                    break;
                case MatrixMode::NetworkRxToDawOut:
                    isConnected = matrix_.getRxToOutput(globalRow, globalCol);
                    break;
                case MatrixMode::DawInToDawOut:
                    isConnected = matrix_.getInputToOutput(globalRow, globalCol);
                    break;
            }

            auto center = cell.getCentre().toFloat();
            bool isHover = (localRow == hoveredRow_ && localCol == hoveredCol_);

            if (currentBank_ == BankView::All_48)
            {
                float radius = isConnected ? (isHover ? 3.5f : 2.5f) : (isHover ? 2.0f : 1.2f);
                if (isConnected)
                {
                    g.setColour(activeColor.withAlpha(0.3f));
                    g.fillEllipse(center.x - radius - 1.5f, center.y - radius - 1.5f, (radius + 1.5f) * 2.0f, (radius + 1.5f) * 2.0f);
                    g.setColour(activeColor);
                    g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);
                }
                else if (isHover)
                {
                    g.setColour(activeColor.withAlpha(0.6f));
                    g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);
                }
                else
                {
                    g.setColour(OmniLookAndFeel::getBorder());
                    g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);
                }
            }
            else
            {
                float radius = isConnected ? (isHover ? 6.5f : 5.5f) : (isHover ? 4.0f : 2.5f);
                if (isConnected)
                {
                    g.setColour(activeColor.withAlpha(0.22f));
                    g.fillEllipse(center.x - radius - 3.5f, center.y - radius - 3.5f, (radius + 3.5f) * 2.0f, (radius + 3.5f) * 2.0f);

                    g.setColour(activeColor);
                    g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);

                    g.setColour(juce::Colours::white);
                    g.fillEllipse(center.x - 2.0f, center.y - 2.0f, 4.0f, 4.0f);
                }
                else
                {
                    g.setColour(isHover ? OmniLookAndFeel::getBorderLight() : OmniLookAndFeel::getBorder());
                    g.drawEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f, 1.0f);

                    if (isHover)
                    {
                        g.setColour(activeColor.withAlpha(0.6f));
                        g.fillEllipse(center.x - 1.5f, center.y - 1.5f, 3.0f, 3.0f);
                    }
                }
            }
        }
    }

    // Live hover tooltip badge showing exact crosspoint channels (clean ASCII arrow, dynamically positioned without overlap)
    if (hoveredRow_ >= 0 && hoveredRow_ < visibleCount && hoveredCol_ >= 0 && hoveredCol_ < visibleCount)
    {
        int gRow = startCh + hoveredRow_ + 1;
        int gCol = startCh + hoveredCol_ + 1;
        juce::String tip = srcLabel + " " + juce::String(gRow) + " -> " + dstLabel + " " + juce::String(gCol);

        if (rowsAreInputs)
            tip += " [IF " + juce::String(matrix_.getInputChannelMap(gRow - 1) + 1) + "]";
        if (colsAreOutputs)
            tip += " [IF " + juce::String(matrix_.getOutputChannelMap(gCol - 1) + 1) + "]";

        float leftBound = 312.0f;
        float rightBound = modeBadge.getX() - 8.0f;
        float availW = rightBound - leftBound;

        if (availW >= 100.0f)
        {
            float tipW = std::min(availW, 200.0f);
            float tipX = leftBound + (availW - tipW) * 0.5f;
            auto tipRect = juce::Rectangle<float>(tipX, 8.0f, tipW, 22.0f);

            g.setColour(OmniLookAndFeel::getSurfaceAlt());
            g.fillRoundedRectangle(tipRect, 3.0f);
            g.setColour(OmniLookAndFeel::getBorder());
            g.drawRoundedRectangle(tipRect, 3.0f, 1.0f);

            g.setFont(juce::Font(juce::FontOptions().withHeight(9.5f).withStyle("Bold")));
            g.setColour(OmniLookAndFeel::getTextPrimary());
            g.drawFittedText(tip, tipRect.toNearestInt().reduced(4, 1), juce::Justification::centred, 1);
        }
    }
}

void PatchbayComponent::toggleCrosspoint(int globalRow, int globalCol, bool connect)
{
    if (globalRow >= 0 && globalRow < RoutingMatrix::MATRIX_SIZE && globalCol >= 0 && globalCol < RoutingMatrix::MATRIX_SIZE)
    {
        switch (currentMode_)
        {
            case MatrixMode::DawInToNetworkTx:
                matrix_.setInputToTx(globalRow, globalCol, connect);
                break;
            case MatrixMode::NetworkRxToDawOut:
                matrix_.setRxToOutput(globalRow, globalCol, connect);
                break;
            case MatrixMode::DawInToDawOut:
                matrix_.setInputToOutput(globalRow, globalCol, connect);
                break;
        }
        repaint();
    }
}

void PatchbayComponent::mouseDown(const juce::MouseEvent& event)
{
    auto grid = getGridBounds();
    int visibleCount = getVisibleChannels();
    float fVisibleCount = static_cast<float>(visibleCount);
    int startCh = getStartChannel();
    float cellW = static_cast<float>(grid.getWidth()) / fVisibleCount;
    float cellH = static_cast<float>(grid.getHeight()) / fVisibleCount;

    bool rowsAreInputs = (currentMode_ == MatrixMode::DawInToNetworkTx || currentMode_ == MatrixMode::DawInToDawOut);
    bool colsAreOutputs = (currentMode_ == MatrixMode::NetworkRxToDawOut || currentMode_ == MatrixMode::DawInToDawOut);

    // 1. Check Row Header click (Select Audio Interface Input Channel)
    for (int localRow = 0; localRow < visibleCount; ++localRow)
    {
        if (getRowHeaderBounds(localRow).contains(event.getPosition()))
        {
            int globalRow = startCh + localRow;
            if (rowsAreInputs)
                showInputChannelSelectMenu(globalRow);
            return;
        }
    }

    // 2. Check Column Header click (Select Audio Interface Output Channel)
    for (int localCol = 0; localCol < visibleCount; ++localCol)
    {
        if (getColHeaderBounds(localCol).contains(event.getPosition()))
        {
            int globalCol = startCh + localCol;
            if (colsAreOutputs)
                showOutputChannelSelectMenu(globalCol);
            return;
        }
    }

    // 3. Check Grid click
    if (grid.contains(event.getPosition()))
    {
        int localCol = static_cast<int>(static_cast<float>(event.x - grid.getX()) / cellW);
        int localRow = static_cast<int>(static_cast<float>(event.y - grid.getY()) / cellH);

        int globalCol = startCh + localCol;
        int globalRow = startCh + localRow;

        if (globalRow >= 0 && globalRow < RoutingMatrix::MATRIX_SIZE && globalCol >= 0 && globalCol < RoutingMatrix::MATRIX_SIZE)
        {
            // Right-click opens context menu
            if (event.mods.isPopupMenu())
            {
                showCellContextMenu(globalRow, globalCol);
                return;
            }

            bool cur = false;
            switch (currentMode_)
            {
                case MatrixMode::DawInToNetworkTx: cur = matrix_.getInputToTx(globalRow, globalCol); break;
                case MatrixMode::NetworkRxToDawOut: cur = matrix_.getRxToOutput(globalRow, globalCol); break;
                case MatrixMode::DawInToDawOut: cur = matrix_.getInputToOutput(globalRow, globalCol); break;
            }

            dragSetState_ = !cur;
            isDragging_ = true;
            toggleCrosspoint(globalRow, globalCol, dragSetState_);
        }
    }
}

void PatchbayComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (!isDragging_)
        return;

    auto grid = getGridBounds();
    if (!grid.contains(event.getPosition()))
        return;

    int visibleCount = getVisibleChannels();
    float fVisibleCount = static_cast<float>(visibleCount);
    int startCh = getStartChannel();
    float cellW = static_cast<float>(grid.getWidth()) / fVisibleCount;
    float cellH = static_cast<float>(grid.getHeight()) / fVisibleCount;

    int localCol = static_cast<int>(static_cast<float>(event.x - grid.getX()) / cellW);
    int localRow = static_cast<int>(static_cast<float>(event.y - grid.getY()) / cellH);

    int globalCol = startCh + localCol;
    int globalRow = startCh + localRow;

    toggleCrosspoint(globalRow, globalCol, dragSetState_);
}

void PatchbayComponent::mouseMove(const juce::MouseEvent& event)
{
    auto grid = getGridBounds();
    int visibleCount = getVisibleChannels();
    int prevHdrRow = hoveredHeaderRow_;
    int prevHdrCol = hoveredHeaderCol_;

    hoveredHeaderRow_ = -1;
    hoveredHeaderCol_ = -1;

    // Check Row Headers
    for (int r = 0; r < visibleCount; ++r)
    {
        if (getRowHeaderBounds(r).contains(event.getPosition()))
        {
            hoveredHeaderRow_ = r;
            break;
        }
    }

    // Check Col Headers
    for (int c = 0; c < visibleCount; ++c)
    {
        if (getColHeaderBounds(c).contains(event.getPosition()))
        {
            hoveredHeaderCol_ = c;
            break;
        }
    }

    if (grid.contains(event.getPosition()))
    {
        float fVisibleCount = static_cast<float>(visibleCount);
        float cellW = static_cast<float>(grid.getWidth()) / fVisibleCount;
        float cellH = static_cast<float>(grid.getHeight()) / fVisibleCount;

        int col = static_cast<int>(static_cast<float>(event.x - grid.getX()) / cellW);
        int row = static_cast<int>(static_cast<float>(event.y - grid.getY()) / cellH);

        if (row != hoveredRow_ || col != hoveredCol_ || hoveredHeaderRow_ != prevHdrRow || hoveredHeaderCol_ != prevHdrCol)
        {
            hoveredRow_ = row;
            hoveredCol_ = col;
            repaint();
        }
    }
    else
    {
        if (hoveredRow_ != -1 || hoveredCol_ != -1 || hoveredHeaderRow_ != prevHdrRow || hoveredHeaderCol_ != prevHdrCol)
        {
            hoveredRow_ = -1;
            hoveredCol_ = -1;
            repaint();
        }
    }
}

void PatchbayComponent::mouseExit(const juce::MouseEvent& /*event*/)
{
    isDragging_ = false;
    if (hoveredRow_ != -1 || hoveredCol_ != -1 || hoveredHeaderRow_ != -1 || hoveredHeaderCol_ != -1)
    {
        hoveredRow_ = -1;
        hoveredCol_ = -1;
        hoveredHeaderRow_ = -1;
        hoveredHeaderCol_ = -1;
        repaint();
    }
}

void PatchbayComponent::showInputChannelSelectMenu(int matrixCh)
{
    juce::PopupMenu menu;
    int currentHw = matrix_.getInputChannelMap(matrixCh);

    menu.addSectionHeader("MAP MATRIX IN " + juce::String(matrixCh + 1) + " -> AUDIO INTERFACE");

    auto createBankMenu = [&](int startHw, int endHw) {
        juce::PopupMenu sub;
        for (int h = startHw; h <= endHw; ++h)
        {
            int hwIdx = h - 1;
            sub.addItem(100 + h, "Interface Channel " + juce::String(h), true, hwIdx == currentHw);
        }
        return sub;
    };

    menu.addSubMenu("Interface Channels 1 - 16", createBankMenu(1, 16));
    menu.addSubMenu("Interface Channels 17 - 32", createBankMenu(17, 32));
    menu.addSubMenu("Interface Channels 33 - 48", createBankMenu(33, 48));

    menu.addSeparator();
    menu.addItem(1, "Map 1:1 Starting at Current (Ch " + juce::String(matrixCh + 1) + " -> IF 1...)");
    menu.addItem(2, "Shift All Inputs +8 Channels");
    menu.addItem(3, "Shift All Inputs +16 Channels");
    menu.addItem(4, "Shift All Inputs -8 Channels");
    menu.addItem(5, "Reset All Inputs 1:1");
    menu.addSeparator();
    menu.addItem(6, "Open 48-Channel Input Remapper...");

    menu.showMenuAsync(juce::PopupMenu::Options().withMousePosition(), [this, matrixCh](int result) {
        if (result >= 101 && result <= 148)
        {
            matrix_.setInputChannelMap(matrixCh, result - 101);
            repaint();
        }
        else if (result == 1)
        {
            for (int i = 0; i < RoutingMatrix::MATRIX_SIZE - matrixCh; ++i)
                matrix_.setInputChannelMap(matrixCh + i, i);
            repaint();
        }
        else if (result == 2)
        {
            matrix_.setInputChannelOffset(8);
            repaint();
        }
        else if (result == 3)
        {
            matrix_.setInputChannelOffset(16);
            repaint();
        }
        else if (result == 4)
        {
            matrix_.setInputChannelOffset(RoutingMatrix::MATRIX_SIZE - 8);
            repaint();
        }
        else if (result == 5)
        {
            matrix_.resetInputChannelMap();
            repaint();
        }
        else if (result == 6)
        {
            openChannelMappingOverlay(ChannelMappingComponent::MapDirection::Inputs);
        }
    });
}

void PatchbayComponent::showOutputChannelSelectMenu(int matrixCh)
{
    juce::PopupMenu menu;
    int currentHw = matrix_.getOutputChannelMap(matrixCh);

    menu.addSectionHeader("MAP MATRIX OUT " + juce::String(matrixCh + 1) + " -> AUDIO INTERFACE");

    auto createBankMenu = [&](int startHw, int endHw) {
        juce::PopupMenu sub;
        for (int h = startHw; h <= endHw; ++h)
        {
            int hwIdx = h - 1;
            sub.addItem(200 + h, "Interface Channel " + juce::String(h), true, hwIdx == currentHw);
        }
        return sub;
    };

    menu.addSubMenu("Interface Channels 1 - 16", createBankMenu(1, 16));
    menu.addSubMenu("Interface Channels 17 - 32", createBankMenu(17, 32));
    menu.addSubMenu("Interface Channels 33 - 48", createBankMenu(33, 48));

    menu.addSeparator();
    menu.addItem(1, "Map 1:1 Starting at Current (Ch " + juce::String(matrixCh + 1) + " -> IF 1...)");
    menu.addItem(2, "Shift All Outputs +8 Channels");
    menu.addItem(3, "Shift All Outputs +16 Channels");
    menu.addItem(4, "Shift All Outputs -8 Channels");
    menu.addItem(5, "Reset All Outputs 1:1");
    menu.addSeparator();
    menu.addItem(6, "Open 48-Channel Output Remapper...");

    menu.showMenuAsync(juce::PopupMenu::Options().withMousePosition(), [this, matrixCh](int result) {
        if (result >= 201 && result <= 248)
        {
            matrix_.setOutputChannelMap(matrixCh, result - 201);
            repaint();
        }
        else if (result == 1)
        {
            for (int i = 0; i < RoutingMatrix::MATRIX_SIZE - matrixCh; ++i)
                matrix_.setOutputChannelMap(matrixCh + i, i);
            repaint();
        }
        else if (result == 2)
        {
            matrix_.setOutputChannelOffset(8);
            repaint();
        }
        else if (result == 3)
        {
            matrix_.setOutputChannelOffset(16);
            repaint();
        }
        else if (result == 4)
        {
            matrix_.setOutputChannelOffset(RoutingMatrix::MATRIX_SIZE - 8);
            repaint();
        }
        else if (result == 5)
        {
            matrix_.resetOutputChannelMap();
            repaint();
        }
        else if (result == 6)
        {
            openChannelMappingOverlay(ChannelMappingComponent::MapDirection::Outputs);
        }
    });
}

void PatchbayComponent::showGlobalChannelMapMenu()
{
    juce::PopupMenu menu;

    menu.addSectionHeader("AUDIO INTERFACE INPUT MAPPING");
    menu.addItem(1, "Inputs: 1:1 Default (Interface Ch 1-48)");
    menu.addItem(2, "Inputs: Map Bank 1 to IF Ch 1-16");
    menu.addItem(3, "Inputs: Map Bank 1 to IF Ch 17-32 (Shift +16)");
    menu.addItem(4, "Inputs: Map Bank 1 to IF Ch 33-48 (Shift +32)");
    menu.addItem(5, "Inputs: Shift +8 Channels");
    menu.addItem(6, "Inputs: Shift -8 Channels");
    menu.addItem(7, "Configure All 48 Inputs...");

    menu.addSeparator();

    menu.addSectionHeader("AUDIO INTERFACE OUTPUT MAPPING");
    menu.addItem(11, "Outputs: 1:1 Default (Interface Ch 1-48)");
    menu.addItem(12, "Outputs: Map Bank 1 to IF Ch 1-16");
    menu.addItem(13, "Outputs: Map Bank 1 to IF Ch 17-32 (Shift +16)");
    menu.addItem(14, "Outputs: Map Bank 1 to IF Ch 33-48 (Shift +32)");
    menu.addItem(15, "Outputs: Shift +8 Channels");
    menu.addItem(16, "Outputs: Shift -8 Channels");
    menu.addItem(17, "Configure All 48 Outputs...");

    menu.addSeparator();
    menu.addItem(20, "Reset All Channels 1:1");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&btnAudioIfMap_), [this](int result) {
        switch (result)
        {
            case 1:  matrix_.resetInputChannelMap(); repaint(); break;
            case 2:  matrix_.setInputBank(0, 0, 16); repaint(); break;
            case 3:  matrix_.setInputBank(0, 16, 16); repaint(); break;
            case 4:  matrix_.setInputBank(0, 32, 16); repaint(); break;
            case 5:  matrix_.setInputChannelOffset(8); repaint(); break;
            case 6:  matrix_.setInputChannelOffset(RoutingMatrix::MATRIX_SIZE - 8); repaint(); break;
            case 7:  openChannelMappingOverlay(ChannelMappingComponent::MapDirection::Inputs); break;

            case 11: matrix_.resetOutputChannelMap(); repaint(); break;
            case 12: matrix_.setOutputBank(0, 0, 16); repaint(); break;
            case 13: matrix_.setOutputBank(0, 16, 16); repaint(); break;
            case 14: matrix_.setOutputBank(0, 32, 16); repaint(); break;
            case 15: matrix_.setOutputChannelOffset(8); repaint(); break;
            case 16: matrix_.setOutputChannelOffset(RoutingMatrix::MATRIX_SIZE - 8); repaint(); break;
            case 17: openChannelMappingOverlay(ChannelMappingComponent::MapDirection::Outputs); break;

            case 20:
                matrix_.resetInputChannelMap();
                matrix_.resetOutputChannelMap();
                repaint();
                break;
            default: break;
        }
    });
}

void PatchbayComponent::showCellContextMenu(int globalRow, int globalCol)
{
    juce::PopupMenu menu;
    bool isConnected = false;
    switch (currentMode_)
    {
        case MatrixMode::DawInToNetworkTx: isConnected = matrix_.getInputToTx(globalRow, globalCol); break;
        case MatrixMode::NetworkRxToDawOut: isConnected = matrix_.getRxToOutput(globalRow, globalCol); break;
        case MatrixMode::DawInToDawOut: isConnected = matrix_.getInputToOutput(globalRow, globalCol); break;
    }

    menu.addItem(1, isConnected ? "Disconnect Crosspoint" : "Connect Crosspoint");
    menu.addSeparator();

    if (currentMode_ == MatrixMode::DawInToNetworkTx || currentMode_ == MatrixMode::DawInToDawOut)
    {
        int hw = matrix_.getInputChannelMap(globalRow) + 1;
        menu.addItem(2, "Select Interface Channel for IN " + juce::String(globalRow + 1) + " (Currently IF " + juce::String(hw) + ")...");
    }

    if (currentMode_ == MatrixMode::NetworkRxToDawOut || currentMode_ == MatrixMode::DawInToDawOut)
    {
        int hw = matrix_.getOutputChannelMap(globalCol) + 1;
        menu.addItem(3, "Select Interface Channel for OUT " + juce::String(globalCol + 1) + " (Currently IF " + juce::String(hw) + ")...");
    }

    menu.addSeparator();
    menu.addItem(4, "Clear Row " + juce::String(globalRow + 1));
    menu.addItem(5, "Clear Column " + juce::String(globalCol + 1));

    menu.showMenuAsync(juce::PopupMenu::Options().withMousePosition(), [this, globalRow, globalCol](int result) {
        if (result == 1)
        {
            switch (currentMode_)
            {
                case MatrixMode::DawInToNetworkTx: matrix_.setInputToTx(globalRow, globalCol, !matrix_.getInputToTx(globalRow, globalCol)); break;
                case MatrixMode::NetworkRxToDawOut: matrix_.setRxToOutput(globalRow, globalCol, !matrix_.getRxToOutput(globalRow, globalCol)); break;
                case MatrixMode::DawInToDawOut: matrix_.setInputToOutput(globalRow, globalCol, !matrix_.getInputToOutput(globalRow, globalCol)); break;
            }
            repaint();
        }
        else if (result == 2)
        {
            showInputChannelSelectMenu(globalRow);
        }
        else if (result == 3)
        {
            showOutputChannelSelectMenu(globalCol);
        }
        else if (result == 4)
        {
            for (int c = 0; c < RoutingMatrix::MATRIX_SIZE; ++c)
                toggleCrosspoint(globalRow, c, false);
        }
        else if (result == 5)
        {
            for (int r = 0; r < RoutingMatrix::MATRIX_SIZE; ++r)
                toggleCrosspoint(r, globalCol, false);
        }
    });
}

void PatchbayComponent::openChannelMappingOverlay(ChannelMappingComponent::MapDirection dir)
{
    mappingOverlay_ = std::make_unique<ChannelMappingComponent>(matrix_, dir, [this] {
        mappingOverlay_.reset();
        repaint();
    });
    addAndMakeVisible(*mappingOverlay_);
    mappingOverlay_->setBounds(getLocalBounds().reduced(16));
}

} // namespace pluginbridge
