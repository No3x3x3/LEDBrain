#pragma once

#include <string>
#include "esp_err.h"

// SD card initialization and management
// Note: Pin configuration may need adjustment based on JC-ESP32P4-M3-DEV schematic
// Check schematic file: docs/hardware/schematics/3_8311&TFCARD.png

namespace {
  // SDMMC pin configuration for JC-ESP32P4-M3-DEV / ESP32-P4
  // ESP32-P4 SDIO 3.0 dedicated pins (SD1 slot):
  constexpr int SDMMC_CLK_PIN = 43;  // SD1_CCLK_PAD - SDMMC clock
  constexpr int SDMMC_CMD_PIN = 44;  // SD1_CCMD_PAD - SDMMC command
  constexpr int SDMMC_D0_PIN = 39;   // SD1_CDATA0_PAD - Data line 0
  constexpr int SDMMC_D1_PIN = 40;   // SD1_CDATA1_PAD - Data line 1
  constexpr int SDMMC_D2_PIN = 41;   // SD1_CDATA2_PAD - Data line 2
  constexpr int SDMMC_D3_PIN = 42;   // SD1_CDATA3_PAD - Data line 3
  constexpr int SDMMC_CD_PIN = -1;   // Card detect pin (optional, -1 if not used)
  
  // Note: For 1-bit mode, only CLK, CMD, and D0 are required
  // For 4-bit mode, all D0-D3 are required for better performance
}

// Initialize SD card interface (SDMMC)
// Returns ESP_OK on success, error code otherwise
esp_err_t sd_card_init();

// Deinitialize SD card interface
void sd_card_deinit();

// Check if SD card is mounted and accessible
bool sd_card_is_mounted();

// Get SD card mount point (e.g., "/sdcard")
std::string sd_card_get_mount_point();

// Get SD card total size in bytes
uint64_t sd_card_get_total_size();

// Get SD card free size in bytes
uint64_t sd_card_get_free_size();

// Get SD card filesystem type (FAT32, exFAT, etc.)
std::string sd_card_get_filesystem_type();

