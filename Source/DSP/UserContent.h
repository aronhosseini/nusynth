#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <numeric>
#include <atomic>
#include <vector>
#include "Wavetables.h"
#include "Parameters/ParameterIDs.h"
#include <algorithm>
#include <cmath>

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


    // ------------------------------------------------------------------ wavetable import modes (the drop menu)
    enum class ImportMode { Default, DynamicZeroSnap, DynamicFollow, FrequencyEstimation, PitchAverage, Fft256, Fft512, Fft1024, Fft2048 };

    /** Imports an audio file as a user wavetable (up to 8 frames).
        FFT n .............. every frame is a chunk of n samples (n = 256 / 512 / 1024 / 2048) taken as one cycle
        Frequency estimation  the pitch of the file is detected once; every frame is one period of it
        Pitch average ...... the pitch is detected at 8 places and averaged; constant frame size
        Dynamic - follow ... every frame is one period at the pitch found AT THAT PLACE (for sweeps / glides)
        Dynamic - zero snap  like follow, but every frame starts and ends on an upward zero crossing (no clicks)  */
    bool importWave (int slot, const juce::File& file, ImportMode mode)
    {
        if (mode == ImportMode::Default) return loadWaveFromFile (slot, file);
        std::vector<float> mono;
        float sr = 44100.0f;
        if (! readMono (file, 44100 * 40, mono, sr)) return false;
        const int total = (int) mono.size();
        const int kMaxFrames = WavetableBank::kFrames;

        if (total < 4096 && (mode == ImportMode::FrequencyEstimation || mode == ImportMode::PitchAverage
                             || mode == ImportMode::DynamicFollow || mode == ImportMode::DynamicZeroSnap))
        {   // too short to analyse: a single-cycle file
            WavetableBank::setUserFrames (slot, { cleanFrame (mono, 0, total) });
            return true;
        }

        int fixedLen = 2048;
        switch (mode)
        {
            case ImportMode::Fft256:  fixedLen = 256;  break;
            case ImportMode::Fft512:  fixedLen = 512;  break;
            case ImportMode::Fft1024: fixedLen = 1024; break;
            default: break;
        }

        const bool pitchBased = mode == ImportMode::FrequencyEstimation || mode == ImportMode::PitchAverage
                             || mode == ImportMode::DynamicFollow || mode == ImportMode::DynamicZeroSnap;
        const int maxPeriod = 2048;
        const int count = pitchBased ? kMaxFrames : std::max (1, std::min (kMaxFrames, total / fixedLen));

        // frame start positions, spread evenly over the file
        auto startOf = [&] (int i, int len)
        {
            const int span = std::max (0, total - len);
            return count > 1 ? (int) ((double) span * (double) i / (double) (count - 1)) : 0;
        };

        std::vector<float> period ((size_t) count, 0.0f);
        float avg = 0.0f;
        if (pitchBased)
        {
            std::vector<float> found;
            for (int i = 0; i < count; ++i)
            {
                period[(size_t) i] = detectPeriod (mono, startOf (i, 4096 + maxPeriod), sr);
                if (period[(size_t) i] > 0.0f) found.push_back (period[(size_t) i]);
            }
            if (found.empty()) { WavetableBank::setUserFrames (slot, { cleanFrame (mono, 0, std::min (total, 2048)) }); return true; }
            std::sort (found.begin(), found.end());
            avg = mode == ImportMode::FrequencyEstimation ? found[found.size() / 2]
                                                          : std::accumulate (found.begin(), found.end(), 0.0f) / (float) found.size();
        }

        std::vector<std::vector<float>> frames;
        for (int i = 0; i < count; ++i)
        {
            int len = fixedLen;
            if (pitchBased)
            {
                const float pi = period[(size_t) i] > 0.0f ? period[(size_t) i] : avg;
                len = (int) std::lround (mode == ImportMode::DynamicFollow || mode == ImportMode::DynamicZeroSnap ? pi : avg);
            }
            len = std::max (8, std::min (len, total));
            int start = startOf (i, len);

            if (mode == ImportMode::DynamicZeroSnap)
            {
                auto upZero = [&mono, total] (int from, int to) { for (int k = std::max (1, from); k < std::min (total, to); ++k) if (mono[(size_t) k - 1] < 0.0f && mono[(size_t) k] >= 0.0f) return k; return -1; };
                const int s = upZero (start, start + len);
                if (s >= 0) start = s;
                const int e = upZero (start + (int) (len * 0.7f), start + (int) (len * 1.3f));
                if (e > start + 8) len = e - start;
            }
            frames.push_back (cleanFrame (mono, start, len));
        }
        WavetableBank::setUserFrames (slot, frames);
        return true;
    }

    /** Writes every user wavetable an oscillator currently uses into  <wtFolder>  and the noise sample into  <noiseFolder>  (called when a preset is saved). */
    void exportUsedAssets (juce::AudioProcessorValueTreeState& apvts, const juce::String& presetName, const juce::File& wtFolder, const juce::File& noiseFolder)
    {
        const auto safeName = juce::File::createLegalFileName (presetName);
        std::vector<int> done;
        for (int o = 0; o < 3; ++o)
        {
            auto* t = apvts.getRawParameterValue (ids::osc (o, "wavetable"));
            if (t == nullptr) continue;
            const int idx = (int) t->load();
            if (! WavetableBank::isUserIndex (idx)) continue;
            const int slot = idx - WavetableBank::kFirstUser;
            if (std::find (done.begin(), done.end(), slot) != done.end()) continue;
            done.push_back (slot);
            wtFolder.createDirectory();
            exportWave (slot, wtFolder.getChildFile (safeName + " - Osc " + juce::String::charToString ((juce::juce_wchar) ('A' + o)) + ".wav"));
        }

        auto* nt = apvts.getRawParameterValue (ids::noiseType);
        int n = 0;
        const float* d = noise.data (n);
        if (nt != nullptr && (int) nt->load() == 5 && d != nullptr && n > 1)
        {
            noiseFolder.createDirectory();
            const juce::File original (noisePath);
            if (noisePath.isNotEmpty() && original.existsAsFile())
            {
                const auto dest = noiseFolder.getChildFile (original.getFileName());
                if (! dest.existsAsFile() && original != dest) original.copyFileTo (dest);
            }
            else
            {
                auto dest = noiseFolder.getChildFile (safeName + " - noise.wav");
                dest.deleteFile();
                juce::WavAudioFormat wav;
                if (std::unique_ptr<juce::FileOutputStream> stream { dest.createOutputStream() })
                {
                    if (std::unique_ptr<juce::AudioFormatWriter> writer { wav.createWriterFor (stream.get(), (double) noise.sampleRate.load(), 1, 24, {}, 0) })
                    {
                        stream.release();
                        juce::AudioBuffer<float> buf (1, n);
                        buf.copyFrom (0, 0, d, n);
                        writer->writeFromAudioSampleBuffer (buf, 0, n);
                    }
                }
            }
        }
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
    /** One cycle = `len` samples starting at `start`, resampled to kUserPoints, DC removed and normalised. */
    static std::vector<float> cleanFrame (const std::vector<float>& mono, int start, int len)
    {
        std::vector<float> part ((size_t) std::max (8, len), 0.0f);
        for (size_t i = 0; i < part.size(); ++i)
        {
            const size_t k = (size_t) start + i;
            if (k < mono.size()) part[i] = mono[k];
        }
        auto r = resample (part, WavetableBank::kUserPoints);
        float mean = 0.0f;
        for (auto v : r) mean += v;
        mean /= (float) r.size();
        float peak = 1.0e-6f;
        for (auto& v : r) { v -= mean; peak = std::max (peak, std::abs (v)); }
        for (auto& v : r) v = v / peak * 0.95f;
        return r;
    }

    /** Fundamental period in samples (0 = none found): normalised auto-correlation, first strong peak (avoids octave errors). */
    static float detectPeriod (const std::vector<float>& x, int start, float sr)
    {
        const int win = 4096;
        const int minLag = std::max (16, (int) (sr / 2000.0f)), maxLag = 2048;
        if ((int) x.size() < win + maxLag + 2) return 0.0f;
        start = std::max (0, std::min (start, (int) x.size() - win - maxLag - 1));
        double e0 = 0.0;
        for (int i = 0; i < win; ++i) e0 += (double) x[(size_t) (start + i)] * x[(size_t) (start + i)];
        if (e0 < 1.0e-6) return 0.0f;

        std::vector<float> corr ((size_t) maxLag + 2, 0.0f);
        float best = 0.0f;
        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            double c = 0.0, e1 = 0.0;
            for (int i = 0; i < win; ++i)
            {
                const double b = x[(size_t) (start + i + lag)];
                c += (double) x[(size_t) (start + i)] * b;
                e1 += b * b;
            }
            corr[(size_t) lag] = (float) (c / std::sqrt (e0 * e1 + 1.0e-12));
            best = std::max (best, corr[(size_t) lag]);
        }
        if (best < 0.5f) return 0.0f;
        for (int lag = minLag + 1; lag < maxLag; ++lag)
            if (corr[(size_t) lag] >= 0.9f * best && corr[(size_t) lag] >= corr[(size_t) lag - 1] && corr[(size_t) lag] >= corr[(size_t) lag + 1])
            {   // parabolic interpolation of the peak
                const float a = corr[(size_t) lag - 1], b = corr[(size_t) lag], c = corr[(size_t) lag + 1];
                const float den = a - 2.0f * b + c;
                return (float) lag + (std::abs (den) > 1.0e-9f ? 0.5f * (a - c) / den : 0.0f);
            }
        return 0.0f;
    }

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
