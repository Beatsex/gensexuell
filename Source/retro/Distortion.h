#pragma once

#include <cmath>

namespace retro
{

/** Multi-style analog-ish distortion/overdrive (A/E/N/T/P voicings). */
class Distortion
{
public:
    void setStyle (int s) noexcept { style = s; }
    void setDrive (float d01) noexcept { drive = 1.0f + d01 * 24.0f; } // 1..25x
    void setPunish (bool p) noexcept { punish = p; }

    inline float process (float x) noexcept
    {
        const float d = punish ? drive * 4.0f : drive;
        const float pre = x * d;

        float y;
        switch (style)
        {
            case 0: // A - soft, a touch of even harmonic
                y = std::tanh (pre) + 0.05f * pre * pre;
                break;
            case 1: // E - valve-ish asymmetric
                y = std::tanh (pre + 0.15f * pre * pre);
                break;
            case 2: // N - transformer, tight odd/even
                y = std::tanh (pre) - 0.08f * std::tanh (pre * pre * pre);
                break;
            case 3: // T - Tape: rounded
                y = std::tanh (pre * 0.8f) * 1.1f;
                break;
            default: // P - Pentode: harder, more odd harmonics
            {
                const float s = pre >= 0.0f ? 1.0f : -1.0f;
                y = s * (1.0f - std::exp (-std::fabs (pre)));
                y += 0.1f * pre; // slight odd residual
                break;
            }
        }

        if (punish)
            y = std::tanh (y * 1.5f);

        return y / std::sqrt (d);
    }

private:
    int style = 0;
    float drive = 1.0f;
    bool punish = false;
};

} // namespace retro
