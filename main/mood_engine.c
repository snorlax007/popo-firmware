#include "mood_engine.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "mood";
static mood_t s_mood = MOOD_NEUTRAL;
static SemaphoreHandle_t s_lock;

static const char *MOOD_NAMES[MOOD_COUNT] = {
    "neutral","happy","sad","excited","confused","sleepy","scared","thinking"
};

void mood_engine_init(void) {
    s_lock = xSemaphoreCreateMutex();
}

void mood_engine_set(mood_t mood) {
    if (mood >= MOOD_COUNT) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_mood != mood) {
        ESP_LOGI(TAG, "Mood: %s → %s", MOOD_NAMES[s_mood], MOOD_NAMES[mood]);
        s_mood = mood;
    }
    xSemaphoreGive(s_lock);
}

mood_t mood_engine_get(void) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    mood_t m = s_mood;
    xSemaphoreGive(s_lock);
    return m;
}

const char *mood_engine_name(mood_t mood) {
    if (mood >= MOOD_COUNT) return "unknown";
    return MOOD_NAMES[mood];
}
