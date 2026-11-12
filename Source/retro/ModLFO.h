#pragma once

#include <cmath>

namespace retro
{

/** Simple sine LFO used for delay/reverb modulation. */
class ModLFO
{
public:
    void prepare (double sampleRate) noexcept
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        phase = 0.0f;
    }

    void setRate (float hz) noexcept
    {
        inc = (float) (hz / sr);
    }

    inline float process() noexcept
    {
        const float v = std::sin (phase * 6.28318530718f);
        phase += inc;
        if (phase >= 1.0f) phase -= 1.0f;
        return v;
    }

private:
    double sr = 44100.0;
    float phase = 0.0f;
    float inc = 0.001f;
};

} // namespace retro
