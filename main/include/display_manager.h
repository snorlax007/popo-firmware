#pragma once
#include "mood_engine.h"

typedef enum {
    DISP_CLOCK = 0,
    DISP_WEATHER,
    DISP_FOCUS_TIMER,
    DISP_MOOD,
    DISP_NOTIFICATION,
} display_state_t;

typedef struct {
    float  temp_c;
    int    humidity;
    char   condition[32];
    char   icon_code[8];
    char   city[32];
} weather_data_t;

esp_err_t display_manager_init(void);
void display_manager_show_boot(void);
void display_manager_set_state(display_state_t state);
void display_manager_sync_mood(mood_t mood);
void display_manager_update_weather(const weather_data_t *wx);
void display_manager_show_focus_timer(uint32_t remaining_sec);
