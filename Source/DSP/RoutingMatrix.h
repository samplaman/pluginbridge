#pragma once

#include "../Network/NetworkProtocol.h"
#include <vector>
#include <array>
#include <atomic>
#include <cmath>

namespace pluginbridge
{

class RoutingMatrix
{
public:
    static constexpr int MATRIX_SIZE = 48;

    RoutingMatrix();
    ~RoutingMatrix() = default;

    void reset();

    // Crosspoints
    // DAW In -> Network TX
    void setInputToTx(int inCh, int txCh, bool connected);
    bool getInputToTx(int inCh, int txCh) const;

    // Network RX -> DAW Out
    void setRxToOutput(int rxCh, int outCh, bool connected);
    bool getRxToOutput(int rxCh, int outCh) const;

    // DAW In -> DAW Out (Thru/Monitor)
    void setInputToOutput(int inCh, int outCh, bool connected);
    bool getInputToOutput(int inCh, int outCh) const;

    // Direct Audio Interface Channel Mapping
    void setInputChannelMap(int matrixCh, int hwInCh);
    int getInputChannelMap(int matrixCh) const;

    void setOutputChannelMap(int matrixCh, int hwOutCh);
    int getOutputChannelMap(int matrixCh) const;

    void setInputChannelOffset(int offset);
    void setOutputChannelOffset(int offset);

    void resetInputChannelMap();
    void resetOutputChannelMap();
    void setInputBank(int startMatrixCh, int startHwCh, int count);
    void setOutputBank(int startMatrixCh, int startHwCh, int count);

    // Mutes and Gains
    void setTxChannelMute(int txCh, bool muted);
    bool isTxChannelMuted(int txCh) const;

    void setRxChannelMute(int rxCh, bool muted);
    bool isRxChannelMuted(int rxCh) const;

    void setTxChannelGain(int txCh, float gainLinear);
    float getTxChannelGain(int txCh) const;

    void setRxChannelGain(int rxCh, float gainLinear);
    float getRxChannelGain(int rxCh) const;

    /**
     * Process audio blocks through the routing matrix.
     * Real-time safe: no allocations, no locks.
     */
    void process(const float* const* dawIn, int numDawIn,
                 float* const* networkTx, int numNetTx,
                 const float* const* networkRx, int numNetRx,
                 float* const* dawOut, int numDawOut,
                 int numFrames);

    // Meter readings (0.0f to 1.0f+, decay handled internally)
    float getDawInPeak(int ch) const;
    float getNetworkTxPeak(int ch) const;
    float getNetworkRxPeak(int ch) const;
    float getDawOutPeak(int ch) const;

    int getNumAvailableDawInputs() const { return availableDawIn_.load(std::memory_order_relaxed); }
    void setNumAvailableDawInputs(int count) { availableDawIn_.store(count, std::memory_order_relaxed); }

    int getNumAvailableDawOutputs() const { return availableDawOut_.load(std::memory_order_relaxed); }
    void setNumAvailableDawOutputs(int count) { availableDawOut_.store(count, std::memory_order_relaxed); }

private:
    std::atomic<int> availableDawIn_ { 2 };
    std::atomic<int> availableDawOut_ { 2 };
    // Crosspoint boolean tables (16 x 16)
    std::array<std::array<std::atomic<bool>, MATRIX_SIZE>, MATRIX_SIZE> inToTx_;
    std::array<std::array<std::atomic<bool>, MATRIX_SIZE>, MATRIX_SIZE> rxToOut_;
    std::array<std::array<std::atomic<bool>, MATRIX_SIZE>, MATRIX_SIZE> inToOut_;

    // Channel controls
    std::array<std::atomic<bool>, MATRIX_SIZE> txMute_;
    std::array<std::atomic<bool>, MATRIX_SIZE> rxMute_;
    std::array<std::atomic<float>, MATRIX_SIZE> txGain_;
    std::array<std::atomic<float>, MATRIX_SIZE> rxGain_;

    // Direct interface channel mapping (Matrix index -> Interface channel index)
    std::array<std::atomic<int>, MATRIX_SIZE> inputChannelMap_;
    std::array<std::atomic<int>, MATRIX_SIZE> outputChannelMap_;

    // Meters
    std::array<std::atomic<float>, MATRIX_SIZE> dawInMeters_;
    std::array<std::atomic<float>, MATRIX_SIZE> netTxMeters_;
    std::array<std::atomic<float>, MATRIX_SIZE> netRxMeters_;
    std::array<std::atomic<float>, MATRIX_SIZE> dawOutMeters_;
};

} // namespace pluginbridge
