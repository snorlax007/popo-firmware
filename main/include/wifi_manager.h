#pragma once
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

esp_err_t wifi_manager_init(EventGroupHandle_t ev_group, EventBits_t ready_bit);
bool      wifi_manager_is_connected(void);
esp_err_t wifi_manager_get_ip(char *buf, size_t len);
