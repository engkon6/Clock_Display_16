# ESP MQTT Matrix Clock

Internet Clock using [ESP8266](https://en.wikipedia.org/wiki/ESP8266) or [ESP32](https://en.wikipedia.org/wiki/ESP32) and [MAX7219](https://www.analog.com/en/products/max7219.html) LED Matrices.

## 🚀 Features

- **Dual Platform Support:** Compatible with ESP8266 (e.g., NodeMCU, D1 Mini) and ESP32 (e.g., ESP32-C3 Super Mini).
- **Auto-Sync Time:** Uses [NTP](https://en.wikipedia.org/wiki/Network_Time_Protocol) to fetch accurate time automatically.
- **Web Configuration:** [Captive portal](https://en.wikipedia.org/wiki/Captive_portal) for WiFi setup and a dedicated web UI for settings (Brightness, Timezone, NTP Server, [MQTT](https://en.wikipedia.org/wiki/MQTT)).
- **MQTT Integration:** 
  - Subscribes to commands to display custom scrolling text.
  - Publishes periodic [telemetry](https://en.wikipedia.org/wiki/Telemetry) (IP, Hostname, Sync Status).
- **OTA Updates:** Support for [Over-the-Air](https://en.wikipedia.org/wiki/Over-the-air_programming) (OTA) firmware updates via ElegantOTA.
- **Dynamic Layout:** Supports 2 to 4 MAX7219 8x8 matrix modules.
- **12h/24h Modes:** Toggleable via web interface.

## 🛠 Hardware Required

- **Microcontroller:** ESP8266 or ESP32.
- **Display:** MAX7219 8x8 [LED Matrix](https://en.wikipedia.org/wiki/LED_matrix) (typically a 4-in-1 module).

### Wiring Diagram

Below is the connection scheme between the microcontroller and the LED matrix.

```text
       [ Microcontroller ]             [ MAX7219 LED Matrix ]
      +-------------------+           +----------------------+
      |        5V         | --------> |         VCC          |
      |        GND        | --------> |         GND          |
      |        MOSI       | --------> |         DIN          |
      |        MISO       | --------> |         CS           |
      |        SLCK       | --------> |         CLK          |
      +-------------------+           +----------------------+
```

#### Detailed Pinout Table

| MAX7219 | ESP8266 (NodeMCU/D1) | ESP32-C3 (Super Mini) | Description       |
|---------|----------------------|-----------------------|-------------------|
| **VCC** | 5V                   | 5V                    | Power Supply (5V) |
| **GND** | GND                  | GND                   | Ground            |
| **DIN** | D7 (MOSI)            | GPIO 6                | Data Input        |
| **CS**  | D6 (MISO)            | GPIO 5                | Chip Select       |
| **CLK** | D5 (SCLK)            | GPIO 4                | Serial Clock      |

## 💻 Software Installation

### Pre-compiled Binaries (Releases)
Download the pre-compiled `.bin` files for your specific architecture from the [**Releases**](https://github.com/engkon6/esp-mqtt-matrix-clock/releases) page.

1. Download `esp8266-firmware.bin` or `esp32c3-firmware.merged.bin`.
2. Flash the file to your device using [web.esphome.io](https://web.esphome.io/) or `esptool`.

### Manual Compilation
Required Libraries (install via [Arduino Library Manager](https://docs.arduino.cc/software/ide-v1/tutorials/installing-libraries)):
- **Adafruit GFX Library:** Core graphics library.
- **Max72xxPanel:** Hardware driver for the matrix.
- **ElegantOTA:** For wireless updates.
- **WiFiManager:** For easy WiFi configuration.
- **PubSubClient:** For MQTT communication.

## ⚙️ Configuration

1. **WiFi Setup:** On first boot, the device creates an [Access Point](https://en.wikipedia.org/wiki/Wireless_access_point) (AP) named `Clock-XXXXXX`. Connect to it with your smartphone or PC and configure your WiFi credentials.
2. **Web UI:** Once connected, find the device's [IP address](https://en.wikipedia.org/wiki/IP_address) on the scrolling display. Navigate to `http://<device-ip>` in your browser.
3. **MQTT Commands:**
   - **Topic:** `cmnd/<hostname>/DisplayText`
   - **Payload:** Any text string (it will scroll once across the display).

## 📄 License
This project is open-source and available under the [MIT License](https://opensource.org/licenses/MIT).
