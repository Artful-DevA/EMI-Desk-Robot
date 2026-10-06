#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <math.h>

// ============================================================
// EMI - Breadboard Personality Prototype
// ESP32 DevKit V1
// SH1106 128x64 OLED
// TTP223 touch sensor on GPIO 27
//
// Version:
// - quick pet attention
// - slow petting drift
// - no jitter
// - no vertical bouncing
// ============================================================

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(
  U8G2_R0,
  U8X8_PIN_NONE
);

const int TOUCH_PIN = 27;

const int EYE_WIDTH  = 23;
const int EYE_HEIGHT = 31;

const int LEFT_EYE_X  = 24;
const int RIGHT_EYE_X = 81;
const int EYE_CENTER_Y = 32;

float gazeX = 0.0f;
float gazeY = 0.0f;

float gazeStartX = 0.0f;
float gazeStartY = 0.0f;
float gazeTargetX = 0.0f;
float gazeTargetY = 0.0f;

bool gazeMoving = false;

unsigned long gazeStartTime = 0;
unsigned long gazeDuration = 100;
unsigned long nextIdleGaze = 0;

bool blinking = false;

unsigned long blinkStartTime = 0;

int blinkCloseTime = 65;
int blinkHoldTime  = 20;
int blinkOpenTime  = 85;

unsigned long nextBlinkTime = 0;

bool doubleBlinkPending = false;
unsigned long secondBlinkTime = 0;

bool rawTouch = false;
bool previousRawTouch = false;
bool stableTouch = false;

unsigned long touchChangedAt = 0;

const unsigned long TOUCH_DEBOUNCE = 30;
const unsigned long PET_GRACE_TIME = 2200;

bool petting = false;

unsigned long lastTouchTime = 0;
unsigned long petStartTime = 0;

int patCount = 0;

unsigned long nextPetDrift = 0;

bool patPulseActive = false;
unsigned long patPulseStart = 0;
const unsigned long PAT_PULSE_DURATION = 520;

bool relaxedBlinkPending = false;
bool relaxedBlinkDone = false;

float smoothStep(float t) {
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  return t * t * (3.0f - 2.0f * t);
}

float getBlinkClosure() {
  if (!blinking) return 0.0f;

  unsigned long elapsed = millis() - blinkStartTime;
  int totalTime = blinkCloseTime + blinkHoldTime + blinkOpenTime;

  if (elapsed < blinkCloseTime) {
    float t = (float)elapsed / (float)blinkCloseTime;
    return smoothStep(t);
  }

  if (elapsed < blinkCloseTime + blinkHoldTime) {
    return 1.0f;
  }

  if (elapsed < totalTime) {
    float t =
      (float)(elapsed - blinkCloseTime - blinkHoldTime) /
      (float)blinkOpenTime;

    return 1.0f - smoothStep(t);
  }

  blinking = false;
  return 0.0f;
}

float getPatPulse() {
  if (!patPulseActive) return 0.0f;

  unsigned long elapsed = millis() - patPulseStart;

  if (elapsed >= PAT_PULSE_DURATION) {
    patPulseActive = false;
    return 0.0f;
  }

  float t = (float)elapsed / (float)PAT_PULSE_DURATION;
  return sinf(t * PI);
}

void drawEmi() {
  display.clearBuffer();

  float closure = getBlinkClosure();

  int eyeHeight =
    EYE_HEIGHT -
    (int)(closure * (EYE_HEIGHT - 4));

  int inward = 0;

  if (petting) {
    inward = 2;
  }

  float patPulse = getPatPulse();

  if (patPulse > 0.0f) {
    inward += (int)round(1.5f * patPulse);
  }

  int y =
    EYE_CENTER_Y -
    eyeHeight / 2 +
    round(gazeY);

  int radius = min(7, eyeHeight / 2);

  display.drawRBox(
    LEFT_EYE_X + round(gazeX) + inward,
    y,
    EYE_WIDTH,
    eyeHeight,
    radius
  );

  display.drawRBox(
    RIGHT_EYE_X + round(gazeX) - inward,
    y,
    EYE_WIDTH,
    eyeHeight,
    radius
  );

  display.sendBuffer();
}

void startGaze(
  float targetX,
  float targetY,
  unsigned long duration
) {
  gazeStartX = gazeX;
  gazeStartY = gazeY;

  gazeTargetX = targetX;
  gazeTargetY = targetY;

  gazeStartTime = millis();
  gazeDuration = duration;

  gazeMoving = true;
}

void updateGaze() {
  if (!gazeMoving) return;

  unsigned long elapsed = millis() - gazeStartTime;

  if (elapsed >= gazeDuration) {
    gazeX = gazeTargetX;
    gazeY = gazeTargetY;
    gazeMoving = false;
    return;
  }

  float t =
    (float)elapsed /
    (float)gazeDuration;

  float eased = smoothStep(t);

  gazeX =
    gazeStartX +
    (gazeTargetX - gazeStartX) * eased;

  gazeY =
    gazeStartY +
    (gazeTargetY - gazeStartY) * eased;
}

void startBlink() {
  if (blinking) return;

  blinking = true;

  blinkCloseTime = 65;
  blinkHoldTime  = 18;
  blinkOpenTime  = 90;

  blinkStartTime = millis();

  if (random(100) < 5) {
    doubleBlinkPending = true;
    secondBlinkTime = millis() + 330;
  }
}

void startRelaxedBlink() {
  if (blinking) return;

  blinking = true;

  blinkCloseTime = 105;
  blinkHoldTime  = 45;
  blinkOpenTime  = 160;

  blinkStartTime = millis();
}

void scheduleNextBlink() {
  nextBlinkTime =
    millis() +
    random(7000, 13000);
}

void chooseIdleGaze() {
  int roll = random(100);

  float targetX;
  float targetY;

  if (roll < 65) {
    targetX = random(-6, 7);
    targetY = random(-4, 5);
  }
  else if (roll < 93) {
    targetX = random(-13, 14);
    targetY = random(-8, 8);
  }
  else {
    targetX = random(-18, 19);
    targetY = random(-13, 11);
  }

  startGaze(
    targetX,
    targetY,
    random(90, 155)
  );

  nextIdleGaze =
    millis() +
    random(2200, 5500);
}

void beginPetting() {
  petting = true;
  patCount = 0;

  petStartTime = millis();

  relaxedBlinkPending = false;
  relaxedBlinkDone = false;

  patPulseActive = false;

  // Fast "EMI noticed me!" movement.
  startGaze(
    0,
    -15,
    105
  );

  nextPetDrift =
    millis() + 750;
}

void reactToPat() {
  patCount++;
  lastTouchTime = millis();

  // Small inward acknowledgement without eye-jitter.
  if (!patPulseActive) {
    patPulseActive = true;
    patPulseStart = millis();
  }

  if (
    patCount >= 3 &&
    !relaxedBlinkDone
  ) {
    relaxedBlinkPending = true;
  }
}

void updatePettingMotion() {
  if (!petting) return;

  unsigned long now = millis();

  // Slow, tiny horizontal drift while gaze remains high.
  if (
    !gazeMoving &&
    now >= nextPetDrift
  ) {
    float targetX = random(-2, 3);

    startGaze(
      targetX,
      -15,
      random(420, 700)
    );

    nextPetDrift =
      now +
      random(1000, 1900);
  }
}

void endPetting() {
  petting = false;
  patCount = 0;

  relaxedBlinkPending = false;
  relaxedBlinkDone = false;

  patPulseActive = false;

  startGaze(
    0,
    0,
    380
  );

  scheduleNextBlink();

  nextIdleGaze =
    millis() +
    random(1800, 3200);
}

void updateTouch() {
  unsigned long now = millis();

  rawTouch =
    digitalRead(TOUCH_PIN);

  if (
    rawTouch !=
    previousRawTouch
  ) {
    previousRawTouch = rawTouch;
    touchChangedAt = now;
  }

  if (
    now - touchChangedAt >
    TOUCH_DEBOUNCE
  ) {
    if (
      stableTouch !=
      rawTouch
    ) {
      stableTouch = rawTouch;

      if (stableTouch) {
        if (!petting) {
          beginPetting();
        }

        reactToPat();
      }
    }
  }

  if (stableTouch) {
    lastTouchTime = now;
  }

  if (
    petting &&
    !stableTouch &&
    now - lastTouchTime >
    PET_GRACE_TIME
  ) {
    endPetting();
  }
}

void updateIdleBehaviour() {
  if (petting) return;

  unsigned long now = millis();

  if (
    !gazeMoving &&
    now >= nextIdleGaze
  ) {
    chooseIdleGaze();
  }

  if (
    !blinking &&
    now >= nextBlinkTime
  ) {
    startBlink();
    scheduleNextBlink();
  }

  if (
    doubleBlinkPending &&
    !blinking &&
    now >= secondBlinkTime
  ) {
    doubleBlinkPending = false;
    startBlink();
  }
}

void updatePettingBehaviour() {
  if (!petting) return;

  unsigned long now = millis();

  updatePettingMotion();

  if (
    relaxedBlinkPending &&
    !relaxedBlinkDone &&
    !blinking &&
    now - lastTouchTime >
    500
  ) {
    startRelaxedBlink();

    relaxedBlinkPending = false;
    relaxedBlinkDone = true;
  }
}

void setup() {
  Wire.begin(21, 22);

  display.begin();

  pinMode(
    TOUCH_PIN,
    INPUT
  );

  randomSeed(micros());

  gazeX = 0;
  gazeY = 0;

  drawEmi();

  nextIdleGaze =
    millis() + 1600;

  scheduleNextBlink();
}

void loop() {
  updateTouch();
  updateGaze();
  updateIdleBehaviour();
  updatePettingBehaviour();

  drawEmi();

  delay(20);
}
