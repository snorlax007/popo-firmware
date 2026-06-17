#pragma once
#include "mood_engine.h"

typedef enum { TTS_SHORT = 0, TTS_MEDIUM, TTS_LONG } tts_length_t;

esp_err_t popo_tts_init(void);
esp_err_t popo_tts_play_mood(mood_t mood, tts_length_t length);
esp_err_t popo_tts_stop(void);
