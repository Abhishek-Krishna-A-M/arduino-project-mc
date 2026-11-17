// ============================================================================
// SMART ENERGY MANAGEMENT SYSTEM (revised)
// Hardware: Arduino Nano, FC-51 IR, HLK-LD2410B Radar, LDR module, Relay, Buzzer
// ============================================================================

// ---------------- Pin Wiring Mappings ----------------
const int PIN_IR         = 2;  // FC-51 IR OUT (Active LOW)
const int PIN_RADAR      = 4;  // LD2410 OUT (Active HIGH)
const int PIN_OVERRIDE   = 5;  // Push switch (D5 to GND)
const int PIN_RELAY      = 6;  // Relay module IN
const int PIN_BUZZER     = 7;  // Buzzer I/O
const int PIN_LED_READY  = 8;  // Auto mode LED
const int PIN_LED_MANUAL = 11;  // Manual override LED
const int PIN_LED_POWER  = 10; // Load power LED
const int PIN_LDR        = A0; // LDR module analog OUT

// ---------------- HARDWARE SETTINGS: CHANGE TO MATCH YOUR MODULES ----------------
const bool RELAY_ACTIVE_LOW  = true; // true if relay switches ON when IN is LOW (most blue modules)
const bool BUZZER_ACTIVE_LOW = false; // true for "low level trigger" buzzer modules
const bool BUZZER_PASSIVE    = true; // true if buzzer only sounds with a tone (passive type)
const int  BUZZER_FREQ       = 2500;  // Hz, only used for passive buzzers
const bool LDR_DARK_IS_HIGH  = false;  // true if the analog reading goes UP in the dark
const bool DEBUG_PRINT       = true;  // prints sensor values once per second

const int RELAY_ON  = RELAY_ACTIVE_LOW ? LOW : HIGH;
const int RELAY_OFF = RELAY_ACTIVE_LOW ? HIGH : LOW;

// ---------------- Thresholds & Timers ----------------
const int LDR_THRESHOLD           = 400;   // Calibrate using the DEBUG output
const unsigned long TIMEOUT_OFF   = 5000;  // Vacancy countdown (5s for testing; use minutes in real use)
const unsigned long BEEP_DUR      = 2000;  // Warning beep length at end of countdown
const unsigned long ENTRY_GRACE   = 10000; // After IR trigger, assume occupied this long so radar can lock on

// ---------------- System State ----------------
bool manualOverride = false;
bool relayState = false;
bool buzzerState = false;

bool prevIRState = HIGH;
bool prevOverrideState = HIGH;

bool irGraceActive = false;
unsigned long irTriggerTime = 0;

unsigned long vacantStartTime = 0;
bool inVacantCountdown = false;

void setup() {
  Serial.begin(115200);

  pinMode(PIN_IR, INPUT_PULLUP);
  pinMode(PIN_OVERRIDE, INPUT_PULLUP);
  pinMode(PIN_RADAR, INPUT);

  // Write the safe level BEFORE switching to OUTPUT to avoid a glitch at boot
  digitalWrite(PIN_RELAY, RELAY_OFF);
  pinMode(PIN_RELAY, OUTPUT);

  digitalWrite(PIN_BUZZER, BUZZER_ACTIVE_LOW ? HIGH : LOW);
  pinMode(PIN_BUZZER, OUTPUT);

  pinMode(PIN_LED_READY, OUTPUT);
  pinMode(PIN_LED_MANUAL, OUTPUT);
  pinMode(PIN_LED_POWER, OUTPUT);

  digitalWrite(PIN_LED_READY, HIGH);
  digitalWrite(PIN_LED_MANUAL, LOW);
  digitalWrite(PIN_LED_POWER, LOW);

  Serial.println(F("=========================================="));
  Serial.println(F(" SYSTEM INITIALIZED - AUTO MODE ACTIVE    "));
  Serial.println(F("=========================================="));
}

void loop() {
  unsigned long now = millis();

  // 1. Read sensors
  bool currIR = digitalRead(PIN_IR);
  bool currOverride = digitalRead(PIN_OVERRIDE);
  //bool targetPresent = false;
  bool targetPresent = digitalRead(PIN_RADAR);
  int ambientLight = analogRead(PIN_LDR);

  // Debug output (non-blocking, once per second)
  static unsigned long lastDebug = 0;
  if (DEBUG_PRINT && now - lastDebug >= 1000) {
    lastDebug = now;
    Serial.print(F("[DBG] IR="));    Serial.print(currIR);
    Serial.print(F(" RADAR="));      Serial.print(targetPresent);
    Serial.print(F(" LDR="));        Serial.print(ambientLight);
    Serial.print(F(" RELAY="));      Serial.println(relayState);
  }

  // 2. Manual override (200ms debounce)
  static unsigned long lastOverrideTime = 0;
  if (prevOverrideState == HIGH && currOverride == LOW) {
    if (now - lastOverrideTime > 200) {
      manualOverride = !manualOverride;

      digitalWrite(PIN_LED_MANUAL, manualOverride ? HIGH : LOW);
      digitalWrite(PIN_LED_READY, manualOverride ? LOW : HIGH);

      Serial.print(F("[MODE] Switched to: "));
      Serial.println(manualOverride ? F("MANUAL OVERRIDE") : F("AUTO"));

      if (manualOverride) {
        setRelay(true);
        inVacantCountdown = false;
        setBuzzer(false);
      } else {
        irGraceActive = false; // start auto mode clean
      }
      lastOverrideTime = now;
    }
  }
  prevOverrideState = currOverride;

  // Manual mode: skip automatic logic, but keep IR edge state current
  if (manualOverride) {
    prevIRState = currIR;
    return;
  }

  // 3. IR trigger (falling edge) opens a timed entry-grace window
  if (prevIRState == HIGH && currIR == LOW) {
    irGraceActive = true;
    irTriggerTime = now;
    Serial.println(F("[TRIGGER] IR triggered. Entry grace window started."));
  }
  prevIRState = currIR;

  if (irGraceActive && (now - irTriggerTime >= ENTRY_GRACE)) {
    irGraceActive = false;
  }

  // 4. Sensor fusion: IR grace window OR radar presence
  bool occupancyDetected = irGraceActive || targetPresent;

  if (occupancyDetected) {
    inVacantCountdown = false;
    setBuzzer(false);

    bool isDark = LDR_DARK_IS_HIGH ? (ambientLight > LDR_THRESHOLD)
                                   : (ambientLight < LDR_THRESHOLD);
    if (!relayState && !isDark) {
      static unsigned long lastWhy = 0;
      if (now - lastWhy > 3000) {
        lastWhy = now;
        Serial.println(F("[AUTO] Occupied but room is bright - relay stays OFF"));
      }
    }
    setRelay(isDark || relayState);
  } else {
    if (relayState && !inVacantCountdown) {
      inVacantCountdown = true;
      vacantStartTime = now;
      Serial.println(F("[TIMER] Room vacant. Shutdown countdown started."));
    }

    if (inVacantCountdown) {
      unsigned long elapsed = now - vacantStartTime;

      setBuzzer(elapsed >= (TIMEOUT_OFF - BEEP_DUR) && elapsed < TIMEOUT_OFF);

      if (elapsed >= TIMEOUT_OFF) {
        setRelay(false);
        irGraceActive = false;
        inVacantCountdown = false;
        setBuzzer(false);
        Serial.println(F("[POWER] Shutdown complete. Relay OFF."));
      }
    }
  }
}

// ----------------------------------------------------------------------------
// Relay helper (handles active-low or active-high modules)
// ----------------------------------------------------------------------------
void setRelay(bool state) {
  if (relayState != state) {
    relayState = state;
    digitalWrite(PIN_RELAY, state ? RELAY_ON : RELAY_OFF);
    digitalWrite(PIN_LED_POWER, state ? HIGH : LOW);

    Serial.print(F("[OUTPUT] Relay changed to: "));
    Serial.println(state ? F("ON") : F("OFF"));
  }
}

// ----------------------------------------------------------------------------
// Buzzer helper (handles active/passive and active-low/high types)
// Only acts on state changes so tone() isn't restarted every loop.
// ----------------------------------------------------------------------------
void setBuzzer(bool on) {
  if (on == buzzerState) return;
  buzzerState = on;

  if (BUZZER_PASSIVE) {
    if (on) {
      tone(PIN_BUZZER, BUZZER_FREQ);
    } else {
      noTone(PIN_BUZZER);
      digitalWrite(PIN_BUZZER, BUZZER_ACTIVE_LOW ? HIGH : LOW); // idle level
    }
  } else {
    bool level = BUZZER_ACTIVE_LOW ? !on : on;
    digitalWrite(PIN_BUZZER, level ? HIGH : LOW);
  }
}
