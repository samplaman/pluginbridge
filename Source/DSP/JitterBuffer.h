#pragma once

#include "../Network/NetworkProtocol.h"
#include <vector>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <cmath>

namespace pluginbridge
{

struct QueuedPacket
{
    uint32_t sequenceNumber { 0 };
    uint64_t timestampUs { 0 };
    uint16_t numChannels { 0 };
    uint16_t numFrames { 0 };
    std::vector<float> samples; // Interleaved or planar
    bool valid { false };
};

class JitterBuffer
{
public:
    JitterBuffer(int maxChannels = MAX_CHANNELS, int targetBufferPackets = 3);
    ~JitterBuffer() = default;

    void setTargetBufferPackets(int packets);
    int getTargetBufferPackets() const { return targetBufferPackets_.load(); }

    void reset();

    /** Called by network RX thread when a valid audio packet arrives */
    void pushPacket(const AudioPacketHeader& header, const float* audioPayload, size_t payloadBytes);

    /** Called by DAW audio thread to retrieve audio frames */
    void readFrames(float* const* outputChannels, int numChannels, int numFrames);

    // Diagnostics / Statistics
    int getBufferFillFrames() const;
    float getBufferFillPackets() const;
    uint32_t getPacketsReceived() const { return packetsReceived_.load(); }
    uint32_t getPacketsLost() const { return packetsLost_.load(); }
    uint32_t getUnderruns() const { return underruns_.load(); }
    uint32_t getOverruns() const { return overruns_.load(); }
    float getPacketLossPercentage() const;

private:
    static bool seqGreaterThan(uint32_t a, uint32_t b)
    {
        return static_cast<int32_t>(a - b) > 0;
    }

    const int maxChannels_;
    std::atomic<int> targetBufferPackets_ { 3 };

    mutable std::mutex queueMutex_;
    std::vector<QueuedPacket> packetQueue_;
    static constexpr size_t MAX_QUEUE_SIZE = 32;

    bool isInitialized_ { false };
    uint32_t nextExpectedSeq_ { 0 };
    uint16_t streamChannels_ { 2 };

    // Internal intermediate staging ring buffer
    std::vector<std::vector<float>> ringBuffers_;
    int ringCapacity_ { 8192 };
    int readIndex_ { 0 };
    int writeIndex_ { 0 };
    std::atomic<int> availableFrames_ { 0 };

    // Packet Loss Concealment (PLC) state
    float plcGain_ { 1.0f };

    // Metrics
    std::atomic<uint32_t> packetsReceived_ { 0 };
    std::atomic<uint32_t> packetsLost_ { 0 };
    std::atomic<uint32_t> underruns_ { 0 };
    std::atomic<uint32_t> overruns_ { 0 };
};

} // namespace pluginbridge
