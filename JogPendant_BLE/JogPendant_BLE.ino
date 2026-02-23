#include <BleKeyboard.h>

/*
  CNCjs Pendant BLE HID (ESP32 / LOLIN D32)

  Features:
  - Robust quadrature decoding (detents) + direction hysteresis
  - Step modes: 1 -> no modifier, 10 -> Alt, 100 -> Ctrl
  - Smooth jog trigger (Option A):
      * Step=10  + fast spin => send Shift+Alt+Arrow (medium smooth)
      * Step=100 + fast spin => send Shift+Arrow     (high smooth)
  - Axis4 "probe trigger": Axis4 selected + 3 inferred-enable clicks within 2s => sends Alt+P
  - E-STOP handling:
      * While E-STOP is pressed: blink LED, block ALL actions (no jog, no probe, no BLE keys)
      * On transition to pressed: stop smooth jog by toggling it off (Shift+Arrow) if BLE connected
        (then we stay blocked until release)

  Notes:
  - There is no dedicated enable input; enable is inferred: (axis selected) AND (step selected).
  - Inputs are assumed active-LOW with internal pullups.
*/

BleKeyboard bleKeyboard("CNC Jog Pendant", "NOV", 100);

// Some BLE keyboard stacks expose F1-F12 only. Provide F13-F18 if missing.
#ifndef KEY_F13
#define KEY_F13 0xF0
#define KEY_F14 0xF1
#define KEY_F15 0xF2
#define KEY_F16 0xF3
#define KEY_F17 0xF4
#define KEY_F18 0xF5
#endif

// ---- PINS ----
#define ENC_A   32
#define ENC_B   22

#define AXIS_X  25
#define AXIS_Y  27
#define AXIS_Z  14
#define AXIS_4  17   // Axis4 selector line (active LOW)

#define STEP_1    12
#define STEP_10   13
#define STEP_100  15

#define ESTOP   4
#define LED_PIN 16

// ---- TUNING ----
const unsigned long ENC_DEBOUNCE_US = 1500;
const unsigned long FAST_DETECT_US  = 20000UL; // user tuning
const unsigned long FAST_RELEASE_MS = 200;
const unsigned long REST_TIMEOUT_MS = 200;
const unsigned long REVERSAL_WINDOW_MS = 120;
const int DETENT_TRANSITIONS = 4;

// ---- PROBE TRIGGER ----
const unsigned long PROBE_CLICK_WINDOW_MS = 2000;
const int PROBE_REQUIRED_CLICKS = 3;
const unsigned long PROBE_MIN_CLICK_SPACING_MS = 80;

// ---- E-STOP ----
#define ESTOP_PRESSED_LEVEL HIGH          // adjust to your wiring (HIGH if pin goes HIGH when pressed)
const unsigned long ESTOP_DEBOUNCE_MS = 30;
const unsigned long ESTOP_BLINK_MS = 250; // toggle every 250ms => ~2Hz blink

// ---- STATE ----
int lastABstate = 0;
int transition_acc = 0;
unsigned long lastTransitionUs = 0;

unsigned long lastDetentMs = 0;
int lastEmittedDir = 0;
int consecutiveOpposite = 0;
unsigned long lastOppositeMs = 0;

// Smooth jog toggle tracking
bool smoothActive = false;
uint8_t smoothArrowKey = 0;
bool smoothUsedAlt = false; // true when smooth started from Step=10 (Shift+Alt+Arrow)
unsigned long lastFastActivityMs = 0;

// Probe click tracking
bool lastEnRaw = false;
int probeClickCount = 0;
unsigned long probeFirstClickMs = 0;
unsigned long probeLastClickMs = 0;

// E-stop debouncing
int lastEstopRead = -1;
int stableEstop = -1;
unsigned long estopLastChangeMs = 0;
bool estopWasPressed = false;
unsigned long estopBlinkLastMs = 0;
bool estopLedState = false;

// ---- Read selectors (active LOW) ----
char readAxisRaw() {
  if (digitalRead(AXIS_X) == LOW) return 'X';
  if (digitalRead(AXIS_Y) == LOW) return 'Y';
  if (digitalRead(AXIS_Z) == LOW) return 'Z';
  if (digitalRead(AXIS_4) == LOW) return '4';
  return 0;
}
int readStepModeRaw() {
  if (digitalRead(STEP_1) == LOW)   return 1;
  if (digitalRead(STEP_10) == LOW)  return 10;
  if (digitalRead(STEP_100) == LOW) return 100;
  return 0;
}

// ---- Axis -> key mapping ----
void mapArrowKeys(char axis, bool positive, uint8_t &keyOut) {
  if (axis == 'X') keyOut = positive ? KEY_F14 : KEY_F13;  // X+ / X-
  else if (axis == 'Y') keyOut = positive ? KEY_F15 : KEY_F16; // Y+ / Y-
  else if (axis == 'Z') keyOut = positive ? KEY_F17 : KEY_F18; // Z+ / Z-
  else keyOut = 0;
}

// ---- Tap modifiers ----
void modsForTap(int stepMode, bool &useCtrl, bool &useAlt, bool &useShift) {
  useCtrl = false; useAlt = false; useShift = false;
  if (stepMode == 10) useAlt = true;
  if (stepMode == 100) useCtrl = true;
}

// ---- BLE helpers ----
void tapWithMods(uint8_t key, bool useCtrl, bool useAlt, bool useShift) {
  if (!bleKeyboard.isConnected()) return;

  if (useCtrl)  bleKeyboard.press(KEY_LEFT_CTRL);
  if (useAlt)   bleKeyboard.press(KEY_LEFT_ALT);
  if (useShift) bleKeyboard.press(KEY_LEFT_SHIFT);

  bleKeyboard.press(key);
  delay(2);
  bleKeyboard.release(key);

  if (useCtrl)  bleKeyboard.release(KEY_LEFT_CTRL);
  if (useAlt)   bleKeyboard.release(KEY_LEFT_ALT);
  if (useShift) bleKeyboard.release(KEY_LEFT_SHIFT);
}

void sendSmoothToggle(uint8_t arrowKey) {
  // Shift + Arrow
  tapWithMods(arrowKey, false, false, true);
}

void stopSmoothIfActive(const char* reason) {
  if (!smoothActive) return;
  Serial.print("SMOOTH STOP ("); Serial.print(reason); Serial.println(")");
  if (bleKeyboard.isConnected() && smoothArrowKey != 0) {
    // toggle off in the node pendant (StartStopSmoothJog)
    if (smoothUsedAlt) {
      tapWithMods(smoothArrowKey, false, true, true); // Shift+Alt+Arrow
    } else {
      sendSmoothToggle(smoothArrowKey); // Shift+Arrow
    }
  }
  smoothActive = false;
  smoothArrowKey = 0;
  smoothUsedAlt = false;
}

void sendProbeTrigger() {
  if (!bleKeyboard.isConnected()) {
    Serial.println("PROBE trigger (Alt+P) not sent: BLE not connected");
    return;
  }
  Serial.println("PROBE TRIGGER: Alt+P");
  bleKeyboard.press(KEY_LEFT_ALT);
  bleKeyboard.press('p');
  delay(5);
  bleKeyboard.release('p');
  bleKeyboard.release(KEY_LEFT_ALT);
}

// ---- E-stop debounced ----
int readEstopDebounced() {
  int v = digitalRead(ESTOP);
  unsigned long now = millis();
  if (lastEstopRead == -1) {
    lastEstopRead = v; stableEstop = v; estopLastChangeMs = now; return stableEstop;
  }
  if (v != lastEstopRead) { lastEstopRead = v; estopLastChangeMs = now; }
  if ((now - estopLastChangeMs) > ESTOP_DEBOUNCE_MS) stableEstop = lastEstopRead;
  return stableEstop;
}

// ---- Quadrature lookup ----
const int8_t quad_table[16] = {
  0,  1, -1,  0,
 -1,  0,  0,  1,
  1,  0,  0, -1,
  0, -1,  1,  0
};

// ---- Emit detent ----
void emitDetent(int detentDir, char axis, int stepMode) {
  unsigned long nowMs = millis();

  // Direction hysteresis (ignore single opposite detent shortly after movement)
  if (lastEmittedDir != 0 && detentDir != lastEmittedDir) {
    unsigned long sinceLast = nowMs - lastDetentMs;
    if (sinceLast > REST_TIMEOUT_MS) {
      lastEmittedDir = detentDir;
      consecutiveOpposite = 0;
    } else {
      if (consecutiveOpposite == 0 || (nowMs - lastOppositeMs) > REVERSAL_WINDOW_MS) consecutiveOpposite = 1;
      else consecutiveOpposite++;
      lastOppositeMs = nowMs;
      if (consecutiveOpposite < 2) return;
      lastEmittedDir = detentDir;
      consecutiveOpposite = 0;
    }
  } else {
    lastEmittedDir = detentDir;
    consecutiveOpposite = 0;
  }
  lastDetentMs = nowMs;

  if (axis == '4') return;          // no jog in Axis4
  if (!axis || !stepMode) return;

  bool positive = (detentDir == 1);
  uint8_t arrowKey;
  mapArrowKeys(axis, positive, arrowKey);
  if (!arrowKey) return;

  // Speed between detents for smooth trigger
  static unsigned long prevDetentMicros = 0;
  unsigned long nowMicros = micros();
  unsigned long dt_us = (prevDetentMicros == 0) ? 0 : (nowMicros - prevDetentMicros);
  prevDetentMicros = nowMicros;
  bool fastSpin = (dt_us > 0 && dt_us < FAST_DETECT_US);

  bool allowSmooth = (stepMode == 100 || stepMode == 10);

  if (allowSmooth && fastSpin) {
    lastFastActivityMs = millis();
    if (!smoothActive) {
      // Start smooth jog:
      //   Step=10  -> Shift+Alt+Arrow (medium speed in node pendant)
      //   Step=100 -> Shift+Arrow     (high speed in node pendant)
      if (stepMode == 10) {
        tapWithMods(arrowKey, false, true, true); // Shift+Alt+Arrow
        smoothUsedAlt = true;
        Serial.println("SMOOTH START (Shift+Alt+Arrow)");
      } else {
        sendSmoothToggle(arrowKey); // Shift+Arrow
        smoothUsedAlt = false;
        Serial.println("SMOOTH START (Shift+Arrow)");
      }
      smoothActive = true;
      smoothArrowKey = arrowKey;
    }
  } else {
    if (smoothActive) stopSmoothIfActive("tap");
    bool useCtrl, useAlt, useShift;
    modsForTap(stepMode, useCtrl, useAlt, useShift);
    tapWithMods(arrowKey, useCtrl, useAlt, useShift);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);

  pinMode(AXIS_X, INPUT_PULLUP);
  pinMode(AXIS_Y, INPUT_PULLUP);
  pinMode(AXIS_Z, INPUT_PULLUP);
  pinMode(AXIS_4, INPUT_PULLUP);

  pinMode(STEP_1, INPUT_PULLUP);
  pinMode(STEP_10, INPUT_PULLUP);
  pinMode(STEP_100, INPUT_PULLUP);

  pinMode(ESTOP, INPUT_PULLUP);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  int a = digitalRead(ENC_A) ? 1 : 0;
  int b = digitalRead(ENC_B) ? 1 : 0;
  lastABstate = (a<<1) | b;
  lastTransitionUs = micros();

  bleKeyboard.begin();

  Serial.println("Pendant: robust + Axis4 probe trigger (Alt+P) + ESTOP lockout");
}

void loop() {
  // --- Read E-stop first ---
  int est = readEstopDebounced();
  bool estopPressed = (est == ESTOP_PRESSED_LEVEL);

  // Edge logs + stop smooth on press
  if (estopPressed && !estopWasPressed) {
    estopWasPressed = true;
    Serial.println("ESTOP pressed -> LOCKOUT");
    stopSmoothIfActive("ESTOP pressed");
    probeClickCount = 0;
    probeFirstClickMs = 0;
    probeLastClickMs = 0;
  } else if (!estopPressed && estopWasPressed) {
    estopWasPressed = false;
    Serial.println("ESTOP released -> normal");
    estopLedState = false;
    digitalWrite(LED_PIN, LOW);
  }

  // If E-stop is pressed: blink LED and block all actions
  if (estopPressed) {
    unsigned long now = millis();
    if (now - estopBlinkLastMs >= ESTOP_BLINK_MS) {
      estopBlinkLastMs = now;
      estopLedState = !estopLedState;
      digitalWrite(LED_PIN, estopLedState ? HIGH : LOW);
    }
    // Keep encoder state updated so we don't "jump" when releasing E-stop
    int a = digitalRead(ENC_A) ? 1 : 0;
    int b = digitalRead(ENC_B) ? 1 : 0;
    lastABstate = (a<<1) | b;
    delay(1);
    return;
  }

  // --- Normal mode (E-stop not pressed) ---
  char axisNow = readAxisRaw();
  int stepNow = readStepModeRaw();
  bool enRaw = (axisNow != 0) && (stepNow != 0);

  // LED indicates inferred enable in normal mode
  digitalWrite(LED_PIN, enRaw ? HIGH : LOW);

  bool rising = (!lastEnRaw && enRaw);
  bool falling = (lastEnRaw && !enRaw);
  lastEnRaw = enRaw;

  // Status print
  static unsigned long lastStatus = 0;
  if (millis() - lastStatus > 1000) {
    lastStatus = millis();
    Serial.print("Enable="); Serial.print(enRaw ? "ON" : "OFF");
    Serial.print(" Axis=");  Serial.print(axisNow ? axisNow : '-');
    Serial.print(" Step=");  Serial.print(stepNow);
    Serial.print(" BLE=");   Serial.println(bleKeyboard.isConnected() ? "Connected" : "Not connected");
  }

  // Clear probe click counting if we leave Axis4 while enabled
  if (enRaw && axisNow && axisNow != '4') {
    if (probeClickCount != 0) Serial.println("Probe click counter cleared (left Axis4)");
    probeClickCount = 0;
    probeFirstClickMs = 0;
    probeLastClickMs = 0;
  }

  // Probe trigger: count enable rising edges while Axis4 selected
  if (rising && axisNow == '4') {
    unsigned long nowMs = millis();
    if (probeLastClickMs != 0 && (nowMs - probeLastClickMs) < PROBE_MIN_CLICK_SPACING_MS) {
      // ignore bounce
    } else {
      if (probeClickCount == 0 || (nowMs - probeFirstClickMs) > PROBE_CLICK_WINDOW_MS) {
        probeFirstClickMs = nowMs;
        probeClickCount = 1;
      } else {
        probeClickCount++;
      }
      probeLastClickMs = nowMs;

      Serial.print("Axis4 click count="); Serial.println(probeClickCount);

      if (probeClickCount >= PROBE_REQUIRED_CLICKS) {
        sendProbeTrigger();
        probeClickCount = 0;
        probeFirstClickMs = 0;
        probeLastClickMs = 0;
      }
    }
  }

  // If enable goes OFF: stop smooth and send '!' (feed hold)
  if (falling) {
    stopSmoothIfActive("enable OFF");
    if (bleKeyboard.isConnected()) {
      bleKeyboard.write('!');
      Serial.println("Sent '!' (feed hold) due to enable OFF");
    }
  }

  // If Axis4 selected while enabled: ensure smooth isn't active
  if (enRaw && axisNow == '4') {
    if (smoothActive) stopSmoothIfActive("Axis4 selected");
  }

  // Quadrature (only when enabled and not Axis4)
  if (enRaw && axisNow != '4') {
    int a = digitalRead(ENC_A) ? 1 : 0;
    int b = digitalRead(ENC_B) ? 1 : 0;
    int curr = (a<<1) | b;
    int idx = (lastABstate<<2) | curr;
    int8_t delta = quad_table[idx];

    unsigned long nowUs = micros();
    if (delta != 0 && (nowUs - lastTransitionUs) > ENC_DEBOUNCE_US) {
      transition_acc += delta;
      lastTransitionUs = nowUs;

      while (transition_acc >= DETENT_TRANSITIONS) {
        emitDetent(+1, axisNow, stepNow);
        transition_acc -= DETENT_TRANSITIONS;
      }
      while (transition_acc <= -DETENT_TRANSITIONS) {
        emitDetent(-1, axisNow, stepNow);
        transition_acc += DETENT_TRANSITIONS;
      }
    }
    lastABstate = curr;
  } else {
    int a = digitalRead(ENC_A) ? 1 : 0;
    int b = digitalRead(ENC_B) ? 1 : 0;
    lastABstate = (a<<1) | b;
  }

  // Smooth timeout stop
  if (smoothActive) {
    if ((millis() - lastFastActivityMs) > FAST_RELEASE_MS) {
      stopSmoothIfActive("timeout");
    }
  }

  delay(1);
}
