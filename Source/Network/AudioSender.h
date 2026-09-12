#pragma once

#include "NetworkProtocol.h"
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace pluginbridge
{

struct SendTarget
{
    std::string ipAddress;
    uint16_t port { DEFAULT_AUDIO_PORT };
    bool isMulticast { false };
};

class AudioSender
{
public:
    AudioSender();
    ~AudioSender();

    void start();
    void stop();

    void setStreamInfo(const std::string& streamId,
                       const std::string& streamName,
                       uint32_t sampleRate,
                       uint16_t numChannels);

    void addTarget(const std::string& ipAddress, uint16_t port);
    void removeTarget(const std::string& ipAddress, uint16_t port);
    void clearTargets();

    void setPacketFrames(int frames); // e.g., 64, 128, 256
    int getPacketFrames() const { return packetFrames_.load(); }

    /**
     * Real-time audio thread: Push channels of audio into lock-free ring buffer.
     */
    void pushAudio(const float* const* channelData, int numChannels, int numFrames);

    // Stats
    uint64_t getPacketsSent() const { return packetsSent_.load(); }
    uint64_t getBytesSent() const { return bytesSent_.load(); }
    float getCurrentBitrateKbps() const;

private:
    void sendWorkerLoop();

    std::atomic<bool> running_ { false };
    std::thread workerThread_;

    std::atomic<int> packetFrames_ { 128 };
    std::atomic<uint32_t> sampleRate_ { 48000 };
    std::atomic<uint16_t> numChannels_ { 2 };

    std::string streamId_ { "default-stream" };
    std::string streamName_ { "Main-LAN" };
    std::string senderHost_ { "Workstation" };
    std::mutex streamInfoMutex_;

    mutable std::mutex targetsMutex_;
    std::vector<SendTarget> targets_;

    // Lock-free circular ring buffer between audio thread and sender thread
    static constexpr int FIFO_CAPACITY = 32768; // ~0.7 seconds at 48kHz
    std::vector<std::vector<float>> fifoBuffers_;
    std::atomic<int> fifoWriteIdx_ { 0 };
    std::atomic<int> fifoReadIdx_ { 0 };
    std::atomic<int> fifoAvailable_ { 0 };

    int sendSocket_ { -1 };
    uint32_t sequenceCounter_ { 0 };

    // Metrics
    std::atomic<uint64_t> packetsSent_ { 0 };
    std::atomic<uint64_t> bytesSent_ { 0 };
    std::atomic<uint64_t> lastBytesSent_ { 0 };
    std::atomic<float> currentBitrateKbps_ { 0.0f };
    uint64_t lastBitrateCheckMs_ { 0 };
};

} // namespace pluginbridge

