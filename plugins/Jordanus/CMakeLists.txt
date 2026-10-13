dpf_add_plugin(jordanus
  TARGETS lv2 vst3 clap jack
  UI_TYPE cairo
  FILES_DSP
      JordanusPlugin.cpp
  FILES_UI
      JordanusUI.cpp)

target_include_directories(jordanus PUBLIC ".")
