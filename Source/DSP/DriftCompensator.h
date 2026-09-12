#pragma once

#include "../Network/NetworkProtocol.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace pluginbridge
{

class DriftCompensator
{
public:
    DriftCompensator(int maxChannels = MAX_CHANNELS);
    ~DriftCompensator() = default;

    void reset();
    void setTargetBufferFrames(int targetFrames);

    /**
     * Process audio through fractional resampler adjusting for clock drift.
     * @param inputChannels Source audio frames from jitter buffer
     * @param numInputFrames Available frames in input
     * @param outputChannels Target audio destination for DAW
     * @param numOutputFrames Required frames for DAW block
     * @param currentBufferFill Current fill level of jitter buffer in frames
     * @return Number of input frames consumed
     */
    int process(const float* const* inputChannels, int numInputFrames,
                float* const* outputChannels, int numOutputFrames,
                int numChannels, int currentBufferFill);

    float getCurrentSpeedRatio() const { return currentSpeedRatio_; }
    float getTargetSpeedRatio() const { return targetSpeedRatio_; }

private:
    float cubicInterpolate(float y0, float y1, float y2, float y3, float mu) const;

    const int maxChannels_;
    int targetBufferFrames_ { 384 }; // e.g. 3 packets of 128 frames

    // PI controller state
    float errorIntegral_ { 0.0f };
    float targetSpeedRatio_ { 1.0f };
    float currentSpeedRatio_ { 1.0f };

    // Sub-sample phase per channel
    double fractionalPhase_ { 0.0 };

    // History for cubic interpolation (3 samples of history per channel)
    std::vector<std::vector<float>> history_;
};

} // namespace pluginbridge
