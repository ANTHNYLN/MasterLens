#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <cmath>
#include <vector>

namespace ml
{
inline constexpr float kSilentDb = -100.0f;

inline float energyToLufs (double energy) noexcept
{
    if (energy <= 1.0e-12)
        return kSilentDb;

    return (float) (-0.691 + 10.0 * std::log10 (energy));
}

inline double lufsToEnergy (double lufs) noexcept
{
    return std::pow (10.0, (lufs + 0.691) / 10.0);
}

//==============================================================================
/** Live measurement values shared between the audio thread and the UI.

    Every field is a lock-free atomic so the editor can poll at frame rate without
    ever blocking the audio callback.
*/
struct Stats
{
    std::atomic<float> integrated   { kSilentDb };
    std::atomic<float> momentary    { kSilentDb };
    std::atomic<float> shortTerm    { kSilentDb };
    std::atomic<float> maxMomentary { kSilentDb };
    std::atomic<float> maxShortTerm { kSilentDb };
    std::atomic<float> lra          { 0.0f };
    std::atomic<float> truePeakDb   { kSilentDb };
    std::atomic<float> samplePeakDb { kSilentDb };
    std::atomic<float> correlation  { 0.0f };
    std::atomic<float> widthPct     { 0.0f };
    std::atomic<float> lowSidePct   { 0.0f };
    std::atomic<float> dcOffsetDb   { kSilentDb };
    std::atomic<int>   clipEvents   { 0 };
    std::atomic<int>   clickEvents  { 0 };
    std::atomic<int>   oversCount   { 0 };
    std::atomic<double> elapsed     { 0.0 };
    std::atomic<bool>  stereo       { false };
    std::atomic<bool>  hasSignal    { false };
};

/** Plain copy of Stats, safe to hand to UI code. */
struct StatsSnapshot
{
    float integrated   = kSilentDb;
    float momentary    = kSilentDb;
    float shortTerm    = kSilentDb;
    float maxMomentary = kSilentDb;
    float maxShortTerm = kSilentDb;
    float lra          = 0.0f;
    float truePeakDb   = kSilentDb;
    float samplePeakDb = kSilentDb;
    float correlation  = 0.0f;
    float widthPct     = 0.0f;
    float lowSidePct   = 0.0f;
    float dcOffsetDb   = kSilentDb;
    int   clipEvents   = 0;
    int   clickEvents  = 0;
    int   oversCount   = 0;
    double elapsed     = 0.0;
    bool  stereo       = false;
    bool  hasSignal    = false;

    float getPlr() const noexcept { return integrated > -70.0f ? juce::jmax (0.0f, truePeakDb - integrated) : 0.0f; }
};

inline StatsSnapshot snapshot (const Stats& s) noexcept
{
    StatsSnapshot r;
    r.integrated   = s.integrated.load();
    r.momentary    = s.momentary.load();
    r.shortTerm    = s.shortTerm.load();
    r.maxMomentary = s.maxMomentary.load();
    r.maxShortTerm = s.maxShortTerm.load();
    r.lra          = s.lra.load();
    r.truePeakDb   = s.truePeakDb.load();
    r.samplePeakDb = s.samplePeakDb.load();
    r.correlation  = s.correlation.load();
    r.widthPct     = s.widthPct.load();
    r.lowSidePct   = s.lowSidePct.load();
    r.dcOffsetDb   = s.dcOffsetDb.load();
    r.clipEvents   = s.clipEvents.load();
    r.clickEvents  = s.clickEvents.load();
    r.oversCount   = s.oversCount.load();
    r.elapsed      = s.elapsed.load();
    r.stereo       = s.stereo.load();
    r.hasSignal    = s.hasSignal.load();
    return r;
}

inline void resetStats (Stats& s) noexcept
{
    s.integrated = kSilentDb;  s.momentary = kSilentDb;  s.shortTerm = kSilentDb;
    s.maxMomentary = kSilentDb; s.maxShortTerm = kSilentDb; s.lra = 0.0f;
    s.truePeakDb = kSilentDb;  s.samplePeakDb = kSilentDb;
    s.correlation = 0.0f;      s.widthPct = 0.0f;        s.lowSidePct = 0.0f;
    s.dcOffsetDb = kSilentDb;  s.clipEvents = 0;         s.clickEvents = 0;
    s.oversCount = 0;          s.elapsed = 0.0;          s.hasSignal = false;
}

//==============================================================================
/** Direct-form-I biquad, double precision state (used at 44.1/48/96 kHz alike). */
struct Biquad
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0;
    double z1 = 0.0, z2 = 0.0;

    inline float process (float x) noexcept
    {
        const double in  = (double) x;
        const double out = b0 * in + z1;
        z1 = b1 * in - a1 * out + z2;
        z2 = b2 * in - a2 * out;
        return (float) out;
    }

    void reset() noexcept { z1 = z2 = 0.0; }

    void setCoefficients (double nb0, double nb1, double nb2, double na1, double na2) noexcept
    {
        b0 = nb0; b1 = nb1; b2 = nb2; a1 = na1; a2 = na2;
    }

    /** RBJ low pass, used for the low-frequency stereo image check. */
    void designLowPass (double sampleRate, double freq, double q) noexcept;
};

//==============================================================================
/** 4x oversampling true-peak detector (windowed-sinc polyphase interpolator).

    Peak magnitude is independent of the group delay, so the plugin stays
    latency-free while still catching inter-sample overs.
*/
class TruePeakDetector
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;

    /** Returns the maximum absolute interpolated value for this input sample. */
    inline float process (float x) noexcept
    {
        history[(size_t) head] = x;
        head = (head + 1) & (historySize - 1);

        float maxAbs = std::abs (x);
        int idx = head - 1;

        for (int p = 0; p < phases; ++p)
        {
            float acc = 0.0f;
            int i = idx;
            const float* c = coeffs[p];

            for (int j = 0; j < tapsPerPhase; ++j)
            {
                acc += c[j] * history[(size_t) (i & (historySize - 1))];
                --i;
            }

            maxAbs = juce::jmax (maxAbs, std::abs (acc));
        }

        return maxAbs;
    }

private:
    static constexpr int phases = 4;
    static constexpr int tapsPerPhase = 24;
    static constexpr int historySize = 32;   // power of two, >= tapsPerPhase

    float coeffs[phases][tapsPerPhase] {};
    float history[historySize] {};
    int head = 0;
};

//==============================================================================
/** BS.1770-4 loudness (I / M / S / LRA) plus mix diagnostics.

    All heavy lifting happens on the audio thread but only involves a handful of
    multiplies per sample. Results are published through Stats, which the editor
    polls at 60 fps.
*/
class AnalysisEngine
{
public:
    static constexpr int   numBands = 10;
    static constexpr int   curveSize = 4096;      // 100 ms points ≈ 6.8 minutes
    static constexpr float histMinDb = -70.0f;
    static constexpr float histStepDb = 0.1f;
    static constexpr int   histBins = 780;

    void prepare (double sampleRate, int numChannels);
    void reset();

    /** Analyses a block in place. Audio is never modified. */
    void process (const juce::AudioBuffer<float>& buffer, Stats& stats, int numSamples);

    /** Copies the most recent short-term (3 s) loudness points, oldest first. */
    void copyRecentShortTerm (std::vector<float>& dest, int maxPoints) const;

private:
    void finishPartialBlock (Stats& stats);
    float computeIntegrated() const;
    float computeLra() const;

    double fs = 48000.0;
    int channels = 2;

    Biquad kShelf[2], kHighPass[2];
    Biquad lowPassA[2], lowPassB[2];
    TruePeakDetector truePeak[2];

    int partialLength = 4800;
    int partialSamples = 0;

    double sumWeightedK = 0.0;
    double sumLR = 0.0, sumL2 = 0.0, sumR2 = 0.0;
    double sumL = 0.0, sumR = 0.0;
    double sumMid = 0.0, sumSide = 0.0;
    double sumLowMid = 0.0, sumLowSide = 0.0;

    double partialMaxTruePeak = 0.0;
    double partialMaxSamplePeak = 0.0;
    float  prevL = 0.0f, prevR = 0.0f;
    float  clickAverage = 0.0f;

    // true DC offset: mean of the 100 ms block means over the last 4 seconds
    double dcRingL[40] {};
    double dcRingR[40] {};
    int    dcRingPos = 0;
    int    dcRingCount = 0;

    bool inClip = false;

    // trailing partial-block energies (100 ms each) for M / S windows
    double partialRing[64] {};
    int    ringWrite = 0;
    int    ringCount = 0;
    int    partialCounter = 0;

    std::atomic<unsigned int> blockHist[histBins] {};
    std::atomic<unsigned int> shortHist[histBins] {};
    std::atomic<unsigned int> shortHistCount { 0 };

    float shortCurve[curveSize] {};
    std::atomic<int> curveWrite { 0 };

    juce::uint32 elapsedSamples = 0;
};
} // namespace ml
