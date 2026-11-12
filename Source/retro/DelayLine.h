#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

namespace retro
{

/** A circular delay line with linear and cubic (Catmull-Rom) fractional reads. */
class DelayLine
{
public:
    void prepare (double sampleRate, double maxDelaySeconds)
    {
        sr = sampleRate > 0.0 ? sampleRate : 44100.0;
        const int newSize = (int) std::ceil (maxDelaySeconds * sr) + 8;
        buffer.assign ((size_t) newSize, 0.0f);
        writePos = 0;
    }

    void reset()
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writePos = 0;
    }

    inline void write (float x) noexcept
    {
        buffer[(size_t) writePos] = x;
        if (++writePos >= (int) buffer.size())
            writePos = 0;
    }

    inline float readLinear (float delaySamples) const noexcept
    {
        const int size = (int) buffer.size();
        float d = clampDelay (delaySamples, size);
        float pos = (float) writePos - d;
        while (pos < 0.0f)          pos += (float) size;
        while (pos >= (float) size) pos -= (float) size;

        const int i0 = (int) pos;
        const int i1 = (i0 + 1) % size;
        const float f = pos - (float) i0;
        return buffer[(size_t) i0] + f * (buffer[(size_t) i1] - buffer[(size_t) i0]);
    }

    inline float readCubic (float delaySamples) const noexcept
    {
        const int size = (int) buffer.size();
        float d = clampDelay (delaySamples, size);
        float pos = (float) writePos - d;
        while (pos < 0.0f)          pos += (float) size;
        while (pos >= (float) size) pos -= (float) size;

        const int i1 = (int) pos;
        const float f = pos - (float) i1;
        const int im1 = (i1 - 1 + size) % size;
        const int i0  = i1;
        const int i2  = (i1 + 1) % size;
        const int i3  = (i1 + 2) % size;

        const float y0 = buffer[(size_t) im1];
        const float y1 = buffer[(size_t) i0];
        const float y2 = buffer[(size_t) i2];
        const float y3 = buffer[(size_t) i3];

        const float a = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
        const float b =         y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c = -0.5f * y0            + 0.5f * y2;

        return ((a * f + b) * f + c) * f + y1;
    }

    int getSize() const noexcept { return (int) buffer.size(); }

private:
    static inline float clampDelay (float d, int size) noexcept
    {
        const float lo = 2.0f;
        const float hi = (float) (size - 4);
        if (d < lo) return lo;
        if (d > hi) return hi;
        return d;
    }

    std::vector<float> buffer;
    int writePos = 0;
    double sr = 44100.0;
};

} // namespace retro
