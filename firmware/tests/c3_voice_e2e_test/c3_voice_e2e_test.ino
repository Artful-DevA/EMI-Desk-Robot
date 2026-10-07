#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "driver/i2s.h"
#include "secrets.h"

// ============================================================
// EMI - C3 END-TO-END VOICE TEST v3
//
// This temporary test isolates the voice path:
//
// mic -> VAD -> RAM WAV -> Pi -> Whisper -> intent -> OLED
//
// It deliberately does NOT run Emi's normal personality/polling loop.
// That removes Wi-Fi polling noise and makes VAD tuning much easier.
//
// Board: ESP32C3 Dev Module
//
// OLED SDA  -> GPIO 8
// OLED SCL  -> GPIO 7
// MIC SCK   -> GPIO 4
// MIC WS    -> GPIO 5
// MIC SD    -> GPIO 6
// MIC L/R   -> GND
//
// GPIO 9 remains free for BOOT.
//
// Keep your existing private secrets.h unchanged.
// ============================================================

const int OLED_SDA = 8;
const int OLED_SCL = 7;

const int MIC_SCK = 4;
const int MIC_WS  = 5;
const int MIC_SD  = 6;

static const i2s_port_t I2S_PORT = I2S_NUM_0;

const int SAMPLE_RATE = 16000;
const int BLOCK_SAMPLES = 256;

// Half a second of pre-roll protects short wake phrases such as
// "Emi time" from losing the beginning of the name.
const int PRE_ROLL_SAMPLES = 8000;
const int MAX_SAMPLES = SAMPLE_RATE * 3;
const int MIN_SAMPLES = SAMPLE_RATE / 2;

// About 2 seconds of quiet calibration after Wi-Fi is already connected.
const int CALIBRATION_BLOCKS = 125;

// This is only an acoustic candidate trigger, NOT a speech detector.
// Keep it quick so the first syllable of "Emi" is safely inside pre-roll.
// The Pi's dedicated keyword spotter is the real wake-word authority.
const int START_CONFIRM_BLOCKS = 8;

// About 480 ms of quiet ends the phrase.
const int END_SILENT_BLOCKS = 30;

// Tuned from the real measurements we saw.
// The sustained requirement is the main false-trigger protection.
const float START_MARGIN = 1200.0f;
const float RELEASE_MARGIN = 500.0f;
const float MIN_START_THRESHOLD = 5000.0f;
const float MIN_RELEASE_THRESHOLD = 4300.0f;

// Do not amplify into hard 16-bit clipping before recognition.
const int PCM_GAIN = 1;

const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;
const unsigned long RETRY_PAUSE_MS = 2000;
const unsigned long COOLDOWN_MS = 1800;

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(
  U8G2_R0,
  U8X8_PIN_NONE
);

int32_t i2sBlock[BLOCK_SAMPLES];
int16_t convertedBlock[BLOCK_SAMPLES];

int16_t preRoll[PRE_ROLL_SAMPLES];
int preRollWrite = 0;
int preRollCount = 0;

alignas(4) uint8_t wavBuffer[
  44 + MAX_SAMPLES * sizeof(int16_t)
];

int16_t *recordedPcm =
  reinterpret_cast<int16_t *>(
    wavBuffer + 44
  );

bool capturing = false;

int recordedSamples = 0;
int startConfirmBlocks = 0;
int silentBlocks = 0;

float noiseFloor = 0.0f;
float smoothedLevel = 0.0f;

unsigned long lastDebug = 0;
unsigned long cooldownUntil = 0;


// ------------------------------------------------------------
// OLED
// ------------------------------------------------------------

void showStatus(
  const char *line1,
  const char *line2 = ""
) {
  display.clearBuffer();

  display.setFont(
    u8g2_font_6x12_tf
  );

  display.drawStr(
    4,
    23,
    line1
  );

  display.drawStr(
    4,
    43,
    line2
  );

  display.sendBuffer();
}


void showTime(
  const String &hhmm
) {
  display.clearBuffer();

  display.setFont(
    u8g2_font_logisoso24_tf
  );

  int width =
    display.getStrWidth(
      hhmm.c_str()
    );

  display.drawStr(
    (128 - width) / 2,
    43,
    hhmm.c_str()
  );

  display.sendBuffer();
}


// ------------------------------------------------------------
// MICROPHONE
// ------------------------------------------------------------

bool setupMicrophone() {
  i2s_config_t config = {};

  config.mode =
    (i2s_mode_t)(
      I2S_MODE_MASTER |
      I2S_MODE_RX
    );

  config.sample_rate =
    SAMPLE_RATE;

  config.bits_per_sample =
    I2S_BITS_PER_SAMPLE_32BIT;

  config.channel_format =
    I2S_CHANNEL_FMT_ONLY_LEFT;

  config.communication_format =
    I2S_COMM_FORMAT_STAND_I2S;

  config.intr_alloc_flags =
    ESP_INTR_FLAG_LEVEL1;

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
    return false;
  }

  result =
    i2s_set_pin(
      I2S_PORT,
      &pins
    );

  if (result != ESP_OK) {
    Serial.print("I2S pin setup failed: ");
    Serial.println((int)result);

    i2s_driver_uninstall(
      I2S_PORT
    );

    return false;
  }

  i2s_zero_dma_buffer(
    I2S_PORT
  );

  return true;
}


// ------------------------------------------------------------
// WIFI
// ------------------------------------------------------------

bool findTargetAP(
  int32_t &channel,
  uint8_t bssid[6]
) {
  Serial.println(
    "Wi-Fi scan: looking for configured network..."
  );

  int count =
    WiFi.scanNetworks(
      false,
      true
    );

  if (count < 0) {
    Serial.println(
      "Wi-Fi scan failed."
    );
    return false;
  }

  int bestIndex = -1;
  int32_t bestRssi = -1000;

  for (int i = 0; i < count; i++) {
    if (
      WiFi.SSID(i) == EMI_WIFI_SSID &&
      WiFi.RSSI(i) > bestRssi
    ) {
      bestIndex = i;
      bestRssi = WiFi.RSSI(i);
    }
  }

  if (bestIndex < 0) {
    WiFi.scanDelete();

    Serial.println(
      "Configured Wi-Fi not found."
    );

    return false;
  }

  channel =
    WiFi.channel(
      bestIndex
    );

  memcpy(
    bssid,
    WiFi.BSSID(bestIndex),
    6
  );

  Serial.print(
    "Wi-Fi target found. RSSI="
  );

  Serial.print(
    bestRssi
  );

  Serial.print(
    " dBm channel="
  );

  Serial.println(
    channel
  );

  WiFi.scanDelete();

  return true;
}


bool connectWiFi() {
  if (
    WiFi.status() ==
    WL_CONNECTED
  ) {
    return true;
  }

  int32_t channel = 0;

  uint8_t bssid[6] =
    {0, 0, 0, 0, 0, 0};

  if (
    !findTargetAP(
      channel,
      bssid
    )
  ) {
    return false;
  }

  Serial.println(
    "Connecting to Wi-Fi..."
  );

  WiFi.begin(
    EMI_WIFI_SSID,
    EMI_WIFI_PASSWORD,
    channel,
    bssid,
    true
  );

  unsigned long started =
    millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - started <
      WIFI_CONNECT_TIMEOUT_MS
  ) {
    delay(100);
  }

  if (
    WiFi.status() !=
    WL_CONNECTED
  ) {
    Serial.println(
      "Wi-Fi connection timed out."
    );

    WiFi.disconnect(
      false,
      false
    );

    return false;
  }

  Serial.print(
    "Wi-Fi connected. IP="
  );

  Serial.println(
    WiFi.localIP()
  );

  return true;
}


// ------------------------------------------------------------
// AUDIO
// ------------------------------------------------------------

int16_t toPcm(
  int32_t centered
) {
  int64_t value =
    (int64_t)centered *
    PCM_GAIN;

  if (value > 32767) {
    value = 32767;
  }

  if (value < -32768) {
    value = -32768;
  }

  return (int16_t)value;
}


float readAudioBlock() {
  size_t bytesRead = 0;

  esp_err_t result =
    i2s_read(
      I2S_PORT,
      i2sBlock,
      sizeof(i2sBlock),
      &bytesRead,
      portMAX_DELAY
    );

  if (
    result != ESP_OK ||
    bytesRead == 0
  ) {
    return -1.0f;
  }

  int count =
    bytesRead /
    sizeof(int32_t);

  int64_t sum = 0;

  for (int i = 0; i < count; i++) {
    sum +=
      i2sBlock[i] >> 8;
  }

  int32_t mean =
    (int32_t)(
      sum / count
    );

  uint64_t magnitudeSum = 0;

  for (int i = 0; i < count; i++) {
    int32_t centered =
      (i2sBlock[i] >> 8) -
      mean;

    int32_t magnitude =
      centered >= 0
      ? centered
      : -centered;

    magnitudeSum +=
      (uint32_t)magnitude;

    convertedBlock[i] =
      toPcm(
        centered
      );
  }

  for (int i = 0; i < count; i++) {
    preRoll[
      preRollWrite
    ] =
      convertedBlock[i];

    preRollWrite =
      (preRollWrite + 1) %
      PRE_ROLL_SAMPLES;

    if (
      preRollCount <
      PRE_ROLL_SAMPLES
    ) {
      preRollCount++;
    }
  }

  return
    (float)(
      magnitudeSum / count
    );
}


// ------------------------------------------------------------
// CALIBRATION
// ------------------------------------------------------------

void calibrateRoom() {
  showStatus(
    "VOICE TEST",
    "STAY QUIET"
  );

  Serial.println();
  Serial.println(
    "Calibrating microphone AFTER Wi-Fi connection."
  );
  Serial.println(
    "Stay quiet for about 2 seconds."
  );

  noiseFloor = 0.0f;
  smoothedLevel = 0.0f;

  for (
    int i = 0;
    i < CALIBRATION_BLOCKS;
    i++
  ) {
    float level =
      readAudioBlock();

    if (level < 0) {
      i--;
      continue;
    }

    if (i == 0) {
      noiseFloor = level;
      smoothedLevel = level;
    }

    else {
      noiseFloor =
        noiseFloor * 0.92f +
        level * 0.08f;

      smoothedLevel =
        smoothedLevel * 0.75f +
        level * 0.25f;
    }
  }

  Serial.print(
    "Calibration complete. noise="
  );

  Serial.println(
    noiseFloor
  );

  startConfirmBlocks = 0;
  silentBlocks = 0;
  capturing = false;
  recordedSamples = 0;

  cooldownUntil =
    millis() + 500;

  showStatus(
    "VOICE TEST",
    "READY"
  );
}


// ------------------------------------------------------------
// WAV
// ------------------------------------------------------------

void writeWavHeader(
  uint8_t *buffer,
  uint32_t pcmBytes
) {
  uint32_t riffSize =
    36 + pcmBytes;

  uint32_t byteRate =
    SAMPLE_RATE * 2;

  memcpy(buffer + 0, "RIFF", 4);

  buffer[4] =
    (uint8_t)riffSize;
  buffer[5] =
    (uint8_t)(riffSize >> 8);
  buffer[6] =
    (uint8_t)(riffSize >> 16);
  buffer[7] =
    (uint8_t)(riffSize >> 24);

  memcpy(buffer + 8, "WAVEfmt ", 8);

  buffer[16] = 16;
  buffer[17] = 0;
  buffer[18] = 0;
  buffer[19] = 0;

  buffer[20] = 1;
  buffer[21] = 0;

  buffer[22] = 1;
  buffer[23] = 0;

  buffer[24] =
    (uint8_t)SAMPLE_RATE;
  buffer[25] =
    (uint8_t)(SAMPLE_RATE >> 8);
  buffer[26] =
    (uint8_t)(SAMPLE_RATE >> 16);
  buffer[27] =
    (uint8_t)(SAMPLE_RATE >> 24);

  buffer[28] =
    (uint8_t)byteRate;
  buffer[29] =
    (uint8_t)(byteRate >> 8);
  buffer[30] =
    (uint8_t)(byteRate >> 16);
  buffer[31] =
    (uint8_t)(byteRate >> 24);

  buffer[32] = 2;
  buffer[33] = 0;

  buffer[34] = 16;
  buffer[35] = 0;

  memcpy(buffer + 36, "data", 4);

  buffer[40] =
    (uint8_t)pcmBytes;
  buffer[41] =
    (uint8_t)(pcmBytes >> 8);
  buffer[42] =
    (uint8_t)(pcmBytes >> 16);
  buffer[43] =
    (uint8_t)(pcmBytes >> 24);
}


// ------------------------------------------------------------
// RESPONSE PARSING
// ------------------------------------------------------------

String jsonString(
  const String &body,
  const String &key
) {
  String needle =
    "\"" +
    key +
    "\":\"";

  int start =
    body.indexOf(
      needle
    );

  if (start < 0) {
    return "";
  }

  start +=
    needle.length();

  int end =
    body.indexOf(
      '\"',
      start
    );

  if (end < 0) {
    return "";
  }

  return
    body.substring(
      start,
      end
    );
}


void showHubResult(
  const String &body
) {
  String intent =
    jsonString(
      body,
      "intent"
    );

  String error =
    jsonString(
      body,
      "error"
    );

  if (intent == "TIME") {
    String hhmm =
      jsonString(
        body,
        "time"
      );

    Serial.println(
      "RESULT: TIME intent matched."
    );

    Serial.print(
      "RESULT: time="
    );

    Serial.println(
      hhmm
    );

    if (
      hhmm.length() == 5
    ) {
      showTime(hhmm);
      delay(2200);
    }

    showStatus(
      "VOICE TEST",
      "READY"
    );

    return;
  }

  if (
    error ==
    "NO_WAKE_WORD"
  ) {
    Serial.println(
      "RESULT: no EMI wake word; ignored."
    );
  }

  else if (
    error ==
    "NO_SPEECH"
  ) {
    Serial.println(
      "RESULT: no speech; ignored."
    );
  }

  else if (
    error ==
    "NO_MATCH"
  ) {
    Serial.println(
      "RESULT: EMI wake detected, but command did not match."
    );
  }

  else {
    Serial.println(
      "RESULT: unknown hub response."
    );
  }

  // Failure/noise is intentionally invisible on the OLED.
  showStatus(
    "VOICE TEST",
    "READY"
  );
}


// ------------------------------------------------------------
// UPLOAD
// ------------------------------------------------------------

void uploadCapture() {
  if (
    recordedSamples <
    MIN_SAMPLES
  ) {
    Serial.println(
      "Capture too short; ignored."
    );

    // Too-short acoustic candidates stay invisible on the OLED.
    return;
  }

  uint32_t pcmBytes =
    (uint32_t)recordedSamples *
    sizeof(int16_t);

  writeWavHeader(
    wavBuffer,
    pcmBytes
  );

  WiFiClient client;
  HTTPClient http;

  String url =
    "http://" +
    String(EMI_HUB_HOST) +
    ":" +
    String(EMI_HUB_PORT) +
    "/device/audio";

  http.setConnectTimeout(
    1500
  );

  http.setTimeout(
    30000
  );

  if (
    !http.begin(
      client,
      url
    )
  ) {
    Serial.println(
      "HTTP begin failed."
    );
    return;
  }

  http.addHeader(
    "X-EMI-Token",
    EMI_SHARED_TOKEN
  );

  http.addHeader(
    "Content-Type",
    "audio/wav"
  );

  // Do not change the OLED while checking an acoustic candidate.
  // Typing/tapping may create candidates but must remain invisible.
  Serial.print(
    "Uploading "
  );

  Serial.print(
    recordedSamples
  );

  Serial.println(
    " samples from RAM..."
  );

  int status =
    http.POST(
      wavBuffer,
      44 + pcmBytes
    );

  String body =
    http.getString();

  http.end();

  Serial.print(
    "HTTP status: "
  );

  Serial.println(
    status
  );

  if (status != 200) {
    Serial.println(
      "RESULT: upload/transcription request failed."
    );

    // Network errors stay diagnostic-only; do not make ordinary
    // acoustic noise look like a user-facing interaction.
    showStatus(
      "VOICE TEST",
      "READY"
    );

    return;
  }

  showHubResult(
    body
  );
}


// ------------------------------------------------------------
// DETECTOR
// ------------------------------------------------------------

void beginCapture() {
  capturing = true;
  recordedSamples = 0;
  silentBlocks = 0;
  startConfirmBlocks = 0;

  int start =
    (
      preRollWrite -
      preRollCount +
      PRE_ROLL_SAMPLES
    )
    %
    PRE_ROLL_SAMPLES;

  for (
    int i = 0;
    i < preRollCount &&
    recordedSamples < MAX_SAMPLES;
    i++
  ) {
    int index =
      (start + i) %
      PRE_ROLL_SAMPLES;

    recordedPcm[
      recordedSamples++
    ] =
      preRoll[index];
  }

  // Important: this means only "acoustic candidate".
  // Do NOT change the OLED here. Typing/tapping can create acoustic
  // candidates and must never look like Emi has started listening.
  Serial.println(
    "AUDIO: candidate start"
  );
}


void finishCapture() {
  capturing = false;

  Serial.print(
    "AUDIO: candidate end. samples="
  );

  Serial.println(
    recordedSamples
  );

  uploadCapture();

  recordedSamples = 0;
  silentBlocks = 0;
  startConfirmBlocks = 0;

  i2s_zero_dma_buffer(
    I2S_PORT
  );

  cooldownUntil =
    millis() +
    COOLDOWN_MS;
}


void processDetector() {
  float level =
    readAudioBlock();

  if (level < 0) {
    return;
  }

  if (
    smoothedLevel <= 0.0f
  ) {
    smoothedLevel = level;
  }

  else {
    smoothedLevel =
      smoothedLevel * 0.78f +
      level * 0.22f;
  }

  // Adapt downward fairly quickly when the room becomes quieter,
  // but upward very slowly so actual speech does not become "silence".
  if (!capturing) {
    if (
      smoothedLevel <
      noiseFloor
    ) {
      noiseFloor =
        noiseFloor * 0.97f +
        smoothedLevel * 0.03f;
    }

    else if (
      smoothedLevel <
      noiseFloor + 500.0f
    ) {
      noiseFloor =
        noiseFloor * 0.999f +
        smoothedLevel * 0.001f;
    }
  }

  float startThreshold =
    max(
      MIN_START_THRESHOLD,
      noiseFloor +
      START_MARGIN
    );

  float releaseThreshold =
    max(
      MIN_RELEASE_THRESHOLD,
      noiseFloor +
      RELEASE_MARGIN
    );

  if (
    !capturing &&
    millis() - lastDebug >= 1000
  ) {
    lastDebug = millis();

    Serial.print(
      "level="
    );

    Serial.print(
      level
    );

    Serial.print(
      " smooth="
    );

    Serial.print(
      smoothedLevel
    );

    Serial.print(
      " noise="
    );

    Serial.print(
      noiseFloor
    );

    Serial.print(
      " start="
    );

    Serial.print(
      startThreshold
    );

    Serial.print(
      " confirm="
    );

    Serial.println(
      startConfirmBlocks
    );
  }

  if (
    millis() <
    cooldownUntil
  ) {
    startConfirmBlocks = 0;
    return;
  }

  if (!capturing) {
    if (
      smoothedLevel >
      startThreshold
    ) {
      startConfirmBlocks++;
    }

    else {
      startConfirmBlocks = 0;
    }

    if (
      startConfirmBlocks >=
      START_CONFIRM_BLOCKS
    ) {
      beginCapture();
    }

    return;
  }

  int count =
    BLOCK_SAMPLES;

  for (
    int i = 0;
    i < count &&
    recordedSamples < MAX_SAMPLES;
    i++
  ) {
    recordedPcm[
      recordedSamples++
    ] =
      convertedBlock[i];
  }

  if (
    smoothedLevel >
    releaseThreshold
  ) {
    silentBlocks = 0;
  }

  else {
    silentBlocks++;
  }

  bool endBySilence =
    silentBlocks >=
    END_SILENT_BLOCKS;

  bool endByMaximum =
    recordedSamples >=
    MAX_SAMPLES;

  if (
    endBySilence ||
    endByMaximum
  ) {
    finishCapture();
  }
}


// ------------------------------------------------------------
// SETUP / LOOP
// ------------------------------------------------------------

void setup() {
  Serial.begin(
    115200
  );

  delay(300);

  Wire.begin(
    OLED_SDA,
    OLED_SCL
  );

  display.begin();

  display.setContrast(
    100
  );

  showStatus(
    "VOICE TEST",
    "BOOTING"
  );

  if (
    !setupMicrophone()
  ) {
    showStatus(
      "MIC ERROR",
      "SEE SERIAL"
    );

    while (true) {
      delay(1000);
    }
  }

  WiFi.persistent(
    false
  );

  WiFi.setAutoReconnect(
    false
  );

  WiFi.mode(
    WIFI_STA
  );

  WiFi.setSleep(
    false
  );

  bool powerOk =
    WiFi.setTxPower(
      WIFI_POWER_8_5dBm
    );

  Serial.print(
    "Wi-Fi TX 8.5 dBm: "
  );

  Serial.println(
    powerOk
    ? "OK"
    : "FAILED"
  );

  while (
    !connectWiFi()
  ) {
    showStatus(
      "VOICE TEST",
      "WIFI RETRY"
    );

    delay(
      RETRY_PAUSE_MS
    );
  }

  // Critical difference from the earlier firmware:
  // calibrate AFTER Wi-Fi has connected and settled.
  delay(750);

  calibrateRoom();

  Serial.println();
  Serial.println(
    "================================"
  );
  Serial.println(
    "EMI VOICE TEST READY"
  );
  Serial.println(
    "OLED stays READY for random noise."
  );
  Serial.println(
    "Try: Emi time   OR   Time Emi"
  );
  Serial.println(
    "================================"
  );
}


void loop() {
  if (
    WiFi.status() !=
    WL_CONNECTED
  ) {
    showStatus(
      "VOICE TEST",
      "WIFI LOST"
    );

    while (
      !connectWiFi()
    ) {
      delay(
        RETRY_PAUSE_MS
      );
    }

    delay(750);

    calibrateRoom();
  }

  processDetector();
}
