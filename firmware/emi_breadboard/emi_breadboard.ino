#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// ============================================================
// EMI - Breadboard Personality Prototype
// ESP32 DevKit V1
// SH1106 128x64 OLED
// TTP223 touch sensor on GPIO 27
//
// Version:
// - richer non-repetitive idle behavior
// - quick upward attention when petting starts
// - partial contented eye-close on every real pat
// - deeper relaxed blink after several pats
// - no vertical petting bounce
// - tiny slow petting drift only
// ============================================================


// ------------------------------------------------------------
// OLED
// ------------------------------------------------------------

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(
  U8G2_R0,
  U8X8_PIN_NONE
);


// ------------------------------------------------------------
// PINS
// ------------------------------------------------------------

const int TOUCH_PIN = 27;


// ------------------------------------------------------------
// EYE GEOMETRY
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
// IDLE BEHAVIOR
// ------------------------------------------------------------

unsigned long nextIdleAction = 0;

enum IdleFollowup {
  IDLE_FOLLOWUP_NONE,
  IDLE_FOLLOWUP_CORRECTION,
  IDLE_FOLLOWUP_RETURN_CENTER
};

IdleFollowup idleFollowup = IDLE_FOLLOWUP_NONE;
unsigned long idleFollowupTime = 0;


// ------------------------------------------------------------
// BLINKING
// ------------------------------------------------------------

bool blinking = false;

unsigned long blinkStartTime = 0;

int blinkCloseTime = 65;
int blinkHoldTime = 18;
int blinkOpenTime = 90;

unsigned long nextBlinkTime = 0;

bool doubleBlinkPending = false;
unsigned long secondBlinkTime = 0;


// ------------------------------------------------------------
// TOUCH / PETTING
// ------------------------------------------------------------

bool rawTouch = false;
bool previousRawTouch = false;
bool stableTouch = false;

unsigned long touchChangedAt = 0;

const unsigned long TOUCH_DEBOUNCE = 30;
const unsigned long PET_GRACE_TIME = 2200;

// Ignore extremely fast re-triggers from contact chatter.
const unsigned long MIN_PAT_INTERVAL = 260;

bool petting = false;

unsigned long lastTouchTime = 0;
unsigned long lastRecognizedPat = 0;

int patCount = 0;


// ------------------------------------------------------------
// PETTING MOTION
// ------------------------------------------------------------

unsigned long nextPetDrift = 0;


// ------------------------------------------------------------
// CONTENTED PARTIAL EYE-CLOSE ON EACH PAT
// ------------------------------------------------------------

bool petStrokeActive = false;
unsigned long petStrokeStart = 0;

const unsigned long PET_STROKE_DURATION = 360;

// 0.0 = fully open
// 1.0 = completely closed
const float PET_STROKE_MAX_CLOSURE = 0.48f;


// ------------------------------------------------------------
// DEEPER RELAXED BLINK
// ------------------------------------------------------------

bool relaxedBlinkPending = false;
bool relaxedBlinkDone = false;


// ------------------------------------------------------------
// HELPERS
// ------------------------------------------------------------

float smoothStep(float t) {

  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;

  return t * t * (3.0f - 2.0f * t);
}


float clampFloat(
  float value,
  float minimum,
  float maximum
) {

  if (value < minimum) return minimum;
  if (value > maximum) return maximum;

  return value;
}


// ------------------------------------------------------------
// NORMAL / RELAXED BLINK CLOSURE
// ------------------------------------------------------------

float getBlinkClosure() {

  if (!blinking) {
    return 0.0f;
  }

  unsigned long elapsed =
    millis() - blinkStartTime;

  int totalTime =
    blinkCloseTime +
    blinkHoldTime +
    blinkOpenTime;


  // Closing
  if (elapsed < (unsigned long)blinkCloseTime) {

    float t =
      (float)elapsed /
      (float)blinkCloseTime;

    return smoothStep(t);
  }


  // Closed
  if (
    elapsed <
    (unsigned long)(
      blinkCloseTime +
      blinkHoldTime
    )
  ) {

    return 1.0f;
  }


  // Opening
  if (elapsed < (unsigned long)totalTime) {

    float t =
      (float)(
        elapsed -
        blinkCloseTime -
        blinkHoldTime
      )
      /
      (float)blinkOpenTime;

    return
      1.0f -
      smoothStep(t);
  }


  blinking = false;

  return 0.0f;
}


// ------------------------------------------------------------
// CONTENTED PAT CLOSURE
// ------------------------------------------------------------

float getPetStrokeClosure() {

  if (!petStrokeActive) {
    return 0.0f;
  }

  unsigned long elapsed =
    millis() - petStrokeStart;


  if (elapsed >= PET_STROKE_DURATION) {

    petStrokeActive = false;

    return 0.0f;
  }


  float t =
    (float)elapsed /
    (float)PET_STROKE_DURATION;


  // Quick soft close for the first 30%.
  if (t < 0.30f) {

    float closeT =
      t / 0.30f;

    return
      PET_STROKE_MAX_CLOSURE *
      smoothStep(closeT);
  }


  // Brief contented hold.
  if (t < 0.46f) {

    return
      PET_STROKE_MAX_CLOSURE;
  }


  // Slower reopening.
  float openT =
    (t - 0.46f) /
    0.54f;

  return
    PET_STROKE_MAX_CLOSURE *
    (
      1.0f -
      smoothStep(openT)
    );
}


// ------------------------------------------------------------
// DRAW EMI
// ------------------------------------------------------------

void drawEmi() {

  display.clearBuffer();


  float blinkClosure =
    getBlinkClosure();

  float petClosure =
    getPetStrokeClosure();


  // Whichever expression is more closed wins.
  float closure =
    max(
      blinkClosure,
      petClosure
    );


  int eyeHeight =
    EYE_HEIGHT -
    (int)(
      closure *
      (EYE_HEIGHT - 4)
    );


  int inward = 0;


  if (petting) {

    // Slightly softer / closer expression.
    inward = 2;
  }


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


  display.sendBuffer();
}


// ------------------------------------------------------------
// GAZE MOVEMENT
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


  if (elapsed >= gazeDuration) {

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
    )
    * eased;


  gazeY =
    gazeStartY +
    (
      gazeTargetY -
      gazeStartY
    )
    * eased;
}


// ------------------------------------------------------------
// BLINKS
// ------------------------------------------------------------

void startBlink() {

  if (blinking) {
    return;
  }


  blinking = true;

  blinkCloseTime = 65;
  blinkHoldTime = 18;
  blinkOpenTime = 95;

  blinkStartTime =
    millis();


  // Very rare double blink.
  if (random(100) < 3) {

    doubleBlinkPending = true;

    secondBlinkTime =
      millis() + 340;
  }
}


void startRelaxedBlink() {

  if (blinking) {
    return;
  }


  blinking = true;

  blinkCloseTime = 115;
  blinkHoldTime = 65;
  blinkOpenTime = 190;

  blinkStartTime =
    millis();
}


void scheduleNextBlink() {

  // Less repetitive than before.
  nextBlinkTime =
    millis()
    + random(
        8500,
        16500
      );
}


// ------------------------------------------------------------
// IDLE PERSONALITY
// ------------------------------------------------------------

void chooseIdleAction() {

  unsigned long now =
    millis();


  idleFollowup =
    IDLE_FOLLOWUP_NONE;


  int roll =
    random(100);


  // ----------------------------------------------------------
  // 0-31:
  // Tiny micro-saccade from the current position.
  // ----------------------------------------------------------

  if (roll < 32) {

    float targetX =
      clampFloat(
        gazeX +
        random(-3, 4),
        -18,
        18
      );


    float targetY =
      clampFloat(
        gazeY +
        random(-2, 3),
        -13,
        10
      );


    startGaze(
      targetX,
      targetY,
      random(
        75,
        120
      )
    );


    nextIdleAction =
      now
      + random(
          2600,
          5200
        );

    return;
  }


  // ----------------------------------------------------------
  // 32-61:
  // Focus on something, then make a tiny correction.
  // ----------------------------------------------------------

  if (roll < 62) {

    float targetX =
      random(-13, 14);

    float targetY =
      random(-8, 8);


    unsigned long moveTime =
      random(
        90,
        150
      );


    startGaze(
      targetX,
      targetY,
      moveTime
    );


    idleFollowup =
      IDLE_FOLLOWUP_CORRECTION;


    idleFollowupTime =
      now +
      moveTime +
      random(
        350,
        850
      );


    nextIdleAction =
      now
      + random(
          3200,
          6000
        );

    return;
  }


  // ----------------------------------------------------------
  // 62-77:
  // Larger exploratory glance, then naturally settle back.
  // ----------------------------------------------------------

  if (roll < 78) {

    float targetX;

    if (random(2) == 0) {
      targetX =
        random(-18, -11);
    }
    else {
      targetX =
        random(11, 19);
    }


    float targetY =
      random(-10, 9);


    unsigned long moveTime =
      random(
        95,
        160
      );


    startGaze(
      targetX,
      targetY,
      moveTime
    );


    idleFollowup =
      IDLE_FOLLOWUP_RETURN_CENTER;


    idleFollowupTime =
      now +
      moveTime +
      random(
        900,
        1800
      );


    nextIdleAction =
      now
      + random(
          4500,
          7200
        );

    return;
  }


  // ----------------------------------------------------------
  // 78-91:
  // Do absolutely nothing for a while.
  //
  // Stillness is part of natural behavior.
  // ----------------------------------------------------------

  if (roll < 92) {

    nextIdleAction =
      now
      + random(
          4800,
          8500
        );

    return;
  }


  // ----------------------------------------------------------
  // 92-99:
  // Slowly settle near center.
  // ----------------------------------------------------------

  startGaze(
    random(-4, 5),
    random(-3, 4),
    random(
      180,
      300
    )
  );


  nextIdleAction =
    now
    + random(
        3400,
        6500
      );
}


void updateIdleFollowup() {

  if (
    idleFollowup ==
    IDLE_FOLLOWUP_NONE
  ) {

    return;
  }


  if (gazeMoving) {
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
    IDLE_FOLLOWUP_CORRECTION
  ) {

    // Tiny "second look" at the same thing.
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
    IDLE_FOLLOWUP_RETURN_CENTER
  ) {

    // Don't snap exactly to 0,0.
    startGaze(
      random(-5, 6),
      random(-3, 4),
      random(
        130,
        220
      )
    );
  }


  idleFollowup =
    IDLE_FOLLOWUP_NONE;
}


// ------------------------------------------------------------
// PETTING
// ------------------------------------------------------------

void beginPetting() {

  petting = true;

  patCount = 0;

  relaxedBlinkPending = false;
  relaxedBlinkDone = false;

  idleFollowup =
    IDLE_FOLLOWUP_NONE;


  // Bring back the immediate "EMI noticed me" feeling.
  //
  // The touch debounce is already 30 ms,
  // then the gaze itself only takes about 95 ms.
  startGaze(
    0,
    -15,
    95
  );


  nextPetDrift =
    millis() + 950;
}


void reactToPat() {

  unsigned long now =
    millis();


  // Reject fast sensor chatter, but allow normal separate strokes.
  if (
    lastRecognizedPat != 0 &&
    now - lastRecognizedPat <
    MIN_PAT_INTERVAL
  ) {

    return;
  }


  lastRecognizedPat =
    now;

  lastTouchTime =
    now;

  patCount++;


  // Every real stroke gets a smooth partial eye-close.
  //
  // This is the main "I like that" feedback.
  petStrokeActive = true;
  petStrokeStart = now;


  // After several strokes, EMI also gives one deeper,
  // slower relaxed blink.
  if (
    patCount >= 3 &&
    !relaxedBlinkDone
  ) {

    relaxedBlinkPending = true;
  }
}


void updatePettingMotion() {

  if (!petting) {
    return;
  }


  unsigned long now =
    millis();


  // Very small horizontal attention changes.
  // Never move vertically while already being petted.
  if (
    !gazeMoving &&
    !petStrokeActive &&
    now >= nextPetDrift
  ) {

    startGaze(
      random(-2, 3),
      -15,
      random(
        450,
        750
      )
    );


    nextPetDrift =
      now
      + random(
          1400,
          2400
        );
  }
}


void endPetting() {

  petting = false;

  patCount = 0;

  relaxedBlinkPending = false;
  relaxedBlinkDone = false;

  petStrokeActive = false;


  // Calm return from the high petting gaze.
  startGaze(
    random(-2, 3),
    random(-1, 3),
    420
  );


  scheduleNextBlink();


  nextIdleAction =
    millis()
    + random(
        2000,
        3800
      );
}


// ------------------------------------------------------------
// TOUCH INPUT
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
    now - touchChangedAt >
    TOUCH_DEBOUNCE
  ) {

    if (
      stableTouch !=
      rawTouch
    ) {

      stableTouch =
        rawTouch;


      // Rising edge = a new physical contact / stroke.
      if (stableTouch) {

        if (!petting) {

          beginPetting();
        }


        reactToPat();
      }
    }
  }


  // Holding the pad keeps the overall petting session alive.
  if (stableTouch) {

    lastTouchTime =
      now;
  }


  // Slow pats remain one continuous interaction.
  if (
    petting &&
    !stableTouch &&
    now - lastTouchTime >
    PET_GRACE_TIME
  ) {

    endPetting();
  }
}


// ------------------------------------------------------------
// UPDATE IDLE
// ------------------------------------------------------------

void updateIdleBehaviour() {

  if (petting) {
    return;
  }


  unsigned long now =
    millis();


  updateIdleFollowup();


  if (
    !gazeMoving &&
    idleFollowup ==
      IDLE_FOLLOWUP_NONE &&
    now >= nextIdleAction
  ) {

    chooseIdleAction();
  }


  // Normal blink.
  if (
    !blinking &&
    now >= nextBlinkTime
  ) {

    startBlink();

    scheduleNextBlink();
  }


  // Rare double blink.
  if (
    doubleBlinkPending &&
    !blinking &&
    now >= secondBlinkTime
  ) {

    doubleBlinkPending = false;

    startBlink();
  }
}


// ------------------------------------------------------------
// UPDATE PETTING
// ------------------------------------------------------------

void updatePettingBehaviour() {

  if (!petting) {
    return;
  }


  unsigned long now =
    millis();


  updatePettingMotion();


  // Wait until the latest partial pet eye-close has finished,
  // then give the deeper blink after several strokes.
  if (
    relaxedBlinkPending &&
    !relaxedBlinkDone &&
    !blinking &&
    !petStrokeActive &&
    now - lastRecognizedPat >
    420
  ) {

    startRelaxedBlink();

    relaxedBlinkPending = false;
    relaxedBlinkDone = true;
  }
}


// ------------------------------------------------------------
// SETUP
// ------------------------------------------------------------

void setup() {

  Wire.begin(
    21,
    22
  );


  display.begin();


  pinMode(
    TOUCH_PIN,
    INPUT
  );


  randomSeed(
    micros()
  );


  gazeX = 0;
  gazeY = 0;


  drawEmi();


  nextIdleAction =
    millis() + 1700;


  scheduleNextBlink();
}


// ------------------------------------------------------------
// MAIN LOOP
// ------------------------------------------------------------

void loop() {

  updateTouch();

  updateGaze();

  updateIdleBehaviour();

  updatePettingBehaviour();

  drawEmi();


  // About 50 FPS.
  delay(20);
}
