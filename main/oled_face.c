/**
 * OLED Face — SSD1306 128×64 I2C
 * Draws pixel-art emoji faces representing Popo's mood.
 */
#include "oled_face.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "oled";

#define OLED_I2C_PORT   I2C_NUM_0
#define OLED_ADDR       0x3C
#define OLED_SDA        GPIO_NUM_21
#define OLED_SCL        GPIO_NUM_22
#define OLED_WIDTH      128
#define OLED_HEIGHT     64

static uint8_t s_fb[OLED_WIDTH * OLED_HEIGHT / 8];

/* SSD1306 command helpers */
static void oled_cmd(uint8_t cmd) {
    uint8_t buf[2] = { 0x00, cmd };
    i2c_master_write_to_device(OLED_I2C_PORT, OLED_ADDR, buf, 2, pdMS_TO_TICKS(50));
}

static void oled_flush(void) {
    oled_cmd(0x21); oled_cmd(0); oled_cmd(127);
    oled_cmd(0x22); oled_cmd(0); oled_cmd(7);
    uint8_t header = 0x40;
    i2c_master_write_to_device(OLED_I2C_PORT, OLED_ADDR, &header, 1, pdMS_TO_TICKS(10));
    i2c_master_write_to_device(OLED_I2C_PORT, OLED_ADDR, s_fb, sizeof(s_fb), pdMS_TO_TICKS(200));
}

static void fb_clear(void) { memset(s_fb, 0, sizeof(s_fb)); }

static void fb_set_pixel(int x, int y, int on) {
    if (x < 0 || x >= OLED_WIDTH || y < 0 || y >= OLED_HEIGHT) return;
    int byte = x + (y / 8) * OLED_WIDTH;
    int bit  = y % 8;
    if (on) s_fb[byte] |= (1 << bit);
    else    s_fb[byte] &= ~(1 << bit);
}

static void fb_circle(int cx, int cy, int r, int on) {
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
            if (dx*dx + dy*dy <= r*r) fb_set_pixel(cx+dx, cy+dy, on);
}

static void fb_hline(int x0, int x1, int y, int on) {
    for (int x = x0; x <= x1; x++) fb_set_pixel(x, y, on);
}

static void fb_arc(int cx, int cy, int r, int y_sign, int on) {
    /* Simple arc: upper (y_sign=-1) or lower (y_sign=+1) half of a circle */
    for (int dx = -r; dx <= r; dx++) {
        int dy = (int)sqrtf((float)(r*r - dx*dx)) * y_sign;
        fb_set_pixel(cx+dx, cy+dy, on);
        fb_set_pixel(cx+dx, cy+dy-y_sign, on);
    }
}

/* --- Face pixel art --- */
static void draw_neutral(void) {
    fb_clear();
    /* Eyes */
    fb_circle(44, 28, 6, 1);  fb_circle(84, 28, 6, 1);
    fb_circle(44, 28, 3, 0);  fb_circle(84, 28, 3, 0);
    /* Flat mouth */
    fb_hline(50, 78, 46, 1);
    fb_hline(50, 78, 47, 1);
    oled_flush();
}

static void draw_happy(void) {
    fb_clear();
    fb_circle(44, 26, 6, 1);  fb_circle(84, 26, 6, 1);
    fb_circle(44, 26, 3, 0);  fb_circle(84, 26, 3, 0);
    fb_arc(64, 40, 14, 1, 1); /* smile */
    oled_flush();
}

static void draw_sad(void) {
    fb_clear();
    fb_circle(44, 28, 6, 1);  fb_circle(84, 28, 6, 1);
    fb_circle(44, 28, 3, 0);  fb_circle(84, 28, 3, 0);
    fb_arc(64, 52, 14, -1, 1); /* frown */
    oled_flush();
}

static void draw_excited(void) {
    fb_clear();
    /* Wide open eyes */
    fb_circle(44, 26, 8, 1);  fb_circle(84, 26, 8, 1);
    fb_circle(44, 26, 4, 0);  fb_circle(84, 26, 4, 0);
    fb_arc(64, 40, 16, 1, 1);
    /* sparkle dots */
    fb_set_pixel(24, 16, 1); fb_set_pixel(26, 14, 1); fb_set_pixel(28, 16, 1);
    fb_set_pixel(100, 16, 1); fb_set_pixel(102, 14, 1); fb_set_pixel(104, 16, 1);
    oled_flush();
}

static void draw_confused(void) {
    fb_clear();
    /* One normal, one squinted eye */
    fb_circle(44, 28, 6, 1);  fb_circle(44, 28, 3, 0);
    fb_hline(78, 90, 27, 1);  fb_hline(78, 90, 28, 1); /* squint line */
    /* Wavy mouth */
    for (int x = 46; x <= 82; x++) {
        int y = 46 + (int)(2.0f * sinf((x - 46) * 3.14f / 9.0f));
        fb_set_pixel(x, y, 1);
    }
    /* Question mark above */
    fb_set_pixel(64, 10, 1); fb_set_pixel(65, 10, 1);
    oled_flush();
}

static void draw_sleepy(void) {
    fb_clear();
    /* Half-closed eyes (horizontal slits) */
    fb_hline(36, 52, 29, 1);  fb_hline(76, 92, 29, 1);
    fb_hline(36, 52, 30, 1);  fb_hline(76, 92, 30, 1);
    /* Gentle smile */
    fb_arc(64, 42, 10, 1, 1);
    /* Zzz */
    fb_set_pixel(98, 15, 1); fb_set_pixel(99, 15, 1); fb_set_pixel(100, 15, 1);
    fb_set_pixel(98, 16, 1); fb_set_pixel(99, 16, 1); fb_set_pixel(100, 16, 1);
    oled_flush();
}

static void draw_scared(void) {
    fb_clear();
    /* Wide circle eyes */
    fb_circle(40, 26, 9, 1);  fb_circle(88, 26, 9, 1);
    fb_circle(40, 26, 5, 0);  fb_circle(88, 26, 5, 0);
    /* O-mouth */
    fb_circle(64, 48, 7, 1);
    fb_circle(64, 48, 4, 0);
    oled_flush();
}

static void draw_thinking(void) {
    fb_clear();
    fb_circle(44, 28, 6, 1);  fb_circle(84, 28, 6, 1);
    fb_circle(44, 28, 3, 0);  fb_circle(84, 28, 3, 0);
    /* Angled mouth */
    for (int x = 50; x <= 78; x++) fb_set_pixel(x, 46 + (x-50)/6, 1);
    /* Thought bubble */
    fb_circle(90, 14, 3, 1);
    fb_circle(96, 10, 4, 1);
    fb_circle(103, 7, 5, 1);
    oled_flush();
}

typedef void (*face_draw_fn)(void);
static const face_draw_fn DRAW_FNS[MOOD_COUNT] = {
    draw_neutral, draw_happy, draw_sad, draw_excited,
    draw_confused, draw_sleepy, draw_scared, draw_thinking
};

esp_err_t oled_face_init(void) {
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = OLED_SDA,
        .scl_io_num = OLED_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000
    };
    i2c_param_config(OLED_I2C_PORT, &cfg);
    i2c_driver_install(OLED_I2C_PORT, cfg.mode, 0, 0, 0);

    /* SSD1306 init sequence */
    uint8_t init_cmds[] = {
        0xAE, 0x20, 0x00, 0xB0, 0xC8, 0x00, 0x10,
        0x40, 0x81, 0xFF, 0xA1, 0xA6, 0xA8, 0x3F,
        0xA4, 0xD3, 0x00, 0xD5, 0xF0, 0xD9, 0x22,
        0xDA, 0x12, 0xDB, 0x20, 0x8D, 0x14, 0xAF
    };
    for (size_t i = 0; i < sizeof(init_cmds); i++) oled_cmd(init_cmds[i]);

    draw_neutral();
    ESP_LOGI(TAG, "OLED face ready");
    return ESP_OK;
}

void oled_face_set(face_t face) {
    if (face < MOOD_COUNT) DRAW_FNS[face]();
}

void oled_face_set_from_mood(mood_t mood) {
    if (mood < MOOD_COUNT) DRAW_FNS[mood]();
}

void oled_face_blink(void) {
    /* Clear eye regions briefly */
    for (int x = 36; x < 96; x++)
        for (int y = 18; y < 38; y++) fb_set_pixel(x, y, 0);
    oled_flush();
    vTaskDelay(pdMS_TO_TICKS(120));
    /* Redraw will be handled by next mood set */
}
