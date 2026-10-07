# Hardware

## Current prototype

- ESP32-C3 Super Mini
- SH1106 128x64 4-pin I2C OLED
- TTP223 / TTP223B capacitive touch sensor
- I2S MEMS microphone with VCC, GND, SA/SD, L/R, WS, SCK
- breadboard
- Dupont jumper wires

## Final controller

The active controller is an **ESP32-C3 Super Mini**.

The current validated mapping keeps OLED SCL on GPIO 7 and leaves GPIO 9 free because GPIO 9 is the C3 BOOT strapping pin. The older DevKit V1 pin assignments remain prototype-only.

## Head movement

Planned actuator:

- SG90
- 180 degree positional servo
- not a 360 degree continuous-rotation servo

The enclosure design supports the head on a plastic neck ring / ledge so the servo primarily rotates the head instead of carrying its full vertical weight.

## Servo power

The servo must not use the ESP32 3.3 V rail.

Planned arrangement:

- servo power from 5 V / VBUS path
- common ground with ESP32
- bulk electrolytic capacitor close to servo power

Current selected capacitor size/specification:

- 470 uF
- 35 V
- low ESR
- approximately 10 x 17 mm
- 5 mm lead spacing

The capacitor goes **across** 5 V and GND, not in series.

## OLED

Current display:

- 1.54 inch class
- 128x64
- monochrome
- 4-pin I2C
- SH1106-compatible
- pins: VCC, GND, SCL, SDA

The emitted pixel color is fixed by the panel. Code cannot turn a white monochrome OLED amber.

## Microphone

The desired microphone is a small 3.3 V-compatible digital I2S MEMS module.

The current module exposes:

- GND
- VCC
- SA / SD
- L/R
- WS
- SCK

The current prototype ties L/R to GND and configures the ESP32 for the left I2S channel.

## Audio output

There is no onboard speaker in the current prototype.

The near-term plan is for EMI to use the desktop computer's speakers over the local network. A MAX98357 I2S amplifier and passive speaker may be added later.

## Enclosure direction

The physical design is a small retro CRT-style head and base using white and black filament.

Current enclosure concepts include:

- recessed black OLED bezel
- touch band around the head near the rear cap
- functional microphone openings
- functional ventilation
- removable / serviceable electronics
- USB-C access at the rear
- no glued-in electronics where avoidable
