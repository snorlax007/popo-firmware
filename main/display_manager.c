/**
 * Display Manager — 2.4" 240×320 TFT IPS via SPI + LVGL
 * Handles: boot splash, clock screen, weather screen, mood overlay
 */
#include "display_manager.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "lvgl.h"
#include "lvgl_helpers.h"
#include <time.h>
#include <string.h>

static const char *TAG = "display";

/* LVGL screen objects */
static lv_obj_t *scr_clock   = NULL;
static lv_obj_t *scr_weather = NULL;
static lv_obj_t *lbl_time    = NULL;
static lv_obj_t *lbl_date    = NULL;
static lv_obj_t *lbl_temp    = NULL;
static lv_obj_t *lbl_cond    = NULL;
static lv_obj_t *lbl_city    = NULL;

static display_state_t s_state = DISP_CLOCK;
static SemaphoreHandle_t s_lv_lock;
static weather_data_t s_weather = {0};

/* Mood accent colours (RGB565 packed) */
static const lv_color_t MOOD_COLORS[MOOD_COUNT] = {
    LV_COLOR_MAKE(0x7c, 0x3a, 0xed),  /* neutral  - purple */
    LV_COLOR_MAKE(0xf5, 0xa6, 0x23),  /* happy    - amber  */
    LV_COLOR_MAKE(0x3b, 0x82, 0xf6),  /* sad      - blue   */
    LV_COLOR_MAKE(0x22, 0xc5, 0x5e),  /* excited  - green  */
    LV_COLOR_MAKE(0xf5, 0x9e, 0x0b),  /* confused - orange */
    LV_COLOR_MAKE(0x6b, 0x72, 0x80),  /* sleepy   - gray   */
    LV_COLOR_MAKE(0xef, 0x44, 0x44),  /* scared   - red    */
    LV_COLOR_MAKE(0xa8, 0x55, 0xf7),  /* thinking - violet */
};

static void build_clock_screen(void) {
    scr_clock = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_clock, lv_color_hex(0x080612), LV_PART_MAIN);

    lbl_time = lv_label_create(scr_clock);
    lv_obj_set_style_text_font(lbl_time, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_time, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(lbl_time, "00:00");
    lv_obj_align(lbl_time, LV_ALIGN_CENTER, 0, -20);

    lbl_date = lv_label_create(scr_clock);
    lv_obj_set_style_text_font(lbl_date, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_date, lv_color_hex(0x9ca3af), LV_PART_MAIN);
    lv_label_set_text(lbl_date, "Mon, 01 Jan 2025");
    lv_obj_align(lbl_date, LV_ALIGN_CENTER, 0, 30);
}

static void build_weather_screen(void) {
    scr_weather = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_weather, lv_color_hex(0x080612), LV_PART_MAIN);

    lbl_temp = lv_label_create(scr_weather);
    lv_obj_set_style_text_font(lbl_temp, &lv_font_montserrat_48, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_temp, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(lbl_temp, "--°C");
    lv_obj_align(lbl_temp, LV_ALIGN_CENTER, 0, -20);

    lbl_cond = lv_label_create(scr_weather);
    lv_obj_set_style_text_font(lbl_cond, &lv_font_montserrat_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_cond, lv_color_hex(0xa78bfa), LV_PART_MAIN);
    lv_label_set_text(lbl_cond, "Fetching…");
    lv_obj_align(lbl_cond, LV_ALIGN_CENTER, 0, 30);

    lbl_city = lv_label_create(scr_weather);
    lv_obj_set_style_text_font(lbl_city, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_city, lv_color_hex(0x6b7280), LV_PART_MAIN);
    lv_label_set_text(lbl_city, "");
    lv_obj_align(lbl_city, LV_ALIGN_CENTER, 0, 60);
}

static void tick_cb(void *arg) {
    lv_tick_inc(10);
}

esp_err_t display_manager_init(void) {
    lv_init();
    lvgl_driver_init();

    s_lv_lock = xSemaphoreCreateMutex();

    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t buf1[240 * 20];
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, 240 * 20);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = 240;
    disp_drv.ver_res  = 320;
    disp_drv.flush_cb = disp_driver_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    /* 10ms LVGL tick via ESP timer */
    const esp_timer_create_args_t timer_args = { .callback = tick_cb, .name = "lv_tick" };
    esp_timer_handle_t timer;
    esp_timer_create(&timer_args, &timer);
    esp_timer_start_periodic(timer, 10 * 1000);

    build_clock_screen();
    build_weather_screen();

    ESP_LOGI(TAG, "Display manager initialized");
    return ESP_OK;
}

void display_manager_show_boot(void) {
    xSemaphoreTake(s_lv_lock, portMAX_DELAY);
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x080612), LV_PART_MAIN);

    lv_obj_t *lbl = lv_label_create(scr);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x7c3aed), LV_PART_MAIN);
    lv_label_set_text(lbl, "Popo :)");
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

    lv_scr_load(scr);
    lv_task_handler();
    xSemaphoreGive(s_lv_lock);
    vTaskDelay(pdMS_TO_TICKS(2000));
}

void display_manager_set_state(display_state_t state) {
    xSemaphoreTake(s_lv_lock, portMAX_DELAY);
    s_state = state;
    if (state == DISP_CLOCK) {
        lv_scr_load_anim(scr_clock, LV_SCR_LOAD_ANIM_FADE_ON, 400, 0, false);
    } else if (state == DISP_WEATHER) {
        /* Refresh weather labels before switching */
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "%.1f°C", s_weather.temp_c);
        lv_label_set_text(lbl_temp, tmp);
        lv_label_set_text(lbl_cond, s_weather.condition);
        lv_label_set_text(lbl_city, s_weather.city);
        lv_scr_load_anim(scr_weather, LV_SCR_LOAD_ANIM_FADE_ON, 400, 0, false);
    }
    xSemaphoreGive(s_lv_lock);
}

void display_manager_sync_mood(mood_t mood) {
    if (mood >= MOOD_COUNT) return;
    xSemaphoreTake(s_lv_lock, portMAX_DELAY);
    lv_obj_set_style_text_color(lbl_time, MOOD_COLORS[mood], LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_temp, MOOD_COLORS[mood], LV_PART_MAIN);
    lv_task_handler();
    xSemaphoreGive(s_lv_lock);
}

void display_manager_update_weather(const weather_data_t *wx) {
    xSemaphoreTake(s_lv_lock, portMAX_DELAY);
    memcpy(&s_weather, wx, sizeof(weather_data_t));
    if (s_state == DISP_WEATHER) {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "%.1f°C", wx->temp_c);
        lv_label_set_text(lbl_temp, tmp);
        lv_label_set_text(lbl_cond, wx->condition);
        lv_label_set_text(lbl_city, wx->city);
        lv_task_handler();
    }
    xSemaphoreGive(s_lv_lock);
}

void display_manager_show_focus_timer(uint32_t remaining_sec) {
    uint32_t m = remaining_sec / 60, s = remaining_sec % 60;
    char buf[12];
    snprintf(buf, sizeof(buf), "%02lu:%02lu", m, s);
    xSemaphoreTake(s_lv_lock, portMAX_DELAY);
    lv_label_set_text(lbl_time, buf);
    lv_task_handler();
    xSemaphoreGive(s_lv_lock);
}
