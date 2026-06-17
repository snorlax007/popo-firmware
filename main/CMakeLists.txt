idf_component_register(
    SRCS
        "main.c"
        "mood_engine.c"
        "popo_tts.c"
        "weather_client.c"
        "display_manager.c"
        "oled_face.c"
        "audio_manager.c"
        "wifi_manager.c"
        "wake_word.c"
        "led_ring.c"
    INCLUDE_DIRS "include"
    REQUIRES
        nvs_flash freertos esp_http_client driver
        esp_wifi esp_event spiffs json
        esp-sr
)
