#pragma once
#include "mood_engine.h"

typedef enum {
    FACE_NEUTRAL = MOOD_NEUTRAL,
    FACE_HAPPY   = MOOD_HAPPY,
    FACE_SAD     = MOOD_SAD,
    FACE_EXCITED = MOOD_EXCITED,
    FACE_CONFUSED= MOOD_CONFUSED,
    FACE_SLEEPY  = MOOD_SLEEPY,
    FACE_SCARED  = MOOD_SCARED,
    FACE_THINKING= MOOD_THINKING,
} face_t;

esp_err_t oled_face_init(void);
void oled_face_set(face_t face);
void oled_face_set_from_mood(mood_t mood);
void oled_face_blink(void);
