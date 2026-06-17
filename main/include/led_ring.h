#pragma once
#include "esp_err.h"
#include "mood_engine.h"

typedef enum {
    LED_COLOR_WHITE  = 0xFFFFFF,
    LED_COLOR_PURPLE = 0x7C3AED,
    LED_COLOR_AMBER  = 0xF59E0B,
    LED_COLOR_BLUE   = 0x3B82F6,
    LED_COLOR_GREEN  = 0x22C55E,
    LED_COLOR_RED    = 0xEF4444,
} led_color_t;

esp_err_t led_ring_init(void);
void led_ring_set(uint32_t rgb);
void led_ring_breathe(led_color_t color, int repeats);  /* 0 = infinite */
void led_ring_spin(led_color_t color);
void led_ring_set_mood(mood_t mood);
void led_ring_off(void);
