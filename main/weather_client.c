#include "weather_client.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "weather";

#define BACKEND_URL  CONFIG_POPO_BACKEND_URL
#define OWM_API_KEY  CONFIG_POPO_OWM_API_KEY
#define OWM_CITY     CONFIG_POPO_CITY

static char s_resp_buf[2048];
static int  s_resp_len = 0;

static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        int remain = sizeof(s_resp_buf) - s_resp_len - 1;
        if (remain > 0) {
            memcpy(s_resp_buf + s_resp_len, evt->data, MIN(evt->data_len, remain));
            s_resp_len += MIN(evt->data_len, remain);
            s_resp_buf[s_resp_len] = '\0';
        }
    }
    return ESP_OK;
}

esp_err_t weather_client_fetch(weather_data_t *out) {
    char url[256];
    snprintf(url, sizeof(url),
        "https://api.openweathermap.org/data/2.5/weather?q=%s&appid=%s&units=metric",
        OWM_CITY, OWM_API_KEY);

    s_resp_len = 0;
    esp_http_client_config_t cfg = { .url = url, .event_handler = http_event_handler };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_err_t err = esp_http_client_perform(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) { ESP_LOGE(TAG, "HTTP GET failed: %s", esp_err_to_name(err)); return err; }

    cJSON *root = cJSON_Parse(s_resp_buf);
    if (!root) return ESP_FAIL;

    cJSON *main = cJSON_GetObjectItem(root, "main");
    cJSON *weather = cJSON_GetArrayItem(cJSON_GetObjectItem(root, "weather"), 0);

    if (main && weather) {
        out->temp_c   = (float)cJSON_GetObjectItem(main, "temp")->valuedouble;
        out->humidity = cJSON_GetObjectItem(main, "humidity")->valueint;
        strncpy(out->condition, cJSON_GetObjectItem(weather, "main")->valuestring, 31);
        strncpy(out->icon_code, cJSON_GetObjectItem(weather, "icon")->valuestring, 7);
        strncpy(out->city, cJSON_GetObjectItem(root, "name")->valuestring, 31);
    }
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t weather_client_post_intent(const char *transcript, popo_intent_t *out) {
    char body[512];
    snprintf(body, sizeof(body), "{\"text\":\"%s\"}", transcript);

    s_resp_len = 0;
    char url[128];
    snprintf(url, sizeof(url), "%s/api/intent", BACKEND_URL);

    esp_http_client_config_t cfg = { .url = url, .method = HTTP_METHOD_POST, .event_handler = http_event_handler };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, strlen(body));

    esp_err_t err = esp_http_client_perform(client);
    esp_http_client_cleanup(client);
    if (err != ESP_OK) return err;

    cJSON *root = cJSON_Parse(s_resp_buf);
    if (!root) return ESP_FAIL;

    const char *intent    = cJSON_GetObjectItem(root, "intent")->valuestring;
    const char *sentiment = cJSON_GetObjectItem(root, "sentiment")->valuestring;
    const char *reply     = cJSON_GetObjectItem(root, "popo_reply")->valuestring;
    cJSON *mood_item = cJSON_GetObjectItem(root, "mood");

    if (intent)    strncpy(out->intent, intent, 31);
    if (sentiment) strncpy(out->sentiment, sentiment, 15);
    if (reply)     strncpy(out->popo_reply, reply, 127);

    /* Map mood string → mood_t integer */
    const char *mood_map[] = {"neutral","happy","sad","excited","confused","sleepy","scared","thinking"};
    const char *mood_str = mood_item ? mood_item->valuestring : "neutral";
    out->mood = 0;
    for (int i = 0; i < 8; i++) {
        if (strcmp(mood_str, mood_map[i]) == 0) { out->mood = i; break; }
    }

    cJSON_Delete(root);
    return ESP_OK;
}
