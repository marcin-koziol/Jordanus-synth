/*
 * Jordanus - custom Cairo UI. Drawing/layout logic lives in
 * ui/UIPainter.hpp (shared with tools/offline_render.cpp); this file wires
 * mouse/keyboard input to parameter changes, plus the preset bar's CRUD - a
 * small file-based preset library (ui/PresetStore.hpp), entirely UI-side.
 * Same structure as the other Sideous plugin UIs, plus the LOW/HIGH
 * harmonics switch and the LCD's live controller meters (output params).
 */

#include "DistrhoUI.hpp"
#include "Params.hpp"
#include "ui/FactoryPresets.hpp"
#include "ui/PresetStore.hpp"
#include "ui/UIPainter.hpp"

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

START_NAMESPACE_DISTRHO

using DGL_NAMESPACE::CairoGraphicsContext;
using namespace jordanus;

// -----------------------------------------------------------------------------------------------------------

class JordanusUI : public UI
{
public:
    JordanusUI()
        : UI()
    {
        fLayout = ui::buildLayout((float)getWidth());

        for (uint32_t i = 0; i < kParamCount; ++i)
            fValues[i] = getParamInfo(i).def;

        fPresetNames = ui::listPresets();
        if (fPresetNames.empty())
        {
            ui::seedFactoryPresets();
            fPresetNames = ui::listPresets();
        }
    }

protected:
    // ---------------------------------------------------------------------

    void parameterChanged(uint32_t index, float value) override
    {
        if (index >= kParamCount)
            return;

        // MIDI learn result: the DSP already bound the CC; store it in the
        // real CC param too, so the host's state and presets carry it
        if (index == kParamLearnedCC)
        {
            fValues[index] = value;
            if (value >= 0.0f && fLearnTarget != 0)
                finishLearn((int)(value + 0.5f));
            return;
        }

        // the DSP disarms on its own when it learns; nothing to track here
        if (index == kParamLearn)
            return;

        // live controller meters for the LCD: no value popup, not part of
        // any preset, so they must never mark a preset dirty
        if (isOutputParam(index))
        {
            if (value != fValues[index])
            {
                fValues[index] = value;
                repaint();
            }
            return;
        }

        if (value != fValues[index])
        {
            fValueDisplayUntil[index] = std::chrono::steady_clock::now() + std::chrono::milliseconds(1200);
            fHasActiveValueDisplay = true;

            // a param moved for a reason other than us applying a preset ->
            // the currently displayed preset (if any) no longer matches
            // what's actually dialed in
            if (!fLoadingPreset)
                fDirty = true;
        }

        fValues[index] = value;
        repaint();
    }

    void uiIdle() override
    {
        // blink the armed LEARN LED / LCD prompt
        if (fLearnTarget != 0)
        {
            const bool blinkOn = learnBlinkOn();
            if (blinkOn != fLastBlinkOn)
            {
                fLastBlinkOn = blinkOn;
                repaint();
            }
        }

        if (!fHasActiveValueDisplay)
            return;

        const auto now = std::chrono::steady_clock::now();
        uint32_t activeCount = 0;
        for (uint32_t i = 0; i < kParamCount; ++i)
            if (now < fValueDisplayUntil[i])
                ++activeCount;

        if (activeCount < fLastActiveValueDisplayCount)
            repaint();

        fLastActiveValueDisplayCount = activeCount;
        fHasActiveValueDisplay = activeCount > 0;
    }

    void onCairoDisplay(const CairoGraphicsContext& context) override
    {
        const auto now = std::chrono::steady_clock::now();
        bool autoShow[kParamCount];
        for (uint32_t i = 0; i < kParamCount; ++i)
            autoShow[i] = now < fValueDisplayUntil[i];

        ui::PaintState state;
        state.values = fValues;
        state.hoverKnob = fHoverKnob;
        state.dragKnob = fDragKnob;
        state.hoverLearn = fHoverLearn;
        state.learnTarget = fLearnTarget;
        state.learnBlinkOn = learnBlinkOn();
        state.autoShowValue = autoShow;

        state.presetName = fCurrentName.c_str();
        state.presetIsUnsaved = fDirty;
        state.presetEditingName = fEditingName;
        state.presetEditBuffer = fEditBuffer.c_str();
        state.hoverPresetButton = fHoverPresetButton;
        state.presetDeleteEnabled = fPresetIndex >= 0;

        ui::paint(context.handle, fLayout, state);
    }

    // ---------------------------------------------------------------------
    // input

    bool onMouse(const MouseEvent& ev) override
    {
        if (ev.button != 1)
            return false;

        const double mx = ev.pos.getX();
        const double my = ev.pos.getY();

        if (ev.press)
        {
            const int presetBtn = hitTestPresetButton(mx, my);
            if (presetBtn >= 0)
            {
                // SAVE must see fEditingName/fEditBuffer as-is - it commits the
                // in-progress rename itself (and clears fEditingName when done).
                // Clearing it here first, like the other buttons do to cancel a
                // pending rename, made SAVE silently discard the typed name and
                // fall back to whatever fCurrentName already was.
                switch (presetBtn)
                {
                case 0: fEditingName = false; presetStep(-1); break;
                case 1: fEditingName = false; presetStep(1);  break;
                case 2: presetSave();   break;
                case 3: fEditingName = false; presetDelete(); break;
                default: fEditingName = false; break;
                }
                repaint();
                return true;
            }

            if (hitTestPresetName(mx, my))
            {
                fEditingName = true;
                fEditBuffer = fCurrentName;
                repaint();
                return true;
            }

            if (fEditingName)
            {
                // clicked elsewhere while editing - cancel without applying,
                // then fall through so the click still does whatever it was
                // actually pointing at (a knob, etc)
                fEditingName = false;
                repaint();
            }

            const int learnIdx = hitTestLearn(mx, my);
            if (learnIdx >= 0)
            {
                const int target = fLayout.learnButtons[(size_t)learnIdx].target;
                setLearn(fLearnTarget == target ? 0 : target); // second click cancels
                return true;
            }

            const int knobIdx = hitTestKnob(mx, my);
            if (knobIdx < 0)
                return false;

            const ui::Knob& k = fLayout.knobs[knobIdx];

            const auto now = std::chrono::steady_clock::now();
            const bool doubleClick = fLastClickKnob == knobIdx &&
                std::chrono::duration_cast<std::chrono::milliseconds>(now - fLastClickTime).count() < 350;
            fLastClickKnob = knobIdx;
            fLastClickTime = now;

            if (doubleClick)
            {
                const float def = getParamInfo(k.param).def;
                editParameter(k.param, true);
                setParameterValue(k.param, def);
                editParameter(k.param, false);
                fValues[k.param] = def;
                fDragKnob = -1;
                repaint();
                return true;
            }

            fDragKnob = knobIdx;
            fDragStartY = my;
            fDragStartT = ui::paramToNormalized(k.param, fValues[k.param]);
            editParameter(k.param, true);
            return true;
        }

        if (fDragKnob >= 0)
        {
            editParameter(fLayout.knobs[fDragKnob].param, false);
            fDragKnob = -1;
            repaint();
            return true;
        }

        return false;
    }

    bool onMotion(const MotionEvent& ev) override
    {
        const double mx = ev.pos.getX();
        const double my = ev.pos.getY();

        if (fDragKnob >= 0)
        {
            const ui::Knob& k = fLayout.knobs[fDragKnob];
            const double dy = fDragStartY - my;
            float t = fDragStartT + (float)(dy / 200.0);
            t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);

            const float value = ui::normalizedToParam(k.param, t);
            fValues[k.param] = value;
            setParameterValue(k.param, value);
            repaint();
            return true;
        }

        const int newHoverKnob = hitTestKnob(mx, my);
        const int newHoverPresetButton = hitTestPresetButton(mx, my);
        const int newHoverLearn = hitTestLearn(mx, my);
        if (newHoverKnob != fHoverKnob || newHoverPresetButton != fHoverPresetButton ||
            newHoverLearn != fHoverLearn)
        {
            fHoverLearn = newHoverLearn;
            fHoverKnob = newHoverKnob;
            fHoverPresetButton = newHoverPresetButton;
            repaint();
        }

        return false;
    }

    bool onScroll(const ScrollEvent& ev) override
    {
        const int knobIdx = hitTestKnob(ev.pos.getX(), ev.pos.getY());
        if (knobIdx < 0)
            return false;

        const ui::Knob& k = fLayout.knobs[knobIdx];
        const float t = ui::paramToNormalized(k.param, fValues[k.param]);
        const float step = 0.02f;
        float newT = t + (ev.delta.getY() > 0.0 ? step : -step);
        newT = newT < 0.0f ? 0.0f : (newT > 1.0f ? 1.0f : newT);

        const float value = ui::normalizedToParam(k.param, newT);
        editParameter(k.param, true);
        setParameterValue(k.param, value);
        editParameter(k.param, false);
        fValues[k.param] = value;
        repaint();
        return true;
    }

    // low-level key events: only used for the few control keys that matter
    // while renaming a preset (Enter/Escape/Backspace); actual typed
    // characters come through onCharacterInput() below, since KeyboardEvent
    // is explicitly documented as not safe to interpret as text input
    // (wrong for anything beyond plain ASCII, doesn't handle IME, etc).
    bool onKeyboard(const KeyboardEvent& ev) override
    {
        if (!fEditingName)
            return false;
        if (!ev.press)
            return true; // still swallow releases while editing so nothing else reacts to them

        if (ev.key == kKeyEnter || ev.key == kKeyPadEnter)
        {
            fCurrentName = ui::sanitizePresetName(fEditBuffer);
            fEditingName = false;
            repaint();
            return true;
        }
        if (ev.key == kKeyEscape)
        {
            fEditingName = false;
            repaint();
            return true;
        }
        if (ev.key == kKeyBackspace || ev.key == kKeyDelete)
        {
            if (!fEditBuffer.empty())
                fEditBuffer.pop_back();
            repaint();
            return true;
        }

        return true; // swallow everything else while editing (e.g. space) rather than let it leak through
    }

    bool onCharacterInput(const CharacterInputEvent& ev) override
    {
        if (!fEditingName)
            return false;

        if (ev.character >= 32 && fEditBuffer.size() < 32) // skip control chars, cap length
            fEditBuffer += ev.string;

        repaint();
        return true;
    }

private:
    int hitTestKnob(double x, double y) const noexcept
    {
        for (size_t i = 0; i < fLayout.knobs.size(); ++i)
        {
            const ui::Knob& k = fLayout.knobs[i];
            const double dx = x - k.cx, dy = y - k.cy;
            const double reach = k.radius + 6.0;
            if (dx * dx + dy * dy <= reach * reach)
                return (int)i;
        }
        return -1;
    }

    int hitTestLearn(double x, double y) const noexcept
    {
        for (size_t i = 0; i < fLayout.learnButtons.size(); ++i)
        {
            const ui::LearnButton& b = fLayout.learnButtons[i];
            if (x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h)
                return (int)i;
        }
        return -1;
    }

    // ---------------------------------------------------------------------
    // MIDI learn (see kParamLearn in Params.hpp for the handshake)

    void setLearn(int target) noexcept
    {
        fLearnTarget = target;
        fLastBlinkOn = learnBlinkOn();
        setParameterValue(kParamLearn, (float)target);
        repaint();
    }

    void finishLearn(int cc) noexcept
    {
        const uint32_t param = fLearnTarget == 1 ? kParamHarmCC : kParamSubCC;

        // the DSP already disarmed itself, but the host still holds the armed
        // value we sent - clear it, or a saved project would reload armed
        fLearnTarget = 0;
        setParameterValue(kParamLearn, 0.0f);

        editParameter(param, true);
        setParameterValue(param, (float)cc);
        editParameter(param, false);

        if (fValues[param] != (float)cc)
            fDirty = true;
        fValues[param] = (float)cc;
        fValueDisplayUntil[param] = std::chrono::steady_clock::now() + std::chrono::milliseconds(1200);
        fHasActiveValueDisplay = true;
        repaint();
    }

    static bool learnBlinkOn() noexcept
    {
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        return (ms / 250) % 2 == 0;
    }

    static bool insideButton(const ui::Button& b, double x, double y) noexcept
    {
        return x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h;
    }

    // 0=prev, 1=next, 2=save, 3=delete, or -1. Delete only hit-tests
    // positive when a real preset (not INIT) is currently loaded - nothing
    // to delete otherwise.
    int hitTestPresetButton(double x, double y) const noexcept
    {
        const ui::PresetBarLayout& pb = fLayout.presetBar;
        if (insideButton(pb.prev, x, y)) return 0;
        if (insideButton(pb.next, x, y)) return 1;
        if (insideButton(pb.save, x, y)) return 2;
        if (insideButton(pb.del, x, y) && fPresetIndex >= 0) return 3;
        return -1;
    }

    bool hitTestPresetName(double x, double y) const noexcept
    {
        const ui::PresetBarLayout& pb = fLayout.presetBar;
        return x >= pb.nameX && x <= pb.nameX + pb.nameW && y >= pb.nameY && y <= pb.nameY + pb.nameH;
    }

    // ---------------------------------------------------------------------
    // preset CRUD - index < 0 means INIT (compiled-in defaults, no file)

    void loadPresetAt(int index) noexcept
    {
        fLoadingPreset = true;

        if (index < 0 || index >= (int)fPresetNames.size())
        {
            for (uint32_t i = 0; i < kParamCount; ++i)
                if (isPresetParam(i))
                    fValues[i] = getParamInfo(i).def;
            fCurrentName = "INIT";
            fPresetIndex = -1;
        }
        else
        {
            ui::loadPreset(fPresetNames[(size_t)index], fValues);
            fCurrentName = fPresetNames[(size_t)index];
            fPresetIndex = index;
        }

        // only real settings get pushed: output meters and the learn
        // handshake aren't part of any preset
        for (uint32_t i = 0; i < kParamCount; ++i)
        {
            if (!isPresetParam(i))
                continue;
            editParameter(i, true);
            setParameterValue(i, fValues[i]);
            editParameter(i, false);
        }

        fDirty = false;
        fLoadingPreset = false;
        repaint();
    }

    // cyclical Prev(-1)/Next(+1) through [INIT, preset0, preset1, ...]
    void presetStep(int dir) noexcept
    {
        const int total = (int)fPresetNames.size() + 1;
        int slot = fPresetIndex + 1; // 0 = INIT, 1..N = fPresetNames[0..N-1]
        slot = ((slot + dir) % total + total) % total;
        loadPresetAt(slot - 1);
    }

    void presetSave() noexcept
    {
        std::string name = fEditingName ? ui::sanitizePresetName(fEditBuffer) : fCurrentName;
        if (name.empty() || name == "INIT")
            name = "New Preset";

        ui::savePreset(name, fValues);
        fPresetNames = ui::listPresets();
        fCurrentName = name;
        fEditingName = false;
        fDirty = false;

        fPresetIndex = -1;
        for (size_t i = 0; i < fPresetNames.size(); ++i)
        {
            if (fPresetNames[i] == name)
            {
                fPresetIndex = (int)i;
                break;
            }
        }
    }

    void presetDelete() noexcept
    {
        if (fPresetIndex < 0)
            return;

        ui::deletePreset(fCurrentName);
        fPresetNames = ui::listPresets();

        const int newIndex = fPresetIndex < (int)fPresetNames.size() ? fPresetIndex : (int)fPresetNames.size() - 1;
        loadPresetAt(newIndex);
    }

    ui::Layout fLayout;
    float fValues[kParamCount];

    std::chrono::steady_clock::time_point fValueDisplayUntil[kParamCount]{};
    bool fHasActiveValueDisplay = false;
    uint32_t fLastActiveValueDisplayCount = 0;

    int fDragKnob = -1;
    double fDragStartY = 0.0;
    float fDragStartT = 0.0f;

    int fHoverKnob = -1;
    int fHoverLearn = -1;

    int fLearnTarget = 0;      // 0 = off, 1 = harmonics, 2 = sub - mirrors what we sent as kParamLearn
    bool fLastBlinkOn = false;

    int fLastClickKnob = -1;
    std::chrono::steady_clock::time_point fLastClickTime{};

    std::vector<std::string> fPresetNames; // sorted, ui::listPresets() order
    int fPresetIndex = -1;                 // -1 = INIT, else index into fPresetNames
    std::string fCurrentName = "INIT";
    bool fDirty = false;                   // true once a param has moved since the last load/save
    bool fLoadingPreset = false;           // guards parameterChanged() while loadPresetAt() is applying values
    bool fEditingName = false;
    std::string fEditBuffer;
    int fHoverPresetButton = -1;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(JordanusUI)
};

// -----------------------------------------------------------------------------------------------------------

UI* createUI()
{
    return new JordanusUI();
}

// -----------------------------------------------------------------------------------------------------------

END_NAMESPACE_DISTRHO
