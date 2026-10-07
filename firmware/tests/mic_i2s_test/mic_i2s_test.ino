#include <Arduino.h>
#include "driver/i2s.h"

// ============================================================
// EMI - I2S Microphone Test
// ESP32 DevKit V1
//
// Wiring:
//   Mic GND     -> ESP32 GND
//   Mic VCC/VDD -> ESP32 3V3
//   Mic L/R     -> ESP32 GND
//   Mic WS      -> GPIO 25
//   Mic SCK     -> GPIO 26
//   Mic SD/SA   -> GPIO 32
//
// What this test does:
// - starts the microphone at 16 kHz
// - reads raw I2S audio
// - removes the DC offset
// - prints LEVEL and PEAK to Serial
//
// Open Serial Monitor or Serial Plotter at 115200 baud.
// Speak or clap near the mic. LEVEL and PEAK should jump.
// ============================================================

static const i2s_port_t I2S_PORT = I2S_NUM_0;

static const int PIN_I2S_WS  = 25;
static const int PIN_I2S_SCK = 26;
static const int PIN_I2S_SD  = 32;

static const int SAMPLE_RATE = 16000;
static const int SAMPLE_COUNT = 256;

int32_t samples[SAMPLE_COUNT];

void setupI2SMicrophone() {
  i2s_config_t i2sConfig = {};

  i2sConfig.mode =
    (i2s_mode_t)(
      I2S_MODE_MASTER |
      I2S_MODE_RX
    );

  i2sConfig.sample_rate = SAMPLE_RATE;

  i2sConfig.bits_per_sample =
    I2S_BITS_PER_SAMPLE_32BIT;

  i2sConfig.channel_format =
    I2S_CHANNEL_FMT_ONLY_LEFT;

  i2sConfig.communication_format =
    I2S_COMM_FORMAT_STAND_I2S;

  i2sConfig.intr_alloc_flags =
    ESP_INTR_FLAG_LEVEL1;

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

    while (true) {
      delay(1000);
    }
  }

  i2s_zero_dma_buffer(I2S_PORT);
}

void setup() {
  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("EMI I2S microphone test starting...");
  Serial.println("Speak or clap near the microphone.");
  Serial.println("Open Serial Plotter for an easy visual test.");
  Serial.println();

  setupI2SMicrophone();
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

  // Most 24-bit I2S microphones deliver their sample
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

  delay(20);
}
