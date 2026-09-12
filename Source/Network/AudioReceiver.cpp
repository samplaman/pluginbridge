#include "AudioReceiver.h"
#include <iostream>
#include <chrono>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <poll.h>
#include <pthread.h>
#include <algorithm>
#include <cstring>
#include <ifaddrs.h>
#include <net/if.h>

namespace pluginbridge
{

static uint64_t getCurrentTimeMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

AudioReceiver::AudioReceiver()
    : jitterBuffer_(MAX_CHANNELS, 3),
      driftCompensator_(MAX_CHANNELS)
{
    scratchBuffer_.resize(MAX_CHANNELS);
    scratchPointers_.resize(MAX_CHANNELS);
    for (int ch = 0; ch < MAX_CHANNELS; ++ch)
    {
        scratchBuffer_[ch].resize(2048, 0.0f);
        scratchPointers_[ch] = scratchBuffer_[ch].data();
    }
}

AudioReceiver::~AudioReceiver()
{
    stop();
}

bool AudioReceiver::start(uint16_t listenPort)
{
    if (running_.load())
        stop();

    listenPort_ = listenPort;

    listenSocket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (listenSocket_ < 0)
        return false;

    int reuse = 1;
    setsockopt(listenSocket_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#ifdef SO_REUSEPORT
    setsockopt(listenSocket_, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
#endif

    // Large receive buffer to prevent dropping packets under heavy 48-channel loads
    int rcvBufSize = 4 * 1024 * 1024; // 4 MB
    setsockopt(listenSocket_, SOL_SOCKET, SO_RCVBUF, &rcvBufSize, sizeof(rcvBufSize));

    sockaddr_in bindAddr {};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(listenPort_);
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listenSocket_, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) < 0)
    {
        close(listenSocket_);
        listenSocket_ = -1;
        return false;
    }

    // Join multicast group so we can receive multicast broadcast streams
    ip_mreq mreq {};
    mreq.imr_multiaddr.s_addr = inet_addr(DEFAULT_MULTICAST_GROUP);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    setsockopt(listenSocket_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));

    ifaddrs* ifap = nullptr;
    if (getifaddrs(&ifap) == 0 && ifap != nullptr)
    {
        for (ifaddrs* ifa = ifap; ifa != nullptr; ifa = ifa->ifa_next)
        {
            if (ifa->ifa_addr == nullptr || ifa->ifa_addr->sa_family != AF_INET)
                continue;
            if ((ifa->ifa_flags & IFF_LOOPBACK) || !(ifa->ifa_flags & IFF_UP))
                continue;

            ip_mreq ifMreq {};
            ifMreq.imr_multiaddr.s_addr = inet_addr(DEFAULT_MULTICAST_GROUP);
            ifMreq.imr_interface = reinterpret_cast<sockaddr_in*>(ifa->ifa_addr)->sin_addr;
            setsockopt(listenSocket_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &ifMreq, sizeof(ifMreq));
        }
        freeifaddrs(ifap);
    }

    jitterBuffer_.reset();
    driftCompensator_.reset();

    running_.store(true);
    lastBitrateTimeMs_ = getCurrentTimeMs();
    workerThread_ = std::thread(&AudioReceiver::receiveWorkerLoop, this);
    return true;
}

void AudioReceiver::stop()
{
    if (!running_.exchange(false))
        return;

    if (listenSocket_ >= 0)
    {
        close(listenSocket_);
        listenSocket_ = -1;
    }

    if (workerThread_.joinable())
        workerThread_.join();

    jitterBuffer_.reset();
    driftCompensator_.reset();
}

void AudioReceiver::setTargetStreamId(const std::string& streamId)
{
    std::lock_guard<std::mutex> lock(streamFilterMutex_);
    targetStreamId_ = streamId;
}

std::string AudioReceiver::getTargetStreamId() const
{
    std::lock_guard<std::mutex> lock(streamFilterMutex_);
    return targetStreamId_;
}

void AudioReceiver::setJitterBufferPackets(int packets)
{
    jitterBuffer_.setTargetBufferPackets(packets);
    driftCompensator_.setTargetBufferFrames(packets * 128);
}

int AudioReceiver::getJitterBufferPackets() const
{
    return jitterBuffer_.getTargetBufferPackets();
}

void AudioReceiver::readAudio(float* const* channels, int numChannels, int numFrames)
{
    if (channels == nullptr || numChannels <= 0 || numFrames <= 0)
        return;

    uint32_t remRate = remoteSampleRate_.load(std::memory_order_relaxed);
    uint32_t locRate = localSampleRate_.load(std::memory_order_relaxed);
    if (remRate > 0 && locRate > 0)
    {
        driftCompensator_.setNominalRatio(static_cast<double>(remRate) / static_cast<double>(locRate));
    }

    float speedRatio = driftCompensator_.getCurrentSpeedRatio();
    int inputFramesToRead = static_cast<int>(std::ceil(numFrames * std::max(1.0f, speedRatio))) + 48;

    // Need enough scratch capacity
    int maxReq = std::max(inputFramesToRead * 2, 512);
    if (static_cast<int>(scratchBuffer_[0].size()) < maxReq)
    {
        for (int ch = 0; ch < MAX_CHANNELS; ++ch)
        {
            scratchBuffer_[ch].resize(maxReq, 0.0f);
            scratchPointers_[ch] = scratchBuffer_[ch].data();
        }
    }

    // Read raw frames from jitter buffer
    jitterBuffer_.readFrames(scratchPointers_.data(), numChannels, inputFramesToRead);

    // Apply drift compensation / fractional resampling
    int fill = jitterBuffer_.getBufferFillFrames();
    driftCompensator_.process(scratchPointers_.data(), inputFramesToRead,
                             channels, numFrames,
                             numChannels, fill);
}

void AudioReceiver::receiveWorkerLoop()
{
#ifdef __APPLE__
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif

    std::vector<uint8_t> buffer(sizeof(AudioPacketHeader) + MAX_PAYLOAD_BYTES);

    while (running_)
    {
        pollfd pfd {};
        pfd.fd = listenSocket_;
        pfd.events = POLLIN;

        int res = poll(&pfd, 1, 100);
        if (res > 0)
        {
            // std::cout << "poll revents=" << pfd.revents << std::endl;
        }
        if (res <= 0 || !(pfd.revents & POLLIN))
            continue;

        sockaddr_in senderAddr {};
        socklen_t addrLen = sizeof(senderAddr);

        ssize_t bytesRead = recvfrom(listenSocket_, buffer.data(), buffer.size(), 0,
                                     reinterpret_cast<sockaddr*>(&senderAddr), &addrLen);

        if (bytesRead < 0)
        {
            // socket error
            continue;
        }

        if (bytesRead >= static_cast<ssize_t>(sizeof(AudioPacketHeader)))
        {
            const auto* header = reinterpret_cast<const AudioPacketHeader*>(buffer.data());

            if (header->magic == AUDIO_MAGIC && header->version == PROTOCOL_VER)
            {
                char ipBuf[INET_ADDRSTRLEN] = {0};
                inet_ntop(AF_INET, &senderAddr.sin_addr, ipBuf, sizeof(ipBuf));
                uint16_t senderPort = ntohs(senderAddr.sin_port);

                {
                    std::lock_guard<std::mutex> lock(lastSenderMutex_);
                    lastSenderIp_ = ipBuf;
                    lastSenderPort_ = senderPort;
                }
                lastPacketTimeMs_.store(getCurrentTimeMs(), std::memory_order_relaxed);
                if (header->sampleRate > 0)
                {
                    remoteSampleRate_.store(header->sampleRate, std::memory_order_relaxed);
                }

                // Check filter if set
                std::string filter;
                {
                    std::lock_guard<std::mutex> lock(streamFilterMutex_);
                    filter = targetStreamId_;
                }

                if (filter.empty() || filter == header->streamId)
                {
                    const float* payload = reinterpret_cast<const float*>(buffer.data() + sizeof(AudioPacketHeader));
                    size_t payloadBytes = bytesRead - sizeof(AudioPacketHeader);

                    jitterBuffer_.pushPacket(*header, payload, payloadBytes);
                    bytesReceived_.fetch_add(bytesRead, std::memory_order_relaxed);
                }
            }
        }

        // Bitrate calculation
        uint64_t now = getCurrentTimeMs();
        if (now - lastBitrateTimeMs_ >= 500)
        {
            uint64_t totalBytes = bytesReceived_.load(std::memory_order_relaxed);
            uint64_t deltaBytes = totalBytes - lastBytesReceived_.load(std::memory_order_relaxed);
            float elapsedSec = static_cast<float>(now - lastBitrateTimeMs_) / 1000.0f;
            if (elapsedSec > 0.0f)
            {
                bitrateKbps_.store((deltaBytes * 8.0f) / (elapsedSec * 1000.0f), std::memory_order_relaxed);
            }
            lastBytesReceived_.store(totalBytes, std::memory_order_relaxed);
            lastBitrateTimeMs_ = now;
        }
    }
}

uint32_t AudioReceiver::getPacketsReceived() const
{
    return jitterBuffer_.getPacketsReceived();
}

uint32_t AudioReceiver::getPacketsLost() const
{
    return jitterBuffer_.getPacketsLost();
}

float AudioReceiver::getPacketLossPercentage() const
{
    return jitterBuffer_.getPacketLossPercentage();
}

float AudioReceiver::getBufferFillPackets() const
{
    return jitterBuffer_.getBufferFillPackets();
}

float AudioReceiver::getCurrentSpeedRatio() const
{
    return driftCompensator_.getCurrentSpeedRatio();
}

float AudioReceiver::getBitrateKbps() const
{
    return bitrateKbps_.load(std::memory_order_relaxed);
}

std::string AudioReceiver::getLastSenderIp() const
{
    std::lock_guard<std::mutex> lock(lastSenderMutex_);
    return lastSenderIp_;
}

uint16_t AudioReceiver::getLastSenderPort() const
{
    std::lock_guard<std::mutex> lock(lastSenderMutex_);
    return lastSenderPort_;
}

uint64_t AudioReceiver::getLastPacketTimeMs() const
{
    return lastPacketTimeMs_.load(std::memory_order_relaxed);
}

} // namespace pluginbridge
