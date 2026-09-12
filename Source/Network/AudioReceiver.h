#pragma once

#include "NetworkProtocol.h"
#include "../DSP/JitterBuffer.h"
#include "../DSP/DriftCompensator.h"
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>

namespace pluginbridge
{

class AudioReceiver
{
public:
    AudioReceiver();
    ~AudioReceiver();

    bool start(uint16_t listenPort = DEFAULT_AUDIO_PORT);
    void stop();

    void setTargetStreamId(const std::string& streamId);
    std::string getTargetStreamId() const;

    void setJitterBufferPackets(int packets);
    int getJitterBufferPackets() const;

    uint16_t getListeningPort() const { return listenPort_; }

    /**
     * Called by DAW audio thread: fetches resampled, drift-compensated audio frames.
     */
    void readAudio(float* const* channels, int numChannels, int numFrames);

    // Diagnostics
    uint32_t getPacketsReceived() const;
    uint32_t getPacketsLost() const;
    float getPacketLossPercentage() const;
    float getBufferFillPackets() const;
    float getCurrentSpeedRatio() const;
    float getBitrateKbps() const;

    void setLocalSampleRate(uint32_t rate) { localSampleRate_.store(rate); }
    uint32_t getRemoteSampleRate() const { return remoteSampleRate_.load(); }

    std::string getLastSenderIp() const;
    uint16_t getLastSenderPort() const;
    uint64_t getLastPacketTimeMs() const;

private:
    void receiveWorkerLoop();

    std::atomic<bool> running_ { false };
    std::thread workerThread_;

    uint16_t listenPort_ { DEFAULT_AUDIO_PORT };
    int listenSocket_ { -1 };

    std::atomic<uint32_t> localSampleRate_ { 48000 };
    std::atomic<uint32_t> remoteSampleRate_ { 48000 };

    std::string targetStreamId_;
    mutable std::mutex streamFilterMutex_;

    JitterBuffer jitterBuffer_;
    DriftCompensator driftCompensator_;

    // Intermediate scratch buffer for drift compensator processing
    std::vector<std::vector<float>> scratchBuffer_;
    std::vector<float*> scratchPointers_;

    // Metrics
    std::atomic<uint64_t> bytesReceived_ { 0 };
    std::atomic<uint64_t> lastBytesReceived_ { 0 };
    std::atomic<float> bitrateKbps_ { 0.0f };
    uint64_t lastBitrateTimeMs_ { 0 };

    mutable std::mutex lastSenderMutex_;
    std::string lastSenderIp_;
    uint16_t lastSenderPort_ { 0 };
    std::atomic<uint64_t> lastPacketTimeMs_ { 0 };
};

} // namespace pluginbridge

