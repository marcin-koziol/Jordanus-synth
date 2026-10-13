// Dev aid, not wired into CMakeLists.txt: renders scripted performances
// through LeadVoice to 48k float WAVs, for comparing against the reference
// recordings with the same analysis. Compile ad hoc:
//   g++ -O2 -std=c++17 -o /tmp/jordanus_dsp dsp_render.cpp
//   /tmp/jordanus_dsp <out_dir>
#include "../dsp/LeadVoice.hpp"

#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

static void writeWav(const std::string& path, const std::vector<float>& data, int sr)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) { std::perror(path.c_str()); return; }
    const uint32_t dataBytes = (uint32_t)data.size() * 4, fmtSize = 16, riffSize = 36 + dataBytes, srU = (uint32_t)sr, byteRate = srU * 4;
    const uint16_t fmtFloat = 3, channels = 1, align = 4, bits = 32;
    std::fwrite("RIFF", 1, 4, f); std::fwrite(&riffSize, 4, 1, f); std::fwrite("WAVEfmt ", 1, 8, f);
    std::fwrite(&fmtSize, 4, 1, f); std::fwrite(&fmtFloat, 2, 1, f); std::fwrite(&channels, 2, 1, f);
    std::fwrite(&srU, 4, 1, f); std::fwrite(&byteRate, 4, 1, f); std::fwrite(&align, 2, 1, f); std::fwrite(&bits, 2, 1, f);
    std::fwrite("data", 1, 4, f); std::fwrite(&dataBytes, 4, 1, f); std::fwrite(data.data(), 4, data.size(), f);
    std::fclose(f);
}

struct Controls { float harm = 0.5f, sub = 0.5f, bend = 0.0f; }; // both bipolar, 0.5 = centre

// renders `seconds` of one held note while `script(t)` drives the controllers
static void render(const std::string& path, int note, float seconds,
                   const std::function<Controls(float)>& script)
{
    const int sr = 48000;
    jordanus::LeadVoice v;
    jordanus::LeadParams p;
    v.setSampleRate(sr);
    v.applyParams(p);
    v.noteOn(note, true);

    std::vector<float> out((size_t)(seconds * sr));
    for (size_t i = 0; i < out.size(); ++i)
    {
        if (i % 32 == 0)
        {
            const Controls c = script((float)i / sr);
            v.setHarmonics(c.harm);
            v.setSub(c.sub);
            v.setPitchBend(c.bend);
        }
        if (i == out.size() - (size_t)(0.3f * sr))
            v.noteOff();
        out[i] = 0.5f * v.process();
    }
    writeWav(path, out, sr);
    std::printf("wrote %s\n", path.c_str());
}

static float ramp(float t, float t0, float t1, float a, float b)
{
    if (t <= t0) return a;
    if (t >= t1) return b;
    return a + (b - a) * (t - t0) / (t1 - t0);
}

int main(int argc, char** argv)
{
    const std::string dir = argc > 1 ? argv[1] : "/tmp";

    // harmonics controller held at each rung of the UP ladder for a second (A3)
    render(dir + "/r_low_steps.wav", 57, 5.3f, [](float t) {
        Controls c; c.harm = 0.5f + std::min(4.0f, std::floor(t)) / 8.0f; return c; });
    // same going DOWN from the centre
    render(dir + "/r_high_steps.wav", 57, 5.3f, [](float t) {
        Controls c; c.harm = 0.5f - std::min(4.0f, std::floor(t)) / 8.0f; return c; });
    // D3: SUB controller from the centre to halfway down (-1 oct), then fully
    // down (-1 and -2 oct), then wheel up while the sub is held
    render(dir + "/r_sub.wav", 50, 4.3f, [](float t) {
        Controls c; c.sub = t < 1.5f ? ramp(t, 0.5f, 0.8f, 0.5f, 0.25f) : ramp(t, 1.5f, 1.8f, 0.25f, 0.0f);
        c.harm = ramp(t, 2.8f, 3.0f, 0.5f, 0.75f); return c; });
    // D3: SUB controller from the centre up: +1 oct, then +2 oct
    render(dir + "/r_up.wav", 50, 3.3f, [](float t) {
        Controls c; c.sub = t < 1.5f ? ramp(t, 0.5f, 0.8f, 0.5f, 0.75f) : ramp(t, 1.5f, 1.8f, 0.75f, 1.0f); return c; });
    // octave bend down and back (like pitch.wav)
    render(dir + "/r_bend.wav", 57, 3.3f, [](float t) {
        Controls c; c.bend = t < 1.5f ? ramp(t, 0.5f, 1.4f, 0.0f, -12.0f) : ramp(t, 2.0f, 2.6f, -12.0f, 0.0f); return c; });
    return 0;
}
