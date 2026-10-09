#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// ============================================================
// EMI - EYES + PETTING TEST
//
// ESP32-C3 Super Mini
//
// OLED SDA  -> GPIO 8
// OLED SCL  -> GPIO 7
// TTP223 OUT -> GPIO 10
// OLED VCC  -> 3V3
// TTP223 VCC -> 3V3
// All grounds together
//
// No Wi-Fi.
// No microphone.
// No Raspberry Pi.
// Just Emi's eyes, idle movement, blinking, and pet reactions.
// ============================================================


// ------------------------------------------------------------
// PINS
// ------------------------------------------------------------

const int OLED_SDA = 8;
const int OLED_SCL = 7;
const int TOUCH_PIN = 10;


// ------------------------------------------------------------
// DISPLAY
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

unsigned long nextIdleMove = 0;


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
// PETTING
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
// GAZE
// ------------------------------------------------------------

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
  if (!gazeMoving) {
    return;
  }

  unsigned long elapsed =
    millis() -
    gazeStartTime;

  if (
    elapsed >=
    gazeDuration
  ) {
    gazeX = gazeTargetX;
    gazeY = gazeTargetY;

    gazeMoving = false;
    return;
  }

  float t =
    (float)elapsed /
    (float)gazeDuration;

  float eased =
    smoothStep(t);

  gazeX =
    gazeStartX +
    (
      gazeTargetX -
      gazeStartX
    ) *
    eased;

  gazeY =
    gazeStartY +
    (
      gazeTargetY -
      gazeStartY
    ) *
    eased;
}


// ------------------------------------------------------------
// BLINKS
// ------------------------------------------------------------

void scheduleNextBlink() {
  nextBlinkTime =
    millis() +
    random(
      5000,
      12000
    );
}


void startBlink() {
  if (
    blinking ||
    petting
  ) {
    return;
  }

  blinking = true;

  blinkCloseTime = 65;
  blinkHoldTime = 18;
  blinkOpenTime = 95;

  blinkStartTime = millis();

  if (
    random(100) <
    4
  ) {
    doubleBlinkPending = true;

    secondBlinkTime =
      millis() +
      350;
  }
}


void startRelaxedBlink() {
  if (blinking) {
    return;
  }

  blinking = true;

  blinkCloseTime = 120;
  blinkHoldTime = 70;
  blinkOpenTime = 200;

  blinkStartTime = millis();
}


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
    (unsigned long)blinkCloseTime
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
      ) /
      (float)blinkOpenTime;

    return
      1.0f -
      smoothStep(t);
  }

  blinking = false;

  return 0.0f;
}


// ------------------------------------------------------------
// PET EXPRESSION
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
    petStrokeActive = false;
    return 0.0f;
  }

  float t =
    (float)elapsed /
    (float)PET_STROKE_DURATION;

  if (t < 0.24f) {
    return
      petStrokeMaxClosure *
      smoothStep(
        t / 0.24f
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
      smoothStep(openT)
    );
}


// ------------------------------------------------------------
// DRAW FACE
// ------------------------------------------------------------

void drawEyes() {
  float closure =
    getBlinkClosure();

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


void drawEmi() {
  display.clearBuffer();

  drawEyes();

  display.sendBuffer();
}


// ------------------------------------------------------------
// IDLE BEHAVIOUR
// ------------------------------------------------------------

void scheduleNextIdleMove() {
  nextIdleMove =
    millis() +
    random(
      1800,
      5200
    );
}


void updateIdleBehaviour() {
  if (petting) {
    return;
  }

  unsigned long now =
    millis();

  if (
    !gazeMoving &&
    now >=
    nextIdleMove
  ) {
    int choice =
      random(100);

    if (choice < 22) {
      startGaze(
        random(-3, 4),
        random(-2, 3),
        random(160, 300)
      );
    }

    else if (choice < 78) {
      startGaze(
        random(-14, 15),
        random(-7, 8),
        random(90, 180)
      );
    }

    else {
      float side =
        random(2) == 0
        ? -17.0f
        : 17.0f;

      startGaze(
        side,
        random(-8, 5),
        random(90, 160)
      );
    }

    scheduleNextIdleMove();
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
    doubleBlinkPending = false;
    startBlink();
  }
}


// ------------------------------------------------------------
// PETTING BEHAVIOUR
// ------------------------------------------------------------

void beginPetting() {
  petting = true;

  patCount = 0;

  relaxedBlinkPending = false;
  relaxedBlinkDone = false;

  doubleBlinkPending = false;

  lastTouchTime = millis();

  startGaze(
    0,
    -14,
    90
  );

  nextPetDrift =
    millis() +
    1100;

  Serial.println(
    "PET: start"
  );
}


void reactToPat() {
  unsigned long now =
    millis();

  if (
    lastRecognizedPat != 0 &&
    now -
    lastRecognizedPat <
    MIN_PAT_INTERVAL
  ) {
    return;
  }

  lastRecognizedPat = now;
  lastTouchTime = now;

  patCount++;

  petStrokeMaxClosure =
    0.22f +
    min(
      patCount,
      5
    ) *
    0.018f;

  petStrokeMaxClosure =
    clampFloat(
      petStrokeMaxClosure,
      0.24f,
      0.32f
    );

  petStrokeActive = true;
  petStrokeStart = now;

  startGaze(
    random(-2, 3),
    -14,
    100
  );

  if (
    patCount >= 4 &&
    !relaxedBlinkDone
  ) {
    relaxedBlinkPending = true;
  }

  Serial.print(
    "PET: pat "
  );

  Serial.println(
    patCount
  );
}


void endPetting() {
  petting = false;

  patCount = 0;

  relaxedBlinkPending = false;
  relaxedBlinkDone = false;

  petStrokeActive = false;

  startGaze(
    random(-3, 4),
    random(-4, -1),
    460
  );

  scheduleNextBlink();
  scheduleNextIdleMove();

  Serial.println(
    "PET: end"
  );
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
      -14,
      random(
        500,
        850
      )
    );

    nextPetDrift =
      now +
      random(
        1700,
        3100
      );
  }
}


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

    relaxedBlinkPending = false;
    relaxedBlinkDone = true;
  }
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
    now -
    lastTouchTime >
    PET_GRACE_TIME
  ) {
    endPetting();
  }
}


// ------------------------------------------------------------
// SETUP / LOOP
// ------------------------------------------------------------

void setup() {
  Serial.begin(
    115200
  );

  delay(250);

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

  randomSeed(
    micros()
  );

  gazeX = 0;
  gazeY = 0;

  scheduleNextBlink();
  scheduleNextIdleMove();

  drawEmi();

  Serial.println();
  Serial.println(
    "EMI EYES + PET TEST READY"
  );
  Serial.println(
    "Touch GPIO10 sensor to pet Emi."
  );
}


void loop() {
  updateTouch();

  updateGaze();

  updateIdleBehaviour();

  updatePettingBehaviour();

  drawEmi();

  delay(20);
}
