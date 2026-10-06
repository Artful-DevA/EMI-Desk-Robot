# Wiring

This document describes the **current breadboard prototype**, which uses an ESP32 DevKit V1. The final EMI is planned around an ESP32-C3 Mini and will get a separate verified pin map before permanent wiring.

## OLED - SH1106 128x64 I2C

| OLED pin | ESP32 DevKit V1 |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

The OLED is a 4-pin I2C module with no separate reset pin.

## TTP223 capacitive touch sensor

| TTP223 pin | ESP32 DevKit V1 |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| OUT | GPIO 27 |

The default firmware expects the TTP223 output to go HIGH when touched.

## I2S MEMS microphone

Current prototype wiring:

| Mic pin | ESP32 DevKit V1 |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| L/R | GND |
| WS | GPIO 25 |
| SCK | GPIO 26 |
| SA / SD | GPIO 32 |

The label `SA` on the module is being treated as the microphone's serial audio data output. This subsystem is not yet considered validated.

The header pins must be soldered to the microphone PCB. Simply pushing loose header pins through plated holes is not a reliable electrical connection.

## Breadboard power

For the prototype:

- ESP32 3V3 can feed the breadboard 3.3 V rail.
- ESP32 GND can feed the breadboard ground rail.
- OLED, touch sensor, and microphone share those rails.

Sharing a power rail does **not** mean the devices share signal pins. Only their power and ground are common.

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
- All grounds must be common.
- Put the electrolytic capacitor across servo 5 V and GND near the servo supply path.
- Capacitor positive goes to 5 V.
- Capacitor negative, usually marked by a stripe, goes to GND.

## Final build power distribution

The final robot will not need a breadboard. The same idea can be implemented with a small soldered distribution point or perfboard:

- one 3.3 V connection branches to low-power 3.3 V devices
- one GND connection branches to all devices
- servo uses the 5 V path, not the 3.3 V path

No extra controller is required merely to split power.
