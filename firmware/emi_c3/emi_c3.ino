#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "driver/i2s.h"
#include "freertos/semphr.h"
#include "secrets.h"

// ============================================================
// EMI - Normal ESP32-C3 Firmware + SHOW_TIME command
// Board: ESP32C3 Dev Module
// Controller: ESP32-C3 Super Mini
//
// Verified hardware map:
//   OLED SDA  -> GPIO 8
//   OLED SCL  -> GPIO 7
//   TOUCH OUT -> GPIO 10
//   MIC SCK   -> GPIO 4
//   MIC WS    -> GPIO 5
//   MIC SD    -> GPIO 6
//   MIC L/R   -> GND
//
// Commands can arrive from Serial or from the authenticated
// Raspberry Pi hub connection over Wi-Fi:
//   SHOW_TIME HH:MM
//
// Example:
//   SHOW_TIME 14:37
//
// Emi closes his eyes, morphs into a large 7-segment clock,
// holds the time briefly, then morphs back into his normal face.
//
// The microphone is physically connected and verified. Normal firmware
// now runs a small local VAD, captures short voice commands in RAM, and
// uploads an in-memory WAV to the Raspberry Pi for local Whisper.
// Raw command audio is never intentionally written to disk by EMI.
// ============================================================


// ------------------------------------------------------------
// VERIFIED C3 PINS
// ------------------------------------------------------------

const int OLED_SDA = 8;
const int OLED_SCL = 7;
// GPIO 9 is intentionally left free for BOOT/download mode.

const int TOUCH_PIN = 10;

const int MIC_SCK = 4;
const int MIC_WS = 5;
const int MIC_SD = 6;


// ------------------------------------------------------------
// NETWORK
// ------------------------------------------------------------

const unsigned long HUB_POLL_MS = 250;
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;
const unsigned long WIFI_RETRY_PAUSE_MS = 2000;

struct NetworkCommand {
  char text[48];
};

QueueHandle_t networkCommandQueue = nullptr;

volatile int lastWiFiDisconnectReason = -1;

SemaphoreHandle_t httpMutex = nullptr;


// ------------------------------------------------------------
// VOICE / MICROPHONE
// ------------------------------------------------------------

static const i2s_port_t I2S_PORT = I2S_NUM_0;

const int VOICE_SAMPLE_RATE = 16000;
const int VOICE_I2S_BLOCK_SAMPLES = 256;
const int VOICE_PRE_ROLL_SAMPLES = 4096;
const int VOICE_MAX_SAMPLES = VOICE_SAMPLE_RATE * 3;
const int VOICE_MIN_SAMPLES = VOICE_SAMPLE_RATE / 2;

const int VOICE_CALIBRATION_BLOCKS = 75;
const int VOICE_START_BLOCKS = 3;
const int VOICE_END_SILENT_BLOCKS = 44;

// Real C3 mic measurements showed a room/noise floor around 4k-6k
// while speech peaks were only around 7k+. The original 3x threshold
// therefore made speech mathematically impossible to trigger.
// Use an additive margin instead: learned floor + 900, with a sane
// minimum threshold to avoid very quiet-room false starts.
const float VOICE_THRESHOLD_MULTIPLIER = 1.0f;
const float VOICE_THRESHOLD_OFFSET = 900.0f;
const float VOICE_MIN_THRESHOLD = 4500.0f;
const int VOICE_PCM_GAIN = 4;

int32_t voiceI2SBlock[VOICE_I2S_BLOCK_SAMPLES];
int16_t voicePreRoll[VOICE_PRE_ROLL_SAMPLES];

alignas(4) uint8_t voiceWavBuffer[
  44 +
  VOICE_MAX_SAMPLES * sizeof(int16_t)
];

volatile bool voiceMonitorReady = false;
volatile bool voiceCapturing = false;
volatile bool voiceProcessing = false;


// ------------------------------------------------------------
// OLED
// ------------------------------------------------------------

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(
  U8G2_R0,
  U8X8_PIN_NONE
);


// ------------------------------------------------------------
// EYES
// ------------------------------------------------------------

const int EYE_WIDTH = 23;
const int EYE_HEIGHT = 31;

const int LEFT_EYE_X = 24;
const int RIGHT_EYE_X = 81;
const int EYE_CENTER_Y = 32;


// ------------------------------------------------------------
// GAZE
// ------------------------------------------------------------

float gazeX = 0.0f;
float gazeY = 0.0f;

float gazeStartX = 0.0f;
float gazeStartY = 0.0f;

float gazeTargetX = 0.0f;
float gazeTargetY = 0.0f;

bool gazeMoving = false;

unsigned long gazeStartTime = 0;
unsigned long gazeDuration = 100;


// ------------------------------------------------------------
// PERSONALITY DRIVES
// ------------------------------------------------------------

float curiosity = 0.38f;
float contentment = 0.25f;

unsigned long lastDriveUpdate = 0;


// ------------------------------------------------------------
// IDLE ACTIONS
// ------------------------------------------------------------

enum IdleAction {
  IDLE_STILL = 0,
  IDLE_MICRO_GLANCE,
  IDLE_FOCUS,
  IDLE_CURIOUS_PEEK,
  IDLE_WIDE_GLANCE,
  IDLE_SETTLE,
  IDLE_SOFT_ATTENTION,
  IDLE_ACTION_COUNT
};

IdleAction lastIdleAction = IDLE_SETTLE;
IdleAction previousIdleAction = IDLE_STILL;

unsigned long nextIdleAction = 0;


enum IdleFollowup {
  FOLLOWUP_NONE = 0,
  FOLLOWUP_TINY_CORRECTION,
  FOLLOWUP_RETURN_NEAR_CENTER,
  FOLLOWUP_PEEK_SETTLE
};

IdleFollowup idleFollowup = FOLLOWUP_NONE;

unsigned long idleFollowupTime = 0;


// ------------------------------------------------------------
// BLINKING
// ------------------------------------------------------------

bool blinking = false;

unsigned long blinkStartTime = 0;

int blinkCloseTime = 65;
int blinkHoldTime = 18;
int blinkOpenTime = 95;

unsigned long nextBlinkTime = 0;

bool doubleBlinkPending = false;
unsigned long secondBlinkTime = 0;


// ------------------------------------------------------------
// ATTENTION BIDS
// ------------------------------------------------------------

enum AttentionBidState {
  ATTENTION_NONE = 0,
  ATTENTION_LOOK,
  ATTENTION_FIRST_BLINK,
  ATTENTION_PAUSE,
  ATTENTION_SECOND_BLINK,
  ATTENTION_SETTLE
};

AttentionBidState attentionState = ATTENTION_NONE;

unsigned long attentionStageTime = 0;
unsigned long nextAttentionBid = 0;
unsigned long lastInteractionTime = 0;


// ------------------------------------------------------------
// TOUCH / PETTING
// ------------------------------------------------------------

bool rawTouch = false;
bool previousRawTouch = false;
bool stableTouch = false;

unsigned long touchChangedAt = 0;

const unsigned long TOUCH_DEBOUNCE = 30;
const unsigned long PET_GRACE_TIME = 2200;
const unsigned long MIN_PAT_INTERVAL = 280;

bool petting = false;

unsigned long lastTouchTime = 0;
unsigned long lastRecognizedPat = 0;

int patCount = 0;

unsigned long nextPetDrift = 0;


// ------------------------------------------------------------
// PET STROKE EXPRESSION
// ------------------------------------------------------------

bool petStrokeActive = false;
unsigned long petStrokeStart = 0;

const unsigned long PET_STROKE_DURATION = 360;

float petStrokeMaxClosure = 0.27f;

bool relaxedBlinkPending = false;
bool relaxedBlinkDone = false;


// ------------------------------------------------------------
// CLOCK UI
// ------------------------------------------------------------

enum ClockState {
  CLOCK_OFF = 0,
  CLOCK_EYES_CLOSING,
  CLOCK_REVEALING,
  CLOCK_HOLDING,
  CLOCK_HIDING,
  CLOCK_EYES_OPENING
};

ClockState clockState = CLOCK_OFF;

unsigned long clockStageStart = 0;

const unsigned long CLOCK_EYE_CLOSE_MS = 420;
const unsigned long CLOCK_REVEAL_MS = 440;
const unsigned long CLOCK_HOLD_MS = 2000;
const unsigned long CLOCK_HIDE_MS = 380;
const unsigned long CLOCK_EYE_OPEN_MS = 480;

int shownHour = 12;
int shownMinute = 34;


// ------------------------------------------------------------
// SERIAL COMMAND INPUT
// ------------------------------------------------------------

String serialCommand;


// ------------------------------------------------------------
// HELPERS
// ------------------------------------------------------------

float clampFloat(
  float value,
  float minimum,
  float maximum
) {

  if (value < minimum) {
    return minimum;
  }

  if (value > maximum) {
    return maximum;
  }

  return value;
}


float smoothStep(float t) {

  t =
    clampFloat(
      t,
      0.0f,
      1.0f
    );

  return
    t *
    t *
    (
      3.0f -
      2.0f * t
    );
}


// ------------------------------------------------------------
// PERSONALITY
// ------------------------------------------------------------

void updatePersonalityDrives() {

  unsigned long now =
    millis();


  if (
    now -
    lastDriveUpdate <
    1000
  ) {

    return;
  }


  lastDriveUpdate =
    now;


  if (petting) {

    curiosity -=
      0.008f;

    contentment +=
      0.004f;
  }

  else {

    curiosity +=
      0.012f;

    contentment -=
      0.0025f;
  }


  curiosity =
    clampFloat(
      curiosity,
      0.0f,
      1.0f
    );


  contentment =
    clampFloat(
      contentment,
      0.0f,
      1.0f
    );
}


// ------------------------------------------------------------
// BLINK ANIMATION
// ------------------------------------------------------------

float getBlinkClosure() {

  if (!blinking) {
    return 0.0f;
  }


  unsigned long elapsed =
    millis() -
    blinkStartTime;


  unsigned long totalTime =
    blinkCloseTime +
    blinkHoldTime +
    blinkOpenTime;


  if (
    elapsed <
    (unsigned long)
      blinkCloseTime
  ) {

    float t =
      (float)elapsed /
      (float)blinkCloseTime;


    return
      smoothStep(t);
  }


  if (
    elapsed <
    (unsigned long)(
      blinkCloseTime +
      blinkHoldTime
    )
  ) {

    return 1.0f;
  }


  if (
    elapsed <
    totalTime
  ) {

    float t =
      (float)(
        elapsed -
        blinkCloseTime -
        blinkHoldTime
      )
      /
      (float)
        blinkOpenTime;


    return
      1.0f -
      smoothStep(t);
  }


  blinking =
    false;


  return 0.0f;
}


// ------------------------------------------------------------
// PET STROKE ANIMATION
// ------------------------------------------------------------

float getPetStrokeClosure() {

  if (!petStrokeActive) {
    return 0.0f;
  }


  unsigned long elapsed =
    millis() -
    petStrokeStart;


  if (
    elapsed >=
    PET_STROKE_DURATION
  ) {

    petStrokeActive =
      false;


    return 0.0f;
  }


  float t =
    (float)elapsed /
    (float)
      PET_STROKE_DURATION;


  if (t < 0.24f) {

    return
      petStrokeMaxClosure *
      smoothStep(
        t /
        0.24f
      );
  }


  if (t < 0.36f) {

    return
      petStrokeMaxClosure;
  }


  float openT =
    (t - 0.36f) /
    0.64f;


  return
    petStrokeMaxClosure *
    (
      1.0f -
      smoothStep(
        openT
      )
    );
}


// ------------------------------------------------------------
// FACE DRAWING
// ------------------------------------------------------------

void drawEyesWithClosure(float forcedClosure) {

  float closure =
    max(
      forcedClosure,
      getBlinkClosure()
    );


  float petClosure =
    getPetStrokeClosure();


  if (
    petClosure >
    closure
  ) {

    closure =
      petClosure;
  }


  int eyeHeight =
    EYE_HEIGHT -
    (int)(
      closure *
      (
        EYE_HEIGHT -
        3
      )
    );


  int inward =
    petting
    ? 2
    : 0;


  int y =
    EYE_CENTER_Y -
    eyeHeight / 2 +
    round(gazeY);


  int radius =
    min(
      7,
      eyeHeight / 2
    );


  display.drawRBox(
    LEFT_EYE_X +
      round(gazeX) +
      inward,

    y,

    EYE_WIDTH,
    eyeHeight,
    radius
  );


  display.drawRBox(
    RIGHT_EYE_X +
      round(gazeX) -
      inward,

    y,

    EYE_WIDTH,
    eyeHeight,
    radius
  );
}


// ------------------------------------------------------------
// 7-SEGMENT CLOCK DRAWING
// ------------------------------------------------------------

// Segment bits:
//   A
// F   B
//   G
// E   C
//   D

const uint8_t SEG_A = 1 << 0;
const uint8_t SEG_B = 1 << 1;
const uint8_t SEG_C = 1 << 2;
const uint8_t SEG_D = 1 << 3;
const uint8_t SEG_E = 1 << 4;
const uint8_t SEG_F = 1 << 5;
const uint8_t SEG_G = 1 << 6;


uint8_t digitSegments(int digit) {

  static const uint8_t map[10] = {
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,
    SEG_B | SEG_C,
    SEG_A | SEG_B | SEG_G | SEG_E | SEG_D,
    SEG_A | SEG_B | SEG_G | SEG_C | SEG_D,
    SEG_F | SEG_G | SEG_B | SEG_C,
    SEG_A | SEG_F | SEG_G | SEG_C | SEG_D,
    SEG_A | SEG_F | SEG_G | SEG_E | SEG_C | SEG_D,
    SEG_A | SEG_B | SEG_C,
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G
  };


  if (
    digit < 0 ||
    digit > 9
  ) {

    return 0;
  }


  return map[digit];
}


void drawSegmentDigit(
  int x,
  int y,
  int digit
) {

  // Keep clock digits slightly smaller than the normal eyes so the
  // two time groups sit naturally inside the same visual face area.
  const int w = 14;
  const int h = 27;
  const int t = 3;

  const int horizontalW =
    w - 2 * t;

  const int upperVerticalH =
    (h / 2) - t - 1;

  const int lowerVerticalH =
    (h / 2) - t;

  uint8_t segments =
    digitSegments(digit);


  if (
    segments &
    SEG_A
  ) {

    display.drawRBox(
      x + t,
      y,
      horizontalW,
      t,
      1
    );
  }


  if (
    segments &
    SEG_G
  ) {

    display.drawRBox(
      x + t,
      y + h / 2 - 1,
      horizontalW,
      t,
      1
    );
  }


  if (
    segments &
    SEG_D
  ) {

    display.drawRBox(
      x + t,
      y + h - t,
      horizontalW,
      t,
      1
    );
  }


  if (
    segments &
    SEG_F
  ) {

    display.drawRBox(
      x,
      y + t,
      t,
      upperVerticalH,
      1
    );
  }


  if (
    segments &
    SEG_B
  ) {

    display.drawRBox(
      x + w - t,
      y + t,
      t,
      upperVerticalH,
      1
    );
  }


  if (
    segments &
    SEG_E
  ) {

    display.drawRBox(
      x,
      y + h / 2 + 2,
      t,
      lowerVerticalH,
      1
    );
  }


  if (
    segments &
    SEG_C
  ) {

    display.drawRBox(
      x + w - t,
      y + h / 2 + 2,
      t,
      lowerVerticalH,
      1
    );
  }
}


void drawLargeClock(bool colonOn) {

  // Optical layout:
  // - HH is centered on Emi's normal left eye
  // - MM is centered on Emi's normal right eye
  // - the whole clock is vertically centered on the normal eye line
  const int digitW = 14;
  const int digitH = 27;
  const int pairGap = 4;
  const int pairWidth =
    digitW * 2 +
    pairGap;

  const int y =
    EYE_CENTER_Y -
    digitH / 2;

  const int leftEyeCenter =
    LEFT_EYE_X +
    EYE_WIDTH / 2;

  const int rightEyeCenter =
    RIGHT_EYE_X +
    EYE_WIDTH / 2;

  const int leftPairX =
    leftEyeCenter -
    pairWidth / 2;

  const int rightPairX =
    rightEyeCenter -
    pairWidth / 2;

  const int d1x =
    leftPairX;

  const int d2x =
    leftPairX +
    digitW +
    pairGap;

  const int d3x =
    rightPairX;

  const int d4x =
    rightPairX +
    digitW +
    pairGap;

  drawSegmentDigit(
    d1x,
    y,
    shownHour / 10
  );

  drawSegmentDigit(
    d2x,
    y,
    shownHour % 10
  );

  drawSegmentDigit(
    d3x,
    y,
    shownMinute / 10
  );

  drawSegmentDigit(
    d4x,
    y,
    shownMinute % 10
  );


  if (colonOn) {

    // Keep the colon visually centered between both eye-aligned groups.
    display.drawRBox(
      63,
      EYE_CENTER_Y - 7,
      3,
      3,
      1
    );

    display.drawRBox(
      63,
      EYE_CENTER_Y + 5,
      3,
      3,
      1
    );
  }
}


// ------------------------------------------------------------
// CLOCK TRANSITION
// ------------------------------------------------------------

void drawClockTransition() {

  unsigned long now =
    millis();


  if (
    clockState ==
    CLOCK_EYES_CLOSING
  ) {

    float t =
      (float)(
        now -
        clockStageStart
      )
      /
      (float)
        CLOCK_EYE_CLOSE_MS;


    drawEyesWithClosure(
      smoothStep(t)
    );


    return;
  }


  if (
    clockState ==
    CLOCK_REVEALING
  ) {

    float t =
      smoothStep(
        (float)(
          now -
          clockStageStart
        )
        /
        (float)
          CLOCK_REVEAL_MS
      );


    int eyeBarWidth =
      (int)(
        EYE_WIDTH *
        (
          1.0f -
          t
        )
      );


    if (
      eyeBarWidth >
      0
    ) {

      int leftCenter =
        LEFT_EYE_X +
        EYE_WIDTH / 2;

      int rightCenter =
        RIGHT_EYE_X +
        EYE_WIDTH / 2;


      display.drawRBox(
        leftCenter -
          eyeBarWidth / 2,
        EYE_CENTER_Y - 1,
        eyeBarWidth,
        3,
        1
      );


      display.drawRBox(
        rightCenter -
          eyeBarWidth / 2,
        EYE_CENTER_Y - 1,
        eyeBarWidth,
        3,
        1
      );
    }


    int halfVisible =
      max(
        1,
        (int)(
          64.0f *
          t
        )
      );


    display.setClipWindow(
      64 - halfVisible,
      0,
      64 + halfVisible,
      64
    );


    drawLargeClock(
      true
    );


    display.setMaxClipWindow();


    return;
  }


  if (
    clockState ==
    CLOCK_HOLDING
  ) {

    // Keep the colon visible for the entire short clock display.
    // With only a ~2 second hold, blinking it made the time briefly read
    // like a four-digit number instead of HH:MM.
    drawLargeClock(
      true
    );


    return;
  }


  if (
    clockState ==
    CLOCK_HIDING
  ) {

    float t =
      smoothStep(
        (float)(
          now -
          clockStageStart
        )
        /
        (float)
          CLOCK_HIDE_MS
      );


    int halfVisible =
      (int)(
        64.0f *
        (
          1.0f -
          t
        )
      );


    if (
      halfVisible >
      0
    ) {

      display.setClipWindow(
        64 - halfVisible,
        0,
        64 + halfVisible,
        64
      );


      drawLargeClock(
        true
      );


      display.setMaxClipWindow();
    }


    // Do not draw flat horizontal eye bars here.
    // They looked like a full line for one frame as the face returned.
    return;
  }


  if (
    clockState ==
    CLOCK_EYES_OPENING
  ) {

    float t =
      (float)(
        now -
        clockStageStart
      )
      /
      (float)
        CLOCK_EYE_OPEN_MS;


    // Start from a rounded, partially-open eye shape instead of
    // a fully closed 3-pixel bar, which avoids the visible line flash.
    const float startClosure = 0.72f;

    drawEyesWithClosure(
      startClosure *
      (
        1.0f -
        smoothStep(t)
      )
    );
  }
}


void drawVoiceStatus() {

  if (!voiceMonitorReady) {
    return;
  }


  if (
    voiceCapturing ||
    voiceProcessing
  ) {

    display.drawDisc(
      123,
      4,
      2
    );
  }

  else {

    display.drawCircle(
      123,
      4,
      2
    );
  }
}


void drawEmi() {

  display.clearBuffer();


  if (
    clockState !=
    CLOCK_OFF
  ) {

    drawClockTransition();
  }

  else {

    drawEyesWithClosure(
      0.0f
    );
  }


  drawVoiceStatus();


  display.sendBuffer();
}


void startShowTime(
  int hour,
  int minute
) {

  shownHour =
    hour;

  shownMinute =
    minute;


  clockState =
    CLOCK_EYES_CLOSING;

  clockStageStart =
    millis();


  idleFollowup =
    FOLLOWUP_NONE;

  attentionState =
    ATTENTION_NONE;

  gazeMoving =
    false;

  blinking =
    false;

  doubleBlinkPending =
    false;

  petStrokeActive =
    false;

  petting =
    false;


  gazeX =
    0;

  gazeY =
    0;


  lastInteractionTime =
    millis();


  Serial.print(
    "Showing time: "
  );

  if (
    hour <
    10
  ) {

    Serial.print(
      '0'
    );
  }

  Serial.print(
    hour
  );

  Serial.print(
    ':'
  );

  if (
    minute <
    10
  ) {

    Serial.print(
      '0'
    );
  }

  Serial.println(
    minute
  );
}


void cancelClockForInteraction() {

  if (
    clockState ==
    CLOCK_OFF
  ) {

    return;
  }


  clockState =
    CLOCK_OFF;


  scheduleNextBlink();

  scheduleNextAttentionBid();

  nextIdleAction =
    millis() +
    1200;
}


void updateClock() {

  if (
    clockState ==
    CLOCK_OFF
  ) {

    return;
  }


  unsigned long now =
    millis();

  unsigned long elapsed =
    now -
    clockStageStart;


  if (
    clockState ==
      CLOCK_EYES_CLOSING &&
    elapsed >=
      CLOCK_EYE_CLOSE_MS
  ) {

    clockState =
      CLOCK_REVEALING;

    clockStageStart =
      now;

    return;
  }


  if (
    clockState ==
      CLOCK_REVEALING &&
    elapsed >=
      CLOCK_REVEAL_MS
  ) {

    clockState =
      CLOCK_HOLDING;

    clockStageStart =
      now;

    return;
  }


  if (
    clockState ==
      CLOCK_HOLDING &&
    elapsed >=
      CLOCK_HOLD_MS
  ) {

    clockState =
      CLOCK_HIDING;

    clockStageStart =
      now;

    return;
  }


  if (
    clockState ==
      CLOCK_HIDING &&
    elapsed >=
      CLOCK_HIDE_MS
  ) {

    clockState =
      CLOCK_EYES_OPENING;

    clockStageStart =
      now;

    return;
  }


  if (
    clockState ==
      CLOCK_EYES_OPENING &&
    elapsed >=
      CLOCK_EYE_OPEN_MS
  ) {

    clockState =
      CLOCK_OFF;


    nextIdleAction =
      now +
      1500;


    scheduleNextBlink();

    scheduleNextAttentionBid();
  }
}


// ------------------------------------------------------------
// SHOW_TIME COMMAND PARSING
// ------------------------------------------------------------

bool parseTimeText(
  const String &timeText,
  int &hour,
  int &minute
) {

  if (
    timeText.length() !=
    5
  ) {

    return false;
  }


  if (
    timeText.charAt(2) !=
    ':'
  ) {

    return false;
  }


  if (
    !isDigit(
      timeText.charAt(0)
    ) ||
    !isDigit(
      timeText.charAt(1)
    ) ||
    !isDigit(
      timeText.charAt(3)
    ) ||
    !isDigit(
      timeText.charAt(4)
    )
  ) {

    return false;
  }


  hour =
    (
      timeText.charAt(0) -
      '0'
    )
    *
    10
    +
    (
      timeText.charAt(1) -
      '0'
    );


  minute =
    (
      timeText.charAt(3) -
      '0'
    )
    *
    10
    +
    (
      timeText.charAt(4) -
      '0'
    );


  if (
    hour < 0 ||
    hour > 23 ||
    minute < 0 ||
    minute > 59
  ) {

    return false;
  }


  return true;
}


void handleCommand(
  String command
) {

  command.trim();


  if (
    command.length() ==
    0
  ) {

    return;
  }


  const String prefix =
    "SHOW_TIME ";


  if (
    command.startsWith(
      prefix
    )
  ) {

    String timeText =
      command.substring(
        prefix.length()
      );


    timeText.trim();


    int hour = 0;
    int minute = 0;


    if (
      parseTimeText(
        timeText,
        hour,
        minute
      )
    ) {

      startShowTime(
        hour,
        minute
      );
    }

    else {

      Serial.println(
        "ERR expected SHOW_TIME HH:MM"
      );
    }


    return;
  }


  if (
    command ==
    "FACE"
  ) {

    cancelClockForInteraction();

    Serial.println(
      "OK face"
    );

    return;
  }


  Serial.println(
    "ERR unknown command"
  );
}


void updateSerialCommands() {

  while (
    Serial.available() >
    0
  ) {

    char c =
      (char)
        Serial.read();


    if (
      c ==
      '\r'
    ) {

      continue;
    }


    if (
      c ==
      '\n'
    ) {

      handleCommand(
        serialCommand
      );


      serialCommand =
        "";


      continue;
    }


    if (
      serialCommand.length() <
      64
    ) {

      serialCommand +=
        c;
    }
  }
}


// ------------------------------------------------------------
// GAZE
// ------------------------------------------------------------

void startGaze(
  float targetX,
  float targetY,
  unsigned long duration
) {

  gazeStartX =
    gazeX;

  gazeStartY =
    gazeY;


  gazeTargetX =
    targetX;

  gazeTargetY =
    targetY;


  gazeStartTime =
    millis();


  gazeDuration =
    duration;


  gazeMoving =
    true;
}


void updateGaze() {

  if (
    !gazeMoving ||
    clockState !=
      CLOCK_OFF
  ) {

    return;
  }


  unsigned long elapsed =
    millis() -
    gazeStartTime;


  if (
    elapsed >=
    gazeDuration
  ) {

    gazeX =
      gazeTargetX;

    gazeY =
      gazeTargetY;

    gazeMoving =
      false;

    return;
  }


  float t =
    (float)elapsed /
    (float)
      gazeDuration;


  float eased =
    smoothStep(t);


  gazeX =
    gazeStartX +
    (
      gazeTargetX -
      gazeStartX
    )
    *
    eased;


  gazeY =
    gazeStartY +
    (
      gazeTargetY -
      gazeStartY
    )
    *
    eased;
}


// ------------------------------------------------------------
// BLINKS
// ------------------------------------------------------------

void startBlink() {

  if (
    blinking ||
    clockState !=
      CLOCK_OFF
  ) {

    return;
  }


  blinking =
    true;


  blinkCloseTime =
    65;

  blinkHoldTime =
    18;

  blinkOpenTime =
    95;


  blinkStartTime =
    millis();


  if (
    random(100) <
    2
  ) {

    doubleBlinkPending =
      true;

    secondBlinkTime =
      millis() +
      350;
  }
}


void startAttentionBlink() {

  if (
    blinking ||
    clockState !=
      CLOCK_OFF
  ) {

    return;
  }


  blinking =
    true;


  blinkCloseTime =
    80;

  blinkHoldTime =
    28;

  blinkOpenTime =
    115;


  blinkStartTime =
    millis();
}


void startRelaxedBlink() {

  if (
    blinking ||
    clockState !=
      CLOCK_OFF
  ) {

    return;
  }


  blinking =
    true;


  blinkCloseTime =
    120;

  blinkHoldTime =
    70;

  blinkOpenTime =
    200;


  blinkStartTime =
    millis();
}


void scheduleNextBlink() {

  nextBlinkTime =
    millis()
    + random(
        9000,
        18000
      );
}


// ------------------------------------------------------------
// ATTENTION BIDS
// ------------------------------------------------------------

void scheduleNextAttentionBid() {

  nextAttentionBid =
    millis()
    + random(
        90000,
        180000
      );
}


void startAttentionBid() {

  attentionState =
    ATTENTION_LOOK;


  startGaze(
    random(-2, 3),
    random(-5, -2),
    170
  );


  attentionStageTime =
    millis();
}


void updateAttentionBid() {

  if (
    petting ||
    clockState !=
      CLOCK_OFF
  ) {

    return;
  }


  unsigned long now =
    millis();


  if (
    attentionState ==
    ATTENTION_NONE
  ) {

    if (
      now >=
      nextAttentionBid &&

      now -
      lastInteractionTime >
        60000
    ) {

      startAttentionBid();
    }


    return;
  }


  if (
    attentionState ==
    ATTENTION_LOOK
  ) {

    if (!gazeMoving) {

      startAttentionBlink();


      attentionState =
        ATTENTION_FIRST_BLINK;


      attentionStageTime =
        now;
    }


    return;
  }


  if (
    attentionState ==
    ATTENTION_FIRST_BLINK
  ) {

    if (
      !blinking &&

      now -
      attentionStageTime >
        250
    ) {

      attentionState =
        ATTENTION_PAUSE;


      attentionStageTime =
        now;
    }


    return;
  }


  if (
    attentionState ==
    ATTENTION_PAUSE
  ) {

    if (
      now -
      attentionStageTime >
        520
    ) {

      startAttentionBlink();


      attentionState =
        ATTENTION_SECOND_BLINK;


      attentionStageTime =
        now;
    }


    return;
  }


  if (
    attentionState ==
    ATTENTION_SECOND_BLINK
  ) {

    if (
      !blinking &&

      now -
      attentionStageTime >
        250
    ) {

      startGaze(
        random(-7, 8),
        random(-2, 4),
        260
      );


      attentionState =
        ATTENTION_SETTLE;


      attentionStageTime =
        now;
    }


    return;
  }


  if (
    attentionState ==
    ATTENTION_SETTLE
  ) {

    if (!gazeMoving) {

      attentionState =
        ATTENTION_NONE;


      scheduleNextAttentionBid();


      nextIdleAction =
        now +
        random(
          2200,
          4200
        );
    }
  }
}


// ------------------------------------------------------------
// IDLE ACTION SELECTION
// ------------------------------------------------------------

IdleAction pickIdleAction() {

  int weights[
    IDLE_ACTION_COUNT
  ] = {

    22,
    17,
    18,
    10,
    8,
    12,
    13
  };


  weights[
    IDLE_CURIOUS_PEEK
  ] +=
    (int)(
      curiosity *
      18.0f
    );


  weights[
    IDLE_WIDE_GLANCE
  ] +=
    (int)(
      curiosity *
      14.0f
    );


  weights[
    IDLE_FOCUS
  ] +=
    (int)(
      curiosity *
      8.0f
    );


  weights[
    IDLE_STILL
  ] +=
    (int)(
      contentment *
      18.0f
    );


  weights[
    IDLE_SOFT_ATTENTION
  ] +=
    (int)(
      contentment *
      13.0f
    );


  weights[
    IDLE_SETTLE
  ] +=
    (int)(
      contentment *
      8.0f
    );


  weights[
    lastIdleAction
  ] =
    max(
      1,

      weights[
        lastIdleAction
      ]
      /
      5
    );


  weights[
    previousIdleAction
  ] =
    max(
      1,

      weights[
        previousIdleAction
      ]
      /
      2
    );


  int totalWeight =
    0;


  for (
    int i = 0;
    i <
    IDLE_ACTION_COUNT;
    i++
  ) {

    totalWeight +=
      weights[i];
  }


  int roll =
    random(
      totalWeight
    );


  for (
    int i = 0;
    i <
    IDLE_ACTION_COUNT;
    i++
  ) {

    if (
      roll <
      weights[i]
    ) {

      return
        (IdleAction)i;
    }


    roll -=
      weights[i];
  }


  return
    IDLE_STILL;
}


// ------------------------------------------------------------
// RUN IDLE ACTION
// ------------------------------------------------------------

void chooseIdleAction() {

  unsigned long now =
    millis();


  idleFollowup =
    FOLLOWUP_NONE;


  IdleAction action =
    pickIdleAction();


  previousIdleAction =
    lastIdleAction;


  lastIdleAction =
    action;


  if (
    action ==
    IDLE_STILL
  ) {

    nextIdleAction =
      now
      + random(
          3800,
          8200
        );


    return;
  }


  if (
    action ==
    IDLE_MICRO_GLANCE
  ) {

    startGaze(
      clampFloat(
        gazeX +
        random(-3, 4),
        -18,
        18
      ),

      clampFloat(
        gazeY +
        random(-2, 3),
        -13,
        10
      ),

      random(
        70,
        115
      )
    );


    curiosity -=
      0.035f;


    nextIdleAction =
      now
      + random(
          2300,
          4800
        );


    return;
  }


  if (
    action ==
    IDLE_FOCUS
  ) {

    unsigned long moveTime =
      random(
        90,
        150
      );


    startGaze(
      random(-13, 14),
      random(-8, 8),
      moveTime
    );


    idleFollowup =
      FOLLOWUP_TINY_CORRECTION;


    idleFollowupTime =
      now +
      moveTime +
      random(
        450,
        1200
      );


    curiosity -=
      0.08f;


    nextIdleAction =
      now
      + random(
          3800,
          6900
        );


    return;
  }


  if (
    action ==
    IDLE_CURIOUS_PEEK
  ) {

    float targetX =
      random(2) ==
      0

      ? random(
          -18,
          -13
        )

      : random(
          13,
          19
        );


    unsigned long moveTime =
      random(
        80,
        130
      );


    startGaze(
      targetX,
      random(-9, -3),
      moveTime
    );


    idleFollowup =
      FOLLOWUP_PEEK_SETTLE;


    idleFollowupTime =
      now +
      moveTime +
      random(
        1100,
        2200
      );


    curiosity -=
      0.16f;


    nextIdleAction =
      now
      + random(
          4800,
          7600
        );


    return;
  }


  if (
    action ==
    IDLE_WIDE_GLANCE
  ) {

    float targetX =
      random(2) ==
      0

      ? random(
          -18,
          -11
        )

      : random(
          11,
          19
        );


    unsigned long moveTime =
      random(
        90,
        155
      );


    startGaze(
      targetX,
      random(-11, 10),
      moveTime
    );


    idleFollowup =
      FOLLOWUP_RETURN_NEAR_CENTER;


    idleFollowupTime =
      now +
      moveTime +
      random(
        1000,
        2100
      );


    curiosity -=
      0.20f;


    nextIdleAction =
      now
      + random(
          5000,
          8200
        );


    return;
  }


  if (
    action ==
    IDLE_SETTLE
  ) {

    startGaze(
      random(-4, 5),
      random(-3, 4),
      random(
        220,
        380
      )
    );


    nextIdleAction =
      now
      + random(
          4300,
          7600
        );


    return;
  }


  startGaze(
    random(-3, 4),
    random(-6, -2),
    random(
      170,
      280
    )
  );


  nextIdleAction =
    now
    + random(
        4200,
        7600
      );
}


// ------------------------------------------------------------
// IDLE FOLLOWUPS
// ------------------------------------------------------------

void updateIdleFollowup() {

  if (
    idleFollowup ==
      FOLLOWUP_NONE ||

    gazeMoving
  ) {

    return;
  }


  unsigned long now =
    millis();


  if (
    now <
    idleFollowupTime
  ) {

    return;
  }


  if (
    idleFollowup ==
    FOLLOWUP_TINY_CORRECTION
  ) {

    startGaze(
      clampFloat(
        gazeX +
        random(-2, 3),
        -18,
        18
      ),

      clampFloat(
        gazeY +
        random(-1, 2),
        -13,
        10
      ),

      random(
        65,
        105
      )
    );
  }


  else if (
    idleFollowup ==
    FOLLOWUP_RETURN_NEAR_CENTER
  ) {

    startGaze(
      random(-5, 6),
      random(-3, 4),
      random(
        150,
        240
      )
    );
  }


  else if (
    idleFollowup ==
    FOLLOWUP_PEEK_SETTLE
  ) {

    float targetX =
      gazeX <
      0

      ? random(
          -10,
          -5
        )

      : random(
          5,
          11
        );


    startGaze(
      targetX,
      random(-5, 2),
      random(
        180,
        300
      )
    );
  }


  idleFollowup =
    FOLLOWUP_NONE;
}


// ------------------------------------------------------------
// PETTING
// ------------------------------------------------------------

void beginPetting() {

  cancelClockForInteraction();


  petting =
    true;


  patCount =
    0;


  relaxedBlinkPending =
    false;


  relaxedBlinkDone =
    false;


  idleFollowup =
    FOLLOWUP_NONE;


  attentionState =
    ATTENTION_NONE;


  lastInteractionTime =
    millis();


  scheduleNextAttentionBid();


  startGaze(
    0,
    -15,
    85
  );


  nextPetDrift =
    millis() +
    1100;
}


void reactToPat() {

  unsigned long now =
    millis();


  if (
    lastRecognizedPat !=
      0 &&

    now -
    lastRecognizedPat <
      MIN_PAT_INTERVAL
  ) {

    return;
  }


  lastRecognizedPat =
    now;


  lastTouchTime =
    now;


  lastInteractionTime =
    now;


  patCount++;


  contentment =
    clampFloat(
      contentment +
      0.11f,
      0.0f,
      1.0f
    );


  curiosity =
    clampFloat(
      curiosity -
      0.04f,
      0.0f,
      1.0f
    );


  petStrokeMaxClosure =
    0.22f +

    min(
      patCount,
      5
    )
    *
    0.012f +

    contentment *
    0.018f;


  petStrokeMaxClosure =
    clampFloat(
      petStrokeMaxClosure,
      0.24f,
      0.32f
    );


  petStrokeActive =
    true;


  petStrokeStart =
    now;


  if (
    patCount >=
      4 &&

    !relaxedBlinkDone
  ) {

    relaxedBlinkPending =
      true;
  }
}


void updatePettingMotion() {

  if (!petting) {
    return;
  }


  unsigned long now =
    millis();


  if (
    !gazeMoving &&

    !petStrokeActive &&

    now >=
      nextPetDrift
  ) {

    startGaze(
      random(-2, 3),
      -15,
      random(
        500,
        850
      )
    );


    nextPetDrift =
      now
      + random(
          1700,
          3100
        );
  }
}


void endPetting() {

  petting =
    false;


  patCount =
    0;


  relaxedBlinkPending =
    false;


  relaxedBlinkDone =
    false;


  petStrokeActive =
    false;


  float returnY =
    -2.0f -
    contentment *
    2.0f;


  startGaze(
    random(-3, 4),
    returnY,
    460
  );


  scheduleNextBlink();

  scheduleNextAttentionBid();


  nextIdleAction =
    millis()
    + random(
        2600,
        5000
      );
}


// ------------------------------------------------------------
// TOUCH
// ------------------------------------------------------------

void updateTouch() {

  unsigned long now =
    millis();


  rawTouch =
    digitalRead(
      TOUCH_PIN
    );


  if (
    rawTouch !=
    previousRawTouch
  ) {

    previousRawTouch =
      rawTouch;


    touchChangedAt =
      now;
  }


  if (
    now -
    touchChangedAt >
      TOUCH_DEBOUNCE
  ) {

    if (
      stableTouch !=
      rawTouch
    ) {

      stableTouch =
        rawTouch;


      if (
        stableTouch
      ) {

        if (
          !petting
        ) {

          beginPetting();
        }


        reactToPat();
      }
    }
  }


  if (
    stableTouch
  ) {

    lastTouchTime =
      now;
  }


  if (
    petting &&

    !stableTouch &&

    now -
    lastTouchTime >
      PET_GRACE_TIME
  ) {

    endPetting();
  }
}


// ------------------------------------------------------------
// IDLE UPDATE
// ------------------------------------------------------------

void updateIdleBehaviour() {

  if (
    petting ||

    clockState !=
      CLOCK_OFF ||

    attentionState !=
      ATTENTION_NONE
  ) {

    return;
  }


  unsigned long now =
    millis();


  updateIdleFollowup();


  if (
    !gazeMoving &&

    idleFollowup ==
      FOLLOWUP_NONE &&

    now >=
      nextIdleAction
  ) {

    chooseIdleAction();
  }


  if (
    !blinking &&

    now >=
      nextBlinkTime
  ) {

    startBlink();


    scheduleNextBlink();
  }


  if (
    doubleBlinkPending &&

    !blinking &&

    now >=
      secondBlinkTime
  ) {

    doubleBlinkPending =
      false;


    startBlink();
  }
}


// ------------------------------------------------------------
// PETTING UPDATE
// ------------------------------------------------------------

void updatePettingBehaviour() {

  if (!petting) {
    return;
  }


  unsigned long now =
    millis();


  updatePettingMotion();


  if (
    relaxedBlinkPending &&

    !relaxedBlinkDone &&

    !blinking &&

    !petStrokeActive &&

    now -
    lastRecognizedPat >
      500
  ) {

    startRelaxedBlink();


    relaxedBlinkPending =
      false;


    relaxedBlinkDone =
      true;
  }
}



// ------------------------------------------------------------
// LIVE VOICE CAPTURE
// ------------------------------------------------------------

bool setupVoiceMicrophone() {

  i2s_config_t config = {};

  config.mode =
    (i2s_mode_t)(
      I2S_MODE_MASTER |
      I2S_MODE_RX
    );

  config.sample_rate =
    VOICE_SAMPLE_RATE;

  config.bits_per_sample =
    I2S_BITS_PER_SAMPLE_32BIT;

  config.channel_format =
    I2S_CHANNEL_FMT_ONLY_LEFT;

  config.communication_format =
    I2S_COMM_FORMAT_STAND_I2S;

  config.intr_alloc_flags =
    ESP_INTR_FLAG_LEVEL1;

  config.dma_buf_count =
    8;

  config.dma_buf_len =
    128;

  config.use_apll =
    false;

  config.tx_desc_auto_clear =
    false;

  config.fixed_mclk =
    0;


  i2s_pin_config_t pins = {};

  pins.bck_io_num =
    MIC_SCK;

  pins.ws_io_num =
    MIC_WS;

  pins.data_out_num =
    I2S_PIN_NO_CHANGE;

  pins.data_in_num =
    MIC_SD;


  esp_err_t result =
    i2s_driver_install(
      I2S_PORT,
      &config,
      0,
      nullptr
    );


  if (
    result !=
    ESP_OK
  ) {

    Serial.print(
      "Voice I2S install failed: "
    );

    Serial.println(
      (int)result
    );

    return false;
  }


  result =
    i2s_set_pin(
      I2S_PORT,
      &pins
    );


  if (
    result !=
    ESP_OK
  ) {

    Serial.print(
      "Voice I2S pin setup failed: "
    );

    Serial.println(
      (int)result
    );

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


void writeWavHeader(
  uint8_t *buffer,
  uint32_t pcmBytes
) {

  uint32_t riffSize =
    36 +
    pcmBytes;

  uint32_t byteRate =
    VOICE_SAMPLE_RATE *
    2;


  memcpy(
    buffer + 0,
    "RIFF",
    4
  );

  buffer[4] =
    (uint8_t)(
      riffSize
    );

  buffer[5] =
    (uint8_t)(
      riffSize >>
      8
    );

  buffer[6] =
    (uint8_t)(
      riffSize >>
      16
    );

  buffer[7] =
    (uint8_t)(
      riffSize >>
      24
    );


  memcpy(
    buffer + 8,
    "WAVEfmt ",
    8
  );


  buffer[16] = 16;
  buffer[17] = 0;
  buffer[18] = 0;
  buffer[19] = 0;

  buffer[20] = 1;
  buffer[21] = 0;

  buffer[22] = 1;
  buffer[23] = 0;


  buffer[24] =
    (uint8_t)(
      VOICE_SAMPLE_RATE
    );

  buffer[25] =
    (uint8_t)(
      VOICE_SAMPLE_RATE >>
      8
    );

  buffer[26] =
    (uint8_t)(
      VOICE_SAMPLE_RATE >>
      16
    );

  buffer[27] =
    (uint8_t)(
      VOICE_SAMPLE_RATE >>
      24
    );


  buffer[28] =
    (uint8_t)(
      byteRate
    );

  buffer[29] =
    (uint8_t)(
      byteRate >>
      8
    );

  buffer[30] =
    (uint8_t)(
      byteRate >>
      16
    );

  buffer[31] =
    (uint8_t)(
      byteRate >>
      24
    );


  buffer[32] = 2;
  buffer[33] = 0;

  buffer[34] = 16;
  buffer[35] = 0;


  memcpy(
    buffer + 36,
    "data",
    4
  );


  buffer[40] =
    (uint8_t)(
      pcmBytes
    );

  buffer[41] =
    (uint8_t)(
      pcmBytes >>
      8
    );

  buffer[42] =
    (uint8_t)(
      pcmBytes >>
      16
    );

  buffer[43] =
    (uint8_t)(
      pcmBytes >>
      24
    );
}


int16_t voiceSampleToPcm(
  int32_t centeredSample
) {

  int64_t amplified =
    (int64_t)
      centeredSample *
    VOICE_PCM_GAIN;


  if (
    amplified >
    32767
  ) {

    amplified =
      32767;
  }


  if (
    amplified <
    -32768
  ) {

    amplified =
      -32768;
  }


  return
    (int16_t)
      amplified;
}


bool uploadVoiceWav(
  int sampleCount
) {

  if (
    sampleCount <
    VOICE_MIN_SAMPLES
  ) {

    return false;
  }


  if (
    WiFi.status() !=
    WL_CONNECTED
  ) {

    Serial.println(
      "Voice discarded: Wi-Fi not connected."
    );

    return false;
  }


  uint32_t pcmBytes =
    (uint32_t)
      sampleCount *
    sizeof(int16_t);


  writeWavHeader(
    voiceWavBuffer,
    pcmBytes
  );


  if (
    httpMutex !=
      nullptr
  ) {

    if (
      xSemaphoreTake(
        httpMutex,
        pdMS_TO_TICKS(
          1500
        )
      )
      !=
      pdTRUE
    ) {

      Serial.println(
        "Voice upload skipped: HTTP busy."
      );

      return false;
    }
  }


  voiceProcessing =
    true;


  WiFiClient client;
  HTTPClient http;


  String url =
    "http://" +
    String(EMI_HUB_HOST) +
    ":" +
    String(EMI_HUB_PORT) +
    "/device/audio";


  http.setConnectTimeout(
    1000
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

    voiceProcessing =
      false;


    if (
      httpMutex !=
        nullptr
    ) {

      xSemaphoreGive(
        httpMutex
      );
    }


    Serial.println(
      "Voice upload failed: HTTP begin."
    );

    return false;
  }


  http.addHeader(
    "X-EMI-Token",
    EMI_SHARED_TOKEN
  );

  http.addHeader(
    "Content-Type",
    "audio/wav"
  );


  int status =
    http.POST(
      voiceWavBuffer,
      44 +
      pcmBytes
    );


  Serial.print(
    "Voice upload HTTP status: "
  );

  Serial.println(
    status
  );


  http.end();


  voiceProcessing =
    false;


  if (
    httpMutex !=
      nullptr
  ) {

    xSemaphoreGive(
      httpMutex
    );
  }


  return
    status ==
    200;
}


void voiceTask(
  void *parameter
) {

  int preRollWrite =
    0;

  int preRollCount =
    0;

  int calibrationBlocks =
    0;

  int loudBlocks =
    0;

  int silentBlocks =
    0;

  int recordedSamples =
    0;

  float noiseFloor =
    0.0f;

  unsigned long lastVoiceDebug =
    0;


  int16_t *recordedPcm =
    reinterpret_cast<int16_t *>(
      voiceWavBuffer +
      44
    );


  voiceMonitorReady =
    true;


  Serial.println(
    "Voice monitor: calibrating room noise..."
  );


  for (;;) {

    size_t bytesRead =
      0;


    esp_err_t result =
      i2s_read(
        I2S_PORT,
        voiceI2SBlock,
        sizeof(
          voiceI2SBlock
        ),
        &bytesRead,
        portMAX_DELAY
      );


    if (
      result !=
        ESP_OK ||
      bytesRead ==
        0
    ) {

      vTaskDelay(
        pdMS_TO_TICKS(
          10
        )
      );

      continue;
    }


    int count =
      bytesRead /
      sizeof(int32_t);


    int64_t sum =
      0;


    for (
      int i = 0;
      i < count;
      i++
    ) {

      sum +=
        voiceI2SBlock[i] >>
        8;
    }


    int32_t mean =
      (int32_t)(
        sum /
        count
      );


    uint64_t magnitudeSum =
      0;


    int16_t converted[
      VOICE_I2S_BLOCK_SAMPLES
    ];


    for (
      int i = 0;
      i < count;
      i++
    ) {

      int32_t centered =
        (
          voiceI2SBlock[i] >>
          8
        )
        -
        mean;


      int32_t magnitude =
        centered >=
          0
        ? centered
        : -centered;


      magnitudeSum +=
        (uint32_t)
          magnitude;


      converted[i] =
        voiceSampleToPcm(
          centered
        );
    }


    float level =
      (float)(
        magnitudeSum /
        count
      );


    for (
      int i = 0;
      i < count;
      i++
    ) {

      voicePreRoll[
        preRollWrite
      ] =
        converted[i];


      preRollWrite =
        (
          preRollWrite +
          1
        )
        %
        VOICE_PRE_ROLL_SAMPLES;


      if (
        preRollCount <
        VOICE_PRE_ROLL_SAMPLES
      ) {

        preRollCount++;
      }
    }


    if (
      calibrationBlocks <
      VOICE_CALIBRATION_BLOCKS
    ) {

      if (
        calibrationBlocks ==
        0
      ) {

        noiseFloor =
          level;
      }

      else {

        noiseFloor =
          noiseFloor *
          0.90f +
          level *
          0.10f;
      }


      calibrationBlocks++;


      if (
        calibrationBlocks ==
        VOICE_CALIBRATION_BLOCKS
      ) {

        Serial.print(
          "Voice monitor ready. Noise floor: "
        );

        Serial.println(
          noiseFloor
        );
      }


      continue;
    }


    float threshold =
      max(
        VOICE_MIN_THRESHOLD,
        noiseFloor *
          VOICE_THRESHOLD_MULTIPLIER +
          VOICE_THRESHOLD_OFFSET
      );


    bool loud =
      level >
      threshold;


    if (
      !voiceCapturing &&
      millis() -
        lastVoiceDebug >=
        1000
    ) {

      lastVoiceDebug =
        millis();

      Serial.print(
        "Voice level="
      );

      Serial.print(
        level
      );

      Serial.print(
        " noise="
      );

      Serial.print(
        noiseFloor
      );

      Serial.print(
        " threshold="
      );

      Serial.println(
        threshold
      );
    }


    if (!voiceCapturing) {

      if (!loud) {

        noiseFloor =
          noiseFloor *
          0.995f +
          level *
          0.005f;

        loudBlocks =
          0;
      }

      else {

        loudBlocks++;
      }


      if (
        loudBlocks >=
        VOICE_START_BLOCKS
      ) {

        voiceCapturing =
          true;

        silentBlocks =
          0;

        loudBlocks =
          0;

        recordedSamples =
          0;


        int start =
          (
            preRollWrite -
            preRollCount +
            VOICE_PRE_ROLL_SAMPLES
          )
          %
          VOICE_PRE_ROLL_SAMPLES;


        for (
          int i = 0;
          i <
            preRollCount &&
          recordedSamples <
            VOICE_MAX_SAMPLES;
          i++
        ) {

          int index =
            (
              start +
              i
            )
            %
            VOICE_PRE_ROLL_SAMPLES;


          recordedPcm[
            recordedSamples++
          ] =
            voicePreRoll[
              index
            ];
        }


        Serial.println(
          "Voice: speech detected."
        );
      }


      continue;
    }


    for (
      int i = 0;
      i < count &&
      recordedSamples <
        VOICE_MAX_SAMPLES;
      i++
    ) {

      recordedPcm[
        recordedSamples++
      ] =
        converted[i];
    }


    if (loud) {

      silentBlocks =
        0;
    }

    else {

      silentBlocks++;
    }


    bool reachedSilence =
      silentBlocks >=
      VOICE_END_SILENT_BLOCKS;


    bool reachedMaximum =
      recordedSamples >=
      VOICE_MAX_SAMPLES;


    if (
      reachedSilence ||
      reachedMaximum
    ) {

      voiceCapturing =
        false;


      Serial.print(
        "Voice: captured "
      );

      Serial.print(
        recordedSamples
      );

      Serial.println(
        " samples; sending to Pi."
      );


      uploadVoiceWav(
        recordedSamples
      );


      recordedSamples =
        0;

      silentBlocks =
        0;

      loudBlocks =
        0;


      i2s_zero_dma_buffer(
        I2S_PORT
      );


      vTaskDelay(
        pdMS_TO_TICKS(
          400
        )
      );
    }
  }
}


// ------------------------------------------------------------
// WIFI / HUB COMMAND TRANSPORT
// ------------------------------------------------------------

const char* wifiReasonName(int reason) {
  switch (reason) {
    case 2:   return "AUTH_EXPIRE";
    case 3:   return "AUTH_LEAVE";
    case 4:   return "ASSOC_EXPIRE";
    case 5:   return "ASSOC_TOOMANY";
    case 6:   return "NOT_AUTHED";
    case 7:   return "NOT_ASSOCED";
    case 8:   return "ASSOC_LEAVE";
    case 15:  return "4WAY_HANDSHAKE_TIMEOUT";
    case 200: return "BEACON_TIMEOUT";
    case 201: return "NO_AP_FOUND";
    case 202: return "AUTH_FAIL";
    case 203: return "ASSOC_FAIL";
    case 204: return "HANDSHAKE_TIMEOUT";
    default:  return "OTHER";
  }
}


void onWiFiEvent(
  WiFiEvent_t event,
  WiFiEventInfo_t info
) {

  if (
    event ==
    ARDUINO_EVENT_WIFI_STA_CONNECTED
  ) {

    Serial.println(
      "Wi-Fi event: associated with access point."
    );

    return;
  }


  if (
    event ==
    ARDUINO_EVENT_WIFI_STA_GOT_IP
  ) {

    Serial.print(
      "Wi-Fi event: got IP "
    );

    Serial.println(
      WiFi.localIP()
    );

    return;
  }


  if (
    event ==
    ARDUINO_EVENT_WIFI_STA_DISCONNECTED
  ) {

    lastWiFiDisconnectReason =
      info.wifi_sta_disconnected.reason;


    Serial.print(
      "Wi-Fi event: disconnected. reason="
    );

    Serial.print(
      lastWiFiDisconnectReason
    );

    Serial.print(
      " ("
    );

    Serial.print(
      wifiReasonName(
        lastWiFiDisconnectReason
      )
    );

    Serial.println(
      ")"
    );
  }
}


bool findTargetAccessPoint(
  int32_t &channel,
  uint8_t bssid[6],
  int32_t &rssi
) {

  Serial.println(
    "Wi-Fi scan: looking for configured SSID..."
  );


  int count =
    WiFi.scanNetworks(
      false,
      true
    );


  if (
    count < 0
  ) {

    Serial.print(
      "Wi-Fi scan failed. code="
    );

    Serial.println(
      count
    );

    return false;
  }


  int bestIndex =
    -1;

  int32_t bestRssi =
    -1000;


  for (
    int i = 0;
    i < count;
    i++
  ) {

    if (
      WiFi.SSID(i) ==
      EMI_WIFI_SSID
    ) {

      if (
        WiFi.RSSI(i) >
        bestRssi
      ) {

        bestIndex =
          i;

        bestRssi =
          WiFi.RSSI(i);
      }
    }
  }


  if (
    bestIndex < 0
  ) {

    WiFi.scanDelete();

    Serial.println(
      "Wi-Fi scan: configured SSID not found."
    );

    return false;
  }


  channel =
    WiFi.channel(
      bestIndex
    );

  rssi =
    WiFi.RSSI(
      bestIndex
    );


  const uint8_t *foundBssid =
    WiFi.BSSID(
      bestIndex
    );


  memcpy(
    bssid,
    foundBssid,
    6
  );


  Serial.print(
    "Wi-Fi target found. RSSI="
  );

  Serial.print(
    rssi
  );

  Serial.print(
    " dBm channel="
  );

  Serial.print(
    channel
  );

  Serial.print(
    " BSSID="
  );


  for (
    int i = 0;
    i < 6;
    i++
  ) {

    if (
      i > 0
    ) {

      Serial.print(
        ':'
      );
    }


    if (
      bssid[i] <
      16
    ) {

      Serial.print(
        '0'
      );
    }


    Serial.print(
      bssid[i],
      HEX
    );
  }


  Serial.println();


  WiFi.scanDelete();


  return true;
}


void queueNetworkCommand(const String &command) {

  if (
    networkCommandQueue ==
    nullptr
  ) {

    return;
  }


  NetworkCommand item = {};


  strlcpy(
    item.text,
    command.c_str(),
    sizeof(item.text)
  );


  xQueueSend(
    networkCommandQueue,
    &item,
    0
  );
}


void pollHubOnce() {

  if (
    WiFi.status() !=
    WL_CONNECTED
  ) {

    return;
  }


  if (
    httpMutex !=
      nullptr
  ) {

    if (
      xSemaphoreTake(
        httpMutex,
        pdMS_TO_TICKS(
          500
        )
      )
      !=
      pdTRUE
    ) {

      return;
    }
  }


  WiFiClient client;
  HTTPClient http;


  String url =
    "http://" +
    String(EMI_HUB_HOST) +
    ":" +
    String(EMI_HUB_PORT) +
    "/device/command";


  http.setConnectTimeout(
    250
  );


  http.setTimeout(
    300
  );


  if (
    !http.begin(
      client,
      url
    )
  ) {

    if (
      httpMutex !=
        nullptr
    ) {

      xSemaphoreGive(
        httpMutex
      );
    }

    return;
  }


  http.addHeader(
    "X-EMI-Token",
    EMI_SHARED_TOKEN
  );


  int status =
    http.GET();


  if (
    status ==
    200
  ) {

    String command =
      http.getString();


    command.trim();


    if (
      command.length() >
      0
    ) {

      queueNetworkCommand(
        command
      );
    }
  }


  http.end();


  if (
    httpMutex !=
      nullptr
  ) {

    xSemaphoreGive(
      httpMutex
    );
  }
}


void networkTask(
  void *parameter
) {

  bool announcedConnection =
    false;

  bool connecting =
    false;

  unsigned long attemptStarted =
    0;

  unsigned long retryAfter =
    0;


  for (;;) {

    unsigned long now =
      millis();

    wl_status_t status =
      WiFi.status();


    if (
      status ==
      WL_CONNECTED
    ) {

      connecting =
        false;


      if (
        !announcedConnection
      ) {

        announcedConnection =
          true;


        Serial.print(
          "Wi-Fi connected. IP: "
        );


        Serial.println(
          WiFi.localIP()
        );
      }


      pollHubOnce();


      vTaskDelay(
        pdMS_TO_TICKS(
          HUB_POLL_MS
        )
      );


      continue;
    }


    announcedConnection =
      false;


    if (connecting) {

      if (
        now -
        attemptStarted >=
        WIFI_CONNECT_TIMEOUT_MS
      ) {

        Serial.print(
          "Wi-Fi connection timed out. status="
        );


        Serial.print(
          (int)status
        );


        Serial.print(
          " last_reason="
        );


        Serial.print(
          lastWiFiDisconnectReason
        );


        Serial.print(
          " ("
        );


        Serial.print(
          wifiReasonName(
            lastWiFiDisconnectReason
          )
        );


        Serial.println(
          ")"
        );


        WiFi.disconnect(
          false,
          false
        );


        connecting =
          false;

        retryAfter =
          now +
          WIFI_RETRY_PAUSE_MS;
      }


      vTaskDelay(
        pdMS_TO_TICKS(
          250
        )
      );


      continue;
    }


    if (
      (long)(
        now -
        retryAfter
      )
      >=
      0
    ) {

      int32_t targetChannel =
        0;

      int32_t targetRssi =
        -1000;

      uint8_t targetBssid[6] =
        {0, 0, 0, 0, 0, 0};


      if (
        !findTargetAccessPoint(
          targetChannel,
          targetBssid,
          targetRssi
        )
      ) {

        retryAfter =
          millis() +
          WIFI_RETRY_PAUSE_MS;


        vTaskDelay(
          pdMS_TO_TICKS(
            250
          )
        );


        continue;
      }


      Serial.print(
        "Wi-Fi connecting directly to 2.4 GHz AP for: "
      );


      Serial.println(
        EMI_WIFI_SSID
      );


      lastWiFiDisconnectReason =
        -1;


      WiFi.begin(
        EMI_WIFI_SSID,
        EMI_WIFI_PASSWORD,
        targetChannel,
        targetBssid,
        true
      );


      attemptStarted =
        millis();

      connecting =
        true;
    }


    vTaskDelay(
      pdMS_TO_TICKS(
        250
      )
    );
  }
}

void updateNetworkCommands() {

  if (
    networkCommandQueue ==
    nullptr
  ) {

    return;
  }


  NetworkCommand item = {};


  while (
    xQueueReceive(
      networkCommandQueue,
      &item,
      0
    )
    ==
    pdTRUE
  ) {

    handleCommand(
      String(
        item.text
      )
    );
  }
}


// ------------------------------------------------------------
// SETUP
// ------------------------------------------------------------

void setup() {

  Serial.begin(
    115200
  );


  Wire.begin(
    OLED_SDA,
    OLED_SCL
  );


  display.begin();


  display.setContrast(
    100
  );


  pinMode(
    TOUCH_PIN,
    INPUT
  );


  networkCommandQueue =
    xQueueCreate(
      4,
      sizeof(
        NetworkCommand
      )
    );


  httpMutex =
    xSemaphoreCreateMutex();


  bool microphoneReady =
    setupVoiceMicrophone();


  if (microphoneReady) {

    xTaskCreate(
      voiceTask,
      "emi-voice",
      6144,
      nullptr,
      1,
      nullptr
    );
  }

  else {

    Serial.println(
      "Voice control disabled: microphone setup failed."
    );
  }


  WiFi.onEvent(
    onWiFiEvent
  );


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


  bool txPowerSet =
    WiFi.setTxPower(
      WIFI_POWER_8_5dBm
    );


  Serial.print(
    "Wi-Fi TX power 8.5 dBm: "
  );


  Serial.println(
    txPowerSet
    ? "OK"
    : "FAILED"
  );


  xTaskCreate(
    networkTask,
    "emi-network",
    6144,
    nullptr,
    1,
    nullptr
  );


  randomSeed(
    micros()
  );


  gazeX =
    0;

  gazeY =
    0;


  lastDriveUpdate =
    millis();


  lastInteractionTime =
    millis();


  nextIdleAction =
    millis() +
    1800;


  scheduleNextBlink();

  scheduleNextAttentionBid();


  drawEmi();


  delay(
    250
  );


  Serial.println();
  Serial.println(
    "EMI C3 ready."
  );
  Serial.println(
    microphoneReady
    ? "Voice control: ON (local VAD -> Pi Whisper)"
    : "Voice control: OFF"
  );
  Serial.println(
    "Command: SHOW_TIME HH:MM"
  );
  Serial.println(
    "Example: SHOW_TIME 14:37"
  );
}


// ------------------------------------------------------------
// LOOP
// ------------------------------------------------------------

void loop() {

  updateSerialCommands();

  updateNetworkCommands();

  updatePersonalityDrives();

  updateTouch();

  updateClock();

  updateGaze();

  updateAttentionBid();

  updateIdleBehaviour();

  updatePettingBehaviour();

  drawEmi();


  delay(
    20
  );
}
