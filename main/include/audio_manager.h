#pragma once
#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int16_t *samples;
    size_t   count;
    uint32_t sample_rate;
} audio_buffer_t;

esp_err_t audio_manager_init(void);
esp_err_t audio_manager_record(audio_buffer_t *buf, uint32_t max_ms);
esp_err_t audio_manager_transcribe(const audio_buffer_t *buf, char *out, size_t out_len);
void      audio_manager_free(audio_buffer_t *buf);
