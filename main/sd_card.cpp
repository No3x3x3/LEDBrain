#include "sd_card.hpp"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_types.h"
#include "ff.h"
#include <cstring>

static const char* TAG = "sd_card";

namespace {
  constexpr const char* MOUNT_POINT = "/sdcard";
  bool s_mounted = false;
  sdmmc_card_t* s_card = nullptr;
  
  // Storage strategy:
  // - Flash (SPIFFS): Critical config, firmware, small files (< 1MB)
  // - SD Card: Large files, logs, backups, media, data files (> 1MB)
}

esp_err_t sd_card_init() {
  if (s_mounted) {
    ESP_LOGW(TAG, "SD card already mounted");
    return ESP_OK;
  }

  ESP_LOGI(TAG, "Initializing SD card...");
  
  // Configure SDMMC host
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.flags = SDMMC_HOST_FLAG_1BIT;  // Start with 1-bit mode for compatibility
  // host.flags = SDMMC_HOST_FLAG_4BIT;  // Can enable 4-bit mode after initialization
  
  // Configure slot (GPIO pins)
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
#ifdef CONFIG_SOC_SDMMC_USE_GPIO_MATRIX
  slot_config.clk = (gpio_num_t)SDMMC_CLK_PIN;
  slot_config.cmd = (gpio_num_t)SDMMC_CMD_PIN;
  slot_config.d0 = (gpio_num_t)SDMMC_D0_PIN;
  slot_config.d1 = (gpio_num_t)SDMMC_D1_PIN;
  slot_config.d2 = (gpio_num_t)SDMMC_D2_PIN;
  slot_config.d3 = (gpio_num_t)SDMMC_D3_PIN;
  slot_config.width = 4;  // 4-bit mode
  if (SDMMC_CD_PIN >= 0) {
    slot_config.gpio_cd = (gpio_num_t)SDMMC_CD_PIN;  // Card detect (optional)
  }
#endif

  // Initialize SDMMC host first (required before slot init)
  esp_err_t ret = sdmmc_host_init();
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize SDMMC host: %s", esp_err_to_name(ret));
    return ret;
  }

  // Initialize slot
  ret = sdmmc_host_init_slot(SDMMC_HOST_SLOT_1, &slot_config);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize SDMMC slot: %s", esp_err_to_name(ret));
    sdmmc_host_deinit();
    return ret;
  }

  // Mount filesystem
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {};
  mount_config.format_if_mount_failed = false;  // Don't format, fail if not formatted
  mount_config.max_files = 5;                   // Maximum number of open files
  mount_config.allocation_unit_size = 16 * 1024; // Allocation unit size (16KB for large cards)

  ret = esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &s_card);
  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Failed to mount filesystem. Card may not be formatted (FAT32/exFAT)");
    } else {
      ESP_LOGE(TAG, "Failed to mount SD card: %s", esp_err_to_name(ret));
    }
    sdmmc_host_deinit();
    return ret;
  }

  s_mounted = true;

  // Print card info
  sdmmc_card_print_info(stdout, s_card);
  
  ESP_LOGI(TAG, "SD card mounted successfully at %s", MOUNT_POINT);
  // Determine card type based on card structure
  const char* card_type = "Unknown";
  if (!s_card->is_mmc) {
    // SD card (not MMC)
    if (s_card->ocr & 0xC0000000) {  // Check if SDHC/SDXC (bit 30-31 set)
      card_type = "SDHC/SDXC";
    } else {
      card_type = "SDSC";
    }
  } else {
    card_type = "MMC";
  }
  ESP_LOGI(TAG, "Card type: %s", card_type);
  ESP_LOGI(TAG, "Card size: %.2f GB", 
    (double)s_card->csd.capacity * s_card->csd.sector_size / (1024.0 * 1024.0 * 1024.0));
  ESP_LOGI(TAG, "Filesystem: %s", sd_card_get_filesystem_type().c_str());

  return ESP_OK;
}

void sd_card_deinit() {
  if (!s_mounted) {
    return;
  }

  ESP_LOGI(TAG, "Unmounting SD card...");
  
  esp_vfs_fat_sdcard_unmount(MOUNT_POINT, s_card);
  sdmmc_host_deinit();
  
  s_card = nullptr;
  s_mounted = false;
  
  ESP_LOGI(TAG, "SD card unmounted");
}

bool sd_card_is_mounted() {
  return s_mounted;
}

std::string sd_card_get_mount_point() {
  return s_mounted ? std::string(MOUNT_POINT) : std::string();
}

uint64_t sd_card_get_total_size() {
  if (!s_mounted || !s_card) {
    return 0;
  }
  return (uint64_t)s_card->csd.capacity * s_card->csd.sector_size;
}

uint64_t sd_card_get_free_size() {
  if (!s_mounted) {
    return 0;
  }
  
  FATFS* fs;
  DWORD fre_clust;
  
  // Get volume label (e.g., "0:" for first drive)
  const char* drive = MOUNT_POINT;
  char drv[3] = {(char)(drive[0]), ':', '\0'};
  
  if (f_getfree(drv, &fre_clust, &fs) != FR_OK) {
    ESP_LOGE(TAG, "Failed to get free space");
    return 0;
  }
  
  uint32_t free_sectors = fre_clust * fs->csize;
  
  // Assuming 512 bytes per sector (standard for FAT32)
  return (uint64_t)free_sectors * 512;
}

std::string sd_card_get_filesystem_type() {
  if (!s_mounted || !s_card) {
    return "unknown";
  }
  
  // FATFS supports FAT12, FAT16, FAT32, and exFAT (ESP-IDF v5.1+)
  // Check card capacity and actual filesystem
  uint64_t total_size = sd_card_get_total_size();
  
  if (total_size == 0) {
    return "unknown";
  }
  
  // Try to detect actual filesystem by checking mount parameters
  // FATFS with exFAT support (CONFIG_FATFS_FS_EXFAT=y) can mount both FAT32 and exFAT
  // Cards > 32GB are typically formatted as exFAT
  // Cards 2GB-32GB are typically FAT32
  // Cards < 2GB may be FAT16 or FAT32
  
  if (total_size > 32ULL * 1024 * 1024 * 1024) {
    // Large card (64GB) - almost certainly exFAT
    // ESP-IDF FATFS with CONFIG_FATFS_FS_EXFAT=y supports exFAT natively
    return "exFAT";
  } else if (total_size > 2ULL * 1024 * 1024 * 1024) {
    // Medium card (2GB-32GB) - typically FAT32
    return "FAT32";
  } else {
    // Small card (< 2GB) - FAT16 or FAT32
    return "FAT16/FAT32";
  }
}

