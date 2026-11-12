#pragma once

#include <cmath>
#include <algorithm>

namespace retro
{

/** One-pole low-pass filter. */
class OnePoleLP
{
public:
    void reset() noexcept { z = 0.0f; }

    void setCutoff (float hz, double sampleRate) noexcept
    {
        const float f = std::min (std::max (hz, 5.0f), (float) sampleRate * 0.49f);
        coeff = std::exp (-2.0f * 3.14159265358979f * f / (float) sampleRate);
    }

    inline float process (float x) noexcept
    {
        z = (1.0f - coeff) * x + coeff * z;
        return z;
    }

private:
    float coeff = 0.99f;
    float z = 0.0f;
};

/** One-pole high-pass (DC-blocker style). */
class OnePoleHP
{
public:
    void reset() noexcept { y = 0.0f; x1 = 0.0f; }

    void setCutoff (float hz, double sampleRate) noexcept
    {
        const float f = std::min (std::max (hz, 5.0f), (float) sampleRate * 0.49f);
        coeff = std::exp (-2.0f * 3.14159265358979f * f / (float) sampleRate);
    }

    inline float process (float x) noexcept
    {
        y = coeff * (y + x - x1);
        x1 = x;
        return y;
    }

private:
    float coeff = 0.99f;
    float y = 0.0f;
    float x1 = 0.0f;
};

} // namespace retro
