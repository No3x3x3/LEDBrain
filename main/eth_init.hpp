#pragma once

#include "config.hpp"
#include "esp_netif.h"

esp_err_t ethernet_start(const NetworkConfig& netcfg);
bool ethernet_is_connected();
std::string ethernet_get_ip();
extern esp_netif_t* netif;
