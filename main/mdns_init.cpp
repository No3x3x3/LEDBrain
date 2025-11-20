#include "mdns.h"
#include "esp_log.h"

static const char *TAG = "mDNS";

bool mdns_start(const char* hostname)
{
    ESP_LOGI(TAG, "Inicjalizacja mDNS...");

    if (mdns_init() != ESP_OK) {
        ESP_LOGE(TAG, "Nie udało się zainicjalizować mDNS");
        return false;
    }

    mdns_hostname_set(hostname);
    mdns_instance_name_set("LED Brain Controller");

    mdns_service_add("LEDBrain Web", "_http", "_tcp", 80, nullptr, 0);

    ESP_LOGI(TAG, "mDNS uruchomiony jako %s.local", hostname);
    return true;
}
