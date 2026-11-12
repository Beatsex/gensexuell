#pragma once

#include <cmath>

namespace retro
{

/** Direct-form-I biquad with RBJ coefficient calculators. */
class Biquad
{
public:
    void reset() noexcept { x1 = x2 = y1 = y2 = 0.0f; }

    void setPeaking (double sr, float freq, float q, float gainDb) noexcept
    {
        const float A  = std::pow (10.0f, gainDb / 40.0f);
        const float w0 = twoPi * freq / (float) sr;
        const float cw = std::cos (w0);
        const float alpha = std::sin (w0) / (2.0f * q);
        const float a0 = 1.0f + alpha / A;
        set (1.0f + alpha * A, -2.0f * cw, 1.0f - alpha * A,
             a0, -2.0f * cw, 1.0f - alpha / A);
    }

    void setLowShelf (double sr, float freq, float q, float gainDb) noexcept
    {
        (void) q;
        const float A  = std::pow (10.0f, gainDb / 40.0f);
        const float w0 = twoPi * freq / (float) sr;
        const float cw = std::cos (w0);
        const float alpha = std::sin (w0) * 0.5f * std::sqrt (2.0f);
        const float sa = 2.0f * std::sqrt (A) * alpha;
        set (A * ((A + 1.0f) - (A - 1.0f) * cw + sa),
             2.0f * A * ((A - 1.0f) - (A + 1.0f) * cw),
             A * ((A + 1.0f) - (A - 1.0f) * cw - sa),
             (A + 1.0f) + (A - 1.0f) * cw + sa,
             -2.0f * ((A - 1.0f) + (A + 1.0f) * cw),
             (A + 1.0f) + (A - 1.0f) * cw - sa);
    }

    void setHighShelf (double sr, float freq, float q, float gainDb) noexcept
    {
        (void) q;
        const float A  = std::pow (10.0f, gainDb / 40.0f);
        const float w0 = twoPi * freq / (float) sr;
        const float cw = std::cos (w0);
        const float alpha = std::sin (w0) * 0.5f * std::sqrt (2.0f);
        const float sa = 2.0f * std::sqrt (A) * alpha;
        set (A * ((A + 1.0f) + (A - 1.0f) * cw + sa),
             -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cw),
             A * ((A + 1.0f) + (A - 1.0f) * cw - sa),
             (A + 1.0f) - (A - 1.0f) * cw + sa,
             2.0f * ((A - 1.0f) - (A + 1.0f) * cw),
             (A + 1.0f) - (A - 1.0f) * cw - sa);
    }

    void setLowpass (double sr, float freq, float q) noexcept
    {
        const float w0 = twoPi * freq / (float) sr;
        const float cw = std::cos (w0);
        const float alpha = std::sin (w0) / (2.0f * q);
        const float b1r = 1.0f - cw;
        set ((1.0f - cw) * 0.5f, b1r, (1.0f - cw) * 0.5f,
             1.0f + alpha, -2.0f * cw, 1.0f - alpha);
    }

    void setHighpass (double sr, float freq, float q) noexcept
    {
        const float w0 = twoPi * freq / (float) sr;
        const float cw = std::cos (w0);
        const float alpha = std::sin (w0) / (2.0f * q);
        set ((1.0f + cw) * 0.5f, -(1.0f + cw), (1.0f + cw) * 0.5f,
             1.0f + alpha, -2.0f * cw, 1.0f - alpha);
    }

    inline float process (float x) noexcept
    {
        const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x;
        y2 = y1; y1 = y;
        return y;
    }

private:
    static constexpr float twoPi = 6.28318530718f;

    void set (float B0, float B1, float B2, float A0, float A1, float A2) noexcept
    {
        const float inv = 1.0f / A0;
        b0 = B0 * inv; b1 = B1 * inv; b2 = B2 * inv;
        a1 = A1 * inv; a2 = A2 * inv;
    }

    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
};

} // namespace retro
