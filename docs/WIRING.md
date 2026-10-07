# Wiring

The ESP32-C3 Super Mini pin map below has now been **physically validated** with the OLED, TTP223 touch sensor, and I2S microphone operating together.

## Verified ESP32-C3 wiring

### OLED - SH1106 128x64 I2C

| OLED pin | ESP32-C3 Super Mini |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 8 |
| SCL | GPIO 7 |

GPIO 9 is intentionally left unused by the OLED because it is the ESP32-C3 BOOT strapping pin. Keeping the OLED clock off GPIO 9 makes manual flashing/recovery much more reliable.

### TTP223 capacitive touch sensor

| TTP223 pin | ESP32-C3 Super Mini |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| OUT | GPIO 10 |

The firmware expects the TTP223 output to go HIGH when touched.

### I2S MEMS microphone

| Mic pin | ESP32-C3 Super Mini |
| --- | --- |
| VDD / VCC | 3V3 |
| GND | GND |
| L/R | GND |
| SCK / BCLK | GPIO 4 |
| WS / LRCLK | GPIO 5 |
| SD / SA | GPIO 6 |

This microphone mapping has been validated with live audio data. The microphone responds to speech and nearby sounds.

The microphone header pins must be soldered to the PCB. Loose header pins pushed through the plated holes are not a reliable electrical connection.

## Previous ESP32 DevKit V1 prototype map

The earlier breadboard prototype used the following mapping:

| Function | ESP32 DevKit V1 |
| --- | --- |
| OLED SDA | GPIO 21 |
| OLED SCL | GPIO 22 |
| Touch OUT | GPIO 27 |
| Mic WS | GPIO 25 |
| Mic SCK | GPIO 26 |
| Mic SD / SA | GPIO 32 |

This map is retained only as a reference for the earlier prototype.

## Power distribution

For the low-power electronics:

- OLED, TTP223, and microphone use 3.3 V.
- They may share the same 3.3 V rail.
- They may share the same GND rail.
- Signal pins remain separate.
- All grounds in the final robot must be common.

## Planned servo wiring

The SG90 has not yet been integrated into the active prototype.

When added:

| SG90 wire/function | Connection |
| --- | --- |
| 5 V power | 5 V / VBUS supply path |
| GND | Common GND |
| Signal | Dedicated GPIO, to be finalized |

Important:

- Do **not** power the SG90 from 3.3 V.
- Put the electrolytic capacitor across servo 5 V and GND near the servo supply path.
- Capacitor positive goes to 5 V.
- Capacitor negative, usually marked by a stripe, goes to GND.

## Final build power distribution

The final robot will not need a breadboard. The same power layout can be implemented with a small soldered distribution point or perfboard:

- one 3.3 V connection branches to the low-power 3.3 V devices
- one GND connection branches to all devices
- servo uses the 5 V path, not the 3.3 V path
