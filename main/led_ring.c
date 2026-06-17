/**
 * LED Ring — WS2812B 12-LED ring via ESP32 RMT peripheral
 */
#include "led_ring.h"
#include "driver/rmt_tx.h"
#include "led_strip_encoder.h"
#include "esp_log.h"
#include <math.h>
#include <string.h>

static const char *TAG = "leds";

#define LED_GPIO       GPIO_NUM_48
#define LED_COUNT      12
#define RMT_RESOLUTION 10000000  /* 10MHz */

static rmt_channel_handle_t s_chan;
static rmt_encoder_handle_t s_encoder;
static uint8_t s_pixels[LED_COUNT * 3];  /* GRB order */

static const uint32_t MOOD_RGB[MOOD_COUNT] = {
    0x7C3AED,  /* neutral  */
    0xF5A623,  /* happy    */
    0x3B82F6,  /* sad      */
    0x22C55E,  /* excited  */
    0xF59E0B,  /* confused */
    0x6B7280,  /* sleepy   */
    0xEF4444,  /* scared   */
    0xA855F7,  /* thinking */
};

static void pixels_commit(void) {
    rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
    rmt_transmit(s_chan, s_encoder, s_pixels, sizeof(s_pixels), &tx_cfg);
    rmt_tx_wait_all_done(s_chan, pdMS_TO_TICKS(100));
}

static void set_all(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < LED_COUNT; i++) {
        s_pixels[i*3+0] = g;  /* WS2812B is GRB */
        s_pixels[i*3+1] = r;
        s_pixels[i*3+2] = b;
    }
}

esp_err_t led_ring_init(void) {
    rmt_tx_channel_config_t tx_cfg = {
        .gpio_num = LED_GPIO,
        .clk_src  = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_RESOLUTION,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };
    rmt_new_tx_channel(&tx_cfg, &s_chan);

    led_strip_encoder_config_t enc_cfg = { .resolution = RMT_RESOLUTION };
    rmt_new_led_strip_encoder(&enc_cfg, &s_encoder);

    rmt_enable(s_chan);

    set_all(0, 0, 0);
    pixels_commit();
    ESP_LOGI(TAG, "LED ring ready (%d LEDs)", LED_COUNT);
    return ESP_OK;
}

void led_ring_set(uint32_t rgb) {
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >>  8) & 0xFF;
    uint8_t b = (rgb      ) & 0xFF;
    set_all(r, g, b);
    pixels_commit();
}

void led_ring_breathe(led_color_t color, int repeats) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >>  8) & 0xFF;
    uint8_t b = (color      ) & 0xFF;
    int count = 0;
    while (repeats == 0 || count < repeats) {
        for (int i = 0; i <= 100; i++) {
            float scale = sinf(i * 3.14f / 100.0f);
            set_all((uint8_t)(r*scale), (uint8_t)(g*scale), (uint8_t)(b*scale));
            pixels_commit();
            vTaskDelay(pdMS_TO_TICKS(15));
        }
        count++;
    }
}

void led_ring_spin(led_color_t color) {
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >>  8) & 0xFF;
    uint8_t b = (color      ) & 0xFF;
    for (int frame = 0; frame < LED_COUNT * 3; frame++) {
        memset(s_pixels, 0, sizeof(s_pixels));
        int lit = frame % LED_COUNT;
        s_pixels[lit*3+0] = g;
        s_pixels[lit*3+1] = r;
        s_pixels[lit*3+2] = b;
        /* Trailing fade */
        int trail = (lit - 1 + LED_COUNT) % LED_COUNT;
        s_pixels[trail*3+0] = g/4; s_pixels[trail*3+1] = r/4; s_pixels[trail*3+2] = b/4;
        pixels_commit();
        vTaskDelay(pdMS_TO_TICKS(60));
    }
}

void led_ring_set_mood(mood_t mood) {
    if (mood < MOOD_COUNT) led_ring_set(MOOD_RGB[mood]);
}

void led_ring_off(void) {
    set_all(0, 0, 0);
    pixels_commit();
}
