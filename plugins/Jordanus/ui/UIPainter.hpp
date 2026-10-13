/*
 * Jordanus - GUI layout and Cairo painting, shared between the real
 * plugin UI and an offline PNG renderer (tools/offline_render.cpp), same
 * technique as the other Sideous plugins.
 *
 * Look: a technical, Korg-workstation-ish panel - charcoal chassis,
 * silk-screened section frames with their title cut into the top line,
 * black knobs with a white pointer and an orange value arc, rubber-style
 * buttons with LEDs, and a pale green-grey LCD showing the preset name plus
 * the live controller state (which flageolet the wheel is on, sub amount).
 */

#pragma once

#include <cairo.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <utility>
#include <vector>
#include <algorithm>

#include "../Params.hpp"
#include "../dsp/LeadVoice.hpp"

namespace jordanus {
namespace ui {

// -----------------------------------------------------------------------------------------------------------
// palette

struct Color { double r, g, b; };

static constexpr Color kChassis   { 0.149, 0.157, 0.169 }; // charcoal
static constexpr Color kSilk      { 0.847, 0.855, 0.863 }; // silk-screen labels/lines
static constexpr Color kSilkDim   { 0.541, 0.553, 0.569 };
static constexpr Color kKnobTop   { 0.204, 0.204, 0.212 };
static constexpr Color kKnobBot   { 0.059, 0.059, 0.063 };
static constexpr Color kOrange    { 0.941, 0.478, 0.102 }; // value arcs, brand accent
static constexpr Color kLedOn     { 1.000, 0.231, 0.184 };
static constexpr Color kLedOff    { 0.290, 0.110, 0.102 };
static constexpr Color kBtnFace   { 0.255, 0.263, 0.275 };
static constexpr Color kLcdBg     { 0.690, 0.761, 0.678 }; // pale green-grey
static constexpr Color kLcdInk    { 0.110, 0.157, 0.125 };
static constexpr Color kLcdInkDim { 0.110, 0.157, 0.125 };

inline void setColor(cairo_t* cr, Color c, double a = 1.0) noexcept
{
    cairo_set_source_rgba(cr, c.r, c.g, c.b, a);
}

// -----------------------------------------------------------------------------------------------------------
// layout model

struct Knob
{
    uint32_t param;
    float cx, cy, radius;
    const char* label;
};

struct PanelBox { float x, y, w, h; const char* title; };


// MIDI learn button, cut into a panel's top frame line (target: 1 = harmonics, 2 = sub)
struct LearnButton { int target; float x, y, w, h; };

// one segment of a radio-style switch bound to an integer parameter
struct SwitchButton { uint32_t param; float value; float x, y, w, h; const char* label; };

// a plain clickable button, not tied to a DPF parameter (preset bar)
struct Button { float x, y, w, h; const char* label; };

struct PresetBarLayout
{
    Button prev, next, save, del;
    float nameX, nameY, nameW, nameH; // the LCD's preset-name line (click to rename)
};

struct LcdBox { float x, y, w, h; };

struct Layout
{
    float width, height;
    LcdBox lcd;
    LcdBox scope; // waveform preview in the WAVEFORM panel
    std::vector<PanelBox> panels;
    std::vector<Knob> knobs;
    std::vector<LearnButton> learnButtons;
    std::vector<SwitchButton> switches;
    PresetBarLayout presetBar;
};

// -----------------------------------------------------------------------------------------------------------
// layout builder

inline void addKnobRow(std::vector<Knob>& knobs, float x, float w, float rowY, float radius,
                       std::initializer_list<std::pair<uint32_t, const char*>> items)
{
    const float slot = w / (float)items.size();
    size_t i = 0;
    for (const auto& item : items)
        knobs.push_back({ item.first, x + slot * ((float)i++ + 0.5f), rowY, radius, item.second });
}

inline Layout buildLayout(float width)
{
    Layout L;
    L.width = width;

    const float margin = 14.0f;
    const float gap = 12.0f;
    const float radius = 21.0f;
    const float panelH = 104.0f;
    const float knobY = 50.0f; // knob centre, relative to panel top

    // --- header ---
    const float headerH = 34.0f;

    // --- LCD flanked by stacked preset buttons: [<][>] LCD [SAVE][DEL] ---
    const float lcdY = margin + headerH + 8.0f;
    const float lcdH = 66.0f;
    const float btnW = 58.0f, btnH = (lcdH - 6.0f) / 2.0f;
    {
        PresetBarLayout& pb = L.presetBar;
        pb.prev = { margin, lcdY, btnW, btnH, "PREV" };
        pb.next = { margin, lcdY + btnH + 6.0f, btnW, btnH, "NEXT" };
        pb.save = { width - margin - btnW, lcdY, btnW, btnH, "WRITE" };
        pb.del  = { width - margin - btnW, lcdY + btnH + 6.0f, btnW, btnH, "DELETE" };

        L.lcd = { margin + btnW + 10.0f, lcdY, width - 2.0f * (margin + btnW + 10.0f), lcdH };
        pb.nameX = L.lcd.x + 8.0f;
        pb.nameY = L.lcd.y + 6.0f;
        pb.nameW = L.lcd.w - 16.0f;
        pb.nameH = 26.0f;
    }

    // --- row 1: HARMONICS | SUB ---
    const float row1Y = lcdY + lcdH + gap + 6.0f;
    const float halfW = (width - 2.0f * margin - gap) / 2.0f;
    const float subX = margin + halfW + gap;
    L.panels.push_back({ margin, row1Y, halfW, panelH, "HARMONICS" });
    L.panels.push_back({ subX,   row1Y, halfW, panelH, "SUB / OCTAVE" });
    {
        const float lw = 58.0f, lh = 16.0f;
        L.learnButtons.push_back({ 1, margin + halfW - 12.0f - lw, row1Y - lh * 0.5f, lw, lh });
        L.learnButtons.push_back({ 2, subX + halfW - 12.0f - lw,   row1Y - lh * 0.5f, lw, lh });
    }

    addKnobRow(L.knobs, margin, halfW, row1Y + knobY, radius,
               { { kParamFlagShape, "FLAGEOLET" }, { kParamUpHarm, "UP" }, { kParamDownHarm, "DOWN" },
                 { kParamBloom, "BLOOM" }, { kParamBaseUnderHarm, "BASE" }, { kParamHarmCC, "CONTROL" } });
    addKnobRow(L.knobs, subX, halfW, row1Y + knobY, radius,
               { { kParamSubLevel, "SUB LEVEL" }, { kParamSubDeep, "-2 OCT" }, { kParamSubMorph, "MORPH" },
                 { kParamBaseUnderSub, "BASE" }, { kParamBaseSub, "BASE SUB" }, { kParamSubCC, "CONTROL" } });

    // --- row 2: WAVEFORM, full width: preview scope + shape knobs ---
    const float waveY = row1Y + panelH + gap + 6.0f;
    L.panels.push_back({ margin, waveY, width - 2.0f * margin, panelH, "WAVEFORM" });
    {
        const float scopeW = 170.0f;
        L.scope = { margin + 14.0f, waveY + 16.0f, scopeW, panelH - 28.0f };
        const float knobsX = L.scope.x + scopeW + 6.0f;
        addKnobRow(L.knobs, knobsX, width - margin - knobsX, waveY + knobY, radius,
                   { { kParamWave, "SINE / SAW / SQR" }, { kParamTilt, "TILT" }, { kParamDetune, "DETUNE" },
                     { kParamVoices, "VOICES" }, { kParamSpread, "SPREAD" }, { kParamSubShape, "SUB WAVE" } });
    }

    // --- row 3: TONE | PITCH | AMP, widths proportional to knob count (3/3/5;
    // PITCH's first slot is the MONO/POLY switch) ---
    const float row2Y = waveY + panelH + gap + 6.0f;
    const float slotW = (width - 2.0f * margin - 2.0f * gap) / 11.0f;
    const float toneX = margin, pitchX = toneX + 3.0f * slotW + gap, ampX = pitchX + 3.0f * slotW + gap;
    L.panels.push_back({ toneX,  row2Y, 3.0f * slotW, panelH, "TONE" });
    L.panels.push_back({ pitchX, row2Y, 3.0f * slotW, panelH, "PITCH" });
    L.panels.push_back({ ampX,   row2Y, 5.0f * slotW, panelH, "AMP" });
    addKnobRow(L.knobs, toneX, 3.0f * slotW, row2Y + knobY, radius,
               { { kParamDrive, "DRIVE" }, { kParamAsym, "ASYM" }, { kParamCab, "CABINET" } });
    {
        // MONO above POLY, stacked in the first slot, LED left of each
        const float bw = 46.0f, bh = 20.0f;
        const float bx = pitchX + (slotW - bw) * 0.5f + 6.0f;
        L.switches.push_back({ kParamVoiceMode, 0.0f, bx, row2Y + knobY - bh - 3.0f, bw, bh, "MONO" });
        L.switches.push_back({ kParamVoiceMode, 1.0f, bx, row2Y + knobY + 3.0f,      bw, bh, "POLY" });
        addKnobRow(L.knobs, pitchX + slotW, 2.0f * slotW, row2Y + knobY, radius,
                   { { kParamBendRange, "BEND" }, { kParamGlide, "GLIDE" } });
    }
    addKnobRow(L.knobs, ampX, 5.0f * slotW, row2Y + knobY, radius,
               { { kParamAttack, "ATTACK" }, { kParamDecay, "DECAY" }, { kParamSustain, "SUSTAIN" },
                 { kParamRelease, "RELEASE" }, { kParamVolume, "VOLUME" } });

    L.height = row2Y + panelH + margin + 14.0f; // + footer line
    return L;
}

// -----------------------------------------------------------------------------------------------------------
// value mapping (knob position <-> parameter value)

inline float paramToNormalized(uint32_t param, float value) noexcept
{
    const ParamInfo& info = getParamInfo(param);
    float t;
    if (info.shape == ParamShape::Logarithmic)
    {
        const float lo = info.min > 0.0f ? info.min : 1e-6f;
        const float hi = info.max > lo ? info.max : lo * 1.0001f;
        const float v = value < lo ? lo : (value > hi ? hi : value);
        t = (float)(std::log(v / lo) / std::log(hi / lo));
    }
    else
    {
        t = (value - info.min) / (info.max - info.min);
    }
    return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
}

inline float normalizedToParam(uint32_t param, float t) noexcept
{
    const ParamInfo& info = getParamInfo(param);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    if (info.shape == ParamShape::Logarithmic)
    {
        const float lo = info.min > 0.0f ? info.min : 1e-6f;
        const float hi = info.max > lo ? info.max : lo * 1.0001f;
        return lo * std::pow(hi / lo, t);
    }
    const float v = info.min + (info.max - info.min) * t;
    return info.shape == ParamShape::Integer ? std::round(v) : v;
}

inline void formatParamValue(uint32_t index, float value, char* out, size_t outSize) noexcept
{
    switch (index)
    {
    case kParamHarmCC:
    case kParamSubCC:
        std::snprintf(out, outSize, "CC %d", (int)(value + 0.5f));
        return;
    case kParamBendRange:
        std::snprintf(out, outSize, "+-%d st", (int)(value + 0.5f));
        return;
    case kParamWave:
    case kParamFlagShape:
        if (value < 0.01f) std::snprintf(out, outSize, "SINE");
        else if (std::fabs(value - 0.5f) < 0.01f) std::snprintf(out, outSize, "SAW");
        else if (value > 0.99f) std::snprintf(out, outSize, "SQUARE");
        else if (value < 0.5f) std::snprintf(out, outSize, "SAW %d%%", (int)std::lround(value * 200.0f));
        else std::snprintf(out, outSize, "SQR %d%%", (int)std::lround((value - 0.5f) * 200.0f));
        return;
    case kParamSubShape:
        if (value < 0.02f) std::snprintf(out, outSize, "SINE");
        else if (value > 0.98f) std::snprintf(out, outSize, "SQUARE");
        else std::snprintf(out, outSize, "%d%% SQR", (int)std::lround(value * 100.0f));
        return;
    case kParamTilt:
        std::snprintf(out, outSize, "1/n^%.2f", value);
        return;
    case kParamUpHarm:
    case kParamDownHarm:
    {
        // harmonic number and the interval it sounds above the note
        const int h = (int)(value + 0.5f);
        std::snprintf(out, outSize, "H%d +%.0fst", h, 12.0 * std::log2((double)h));
        return;
    }
    case kParamVoices:
    {
        const int v = (int)(value + 0.5f);
        std::snprintf(out, outSize, v == 1 ? "1 (OFF)" : "%d", v);
        return;
    }
    case kParamSpread:
        std::snprintf(out, outSize, "%.0f ct", value);
        return;
    case kParamDetune:
        std::snprintf(out, outSize, "+-%.1f ct", value);
        return;
    case kParamBaseUnderHarm:
    case kParamBaseUnderSub:
    case kParamBaseSub:
        // levels vs the fundamental: dB reads better than 0.05
        if (value < 0.001f) std::snprintf(out, outSize, index == kParamBaseSub ? "OFF" : "MUTE");
        else std::snprintf(out, outSize, "%.0f dB", 20.0 * std::log10((double)value));
        return;
    default:
        break;
    }

    const ParamInfo& info = getParamInfo(index);
    if (std::strcmp(info.unit, "Hz") == 0)
        std::snprintf(out, outSize, "%.1fk", value / 1000.0);
    else if (std::strcmp(info.unit, "s") == 0)
        std::snprintf(out, outSize, value < 1.0f ? "%.0fms" : "%.2fs", value < 1.0f ? value * 1000.0 : (double)value);
    else
        std::snprintf(out, outSize, "%.0f%%", value * 100.0);
}

// the bipolar harmonics controller's state (mirrors
// LeadVoice::updateHarmonicGains()): "BASE" at the centre, else the
// direction's flageolet and how far it has crossfaded in, e.g. "UP H3 60%"
inline void formatHarmonicPosition(float amount, int upHarmonic, int downHarmonic, char* out, size_t outSize) noexcept
{
    const float b = jordanus::LeadVoice::bipolar(amount);
    if (std::fabs(b) < 0.01f)
        std::snprintf(out, outSize, "BASE");
    else
        std::snprintf(out, outSize, "%s H%d %d%%", b > 0.0f ? "UP" : "DN",
                      b > 0.0f ? upHarmonic : downHarmonic, (int)std::lround(std::fabs(b) * 100.0f));
}

// where the bipolar SUB / OCTAVE controller sits (mirrors LeadVoice::process()):
// "OFF" at the centre, "-1 OCT" / "-1-2 OCT" below, "+1 OCT" ... "+2 OCT" above
inline void formatOctavePosition(float amount, char* out, size_t outSize) noexcept
{
    const float b = jordanus::LeadVoice::bipolar(amount);
    const float a = std::fabs(b);
    const char* label;
    if (a < 0.02f)
        label = "OFF";
    else if (b < 0.0f)
        label = a < 0.5f ? "-1 OCT" : "-1-2";
    else
    {
        const float pos = 2.0f * a;
        label = pos < 0.85f ? "0>+1" : pos < 1.15f ? "+1 OCT" : pos < 1.85f ? "+1>+2" : "+2 OCT";
    }
    std::snprintf(out, outSize, "%s", label);
}

// -----------------------------------------------------------------------------------------------------------
// drawing helpers

inline void roundedRect(cairo_t* cr, double x, double y, double w, double h, double r)
{
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r,     r, -M_PI_2, 0.0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0.0,      M_PI_2);
    cairo_arc(cr, x + r,     y + h - r, r, M_PI_2,   M_PI);
    cairo_arc(cr, x + r,     y + r,     r, M_PI,     3.0 * M_PI_2);
    cairo_close_path(cr);
}

inline void drawText(cairo_t* cr, const char* text, double x, double y)
{
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, text);
    cairo_new_path(cr);
}

inline double textWidth(cairo_t* cr, const char* text)
{
    cairo_text_extents_t ext;
    cairo_text_extents(cr, text, &ext);
    return ext.x_advance;
}

inline void centeredText(cairo_t* cr, const char* text, double cx, double cy)
{
    cairo_text_extents_t ext;
    cairo_text_extents(cr, text, &ext);
    drawText(cr, text, cx - ext.width / 2.0 - ext.x_bearing, cy - ext.height / 2.0 - ext.y_bearing);
}

inline void setSilkFont(cairo_t* cr, double size, bool bold = true)
{
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL,
                           bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
}

inline void setLcdFont(cairo_t* cr, double size, bool bold = true)
{
    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL,
                           bold ? CAIRO_FONT_WEIGHT_BOLD : CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
}

// -----------------------------------------------------------------------------------------------------------
// painting

struct PaintState
{
    const float* values = nullptr;
    int hoverKnob = -1;
    int dragKnob = -1;
    int hoverLearn = -1;
    int hoverSwitch = -1;
    int learnTarget = 0;       // 0 = off, 1 = harmonics, 2 = sub
    bool learnBlinkOn = false; // blink phase for the armed LED / LCD prompt
    const bool* autoShowValue = nullptr;

    const char* presetName = "INIT";
    bool presetIsUnsaved = false;
    bool presetEditingName = false;
    const char* presetEditBuffer = "";
    int hoverPresetButton = -1;       // 0=prev,1=next,2=save,3=delete, or -1
    bool presetDeleteEnabled = false; // false while on INIT - nothing to delete
};

// silk-screened frame with the title cut into the top line
inline void paintPanel(cairo_t* cr, const PanelBox& p)
{
    setSilkFont(cr, 10.5);
    const double titleW = textWidth(cr, p.title);
    const double tx = p.x + 12.0, ty = p.y;

    setColor(cr, kSilkDim);
    cairo_set_line_width(cr, 1.2);
    cairo_move_to(cr, tx - 5.0, ty);
    cairo_line_to(cr, p.x + 3.0, ty);
    cairo_arc_negative(cr, p.x + 3.0, ty + 3.0, 3.0, -M_PI_2, -M_PI);
    cairo_line_to(cr, p.x, ty + p.h - 3.0);
    cairo_arc_negative(cr, p.x + 3.0, ty + p.h - 3.0, 3.0, M_PI, M_PI_2);
    cairo_line_to(cr, p.x + p.w - 3.0, ty + p.h);
    cairo_arc_negative(cr, p.x + p.w - 3.0, ty + p.h - 3.0, 3.0, M_PI_2, 0.0);
    cairo_line_to(cr, p.x + p.w, ty + 3.0);
    cairo_arc_negative(cr, p.x + p.w - 3.0, ty + 3.0, 3.0, 0.0, -M_PI_2);
    cairo_line_to(cr, tx + titleW + 5.0, ty);
    cairo_stroke(cr);

    setColor(cr, kSilk);
    drawText(cr, p.title, tx, ty + 4.0);
}

inline void paintKnob(cairo_t* cr, const Knob& k, float value, bool hovered, bool dragging, bool autoShow)
{
    const float t = paramToNormalized(k.param, value);
    const double startAngle = M_PI * 0.75;
    const double sweep = M_PI * 1.5;
    const double valueAngle = startAngle + sweep * t;

    // scale ticks
    setColor(cr, kSilkDim);
    cairo_set_line_width(cr, 1.0);
    for (int i = 0; i <= 10; ++i)
    {
        const double a = startAngle + sweep * i / 10.0;
        const double r0 = k.radius + 4.0, r1 = k.radius + (i % 5 == 0 ? 8.0 : 6.0);
        cairo_move_to(cr, k.cx + std::cos(a) * r0, k.cy + std::sin(a) * r0);
        cairo_line_to(cr, k.cx + std::cos(a) * r1, k.cy + std::sin(a) * r1);
    }
    cairo_stroke(cr);

    // value arc
    cairo_set_line_width(cr, 2.5);
    setColor(cr, kOrange, dragging || hovered ? 1.0 : 0.85);
    cairo_arc(cr, k.cx, k.cy, k.radius + 2.0, startAngle, valueAngle);
    cairo_stroke(cr);

    // body: dark radial gradient with a knurled-ish edge
    cairo_pattern_t* body = cairo_pattern_create_linear(k.cx, k.cy - k.radius, k.cx, k.cy + k.radius);
    cairo_pattern_add_color_stop_rgb(body, 0.0, kKnobTop.r, kKnobTop.g, kKnobTop.b);
    cairo_pattern_add_color_stop_rgb(body, 1.0, kKnobBot.r, kKnobBot.g, kKnobBot.b);
    cairo_set_source(cr, body);
    cairo_arc(cr, k.cx, k.cy, k.radius - 1.0, 0.0, 2.0 * M_PI);
    cairo_fill(cr);
    cairo_pattern_destroy(body);

    setColor(cr, kKnobBot);
    cairo_set_line_width(cr, 1.0);
    for (int i = 0; i < 36; ++i)
    {
        const double a = 2.0 * M_PI * i / 36.0;
        cairo_move_to(cr, k.cx + std::cos(a) * (k.radius - 4.0), k.cy + std::sin(a) * (k.radius - 4.0));
        cairo_line_to(cr, k.cx + std::cos(a) * (k.radius - 1.0), k.cy + std::sin(a) * (k.radius - 1.0));
    }
    cairo_stroke(cr);

    if (hovered || dragging)
    {
        setColor(cr, kSilk, 0.08);
        cairo_arc(cr, k.cx, k.cy, k.radius - 1.0, 0.0, 2.0 * M_PI);
        cairo_fill(cr);
    }

    // pointer
    setColor(cr, kSilk);
    cairo_set_line_width(cr, 2.2);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, k.cx + std::cos(valueAngle) * (k.radius * 0.35), k.cy + std::sin(valueAngle) * (k.radius * 0.35));
    cairo_line_to(cr, k.cx + std::cos(valueAngle) * (k.radius - 4.0), k.cy + std::sin(valueAngle) * (k.radius - 4.0));
    cairo_stroke(cr);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);

    // label, or the value while dragging / just changed
    char buf[32];
    const bool showValue = dragging || autoShow;
    if (showValue)
        formatParamValue(k.param, value, buf, sizeof(buf));
    setSilkFont(cr, 9.5);
    setColor(cr, showValue ? kOrange : kSilk);
    centeredText(cr, showValue ? buf : k.label, k.cx, k.cy + k.radius + 16.0);
}

inline void paintLed(cairo_t* cr, double cx, double cy, bool on)
{
    if (on)
    {
        cairo_pattern_t* glow = cairo_pattern_create_radial(cx, cy, 0.0, cx, cy, 7.0);
        cairo_pattern_add_color_stop_rgba(glow, 0.0, kLedOn.r, kLedOn.g, kLedOn.b, 0.55);
        cairo_pattern_add_color_stop_rgba(glow, 1.0, kLedOn.r, kLedOn.g, kLedOn.b, 0.0);
        cairo_set_source(cr, glow);
        cairo_arc(cr, cx, cy, 7.0, 0.0, 2.0 * M_PI);
        cairo_fill(cr);
        cairo_pattern_destroy(glow);
    }
    setColor(cr, on ? kLedOn : kLedOff);
    cairo_arc(cr, cx, cy, 2.8, 0.0, 2.0 * M_PI);
    cairo_fill(cr);
}

inline void paintRubberButton(cairo_t* cr, double x, double y, double w, double h,
                              const char* label, bool hovered, bool enabled)
{
    roundedRect(cr, x, y, w, h, 3.0);
    setColor(cr, kBtnFace, enabled ? 1.0 : 0.5);
    cairo_fill_preserve(cr);
    setColor(cr, hovered && enabled ? kSilk : kKnobBot, hovered && enabled ? 0.6 : 1.0);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    setSilkFont(cr, 9.0);
    setColor(cr, enabled ? kSilk : kSilkDim);
    centeredText(cr, label, x + w / 2.0, y + h / 2.0);
}

inline void paintSwitch(cairo_t* cr, const SwitchButton& s, float value, bool hovered)
{
    paintRubberButton(cr, s.x, s.y, s.w, s.h, s.label, hovered, true);
    paintLed(cr, s.x - 9.0, s.y + s.h / 2.0, std::fabs(value - s.value) < 0.5f);
}

inline void paintLearnButton(cairo_t* cr, const LearnButton& b, bool armed, bool blinkOn, bool hovered)
{
    // chassis-coloured pad first, so the button cuts the panel's frame line
    setColor(cr, kChassis);
    cairo_rectangle(cr, b.x - 5.0, b.y, b.w + 10.0, b.h);
    cairo_fill(cr);

    roundedRect(cr, b.x, b.y, b.w, b.h, 3.0);
    setColor(cr, kBtnFace);
    cairo_fill_preserve(cr);
    setColor(cr, hovered ? kSilk : kKnobBot, hovered ? 0.6 : 1.0);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    paintLed(cr, b.x + 9.0, b.y + b.h / 2.0, armed && blinkOn);

    setSilkFont(cr, 8.5);
    setColor(cr, armed ? kOrange : kSilk);
    centeredText(cr, "LEARN", b.x + 9.0 + (b.w - 9.0) / 2.0, b.y + b.h / 2.0);
}

inline void paintBar(cairo_t* cr, double x, double y, double w, double h, float amount)
{
    setColor(cr, kLcdInk);
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, x + 0.5, y + 0.5, w - 1.0, h - 1.0);
    cairo_stroke(cr);

    // segmented fill, LCD style
    const int segs = 20;
    const double segW = (w - 4.0) / segs;
    const int lit = (int)std::lround(std::clamp(amount, 0.0f, 1.0f) * segs);
    for (int i = 0; i < lit; ++i)
        cairo_rectangle(cr, x + 2.0 + i * segW, y + 2.0, segW - 1.0, h - 4.0);
    cairo_fill(cr);
}

// LCD bar filled from the centre outwards, for the bipolar SUB / OCTAVE controller
inline void paintBipolarBar(cairo_t* cr, double x, double y, double w, double h, float amount)
{
    setColor(cr, kLcdInk);
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, x + 0.5, y + 0.5, w - 1.0, h - 1.0);
    cairo_stroke(cr);
    cairo_move_to(cr, x + w / 2.0, y - 2.0);
    cairo_line_to(cr, x + w / 2.0, y + h + 2.0);
    cairo_stroke(cr);

    const int half = 10;
    const double segW = (w - 4.0) / (2 * half);
    const float b = 2.0f * std::clamp(amount, 0.0f, 1.0f) - 1.0f;
    const int lit = (int)std::lround(std::fabs(b) * half);
    for (int i = 0; i < lit; ++i)
    {
        const int seg = b > 0.0f ? half + i : half - 1 - i;
        cairo_rectangle(cr, x + 2.0 + seg * segW, y + 2.0, segW - 1.0, h - 4.0);
    }
    cairo_fill(cr);
}

inline void paintLcd(cairo_t* cr, const Layout& L, const PaintState& state)
{
    const LcdBox& d = L.lcd;

    // bezel + glass
    roundedRect(cr, d.x - 3.0, d.y - 3.0, d.w + 6.0, d.h + 6.0, 4.0);
    setColor(cr, kKnobBot);
    cairo_fill(cr);
    cairo_rectangle(cr, d.x, d.y, d.w, d.h);
    setColor(cr, kLcdBg);
    cairo_fill(cr);

    const PresetBarLayout& pb = L.presetBar;

    // line 1: preset name (click to rename)
    char buf[64];
    setLcdFont(cr, 16.0);
    setColor(cr, kLcdInk);
    if (state.presetEditingName)
        std::snprintf(buf, sizeof(buf), "%s_", state.presetEditBuffer);
    else
        std::snprintf(buf, sizeof(buf), "%s%s", state.presetName, state.presetIsUnsaved ? " *" : "");
    drawText(cr, buf, pb.nameX + 2.0, pb.nameY + 19.0);
    if (state.presetEditingName)
    {
        cairo_rectangle(cr, pb.nameX, pb.nameY, pb.nameW, pb.nameH);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);
    }

    setLcdFont(cr, 10.0, false);
    setColor(cr, kLcdInkDim, 0.75);
    {
        const char* tag = "PROG";
        drawText(cr, tag, d.x + d.w - 8.0 - textWidth(cr, tag), pb.nameY + 17.0);
    }

    // separator
    setColor(cr, kLcdInk, 0.5);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, d.x + 6.0, d.y + 36.5);
    cairo_line_to(cr, d.x + d.w - 6.0, d.y + 36.5);
    cairo_stroke(cr);

    // line 2: live controller state, or the learn prompt while armed
    const float* v = state.values;
    if (state.learnTarget != 0)
    {
        setLcdFont(cr, 10.0);
        setColor(cr, kLcdInk, state.learnBlinkOn ? 1.0 : 0.35);
        std::snprintf(buf, sizeof(buf), "LEARN %s: MOVE A STRIP / WHEEL / KNOB  (CLICK LEARN TO CANCEL)",
                      state.learnTarget == 1 ? "HARMONICS" : "SUB");
        drawText(cr, buf, d.x + 8.0, d.y + 53.0);
        return;
    }

    const double rowY = d.y + 44.0, barH = 11.0;
    const double colW = (d.w - 16.0) / 2.0;

    setLcdFont(cr, 10.0);
    setColor(cr, kLcdInk);

    std::snprintf(buf, sizeof(buf), "HARM CC%d", (int)(v[kParamHarmCC] + 0.5f));
    drawText(cr, buf, d.x + 8.0, rowY + 9.0);
    double bx = d.x + 8.0 + textWidth(cr, buf) + 8.0;
    char pos[16];
    formatHarmonicPosition(v[kParamHarmView], (int)(v[kParamUpHarm] + 0.5f), (int)(v[kParamDownHarm] + 0.5f), pos, sizeof(pos));
    const double posW = 66.0;
    paintBipolarBar(cr, bx, rowY, d.x + 8.0 + colW - 10.0 - posW - bx, barH, v[kParamHarmView]);
    setColor(cr, kLcdInk);
    drawText(cr, pos, d.x + 8.0 + colW - 6.0 - posW, rowY + 9.0);

    const double sx = d.x + 8.0 + colW + 6.0;
    std::snprintf(buf, sizeof(buf), "SUB CC%d", (int)(v[kParamSubCC] + 0.5f));
    drawText(cr, buf, sx, rowY + 9.0);
    bx = sx + textWidth(cr, buf) + 8.0;
    formatOctavePosition(v[kParamSubView], pos, sizeof(pos));
    paintBipolarBar(cr, bx, rowY, d.x + d.w - 8.0 - posW - bx, barH, v[kParamSubView]);
    setColor(cr, kLcdInk);
    drawText(cr, pos, d.x + d.w - 4.0 - posW, rowY + 9.0);
}

// LCD-style scope: two cycles of the plain tone, computed by the DSP's own
// LeadVoice::previewCycle() from the current knob values
inline void paintScope(cairo_t* cr, const LcdBox& d, const float* v)
{
    roundedRect(cr, d.x - 3.0, d.y - 3.0, d.w + 6.0, d.h + 6.0, 4.0);
    setColor(cr, kKnobBot);
    cairo_fill(cr);
    cairo_rectangle(cr, d.x, d.y, d.w, d.h);
    setColor(cr, kLcdBg);
    cairo_fill(cr);

    setColor(cr, kLcdInk, 0.25);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, d.x + 4.0, d.y + d.h / 2.0 + 0.5);
    cairo_line_to(cr, d.x + d.w - 4.0, d.y + d.h / 2.0 + 0.5);
    cairo_stroke(cr);

    jordanus::LeadParams p;
    jordanus::LeadVoice::applyWave(p, v[kParamWave]);
    p.tilt = v[kParamTilt];
    p.drive = v[kParamDrive];
    p.asym = v[kParamAsym];
    p.baseSub = v[kParamBaseSub];
    p.subShape = v[kParamSubShape];

    const int points = 240;
    float ys[points + 1];
    float peak = 1e-6f;
    for (int i = 0; i <= points; ++i)
    {
        ys[i] = jordanus::LeadVoice::previewCycle(p, 2.0 * i / points);
        peak = std::max(peak, std::fabs(ys[i]));
    }

    const double x0 = d.x + 6.0, w = d.w - 12.0, mid = d.y + d.h / 2.0, amp = (d.h / 2.0 - 7.0) / peak;
    setColor(cr, kLcdInk);
    cairo_set_line_width(cr, 1.6);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    for (int i = 0; i <= points; ++i)
    {
        const double x = x0 + w * i / points, y = mid - ys[i] * amp;
        if (i == 0) cairo_move_to(cr, x, y);
        else cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);

    setLcdFont(cr, 8.0, false);
    setColor(cr, kLcdInk, 0.75);
    drawText(cr, "BASE WAVE", d.x + 5.0, d.y + 10.0);
}

inline void paint(cairo_t* cr, const Layout& L, const PaintState& state)
{
    setColor(cr, kChassis);
    cairo_paint(cr);

    // header: model right, thin orange rule under it
    setSilkFont(cr, 24.0);
    setColor(cr, kSilk);
    const char* model = "JORDANUS";
    const double modelW = textWidth(cr, model);
    drawText(cr, model, L.width - 16.0 - modelW, 33.0);

    setSilkFont(cr, 8.5, false);
    setColor(cr, kSilkDim);
    {
        const char* sub = "HARMONIC LEAD SYNTHESIZER";
        drawText(cr, sub, L.width - 16.0 - modelW - 12.0 - textWidth(cr, sub), 31.0);
    }

    setColor(cr, kOrange);
    cairo_rectangle(cr, 14.0, 41.0, L.width - 28.0, 1.5);
    cairo_fill(cr);

    // LCD + preset buttons
    paintLcd(cr, L, state);
    const PresetBarLayout& pb = L.presetBar;
    paintRubberButton(cr, pb.prev.x, pb.prev.y, pb.prev.w, pb.prev.h, pb.prev.label, state.hoverPresetButton == 0, true);
    paintRubberButton(cr, pb.next.x, pb.next.y, pb.next.w, pb.next.h, pb.next.label, state.hoverPresetButton == 1, true);
    paintRubberButton(cr, pb.save.x, pb.save.y, pb.save.w, pb.save.h, pb.save.label, state.hoverPresetButton == 2, true);
    paintRubberButton(cr, pb.del.x,  pb.del.y,  pb.del.w,  pb.del.h,  pb.del.label,  state.hoverPresetButton == 3,
                      state.presetDeleteEnabled);

    for (const PanelBox& p : L.panels)
        paintPanel(cr, p);
    paintScope(cr, L.scope, state.values);

    for (size_t i = 0; i < L.switches.size(); ++i)
        paintSwitch(cr, L.switches[i], state.values[L.switches[i].param], (int)i == state.hoverSwitch);

    for (size_t i = 0; i < L.learnButtons.size(); ++i)
    {
        const LearnButton& b = L.learnButtons[i];
        paintLearnButton(cr, b, state.learnTarget == b.target, state.learnBlinkOn, (int)i == state.hoverLearn);
    }


    for (size_t i = 0; i < L.knobs.size(); ++i)
    {
        const Knob& k = L.knobs[i];
        const bool autoShow = state.autoShowValue != nullptr && state.autoShowValue[k.param];
        paintKnob(cr, k, state.values[k.param], (int)i == state.hoverKnob, (int)i == state.dragKnob, autoShow);
    }

    // footer silk: what's on which controller
    setSilkFont(cr, 8.5, false);
    setColor(cr, kSilkDim);
    centeredText(cr, "BOTH CONTROLLERS CENTRED = PLAIN TONE   \xc2\xb7   HARM: UP / DOWN FLAGEOLETS   \xc2\xb7   SUB: DOWN = SUB, UP = OCTAVE UP",
                 L.width / 2.0, L.height - 12.0);
}

} // namespace ui
} // namespace jordanus
