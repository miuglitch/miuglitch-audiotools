#include "harmonizer.h"
#include <algorithm>

namespace GlitchCore
{

static constexpr float kPiH    = 3.14159265358979f;
static constexpr float kTwoPiH = 6.28318530717959f;

//==============================================================================
// Construction / preparation
//==============================================================================

SpectralHarmonizer::SpectralHarmonizer()
{
    fft2048_.prepare (kMaxFftOrder - 1);
    fft4096_.prepare (kMaxFftOrder);
}

void SpectralHarmonizer::prepare (double sampleRate, int maxBlockSize, FftSize size)
{
    sampleRate_   = sampleRate > 0.0 ? sampleRate : 44100.0;
    maxBlockSize_ = maxBlockSize > 0 ? maxBlockSize : 512;

    for (auto& c : chans_)
    {
        c.inRing  .assign ((size_t) kMaxFftSize, 0.0f);
        c.outAccum.assign ((size_t) kMaxFftSize, 0.0f);
        c.dryRing .assign ((size_t) kMaxFftSize, 0.0f);
        c.spectrum.assign ((size_t) kMaxFftSize, Complex {});
        c.tonal   .assign ((size_t) kMaxFftSize, Complex {});
    }
    for (auto& b : blockIn_) b.assign ((size_t) maxBlockSize_, 0.0f);

    window_   .assign ((size_t) kMaxFftSize, 0.0f);
    binLog2_  .assign ((size_t) kMaxFftSize / 2 + 1, 0.0f);
    binTarget_.assign ((size_t) kMaxFftSize / 2 + 1, -1);
    binCents_ .assign ((size_t) kMaxFftSize / 2 + 1, 0.0f);

    prepared_ = true;
    setFftSize (size);
}

void SpectralHarmonizer::setFftSize (FftSize size)
{
    fftSize_ = size;
    fftN_    = (size == FftSize::k2048) ? kMaxFftSize / 2 : kMaxFftSize;
    hop_     = fftN_ / kOverlap;
    numBins_ = fftN_ / 2 + 1;
    fft_     = (size == FftSize::k2048) ? &fft2048_ : &fft4096_;

    if (! prepared_) return;

    // Periodic Hann: overlap-add of w^2 at hop N/4 sums to exactly 1.5.
    for (int n = 0; n < fftN_; ++n)
        window_[(size_t) n] = 0.5f - 0.5f * std::cos (kTwoPiH * (float) n / (float) fftN_);

    for (int k = 0; k < numBins_; ++k)
    {
        const double hz = std::max (1.0, (double) k * sampleRate_ / (double) fftN_);
        binLog2_[(size_t) k] = (float) std::log2 (hz);
    }

    reset();
}

void SpectralHarmonizer::reset()
{
    for (auto& c : chans_)
    {
        std::fill (c.inRing.begin(),   c.inRing.end(),   0.0f);
        std::fill (c.outAccum.begin(), c.outAccum.end(), 0.0f);
        std::fill (c.dryRing.begin(),  c.dryRing.end(),  0.0f);
        for (auto& p : c.partials) p = Partial {};
    }
    for (auto& d : extraDry_) std::fill (d.begin(), d.end(), 0.0f);
    partialPhase_.fill (0.0f);
    ringPos_ = 0;
    hopCounter_ = 0;
    wetSamplesInFlight_ = 0;
    frameAmount_ = 0.0f;
}

double SpectralHarmonizer::getTailLengthSeconds (float sustain) const
{
    // Latency plus the longest partial release (see processFrame).
    const double releaseSec = 0.05 + 12.0 * std::pow ((double) sustain, 2.0);
    return (double) fftN_ / sampleRate_ + releaseSec * 3.0;
}

//==============================================================================
// MIDI
//==============================================================================

int SpectralHarmonizer::findVoice (int channel, int note) const
{
    for (int i = 0; i < kMaxVoices; ++i)
        if (voices_[(size_t) i].active && voices_[(size_t) i].keyDown
            && voices_[(size_t) i].channel == channel && voices_[(size_t) i].note == note)
            return i;
    return -1;
}

int SpectralHarmonizer::allocateVoice()
{
    for (int i = 0; i < kMaxVoices; ++i)
        if (! voices_[(size_t) i].active) return i;

    // Steal the oldest, preferring released (latched / pedalled) voices.
    int best = 0;
    std::uint32_t bestAge = 0xffffffffu;
    bool bestReleased = false;
    for (int i = 0; i < kMaxVoices; ++i)
    {
        const auto& v = voices_[(size_t) i];
        const bool released = ! v.keyDown;
        if ((released && ! bestReleased) || (released == bestReleased && v.age < bestAge))
        {
            best = i; bestAge = v.age; bestReleased = released;
        }
    }
    return best;
}

void SpectralHarmonizer::noteOn (int channel, int note, float velocity)
{
    if (velocity <= 0.0f) { noteOff (channel, note); return; }
    channel = std::clamp (channel, 1, 16);

    // Latch: a new note after all keys were released starts a fresh chord.
    if (latch_)
    {
        bool anyDown = false;
        for (const auto& v : voices_) anyDown = anyDown || (v.active && v.keyDown);
        if (! anyDown)
            for (auto& v : voices_) v.active = false;
    }

    int idx = findVoice (channel, note);
    if (idx < 0) idx = allocateVoice();

    auto& v   = voices_[(size_t) idx];
    v.active   = true;
    v.keyDown  = true;
    v.channel  = channel;
    v.note     = std::clamp (note, 0, 127);
    v.velocity = std::clamp (velocity, 0.0f, 1.0f);
    v.pressure = 0.0f;
    v.timbre   = 0.5f;
    v.age      = ++voiceClock_;

    // In MPE, a member channel's bend/pressure state belongs to the new note.
    if (mpe_ && channel != kMpeMasterChannel)
    {
        channelBend_[(size_t) channel]     = 0.0f;
        channelPressure_[(size_t) channel] = 0.0f;
        channelTimbre_[(size_t) channel]   = 0.5f;
    }
}

void SpectralHarmonizer::releaseVoice (Voice& v)
{
    v.keyDown = false;
    if (! latch_ && ! sustainPedal_)
        v.active = false;
}

void SpectralHarmonizer::noteOff (int channel, int note)
{
    channel = std::clamp (channel, 1, 16);
    const int idx = findVoice (channel, note);
    if (idx >= 0) releaseVoice (voices_[(size_t) idx]);
}

void SpectralHarmonizer::pitchBend (int channel, float bend)
{
    channelBend_[(size_t) std::clamp (channel, 1, 16)] = std::clamp (bend, -1.0f, 1.0f);
}

void SpectralHarmonizer::channelPressure (int channel, float pressure)
{
    channelPressure_[(size_t) std::clamp (channel, 1, 16)] = std::clamp (pressure, 0.0f, 1.0f);
}

void SpectralHarmonizer::polyPressure (int channel, int note, float pressure)
{
    const int idx = findVoice (std::clamp (channel, 1, 16), note);
    if (idx >= 0) voices_[(size_t) idx].pressure = std::clamp (pressure, 0.0f, 1.0f);
}

void SpectralHarmonizer::controller (int channel, int cc, float value)
{
    channel = std::clamp (channel, 1, 16);
    if (cc == 74)
    {
        channelTimbre_[(size_t) channel] = std::clamp (value, 0.0f, 1.0f);
    }
    else if (cc == 64)
    {
        sustainPedal_ = value >= 0.5f;
        if (! sustainPedal_)
            for (auto& v : voices_)
                if (v.active && ! v.keyDown && ! latch_) v.active = false;
    }
    else if (cc == 123 || cc == 120)
    {
        allNotesOff();
    }
}

void SpectralHarmonizer::allNotesOff()
{
    for (auto& v : voices_) { v.active = false; v.keyDown = false; }
    sustainPedal_ = false;
}

bool SpectralHarmonizer::hasActiveNotes() const
{
    for (const auto& v : voices_) if (v.active) return true;
    return false;
}

int SpectralHarmonizer::getActiveNoteCount() const
{
    int n = 0;
    for (const auto& v : voices_) if (v.active) ++n;
    return n;
}

void SpectralHarmonizer::setLatch (bool shouldLatch)
{
    if (latch_ && ! shouldLatch)
        for (auto& v : voices_)
            if (v.active && ! v.keyDown && ! sustainPedal_) v.active = false;
    latch_ = shouldLatch;
}

float SpectralHarmonizer::noteFrequency (const Voice& v, const Params& p) const
{
    float semis = (float) v.note;
    if (p.mpe)
    {
        semis += channelBend_[(size_t) kMpeMasterChannel] * p.bendRange;
        if (v.channel != kMpeMasterChannel)
            semis += channelBend_[(size_t) v.channel] * p.mpeBendRange;
    }
    else
    {
        semis += channelBend_[(size_t) v.channel] * p.bendRange;
    }
    return 440.0f * std::exp2 ((semis - 69.0f) / 12.0f);
}

//==============================================================================
// Targets
//==============================================================================

void SpectralHarmonizer::buildTargets (const Params& p)
{
    numTargets_ = 0;
    const float nyq   = (float) sampleRate_ * 0.5f;
    const float maxHz = std::min (20000.0f, nyq * 0.97f);
    const float minHz = 20.0f;

    auto push = [&] (float hz, float weight, int id)
    {
        if (hz < minHz || hz > maxHz || weight <= 0.0f || numTargets_ >= kNumPartialIds) return;
        targets_[(size_t) numTargets_++] = { hz, std::log2 (hz), weight, id };
    };

    const bool useMidi = (p.source == TargetSource::Midi)
                      || (p.source == TargetSource::Auto && hasActiveNotes());

    if (useMidi)
    {
        const int numH = std::clamp (p.numHarmonics, 1, kMaxPartialsPerVoice);
        for (int vi = 0; vi < kMaxVoices; ++vi)
        {
            const auto& v = voices_[(size_t) vi];
            if (! v.active) continue;

            const float f0 = noteFrequency (v, p);
            float weight   = 0.5f + 0.5f * v.velocity;
            float tilt     = 0.0f;     // spectral tilt exponent across partials
            if (p.mpe)
            {
                const float press = std::max (v.pressure, channelPressure_[(size_t) v.channel]);
                weight *= 0.5f + press;
                const float timbre = v.channel != kMpeMasterChannel
                                   ? channelTimbre_[(size_t) v.channel] : 0.5f;
                tilt = (0.5f - timbre) * 2.0f;   // CC74 up = brighter
            }
            else if (v.pressure > 0.0f)
            {
                weight *= 0.5f + v.pressure;
            }

            const int idBase = kNumScaleIds + vi * kMaxPartialsPerVoice;
            if (p.midiTargets == MidiTargets::Harmonics)
            {
                for (int h = 1; h <= numH; ++h)
                    push (f0 * (float) h, weight * std::pow ((float) h, -tilt), idBase + h - 1);
            }
            else
            {
                // Every octave of the note's pitch class, octave index 0 = f0 / 32.
                for (int o = 0; o < kMaxPartialsPerVoice && o < 16; ++o)
                {
                    const float hz = f0 * std::exp2 ((float) (o - 5));
                    push (hz, weight * std::pow (std::exp2 ((float) (o - 5)) , -tilt * 0.5f), idBase + o);
                }
            }
        }
    }
    else
    {
        const bool custom = p.scale >= kCustomScaleIndex;
        const std::uint16_t mask = custom ? 0 : kScales[(size_t) std::clamp (p.scale, 0, (int) kScales.size() - 1)].mask;
        const int root = ((p.root % 12) + 12) % 12;

        for (int m = 0; m < kNumScaleIds; ++m)
        {
            bool in;
            if (custom) in = ((p.customMask >> (m % 24)) & 1u) != 0;        // absolute, C-anchored
            else        in = ((mask >> (((m - root) % 12 + 12) % 12)) & 1u) != 0;
            if (in)
                push (440.0f * std::exp2 ((float) (m - 69) / 12.0f), 1.0f, m);
        }
    }

    std::sort (targets_.begin(), targets_.begin() + numTargets_,
               [] (const Target& a, const Target& b) { return a.freq < b.freq; });
}

void SpectralHarmonizer::updateFrameAmount (float target)
{
    target = std::clamp (target, 0.0f, 1.0f);
    // ~50 ms glide at hop 1024 / 48k, snaps to exactly 0 so the null is reachable.
    frameAmount_ += 0.45f * (target - frameAmount_);
    if (std::abs (target - frameAmount_) < 1.0e-4f) frameAmount_ = target;
}

//==============================================================================
// Hann main lobe of a zero-phase (centred) frame, x in bins from the peak.
//==============================================================================

float SpectralHarmonizer::lobeKernel (float x)
{
    auto sinc = [] (float t)
    {
        if (std::abs (t) < 1.0e-6f) return 1.0f;
        return std::sin (kPiH * t) / (kPiH * t);
    };
    return sinc (x) + 0.5f * (sinc (x - 1.0f) + sinc (x + 1.0f));
}

//==============================================================================
// One STFT frame
//==============================================================================

void SpectralHarmonizer::processFrame (int channel)
{
    auto& c        = chans_[(size_t) channel];
    const int N    = fftN_;
    const int half = N / 2;
    Complex* X     = c.spectrum.data();

    // Windowed frame from the ring, rotated by N/2 so it is zero-phase about
    // its centre: a sinusoid's main lobe is then real-valued in the spectrum.
    for (int n = 0; n < N; ++n)
    {
        const int src = (ringPos_ + (kMaxFftSize - N) + n) % kMaxFftSize;
        const float s = c.inRing[(size_t) src] * window_[(size_t) n];
        X[(n + half) % N] = Complex (s, 0.0f);
    }
    fft_->perform (X, false);

    const float amount   = frameAmount_;
    const float hopSec   = (float) hop_ / (float) sampleRate_;
    const float release  = std::exp (-hopSec / (0.05f + 12.0f * frameSustain_ * frameSustain_));
    const float binHz    = (float) sampleRate_ / (float) N;

    // --- Capture: every bin's energy goes to its nearest target -------------
    for (auto& part : c.partials) part.isTarget = false;

    if (amount > 0.0f && numTargets_ > 0)
    {
        int t = 0;
        for (int k = 1; k < half; ++k)
        {
            const float lf = binLog2_[(size_t) k];
            while (t + 1 < numTargets_
                   && std::abs (targets_[(size_t) t + 1].logFreq - lf)
                      <= std::abs (targets_[(size_t) t].logFreq - lf))
                ++t;

            const float cents = (lf - targets_[(size_t) t].logFreq) * 1200.0f;
            binTarget_[(size_t) k] = (std::abs (cents) <= 700.0f) ? t : -1;
            binCents_ [(size_t) k] = cents;
        }

        std::array<float, kNumPartialIds> captured {};
        for (int k = 1; k < half; ++k)
        {
            const int t2 = binTarget_[(size_t) k];
            if (t2 >= 0) captured[(size_t) targets_[(size_t) t2].id] += std::norm (X[k]);
        }

        for (int i = 0; i < numTargets_; ++i)
        {
            const auto& tg = targets_[(size_t) i];
            auto& part     = c.partials[(size_t) tg.id];
            part.isTarget  = true;
            part.freq      = tg.freq;
            part.weight    = tg.weight;
            part.energy    = std::max (captured[(size_t) tg.id], part.energy * release);
        }
    }
    else
    {
        binTarget_.assign (binTarget_.size(), -1);
    }

    // Partials that stopped being targets (note off, scale change) ring out.
    for (auto& part : c.partials)
        if (! part.isTarget) part.energy *= release;

    // --- Resynthesise the partials as phase-continuous sinusoids ------------
    Complex* T = c.tonal.data();
    std::fill (T, T + half + 1, Complex {});
    if (amount > 0.0f)
    {
        for (int id = 0; id < kNumPartialIds; ++id)
        {
            const auto& part = c.partials[(size_t) id];
            if (part.energy < 1.0e-12f || part.freq <= 0.0f) continue;

            const float b  = part.freq / binHz;
            const int   k0 = (int) std::floor (b) - 1;
            float kern[4];
            float kSum = 0.0f;
            for (int j = 0; j < 4; ++j)
            {
                kern[j] = lobeKernel ((float) (k0 + j) - b);
                kSum   += kern[j] * kern[j];
            }
            if (kSum <= 1.0e-9f) continue;

            const float amp = std::sqrt (part.energy / kSum) * part.weight;
            const float ph  = partialPhase_[(size_t) id];
            const Complex rot (std::cos (ph), std::sin (ph));
            for (int j = 0; j < 4; ++j)
            {
                const int k = k0 + j;
                if (k >= 1 && k < half) T[k] += rot * (amp * kern[j]);
            }
        }
    }

    // --- Mix: sieve the original, crossfade (equal power) into the partials --
    if (amount > 0.0f)
    {
        const float theta  = amount * kPiH * 0.5f;
        const float dryG   = std::cos (theta);
        const float tonalG = std::sin (theta);
        const float sieve  = std::min (1.0f, amount * 1.5f);
        const float sigma  = 120.0f - 100.0f * amount;   // sieve width, cents

        for (int k = 0; k <= half; ++k)
        {
            float g = 1.0f - sieve;
            const int t = (k > 0 && k < half) ? binTarget_[(size_t) k] : -1;
            if (t >= 0)
            {
                const float d = binCents_[(size_t) k] / sigma;
                g += sieve * std::exp (-d * d);
            }
            X[k] = X[k] * (dryG * g) + T[k] * tonalG;
        }

        // Hermitian mirror so the inverse transform stays real.
        X[0]    = Complex (X[0].real(), 0.0f);
        X[half] = Complex (X[half].real(), 0.0f);
        for (int k = 1; k < half; ++k) X[N - k] = std::conj (X[k]);
    }

    fft_->perform (X, true);

    // Undo the rotation, synthesis window, overlap-add.
    const float norm = 1.0f / ((float) N * 1.5f);
    for (int n = 0; n < N; ++n)
        c.outAccum[(size_t) n] += X[(n + half) % N].real() * window_[(size_t) n] * norm;
}

//==============================================================================
// Process
//==============================================================================

void SpectralHarmonizer::process (const float* const* inputChannels,
                                  float* const*       outputChannels,
                                  int                 numChannels,
                                  int                 numSamples,
                                  const Params&       params)
{
    if (! prepared_ || numChannels < 1 || numSamples < 1)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            if (inputChannels[ch] != outputChannels[ch])
                std::copy (inputChannels[ch], inputChannels[ch] + numSamples, outputChannels[ch]);
        return;
    }

    mpe_ = params.mpe;
    setLatch (params.latch);

    const int procCh = std::min (numChannels, kMaxChannels);

    // Extra (surround) channels: plain delay so everything stays aligned.
    if ((int) extraDry_.size() < numChannels - kMaxChannels)
        extraDry_.resize ((size_t) (numChannels - kMaxChannels),
                          std::vector<float> ((size_t) kMaxFftSize, 0.0f));

    int done = 0;
    while (done < numSamples)
    {
        const int chunk = std::min (numSamples - done, maxBlockSize_);

        // Copy inputs first: host buffers may be processed in place.
        for (int ch = 0; ch < procCh; ++ch)
            std::copy (inputChannels[ch] + done, inputChannels[ch] + done + chunk,
                       blockIn_[(size_t) ch].begin());

        for (int s = 0; s < chunk; ++s)
        {
            const int latePos = (ringPos_ + kMaxFftSize - fftN_) % kMaxFftSize;
            const bool useDry = (wetSamplesInFlight_ == 0);

            for (int ch = 0; ch < procCh; ++ch)
            {
                auto& c = chans_[(size_t) ch];
                const float x = blockIn_[(size_t) ch][(size_t) s];

                const float wet = c.outAccum[(size_t) hopCounter_];
                const float dry = c.dryRing[(size_t) latePos];   // input from N samples ago
                outputChannels[ch][done + s] = useDry ? dry : wet;

                c.inRing [(size_t) ringPos_] = x;
                c.dryRing[(size_t) ringPos_] = x;
            }
            for (int ch = kMaxChannels; ch < numChannels; ++ch)
            {
                auto& d = extraDry_[(size_t) (ch - kMaxChannels)];
                const float x = inputChannels[ch][done + s];
                outputChannels[ch][done + s] = d[(size_t) latePos];
                d[(size_t) ringPos_] = x;
            }

            ringPos_ = (ringPos_ + 1) % kMaxFftSize;
            if (wetSamplesInFlight_ > 0) --wetSamplesInFlight_;

            if (++hopCounter_ == hop_)
            {
                hopCounter_ = 0;
                updateFrameAmount (params.amount);
                frameSustain_ = std::clamp (params.sustain, 0.0f, 1.0f);
                buildTargets (params);

                for (int ch = 0; ch < procCh; ++ch)
                {
                    auto& c = chans_[(size_t) ch];
                    std::copy (c.outAccum.begin() + hop_, c.outAccum.begin() + fftN_, c.outAccum.begin());
                    std::fill (c.outAccum.begin() + (fftN_ - hop_), c.outAccum.begin() + fftN_, 0.0f);
                    processFrame (ch);
                }

                // Advance the shared oscillator phases by one hop.
                const float hopSec = (float) hop_ / (float) sampleRate_;
                const auto& parts  = chans_[0].partials;
                for (int id = 0; id < kNumPartialIds; ++id)
                {
                    if (parts[(size_t) id].freq <= 0.0f) continue;
                    float ph = partialPhase_[(size_t) id] + kTwoPiH * parts[(size_t) id].freq * hopSec;
                    ph -= kTwoPiH * std::floor (ph / kTwoPiH);
                    partialPhase_[(size_t) id] = ph;
                }

                // Any frame processed with Amount > 0 keeps the wet path live
                // until it has fully left the overlap-add buffer.
                const bool ringing = frameAmount_ > 0.0f;
                if (ringing) wetSamplesInFlight_ = fftN_;
            }
        }
        done += chunk;
    }
}

} // namespace GlitchCore
