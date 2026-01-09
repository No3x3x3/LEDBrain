#pragma once

#include "led_engine/types.hpp"
#include "esp_err.h"

// Lightweight Snapcast PCM subscriber: connects to Snapserver and exposes audio frames to the app.
// It does NOT output audio; intended for analysis/visualization only.

esp_err_t snapclient_light_start(const SnapcastConfig& cfg);
void snapclient_light_stop();
