// Offline checks for Polychromatic: null at 0 %, latency, tonal pull.
#include "dsp/polychromatic/polychromatic.h"
#include <cstdio>
#include <cmath>
#include <random>
#include <vector>

using GlitchCore::Polychromatic;
static constexpr double kPi = 3.14159265358979323846;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf ("FAIL: " __VA_ARGS__); std::printf ("\n"); } else { std::printf ("ok:   " __VA_ARGS__); std::printf ("\n"); } } while (0)

static std::vector<float> noise (int n, unsigned seed)
{
    std::mt19937 r (seed); std::normal_distribution<float> d (0.0f, 0.2f);
    std::vector<float> v ((size_t) n); for (auto& x : v) x = d (r); return v;
}

static void run (Polychromatic& h, std::vector<float>& L, std::vector<float>& R,
                 std::vector<float>& oL, std::vector<float>& oR, const Polychromatic::Params& p, int block)
{
    const int n = (int) L.size();
    oL.assign ((size_t) n, 0); oR.assign ((size_t) n, 0);
    for (int i = 0; i < n; i += block)
    {
        const int m = std::min (block, n - i);
        const float* in[2] = { L.data() + i, R.data() + i };
        float* out[2] = { oL.data() + i, oR.data() + i };
        h.process (in, out, 2, m, p);
    }
}

// Energy fraction within +-25 cents of the given note set (pitch classes)
static double tonalFraction (const std::vector<float>& x, double sr, int start, std::uint16_t pcMask, int root)
{
    const int N = 16384; double inBand = 0, total = 0;
    for (int k = 1; k < N / 2; ++k)
    {
        double re = 0, im = 0;
        if (k % 4 != 0) continue;     // coarse but fine for a fraction
        for (int n = 0; n < N; ++n)
        {
            const double w = 0.5 - 0.5 * std::cos (2 * kPi * n / N);
            const double a = -2 * kPi * k * n / N;
            re += x[(size_t) (start + n)] * w * std::cos (a);
            im += x[(size_t) (start + n)] * w * std::sin (a);
        }
        const double e = re * re + im * im, hz = k * sr / N;
        if (hz < 40 || hz > 15000) continue;
        total += e;
        const double midi = 69 + 12 * std::log2 (hz / 440.0);
        const int nearest = (int) std::lround (midi);
        const int pc = ((nearest - root) % 12 + 12) % 12;
        if (std::abs (midi - nearest) < 0.25 && ((pcMask >> pc) & 1)) inBand += e;
    }
    return inBand / total;
}

int main()
{
    const double sr = 48000.0;
    for (auto size : { Polychromatic::FftSize::k4096, Polychromatic::FftSize::k2048 })
    {
        Polychromatic h; h.prepare (sr, 512, size);
        const int lat = h.getLatencySamples();
        const int n = 48000 * 2;
        auto L = noise (n, 1), R = noise (n, 2);
        std::vector<float> oL, oR;
        Polychromatic::Params p; p.amount = 0.0f;
        run (h, L, R, oL, oR, p, 333);
        double err = 0;
        for (int i = lat; i < n; ++i) err = std::max (err, (double) std::abs (oL[(size_t) i] - L[(size_t) (i - lat)]));
        CHECK (err == 0.0, "N=%d: 0%% amount is a bit-exact null at latency %d (max err %g)", lat, lat, err);

        // Engage then return to 0: wet path itself must also reconstruct cleanly.
        h.reset();
        p.amount = 1e-6f;   // forces STFT path, effectively no change
        run (h, L, R, oL, oR, p, 512);
        double e2 = 0, s2 = 0;
        for (int i = lat * 2; i < n; ++i) { const double d = oL[(size_t) i] - L[(size_t) (i - lat)]; e2 += d * d; s2 += (double) L[(size_t) (i - lat)] * L[(size_t) (i - lat)]; }
        const double db = 10 * std::log10 (e2 / s2 + 1e-30);
        CHECK (db < -90.0, "N=%d: STFT path reconstruction error %.1f dB", lat, db);
    }

    // Tonal pull: noise in, C major at 100 % → energy concentrates on the scale.
    {
        Polychromatic h; h.prepare (sr, 512);
        const int n = 48000 * 3;
        auto L = noise (n, 3), R = noise (n, 4);
        std::vector<float> oL, oR;
        Polychromatic::Params p; p.scale = 0; p.root = 0;
        const std::uint16_t cmaj = GlitchCore::kScales[0].mask;
        const double before = tonalFraction (L, sr, 48000, cmaj, 0);
        p.amount = 0.5f; run (h, L, R, oL, oR, p, 512);
        const double half = tonalFraction (oL, sr, 48000, cmaj, 0);
        h.reset(); p.amount = 1.0f; run (h, L, R, oL, oR, p, 512);
        const double full = tonalFraction (oL, sr, 48000, cmaj, 0);
        double eIn = 0, eOut = 0; for (int i = 48000; i < n; ++i) { eIn += L[(size_t) i] * L[(size_t) i]; eOut += oL[(size_t) i] * oL[(size_t) i]; }
        std::printf ("      in-scale energy: input %.2f, 50%% %.2f, 100%% %.2f; level change at 100%% %.1f dB\n",
                     before, half, full, 10 * std::log10 (eOut / eIn));
        CHECK (full > 0.85 && half > before, "100%% amount pulls noise onto C major");
        CHECK (std::abs (10 * std::log10 (eOut / eIn)) < 6.0, "level stays within 6 dB");
        bool finite = true; for (float v : oL) finite = finite && std::isfinite (v);
        CHECK (finite, "output finite");
    }

    // MIDI: one held A3 in harmonic mode → output energy on A harmonics.
    {
        Polychromatic h; h.prepare (sr, 512);
        const int n = 48000 * 2;
        auto L = noise (n, 5), R = noise (n, 6);
        std::vector<float> oL, oR;
        Polychromatic::Params p; p.amount = 1.0f; p.numHarmonics = 8;
        h.noteOn (1, 57, 1.0f);   // A3 = 220 Hz
        run (h, L, R, oL, oR, p, 512);
        // Measure energy within 15 Hz of k*220 vs total, 100..2000 Hz
        const int N = 8192, st = 48000; double in = 0, tot = 0;
        for (int k = 1; k < N / 2; ++k)
        {
            const double hz = k * sr / N; if (hz < 100 || hz > 2000) continue;
            double re = 0, im = 0;
            for (int i = 0; i < N; ++i) { const double w = 0.5 - 0.5 * std::cos (2 * kPi * i / N); re += oL[(size_t) (st + i)] * w * std::cos (2 * kPi * k * i / N); im -= oL[(size_t) (st + i)] * w * std::sin (2 * kPi * k * i / N); }
            const double e = re * re + im * im; tot += e;
            const double hn = hz / 220.0; if (std::abs (hz - std::round (hn) * 220.0) < 15 && std::round (hn) <= 8) in += e;
        }
        std::printf ("      energy on A3 harmonics: %.2f\n", in / tot);
        CHECK (in / tot > 0.9, "MIDI note A3 pulls energy onto its harmonics");

        // MPE: bend member channel by +1 semitone (48 range) -> 233 Hz
        Polychromatic h2; h2.prepare (sr, 512);
        p.mpe = true; p.numHarmonics = 1;
        h2.noteOn (2, 57, 1.0f); h2.pitchBend (2, 1.0f / 48.0f);
        run (h2, L, R, oL, oR, p, 512);
        double best = 0; double bestHz = 0;
        for (int k = 1; k < 600; ++k)
        {
            double re = 0, im = 0; const double hz = k * sr / N;
            for (int i = 0; i < N; ++i) { const double w = 0.5 - 0.5 * std::cos (2 * kPi * i / N); re += oL[(size_t) (st + i)] * w * std::cos (2 * kPi * k * i / N); im -= oL[(size_t) (st + i)] * w * std::sin (2 * kPi * k * i / N); }
            if (re * re + im * im > best) { best = re * re + im * im; bestHz = hz; }
        }
        CHECK (std::abs (bestHz - 233.08) < 6.0, "MPE per-note bend moves the partial (peak at %.1f Hz)", bestHz);
    }

    // Custom scale: only D in the low octave, only A in the high one.
    {
        Polychromatic::Params p; p.scale = GlitchCore::kCustomScaleIndex;
        p.customMask = (1u << 2) | (1u << (12 + 9)); p.amount = 1.0f;
        Polychromatic h; h.prepare (sr, 512);
        const int n = 48000 * 2; auto L = noise (n, 7), R = noise (n, 8);
        std::vector<float> oL, oR; run (h, L, R, oL, oR, p, 512);
        const double frac = tonalFraction (oL, sr, 48000, (1u << 2) | (1u << 9), 0);
        CHECK (frac > 0.85, "custom two-octave scale (D/A) captures %.2f of energy", frac);
    }

    std::printf (failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
    return failures ? 1 : 0;
}
