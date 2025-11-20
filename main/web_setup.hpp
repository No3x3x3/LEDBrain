#pragma once
#include "config.hpp"
#include "led_engine.hpp"

void start_web_server(AppConfig& cfg, LedEngineRuntime* runtime = nullptr);
