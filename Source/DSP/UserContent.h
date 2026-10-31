#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <vector>
#include "Wavetables.h"

namespace nusynth
{
/** Sample used by the "Sample" noise type (triple-buffered so the audio thread never sees a half-written buffer). */
struct NoiseSample
{
    std::array<std::vector<float>, 3> buf;
    std::array<int, 3> len { { 0, 0, 0 } };
    std::atomic<int> active { 0 };
    std::atomic<float> sampleRate { 44100.0f };

    void set (std::vector<float>&& data, float sr)
    {
        const int t = (active.load() + 1) % 3;
        buf[(size_t) t] = std::move (data);
        len[(size_t) t] = (int) buf[(size_t) t].size();
        sampleRate.store (sr);
        active.store (t, std::memory_order_release);
    }
    const float* data (int& n) const noexcept
    {
        const int a = active.load (std::memory_order_acquire);
        n = len[(size_t) a];
        return buf[(size_t) a].data();
    }
};

/** User waveforms (8 slots, drawn or imported from audio files) and the noise sample. Message thread only. */
class UserContent
{
public:
    explicit UserContent (NoiseSample& n) : noise (n) { formats.registerBasicFormats(); }

    void setDrawn (int slot, const std::vector<float>& pts) { WavetableBank::setUserWave (slot, resample (pts, WavetableBank::kUserPoints)); }

    /** A file shorter than 2048 samples becomes one single-cycle frame, a longer one is cut into up to 8 frames of 2048 samples. */
    bool loadWaveFromFile (int slot, const juce::File& file)
    {
        std::vector<float> mono;
        float sr = 44100.0f;
        if (! readMono (file, 8 * 2048, mono, sr)) return false;
        std::vector<std::vector<float>> frames;
        if (mono.size() < 2048) frames.push_back (resample (mono, WavetableBank::kUserPoints));
        else
        {
            const int count = std::min (WavetableBank::kFrames, (int) (mono.size() / 2048));
            for (int f = 0; f < count; ++f)
            {
                std::vector<float> part (mono.begin() + (long) f * 2048, mono.begin() + (long) (f + 1) * 2048);
                frames.push_back (resample (part, WavetableBank::kUserPoints));
            }
        }
        WavetableBank::setUserFrames (slot, frames);
        return true;
    }

    /** Writes the user wavetable (all frames, 2048 samples each) as a mono 32-bit wav file. */
    bool exportWave (int slot, const juce::File& file)
    {
        const auto frames = WavetableBank::getUserFrames (slot);
        if (frames.empty()) return false;
        const int n = (int) frames.size() * 2048;
        juce::AudioBuffer<float> buf (1, n);
        for (size_t f = 0; f < frames.size(); ++f)
        {
            const auto r = resample (frames[f], 2048);
            float peak = 1.0e-6f;
            for (auto v : r) peak = std::max (peak, std::abs (v));
            for (int i = 0; i < 2048; ++i) buf.setSample (0, (int) f * 2048 + i, r[(size_t) i] / peak * 0.95f);
        }
        file.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::FileOutputStream> stream (file.createOutputStream());
        if (stream == nullptr) return false;
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), 44100.0, 1, 32, {}, 0));
        if (writer == nullptr) return false;
        stream.release();
        return writer->writeFromAudioSampleBuffer (buf, 0, n);
    }

    bool loadNoiseSample (const juce::File& file)
    {
        std::vector<float> mono;
        float sr = 44100.0f;
        if (! readMono (file, 44100 * 12, mono, sr)) return false;
        float peak = 1.0e-6f;
        for (auto v : mono) peak = std::max (peak, std::abs (v));
        for (auto& v : mono) v /= peak;
        noise.set (std::move (mono), sr);
        noisePath = file.getFullPathName();
        return true;
    }

    void saveToState (juce::ValueTree& st) const
    {
        for (int s = 0; s < WavetableBank::kUserSlots; ++s)
        {
            const auto frames = WavetableBank::getUserFrames (s);
            if (frames.empty()) continue;
            juce::MemoryBlock mb;
            size_t pos = 0;
            mb.setSize (frames.size() * (size_t) WavetableBank::kUserPoints * 2);
            for (const auto& fr : frames)
            {
                const auto r = resample (fr, WavetableBank::kUserPoints);
                for (size_t i = 0; i < r.size(); ++i, pos += 2)
                {
                    const auto v = (juce::int16) juce::jlimit (-32767, 32767, (int) std::lround (r[i] * 32767.0f));
                    mb.copyFrom (&v, pos, 2);
                }
            }
            st.setProperty ("userWave" + juce::String (s + 1), mb.toBase64Encoding(), nullptr);
        }
        st.setProperty ("noiseSamplePath", noisePath, nullptr);
    }

    void loadFromState (const juce::ValueTree& st)
    {
        for (int s = 0; s < WavetableBank::kUserSlots; ++s)
        {
            const auto str = st.getProperty ("userWave" + juce::String (s + 1)).toString();
            if (str.isEmpty()) continue;
            juce::MemoryBlock mb;
            if (! mb.fromBase64Encoding (str) || mb.getSize() < 16) continue;
            const size_t per = (size_t) WavetableBank::kUserPoints * 2;
            const size_t count = std::max<size_t> (1, std::min<size_t> ((size_t) WavetableBank::kFrames, mb.getSize() / per));
            std::vector<std::vector<float>> frames;
            for (size_t f = 0; f < count; ++f)
            {
                const size_t pts = std::min (per, mb.getSize() - f * per) / 2;
                std::vector<float> fr (pts);
                for (size_t i = 0; i < pts; ++i)
                {
                    juce::int16 v = 0;
                    mb.copyTo (&v, f * per + i * 2, 2);
                    fr[i] = (float) v / 32767.0f;
                }
                frames.push_back (std::move (fr));
            }
            WavetableBank::setUserFrames (s, frames);
        }
        const auto path = st.getProperty ("noiseSamplePath").toString();
        if (path.isNotEmpty() && path != noisePath && juce::File (path).existsAsFile()) loadNoiseSample (juce::File (path));
    }

    juce::String noisePath;

private:
    static std::vector<float> resample (const std::vector<float>& in, int outN)
    {
        std::vector<float> out ((size_t) outN, 0.0f);
        if (in.size() < 2) return out;
        const float step = (float) in.size() / (float) outN;
        for (int i = 0; i < outN; ++i)
        {
            const float pos = (float) i * step;
            const size_t i0 = (size_t) pos % in.size(), i1 = (i0 + 1) % in.size();
            const float fr = pos - std::floor (pos);
            out[(size_t) i] = in[i0] + (in[i1] - in[i0]) * fr;
        }
        return out;
    }

    bool readMono (const juce::File& file, int maxSamples, std::vector<float>& out, float& sr)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
        if (reader == nullptr || reader->lengthInSamples < 8) return false;
        const int n = (int) std::min<juce::int64> (reader->lengthInSamples, maxSamples);
        juce::AudioBuffer<float> buf ((int) std::max<unsigned> (1u, reader->numChannels), n);
        reader->read (&buf, 0, n, 0, true, true);
        out.assign ((size_t) n, 0.0f);
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            const float* d = buf.getReadPointer (ch);
            for (int i = 0; i < n; ++i) out[(size_t) i] += d[i] / (float) buf.getNumChannels();
        }
        sr = (float) reader->sampleRate;
        return true;
    }

    NoiseSample& noise;
    juce::AudioFormatManager formats;
};
}
