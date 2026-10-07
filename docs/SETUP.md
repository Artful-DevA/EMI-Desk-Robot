# Setup

## Current development board

The active EMI controller is an **ESP32-C3 Super Mini**.

Arduino IDE board selection:

- **ESP32C3 Dev Module**

The older ESP32 DevKit V1 is retained only as prototype/reference hardware.

## Arduino IDE

1. Install Arduino IDE.
2. Install the **esp32** board package by **Espressif Systems** through Boards Manager.
3. Install **U8g2 by oliver** through Library Manager.
4. Open `firmware/emi_c3/emi_c3.ino`.
5. Keep a private `secrets.h` beside `emi_c3.ino`.
6. Connect the ESP32-C3 over USB.
7. Select **ESP32C3 Dev Module**.
8. Select the COM / serial port belonging to the C3.
9. Compile.
10. Upload.

The first ESP32 compilation can take considerably longer because toolchain files are being built and cached.

## Private network configuration

The normal C3 firmware requires:

`firmware/emi_c3/secrets.h`

Start from:

`firmware/emi_c3/secrets.example.h`

The private file contains:

- Wi-Fi SSID
- Wi-Fi password
- Raspberry Pi LAN host
- EMI Hub port
- EMI shared token

Never commit `secrets.h`. It is excluded by `.gitignore`.

Example structure:

```cpp
#pragma once

#define EMI_WIFI_SSID "YOUR_WIFI_NAME"
#define EMI_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define EMI_HUB_HOST "YOUR_PI_LAN_IP"
#define EMI_HUB_PORT 17840
#define EMI_SHARED_TOKEN "YOUR_PRIVATE_EMI_TOKEN"
```

## Current verified C3 wiring

### OLED

| OLED pin | ESP32-C3 Super Mini |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 8 |
| SCL | GPIO 7 |

GPIO 9 is intentionally left free because it is the C3 BOOT/download strapping pin.

### TTP223 touch sensor

| TTP223 pin | ESP32-C3 Super Mini |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| OUT | GPIO 10 |

### I2S microphone

| Mic pin | ESP32-C3 Super Mini |
| --- | --- |
| VDD / VCC | 3V3 |
| GND | GND |
| L/R | GND |
| SCK / BCLK | GPIO 4 |
| WS / LRCLK | GPIO 5 |
| SD / SA | GPIO 6 |

The microphone mapping has already been physically validated with live audio data.

## Expected startup

With the current normal firmware, EMI should:

1. initialize the OLED
2. show the normal eye face
3. apply the proven **8.5 dBm Wi-Fi TX-power setting**
4. scan for the configured 2.4 GHz SSID
5. connect to the selected AP
6. obtain a LAN IP
7. poll the Raspberry Pi hub for commands
8. continue normal eye/touch behavior while networking runs in the background

Useful Serial Monitor output includes:

```text
Wi-Fi TX power 8.5 dBm: OK
Wi-Fi target found. RSSI=...
Wi-Fi event: associated with access point.
Wi-Fi event: got IP ...
Wi-Fi connected. IP: ...
EMI C3 ready.
```

## C3 upload recovery

If Arduino compiles successfully but `esptool` reports:

```text
Failed to connect to ESP32-C3: No serial data received.
```

use the recovery sequence that has worked reliably on this board:

1. unplug the C3 from USB
2. hold **BOOT**
3. plug USB back in while still holding BOOT
4. keep holding BOOT for about 3 seconds
5. release BOOT
6. re-select the COM port if Windows changed it
7. upload the sketch
8. after upload, reconnect/reset normally without holding BOOT

If Serial Monitor is blank, open it at **115200 baud** and press RESET once normally.

## Wi-Fi authentication note

This particular ESP32-C3 Super Mini could see the correct 2.4 GHz network but repeatedly failed with:

`AUTH_EXPIRE (2)`

The verified fix is already built into the normal firmware:

```cpp
WiFi.setTxPower(WIFI_POWER_8_5dBm);
```

Do not remove this while using the current board.

## Testing the physical Pi -> EMI command

Once the Raspberry Pi hub is running and EMI is connected, send an intent request from the Pi:

```bash
curl -s -H 'Content-Type: application/json' \
  -d '{"text":"Yo Emi time"}' \
  http://127.0.0.1:17840/intent
```

Expected physical result:

1. EMI closes his eyes
2. the face transitions into the current `HH:MM`
3. the hour pair aligns with the normal left-eye position
4. the minute pair aligns with the normal right-eye position
5. the clock holds briefly
6. EMI returns cleanly to his face

This end-to-end path has been physically validated.

## Touch test

Touch the TTP223 pad.

Expected behavior:

1. EMI quickly looks toward the touch area.
2. His eyes move slightly inward.
3. Slow repeated pats remain one petting session.
4. Individual strokes create a contented partial eye-close.
5. Several strokes can trigger a deeper relaxed blink.
6. After the petting grace period, EMI returns toward normal idle behavior.

## Troubleshooting OLED

Current OLED driver:

`U8G2_SH1106_128X64_NONAME_F_HW_I2C`

Current wiring:

- SDA: GPIO 8
- SCL: GPIO 7
- VCC: 3V3
- GND: GND

The OLED is monochrome. Its emitted pixel color is a physical property of the panel and cannot be changed to amber in software.

## Current microphone status

The microphone hardware is validated.

The next milestone is **not** another electrical mic test. It is to capture audio into RAM on the C3 and stream it to the Raspberry Pi for local speech-to-text without writing raw audio to disk.
