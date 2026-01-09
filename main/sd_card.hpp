#pragma once

#include <string>
#include "esp_err.h"

// SD card initialization and management
// Note: Pin configuration may need adjustment based on JC-ESP32P4-M3-DEV schematic
// Check schematic file: docs/hardware/schematics/3_8311&TFCARD.png

namespace {
  // SDMMC pin configuration for JC-ESP32P4-M3-DEV
  // Based on typical ESP32-P4 SDMMC pin assignments and board layout
  // For microSD card slot (check schematic 3_8311&TFCARD.png for verification)
  // ESP32-P4 SDMMC typically uses GPIO matrix, so pins can be flexible
  // Common ESP32-P4 SDMMC pin assignments:
  constexpr int SDMMC_CLK_PIN = 14;  // SDMMC clock pin (CLK)
  constexpr int SDMMC_CMD_PIN = 15;  // SDMMC command pin (CMD)
  constexpr int SDMMC_D0_PIN = 2;    // SDMMC data line 0 (D0/DAT0)
  constexpr int SDMMC_D1_PIN = 4;    // SDMMC data line 1 (D1/DAT1) - optional for 1-bit mode
  constexpr int SDMMC_D2_PIN = 12;   // SDMMC data line 2 (D2/DAT2) - optional for 1-bit mode
  constexpr int SDMMC_D3_PIN = 13;   // SDMMC data line 3 (D3/DAT3) - optional for 1-bit mode
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

