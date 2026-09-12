#include "DriftCompensator.h"

namespace pluginbridge
{

DriftCompensator::DriftCompensator(int maxChannels)
    : maxChannels_(maxChannels)
{
    history_.resize(maxChannels_);
    for (int ch = 0; ch < maxChannels_; ++ch)
    {
        history_[ch].resize(4, 0.0f);
    }
}

void DriftCompensator::reset()
{
    errorIntegral_ = 0.0f;
    targetSpeedRatio_ = 1.0f;
    currentSpeedRatio_ = 1.0f;
    fractionalPhase_ = 0.0;
    for (int ch = 0; ch < maxChannels_; ++ch)
    {
        std::fill(history_[ch].begin(), history_[ch].end(), 0.0f);
    }
}

void DriftCompensator::setTargetBufferFrames(int targetFrames)
{
    targetBufferFrames_ = std::max(64, targetFrames);
}

float DriftCompensator::cubicInterpolate(float y0, float y1, float y2, float y3, float mu) const
{
    // Catmull-Rom cubic spline interpolation
    float a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
    float a1 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float a2 = -0.5f * y0 + 0.5f * y2;
    float a3 = y1;

    float mu2 = mu * mu;
    return a0 * mu * mu2 + a1 * mu2 + a2 * mu + a3;
}

int DriftCompensator::process(const float* const* inputChannels, int numInputFrames,
                              float* const* outputChannels, int numOutputFrames,
                              int numChannels, int currentBufferFill)
{
    if (numOutputFrames <= 0 || numChannels <= 0)
        return 0;

    // PI controller tracking buffer occupancy
    float error = static_cast<float>(currentBufferFill - targetBufferFrames_);
    errorIntegral_ += error * 0.000005f;
    errorIntegral_ = std::clamp(errorIntegral_, -0.0005f, 0.0005f);

    float delta = error * 0.00002f + errorIntegral_;
    targetSpeedRatio_ = std::clamp(1.0f + delta, 0.9990f, 1.0010f);

    // Exponential smoothing to avoid sudden pitch shifts
    currentSpeedRatio_ = 0.98f * currentSpeedRatio_ + 0.02f * targetSpeedRatio_;

    double step = currentSpeedRatio_;
    double phase = fractionalPhase_;
    int inIndex = 0;
    int channelsToProcess = std::min(numChannels, maxChannels_);

    for (int outIdx = 0; outIdx < numOutputFrames; ++outIdx)
    {
        inIndex = static_cast<int>(phase);
        float mu = static_cast<float>(phase - inIndex);

        for (int ch = 0; ch < channelsToProcess; ++ch)
        {
            if (outputChannels[ch] == nullptr)
                continue;

            // Fetch 4 points for cubic interpolation
            auto sampleAt = [&](int idx) -> float {
                if (idx < 0)
                {
                    int hIdx = 4 + idx;
                    return (hIdx >= 0 && hIdx < 4) ? history_[ch][hIdx] : 0.0f;
                }
                if (idx < numInputFrames && inputChannels[ch] != nullptr)
                {
                    return inputChannels[ch][idx];
                }
                return 0.0f;
            };

            float y0 = sampleAt(inIndex - 1);
            float y1 = sampleAt(inIndex);
            float y2 = sampleAt(inIndex + 1);
            float y3 = sampleAt(inIndex + 2);

            outputChannels[ch][outIdx] = cubicInterpolate(y0, y1, y2, y3, mu);
        }

        phase += step;
    }

    // Save tail samples to history buffer for next block continuity
    int consumedFrames = static_cast<int>(phase);
    for (int ch = 0; ch < channelsToProcess; ++ch)
    {
        if (inputChannels[ch] == nullptr)
            continue;

        for (int i = 0; i < 4; ++i)
        {
            int idx = consumedFrames - 4 + i;
            if (idx >= 0 && idx < numInputFrames)
                history_[ch][i] = inputChannels[ch][idx];
            else if (idx >= 0)
                history_[ch][i] = 0.0f;
        }
    }

    fractionalPhase_ = phase - consumedFrames;
    return std::min(consumedFrames, numInputFrames);
}

} // namespace pluginbridge

