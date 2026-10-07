#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// ============================================================
// EMI - Breadboard Personality Prototype
// ESP32 DevKit V1
// SH1106 128x64 OLED
// TTP223 touch sensor on GPIO 27
//
// Personality revision:
// - internal curiosity + contentment drives
// - recent-action memory to avoid repeating the same idle move
// - several distinct idle behavior sequences
// - longer periods of deliberate stillness
// - fast upward attention when petting starts
// - every real stroke gets a contented partial eye-close
// - pet enjoyment grows slightly across a petting session
// - no vertical petting bounce
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
//
// These are intentionally simple.
// They make behavior depend on what EMI has been doing,
// instead of every idle movement being pure random chance.
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


// ------------------------------------------------------------
// MULTI-STAGE IDLE BEHAVIORS
// ------------------------------------------------------------

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
// TOUCH / PETTING
// ------------------------------------------------------------

bool rawTouch = false;
bool previousRawTouch = false;
bool stableTouch = false;

unsigned long touchChangedAt = 0;

const unsigned long TOUCH_DEBOUNCE = 30;
const unsigned long PET_GRACE_TIME = 2200;

// Prevent contact chatter from being interpreted as many pats.
const unsigned long MIN_PAT_INTERVAL = 280;

bool petting = false;

unsigned long lastTouchTime = 0;
unsigned long lastRecognizedPat = 0;

int patCount = 0;


// ------------------------------------------------------------
// PETTING MOTION
// ------------------------------------------------------------

unsigned long nextPetDrift = 0;


// ------------------------------------------------------------
// CONTENTED PARTIAL EYE-CLOSE
// ------------------------------------------------------------

bool petStrokeActive = false;

unsigned long petStrokeStart = 0;

const unsigned long PET_STROKE_DURATION = 430;

// This changes slightly as EMI becomes more content.
float petStrokeMaxClosure = 0.42f;


// ------------------------------------------------------------
// DEEP RELAXED PET BLINK
// ------------------------------------------------------------

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

  if (value < minimum) return minimum;
  if (value > maximum) return maximum;

  return value;
}


float smoothStep(float t) {

  t = clampFloat(
    t,
    0.0f,
    1.0f
  );

  return
    t * t *
    (3.0f - 2.0f * t);
}


// ------------------------------------------------------------
// PERSONALITY UPDATE
// ------------------------------------------------------------

void updatePersonalityDrives() {

  unsigned long now = millis();


  if (
    now - lastDriveUpdate <
    1000
  ) {

    return;
  }


  lastDriveUpdate = now;


  if (petting) {

    // Being interacted with settles EMI.
    curiosity -= 0.008f;
    contentment += 0.004f;
  }

  else {

    // When left alone, curiosity slowly grows.
    curiosity += 0.012f;

    // Contentment fades very slowly, not instantly.
    contentment -= 0.0025f;
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
// BLINK CLOSURE
// ------------------------------------------------------------

float getBlinkClosure() {

  if (!blinking) {
    return 0.0f;
  }


  unsigned long elapsed =
    millis() - blinkStartTime;


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

    return smoothStep(t);
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


  if (elapsed < totalTime) {

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
// PET STROKE CLOSURE
// ------------------------------------------------------------

float getPetStrokeClosure() {

  if (!petStrokeActive) {
    return 0.0f;
  }


  unsigned long elapsed =
    millis() - petStrokeStart;


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


  // Soft but fairly quick close.
  if (t < 0.28f) {

    float closeT =
      t / 0.28f;


    return
      petStrokeMaxClosure *
      smoothStep(closeT);
  }


  // Small contented hold.
  if (t < 0.48f) {

    return
      petStrokeMaxClosure;
  }


  // Slower reopen.
  float openT =
    (t - 0.48f) /
    0.52f;


  return
    petStrokeMaxClosure *
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
      (EYE_HEIGHT - 4)
    );


  int inward = 0;


  if (petting) {

    // Gentle petting expression.
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


  // Rare enough that it doesn't become a pattern.
  if (
    random(100) < 2
  ) {

    doubleBlinkPending = true;

    secondBlinkTime =
      millis() + 350;
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

  blinkStartTime =
    millis();
}


void scheduleNextBlink() {

  // Large window helps break the "robot timer" feeling.
  nextBlinkTime =
    millis()
    + random(
        9000,
        18000
      );
}


// ------------------------------------------------------------
// IDLE ACTION SELECTION
// ------------------------------------------------------------

IdleAction pickIdleAction() {

  int weights[IDLE_ACTION_COUNT] = {
    22, // still
    17, // micro glance
    18, // focus
    10, // curious peek
    8,  // wide glance
    12, // settle
    13  // soft attention
  };


  // High curiosity makes exploration more likely.
  weights[IDLE_CURIOUS_PEEK] +=
    (int)(
      curiosity * 18.0f
    );


  weights[IDLE_WIDE_GLANCE] +=
    (int)(
      curiosity * 14.0f
    );


  weights[IDLE_FOCUS] +=
    (int)(
      curiosity * 8.0f
    );


  // High contentment favors calm / centered behavior.
  weights[IDLE_STILL] +=
    (int)(
      contentment * 18.0f
    );


  weights[IDLE_SOFT_ATTENTION] +=
    (int)(
      contentment * 13.0f
    );


  weights[IDLE_SETTLE] +=
    (int)(
      contentment * 8.0f
    );


  // Strongly discourage repeating the last two actions.
  weights[lastIdleAction] =
    max(
      1,
      weights[lastIdleAction] / 5
    );


  weights[previousIdleAction] =
    max(
      1,
      weights[previousIdleAction] / 2
    );


  int totalWeight = 0;


  for (
    int i = 0;
    i < IDLE_ACTION_COUNT;
    i++
  ) {

    totalWeight +=
      weights[i];
  }


  int roll =
    random(totalWeight);


  for (
    int i = 0;
    i < IDLE_ACTION_COUNT;
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


  // ----------------------------------------------------------
  // STILL
  // ----------------------------------------------------------

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


  // ----------------------------------------------------------
  // MICRO GLANCE
  // ----------------------------------------------------------

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


    curiosity -= 0.035f;


    nextIdleAction =
      now
      + random(
          2300,
          4800
        );

    return;
  }


  // ----------------------------------------------------------
  // FOCUS + TINY CORRECTION
  // ----------------------------------------------------------

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


    curiosity -= 0.08f;


    nextIdleAction =
      now
      + random(
          3800,
          6900
        );

    return;
  }


  // ----------------------------------------------------------
  // CURIOUS PEEK
  // ----------------------------------------------------------

  if (
    action ==
    IDLE_CURIOUS_PEEK
  ) {

    float targetX;


    if (
      random(2) == 0
    ) {

      targetX =
        random(-18, -13);
    }

    else {

      targetX =
        random(13, 19);
    }


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


    curiosity -= 0.16f;


    nextIdleAction =
      now
      + random(
          4800,
          7600
        );

    return;
  }


  // ----------------------------------------------------------
  // WIDE EXPLORATORY GLANCE
  // ----------------------------------------------------------

  if (
    action ==
    IDLE_WIDE_GLANCE
  ) {

    float targetX;


    if (
      random(2) == 0
    ) {

      targetX =
        random(-18, -11);
    }

    else {

      targetX =
        random(11, 19);
    }


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


    curiosity -= 0.20f;


    nextIdleAction =
      now
      + random(
          5000,
          8200
        );

    return;
  }


  // ----------------------------------------------------------
  // SETTLE
  // ----------------------------------------------------------

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


  // ----------------------------------------------------------
  // SOFT ATTENTION
  //
  // A calm near-center, slightly-upward look.
  // ----------------------------------------------------------

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
    FOLLOWUP_NONE
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

    // Stay on the same side, but relax the extreme peek.
    float targetX;


    if (gazeX < 0) {

      targetX =
        random(-10, -5);
    }

    else {

      targetX =
        random(5, 11);
    }


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
// BEGIN PETTING
// ------------------------------------------------------------

void beginPetting() {

  petting = true;

  patCount = 0;

  relaxedBlinkPending = false;
  relaxedBlinkDone = false;

  idleFollowup =
    FOLLOWUP_NONE;


  // Immediate attention.
  //
  // 30 ms debounce + ~85 ms movement makes this feel
  // like EMI notices your hand almost instantly.
  startGaze(
    0,
    -15,
    85
  );


  nextPetDrift =
    millis() + 1100;
}


// ------------------------------------------------------------
// ONE PET STROKE
// ------------------------------------------------------------

void reactToPat() {

  unsigned long now =
    millis();


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


  // Each stroke increases contentment.
  contentment +=
    0.11f;


  contentment =
    clampFloat(
      contentment,
      0.0f,
      1.0f
    );


  curiosity -=
    0.04f;


  curiosity =
    clampFloat(
      curiosity,
      0.0f,
      1.0f
    );


  // First strokes are soft.
  // Repeated petting makes the eye-close a little deeper.
  petStrokeMaxClosure =
    0.36f +
    min(
      patCount,
      5
    )
    * 0.035f +
    contentment * 0.05f;


  petStrokeMaxClosure =
    clampFloat(
      petStrokeMaxClosure,
      0.40f,
      0.60f
    );


  petStrokeActive = true;

  petStrokeStart =
    now;


  // Don't deep-blink immediately.
  // Let enjoyment build across several real strokes.
  if (
    patCount >= 4 &&
    !relaxedBlinkDone
  ) {

    relaxedBlinkPending = true;
  }
}


// ------------------------------------------------------------
// PETTING MOTION
// ------------------------------------------------------------

void updatePettingMotion() {

  if (!petting) {
    return;
  }


  unsigned long now =
    millis();


  // Tiny, slow horizontal movement only.
  //
  // The first reaction is quick;
  // once EMI is enjoying the pet, movement becomes calm.
  if (
    !gazeMoving &&
    !petStrokeActive &&
    now >= nextPetDrift
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


// ------------------------------------------------------------
// END PETTING
// ------------------------------------------------------------

void endPetting() {

  petting = false;

  patCount = 0;

  relaxedBlinkPending = false;
  relaxedBlinkDone = false;

  petStrokeActive = false;


  // Linger slightly upward if contentment is high.
  float returnY =
    -2.0f -
    contentment * 2.0f;


  startGaze(
    random(-3, 4),
    returnY,
    460
  );


  scheduleNextBlink();


  // After being petted, EMI naturally spends more time
  // in calm behaviors because contentment remains elevated.
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
    now - touchChangedAt >
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

    lastTouchTime =
      now;
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


// ------------------------------------------------------------
// IDLE UPDATE
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
      FOLLOWUP_NONE &&
    now >= nextIdleAction
  ) {

    chooseIdleAction();
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
    now - lastRecognizedPat >
    500
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


  lastDriveUpdate =
    millis();


  nextIdleAction =
    millis() + 1800;


  scheduleNextBlink();
}


// ------------------------------------------------------------
// LOOP
// ------------------------------------------------------------

void loop() {

  updatePersonalityDrives();

  updateTouch();

  updateGaze();

  updateIdleBehaviour();

  updatePettingBehaviour();

  drawEmi();


  delay(20);
}
