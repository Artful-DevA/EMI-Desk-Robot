#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// ============================================================
// EMI - Breadboard Personality Prototype
// ESP32 DevKit V1
// SH1106 128x64 OLED
// TTP223 touch sensor on GPIO 27
//
// Current test build:
// - stateful idle personality
// - occasional attention-seeking blink
// - gentler pet-stroke eye close
// - 10-minute miniature timer
// - one-shot eyes -> clock -> eyes transition demo
//
// NOTE:
// The clock demo intentionally shows 12:34 because real network
// time is not connected yet. It is only testing the animation.
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
// ATTENTION BID
//
// This is deliberately noticeable because blinking is what most
// reliably catches your eye.
//
// Prototype timing is fairly short so you can test it. In final
// EMI this should be calendar/DND aware and happen much less.
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


// ------------------------------------------------------------
// PETTING MOTION
// ------------------------------------------------------------

unsigned long nextPetDrift = 0;


// ------------------------------------------------------------
// GENTLE CONTENTED PARTIAL EYE-CLOSE
// ------------------------------------------------------------

bool petStrokeActive = false;

unsigned long petStrokeStart = 0;

const unsigned long PET_STROKE_DURATION = 360;

float petStrokeMaxClosure = 0.27f;


// ------------------------------------------------------------
// DEEP RELAXED PET BLINK
// ------------------------------------------------------------

bool relaxedBlinkPending = false;
bool relaxedBlinkDone = false;


// ------------------------------------------------------------
// 10-MINUTE MINI TIMER TEST
// ------------------------------------------------------------

const unsigned long TEST_TIMER_DURATION_MS =
  10UL * 60UL * 1000UL;

unsigned long testTimerStart = 0;


// ------------------------------------------------------------
// EYES -> CLOCK -> EYES TRANSITION TEST
// ------------------------------------------------------------

enum ClockDemoState {
  CLOCK_DEMO_OFF = 0,
  CLOCK_EYES_CLOSING,
  CLOCK_REVEALING,
  CLOCK_HOLDING,
  CLOCK_HIDING,
  CLOCK_EYES_OPENING
};

ClockDemoState clockDemoState = CLOCK_DEMO_OFF;

bool clockDemoDone = false;

unsigned long clockDemoStart = 0;
unsigned long clockStageStart = 0;

const unsigned long CLOCK_DEMO_TRIGGER_MS = 12000;
const unsigned long CLOCK_EYE_CLOSE_MS = 520;
const unsigned long CLOCK_REVEAL_MS = 420;
const unsigned long CLOCK_HOLD_MS = 2600;
const unsigned long CLOCK_HIDE_MS = 380;
const unsigned long CLOCK_EYE_OPEN_MS = 560;

const char CLOCK_DEMO_TEXT[] = "12:34";


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
    curiosity -= 0.008f;
    contentment += 0.004f;
  }
  else {
    curiosity += 0.012f;
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

  if (t < 0.24f) {
    float closeT =
      t / 0.24f;

    return
      petStrokeMaxClosure *
      smoothStep(closeT);
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
// MINI TIMER
// ------------------------------------------------------------

void drawMiniTimer() {
  if (
    petting ||
    clockDemoState != CLOCK_DEMO_OFF
  ) {
    return;
  }

  unsigned long elapsed =
    millis() - testTimerStart;

  unsigned long remainingMs;

  if (
    elapsed >=
    TEST_TIMER_DURATION_MS
  ) {
    remainingMs = 0;
  }
  else {
    remainingMs =
      TEST_TIMER_DURATION_MS -
      elapsed;
  }

  unsigned long totalSeconds =
    (remainingMs + 999UL) /
    1000UL;

  unsigned int minutes =
    totalSeconds / 60UL;

  unsigned int seconds =
    totalSeconds % 60UL;

  char timerText[6];

  snprintf(
    timerText,
    sizeof(timerText),
    "%02u:%02u",
    minutes,
    seconds
  );

  display.setFont(
    u8g2_font_4x6_tf
  );

  int textWidth =
    display.getStrWidth(
      timerText
    );

  int textX =
    127 -
    textWidth;

  display.setDrawColor(0);

  display.drawBox(
    textX - 1,
    0,
    textWidth + 2,
    8
  );

  display.setDrawColor(1);

  display.drawStr(
    textX,
    6,
    timerText
  );
}


// ------------------------------------------------------------
// DRAW NORMAL EYES
// ------------------------------------------------------------

void drawEyesWithClosure(
  float forcedClosure
) {
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
      (EYE_HEIGHT - 3)
    );

  int inward = 0;

  if (petting) {
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
}


// ------------------------------------------------------------
// DRAW CLOCK DEMO
// ------------------------------------------------------------

void drawClockDemo() {
  unsigned long now =
    millis();

  if (
    clockDemoState ==
    CLOCK_EYES_CLOSING
  ) {
    float t =
      (float)(
        now - clockStageStart
      )
      /
      (float)CLOCK_EYE_CLOSE_MS;

    float eased =
      smoothStep(t);

    drawEyesWithClosure(
      eased
    );

    return;
  }


  display.setFont(
    u8g2_font_logisoso20_tn
  );

  int textWidth =
    display.getStrWidth(
      CLOCK_DEMO_TEXT
    );

  int textX =
    (128 - textWidth) / 2;

  int textY = 41;


  if (
    clockDemoState ==
    CLOCK_REVEALING
  ) {
    float t =
      (float)(
        now - clockStageStart
      )
      /
      (float)CLOCK_REVEAL_MS;

    t =
      clampFloat(
        t,
        0.0f,
        1.0f
      );

    float eased =
      smoothStep(t);

    int halfVisible =
      (int)(
        (textWidth / 2 + 2) *
        eased
      );

    int centerX = 64;

    display.setClipWindow(
      centerX - halfVisible,
      0,
      centerX + halfVisible,
      64
    );

    display.drawStr(
      textX,
      textY,
      CLOCK_DEMO_TEXT
    );

    display.setMaxClipWindow();

    return;
  }


  if (
    clockDemoState ==
    CLOCK_HOLDING
  ) {
    display.drawStr(
      textX,
      textY,
      CLOCK_DEMO_TEXT
    );

    return;
  }


  if (
    clockDemoState ==
    CLOCK_HIDING
  ) {
    float t =
      (float)(
        now - clockStageStart
      )
      /
      (float)CLOCK_HIDE_MS;

    t =
      clampFloat(
        t,
        0.0f,
        1.0f
      );

    float eased =
      1.0f -
      smoothStep(t);

    int halfVisible =
      (int)(
        (textWidth / 2 + 2) *
        eased
      );

    int centerX = 64;

    if (halfVisible > 0) {
      display.setClipWindow(
        centerX - halfVisible,
        0,
        centerX + halfVisible,
        64
      );

      display.drawStr(
        textX,
        textY,
        CLOCK_DEMO_TEXT
      );

      display.setMaxClipWindow();
    }

    return;
  }


  if (
    clockDemoState ==
    CLOCK_EYES_OPENING
  ) {
    float t =
      (float)(
        now - clockStageStart
      )
      /
      (float)CLOCK_EYE_OPEN_MS;

    float eased =
      smoothStep(t);

    drawEyesWithClosure(
      1.0f - eased
    );

    return;
  }
}


// ------------------------------------------------------------
// DRAW EMI
// ------------------------------------------------------------

void drawEmi() {
  display.clearBuffer();

  if (
    clockDemoState !=
    CLOCK_DEMO_OFF
  ) {
    drawClockDemo();
  }
  else {
    drawEyesWithClosure(
      0.0f
    );

    drawMiniTimer();
  }

  display.sendBuffer();
}


// ------------------------------------------------------------
// CLOCK DEMO STATE MACHINE
// ------------------------------------------------------------

void startClockDemo() {
  clockDemoState =
    CLOCK_EYES_CLOSING;

  clockStageStart =
    millis();

  idleFollowup =
    FOLLOWUP_NONE;

  attentionState =
    ATTENTION_NONE;

  gazeMoving =
    false;

  gazeX = 0;
  gazeY = 0;
}


void cancelClockDemoForInteraction() {
  if (
    clockDemoState ==
    CLOCK_DEMO_OFF
  ) {
    return;
  }

  clockDemoState =
    CLOCK_DEMO_OFF;

  clockDemoDone =
    true;
}


void updateClockDemo() {
  unsigned long now =
    millis();

  if (
    !clockDemoDone &&
    clockDemoState ==
      CLOCK_DEMO_OFF &&
    !petting &&
    now - clockDemoStart >=
      CLOCK_DEMO_TRIGGER_MS
  ) {
    startClockDemo();

    return;
  }


  if (
    clockDemoState ==
    CLOCK_DEMO_OFF
  ) {
    return;
  }


  unsigned long elapsed =
    now - clockStageStart;


  if (
    clockDemoState ==
      CLOCK_EYES_CLOSING &&
    elapsed >=
      CLOCK_EYE_CLOSE_MS
  ) {
    clockDemoState =
      CLOCK_REVEALING;

    clockStageStart =
      now;

    return;
  }


  if (
    clockDemoState ==
      CLOCK_REVEALING &&
    elapsed >=
      CLOCK_REVEAL_MS
  ) {
    clockDemoState =
      CLOCK_HOLDING;

    clockStageStart =
      now;

    return;
  }


  if (
    clockDemoState ==
      CLOCK_HOLDING &&
    elapsed >=
      CLOCK_HOLD_MS
  ) {
    clockDemoState =
      CLOCK_HIDING;

    clockStageStart =
      now;

    return;
  }


  if (
    clockDemoState ==
      CLOCK_HIDING &&
    elapsed >=
      CLOCK_HIDE_MS
  ) {
    clockDemoState =
      CLOCK_EYES_OPENING;

    clockStageStart =
      now;

    return;
  }


  if (
    clockDemoState ==
      CLOCK_EYES_OPENING &&
    elapsed >=
      CLOCK_EYE_OPEN_MS
  ) {
    clockDemoState =
      CLOCK_DEMO_OFF;

    clockDemoDone =
      true;

    nextIdleAction =
      now + 1500;

    scheduleNextBlink();
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
  gazeStartX = gazeX;
  gazeStartY = gazeY;

  gazeTargetX = targetX;
  gazeTargetY = targetY;

  gazeStartTime = millis();
  gazeDuration = duration;

  gazeMoving = true;
}


void updateGaze() {
  if (
    !gazeMoving ||
    clockDemoState != CLOCK_DEMO_OFF
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
  if (
    blinking ||
    clockDemoState != CLOCK_DEMO_OFF
  ) {
    return;
  }

  blinking = true;

  blinkCloseTime = 65;
  blinkHoldTime = 18;
  blinkOpenTime = 95;

  blinkStartTime =
    millis();

  if (
    random(100) < 2
  ) {
    doubleBlinkPending = true;

    secondBlinkTime =
      millis() + 350;
  }
}


void startAttentionBlink() {
  if (
    blinking ||
    clockDemoState != CLOCK_DEMO_OFF
  ) {
    return;
  }

  blinking = true;

  blinkCloseTime = 80;
  blinkHoldTime = 28;
  blinkOpenTime = 115;

  blinkStartTime =
    millis();
}


void startRelaxedBlink() {
  if (
    blinking ||
    clockDemoState != CLOCK_DEMO_OFF
  ) {
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
  nextBlinkTime =
    millis()
    + random(
        9000,
        18000
      );
}


// ------------------------------------------------------------
// ATTENTION BID
// ------------------------------------------------------------

void scheduleNextAttentionBid() {
  nextAttentionBid =
    millis()
    + random(
        35000,
        70000
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
    clockDemoState != CLOCK_DEMO_OFF
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
      now >= nextAttentionBid &&
      now - lastInteractionTime >
        30000
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
      now - attentionStageTime >
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
      now - attentionStageTime >
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
      now - attentionStageTime >
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
  int weights[IDLE_ACTION_COUNT] = {
    22,
    17,
    18,
    10,
    8,
    12,
    13
  };

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

    curiosity -= 0.035f;

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

    curiosity -= 0.08f;

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
  cancelClockDemoForInteraction();

  petting = true;

  patCount = 0;

  relaxedBlinkPending = false;
  relaxedBlinkDone = false;

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

  lastInteractionTime =
    now;

  patCount++;

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

  // Noticeably gentler than the previous version.
  petStrokeMaxClosure =
    0.22f +
    min(
      patCount,
      5
    )
    * 0.012f +
    contentment * 0.018f;

  petStrokeMaxClosure =
    clampFloat(
      petStrokeMaxClosure,
      0.24f,
      0.32f
    );

  petStrokeActive = true;

  petStrokeStart =
    now;

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

  float returnY =
    -2.0f -
    contentment * 2.0f;

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
  if (
    petting ||
    clockDemoState != CLOCK_DEMO_OFF ||
    attentionState != ATTENTION_NONE
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

  testTimerStart =
    millis();

  clockDemoStart =
    millis();

  lastDriveUpdate =
    millis();

  lastInteractionTime =
    millis();

  nextIdleAction =
    millis() + 1800;

  scheduleNextBlink();

  scheduleNextAttentionBid();

  drawEmi();
}


// ------------------------------------------------------------
// LOOP
// ------------------------------------------------------------

void loop() {
  updatePersonalityDrives();

  updateTouch();

  updateClockDemo();

  updateGaze();

  updateAttentionBid();

  updateIdleBehaviour();

  updatePettingBehaviour();

  drawEmi();

  delay(20);
}
