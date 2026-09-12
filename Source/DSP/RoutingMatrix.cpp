#include "RoutingMatrix.h"
#include <algorithm>

namespace pluginbridge
{

RoutingMatrix::RoutingMatrix()
{
    reset();
}

void RoutingMatrix::reset()
{
    for (int i = 0; i < MATRIX_SIZE; ++i)
    {
        for (int j = 0; j < MATRIX_SIZE; ++j)
        {
            inToTx_[i][j].store(i == j); // Default 1:1 routing
            rxToOut_[i][j].store(i == j); // Default 1:1 routing
            inToOut_[i][j].store(false);
        }
        txMute_[i].store(false);
        rxMute_[i].store(false);
        txGain_[i].store(1.0f);
        rxGain_[i].store(1.0f);

        inputChannelMap_[i].store(i);
        outputChannelMap_[i].store(i);

        dawInMeters_[i].store(0.0f);
        netTxMeters_[i].store(0.0f);
        netRxMeters_[i].store(0.0f);
        dawOutMeters_[i].store(0.0f);
    }
}

void RoutingMatrix::setInputToTx(int inCh, int txCh, bool connected)
{
    if (inCh >= 0 && inCh < MATRIX_SIZE && txCh >= 0 && txCh < MATRIX_SIZE)
        inToTx_[inCh][txCh].store(connected, std::memory_order_relaxed);
}

bool RoutingMatrix::getInputToTx(int inCh, int txCh) const
{
    if (inCh >= 0 && inCh < MATRIX_SIZE && txCh >= 0 && txCh < MATRIX_SIZE)
        return inToTx_[inCh][txCh].load(std::memory_order_relaxed);
    return false;
}

void RoutingMatrix::setRxToOutput(int rxCh, int outCh, bool connected)
{
    if (rxCh >= 0 && rxCh < MATRIX_SIZE && outCh >= 0 && outCh < MATRIX_SIZE)
        rxToOut_[rxCh][outCh].store(connected, std::memory_order_relaxed);
}

bool RoutingMatrix::getRxToOutput(int rxCh, int outCh) const
{
    if (rxCh >= 0 && rxCh < MATRIX_SIZE && outCh >= 0 && outCh < MATRIX_SIZE)
        return rxToOut_[rxCh][outCh].load(std::memory_order_relaxed);
    return false;
}

void RoutingMatrix::setInputToOutput(int inCh, int outCh, bool connected)
{
    if (inCh >= 0 && inCh < MATRIX_SIZE && outCh >= 0 && outCh < MATRIX_SIZE)
        inToOut_[inCh][outCh].store(connected, std::memory_order_relaxed);
}

bool RoutingMatrix::getInputToOutput(int inCh, int outCh) const
{
    if (inCh >= 0 && inCh < MATRIX_SIZE && outCh >= 0 && outCh < MATRIX_SIZE)
        return inToOut_[inCh][outCh].load(std::memory_order_relaxed);
    return false;
}

void RoutingMatrix::setInputChannelMap(int matrixCh, int hwInCh)
{
    if (matrixCh >= 0 && matrixCh < MATRIX_SIZE)
        inputChannelMap_[matrixCh].store(hwInCh, std::memory_order_relaxed);
}

int RoutingMatrix::getInputChannelMap(int matrixCh) const
{
    if (matrixCh >= 0 && matrixCh < MATRIX_SIZE)
        return inputChannelMap_[matrixCh].load(std::memory_order_relaxed);
    return matrixCh;
}

void RoutingMatrix::setOutputChannelMap(int matrixCh, int hwOutCh)
{
    if (matrixCh >= 0 && matrixCh < MATRIX_SIZE)
        outputChannelMap_[matrixCh].store(hwOutCh, std::memory_order_relaxed);
}

int RoutingMatrix::getOutputChannelMap(int matrixCh) const
{
    if (matrixCh >= 0 && matrixCh < MATRIX_SIZE)
        return outputChannelMap_[matrixCh].load(std::memory_order_relaxed);
    return matrixCh;
}

void RoutingMatrix::setInputChannelOffset(int offset)
{
    for (int i = 0; i < MATRIX_SIZE; ++i)
    {
        inputChannelMap_[i].store((offset + i) % MATRIX_SIZE, std::memory_order_relaxed);
    }
}

void RoutingMatrix::setOutputChannelOffset(int offset)
{
    for (int i = 0; i < MATRIX_SIZE; ++i)
    {
        outputChannelMap_[i].store((offset + i) % MATRIX_SIZE, std::memory_order_relaxed);
    }
}

void RoutingMatrix::resetInputChannelMap()
{
    for (int i = 0; i < MATRIX_SIZE; ++i)
        inputChannelMap_[i].store(i, std::memory_order_relaxed);
}

void RoutingMatrix::resetOutputChannelMap()
{
    for (int i = 0; i < MATRIX_SIZE; ++i)
        outputChannelMap_[i].store(i, std::memory_order_relaxed);
}

void RoutingMatrix::setInputBank(int startMatrixCh, int startHwCh, int count)
{
    for (int i = 0; i < count; ++i)
    {
        int m = startMatrixCh + i;
        int h = (startHwCh + i) % MATRIX_SIZE;
        if (m >= 0 && m < MATRIX_SIZE)
            inputChannelMap_[m].store(h, std::memory_order_relaxed);
    }
}

void RoutingMatrix::setOutputBank(int startMatrixCh, int startHwCh, int count)
{
    for (int i = 0; i < count; ++i)
    {
        int m = startMatrixCh + i;
        int h = (startHwCh + i) % MATRIX_SIZE;
        if (m >= 0 && m < MATRIX_SIZE)
            outputChannelMap_[m].store(h, std::memory_order_relaxed);
    }
}

void RoutingMatrix::setTxChannelMute(int txCh, bool muted)
{
    if (txCh >= 0 && txCh < MATRIX_SIZE)
        txMute_[txCh].store(muted, std::memory_order_relaxed);
}

bool RoutingMatrix::isTxChannelMuted(int txCh) const
{
    if (txCh >= 0 && txCh < MATRIX_SIZE)
        return txMute_[txCh].load(std::memory_order_relaxed);
    return false;
}

void RoutingMatrix::setRxChannelMute(int rxCh, bool muted)
{
    if (rxCh >= 0 && rxCh < MATRIX_SIZE)
        rxMute_[rxCh].store(muted, std::memory_order_relaxed);
}

bool RoutingMatrix::isRxChannelMuted(int rxCh) const
{
    if (rxCh >= 0 && rxCh < MATRIX_SIZE)
        return rxMute_[rxCh].load(std::memory_order_relaxed);
    return false;
}

void RoutingMatrix::setTxChannelGain(int txCh, float gainLinear)
{
    if (txCh >= 0 && txCh < MATRIX_SIZE)
        txGain_[txCh].store(std::max(0.0f, gainLinear), std::memory_order_relaxed);
}

float RoutingMatrix::getTxChannelGain(int txCh) const
{
    if (txCh >= 0 && txCh < MATRIX_SIZE)
        return txGain_[txCh].load(std::memory_order_relaxed);
    return 1.0f;
}

void RoutingMatrix::setRxChannelGain(int rxCh, float gainLinear)
{
    if (rxCh >= 0 && rxCh < MATRIX_SIZE)
        rxGain_[rxCh].store(std::max(0.0f, gainLinear), std::memory_order_relaxed);
}

float RoutingMatrix::getRxChannelGain(int rxCh) const
{
    if (rxCh >= 0 && rxCh < MATRIX_SIZE)
        return rxGain_[rxCh].load(std::memory_order_relaxed);
    return 1.0f;
}

void RoutingMatrix::process(const float* const* dawIn, int numDawIn,
                            float* const* networkTx, int numNetTx,
                            const float* const* networkRx, int numNetRx,
                            float* const* dawOut, int numDawOut,
                            int numFrames)
{
    if (numFrames <= 0)
        return;

    // Clear network TX buffers
    for (int txCh = 0; txCh < numNetTx; ++txCh)
    {
        if (networkTx[txCh] != nullptr)
            std::fill(networkTx[txCh], networkTx[txCh] + numFrames, 0.0f);
    }

    // Clear DAW Out buffers
    for (int outCh = 0; outCh < numDawOut; ++outCh)
    {
        if (dawOut[outCh] != nullptr)
            std::fill(dawOut[outCh], dawOut[outCh] + numFrames, 0.0f);
    }

    // Meter decay factor (~20dB/sec at typical buffer sizes)
    const float decay = 0.92f;

    // 1. Process DAW In -> Network TX and Thru -> DAW Out
    for (int inCh = 0; inCh < MATRIX_SIZE; ++inCh)
    {
        int hwIn = inputChannelMap_[inCh].load(std::memory_order_relaxed);
        const float* src = (hwIn >= 0 && hwIn < numDawIn) ? dawIn[hwIn] : nullptr;
        float peak = dawInMeters_[inCh].load(std::memory_order_relaxed) * decay;

        if (src != nullptr)
        {
            for (int f = 0; f < numFrames; ++f)
            {
                float s = std::abs(src[f]);
                if (s > peak) peak = s;
            }

            // Route to Network TX
            for (int txCh = 0; txCh < std::min(numNetTx, MATRIX_SIZE); ++txCh)
            {
                if (inToTx_[inCh][txCh].load(std::memory_order_relaxed) && networkTx[txCh] != nullptr)
                {
                    for (int f = 0; f < numFrames; ++f)
                    {
                        networkTx[txCh][f] += src[f];
                    }
                }
            }

            // Route Thru to DAW Out
            for (int outCh = 0; outCh < MATRIX_SIZE; ++outCh)
            {
                int hwOut = outputChannelMap_[outCh].load(std::memory_order_relaxed);
                if (inToOut_[inCh][outCh].load(std::memory_order_relaxed) && hwOut >= 0 && hwOut < numDawOut && dawOut[hwOut] != nullptr)
                {
                    for (int f = 0; f < numFrames; ++f)
                    {
                        dawOut[hwOut][f] += src[f];
                    }
                }
            }
        }
        dawInMeters_[inCh].store(peak, std::memory_order_relaxed);
    }

    // Apply TX Channel Gain and Mute, update TX meters
    for (int txCh = 0; txCh < std::min(numNetTx, MATRIX_SIZE); ++txCh)
    {
        float peak = netTxMeters_[txCh].load(std::memory_order_relaxed) * decay;
        if (networkTx[txCh] != nullptr)
        {
            bool muted = txMute_[txCh].load(std::memory_order_relaxed);
            float gain = txGain_[txCh].load(std::memory_order_relaxed);

            for (int f = 0; f < numFrames; ++f)
            {
                if (muted)
                {
                    networkTx[txCh][f] = 0.0f;
                }
                else
                {
                    networkTx[txCh][f] *= gain;
                    float s = std::abs(networkTx[txCh][f]);
                    if (s > peak) peak = s;
                }
            }
        }
        netTxMeters_[txCh].store(peak, std::memory_order_relaxed);
    }

    // 2. Process Network RX -> DAW Out
    for (int rxCh = 0; rxCh < std::min(numNetRx, MATRIX_SIZE); ++rxCh)
    {
        const float* src = networkRx[rxCh];
        float peak = netRxMeters_[rxCh].load(std::memory_order_relaxed) * decay;

        if (src != nullptr)
        {
            bool muted = rxMute_[rxCh].load(std::memory_order_relaxed);
            float gain = rxGain_[rxCh].load(std::memory_order_relaxed);

            for (int f = 0; f < numFrames; ++f)
            {
                float s = std::abs(src[f]) * gain;
                if (!muted && s > peak) peak = s;
            }

            if (!muted)
            {
                for (int outCh = 0; outCh < MATRIX_SIZE; ++outCh)
                {
                    int hwOut = outputChannelMap_[outCh].load(std::memory_order_relaxed);
                    if (rxToOut_[rxCh][outCh].load(std::memory_order_relaxed) && hwOut >= 0 && hwOut < numDawOut && dawOut[hwOut] != nullptr)
                    {
                        for (int f = 0; f < numFrames; ++f)
                        {
                            dawOut[hwOut][f] += src[f] * gain;
                        }
                    }
                }
            }
        }
        netRxMeters_[rxCh].store(peak, std::memory_order_relaxed);
    }

    // Measure final DAW Out meters
    for (int outCh = 0; outCh < MATRIX_SIZE; ++outCh)
    {
        int hwOut = outputChannelMap_[outCh].load(std::memory_order_relaxed);
        float peak = dawOutMeters_[outCh].load(std::memory_order_relaxed) * decay;
        if (hwOut >= 0 && hwOut < numDawOut && dawOut[hwOut] != nullptr)
        {
            for (int f = 0; f < numFrames; ++f)
            {
                float s = std::abs(dawOut[hwOut][f]);
                if (s > peak) peak = s;
            }
        }
        dawOutMeters_[outCh].store(peak, std::memory_order_relaxed);
    }
}

float RoutingMatrix::getDawInPeak(int ch) const
{
    if (ch >= 0 && ch < MATRIX_SIZE)
        return dawInMeters_[ch].load(std::memory_order_relaxed);
    return 0.0f;
}

float RoutingMatrix::getNetworkTxPeak(int ch) const
{
    if (ch >= 0 && ch < MATRIX_SIZE)
        return netTxMeters_[ch].load(std::memory_order_relaxed);
    return 0.0f;
}

float RoutingMatrix::getNetworkRxPeak(int ch) const
{
    if (ch >= 0 && ch < MATRIX_SIZE)
        return netRxMeters_[ch].load(std::memory_order_relaxed);
    return 0.0f;
}

float RoutingMatrix::getDawOutPeak(int ch) const
{
    if (ch >= 0 && ch < MATRIX_SIZE)
        return dawOutMeters_[ch].load(std::memory_order_relaxed);
    return 0.0f;
}

} // namespace pluginbridge

