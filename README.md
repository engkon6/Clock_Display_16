# ESP MQTT Matrix Clock

A professional, feature-rich Internet Clock using ESP8266 or ESP32 and MAX7219 LED Matrices. Features a web configuration portal, NTP time synchronization, and MQTT integration for remote messaging.

## 🚀 Features

- **Dual Platform Support:** Compatible with ESP8266 (e.g., NodeMCU, D1 Mini) and ESP32 (e.g., ESP32-C3 Super Mini).
- **Auto-Sync Time:** Uses NTP to fetch accurate time automatically.
- **Web Configuration:** Captive portal for WiFi setup and a dedicated web UI for settings (Brightness, Timezone, NTP Server, MQTT).
- **MQTT Integration:** 
  - Subscribes to commands to display custom scrolling text.
  - Publishes periodic telemetry (IP, Hostname, Sync Status).
- **OTA Updates:** Support for wireless firmware updates via ElegantOTA.
- **Dynamic Layout:** Supports 2 to 4 MAX7219 8x8 matrix modules.
- **12h/24h Modes:** Toggleable via web interface.

## 🛠 Hardware Required

- **Microcontroller:** ESP8266 or ESP32-C3.
- **Display:** MAX7219 8x8 LED Matrix (typically a 4-in-1 module).
- **Power:** 5V Micro USB.

### Wiring (Default)

| MAX7219 | ESP8266 | ESP32-C3 |
|---------|---------|----------|
| VCC     | 5V / VIN| 5V       |
| GND     | GND     | GND      |
| DIN     | D7 (MOSI)| GPIO 6   |
| CS      | D6      | GPIO 7   |
| CLK     | D5 (SCK) | GPIO 4   |
## 💻 Software Installation

### Pre-compiled Binaries (GitHub Actions)
For the easiest installation, use the pre-compiled binaries provided by our CI/CD pipeline:
1. Go to the [**Actions**](https://github.com/engkon6/esp-mqtt-matrix-clock/actions) tab of this repository.
2. Select the most recent successful run.
3. Scroll down to the **Artifacts** section.
4. Download `esp8266-firmware` or `esp32c3-firmware` based on your hardware.
5. Flash the `.bin` file to your device using [web.esphome.io](https://web.esphome.io/) or `esptool`.

### Manual Compilation
...
## 🤝 Contributors

- **engkon6** - Initial project and MQTT implementation.
- **Gemini CLI** - Refactored for cross-platform support (ESP32/ESP8266), modernized project structure, and implemented GitHub Actions CI/CD.

## 📄 License

- Adafruit GFX Library
- Max72xxPanel (Mark Ruys)
- ElegantOTA
- WiFiManager
- PubSubClient

## ⚙️ Configuration

1. **WiFi Setup:** On first boot, the device creates an AP named `Clock-XXXXXX`. Connect to it and configure your WiFi credentials.
2. **Web UI:** Once connected, find the device's IP address on the scrolling display. Navigate to `http://<device-ip>` in your browser.
3. **MQTT Commands:**
   - **Topic:** `cmnd/<hostname>/DisplayText`
   - **Payload:** Any string (it will scroll once across the display).

## 📄 License
This project is open-source and available under the MIT License.
