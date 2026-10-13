/*
 * Jordanus - monophonic "guitar-like" solo lead: flageolet harmonics
 * on the mod wheel / ribbon, a clean sub low end on a second controller, and
 * a wide pitch bend. Velocity and aftertouch are intentionally ignored - the
 * whole expression lives on the controllers. See dsp/LeadVoice.hpp for the
 * sound engine.
 *
 * Structure mirrors the other Sideous plugins (same initParameter/
 * getParameterValue/setParameterValue/run() shape), with the mono note
 * stack from Sideous' mono mode: releasing the current key falls back to
 * the previous still-held one, legato, without re-attacking.
 */

#include "DistrhoPlugin.hpp"
#include "Params.hpp"
#include "dsp/LeadVoice.hpp"

#include <cmath>

START_NAMESPACE_DISTRHO

using namespace jordanus;

// -----------------------------------------------------------------------------------------------------------

static constexpr const uint32_t kMaxHeldNotes = 16;

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
                setParameterValue(i, getParamInfo(i).def);
    }

protected:
    // ---------------------------------------------------------------------
    // Information

    const char* getLabel() const override { return "Jordanus"; }
    const char* getDescription() const override
    {
        return "Monophonic guitar-like solo lead. The mod wheel / ribbon walks a ladder of "
               "flageolet harmonics (LOW or HIGH set) that bloom in like guitar feedback, a "
               "second controller blends in a clean sub -1/-2 octave low end, and a wide pitch "
               "bend does the rest. Velocity and aftertouch are deliberately ignored.";
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
            if (info.shape == ParamShape::Logarithmic)
                parameter.hints |= kParameterIsLogarithmic;
            if (info.shape == ParamShape::Integer)
                parameter.hints |= kParameterIsInteger;
        }

        parameter.name = info.name;
        parameter.symbol = info.symbol;
        parameter.unit = info.unit;
        parameter.ranges.min = info.min;
        parameter.ranges.max = info.max;
        parameter.ranges.def = info.def;
    }

    // ---------------------------------------------------------------------
    // Internal data

    float getParameterValue(uint32_t index) const override
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
        case kParamBendRange: fBendRange = value; fVoice.setPitchBend(fBendNormalized * fBendRange); return;
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

        fVoice.applyParams(fParams);
    }

    // ---------------------------------------------------------------------
    // Audio/MIDI Processing

    void activate() override
    {
        sampleRateChanged(getSampleRate());
    }

    void sampleRateChanged(double newSampleRate) override
    {
        fVoice.setSampleRate(newSampleRate);
        fVoice.applyParams(fParams);
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

            float out = fVoice.process() * fVolume * kHeadroom;
            out = out < -1.0f ? -1.0f : (out > 1.0f ? 1.0f : out); // last-resort guard only

            outL[frame] = out;
            outR[frame] = out;
        }

        while (nextEvent < midiEventCount)
            handleMidiEvent(midiEvents[nextEvent++]);
    }

private:
    static constexpr const float kHeadroom = 0.7f;

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
            noteOn(data1);
        }
        else if (status == 0x80 || (status == 0x90 && data2 == 0))
        {
            noteOff(data1);
        }
        else if (status == 0xE0 && event.size >= 3) // pitch bend, 14-bit, center = 8192
        {
            const int raw = (int)data1 | ((int)data2 << 7);
            const float normalized = ((float)raw - 8192.0f) / 8192.0f;
            fBendNormalized = normalized < -1.0f ? -1.0f : (normalized > 1.0f ? 1.0f : normalized);
            fVoice.setPitchBend(fBendNormalized * fBendRange);
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
                fVoice.setHarmonics(fHarmValue);
            }
            if (data1 == fSubCC)
            {
                fSubValue = (float)data2 / 127.0f;
                fVoice.setSub(fSubValue);
            }
            if (data1 == 120 || data1 == 123) // all sound off / all notes off
            {
                fHeldCount = 0;
                fVoice.kill();
            }
        }
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
        fVoice.noteOn(note, retrigger);
    }

    void noteOff(int note) noexcept
    {
        const bool wasCurrent = fHeldCount > 0 && fHeld[fHeldCount - 1] == note;
        removeHeld(note);

        if (!wasCurrent)
            return;
        if (fHeldCount > 0)
            fVoice.noteOn(fHeld[fHeldCount - 1], false); // legato fall-back to the previous key
        else
            fVoice.noteOff();
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

    jordanus::LeadVoice fVoice;
    jordanus::LeadParams fParams;

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
