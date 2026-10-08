#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "driver/i2s.h"
#include "secrets.h"

// ============================================================
// EMI - C3 END-TO-END VOICE + TIMER TEST v7
//
// This temporary test isolates the voice path:
//
// mic -> acoustic candidate -> Pi wake/intent gates -> command -> OLED
//
// Timer core is owned by the Pi. The C3 keeps a local countdown mirror
// only so the current diagnostic firmware can show expiry without polling
// the network continuously while listening for voice.
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

// Timer phrases are noticeably longer than "Emi time".
// Five seconds prevents natural commands from being chopped by the
// hard maximum while still keeping the entire WAV in RAM.
const int MAX_SAMPLES = SAMPLE_RATE * 5;
const int MIN_SAMPLES = SAMPLE_RATE / 2;

// About 2 seconds of quiet calibration after Wi-Fi is already connected.
const int CALIBRATION_BLOCKS = 125;

// This is only an acoustic candidate trigger, NOT a speech detector.
// Keep it quick so the first syllable of "Emi" is safely inside pre-roll.
// The Pi's explicit EMI wake gate is the real wake-word authority.
const int START_CONFIRM_BLOCKS = 8;

// End-of-speech is based on the LAST real voice activity, not on a
// pre-selected phrase length. EMI keeps recording while the signal stays
// above the learned room-noise release threshold. Only after the sound has
// returned to the room baseline for this long is the phrase considered done.
//
// The hard MAX_SAMPLES limit still exists only as a safety guard so a fan,
// sustained noise, or a stuck detector cannot consume RAM forever.
const unsigned long END_OF_SPEECH_QUIET_MS = 800;

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

unsigned long lastVoiceActivityMs = 0;

float noiseFloor = 0.0f;
float smoothedLevel = 0.0f;

unsigned long lastDebug = 0;
unsigned long cooldownUntil = 0;

bool localTimerActive = false;
unsigned long localTimerDeadline = 0;

bool timerDoneVisible = false;
unsigned long timerDoneUntil = 0;


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


String timerText(
  uint32_t seconds
) {
  if (seconds < 60) {
    return
      String(seconds) +
      "s";
  }

  if (seconds < 3600) {
    uint32_t minutes =
      seconds / 60;

    uint32_t secs =
      seconds % 60;

    String text =
      String(minutes) +
      ":";

    if (secs < 10) {
      text += "0";
    }

    text +=
      String(secs);

    return text;
  }

  uint32_t hours =
    seconds / 3600;

  uint32_t minutes =
    (seconds % 3600) /
    60;

  return
    String(hours) +
    "h " +
    String(minutes) +
    "m";
}


void showTimerValue(
  uint32_t seconds
) {
  String text =
    timerText(
      seconds
    );

  display.clearBuffer();

  display.setFont(
    u8g2_font_logisoso24_tf
  );

  int width =
    display.getStrWidth(
      text.c_str()
    );

  if (width > 124) {
    display.setFont(
      u8g2_font_10x20_tf
    );

    width =
      display.getStrWidth(
        text.c_str()
      );
  }

  display.drawStr(
    max(
      2,
      (128 - width) / 2
    ),
    43,
    text.c_str()
  );

  display.sendBuffer();
}


void startLocalTimerMirror(
  uint32_t seconds
) {
  localTimerActive =
    seconds > 0;

  localTimerDeadline =
    millis() +
    seconds * 1000UL;

  timerDoneVisible =
    false;
}


void clearLocalTimerMirror() {
  localTimerActive =
    false;

  localTimerDeadline =
    0;

  timerDoneVisible =
    false;
}


uint32_t localTimerRemaining() {
  if (!localTimerActive) {
    return 0;
  }

  long delta =
    (long)(
      localTimerDeadline -
      millis()
    );

  if (delta <= 0) {
    return 0;
  }

  return
    (uint32_t)(
      (
        (unsigned long)delta +
        999UL
      )
      /
      1000UL
    );
}


void updateLocalTimer() {
  if (
    localTimerActive &&
    localTimerRemaining() == 0
  ) {
    localTimerActive =
      false;

    timerDoneVisible =
      true;

    timerDoneUntil =
      millis() +
      5000UL;

    Serial.println(
      "TIMER: DONE"
    );

    showStatus(
      "TIMER",
      "DONE"
    );

    return;
  }

  if (
    timerDoneVisible &&
    (long)(
      millis() -
      timerDoneUntil
    )
    >=
    0
  ) {
    timerDoneVisible =
      false;

    showStatus(
      "VOICE TEST",
      "READY"
    );
  }
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
  lastVoiceActivityMs = 0;
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


long jsonInt(
  const String &body,
  const String &key,
  long fallback = -1
) {
  String needle =
    "\"" +
    key +
    "\":";

  int start =
    body.indexOf(
      needle
    );

  if (start < 0) {
    return fallback;
  }

  start +=
    needle.length();

  while (
    start < body.length() &&
    body.charAt(start) == ' '
  ) {
    start++;
  }

  bool negative = false;

  if (
    start < body.length() &&
    body.charAt(start) == '-'
  ) {
    negative = true;
    start++;
  }

  long value = 0;
  bool foundDigit = false;

  while (
    start < body.length() &&
    isDigit(
      body.charAt(start)
    )
  ) {
    foundDigit = true;

    value =
      value * 10 +
      (
        body.charAt(start) -
        '0'
      );

    start++;
  }

  if (!foundDigit) {
    return fallback;
  }

  return
    negative
    ? -value
    : value;
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

  if (intent == "TIMER_SET") {
    long seconds =
      jsonInt(
        body,
        "remaining_seconds",
        -1
      );

    if (seconds > 0) {
      startLocalTimerMirror(
        (uint32_t)seconds
      );

      Serial.print(
        "RESULT: timer set for "
      );

      Serial.print(
        seconds
      );

      Serial.println(
        " seconds."
      );

      showTimerValue(
        (uint32_t)seconds
      );

      delay(1400);

      showStatus(
        "VOICE TEST",
        "READY"
      );
    }

    return;
  }

  if (intent == "TIMER_LEFT") {
    long seconds =
      jsonInt(
        body,
        "remaining_seconds",
        -1
      );

    if (seconds > 0) {
      startLocalTimerMirror(
        (uint32_t)seconds
      );

      Serial.print(
        "RESULT: timer remaining="
      );

      Serial.println(
        seconds
      );

      showTimerValue(
        (uint32_t)seconds
      );

      delay(1800);

      showStatus(
        "VOICE TEST",
        "READY"
      );
    }

    else {
      Serial.println(
        "RESULT: no active timer."
      );

      clearLocalTimerMirror();

      showStatus(
        "TIMER",
        "NONE"
      );

      delay(1000);

      showStatus(
        "VOICE TEST",
        "READY"
      );
    }

    return;
  }

  if (intent == "TIMER_CANCEL") {
    clearLocalTimerMirror();

    Serial.println(
      "RESULT: timer cancelled."
    );

    showStatus(
      "TIMER",
      "CANCELLED"
    );

    delay(1000);

    showStatus(
      "VOICE TEST",
      "READY"
    );

    return;
  }

  if (
    error ==
    "NO_TIMER"
  ) {
    clearLocalTimerMirror();

    Serial.println(
      "RESULT: no active timer."
    );

    showStatus(
      "TIMER",
      "NONE"
    );

    delay(1000);

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

const size_t HTTP_UPLOAD_CHUNK_BYTES = 1024;
const unsigned long HTTP_SEND_STALL_TIMEOUT_MS = 5000;
const unsigned long HTTP_RESPONSE_TIMEOUT_MS = 30000;


bool writeClientAll(
  WiFiClient &client,
  const uint8_t *data,
  size_t length,
  bool reportProgress = false
) {
  size_t sent = 0;
  size_t nextReport = 16384;

  unsigned long lastProgress =
    millis();

  while (sent < length) {
    if (!client.connected()) {
      Serial.print(
        "UPLOAD: connection closed after "
      );

      Serial.print(sent);
      Serial.print("/");
      Serial.print(length);
      Serial.println(" bytes.");

      return false;
    }

    size_t remaining =
      length - sent;

    size_t chunk =
      min(
        HTTP_UPLOAD_CHUNK_BYTES,
        remaining
      );

    size_t written =
      client.write(
        data + sent,
        chunk
      );

    if (written > 0) {
      sent += written;
      lastProgress = millis();

      if (
        reportProgress &&
        (
          sent >= nextReport ||
          sent == length
        )
      ) {
        Serial.print(
          "UPLOAD: sent "
        );

        Serial.print(sent);
        Serial.print("/");
        Serial.print(length);
        Serial.println(" bytes.");

        while (
          nextReport <= sent
        ) {
          nextReport += 16384;
        }
      }

      continue;
    }

    if (
      millis() - lastProgress >=
      HTTP_SEND_STALL_TIMEOUT_MS
    ) {
      Serial.print(
        "UPLOAD: stalled after "
      );

      Serial.print(sent);
      Serial.print("/");
      Serial.print(length);
      Serial.println(" bytes.");

      return false;
    }

    delay(1);
  }

  return true;
}


bool readHttpResponse(
  WiFiClient &client,
  int &status,
  String &body
) {
  status = -1;
  body = "";

  unsigned long started =
    millis();

  while (
    !client.available() &&
    client.connected() &&
    millis() - started <
      HTTP_RESPONSE_TIMEOUT_MS
  ) {
    delay(5);
  }

  if (!client.available()) {
    Serial.println(
      "HTTP response timed out."
    );

    return false;
  }

  client.setTimeout(
    HTTP_RESPONSE_TIMEOUT_MS
  );

  String statusLine =
    client.readStringUntil('\n');

  statusLine.trim();

  Serial.print(
    "HTTP status line: "
  );

  Serial.println(
    statusLine
  );

  int firstSpace =
    statusLine.indexOf(' ');

  if (firstSpace < 0) {
    return false;
  }

  int secondSpace =
    statusLine.indexOf(
      ' ',
      firstSpace + 1
    );

  String statusText =
    secondSpace > firstSpace
    ? statusLine.substring(
        firstSpace + 1,
        secondSpace
      )
    : statusLine.substring(
        firstSpace + 1
      );

  status =
    statusText.toInt();

  int contentLength = -1;

  while (true) {
    String line =
      client.readStringUntil('\n');

    line.trim();

    if (line.length() == 0) {
      break;
    }

    if (
      line.startsWith(
        "Content-Length:"
      ) ||
      line.startsWith(
        "content-length:"
      )
    ) {
      int colon =
        line.indexOf(':');

      if (colon >= 0) {
        String value =
          line.substring(
            colon + 1
          );

        value.trim();

        contentLength =
          value.toInt();
      }
    }
  }

  unsigned long bodyStarted =
    millis();

  if (contentLength >= 0) {
    body.reserve(
      contentLength + 1
    );

    while (
      (int)body.length() <
        contentLength &&
      millis() - bodyStarted <
        HTTP_RESPONSE_TIMEOUT_MS
    ) {
      while (
        client.available() &&
        (int)body.length() <
          contentLength
      ) {
        body +=
          (char)client.read();
      }

      if (
        !client.connected() &&
        !client.available()
      ) {
        break;
      }

      delay(1);
    }
  }

  else {
    while (
      (
        client.connected() ||
        client.available()
      ) &&
      millis() - bodyStarted <
        HTTP_RESPONSE_TIMEOUT_MS
    ) {
      while (
        client.available()
      ) {
        body +=
          (char)client.read();
      }

      delay(1);
    }
  }

  return true;
}


void uploadCapture() {
  if (
    recordedSamples <
    MIN_SAMPLES
  ) {
    Serial.println(
      "Capture too short; ignored."
    );

    return;
  }

  uint32_t pcmBytes =
    (uint32_t)recordedSamples *
    sizeof(int16_t);

  uint32_t wavBytes =
    44 + pcmBytes;

  writeWavHeader(
    wavBuffer,
    pcmBytes
  );

  if (
    WiFi.status() !=
    WL_CONNECTED
  ) {
    Serial.println(
      "UPLOAD: Wi-Fi is not connected."
    );

    return;
  }

  WiFiClient client;

  client.setTimeout(
    HTTP_RESPONSE_TIMEOUT_MS
  );

  Serial.print(
    "Uploading "
  );

  Serial.print(
    recordedSamples
  );

  Serial.print(
    " samples / "
  );

  Serial.print(
    wavBytes
  );

  Serial.println(
    " WAV bytes from RAM..."
  );

  if (
    !client.connect(
      EMI_HUB_HOST,
      EMI_HUB_PORT
    )
  ) {
    Serial.println(
      "UPLOAD: TCP connect failed."
    );

    return;
  }

  String request =
    "POST /device/audio HTTP/1.1\r\n"
    "Host: " +
    String(EMI_HUB_HOST) +
    ":" +
    String(EMI_HUB_PORT) +
    "\r\n"
    "X-EMI-Token: " +
    String(EMI_SHARED_TOKEN) +
    "\r\n"
    "Content-Type: audio/wav\r\n"
    "Content-Length: " +
    String(wavBytes) +
    "\r\n"
    "Connection: close\r\n"
    "\r\n";

  if (
    !writeClientAll(
      client,
      reinterpret_cast<const uint8_t *>(
        request.c_str()
      ),
      request.length(),
      false
    )
  ) {
    Serial.println(
      "RESULT: HTTP headers could not be sent."
    );

    client.stop();
    return;
  }

  Serial.println(
    "UPLOAD: headers sent; streaming WAV..."
  );

  if (
    !writeClientAll(
      client,
      wavBuffer,
      wavBytes,
      true
    )
  ) {
    Serial.println(
      "RESULT: WAV upload failed before completion."
    );

    client.stop();
    return;
  }

  Serial.println(
    "UPLOAD: WAV body complete; waiting for hub..."
  );

  int status = -1;
  String body;

  bool responseOk =
    readHttpResponse(
      client,
      status,
      body
    );

  client.stop();

  Serial.print(
    "HTTP status: "
  );

  Serial.println(
    status
  );

  if (
    !responseOk ||
    status != 200
  ) {
    Serial.println(
      "RESULT: upload/transcription request failed."
    );

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
// TIMER SYNC
// ------------------------------------------------------------

void syncTimerFromHub() {
  if (
    WiFi.status() !=
    WL_CONNECTED
  ) {
    return;
  }

  WiFiClient client;
  HTTPClient http;

  String url =
    "http://" +
    String(EMI_HUB_HOST) +
    ":" +
    String(EMI_HUB_PORT) +
    "/device/timer";

  http.setConnectTimeout(
    1000
  );

  http.setTimeout(
    2000
  );

  if (
    !http.begin(
      client,
      url
    )
  ) {
    return;
  }

  http.addHeader(
    "X-EMI-Token",
    EMI_SHARED_TOKEN
  );

  int status =
    http.GET();

  String body =
    http.getString();

  http.end();

  if (status != 200) {
    return;
  }

  long seconds =
    jsonInt(
      body,
      "remaining_seconds",
      0
    );

  if (
    body.indexOf(
      "\"active\":true"
    )
    >= 0 &&
    seconds > 0
  ) {
    startLocalTimerMirror(
      (uint32_t)seconds
    );

    Serial.print(
      "Timer synced from Pi. remaining="
    );

    Serial.println(
      seconds
    );
  }

  else {
    clearLocalTimerMirror();
  }
}


// ------------------------------------------------------------
// DETECTOR
// ------------------------------------------------------------

void beginCapture() {
  capturing = true;
  recordedSamples = 0;
  startConfirmBlocks = 0;
  lastVoiceActivityMs = millis();

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
  startConfirmBlocks = 0;
  lastVoiceActivityMs = 0;

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

  // "Still speaking" means the current acoustic energy is above the
  // learned release threshold. Every such block pushes the endpoint
  // forward. Word gaps do not end the clip unless the audio stays back
  // at room-noise level continuously for END_OF_SPEECH_QUIET_MS.
  if (
    smoothedLevel >
    releaseThreshold
  ) {
    lastVoiceActivityMs =
      millis();
  }

  unsigned long quietForMs =
    millis() -
    lastVoiceActivityMs;

  bool endBySilence =
    quietForMs >=
    END_OF_SPEECH_QUIET_MS;

  bool endByMaximum =
    recordedSamples >=
    MAX_SAMPLES;

  if (
    endBySilence ||
    endByMaximum
  ) {
    Serial.print(
      "AUDIO: ending by "
    );

    Serial.print(
      endByMaximum
      ? "maximum"
      : "audio stopped"
    );

    Serial.print(
      " duration_ms="
    );

    Serial.print(
      (
        (unsigned long)
          recordedSamples *
        1000UL
      )
      /
      SAMPLE_RATE
    );

    Serial.print(
      " quiet_ms="
    );

    Serial.println(
      quietForMs
    );

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

  syncTimerFromHub();

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
    "Try: Emi time / Emi set a timer for 30 seconds"
  );
  Serial.println(
    "Also: Emi how much time is left / Emi cancel timer"
  );
  Serial.println(
    "v7 streams WAV to the Pi in small chunks."
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

  updateLocalTimer();

  processDetector();
}
