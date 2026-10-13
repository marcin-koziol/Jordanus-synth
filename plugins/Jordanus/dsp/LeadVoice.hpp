/*
 * Jordanus - the single monophonic lead voice.
 *
 * Signal flow (derived from analysing reference recordings of the target
 * sound, see README):
 *
 *   fundamental (sine) -----------------+
 *   + detuned 1/n body series           |
 *   flageolet (waveform @ k*f) ---------+--> tanh shaper --> DC --> cabinet --> amp env
 *     ...all of it at 1x/2x/4x f        |                           (4k bell + steep LP)
 *        (octave-up half of SUB CC)     |
 *   sub -1 oct / -2 oct sines ----------+
 *     (octave-down half of SUB CC)
 *
 * - Flageolets: the harmonics controller is bipolar around its centre (=
 *   the plain tone, base.wav). Each direction has ONE flageolet - by
 *   default up the 3rd harmonic (harm1.wav), down the 2nd (harm2.wav),
 *   selectable 2nd..8th with the UP/DOWN knobs - and the strip
 *   position crossfades (equal-power) from the plain tone to it: the
 *   flageolet swells in and pushes the base down to kBaseFloor at full
 *   travel, smoothed over time by the Bloom knob. No passing through other
 *   harmonics - the references never do. Each flageolet is a whole waveform
 *   at k*f (FLAGEOLET knob: sine -> saw -> square; the references sit
 *   between saw and square).
 * - VOICES/SPREAD add detuned unison copies of the base oscillator
 *   (fundamental + body) - a supersaw with WAVE on saw. Flageolets and subs
 *   stay single and phase-locked, so they don't smear.
 * - Body: the references' upper harmonics wobble in level/phase (40-90%)
 *   while the fundamental stays rock steady, and the plain tone has no fixed
 *   waveform edge. A 1/n series split into two copies detuned +-kBodyDetune
 *   cents reproduces that: each harmonic beats against itself, the
 *   fundamental (not part of the body) stays locked.
 * - SUB controller is bipolar around its centre (CC 64 = nothing): going
 *   down brings in a -1 oct, then a -2 oct sine; going up moves the whole
 *   tone up one, then two octaves, replacing the original pitch (as in the
 *   first 0.4s of harmonics3.wav). The subs go INTO the shaper with the lead:
 *   the references show intermodulation sidebands (f +- f/4, f +- f/2 at
 *   ~-30dB) that only a shared nonlinearity makes - that grit is what kept
 *   clean post-shaper subs sounding too soft. All oscillators of the lead
 *   and the subs derive from one phase accumulator running at f/4, so the
 *   subs can never drift/beat against the lead.
 * - Velocity is deliberately not used anywhere.
 */

#pragma once

#include "ADSR.hpp"

#include <algorithm>
#include <cmath>
#include <complex>

namespace jordanus {

struct LeadParams
{
    float bloom = 0.08f;     // s, flageolet swell smoothing (harm2.wav swells in ~0.15s)
    float subLevel = 0.6f;
    float subDeep = 0.6f;    // -2 oct level at the bottom of the SUB controller, vs -1 oct
    float subMorph = 0.15f;  // s, how gradually the SUB controller's moves take effect
    float body = 0.2f;       // detuned body layer level (1 = full saw/square series); = WAVE 0.1
    float shape = 0.0f;      // body series: 0 = saw-like (all harmonics) .. 1 = square-like (odd only)
    float tilt = 0.8f;       // body series rolloff exponent: harmonic n at 1/n^tilt
    float detune = 7.0f;     // cents, body copies at +-detune
    float flagShape = 0.6f;  // flageolet waveform: 0 = sine .. 0.5 = saw .. 1 = square (same scale as WAVE)
    float subShape = 0.0f;   // sub: 0 = sine .. 1 = band-limited square
    float baseUnderHarm = 0.05f; // base (fundamental + body) left under a full flageolet: -15..-40dB in harm1/harm2
    float baseUnderSub = 1.0f;   // base left with the SUB controller fully down (1 = the sub just adds)
    float baseSub = 0.0f;        // fixed -1 oct sub under the base, vs the fundamental; ducks with the base
    float drive = 0.06f;     // defaults from drive..tilt fitted to base.wav (both controllers centred)
    float asym = 0.1f;
    float cabHz = 4200.0f;   // cabinet resonance; the lowpass sits above it
    float glide = 0.0f;      // s
    float attack = 0.004f;   // s
    float decay = 0.25f;     // s
    float sustain = 0.7f;    // base.wav settles ~3dB under its attack after ~0.2s
    float release = 0.18f;   // s
    int upHarmonic = 3;      // flageolet above the harmonics controller's centre (harm1.wav)
    int downHarmonic = 2;    // flageolet below it (harm2.wav)
    int voices = 1;          // unison copies of the base oscillator (supersaw), 1 = off
    float spread = 12.0f;    // cents between the outermost unison copies
};

// RBJ cookbook biquad, transposed direct form II
class Biquad
{
public:
    void setLowpass(float hz, float q, float sampleRate) noexcept
    {
        const float w0 = 2.0f * (float)M_PI * hz / sampleRate;
        const float cosw = std::cos(w0);
        const float alpha = std::sin(w0) / (2.0f * q);
        normalize((1.0f - cosw) * 0.5f, 1.0f - cosw, (1.0f - cosw) * 0.5f,
                  1.0f + alpha, -2.0f * cosw, 1.0f - alpha);
    }

    void setPeak(float hz, float q, float gainDb, float sampleRate) noexcept
    {
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float w0 = 2.0f * (float)M_PI * hz / sampleRate;
        const float cosw = std::cos(w0);
        const float alpha = std::sin(w0) / (2.0f * q);
        normalize(1.0f + alpha * A, -2.0f * cosw, 1.0f - alpha * A,
                  1.0f + alpha / A, -2.0f * cosw, 1.0f - alpha / A);
    }

    // |H(f)| of the current coefficients
    float magnitude(float hz, float sampleRate) const noexcept
    {
        const std::complex<float> z1 = std::polar(1.0f, -2.0f * (float)M_PI * hz / sampleRate);
        const std::complex<float> z2 = z1 * z1;
        return std::abs((fB0 + fB1 * z1 + fB2 * z2) / (1.0f + fA1 * z1 + fA2 * z2));
    }

    float process(float x) noexcept
    {
        const float y = fB0 * x + fZ1;
        fZ1 = fB1 * x - fA1 * y + fZ2;
        fZ2 = fB2 * x - fA2 * y;
        return y;
    }

    void reset() noexcept { fZ1 = fZ2 = 0.0f; }

private:
    void normalize(float b0, float b1, float b2, float a0, float a1, float a2) noexcept
    {
        fB0 = b0 / a0; fB1 = b1 / a0; fB2 = b2 / a0;
        fA1 = a1 / a0; fA2 = a2 / a0;
    }

    float fB0 = 1.0f, fB1 = 0.0f, fB2 = 0.0f, fA1 = 0.0f, fA2 = 0.0f;
    float fZ1 = 0.0f, fZ2 = 0.0f;
};

class LeadVoice
{
public:
    static constexpr int kMaxHarmonic = 8;   // highest selectable flageolet
    static constexpr int kMaxVoices = 7;     // unison copies of the base oscillator
    static constexpr int kMaxPartial = 128;  // highest partial of the additive sum

    // bipolar controller 0..1 -> -1..+1 with a small dead zone, so CC 64
    // (= 0.504) really is the centre
    static float bipolar(float amount) noexcept
    {
        const float b = 2.0f * std::clamp(amount, 0.0f, 1.0f) - 1.0f;
        return std::fabs(b) < kCentreDeadZone ? 0.0f
             : std::copysign((std::fabs(b) - kCentreDeadZone) / (1.0f - kCentreDeadZone), b);
    }

    void setSampleRate(double sampleRate) noexcept
    {
        fSampleRate = sampleRate > 0.0 ? (float)sampleRate : 44100.0f;
        fEnv.setSampleRate(fSampleRate);
        // low enough to pass a -2 oct sub of low notes (E2 / 4 = 20Hz)
        fDcCoeff = std::exp(-2.0f * (float)M_PI * 5.0f / fSampleRate);
        fBendCoeff = onePole(0.003f);
        fHarmCoeff = onePole(0.008f);
        applyParams(fParams);
    }

    void applyParams(const LeadParams& p) noexcept
    {
        fParams = p;
        fBloomCoeff = onePole(std::max(0.001f, p.bloom));
        fSubCoeff = onePole(std::max(0.001f, p.subMorph));
        fGlideCoeff = p.glide > 0.0005f ? onePole(p.glide) : 1.0f;
        fEnv.setAttack(p.attack);
        fEnv.setDecay(p.decay);
        fEnv.setSustain(p.sustain);
        fEnv.setRelease(p.release);
        fEnv.setCurve(0.5f);

        const float cab = std::min(p.cabHz, 0.3f * fSampleRate);
        fCabBell.setPeak(cab, 2.5f, 12.0f, fSampleRate);
        fCabLp1.setLowpass(cab * 1.4f, 0.7f, fSampleRate);
        fCabLp2.setLowpass(cab * 1.4f, 0.7f, fSampleRate);

        // nothing much above the cabinet lowpass survives anyway, but partials
        // up there would still feed the shaper and fold back down as aliasing
        fPartialLimitHz = std::min(0.45f * fSampleRate, cab * 1.4f * 1.2f);

        // waveform tables: per-partial weights for the body / flageolet /
        // sub series, so process() never calls pow()
        for (int n = 1; n <= kMaxPartial; ++n)
        {
            fBodyCoef[n] = bodyCoefficient(n, p.shape, p.tilt);
            fFlagCoef[n] = flageoletCoefficient(n, p.flagShape);
        }
        for (int j = 1; j <= kSubHarmonics; ++j)
            fSubCoef[j] = j == 1 ? 1.0f : p.subShape / (float)j;
        fBodyRatio = std::exp2((double)p.detune / 1200.0);

        // unison: the main copy stays put; the extra ones fan out in
        // symmetric pairs, outermost at +-spread/2 (cents)
        fVoices = std::clamp(p.voices, 1, kMaxVoices);
        const int extra = fVoices - 1;
        const int pairs = (extra + 1) / 2;
        for (int j = 1; j <= extra; ++j)
        {
            const double cents = (double)((j + 1) / 2) / (double)std::max(1, pairs) * 0.5 * (double)p.spread;
            fUnisonRatio[j] = std::exp2((j % 2 ? cents : -cents) / 1200.0);
        }
    }

    // DRIVE knob -> shaper gain. Starts well below 1 so the bottom of the
    // knob is nearly clean: base.wav's plain tone has its 3rd harmonic at
    // -29dB, which a gain of 1+ already overshoots (-15dB).
    static float shaperGain(float drive) noexcept { return 0.25f + 9.75f * drive; }

    // The WAVE knob: one control from sine through saw to square. First half
    // brings the body series in (0 = plain sine, 0.5 = full saw), second half
    // fades its even partials out (1 = square). Shared by the plugin and the
    // UI preview so both read the knob the same way.
    static void applyWave(LeadParams& p, float wave) noexcept
    {
        wave = std::clamp(wave, 0.0f, 1.0f);
        p.body = std::min(1.0f, 2.0f * wave);
        p.shape = std::max(0.0f, 2.0f * wave - 1.0f);
    }

    // weight of multiple m of a flageolet, on the WAVE-style scale: first half
    // brings the upper multiples in (sine -> saw), second half fades the even
    // ones out (saw -> square)
    static float flageoletCoefficient(int m, float shape) noexcept
    {
        if (m == 1)
            return 1.0f;
        const float amount = std::min(1.0f, 2.0f * shape);
        const float evenGain = (m % 2 == 0) ? 1.0f - std::max(0.0f, 2.0f * shape - 1.0f) : 1.0f;
        return amount * evenGain / (float)m;
    }

    // weight of body partial n (n >= 2): 1/n^tilt, even partials faded out
    // by shape (saw-like -> square-like). Shared with previewCycle().
    static float bodyCoefficient(int n, float shape, float tilt) noexcept
    {
        const float evenGain = (n % 2 == 0) ? 1.0f - shape : 1.0f;
        return evenGain / std::pow((float)n, tilt);
    }

    // one cycle of the plain tone (fundamental + body + base sub, through the shaper,
    // before the cabinet) at phase 0..1, for the UI's waveform preview. The
    // body copies are drawn in phase - their detune only moves over time.
    static float previewCycle(const LeadParams& p, double phase) noexcept
    {
        const double theta = 2.0 * M_PI * phase;
        float body = 0.0f;
        for (int n = 2; n <= 48; ++n)
            body += bodyCoefficient(n, p.shape, p.tilt) * (float)std::sin(n * theta);
        float x = (float)std::sin(theta) + kBodyMax * p.body * body;
        // fixed BASE SUB (-1 oct, SUB WAVE shape): the preview spans two
        // cycles of the fundamental, i.e. exactly one of the sub
        for (int j = 1; p.baseSub > 1e-4f && j <= kSubHarmonics; j += 2)
            x += p.baseSub * (j == 1 ? 1.0f : p.subShape / (float)j) * (float)std::sin(j * 0.5 * theta);
        const float d = shaperGain(p.drive), b = 0.5f * p.asym;
        return (std::tanh(d * (x + b)) - std::tanh(d * b)) / std::tanh(d);
    }

    // retrigger = fresh attack (no key was held); otherwise a legato move to
    // the new note, gliding when Glide > 0
    void noteOn(int note, bool retrigger) noexcept
    {
        fTargetNote = (float)note;
        if (retrigger || fEnv.isIdle())
        {
            fCurrentNote = fTargetNote;
            fEnv.noteOn();
        }
        fNote = note;
    }

    void noteOff() noexcept { fEnv.noteOff(); }

    // hard stop (all-notes-off), also clears filter memory
    void kill() noexcept
    {
        fEnv = ADSR();
        setSampleRate(fSampleRate);
        fCabBell.reset();
        fCabLp1.reset();
        fCabLp2.reset();
    }

    bool isActive() const noexcept { return !fEnv.isIdle(); }
    int getNote() const noexcept { return fNote; }

    void setPitchBend(float semitones) noexcept { fBendTarget = semitones; }
    void setHarmonics(float amount) noexcept { fHarmTarget = std::clamp(amount, 0.0f, 1.0f); }
    void setSub(float amount) noexcept { fSubTarget = std::clamp(amount, 0.0f, 1.0f); }

    float process() noexcept
    {
        // controller smoothing runs even while idle, so a wheel moved
        // between notes is already settled when the next note starts
        fBend += (fBendTarget - fBend) * fBendCoeff;
        fHarm += (fHarmTarget - fHarm) * fHarmCoeff;
        fSub += (fSubTarget - fSub) * fSubCoeff;
        fCurrentNote += (fTargetNote - fCurrentNote) * fGlideCoeff;

        if (fEnv.isIdle())
            return 0.0f;

        const float freq = 440.0f * std::exp2((fCurrentNote + fBend - 69.0f) / 12.0f);
        fPhase4 += (double)freq * 0.25 / (double)fSampleRate;
        fPhase4 -= std::floor(fPhase4);
        fBodyPhaseUp += (double)freq * fBodyRatio / (double)fSampleRate;
        fBodyPhaseUp -= std::floor(fBodyPhaseUp);
        fBodyPhaseDown += (double)freq / fBodyRatio / (double)fSampleRate;
        fBodyPhaseDown -= std::floor(fBodyPhaseDown);
        for (int j = 1; j < fVoices; ++j)
        {
            fUnisonPhase[j] += (double)freq * fUnisonRatio[j] / (double)fSampleRate;
            fUnisonPhase[j] -= std::floor(fUnisonPhase[j]);
        }

        updateHarmonicGains();

        // --- SUB controller: bipolar around the centre ---
        const float subPos = bipolar(fSub);
        const float down = std::max(0.0f, -subPos);
        const float up = std::max(0.0f, subPos);
        fBaseDuck = 1.0f - (1.0f - fParams.baseUnderSub) * down;

        // --- partial tables for the lead and both body copies ---
        const int maxPartial = std::min(kMaxPartial, (int)(fPartialLimitHz / freq));
        fillSines(fSin, fPhase4 * 4.0, maxPartial);
        fillSines(fSinBodyUp, fBodyPhaseUp, maxPartial);
        fillSines(fSinBodyDown, fBodyPhaseDown, maxPartial);
        for (int j = 1; j < fVoices; ++j)
            fillSines(fSinUnison[j], fUnisonPhase[j], maxPartial);

        // octave-up half: equal-power walk 1x -> 2x -> 4x the whole tone. An
        // octave that wouldn't fit under the partial limit folds back down
        // instead of going silent on high notes.
        float partials = 0.0f;
        float effectiveMult = 0.0f; // weight-averaged, for the cabinet compensation below
        const float upPos = up * 2.0f;
        for (int i = 0; i < 3; ++i)
        {
            const float w = 1.0f - std::fabs(upPos - (float)i);
            if (w <= 1e-4f)
                continue;
            int mult = 1 << i;
            while (mult > 1 && (float)mult * freq > fPartialLimitHz)
                mult >>= 1;
            const float gain = std::sin(w * (float)M_PI_2);
            partials += gain * tone(mult, maxPartial);
            effectiveMult += w * (float)mult;
        }

        // octave-down half: -1 oct first, then -2 oct joins in; into the shaper
        // (sub waves: odd harmonics j of f/2 and f/4, i.e. multiples 2j and j of f/4)
        // plus the fixed BASE SUB: the same -1 oct wave, always under the base
        // and ducking with it (flageolet / SUB controller)
        const float sub1 = std::min(1.0f, 2.0f * down);
        const float sub2 = std::max(0.0f, 2.0f * down - 1.0f) * fParams.subDeep;
        const float baseSub = fParams.baseSub * fGains[1] * fBaseDuck;
        if (sub1 > 0.0f || baseSub > 1e-4f)
        {
            const int quarterLimit = std::min(2 * kSubHarmonics, (int)(4.0f * fPartialLimitHz / freq));
            fillSines(fSinQuarter, fPhase4, std::max(2, quarterLimit));
            float wave1 = 0.0f, wave2 = 0.0f;
            for (int j = 1; j <= kSubHarmonics; j += 2)
            {
                if (2 * j <= quarterLimit || j == 1)
                    wave1 += fSubCoef[j] * fSinQuarter[2 * j];
                if (j <= quarterLimit)
                    wave2 += fSubCoef[j] * fSinQuarter[j];
            }
            partials += fParams.subLevel * kSubScale * (sub1 * wave1 + sub2 * wave2) + baseSub * wave1;
        }

        // --- shaper, DC block, cabinet ---
        const float drive = shaperGain(fParams.drive);
        float y = shapeAdaa(partials, drive, 0.5f * fParams.asym);

        const float dcOut = y - fDcX1 + fDcCoeff * fDcY1;
        fDcX1 = y;
        fDcY1 = dcOut;

        if (fCabCompCountdown-- <= 0)
        {
            fCabCompCountdown = 32;
            fCabComp = cabinetCompensation(freq * effectiveMult * dominantHarmonic((int)((float)maxPartial / std::max(1.0f, effectiveMult))));
        }
        y = fCabLp2.process(fCabLp1.process(fCabBell.process(dcOut))) * kCabTrim * fCabComp;

        return y * fEnv.process();
    }

private:
    static constexpr float kSubScale = 0.32f; // default LEVEL -> -1 oct ~-18dB vs the fundamental, as in harmonics3/harmo4
    static constexpr float kCentreDeadZone = 0.03f;
    static constexpr float kBodyMax = 1.0f;  // Body knob at 1 -> the full 1/n series, i.e. a real saw/square (default 0.25, fitted to harmonics1.wav)
    static constexpr int kSubHarmonics = 15; // highest odd harmonic of the sub waves
    static constexpr float kCabTrim = 0.5f; // the +12dB bell would otherwise run hot

    // The cabinet (+12dB bell, steep lowpass above it) is right for a
    // guitar-range tone whose energy sits far below the bell - but high
    // notes, +1/+2 oct and high flageolets move the dominant partial onto the
    // bell (~+9dB jump) or past the lowpass (~-7dB). Undo most of the
    // cabinet's gain at the dominant frequency, measured on the actual
    // filters (relative to its gain at 200Hz, where the plain tone lives),
    // limited to -12..+6dB.
    float cabinetCompensation(float dominantHz) const noexcept
    {
        auto cabGain = [&](float hz) {
            hz = std::min(hz, 0.49f * fSampleRate);
            return fCabBell.magnitude(hz, fSampleRate) * fCabLp1.magnitude(hz, fSampleRate) * fCabLp2.magnitude(hz, fSampleRate);
        };
        const float relative = cabGain(dominantHz) / cabGain(200.0f);
        return std::clamp(std::pow(relative, -0.8f), 0.25f, 2.0f);
    }

    // where the flageolet gains put the energy, as a harmonic number
    // (power-weighted, in octaves): 1 for the plain tone, ~3 on the 3rd, ...
    // Flageolets above `limit` are folded into the fundamental by tone(), so
    // they count as harmonic 1 here.
    float dominantHarmonic(int limit) const noexcept
    {
        float weighted = 0.0f, power = 0.0f;
        for (int k = 1; k <= kMaxHarmonic; ++k)
        {
            const float p = fGains[k] * fGains[k];
            weighted += p * (k <= limit ? fLog2Harmonic[k] : 0.0f);
            power += p;
        }
        return power > 0.0f ? std::exp2(weighted / power) : 1.0f;
    }

    // sin(n * 2pi * phase) for n = 0..count via the Chebyshev recurrence
    static void fillSines(float* out, double phase, int count) noexcept
    {
        const double theta = 2.0 * M_PI * (phase - std::floor(phase));
        const float c1 = (float)std::cos(theta);
        out[0] = 0.0f;
        out[1] = (float)std::sin(theta);
        for (int n = 2; n <= count; ++n)
            out[n] = 2.0f * c1 * out[n - 1] - out[n - 2];
    }

    // the whole lead tone (fundamental + body + flageolets) at mult x f
    float tone(int mult, int maxPartial) const noexcept
    {
        const int limit = maxPartial / mult;
        if (limit < 1)
            return 0.0f;

        // body: 1/n series under the fundamental, scaled with it so it fades
        // with it under LOW-mode flageolets
        const float bodyLevel = kBodyMax * fParams.body;
        float body = 0.0f;
        for (int n = 2; n <= limit; ++n)
            body += (fSinBodyUp[n * mult] + fSinBodyDown[n * mult]) * fBodyCoef[n];
        float base = fSin[mult] + 0.5f * bodyLevel * body;

        // unison copies: each its own detuned fundamental + body (their own
        // detune already makes them move, so no extra body pair per copy),
        // power-normalised so VOICES changes thickness, not level
        if (fVoices > 1)
        {
            for (int j = 1; j < fVoices; ++j)
            {
                const float* sv = fSinUnison[j];
                float copy = sv[mult];
                for (int n = 2; n <= limit; ++n)
                    copy += bodyLevel * fBodyCoef[n] * sv[n * mult];
                base += copy;
            }
            base *= 1.0f / std::sqrt((float)fVoices);
        }
        float out = fGains[1] * fBaseDuck * base;

        for (int k = 2; k <= kMaxHarmonic; ++k)
        {
            if (fGains[k] < 1e-4f)
                continue;
            // a flageolet above the band (high notes) would just vanish and
            // leave the suppressed fundamental alone - give its level to the
            // fundamental instead, so the note doesn't drop out
            if (k > limit)
            {
                out += fGains[k] * fSin[mult];
                continue;
            }
            float flag = 0.0f;
            const int mMax = fParams.flagShape > 1e-4f ? limit / k : 1; // pure sine: just the harmonic itself
            for (int m = 1; m <= mMax; ++m)
                flag += fSin[m * k * mult] * fFlagCoef[m];
            out += fGains[k] * flag;
        }
        return out;
    }

    float onePole(float seconds) const noexcept
    {
        return 1.0f - std::exp(-1.0f / (seconds * fSampleRate));
    }

    void updateHarmonicGains() noexcept
    {
        // the signed position is slewed (Bloom), so a flip from one side to
        // the other passes back through the plain tone instead of jumping
        fHarmSlewed += (bipolar(fHarm) - fHarmSlewed) * fBloomCoeff;
        const float amount = std::fabs(fHarmSlewed);
        const int flageolet = std::clamp(fHarmSlewed >= 0.0f ? fParams.upHarmonic : fParams.downHarmonic, 2, kMaxHarmonic);

        for (int k = 1; k <= kMaxHarmonic; ++k)
            fGains[k] = 0.0f;
        fGains[1] = std::max(std::cos(amount * (float)M_PI_2), fParams.baseUnderHarm);
        fGains[flageolet] = std::sin(amount * (float)M_PI_2);
    }

    // log(cosh(u)) without overflow: |u| + log1p(exp(-2|u|)) - log 2
    static double logCosh(double u) noexcept
    {
        const double a = std::fabs(u);
        return a + std::log1p(std::exp(-2.0 * a)) - M_LN2;
    }

    // first-order antiderivative anti-aliasing of f(x) = tanh(d*(x+b)) - tanh(d*b),
    // normalised so a full-scale input lands near +-1 regardless of drive.
    // Double precision on purpose: F is O(10) while dx can be O(1e-4), so in
    // float the difference quotient is mostly rounding noise.
    float shapeAdaa(float xIn, float dIn, float bIn) noexcept
    {
        const double x = xIn, d = dIn, b = bIn;
        const double F = logCosh(d * (x + b)) / d;
        const double dx = x - fShapeX1;
        double y;
        if (std::fabs(dx) > 1e-6)
            y = (F - fShapeF1) / dx;
        else
            y = std::tanh(d * (0.5 * (x + fShapeX1) + b));
        fShapeX1 = x;
        fShapeF1 = F;
        return (float)((y - std::tanh(d * b)) / std::tanh(d));
    }

    LeadParams fParams;
    ADSR fEnv;
    Biquad fCabBell, fCabLp1, fCabLp2;

    float fSampleRate = 44100.0f;
    float fPartialLimitHz = 8000.0f;
    double fPhase4 = 0.0;
    double fBodyPhaseUp = 0.0, fBodyPhaseDown = 0.37; // offset so the two body copies don't start in phase
    float fSin[kMaxPartial + 1] = {};
    float fSinBodyUp[kMaxPartial + 1] = {};
    float fSinBodyDown[kMaxPartial + 1] = {};
    float fSinUnison[kMaxVoices][kMaxPartial + 1] = {};

    // unison state; [0] unused (the main copy runs on fPhase4)
    int fVoices = 1;
    double fUnisonRatio[kMaxVoices] = {};
    double fUnisonPhase[kMaxVoices] = { 0.0, 0.13, 0.71, 0.29, 0.53, 0.89, 0.41 }; // spread start phases
    float fSinQuarter[2 * kSubHarmonics + 1] = {};

    // waveform tables, rebuilt in applyParams()
    float fBodyCoef[kMaxPartial + 1] = {};
    float fFlagCoef[kMaxPartial + 1] = {};
    float fSubCoef[kSubHarmonics + 1] = {};
    double fBodyRatio = 1.004052; // 2^(7/1200)

    int fNote = -1;
    float fTargetNote = 60.0f, fCurrentNote = 60.0f;
    float fGlideCoeff = 1.0f;

    float fBend = 0.0f, fBendTarget = 0.0f, fBendCoeff = 1.0f;
    float fHarm = 0.5f, fHarmTarget = 0.5f, fHarmCoeff = 1.0f; // 0.5 = centre = plain tone
    float fSub = 0.5f, fSubTarget = 0.5f, fSubCoeff = 1.0f; // 0.5 = centre = no sub/octave
    float fBaseDuck = 1.0f; // base level from the SUB controller (BASE UNDER SUB)

    float fGains[kMaxHarmonic + 1] = { 0.0f, 1.0f };
    const float fLog2Harmonic[kMaxHarmonic + 1] = { 0.0f, 0.0f, 1.0f, 1.5849625f, 2.0f, 2.3219281f, 2.5849625f, 2.8073549f, 3.0f };
    float fBloomCoeff = 1.0f;
    float fHarmSlewed = 0.0f; // bipolar harmonics position after the Bloom slew, -1..+1

    double fShapeX1 = 0.0, fShapeF1 = 0.0;
    float fCabComp = 1.0f;
    int fCabCompCountdown = 0;
    float fDcX1 = 0.0f, fDcY1 = 0.0f, fDcCoeff = 0.999f;
};

} // namespace jordanus
