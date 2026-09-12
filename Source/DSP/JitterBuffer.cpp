#include "JitterBuffer.h"

namespace pluginbridge
{

JitterBuffer::JitterBuffer(int maxChannels, int targetBufferPackets)
    : maxChannels_(maxChannels),
      targetBufferPackets_(targetBufferPackets)
{
    ringBuffers_.resize(maxChannels_);
    for (int ch = 0; ch < maxChannels_; ++ch)
    {
        ringBuffers_[ch].resize(ringCapacity_, 0.0f);
    }
}

void JitterBuffer::setTargetBufferPackets(int packets)
{
    targetBufferPackets_.store(std::max(1, std::min(packets, 16)));
}

void JitterBuffer::reset()
{
    std::lock_guard<std::mutex> lock(queueMutex_);
    packetQueue_.clear();
    isInitialized_ = false;
    nextExpectedSeq_ = 0;
    readIndex_ = 0;
    writeIndex_ = 0;
    availableFrames_.store(0);
    plcGain_ = 1.0f;
    for (int ch = 0; ch < maxChannels_; ++ch)
    {
        std::fill(ringBuffers_[ch].begin(), ringBuffers_[ch].end(), 0.0f);
    }
}

void JitterBuffer::pushPacket(const AudioPacketHeader& header, const float* audioPayload, size_t payloadBytes)
{
    if (audioPayload == nullptr || header.numFrames == 0 || header.numChannels == 0)
        return;

    packetsReceived_.fetch_add(1, std::memory_order_relaxed);

    size_t expectedFloats = static_cast<size_t>(header.numChannels) * header.numFrames;
    if (payloadBytes < expectedFloats * sizeof(float))
        return;

    std::lock_guard<std::mutex> lock(queueMutex_);

    if (!isInitialized_)
    {
        isInitialized_ = true;
        nextExpectedSeq_ = header.sequenceNumber;
        streamChannels_ = std::min<uint16_t>(header.numChannels, static_cast<uint16_t>(maxChannels_));
    }

    // Check if packet is ridiculously stale (older than current window)
    if (seqGreaterThan(nextExpectedSeq_, header.sequenceNumber) && (nextExpectedSeq_ - header.sequenceNumber > 64))
    {
        return; // drop duplicate / ancient packet
    }

    // Unpack interleaved audio payload directly into our ring buffer if in order
    // or handle queueing
    int channelsToCopy = std::min<int>(header.numChannels, maxChannels_);
    int framesToCopy = header.numFrames;

    // Check capacity
    int currentAvail = availableFrames_.load(std::memory_order_relaxed);
    if (currentAvail + framesToCopy > ringCapacity_)
    {
        overruns_.fetch_add(1, std::memory_order_relaxed);
        // Advance read pointer to discard old audio and avoid infinite lag
        int discard = (currentAvail + framesToCopy) - ringCapacity_;
        readIndex_ = (readIndex_ + discard) % ringCapacity_;
        availableFrames_.fetch_sub(discard, std::memory_order_relaxed);
    }

    // Sequence jump / packet loss detection
    if (seqGreaterThan(header.sequenceNumber, nextExpectedSeq_))
    {
        uint32_t lost = header.sequenceNumber - nextExpectedSeq_;
        packetsLost_.fetch_add(lost, std::memory_order_relaxed);
    }
    nextExpectedSeq_ = header.sequenceNumber + 1;

    // Deinterleave and write to ring buffers
    for (int f = 0; f < framesToCopy; ++f)
    {
        int writePos = (writeIndex_ + f) % ringCapacity_;
        for (int ch = 0; ch < channelsToCopy; ++ch)
        {
            ringBuffers_[ch][writePos] = audioPayload[f * header.numChannels + ch];
        }
    }

    writeIndex_ = (writeIndex_ + framesToCopy) % ringCapacity_;
    availableFrames_.fetch_add(framesToCopy, std::memory_order_release);
}

void JitterBuffer::readFrames(float* const* outputChannels, int numChannels, int numFrames)
{
    int currentAvail = availableFrames_.load(std::memory_order_acquire);
    int targetPreload = targetBufferPackets_.load(std::memory_order_relaxed) * 128;

    // If we haven't preloaded enough frames after reset, output silence
    if (!isInitialized_ || currentAvail < targetPreload / 2)
    {
        if (isInitialized_ && currentAvail == 0)
        {
            underruns_.fetch_add(1, std::memory_order_relaxed);
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (outputChannels[ch] != nullptr)
            {
                std::fill(outputChannels[ch], outputChannels[ch] + numFrames, 0.0f);
            }
        }
        return;
    }

    int framesToRead = std::min(numFrames, currentAvail);
    int channelsToCopy = std::min(numChannels, maxChannels_);

    for (int ch = 0; ch < channelsToCopy; ++ch)
    {
        if (outputChannels[ch] == nullptr)
            continue;

        for (int f = 0; f < framesToRead; ++f)
        {
            int rPos = (readIndex_ + f) % ringCapacity_;
            outputChannels[ch][f] = ringBuffers_[ch][rPos];
        }

        // Fill remaining with silence / fade if underflow occurred
        if (framesToRead < numFrames)
        {
            std::fill(outputChannels[ch] + framesToRead, outputChannels[ch] + numFrames, 0.0f);
        }
    }

    // Zero any extra channels requested by DAW
    for (int ch = channelsToCopy; ch < numChannels; ++ch)
    {
        if (outputChannels[ch] != nullptr)
        {
            std::fill(outputChannels[ch], outputChannels[ch] + numFrames, 0.0f);
        }
    }

    readIndex_ = (readIndex_ + framesToRead) % ringCapacity_;
    availableFrames_.fetch_sub(framesToRead, std::memory_order_release);

    if (framesToRead < numFrames)
    {
        underruns_.fetch_add(1, std::memory_order_relaxed);
    }
}

int JitterBuffer::getBufferFillFrames() const
{
    return availableFrames_.load(std::memory_order_relaxed);
}

float JitterBuffer::getBufferFillPackets() const
{
    return static_cast<float>(getBufferFillFrames()) / 128.0f;
}

float JitterBuffer::getPacketLossPercentage() const
{
    uint32_t rcv = packetsReceived_.load(std::memory_order_relaxed);
    uint32_t lost = packetsLost_.load(std::memory_order_relaxed);
    if (rcv + lost == 0)
        return 0.0f;
    return (static_cast<float>(lost) / static_cast<float>(rcv + lost)) * 100.0f;
}

} // namespace pluginbridge

