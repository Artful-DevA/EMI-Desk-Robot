#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// ============================================================
// EMI - ESP32-C3 OLED Test
// Board: ESP32C3 Dev Module
//
// Wiring:
//   OLED VCC -> C3 3V3
//   OLED GND -> C3 GND
//   OLED SDA -> GPIO 8
//   OLED SCL -> GPIO 9
//
// Display: SH1106 128x64 I2C
// ============================================================

static const int OLED_SDA = 8;
static const int OLED_SCL = 9;

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(
  U8G2_R0,
  U8X8_PIN_NONE
);

void setup() {
  Wire.begin(OLED_SDA, OLED_SCL);

  display.begin();
  display.setContrast(100);

  display.clearBuffer();

  display.setFont(u8g2_font_6x12_tf);
  display.drawStr(0, 14, "EMI C3 OLED TEST");

  display.setFont(u8g2_font_ncenB14_tr);
  display.drawStr(33, 42, "HELLO");

  display.setFont(u8g2_font_6x12_tf);
  display.drawStr(19, 60, "GPIO 8 / GPIO 9");

  display.sendBuffer();
}

void loop() {
}
