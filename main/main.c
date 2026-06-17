/**
 * Popo AI Desk Companion — Main Entry Point
 * ESP32-S3 / ESP-IDF v5.x
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "display_manager.h"
#include "oled_face.h"
#include "audio_manager.h"
#include "wifi_manager.h"
#include "weather_client.h"
#include "mood_engine.h"
#include "popo_tts.h"
#include "wake_word.h"
#include "led_ring.h"

static const char *TAG = "popo_main";

static EventGroupHandle_t s_app_events;
#define EV_WIFI_READY    BIT0
#define EV_WAKE_WORD     BIT1
#define EV_AUDIO_DONE    BIT2
#define EV_MOOD_CHANGE   BIT3

/* Forward declarations */
static void conversation_task(void *arg);
static void display_task(void *arg);
static void weather_task(void *arg);

void app_main(void) {
    ESP_LOGI(TAG, "Popo v1.0 starting up…");

    /* NVS init */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    s_app_events = xEventGroupCreate();

    /* Boot animation */
    display_manager_init();
    oled_face_init();
    led_ring_init();
    display_manager_show_boot();
    oled_face_set(FACE_NEUTRAL);
    led_ring_breathe(LED_COLOR_PURPLE, 3);

    /* Audio subsystem */
    audio_manager_init();
    popo_tts_init();

    /* WiFi — BLE provisioned on first boot */
    wifi_manager_init(s_app_events, EV_WIFI_READY);
    xEventGroupWaitBits(s_app_events, EV_WIFI_READY, pdFALSE, pdTRUE, pdMS_TO_TICKS(30000));

    /* Wake word detector */
    wake_word_init(s_app_events, EV_WAKE_WORD);

    /* Spawn tasks */
    xTaskCreate(conversation_task, "conv",  8192, NULL, 5, NULL);
    xTaskCreate(display_task,      "disp",  4096, NULL, 4, NULL);
    xTaskCreate(weather_task,      "wthr",  4096, NULL, 3, NULL);

    ESP_LOGI(TAG, "All tasks launched. Popo ready! 😊");
}

/**
 * Main conversation loop:
 * Wake word → capture audio → STT → intent/mood → TTS reply → idle
 */
static void conversation_task(void *arg) {
    while (1) {
        /* Wait for wake word */
        xEventGroupWaitBits(s_app_events, EV_WAKE_WORD, pdTRUE, pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG, "Wake word detected!");

        mood_engine_set(MOOD_THINKING);
        led_ring_spin(LED_COLOR_PURPLE);

        /* Capture utterance */
        audio_buffer_t buf = {0};
        audio_manager_record(&buf, 5000);  /* max 5s */

        /* STT via cloud */
        char transcript[256] = {0};
        esp_err_t err = audio_manager_transcribe(&buf, transcript, sizeof(transcript));
        if (err != ESP_OK || strlen(transcript) == 0) {
            mood_engine_set(MOOD_CONFUSED);
            popo_tts_play_mood(MOOD_CONFUSED, TTS_SHORT);
            continue;
        }
        ESP_LOGI(TAG, "Heard: %s", transcript);

        /* Intent + mood from backend */
        popo_intent_t intent = {0};
        weather_client_post_intent(transcript, &intent);

        mood_engine_set(intent.mood);
        led_ring_set_mood(intent.mood);
        popo_tts_play_mood(intent.mood, TTS_MEDIUM);

        /* Return to idle */
        vTaskDelay(pdMS_TO_TICKS(2000));
        mood_engine_set(MOOD_NEUTRAL);
        led_ring_breathe(LED_COLOR_WHITE, 0);
    }
}

/**
 * Display update task — clock, weather, mood face sync
 */
static void display_task(void *arg) {
    display_state_t state = DISP_CLOCK;
    TickType_t last_switch = xTaskGetTickCount();

    while (1) {
        mood_t current_mood = mood_engine_get();
        display_manager_sync_mood(current_mood);
        oled_face_set_from_mood(current_mood);

        /* Rotate display every 30s: clock → weather → clock */
        if ((xTaskGetTickCount() - last_switch) > pdMS_TO_TICKS(30000)) {
            state = (state == DISP_CLOCK) ? DISP_WEATHER : DISP_CLOCK;
            display_manager_set_state(state);
            last_switch = xTaskGetTickCount();
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/**
 * Weather refresh task — every 10 minutes
 */
static void weather_task(void *arg) {
    while (1) {
        weather_data_t wx = {0};
        if (weather_client_fetch(&wx) == ESP_OK) {
            display_manager_update_weather(&wx);
            ESP_LOGI(TAG, "Weather: %s %.1f°C", wx.condition, wx.temp_c);
        }
        vTaskDelay(pdMS_TO_TICKS(10 * 60 * 1000));
    }
}
