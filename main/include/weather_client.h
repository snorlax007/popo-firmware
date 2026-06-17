#pragma once
#include "esp_err.h"
#include "display_manager.h"

typedef struct {
    char  intent[32];
    char  sentiment[16];
    int   mood;        /* maps to mood_t */
    char  popo_reply[128];
} popo_intent_t;

esp_err_t weather_client_fetch(weather_data_t *out);
esp_err_t weather_client_post_intent(const char *transcript, popo_intent_t *out);
