/*
 * Jordanus - "guitar-like" solo lead: flageolet harmonics on one strip /
 * joystick axis, sub / octave on the other, and a wide pitch bend. Velocity
 * and aftertouch are intentionally ignored - the whole expression lives on
 * the controllers. See dsp/LeadVoice.hpp for the sound engine.
 *
 * Voice Mode: MONO plays one LeadVoice through the note stack from Sideous'
 * mono mode (releasing the current key falls back to the previous
 * still-held one, legato, without re-attacking; Glide applies). POLY spreads
 * notes over kPolyVoices voices (free -> releasing -> oldest), each a full
 * LeadVoice with its own shaper and cabinet, so a single note sounds
 * exactly as in mono and chords stay clean. The controllers and pitch bend
 * always drive every voice.
 */

#include "DistrhoPlugin.hpp"
#include "Params.hpp"
#include "dsp/LeadVoice.hpp"

#include <array>
#include <cmath>

START_NAMESPACE_DISTRHO

using namespace jordanus;

// -----------------------------------------------------------------------------------------------------------

static constexpr const uint32_t kMaxHeldNotes = 16;
static constexpr const uint32_t kPolyVoices = 8;

class JordanusPlugin : public Plugin
{
public:
    JordanusPlugin()
        : Plugin(kParamCount, 0, 0)
    {
        sampleRateChanged(getSampleRate());

        // Params.hpp's table is the single source of truth for defaults
        // (shared with the UI) - push it through the real setter so the two
        // can't drift apart
        for (uint32_t i = 0; i < kParamCount; ++i)
            if (!isOutputParam(i))
                setParameterValue(i, paramToHost(i, getParamInfo(i).def));
    }

protected:
    // ---------------------------------------------------------------------
    // Information

    const char* getLabel() const override { return "Jordanus"; }
    const char* getDescription() const override
    {
        return "Guitar-like solo lead, mono (legato) or 8-voice poly. One strip / joystick axis "
               "swells a flageolet in over the plain tone (a different harmonic up and down), the "
               "other brings in a sub below or an octave above, and a wide pitch bend does the "
               "rest. Velocity and aftertouch are deliberately ignored.";
    }
    const char* getMaker() const override { return "Jordanus"; }
    const char* getHomePage() const override { return DISTRHO_PLUGIN_URI; }
    const char* getLicense() const override { return "ISC"; }
    uint32_t getVersion() const override { return d_version(0, 1, 0); }

    // ---------------------------------------------------------------------
    // Init

    void initParameter(uint32_t index, Parameter& parameter) override
    {
        const ParamInfo& info = getParamInfo(index);

        if (isOutputParam(index))
        {
            parameter.hints = kParameterIsOutput;
        }
        else if (index == kParamLearn)
        {
            // UI-only handshake: hidden from host generic UIs, never automated
            parameter.hints = kParameterIsInteger | kParameterIsHidden;
        }
        else
        {
            parameter.hints = kParameterIsAutomatable;
            if (info.shape == ParamShape::Integer)
                parameter.hints |= kParameterIsInteger;
        }

        if (index == kParamVoiceMode)
        {
            parameter.enumValues.count = 2;
            parameter.enumValues.restrictedMode = true;
            ParameterEnumerationValue* const values = new ParameterEnumerationValue[2];
            values[0].value = 0.0f;
            values[0].label = "Mono";
            values[1].value = 1.0f;
            values[1].label = "Poly";
            parameter.enumValues.values = values;
        }

        parameter.name = info.name;
        parameter.symbol = info.symbol;
        parameter.unit = info.unit;
        parameter.ranges.min = info.min;
        parameter.ranges.max = info.max;
        parameter.ranges.def = info.def;

        // log-curved parameters reach the host as a 0..100 % position (see
        // paramToHost() in Params.hpp), so its automation sweeps them evenly
        if (isHostPosition(index))
        {
            parameter.unit = "%";
            parameter.ranges.min = 0.0f;
            parameter.ranges.max = 100.0f;
            parameter.ranges.def = paramToHost(index, info.def);
        }
    }

    // ---------------------------------------------------------------------
    // Internal data

    float getParameterValue(uint32_t index) const override
    {
        return paramToHost(index, getEngineeringValue(index));
    }

    // a parameter in real units (Hz, s, ...), as the DSP sees it
    float getEngineeringValue(uint32_t index) const
    {
        switch (index)
        {
        case kParamBloom:     return fParams.bloom;
        case kParamHarmCC:    return (float)fHarmCC;
        case kParamUpHarm:    return (float)fParams.upHarmonic;
        case kParamDownHarm:  return (float)fParams.downHarmonic;
        case kParamBaseUnderHarm: return fParams.baseUnderHarm;
        case kParamBaseUnderSub:  return fParams.baseUnderSub;
        case kParamBaseSub:   return fParams.baseSub;
        case kParamSubLevel:  return fParams.subLevel;
        case kParamSubDeep:   return fParams.subDeep;
        case kParamSubMorph:  return fParams.subMorph;
        case kParamSubCC:     return (float)fSubCC;
        case kParamDrive:     return fParams.drive;
        case kParamWave:      return fWave;
        case kParamTilt:      return fParams.tilt;
        case kParamDetune:    return fParams.detune;
        case kParamFlagShape: return fParams.flagShape;
        case kParamSubShape:  return fParams.subShape;
        case kParamVoices:    return (float)fParams.voices;
        case kParamSpread:    return fParams.spread;
        case kParamAsym:      return fParams.asym;
        case kParamCab:       return fParams.cabHz;
        case kParamBendRange: return fBendRange;
        case kParamGlide:     return fParams.glide;
        case kParamVoiceMode: return fPoly ? 1.0f : 0.0f;
        case kParamAttack:    return fParams.attack;
        case kParamDecay:     return fParams.decay;
        case kParamSustain:   return fParams.sustain;
        case kParamRelease:   return fParams.release;
        case kParamVolume:    return fVolume;
        case kParamLearn:     return (float)fLearnTarget;
        case kParamHarmView:  return fHarmValue;
        case kParamSubView:   return fSubValue;
        case kParamLearnedCC: return (float)fLearnedCC;
        default:              return 0.0f;
        }
    }

    void setParameterValue(uint32_t index, float value) override
    {
        value = paramFromHost(index, value); // host position -> real units (Params.hpp)

        switch (index)
        {
        case kParamBloom:     fParams.bloom = value; break;
        case kParamHarmCC:    fHarmCC = (int)(value + 0.5f); return;
        case kParamUpHarm:    fParams.upHarmonic = (int)(value + 0.5f); break;
        case kParamDownHarm:  fParams.downHarmonic = (int)(value + 0.5f); break;
        case kParamBaseUnderHarm: fParams.baseUnderHarm = value; break;
        case kParamBaseUnderSub:  fParams.baseUnderSub = value; break;
        case kParamBaseSub:   fParams.baseSub = value; break;
        case kParamSubLevel:  fParams.subLevel = value; break;
        case kParamSubDeep:   fParams.subDeep = value; break;
        case kParamSubMorph:  fParams.subMorph = value; break;
        case kParamSubCC:     fSubCC = (int)(value + 0.5f); return;
        case kParamDrive:     fParams.drive = value; break;
        case kParamWave:      fWave = value; jordanus::LeadVoice::applyWave(fParams, value); break;
        case kParamTilt:      fParams.tilt = value; break;
        case kParamDetune:    fParams.detune = value; break;
        case kParamFlagShape: fParams.flagShape = value; break;
        case kParamSubShape:  fParams.subShape = value; break;
        case kParamVoices:    fParams.voices = (int)(value + 0.5f); break;
        case kParamSpread:    fParams.spread = value; break;
        case kParamAsym:      fParams.asym = value; break;
        case kParamCab:       fParams.cabHz = value; break;
        case kParamBendRange:
            fBendRange = value;
            for (jordanus::LeadVoice& v : fVoices)
                v.setPitchBend(fBendNormalized * fBendRange);
            return;
        case kParamVoiceMode:
        {
            const bool poly = value >= 0.5f;
            if (poly != fPoly)
            {
                // switching modes mid-note would strand voices / the note stack
                allNotesOff();
                fPoly = poly;
            }
            return;
        }
        case kParamGlide:     fParams.glide = value; break;
        case kParamAttack:    fParams.attack = value; break;
        case kParamDecay:     fParams.decay = value; break;
        case kParamSustain:   fParams.sustain = value; break;
        case kParamRelease:   fParams.release = value; break;
        case kParamVolume:    fVolume = value; return;
        case kParamLearn:
            fLearnTarget = (int)(value + 0.5f);
            if (fLearnTarget != 0)
                fLearnedCC = -1; // re-arm: the next learned CC must read as a change, even if it's the same number
            return;
        default:              return; // output meters: hosts must not write them
        }

        for (jordanus::LeadVoice& v : fVoices)
            v.applyParams(fParams);
    }

    // ---------------------------------------------------------------------
    // Audio/MIDI Processing

    void activate() override
    {
        sampleRateChanged(getSampleRate());
    }

    void sampleRateChanged(double newSampleRate) override
    {
        for (jordanus::LeadVoice& v : fVoices)
        {
            v.setSampleRate(newSampleRate);
            v.applyParams(fParams);
        }
    }

    void run(const float**, float** outputs, uint32_t frames,
             const MidiEvent* midiEvents, uint32_t midiEventCount) override
    {
        float* outL = outputs[0];
        float* outR = outputs[1];

        uint32_t nextEvent = 0;

        for (uint32_t frame = 0; frame < frames; ++frame)
        {
            while (nextEvent < midiEventCount && midiEvents[nextEvent].frame == frame)
                handleMidiEvent(midiEvents[nextEvent++]);

            // idle voices only run their controller smoothing - cheap
            float mix = 0.0f;
            for (jordanus::LeadVoice& v : fVoices)
                mix += v.process();

            const float out = safetyClip(mix * fVolume * kHeadroom * (fPoly ? kPolyGain : 1.0f));

            outL[frame] = out;
            outR[frame] = out;
        }

        while (nextEvent < midiEventCount)
            handleMidiEvent(midiEvents[nextEvent++]);
    }

private:
    static constexpr const float kHeadroom = 0.7f;
    // poly: a single note ~6dB under mono, so a 4-note chord lands near a
    // mono note's level instead of slamming the safety clip
    static constexpr const float kPolyGain = 0.5f;

    // last-resort guard: untouched below 0.8 (a mono note peaks ~0.72),
    // soft above, never past 1
    static float safetyClip(float x) noexcept
    {
        const float a = std::fabs(x);
        if (a <= 0.8f)
            return x;
        return std::copysign(0.8f + 0.2f * std::tanh((a - 0.8f) / 0.2f), x);
    }

    void handleMidiEvent(const MidiEvent& event) noexcept
    {
        if (event.size < 2 || event.size > 3)
            return;

        const uint8_t status = event.data[0] & 0xF0;
        const uint8_t data1 = event.data[1];
        const uint8_t data2 = event.size > 2 ? event.data[2] : 0;

        // velocity (data2 on note-on) is deliberately never read
        if (status == 0x90 && data2 > 0)
        {
            if (fPoly)
                polyNoteOn(data1);
            else
                noteOn(data1);
        }
        else if (status == 0x80 || (status == 0x90 && data2 == 0))
        {
            if (fPoly)
                polyNoteOff(data1);
            else
                noteOff(data1);
        }
        else if (status == 0xE0 && event.size >= 3) // pitch bend, 14-bit, center = 8192
        {
            const int raw = (int)data1 | ((int)data2 << 7);
            const float normalized = ((float)raw - 8192.0f) / 8192.0f;
            fBendNormalized = normalized < -1.0f ? -1.0f : (normalized > 1.0f ? 1.0f : normalized);
            for (jordanus::LeadVoice& v : fVoices)
                v.setPitchBend(fBendNormalized * fBendRange);
        }
        else if (status == 0xB0 && event.size >= 3)
        {
            // MIDI learn: bind the first regular CC (120+ are channel mode
            // messages) right here, so it works before the UI round trip
            if (fLearnTarget != 0 && data1 < 120)
            {
                (fLearnTarget == 1 ? fHarmCC : fSubCC) = data1;
                fLearnedCC = data1;
                fLearnTarget = 0;
            }

            // checked independently, so both gestures can share one CC if wanted
            if (data1 == fHarmCC)
            {
                fHarmValue = (float)data2 / 127.0f;
                for (jordanus::LeadVoice& v : fVoices)
                    v.setHarmonics(fHarmValue);
            }
            if (data1 == fSubCC)
            {
                fSubValue = (float)data2 / 127.0f;
                for (jordanus::LeadVoice& v : fVoices)
                    v.setSub(fSubValue);
            }
            if (data1 == 120 || data1 == 123) // all sound off / all notes off
                allNotesOff();
        }
    }

    void allNotesOff() noexcept
    {
        fHeldCount = 0;
        for (jordanus::LeadVoice& v : fVoices)
            v.kill();
    }

    // ---------------------------------------------------------------------
    // poly allocation: a voice already on this note re-attacks; else the
    // first idle voice, else the oldest releasing one, else the oldest of all

    void polyNoteOn(int note) noexcept
    {
        int target = -1;
        for (uint32_t i = 0; i < kPolyVoices && target < 0; ++i)
            if (fVoices[i].isActive() && fVoices[i].getNote() == note)
                target = (int)i;
        for (uint32_t i = 0; i < kPolyVoices && target < 0; ++i)
            if (!fVoices[i].isActive())
                target = (int)i;
        if (target < 0)
            target = oldestVoice(true);
        if (target < 0)
            target = oldestVoice(false);

        fVoiceAge[target] = ++fAgeCounter;
        fVoices[target].noteOn(note, true);
    }

    void polyNoteOff(int note) noexcept
    {
        for (jordanus::LeadVoice& v : fVoices)
            if (v.isActive() && !v.isReleasing() && v.getNote() == note)
                v.noteOff();
    }

    int oldestVoice(bool releasingOnly) const noexcept
    {
        int oldest = -1;
        for (uint32_t i = 0; i < kPolyVoices; ++i)
        {
            if (releasingOnly && !fVoices[i].isReleasing())
                continue;
            if (oldest < 0 || fVoiceAge[i] < fVoiceAge[oldest])
                oldest = (int)i;
        }
        return oldest;
    }

    // ---------------------------------------------------------------------
    // mono note stack (last-note priority). Retriggering is decided from the
    // stack going 0 -> 1 held keys, not from whether the voice still rings,
    // so a fresh staccato note always re-attacks even during a long release.

    void noteOn(int note) noexcept
    {
        removeHeld(note);
        const bool retrigger = fHeldCount == 0;
        if (fHeldCount < kMaxHeldNotes)
            fHeld[fHeldCount++] = note;
        fVoices[0].noteOn(note, retrigger);
    }

    void noteOff(int note) noexcept
    {
        const bool wasCurrent = fHeldCount > 0 && fHeld[fHeldCount - 1] == note;
        removeHeld(note);

        if (!wasCurrent)
            return;
        if (fHeldCount > 0)
            fVoices[0].noteOn(fHeld[fHeldCount - 1], false); // legato fall-back to the previous key
        else
            fVoices[0].noteOff();
    }

    void removeHeld(int note) noexcept
    {
        for (uint32_t i = 0; i < fHeldCount; ++i)
        {
            if (fHeld[i] == note)
            {
                for (uint32_t j = i + 1; j < fHeldCount; ++j)
                    fHeld[j - 1] = fHeld[j];
                --fHeldCount;
                return;
            }
        }
    }

    std::array<jordanus::LeadVoice, kPolyVoices> fVoices; // mono mode plays fVoices[0] only
    jordanus::LeadParams fParams;
    bool fPoly = false;
    uint32_t fVoiceAge[kPolyVoices] = {}; // note-on order, for stealing the oldest
    uint32_t fAgeCounter = 0;

    int fHarmCC = 1;
    int fSubCC = 2;
    int fLearnTarget = 0; // 0 = off, 1 = harmonics, 2 = sub (see kParamLearn)
    int fLearnedCC = -1;
    float fBendRange = 12.0f;
    float fVolume = 0.8f;
    float fWave = 0.1f; // the knob value; LeadParams holds what it maps to (body + shape)

    float fBendNormalized = 0.0f;
    float fHarmValue = 0.5f; // last received controller values, echoed to the UI meters
    float fSubValue = 0.5f; // bipolar controller, centre = nothing

    int fHeld[kMaxHeldNotes] = {};
    uint32_t fHeldCount = 0;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JordanusPlugin)
};

// -----------------------------------------------------------------------------------------------------------

Plugin* createPlugin()
{
    return new JordanusPlugin();
}

// -----------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
