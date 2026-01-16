idf_component_register(
  SRCS "led_engine.cpp" "audio_pipeline.cpp" "pinout.cpp" "rmt_driver.cpp" "chipset_info.cpp" "color_processing.cpp" "matrix_utils.cpp" "ppa_accelerator.cpp" "framebuffer.cpp"
  INCLUDE_DIRS "include"
  REQUIRES esp_timer driver esp_pm
  PRIV_REQUIRES esp_driver_ppa
)
