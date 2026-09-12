#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../DSP/RoutingMatrix.h"
#include "MeterComponent.h"
#include "ChannelMappingComponent.h"
#include <array>
#include <memory>

namespace pluginbridge
{

class PatchbayComponent : public juce::Component
{
public:
    enum class MatrixMode
    {
        DawInToNetworkTx,
        NetworkRxToDawOut,
        DawInToDawOut
    };

    enum class BankView
    {
        Bank1_16,   // Channels 1 - 16
        Bank17_32,  // Channels 17 - 32
        Bank33_48,  // Channels 33 - 48
        All_48      // Full 48 x 48 matrix overview
    };

    explicit PatchbayComponent(RoutingMatrix& matrix);
    ~PatchbayComponent() override = default;

    void setMode(MatrixMode mode);
    MatrixMode getMode() const { return currentMode_; }

    void setBankView(BankView bank);
    BankView getBankView() const { return currentBank_; }

    void updateMeters();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    void showInputChannelSelectMenu(int matrixCh);
    void showOutputChannelSelectMenu(int matrixCh);
    void showGlobalChannelMapMenu();
    void showCellContextMenu(int globalRow, int globalCol);
    void openChannelMappingOverlay(ChannelMappingComponent::MapDirection dir);

private:
    RoutingMatrix& matrix_;
    MatrixMode currentMode_ { MatrixMode::DawInToNetworkTx };
    BankView currentBank_ { BankView::Bank1_16 };

    // Bank buttons
    juce::TextButton bank1Btn_;
    juce::TextButton bank2Btn_;
    juce::TextButton bank3Btn_;
    juce::TextButton bankAllBtn_;

    // Hardware Audio Interface Channel Mapping button
    juce::TextButton btnAudioIfMap_;

    // Optional mapping overlay dialog
    std::unique_ptr<ChannelMappingComponent> mappingOverlay_;

    std::array<std::unique_ptr<MeterComponent>, RoutingMatrix::MATRIX_SIZE> sourceMeters_;
    std::array<std::unique_ptr<MeterComponent>, RoutingMatrix::MATRIX_SIZE> destMeters_;

    int hoveredRow_ { -1 };
    int hoveredCol_ { -1 };
    int hoveredHeaderRow_ { -1 };
    int hoveredHeaderCol_ { -1 };
    bool isDragging_ { false };
    bool dragSetState_ { true };

    int getVisibleChannels() const;
    int getStartChannel() const;

    juce::Rectangle<int> getGridBounds() const;
    juce::Rectangle<int> getCellBounds(int localRow, int localCol) const;
    juce::Rectangle<int> getRowHeaderBounds(int localRow) const;
    juce::Rectangle<int> getColHeaderBounds(int localCol) const;

    void toggleCrosspoint(int globalRow, int globalCol, bool connect);
    void updateBankButtonStyles();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchbayComponent)
};

} // namespace pluginbridge
