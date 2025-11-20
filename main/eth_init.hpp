#pragma once

#include "config.hpp"
#include "esp_netif.h"

esp_err_t ethernet_start(const NetworkConfig& netcfg);
extern esp_netif_t* netif;
