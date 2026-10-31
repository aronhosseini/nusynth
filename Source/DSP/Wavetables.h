#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <mutex>
#include <vector>

namespace nusynth
{
/** 65 procedurally generated wavetables (8 morph frames each, band-limited mip levels) + 8 user slots (draw / sample import). */
class WavetableBank
{
public:
    static constexpr int kSize = 2048, kFrames = 8, kLevels = 10;
    static constexpr int kProcTables = 65, kUserSlots = 8, kFirstUser = 65, kTables = 73, kUserPoints = 512;

    static const WavetableBank& get();

    /** Pointer to kSize+1 floats (last = first, for interpolation). User tables are single-frame. */
    const float* frame (int table, int level, int frameIdx) const noexcept
    {
        if (table >= kFirstUser)
        {
            const int slot = table - kFirstUser;
            const int buf = userActive[(size_t) slot].load (std::memory_order_acquire);
            return userData[(size_t) slot][(size_t) buf].data() + ((size_t) level * kFrames + (size_t) frameIdx) * (size_t) (kSize + 1);
        }
        return data.data() + ((((size_t) table * kLevels + (size_t) level) * kFrames + (size_t) frameIdx) * (size_t) (kSize + 1));
    }

    /** Mip level whose highest harmonic stays below Nyquist for a given oscillator frequency. */
    static int levelFor (float freqHz, float sampleRate) noexcept
    {
        const float maxH = 0.5f * sampleRate / (freqHz > 1.0f ? freqHz : 1.0f);
        int L = 0;
        float lim = 1023.0f;
        while (L < kLevels - 1 && lim > maxH) { ++L; lim = (float) (1024 >> L); }
        return L;
    }

    /** Message thread only. A user wavetable is 1..8 frames; each frame is one single cycle (any length >= 8, values -1..1).
        The 8 playable frames are interpolated from the frames you give. */
    static void setUserFrames (int slot, const std::vector<std::vector<float>>& frames);
    static std::vector<std::vector<float>> getUserFrames (int slot);
    static void setUserWave (int slot, const std::vector<float>& points) { setUserFrames (slot, { points }); }
    static std::vector<float> getUserWave (int slot) { auto f = getUserFrames (slot); return f.empty() ? std::vector<float>() : f[0]; }

private:
    WavetableBank();
    void buildUser (int slot, const std::vector<std::vector<float>>& frames);

    std::vector<float> data;
    std::array<std::array<std::vector<float>, 3>, kUserSlots> userData;
    std::array<std::atomic<int>, kUserSlots> userActive;
    std::array<std::vector<std::vector<float>>, kUserSlots> userSrc;
    std::mutex userMutex;
};
}
