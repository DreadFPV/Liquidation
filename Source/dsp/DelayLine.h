#pragma once

#include <vector>
#include <algorithm>

namespace liq
{
// Fixed integer delay for keeping parallel signal paths time-aligned.
class FixedDelay
{
public:
    void prepare (int delaySamples)
    {
        delay = std::max (0, delaySamples);
        buf.assign ((size_t) std::max (1, delay), 0.0f);
        pos = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); pos = 0; }

    float process (float x) noexcept
    {
        if (delay == 0) return x;
        const float y = buf[(size_t) pos];
        buf[(size_t) pos] = x;
        if (++pos >= delay) pos = 0;
        return y;
    }

    void process (float* data, int n) noexcept { for (int i = 0; i < n; ++i) data[i] = process (data[i]); }
    int getDelay() const noexcept { return delay; }

private:
    std::vector<float> buf;
    int delay = 0, pos = 0;
};
}
