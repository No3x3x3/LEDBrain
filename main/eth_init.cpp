#include "eth_init.hpp"
#include "esp_eth.h"
#include "esp_eth_mac.h"
#include "esp_eth_phy.h"
#include "esp_eth_netif_glue.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "soc/soc_caps.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/ip4_addr.h"

static const char* TAG = "eth";
esp_netif_t* netif = nullptr;
static esp_eth_netif_glue_handle_t eth_glue = nullptr;
static bool event_handlers_registered = false;

namespace {

constexpr gpio_num_t PHY_RESET_GPIO = GPIO_NUM_51;
constexpr gpio_num_t PHY_POWER_GPIO = GPIO_NUM_NC;
constexpr int PHY_ADDR = 1;
constexpr int RMII_TX_EN_GPIO = 49;
constexpr int RMII_TXD0_GPIO = 34;
constexpr int RMII_TXD1_GPIO = 35;
constexpr int RMII_CRS_DV_GPIO = 28;
constexpr int RMII_RXD0_GPIO = 29;
constexpr int RMII_RXD1_GPIO = 30;
constexpr int RMII_CLK_IN_GPIO_NUM = 50;

static uint64_t gpio_mask(gpio_num_t gpio) {
  if (gpio < 0) {
    return 0;
  }
  const auto idx = static_cast<uint32_t>(gpio);
  if (idx >= 64) {
    return 0;
  }
  return 1ULL << idx;
}

void configure_static_ip(const NetworkConfig& cfg) {
  esp_netif_ip_info_t ip_info{};
  ESP_ERROR_CHECK(esp_netif_str_to_ip4(cfg.static_ip.c_str(), &ip_info.ip));
  ESP_ERROR_CHECK(esp_netif_str_to_ip4(cfg.netmask.c_str(), &ip_info.netmask));
  ESP_ERROR_CHECK(esp_netif_str_to_ip4(cfg.gateway.c_str(), &ip_info.gw));
  ESP_ERROR_CHECK(esp_netif_set_ip_info(netif, &ip_info));

  if (!cfg.dns.empty()) {
    esp_netif_dns_info_t dns{};
    ESP_ERROR_CHECK(esp_netif_str_to_ip4(cfg.dns.c_str(), &dns.ip.u_addr.ip4));
    ESP_ERROR_CHECK(esp_netif_set_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns));
  }
}

void ensure_phy_powered() {
  if (PHY_POWER_GPIO != GPIO_NUM_NC) {
    gpio_config_t cfg{};
    cfg.pin_bit_mask = gpio_mask(PHY_POWER_GPIO);
    cfg.mode = GPIO_MODE_OUTPUT;
    cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = GPIO_INTR_DISABLE;
    if (cfg.pin_bit_mask) {
      ESP_ERROR_CHECK(gpio_config(&cfg));
      gpio_set_level(PHY_POWER_GPIO, 1);
      vTaskDelay(pdMS_TO_TICKS(50));
    } else {
      ESP_LOGW(TAG, "PHY power GPIO %d outside mask range, skipped", static_cast<int>(PHY_POWER_GPIO));
    }
  }

  if (PHY_RESET_GPIO != GPIO_NUM_NC) {
    gpio_config_t cfg{};
    cfg.pin_bit_mask = gpio_mask(PHY_RESET_GPIO);
    cfg.mode = GPIO_MODE_OUTPUT;
    cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    cfg.intr_type = GPIO_INTR_DISABLE;
    if (cfg.pin_bit_mask) {
      ESP_ERROR_CHECK(gpio_config(&cfg));
      gpio_set_level(PHY_RESET_GPIO, 0);
      vTaskDelay(pdMS_TO_TICKS(50));
      gpio_set_level(PHY_RESET_GPIO, 1);
      vTaskDelay(pdMS_TO_TICKS(50));
    } else {
      ESP_LOGW(TAG, "PHY reset GPIO %d outside mask range, skipped", static_cast<int>(PHY_RESET_GPIO));
    }
  }
}

void log_eth_event(void*, esp_event_base_t, int32_t event_id, void* event_data) {
  switch (event_id) {
    case ETHERNET_EVENT_CONNECTED: {
      uint8_t mac_addr[6] = {0};
      auto handle = *static_cast<esp_eth_handle_t*>(event_data);
      if (esp_eth_ioctl(handle, ETH_CMD_G_MAC_ADDR, mac_addr) == ESP_OK) {
        ESP_LOGI(TAG, "Ethernet link up, MAC %02X:%02X:%02X:%02X:%02X:%02X",
                 mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
      } else {
        ESP_LOGI(TAG, "Ethernet link up");
      }
      break;
    }
    case ETHERNET_EVENT_DISCONNECTED:
      ESP_LOGW(TAG, "Ethernet link down");
      break;
    case ETHERNET_EVENT_START:
      ESP_LOGI(TAG, "Ethernet driver started");
      break;
    case ETHERNET_EVENT_STOP:
      ESP_LOGI(TAG, "Ethernet driver stopped");
      break;
    default:
      break;
  }
}

void log_ip_event(void*, esp_event_base_t, int32_t event_id, void* event_data) {
  if (event_id == IP_EVENT_ETH_GOT_IP) {
    auto* event = static_cast<ip_event_got_ip_t*>(event_data);
    const auto& info = event->ip_info;
    ESP_LOGI(TAG, "Ethernet got IP " IPSTR " mask " IPSTR " gw " IPSTR,
             IP2STR(&info.ip), IP2STR(&info.netmask), IP2STR(&info.gw));
  }
}

}  // namespace

esp_err_t ethernet_start(const NetworkConfig& cfg) {
  esp_eth_mac_t* mac = nullptr;
  esp_eth_phy_t* phy = nullptr;
  esp_eth_handle_t eth_handle = nullptr;
  esp_eth_netif_glue_handle_t glue = nullptr;
  esp_err_t result = ESP_OK;
  bool driver_installed = false;
  bool started = false;
  bool netif_created = false;

  if (!netif) {
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    netif = esp_netif_new(&netif_cfg);
    if (!netif) {
      ESP_LOGE(TAG, "esp_netif_new failed");
      return ESP_FAIL;
    }
    netif_created = true;
  }

  eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
  eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
  phy_config.phy_addr = PHY_ADDR;
  phy_config.reset_gpio_num = PHY_RESET_GPIO;

  eth_esp32_emac_config_t emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
  emac_config.smi_gpio.mdc_num = 31;
  emac_config.smi_gpio.mdio_num = 52;
  emac_config.interface = EMAC_DATA_INTERFACE_RMII;
  emac_config.clock_config.rmii.clock_mode = EMAC_CLK_EXT_IN;
  emac_config.clock_config.rmii.clock_gpio = RMII_CLK_IN_GPIO_NUM;
#if SOC_EMAC_USE_MULTI_IO_MUX || SOC_EMAC_MII_USE_GPIO_MATRIX
  emac_config.emac_dataif_gpio.rmii.tx_en_num = RMII_TX_EN_GPIO;
  emac_config.emac_dataif_gpio.rmii.txd0_num = RMII_TXD0_GPIO;
  emac_config.emac_dataif_gpio.rmii.txd1_num = RMII_TXD1_GPIO;
  emac_config.emac_dataif_gpio.rmii.crs_dv_num = RMII_CRS_DV_GPIO;
  emac_config.emac_dataif_gpio.rmii.rxd0_num = RMII_RXD0_GPIO;
  emac_config.emac_dataif_gpio.rmii.rxd1_num = RMII_RXD1_GPIO;
#endif

  ensure_phy_powered();

  mac = esp_eth_mac_new_esp32(&emac_config, &mac_config);
  ESP_RETURN_ON_FALSE(mac, ESP_FAIL, TAG, "mac init failed");

  phy = esp_eth_phy_new_ip101(&phy_config);
  if (!phy) {
    ESP_LOGE(TAG, "phy init failed");
    mac->del(mac);
    mac = nullptr;
    return ESP_FAIL;
  }

  esp_eth_config_t eth_config = ETH_DEFAULT_CONFIG(mac, phy);
  result = esp_eth_driver_install(&eth_config, &eth_handle);
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "esp_eth_driver_install failed: %s", esp_err_to_name(result));
    goto cleanup;
  }
  driver_installed = true;

  glue = esp_eth_new_netif_glue(eth_handle);
  result = esp_netif_attach(netif, glue);
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "esp_netif_attach failed: %s", esp_err_to_name(result));
    if (glue) {
      esp_eth_del_netif_glue(glue);
      glue = nullptr;
    }
    goto cleanup;
  }
  result = esp_netif_set_default_netif(netif);
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "set_default_netif failed: %s", esp_err_to_name(result));
    if (glue) {
      esp_eth_del_netif_glue(glue);
      glue = nullptr;
    }
    goto cleanup;
  }
  eth_glue = glue;

  if (!event_handlers_registered) {
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &log_eth_event, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &log_ip_event, nullptr));
    event_handlers_registered = true;
  }

  result = esp_eth_start(eth_handle);
  if (result != ESP_OK) {
    ESP_LOGE(TAG, "esp_eth_start failed: %s", esp_err_to_name(result));
    goto cleanup;
  }
  started = true;

  if (!cfg.hostname.empty()) {
    ESP_ERROR_CHECK(esp_netif_set_hostname(netif, cfg.hostname.c_str()));
  }

  if (cfg.use_dhcp) {
    ESP_LOGI(TAG, "DHCP enabled");
    esp_netif_dhcp_status_t dhcp_status = ESP_NETIF_DHCP_STOPPED;
    if (esp_netif_dhcpc_get_status(netif, &dhcp_status) == ESP_OK &&
        dhcp_status != ESP_NETIF_DHCP_STARTED) {
      ESP_ERROR_CHECK(esp_netif_dhcpc_start(netif));
    }
  } else if (!cfg.static_ip.empty()) {
    ESP_LOGI(TAG, "Static IP %s", cfg.static_ip.c_str());
    esp_netif_dhcp_status_t dhcp_status = ESP_NETIF_DHCP_STOPPED;
    if (esp_netif_dhcpc_get_status(netif, &dhcp_status) == ESP_OK &&
        dhcp_status == ESP_NETIF_DHCP_STARTED) {
      ESP_ERROR_CHECK(esp_netif_dhcpc_stop(netif));
    }
    configure_static_ip(cfg);
  }

  return ESP_OK;

cleanup:
  if (started && eth_handle) {
    esp_eth_stop(eth_handle);
  }
  if (driver_installed && eth_handle) {
    esp_eth_driver_uninstall(eth_handle);
    eth_handle = nullptr;
  }
  if (mac) {
    mac->del(mac);
  }
  if (phy) {
    phy->del(phy);
  }
  if (eth_glue) {
    esp_eth_del_netif_glue(eth_glue);
    eth_glue = nullptr;
  }
  if (PHY_POWER_GPIO != GPIO_NUM_NC) {
    gpio_set_level(PHY_POWER_GPIO, 0);
  }
  if (netif_created && netif) {
    esp_netif_destroy(netif);
    netif = nullptr;
  }
  return result;
}
