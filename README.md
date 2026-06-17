# Popo Firmware

ESP32-S3 firmware for the Popo AI Desk Companion.

## Hardware
- ESP32-S3 (dual-core, 8MB flash, 8MB PSRAM)
- 2.4" TFT IPS 240×320 (SPI) — main display via LVGL
- 1.3" OLED 128×64 (I2C/SSD1306) — mood face
- INMP441 I2S MEMS microphone
- MAX98357A I2S DAC + 3W speaker
- WS2812B 12-LED ring (RMT)
- TP4056 + MT3608 Li-Po power circuit

## Prerequisites
- ESP-IDF v5.x (`idf.py` in PATH)
- `esp-sr` component for WakeNet
- `lvgl` + `lvgl_helpers` components

## Build & Flash
```bash
# Set up ESP-IDF environment
. $IDF_PATH/export.sh

# Configure (set WiFi/API keys via menuconfig or sdkconfig.defaults)
idf.py menuconfig

# Build
idf.py build

# Flash + monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

## Configuration (menuconfig → Popo Configuration)
| Key | Description |
|-----|-------------|
| `POPO_BACKEND_URL` | Deployed popo-app URL (e.g. `https://popo-app.vercel.app`) |
| `POPO_OWM_API_KEY` | OpenWeatherMap API key |
| `POPO_CITY` | Default city for weather |
| `POPO_WAKE_WORD_THRESHOLD` | Wake word sensitivity (50–99) |

## Pin Map
| Signal | GPIO |
|--------|------|
| TFT SPI MOSI | 11 |
| TFT SPI CLK | 12 |
| TFT CS | 10 |
| TFT DC | 13 |
| TFT RST | 14 |
| OLED SDA | 21 |
| OLED SCL | 22 |
| MIC BCLK | 38 |
| MIC WS | 39 |
| MIC DATA | 40 |
| DAC BCLK | 15 |
| DAC LRCK | 16 |
| DAC DATA | 17 |
| LED Ring | 48 |

## Architecture
```
app_main()
├── display_manager (LVGL, TFT SPI)
├── oled_face (SSD1306 I2C pixel art)
├── led_ring (WS2812B RMT)
├── audio_manager (I2S mic + STT via backend)
├── popo_tts (SPIFFS phoneme clips → I2S DAC)
├── wifi_manager (BLE provisioning + STA)
├── weather_client (OWM HTTP + backend intent API)
├── wake_word (ESP-SR WakeNet)
└── mood_engine (thread-safe mood state)
```
