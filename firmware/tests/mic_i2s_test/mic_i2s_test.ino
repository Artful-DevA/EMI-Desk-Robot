#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "driver/i2s.h"

// ============================================================
// EMI - I2S Microphone + OLED Test
// ESP32 DevKit V1
//
// OLED wiring:
//   OLED VCC -> ESP32 3V3
//   OLED GND -> ESP32 GND
//   OLED SCL -> GPIO 22
//   OLED SDA -> GPIO 21
//
// Microphone wiring:
//   Mic GND     -> ESP32 GND
//   Mic VCC/VDD -> ESP32 3V3
//   Mic L/R     -> ESP32 GND
//   Mic WS      -> GPIO 25
//   Mic SCK     -> GPIO 26
//   Mic SD/SA   -> GPIO 32
//
// The OLED shows:
// - live microphone LEVEL number
// - live PEAK number
// - a large horizontal sound bar
//
// Serial output is also available at 115200 baud.
// ============================================================

static const i2s_port_t I2S_PORT = I2S_NUM_0;

static const int PIN_I2S_WS  = 25;
static const int PIN_I2S_SCK = 26;
static const int PIN_I2S_SD  = 32;

static const int PIN_OLED_SDA = 21;
static const int PIN_OLED_SCL = 22;

static const int SAMPLE_RATE = 16000;
static const int SAMPLE_COUNT = 256;

// Adjust this only if the bar is always tiny or always full.
static const uint32_t DISPLAY_LEVEL_MAX = 1200;

int32_t samples[SAMPLE_COUNT];

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(
  U8G2_R0,
  U8X8_PIN_NONE
);

void setupI2SMicrophone() {
  i2s_config_t i2sConfig = {};

  i2sConfig.mode =
    (i2s_mode_t)(
      I2S_MODE_MASTER |
      I2S_MODE_RX
    );

  i2sConfig.sample_rate = SAMPLE_RATE;
  i2sConfig.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  i2sConfig.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  i2sConfig.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  i2sConfig.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  i2sConfig.dma_buf_count = 8;
  i2sConfig.dma_buf_len = 128;
  i2sConfig.use_apll = false;
  i2sConfig.tx_desc_auto_clear = false;
  i2sConfig.fixed_mclk = 0;

  i2s_pin_config_t pinConfig = {};

  pinConfig.bck_io_num = PIN_I2S_SCK;
  pinConfig.ws_io_num = PIN_I2S_WS;
  pinConfig.data_out_num = I2S_PIN_NO_CHANGE;
  pinConfig.data_in_num = PIN_I2S_SD;

  esp_err_t result =
    i2s_driver_install(
      I2S_PORT,
      &i2sConfig,
      0,
      nullptr
    );

  if (result != ESP_OK) {
    Serial.print("i2s_driver_install failed: ");
    Serial.println((int)result);

    display.clearBuffer();
    display.setFont(u8g2_font_6x12_tf);
    display.drawStr(0, 15, "I2S install failed");
    display.sendBuffer();

    while (true) {
      delay(1000);
    }
  }

  result =
    i2s_set_pin(
      I2S_PORT,
      &pinConfig
    );

  if (result != ESP_OK) {
    Serial.print("i2s_set_pin failed: ");
    Serial.println((int)result);

    display.clearBuffer();
    display.setFont(u8g2_font_6x12_tf);
    display.drawStr(0, 15, "I2S pin failed");
    display.sendBuffer();

    while (true) {
      delay(1000);
    }
  }

  i2s_zero_dma_buffer(I2S_PORT);
}

void drawMicDisplay(uint32_t level, uint32_t peak) {
  uint32_t clippedLevel =
    min(level, DISPLAY_LEVEL_MAX);

  int barWidth =
    map(
      clippedLevel,
      0,
      DISPLAY_LEVEL_MAX,
      0,
      118
    );

  display.clearBuffer();

  display.setFont(u8g2_font_6x12_tf);
  display.drawStr(0, 11, "EMI MIC TEST");

  display.setCursor(0, 27);
  display.print("LEVEL: ");
  display.print(level);

  display.setCursor(0, 40);
  display.print("PEAK : ");
  display.print(peak);

  display.drawFrame(4, 49, 120, 12);

  if (barWidth > 0) {
    display.drawBox(
      5,
      50,
      barWidth,
      10
    );
  }

  display.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(
    PIN_OLED_SDA,
    PIN_OLED_SCL
  );

  display.begin();

  // Lower than the OLED's very bright default.
  display.setContrast(100);

  display.clearBuffer();
  display.setFont(u8g2_font_6x12_tf);
  display.drawStr(0, 15, "EMI MIC TEST");
  display.drawStr(0, 31, "Starting...");
  display.sendBuffer();

  Serial.println();
  Serial.println("EMI I2S microphone + OLED test starting...");
  Serial.println("Speak, clap, or tap near the microphone.");
  Serial.println();

  setupI2SMicrophone();

  delay(300);
}

void loop() {
  size_t bytesRead = 0;

  esp_err_t result =
    i2s_read(
      I2S_PORT,
      samples,
      sizeof(samples),
      &bytesRead,
      portMAX_DELAY
    );

  if (result != ESP_OK) {
    Serial.print("I2S read error: ");
    Serial.println((int)result);
    delay(100);
    return;
  }

  int sampleCount =
    bytesRead /
    sizeof(int32_t);

  if (sampleCount <= 0) {
    return;
  }

  // Most 24-bit I2S microphones place the useful sample
  // inside a 32-bit word. Shift away unused low bits.
  int64_t sum = 0;

  for (int i = 0; i < sampleCount; i++) {
    int32_t sample =
      samples[i] >> 8;

    sum += sample;
  }

  int32_t mean =
    (int32_t)(
      sum /
      sampleCount
    );

  uint64_t absSum = 0;
  int32_t peak = 0;

  for (int i = 0; i < sampleCount; i++) {
    int32_t sample =
      (samples[i] >> 8) -
      mean;

    int32_t magnitude =
      abs(sample);

    absSum +=
      (uint32_t)magnitude;

    if (magnitude > peak) {
      peak = magnitude;
    }
  }

  uint32_t level =
    (uint32_t)(
      absSum /
      sampleCount
    );

  Serial.print("level:");
  Serial.print(level);
  Serial.print(" peak:");
  Serial.println(peak);

  drawMicDisplay(
    level,
    (uint32_t)peak
  );

  delay(20);
}
