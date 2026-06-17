#pragma once
#include <stdint.h>

typedef enum {
    MOOD_NEUTRAL = 0,
    MOOD_HAPPY,
    MOOD_SAD,
    MOOD_EXCITED,
    MOOD_CONFUSED,
    MOOD_SLEEPY,
    MOOD_SCARED,
    MOOD_THINKING,
    MOOD_COUNT
} mood_t;

void    mood_engine_init(void);
void    mood_engine_set(mood_t mood);
mood_t  mood_engine_get(void);
const char *mood_engine_name(mood_t mood);
