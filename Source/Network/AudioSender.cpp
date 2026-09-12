#include "AudioSender.h"
#include <iostream>
#include <chrono>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <pthread.h>
#include <algorithm>
#include <cstring>

namespace pluginbridge
{

static uint64_t getCurrentTimeUs()
{
    using namespace std::chrono;
    return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

static uint64_t getCurrentTimeMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

AudioSender::AudioSender()
{
    fifoBuffers_.resize(MAX_CHANNELS);
    for (int ch = 0; ch < MAX_CHANNELS; ++ch)
    {
        fifoBuffers_[ch].resize(FIFO_CAPACITY, 0.0f);
    }

    char host[32] = {0};
    if (gethostname(host, sizeof(host) - 1) == 0)
        senderHost_ = host;
}

AudioSender::~AudioSender()
{
    stop();
}

void AudioSender::start()
{
    if (running_.exchange(true))
        return;

    sendSocket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sendSocket_ >= 0)
    {
        // Increase socket send buffer for 48-channel high-throughput streaming
        int sendBufSize = 2 * 1024 * 1024; // 2 MB
        setsockopt(sendSocket_, SOL_SOCKET, SO_SNDBUF, &sendBufSize, sizeof(sendBufSize));

        int broadcastEnable = 1;
        setsockopt(sendSocket_, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

        uint8_t loop = 1;
        setsockopt(sendSocket_, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));

        uint8_t ttl = 2;
        setsockopt(sendSocket_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));
    }

    lastBitrateCheckMs_ = getCurrentTimeMs();
    workerThread_ = std::thread(&AudioSender::sendWorkerLoop, this);
}

void AudioSender::stop()
{
    if (!running_.exchange(false))
        return;

    if (workerThread_.joinable())
        workerThread_.join();

    if (sendSocket_ >= 0)
    {
        close(sendSocket_);
        sendSocket_ = -1;
    }
}

void AudioSender::setStreamInfo(const std::string& streamId,
                                const std::string& streamName,
                                uint32_t sampleRate,
                                uint16_t numChannels)
{
    std::lock_guard<std::mutex> lock(streamInfoMutex_);
    streamId_ = streamId;
    streamName_ = streamName;
    sampleRate_.store(sampleRate);
    numChannels_.store(std::min<uint16_t>(numChannels, MAX_CHANNELS));
}

void AudioSender::addTarget(const std::string& ipAddress, uint16_t port)
{
    std::lock_guard<std::mutex> lock(targetsMutex_);
    for (const auto& t : targets_)
    {
        if (t.ipAddress == ipAddress && t.port == port)
            return;
    }
    SendTarget target;
    target.ipAddress = ipAddress;
    target.port = port;
    target.isMulticast = (ipAddress.rfind("239.", 0) == 0 || ipAddress.rfind("224.", 0) == 0);
    targets_.push_back(target);
}

void AudioSender::removeTarget(const std::string& ipAddress, uint16_t port)
{
    std::lock_guard<std::mutex> lock(targetsMutex_);
    targets_.erase(std::remove_if(targets_.begin(), targets_.end(), [&](const SendTarget& t) {
        return t.ipAddress == ipAddress && t.port == port;
    }), targets_.end());
}

void AudioSender::clearTargets()
{
    std::lock_guard<std::mutex> lock(targetsMutex_);
    targets_.clear();
}

void AudioSender::setPacketFrames(int frames)
{
    packetFrames_.store(std::clamp(frames, 32, MAX_FRAMES_PER_PACKET));
}

void AudioSender::pushAudio(const float* const* channelData, int numChannels, int numFrames)
{
    if (numFrames <= 0 || channelData == nullptr)
        return;

    int channelsToCopy = std::min(numChannels, MAX_CHANNELS);
    int currentAvail = fifoAvailable_.load(std::memory_order_relaxed);

    if (currentAvail + numFrames > FIFO_CAPACITY)
    {
        // Avoid overflow: drop oldest audio
        int drop = (currentAvail + numFrames) - FIFO_CAPACITY;
        int rIdx = fifoReadIdx_.load(std::memory_order_relaxed);
        fifoReadIdx_.store((rIdx + drop) % FIFO_CAPACITY, std::memory_order_relaxed);
        fifoAvailable_.fetch_sub(drop, std::memory_order_relaxed);
    }

    int wIdx = fifoWriteIdx_.load(std::memory_order_relaxed);

    for (int ch = 0; ch < channelsToCopy; ++ch)
    {
        const float* src = channelData[ch];
        if (src == nullptr)
            continue;

        for (int f = 0; f < numFrames; ++f)
        {
            int pos = (wIdx + f) % FIFO_CAPACITY;
            fifoBuffers_[ch][pos] = src[f];
        }
    }

    // Zero any channels not provided
    for (int ch = channelsToCopy; ch < MAX_CHANNELS; ++ch)
    {
        for (int f = 0; f < numFrames; ++f)
        {
            int pos = (wIdx + f) % FIFO_CAPACITY;
            fifoBuffers_[ch][pos] = 0.0f;
        }
    }

    fifoWriteIdx_.store((wIdx + numFrames) % FIFO_CAPACITY, std::memory_order_release);
    fifoAvailable_.fetch_add(numFrames, std::memory_order_release);
}

void AudioSender::sendWorkerLoop()
{
#ifdef __APPLE__
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif

    std::vector<uint8_t> packetBuffer(sizeof(AudioPacketHeader) + MAX_PAYLOAD_BYTES);
    auto* header = reinterpret_cast<AudioPacketHeader*>(packetBuffer.data());
    float* payload = reinterpret_cast<float*>(packetBuffer.data() + sizeof(AudioPacketHeader));

    while (running_)
    {
        int targetChunk = packetFrames_.load(std::memory_order_relaxed);
        int available = fifoAvailable_.load(std::memory_order_acquire);

        if (available < targetChunk)
        {
            // Spin-sleep for sub-millisecond precision
            std::this_thread::sleep_for(std::chrono::microseconds(200));
            continue;
        }

        int chCount = numChannels_.load(std::memory_order_relaxed);
        int rIdx = fifoReadIdx_.load(std::memory_order_relaxed);

        // Fill Header
        header->magic = AUDIO_MAGIC;
        header->version = PROTOCOL_VER;
        header->flags = 0;
        header->numChannels = static_cast<uint16_t>(chCount);
        header->sampleRate = sampleRate_.load(std::memory_order_relaxed);
        header->numFrames = static_cast<uint16_t>(targetChunk);
        header->bitDepth = 32;
        header->reserved = 0;
        header->sequenceNumber = sequenceCounter_++;
        header->timestampUs = getCurrentTimeUs();

        {
            std::lock_guard<std::mutex> lock(streamInfoMutex_);
            std::strncpy(header->streamId, streamId_.c_str(), sizeof(header->streamId) - 1);
            std::strncpy(header->streamName, streamName_.c_str(), sizeof(header->streamName) - 1);
            std::strncpy(header->senderHost, senderHost_.c_str(), sizeof(header->senderHost) - 1);
        }

        // Interleave audio samples into payload
        for (int f = 0; f < targetChunk; ++f)
        {
            int pos = (rIdx + f) % FIFO_CAPACITY;
            for (int ch = 0; ch < chCount; ++ch)
            {
                payload[f * chCount + ch] = fifoBuffers_[ch][pos];
            }
        }

        // Advance FIFO
        fifoReadIdx_.store((rIdx + targetChunk) % FIFO_CAPACITY, std::memory_order_release);
        fifoAvailable_.fetch_sub(targetChunk, std::memory_order_release);

        size_t packetSize = sizeof(AudioPacketHeader) + (chCount * targetChunk * sizeof(float));

        // Transmit to active targets
        if (sendSocket_ >= 0)
        {
            std::vector<SendTarget> currentTargets;
            {
                std::lock_guard<std::mutex> lock(targetsMutex_);
                currentTargets = targets_;
            }

            for (const auto& target : currentTargets)
            {
                sockaddr_in destAddr {};
                destAddr.sin_family = AF_INET;
                destAddr.sin_port = htons(target.port);
                destAddr.sin_addr.s_addr = inet_addr(target.ipAddress.c_str());

                ssize_t sent = sendto(sendSocket_, packetBuffer.data(), packetSize, 0,
                                      reinterpret_cast<sockaddr*>(&destAddr), sizeof(destAddr));
                if (sent > 0)
                {
                    bytesSent_.fetch_add(sent, std::memory_order_relaxed);
                }
            }

            if (!currentTargets.empty())
            {
                packetsSent_.fetch_add(1, std::memory_order_relaxed);
            }
        }

        // Calculate bitrate every 500ms
        uint64_t now = getCurrentTimeMs();
        if (now - lastBitrateCheckMs_ >= 500)
        {
            uint64_t totalBytes = bytesSent_.load(std::memory_order_relaxed);
            uint64_t deltaBytes = totalBytes - lastBytesSent_.load(std::memory_order_relaxed);
            float elapsedSec = static_cast<float>(now - lastBitrateCheckMs_) / 1000.0f;
            if (elapsedSec > 0.0f)
            {
                currentBitrateKbps_.store((deltaBytes * 8.0f) / (elapsedSec * 1000.0f), std::memory_order_relaxed);
            }
            lastBytesSent_.store(totalBytes, std::memory_order_relaxed);
            lastBitrateCheckMs_ = now;
        }
    }
}

float AudioSender::getCurrentBitrateKbps() const
{
    return currentBitrateKbps_.load(std::memory_order_relaxed);
}

} // namespace pluginbridge
