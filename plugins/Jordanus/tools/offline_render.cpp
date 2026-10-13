// Dev aid, not wired into CMakeLists.txt: paints the UI to a PNG so the look
// can be checked without a host. Compile ad hoc:
//   g++ $(pkg-config --cflags --libs cairo) -std=c++17 -o /tmp/jordanus_ui offline_render.cpp
//   /tmp/jordanus_ui [out.png] [learn]
#include "../ui/UIPainter.hpp"

#include <cstdio>
#include <cstring>

int main(int argc, char** argv)
{
    const float width = 760.0f;
    jordanus::ui::Layout layout = jordanus::ui::buildLayout(width);
    std::printf("layout width=%.1f height=%.1f\n", layout.width, layout.height);

    cairo_surface_t* surface = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32, (int)layout.width, (int)std::ceil(layout.height));
    cairo_t* cr = cairo_create(surface);

    float values[jordanus::kParamCount];
    for (uint32_t i = 0; i < jordanus::kParamCount; ++i)
        values[i] = jordanus::getParamInfo(i).def;
    // some live controller state, so the LCD meters show something
    values[jordanus::kParamHarmView] = 0.85f; // up, between H3 and H4
    values[jordanus::kParamSubView] = 0.25f; // halfway down: -1 oct

    bool autoShow[jordanus::kParamCount] = {};
    autoShow[jordanus::kParamCab] = true;

    jordanus::ui::PaintState state;
    state.values = values;
    state.autoShowValue = autoShow;
    state.presetName = "RUDE LEAD";
    state.hoverKnob = 2;
    if (argc > 2 && std::strcmp(argv[2], "learn") == 0) // armed MIDI learn look
    {
        state.learnTarget = 2;
        state.learnBlinkOn = true;
    }

    jordanus::ui::paint(cr, layout, state);

    const char* outPath = argc > 1 ? argv[1] : "/tmp/jordanus_render.png";
    cairo_surface_write_to_png(surface, outPath);
    std::printf("wrote %s\n", outPath);

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    return 0;
}
