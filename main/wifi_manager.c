/**
 * WiFi Manager — BLE provisioning on first boot, stored creds on subsequent boots
 */
#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "wifi";
static EventGroupHandle_t s_ev_group;
static EventBits_t        s_ready_bit;

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&ev->ip_info.ip));
        xEventGroupSetBits(s_ev_group, s_ready_bit);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Disconnected — retrying…");
        esp_wifi_connect();
    } else if (base == WIFI_PROV_EVENT && id == WIFI_PROV_CRED_RECV) {
        ESP_LOGI(TAG, "Provisioning credentials received");
    }
}

esp_err_t wifi_manager_init(EventGroupHandle_t ev_group, EventBits_t ready_bit) {
    s_ev_group  = ev_group;
    s_ready_bit = ready_bit;

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_register(WIFI_EVENT,      ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT,        IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    /* Check for stored credentials */
    wifi_config_t stored = {0};
    esp_err_t err = esp_wifi_get_config(WIFI_IF_STA, &stored);
    bool has_creds = (err == ESP_OK && strlen((char *)stored.sta.ssid) > 0);

    if (has_creds) {
        ESP_LOGI(TAG, "Connecting to saved SSID: %s", stored.sta.ssid);
        esp_wifi_connect();
    } else {
        ESP_LOGI(TAG, "No WiFi credentials — starting BLE provisioning");
        wifi_prov_mgr_config_t prov_cfg = {
            .scheme = wifi_prov_scheme_ble,
            .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
        };
        wifi_prov_mgr_init(prov_cfg);
        wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, "popo_prov", "POPO-0000", NULL);
    }

    return ESP_OK;
}

bool wifi_manager_is_connected(void) {
    return (xEventGroupGetBits(s_ev_group) & s_ready_bit) != 0;
}

esp_err_t wifi_manager_get_ip(char *buf, size_t len) {
    esp_netif_ip_info_t info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) return ESP_FAIL;
    esp_netif_get_ip_info(netif, &info);
    snprintf(buf, len, IPSTR, IP2STR(&info.ip));
    return ESP_OK;
}
