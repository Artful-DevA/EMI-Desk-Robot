# Setup

## Current development board

The breadboard prototype currently uses an **ESP32 DevKit V1** rather than the final ESP32-C3 Mini.

Arduino IDE board selection:

- **DOIT ESP32 DEVKIT V1**

On Windows, the current DevKit uses a Silicon Labs CP210x USB-to-UART bridge. If the serial port is missing, install the official Silicon Labs CP210x VCP driver.

## Arduino IDE

1. Install Arduino IDE.
2. Install the **esp32** board package by **Espressif Systems** through Boards Manager.
3. Install **U8g2 by oliver** through Library Manager.
4. Connect the ESP32 over USB.
5. Select **DOIT ESP32 DEVKIT V1**.
6. Select the COM / serial port belonging to the ESP32.
7. Open `firmware/emi_breadboard/emi_breadboard.ino`.
8. Compile.
9. Upload.

The first ESP32 compilation can take considerably longer than later builds because toolchain files are being compiled and cached.

## Expected startup

After reset, the OLED should show two white rounded rectangular eyes.

The current firmware then:

- waits briefly before the first idle gaze change
- moves the eyes in quick saccade-like motions
- holds each gaze for a while
- blinks infrequently
- reacts to the TTP223 on GPIO 27

## Touch test

Touch the TTP223 pad.

Expected response:

1. EMI quickly looks high toward the touch area.
2. His eyes move slightly inward.
3. Slow repeated pats keep the petting state alive.
4. Tiny slow horizontal gaze drift prevents a frozen stare.
5. Several pats may cause one relaxed blink.
6. About 2.2 seconds after the last touch, EMI returns toward normal.

## Troubleshooting OLED

Current OLED wiring:

- SDA: GPIO 21
- SCL: GPIO 22
- VCC: 3V3
- GND: GND

The current driver constructor is:

`U8G2_SH1106_128X64_NONAME_F_HW_I2C`

The OLED is monochrome. Its white pixel color is physical and cannot be changed to amber in software.

## Troubleshooting touch

If the touch sensor does nothing:

- verify VCC is 3.3 V
- verify GND
- verify OUT goes to GPIO 27
- verify the sensor actually changes OUT when touched
- check for a loose Dupont connection

## Microphone status

The microphone is currently experimental. Finish and inspect the header solder joints before treating microphone measurements as meaningful.
