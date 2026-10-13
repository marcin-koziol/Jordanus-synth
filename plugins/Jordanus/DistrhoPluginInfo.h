#ifndef DISTRHO_PLUGIN_INFO_H_INCLUDED
#define DISTRHO_PLUGIN_INFO_H_INCLUDED

#define DISTRHO_PLUGIN_BRAND   "Jordanus"
#define DISTRHO_PLUGIN_NAME    "Jordanus"
#define DISTRHO_PLUGIN_URI     "https://github.com/marcin-koziol/Jordanus-synth"
#define DISTRHO_PLUGIN_CLAP_ID "jordanus.jordanus"

#define DISTRHO_PLUGIN_BRAND_ID  Jrdn
#define DISTRHO_PLUGIN_UNIQUE_ID Jrd1

#define DISTRHO_PLUGIN_HAS_UI           1
#define DISTRHO_PLUGIN_IS_RT_SAFE       1
#define DISTRHO_PLUGIN_IS_SYNTH         1
#define DISTRHO_PLUGIN_NUM_INPUTS       0
#define DISTRHO_PLUGIN_NUM_OUTPUTS      2
#define DISTRHO_PLUGIN_WANT_MIDI_INPUT  1
#define DISTRHO_PLUGIN_WANT_STATE       0
#define DISTRHO_PLUGIN_WANT_PROGRAMS    0
#define DISTRHO_PLUGIN_WANT_TIMEPOS     0

#define DISTRHO_UI_USE_CAIRO             1
#define DISTRHO_UI_DEFAULT_WIDTH         760
#define DISTRHO_UI_DEFAULT_HEIGHT        516
// must match the height ui/UIPainter.hpp's buildLayout() actually lays out to
// (verified via the offline PNG renderer) - keep these two in sync by hand.

#endif // DISTRHO_PLUGIN_INFO_H_INCLUDED
