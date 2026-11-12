#pragma once

#include "DelayLine.h"

namespace retro
{

/** Schroeder allpass, used for diffusion stages. */
class Allpass
{
public:
    void prepare (double sr, double maxDelaySeconds)
    {
        line.prepare (sr, maxDelaySeconds);
        reset();
    }

    void reset() { line.reset(); }

    void setDelaySamples (float d) noexcept { delay = d; }
    void setCoef (float g) noexcept { coef = g; }

    inline float process (float x) noexcept
    {
        const float d = line.readLinear (delay);
        const float w = x + coef * d;
        line.write (w);
        return d - coef * w;
    }

private:
    DelayLine line;
    float delay = 100.0f;
    float coef = 0.5f;
};

} // namespace retro
