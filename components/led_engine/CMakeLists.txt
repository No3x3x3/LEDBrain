idf_component_register(
  SRCS "led_engine.cpp" "audio_pipeline.cpp" "pinout.cpp"
  INCLUDE_DIRS "include"
  REQUIRES esp_timer driver esp_pm
)
