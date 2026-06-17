/**
 * Popo TTS — Phoneme sequencer
 * Chains pre-recorded Popish phoneme clips based on mood + desired length.
 * Clips stored in SPIFFS: /spiffs/tts/<mood>/<index>.wav
 */
#include "popo_tts.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "driver/i2s_std.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "popo_tts";

/* Number of phoneme clips per mood */
static const int CLIPS_PER_MOOD[MOOD_COUNT] = { 5, 8, 6, 8, 6, 5, 5, 6 };
static const char *MOOD_DIRS[MOOD_COUNT] = {
    "neutral","happy","sad","excited","confused","sleepy","scared","thinking"
};

/* How many clips to chain per length */
static const int CHAIN_LEN[3] = { 2, 4, 6 };

static i2s_chan_handle_t s_tx_chan;

esp_err_t popo_tts_init(void) {
    esp_vfs_spiffs_conf_t cfg = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 8,
        .format_if_mount_failed = false
    };
    esp_err_t ret = esp_vfs_spiffs_register(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "SPIFFS mount failed (%s) — TTS unavailable until phoneme assets are loaded", esp_err_to_name(ret));
        return ret;
    }

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, &s_tx_chan, NULL);

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(44100),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = GPIO_NUM_15,
            .ws   = GPIO_NUM_16,
            .dout = GPIO_NUM_17,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { .mclk_inv=false, .bclk_inv=false, .ws_inv=false }
        }
    };
    i2s_channel_init_std_mode(s_tx_chan, &std_cfg);
    i2s_channel_enable(s_tx_chan);

    ESP_LOGI(TAG, "TTS engine ready");
    return ESP_OK;
}

static esp_err_t play_clip(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGW(TAG, "Clip not found: %s", path);
        return ESP_ERR_NOT_FOUND;
    }
    /* Skip WAV header (44 bytes) */
    fseek(f, 44, SEEK_SET);

    uint8_t buf[512];
    size_t read, written;
    while ((read = fread(buf, 1, sizeof(buf), f)) > 0) {
        i2s_channel_write(s_tx_chan, buf, read, &written, pdMS_TO_TICKS(500));
    }
    fclose(f);
    return ESP_OK;
}

esp_err_t popo_tts_play_mood(mood_t mood, tts_length_t length) {
    if (mood >= MOOD_COUNT) return ESP_ERR_INVALID_ARG;
    int clips = CLIPS_PER_MOOD[mood];
    int chain = CHAIN_LEN[length];
    char path[64];

    ESP_LOGI(TAG, "Playing %s × %d clips", MOOD_DIRS[mood], chain);
    for (int i = 0; i < chain; i++) {
        int idx = (rand() % clips) + 1;
        snprintf(path, sizeof(path), "/spiffs/tts/%s/%02d.wav", MOOD_DIRS[mood], idx);
        play_clip(path);
        vTaskDelay(pdMS_TO_TICKS(80));
    }
    return ESP_OK;
}

esp_err_t popo_tts_stop(void) {
    i2s_channel_disable(s_tx_chan);
    i2s_channel_enable(s_tx_chan);
    return ESP_OK;
}
