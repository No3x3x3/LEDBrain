#pragma once
#include "config.hpp"
#include "led_engine.hpp"

class WledEffectsRuntime;

void start_web_server(AppConfig& cfg,
                      LedEngineRuntime* runtime = nullptr,
                      WledEffectsRuntime* wled_runtime = nullptr);
