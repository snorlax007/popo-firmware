/**
 * Audio Manager — INMP441 I2S mic capture + Whisper STT via HTTPS
 * Records 16kHz mono PCM, WAV-encodes in memory, POSTs to backend /api/transcribe
 */
#include "audio_manager.h"
#include "esp_log.h"
#include "driver/i2s_std.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const char *TAG = "audio";

#define I2S_MIC_PORT   I2S_NUM_1
#define MIC_BCLK       GPIO_NUM_38
#define MIC_WS         GPIO_NUM_39
#define MIC_DIN        GPIO_NUM_40
#define SAMPLE_RATE    16000
#define VAD_THRESHOLD  400   /* amplitude threshold for voice activity */
#define SILENCE_MS     800   /* ms of silence before stopping capture */

static i2s_chan_handle_t s_rx_chan;

esp_err_t audio_manager_init(void) {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_MIC_PORT, I2S_ROLE_MASTER);
    i2s_new_channel(&chan_cfg, NULL, &s_rx_chan);

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PCM_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = MIC_BCLK,
            .ws   = MIC_WS,
            .dout = I2S_GPIO_UNUSED,
            .din  = MIC_DIN,
        }
    };
    i2s_channel_init_std_mode(s_rx_chan, &std_cfg);
    i2s_channel_enable(s_rx_chan);

    ESP_LOGI(TAG, "Audio manager ready @ %dHz", SAMPLE_RATE);
    return ESP_OK;
}

static int16_t rms(const int16_t *buf, size_t n) {
    int64_t sum = 0;
    for (size_t i = 0; i < n; i++) sum += (int64_t)buf[i] * buf[i];
    return (int16_t)sqrtf((float)(sum / n));
}

esp_err_t audio_manager_record(audio_buffer_t *buf, uint32_t max_ms) {
    const int CHUNK = 256;
    size_t max_samples = (SAMPLE_RATE * max_ms) / 1000;
    buf->samples = malloc(max_samples * sizeof(int16_t));
    if (!buf->samples) return ESP_ERR_NO_MEM;
    buf->sample_rate = SAMPLE_RATE;
    buf->count = 0;

    int16_t chunk[CHUNK];
    size_t bytes_read;
    uint32_t silence_ms = 0;

    while (buf->count < max_samples) {
        i2s_channel_read(s_rx_chan, chunk, CHUNK * sizeof(int16_t), &bytes_read, pdMS_TO_TICKS(100));
        size_t n = bytes_read / sizeof(int16_t);

        memcpy(buf->samples + buf->count, chunk, n * sizeof(int16_t));
        buf->count += n;

        int16_t level = rms(chunk, n);
        if (level < VAD_THRESHOLD) {
            silence_ms += (n * 1000) / SAMPLE_RATE;
            if (silence_ms >= SILENCE_MS && buf->count > SAMPLE_RATE / 2) break;
        } else {
            silence_ms = 0;
        }
    }

    ESP_LOGI(TAG, "Recorded %zu samples (%.1fs)", buf->count, (float)buf->count / SAMPLE_RATE);
    return ESP_OK;
}

/* Build minimal WAV header in-place before the PCM data */
static void prepend_wav_header(uint8_t *buf, size_t pcm_bytes, uint32_t rate) {
    uint32_t data_size  = pcm_bytes;
    uint32_t riff_size  = 36 + data_size;
    uint16_t channels   = 1;
    uint16_t bits       = 16;
    uint32_t byte_rate  = rate * channels * bits / 8;
    uint16_t block_align = channels * bits / 8;

    memcpy(buf,      "RIFF", 4);
    memcpy(buf+4,    &riff_size, 4);
    memcpy(buf+8,    "WAVE", 4);
    memcpy(buf+12,   "fmt ", 4);
    uint32_t chunk16 = 16; memcpy(buf+16, &chunk16, 4);
    uint16_t pcm_fmt = 1;  memcpy(buf+20, &pcm_fmt, 2);
    memcpy(buf+22,   &channels, 2);
    memcpy(buf+24,   &rate, 4);
    memcpy(buf+28,   &byte_rate, 4);
    memcpy(buf+32,   &block_align, 2);
    memcpy(buf+34,   &bits, 2);
    memcpy(buf+36,   "data", 4);
    memcpy(buf+40,   &data_size, 4);
}

static char s_stt_resp[256];
static int  s_stt_len = 0;

static esp_err_t stt_event_cb(esp_http_client_event_t *evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        int remain = sizeof(s_stt_resp) - s_stt_len - 1;
        if (remain > 0) {
            memcpy(s_stt_resp + s_stt_len, evt->data, MIN(evt->data_len, remain));
            s_stt_len += MIN(evt->data_len, remain);
            s_stt_resp[s_stt_len] = '\0';
        }
    }
    return ESP_OK;
}

extern const char *CONFIG_POPO_BACKEND_URL;

esp_err_t audio_manager_transcribe(const audio_buffer_t *buf, char *out, size_t out_len) {
    size_t pcm_bytes = buf->count * sizeof(int16_t);
    size_t wav_bytes = 44 + pcm_bytes;

    uint8_t *wav = malloc(wav_bytes);
    if (!wav) return ESP_ERR_NO_MEM;
    prepend_wav_header(wav, pcm_bytes, buf->sample_rate);
    memcpy(wav + 44, buf->samples, pcm_bytes);

    char url[128];
    snprintf(url, sizeof(url), "%s/api/transcribe", CONFIG_POPO_BACKEND_URL);

    s_stt_len = 0;
    esp_http_client_config_t cfg = { .url = url, .method = HTTP_METHOD_POST, .event_handler = stt_event_cb };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "Content-Type", "audio/wav");
    esp_http_client_set_post_field(client, (const char *)wav, wav_bytes);

    esp_err_t err = esp_http_client_perform(client);
    esp_http_client_cleanup(client);
    free(wav);

    if (err != ESP_OK) return err;

    cJSON *root = cJSON_Parse(s_stt_resp);
    if (!root) return ESP_FAIL;
    const char *text = cJSON_GetObjectItem(root, "text")->valuestring;
    if (text) strncpy(out, text, out_len - 1);
    cJSON_Delete(root);
    return ESP_OK;
}

void audio_manager_free(audio_buffer_t *buf) {
    free(buf->samples);
    buf->samples = NULL;
    buf->count = 0;
}
