#pragma once
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

esp_err_t wake_word_init(EventGroupHandle_t ev_group, EventBits_t wake_bit);
void      wake_word_suspend(void);
void      wake_word_resume(void);
