/**
 * Wake Word — ESP-SR WakeNet "Hey Popo"
 * Runs on separate task, sets event bit when triggered.
 */
#include "wake_word.h"
#include "esp_log.h"
#include "esp_wn_iface.h"
#include "esp_wn_models.h"
#include "driver/i2s_std.h"
#include <string.h>

static const char *TAG = "wake";

#define WW_SAMPLE_RATE  16000
#define WW_CHUNK_MS     30
#define WW_CHUNK_FRAMES (WW_SAMPLE_RATE * WW_CHUNK_MS / 1000)

static EventGroupHandle_t s_ev_group;
static EventBits_t        s_wake_bit;
static esp_wn_iface_t    *s_model;
static model_iface_data_t *s_model_data;
static TaskHandle_t       s_task;
static i2s_chan_handle_t  s_ww_rx;
static volatile bool      s_suspended = false;

static void wake_task(void *arg) {
    int16_t *buf = malloc(WW_CHUNK_FRAMES * sizeof(int16_t));
    size_t bytes;

    while (1) {
        if (s_suspended) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }

        i2s_channel_read(s_ww_rx, buf, WW_CHUNK_FRAMES * sizeof(int16_t), &bytes, pdMS_TO_TICKS(200));

        int r = s_model->detect(s_model_data, buf);
        if (r > 0) {
            ESP_LOGI(TAG, "Wake word detected (score=%d)", r);
            xEventGroupSetBits(s_ev_group, s_wake_bit);
            vTaskDelay(pdMS_TO_TICKS(1500)); /* debounce */
        }
    }
    free(buf);
}

esp_err_t wake_word_init(EventGroupHandle_t ev_group, EventBits_t wake_bit) {
    s_ev_group = ev_group;
    s_wake_bit = wake_bit;

    s_model      = (esp_wn_iface_t *)esp_wn_handle_from_name("wn9_hilexin");
    s_model_data = s_model->create(NULL, DET_MODE_90);

    /* Dedicated I2S channel for wake word (same mic, separate channel) */
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, NULL, &s_ww_rx);
    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(WW_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PCM_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = GPIO_NUM_38,
            .ws   = GPIO_NUM_39,
            .dout = I2S_GPIO_UNUSED,
            .din  = GPIO_NUM_40,
        }
    };
    i2s_channel_init_std_mode(s_ww_rx, &std_cfg);
    i2s_channel_enable(s_ww_rx);

    xTaskCreate(wake_task, "wake_word", 4096, NULL, 7, &s_task);
    ESP_LOGI(TAG, "Wake word engine started");
    return ESP_OK;
}

void wake_word_suspend(void) { s_suspended = true; }
void wake_word_resume(void)  { s_suspended = false; }
