#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include "driver/i2s.h"

// ============================================================
// EMI - ESP32-C3 OLED + Touch + I2S Microphone Test
// Board: ESP32C3 Dev Module
//
// OLED:
//   VCC -> 3V3
//   GND -> GND
//   SDA -> GPIO 8
//   SCL -> GPIO 9
//
// TTP223:
//   VCC -> 3V3
//   GND -> GND
//   OUT -> GPIO 10
//
// I2S microphone:
//   VDD / VCC -> 3V3
//   GND       -> GND
//   L/R       -> GND
//   SCK/BCLK  -> GPIO 4
//   WS/LRCLK  -> GPIO 5
//   SD/SA     -> GPIO 6
//
// What to expect:
// - two eyes on the OLED
// - touching the TTP223 changes the eyes and shows TOUCH
// - speaking/clapping makes the bottom microphone bar move
// - Serial Monitor at 115200 also prints touch + mic values
// ============================================================

static const int OLED_SDA = 8;
static const int OLED_SCL = 9;

static const int TOUCH_PIN = 10;

static const int MIC_SCK = 4;
static const int MIC_WS  = 5;
static const int MIC_SD  = 6;

static const i2s_port_t I2S_PORT = I2S_NUM_0;
static const int SAMPLE_RATE = 16000;
static const int SAMPLE_COUNT = 256;

static const int EYE_W = 23;
static const int EYE_H = 29;
static const int LEFT_EYE_X = 24;
static const int RIGHT_EYE_X = 81;
static const int EYE_CENTER_Y = 31;

int32_t samples[SAMPLE_COUNT];

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(
  U8G2_R0,
  U8X8_PIN_NONE
);

float smoothedLevel = 0.0f;
float displayScale = 1000.0f;

void setupMicrophone() {
  i2s_config_t config = {};

  config.mode = (i2s_mode_t)(
    I2S_MODE_MASTER |
    I2S_MODE_RX
  );

  config.sample_rate = SAMPLE_RATE;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;

  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;

  config.dma_buf_count = 8;
  config.dma_buf_len = 128;

  config.use_apll = false;
  config.tx_desc_auto_clear = false;
  config.fixed_mclk = 0;

  i2s_pin_config_t pins = {};

  pins.bck_io_num = MIC_SCK;
  pins.ws_io_num = MIC_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = MIC_SD;

  esp_err_t result =
    i2s_driver_install(
      I2S_PORT,
      &config,
      0,
      nullptr
    );

  if (result != ESP_OK) {
    Serial.print("I2S install failed: ");
    Serial.println((int)result);

    display.clearBuffer();
    display.setFont(u8g2_font_6x12_tf);
    display.drawStr(0, 15, "I2S INSTALL FAIL");
    display.sendBuffer();

    while (true) {
      delay(1000);
    }
  }

  result =
    i2s_set_pin(
      I2S_PORT,
      &pins
    );

  if (result != ESP_OK) {
    Serial.print("I2S pin setup failed: ");
    Serial.println((int)result);

    display.clearBuffer();
    display.setFont(u8g2_font_6x12_tf);
    display.drawStr(0, 15, "I2S PIN FAIL");
    display.sendBuffer();

    while (true) {
      delay(1000);
    }
  }

  i2s_zero_dma_buffer(I2S_PORT);
}

uint32_t readMicrophoneLevel() {
  size_t bytesRead = 0;

  esp_err_t result =
    i2s_read(
      I2S_PORT,
      samples,
      sizeof(samples),
      &bytesRead,
      portMAX_DELAY
    );

  if (result != ESP_OK || bytesRead == 0) {
    return 0;
  }

  int count =
    bytesRead /
    sizeof(int32_t);

  int64_t sum = 0;

  for (int i = 0; i < count; i++) {
    // Typical 24-bit I2S MEMS sample inside a 32-bit word.
    int32_t sample =
      samples[i] >> 8;

    sum += sample;
  }

  int32_t mean =
    (int32_t)(
      sum /
      count
    );

  uint64_t magnitudeSum = 0;

  for (int i = 0; i < count; i++) {
    int32_t sample =
      (samples[i] >> 8) -
      mean;

    int32_t magnitude =
      abs(sample);

    magnitudeSum +=
      (uint32_t)magnitude;
  }

  return
    (uint32_t)(
      magnitudeSum /
      count
    );
}

void updateMicMeter(uint32_t rawLevel) {
  // Fast rise, slower fall.
  if ((float)rawLevel > smoothedLevel) {
    smoothedLevel =
      smoothedLevel * 0.55f +
      (float)rawLevel * 0.45f;
  } else {
    smoothedLevel =
      smoothedLevel * 0.90f +
      (float)rawLevel * 0.10f;
  }

  // Auto-ranging keeps the meter useful even if this microphone's
  // raw numbers are much larger or smaller than expected.
  float wantedScale =
    max(
      100.0f,
      smoothedLevel * 1.35f
    );

  if (wantedScale > displayScale) {
    displayScale =
      displayScale * 0.70f +
      wantedScale * 0.30f;
  } else {
    displayScale =
      displayScale * 0.997f +
      wantedScale * 0.003f;
  }
}

void drawOpenEye(int centerX, int centerY) {
  int x =
    centerX -
    EYE_W / 2;

  int y =
    centerY -
    EYE_H / 2;

  display.drawRBox(
    x,
    y,
    EYE_W,
    EYE_H,
    7
  );
}

void drawTouchedEye(int centerX, int centerY) {
  int x1 = centerX - 11;
  int x2 = centerX + 11;
  int y = centerY + 2;

  display.drawLine(
    x1,
    y,
    centerX,
    y + 5
  );

  display.drawLine(
    centerX,
    y + 5,
    x2,
    y
  );

  display.drawLine(
    x1,
    y + 1,
    centerX,
    y + 6
  );

  display.drawLine(
    centerX,
    y + 6,
    x2,
    y + 1
  );
}

void drawScreen(bool touched, uint32_t rawLevel) {
  display.clearBuffer();

  if (touched) {
    drawTouchedEye(
      LEFT_EYE_X,
      EYE_CENTER_Y
    );

    drawTouchedEye(
      RIGHT_EYE_X,
      EYE_CENTER_Y
    );

    display.setFont(u8g2_font_5x8_tf);
    display.drawStr(51, 8, "TOUCH");
  } else {
    drawOpenEye(
      LEFT_EYE_X,
      EYE_CENTER_Y
    );

    drawOpenEye(
      RIGHT_EYE_X,
      EYE_CENTER_Y
    );
  }

  int barWidth = 0;

  if (displayScale > 1.0f) {
    float ratio =
      smoothedLevel /
      displayScale;

    ratio =
      constrain(
        ratio,
        0.0f,
        1.0f
      );

    barWidth =
      (int)(
        ratio * 118.0f
      );
  }

  display.drawFrame(
    4,
    53,
    120,
    9
  );

  if (barWidth > 0) {
    display.drawBox(
      5,
      54,
      barWidth,
      7
    );
  }

  // Tiny raw value in the top-left corner. Useful for confirming
  // that the microphone is producing real changing data.
  display.setFont(u8g2_font_4x6_tf);
  display.setCursor(0, 6);
  display.print("M:");
  display.print(rawLevel);

  display.sendBuffer();
}

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(
    TOUCH_PIN,
    INPUT
  );

  Wire.begin(
    OLED_SDA,
    OLED_SCL
  );

  display.begin();
  display.setContrast(100);

  display.clearBuffer();
  display.setFont(u8g2_font_6x12_tf);
  display.drawStr(13, 20, "EMI C3 FULL TEST");
  display.drawStr(25, 38, "Starting...");
  display.sendBuffer();

  setupMicrophone();

  delay(500);

  Serial.println();
  Serial.println("EMI ESP32-C3 OLED + touch + mic test");
  Serial.println("Touch the sensor and speak/clap near the mic.");
  Serial.println();
}

void loop() {
  bool touched =
    digitalRead(TOUCH_PIN) == HIGH;

  uint32_t micLevel =
    readMicrophoneLevel();

  updateMicMeter(
    micLevel
  );

  drawScreen(
    touched,
    micLevel
  );

  static unsigned long lastSerialPrint = 0;

  if (
    millis() -
    lastSerialPrint >= 100
  ) {
    lastSerialPrint =
      millis();

    Serial.print("touch:");
    Serial.print(touched ? 1 : 0);

    Serial.print(" mic:");
    Serial.println(micLevel);
  }

  delay(15);
}
