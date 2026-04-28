// ============================================================
// ASRS - Automated Storage & Retrieval System
// Arduino Mega | 3x3 shelf | Stepper X-axis | Relay Y-axis
// ============================================================

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

// ============================================================
// SECTION 1: HARDWARE CONFIGURATION
// ============================================================

// --- LCD ---
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- Keypad ---
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {14, 15, 16, 17};
byte colPins[COLS] = {18, 19, 22, 23};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// --- Relay Pins (active-LOW: LOW = ON, HIGH = OFF) ---
const int RELAY_HORIZ     = 5;  // Horizontal push cylinder
const int RELAY_VERT_DOWN = 6;  // Vertical descent
const int RELAY_VERT_UP   = 7;  // Vertical ascent

// --- IR Level Sensors ---
const int IR_BOTTOM = 8;
const int IR_MIDDLE = 9;
const int IR_TOP    = 10;

// --- Stepper Motor (X-axis) ---
const int STEP_PIN = 2;
const int DIR_PIN  = 3;

// --- Ultrasonic Sensors: trigPins[row][col], echoPins[row][col] ---
const int trigPins[3][3] = {{30,32,34},{36,38,40},{42,44,46}};
const int echoPins[3][3] = {{31,33,35},{37,39,41},{43,45,47}};

// ============================================================
// SECTION 2: MOTION CONSTANTS
// ============================================================

// X-axis target positions in steps (col 0=left, 1=center, 2=right)
const long X_POS_LEFT   =  35000;
const long X_POS_CENTER =  0;
const long X_POS_RIGHT  = -35000;

// Stepper speed settings (microseconds delay)
const int STEP_DELAY_START = 2000;  // Slow start (acceleration)
const int STEP_DELAY_MIN   = 400;   // Max speed
const int STEP_DELAY_DECR  = 2;     // Acceleration rate

// Ultrasonic: distance threshold to consider box empty (cm)
const float EMPTY_THRESHOLD_CM = 10.0;

// ============================================================
// SECTION 3: SYSTEM STATE
// ============================================================

int  selectedRow = -1;
int  selectedCol = -1;
long currentPosition = 0;
bool isHomed = false;

// ============================================================
// SECTION 4: SETUP
// ============================================================

void setup() {
  delay(1000);
  Serial.begin(9600);

  // Init LCD
  Wire.setWireTimeout(3000, true);
  refreshScreen();
  lcd.setCursor(1, 0); lcd.print("WELCOME SIR");
  lcd.setCursor(1, 1); lcd.print("ASRS Smart Sys");
  delay(2000);

  // Relay pins
  pinMode(RELAY_HORIZ,     OUTPUT);
  pinMode(RELAY_VERT_UP,   OUTPUT);
  pinMode(RELAY_VERT_DOWN, OUTPUT);

  // IR sensor pins
  pinMode(IR_BOTTOM, INPUT);
  pinMode(IR_MIDDLE, INPUT);
  pinMode(IR_TOP,    INPUT);

  // Stepper pins
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN,  OUTPUT);

  // Ultrasonic pins
  for (int r = 0; r < 3; r++)
    for (int c = 0; c < 3; c++) {
      pinMode(trigPins[r][c], OUTPUT);
      pinMode(echoPins[r][c], INPUT);
    }

  stopAllRelays();
  showCalibrationMessage();
}

// ============================================================
// SECTION 5: MAIN LOOP
// ============================================================

void loop() {
  char key = keypad.getKey();

  if (key) {
    if (!isHomed) {
      handleHomingKeys(key);
    } else {
      handleOperationKeys(key);
    }
  }

  // Safety: stop descent relay if bottom IR triggered
  if (digitalRead(IR_BOTTOM) == LOW && digitalRead(RELAY_VERT_DOWN) == LOW) {
    digitalWrite(RELAY_VERT_DOWN, HIGH);
  }
}

// ============================================================
// SECTION 6: KEY HANDLERS
// ============================================================

void handleHomingKeys(char key) {
  if      (key == 'A') jogMotor(LOW,  500);  // Jog left
  else if (key == 'B') jogMotor(HIGH, 500);  // Jog right
  else if (key == 'C') {
    currentPosition = 0;
    isHomed = true;
    showIdleMessage();
  }
}

void handleOperationKeys(char key) {
  if (key >= '1' && key <= '9') {
    // Map key 1-9 to row/col on the 3x3 grid
    // Keys: 1=bottom-left ... 9=top-right
    int num = key - '1';
    selectedRow = 2 - (num / 3);  // Row 2 = top, 0 = bottom
    selectedCol = num % 3;

    refreshScreen();
    lcd.print("Box: "); lcd.print(key);
    lcd.setCursor(0, 1);
    lcd.print("Press # to START");
  }
  else if (key == '#') {
    if (selectedRow != -1) {
      runFullSequence();
      showIdleMessage();
    }
  }
  else if (key == 'D') {
    autoScanAndStore();
  }
}

// ============================================================
// SECTION 7: MAIN STORE SEQUENCE
// ============================================================

void runFullSequence() {
  refreshScreen();
  lcd.print("Status: Checking");

  if (!isBoxEmpty(selectedRow, selectedCol)) {
    refreshScreen();
    lcd.print("BOX FULL!");
    delay(3000);
    selectedRow = selectedCol = -1;
    return;
  }

  lcd.setCursor(0, 1);
  lcd.print("Box: EMPTY (OK) ");
  delay(1000);

  // Step 1: Ascend to target row (if not ground level)
  if (selectedRow > 0) {
    ascendToRow(selectedRow);
  }

  // Step 2: Move X-axis to target column
  moveToColumn(selectedCol);

  // Step 3: Push item into shelf
  pushItemIntoShelf();

  // Step 4: Return X-axis to center
  refreshScreen();
  lcd.print("Status: Return X");
  moveToPosition(X_POS_CENTER);

  // Step 5: Descend back to bottom (if was elevated)
  if (selectedRow > 0) {
    descendToBottom();
  }

  selectedRow = selectedCol = -1;
}

// ============================================================
// SECTION 8: AUTO SCAN & STORE
// ============================================================

void autoScanAndStore() {
  lcd.clear();
  lcd.print("Scanning...");
  bool found = false;

  for (int r = 0; r < 3 && !found; r++) {
    for (int c = 0; c < 3 && !found; c++) {
      if (isBoxEmpty(r, c)) {
        selectedRow = r;
        selectedCol = c;
        lcd.setCursor(0, 1);
        lcd.print("Target: R"); lcd.print(r);
        lcd.print(" C"); lcd.print(c);
        delay(1000);
        runFullSequence();
        found = true;
      }
    }
  }

  if (!found) {
    lcd.clear();
    lcd.print("SYSTEM FULL!");
    delay(3000);
  }

  showIdleMessage();
}

// ============================================================
// SECTION 9: MOVEMENT HELPERS
// ============================================================

void ascendToRow(int row) {
  refreshScreen();
  lcd.print("Status: Ascend");

  int targetIR = (row == 1) ? IR_MIDDLE : IR_TOP;
  digitalWrite(RELAY_VERT_UP, LOW);
  while (digitalRead(targetIR) == HIGH) { delay(1); }
  digitalWrite(RELAY_VERT_UP, HIGH);
  delay(1000);
}

void descendToBottom() {
  refreshScreen();
  lcd.print("Status: Return Y");

  digitalWrite(RELAY_VERT_DOWN, LOW);
  while (digitalRead(IR_BOTTOM) == HIGH) { delay(1); }
  digitalWrite(RELAY_VERT_DOWN, HIGH);
  delay(1000);
}

void moveToColumn(int col) {
  refreshScreen();
  lcd.print("Status: Moving X");

  long target;
  if      (col == 0) target = X_POS_LEFT;
  else if (col == 2) target = X_POS_RIGHT;
  else               target = X_POS_CENTER;

  moveToPosition(target);
}

void pushItemIntoShelf() {
  refreshScreen();
  lcd.print("Status: Pushing");

  digitalWrite(RELAY_HORIZ, LOW);
  delay(2000);
  digitalWrite(RELAY_HORIZ, HIGH);
  delay(1500);
}

// ============================================================
// SECTION 10: STEPPER MOTOR CONTROL
// ============================================================

void jogMotor(int direction, int steps) {
  digitalWrite(DIR_PIN, direction);
  for (int i = 0; i < steps; i++) {
    digitalWrite(STEP_PIN, HIGH); delayMicroseconds(600);
    digitalWrite(STEP_PIN, LOW);  delayMicroseconds(600);
  }
}

void moveToPosition(long targetPosition) {
  long stepsToMove = targetPosition - currentPosition;
  if (stepsToMove == 0) return;

  digitalWrite(DIR_PIN, (stepsToMove > 0) ? HIGH : LOW);
  executeMotion(abs(stepsToMove));
  currentPosition = targetPosition;
}

void executeMotion(long totalSteps) {
  int stepDelay = STEP_DELAY_START;

  for (long i = 0; i < totalSteps; i++) {
    digitalWrite(STEP_PIN, HIGH); delayMicroseconds(stepDelay);
    digitalWrite(STEP_PIN, LOW);  delayMicroseconds(stepDelay);

    // Accelerate gradually
    if (stepDelay > STEP_DELAY_MIN) stepDelay -= STEP_DELAY_DECR;
  }
}

// ============================================================
// SECTION 11: SENSOR HELPERS
// ============================================================

bool isBoxEmpty(int row, int col) {
  // Trigger ultrasonic pulse
  digitalWrite(trigPins[row][col], LOW);  delayMicroseconds(2);
  digitalWrite(trigPins[row][col], HIGH); delayMicroseconds(10);
  digitalWrite(trigPins[row][col], LOW);

  // Read echo (30ms timeout)
  long duration = pulseIn(echoPins[row][col], HIGH, 30000);
  delay(50);

  if (duration == 0) return true;  // No echo = nothing detected = empty

  float distanceCm = duration * 0.034 / 2.0;
  return (distanceCm > EMPTY_THRESHOLD_CM);
}

// ============================================================
// SECTION 12: LCD / UI HELPERS
// ============================================================

void refreshScreen() {
  lcd.init();
  lcd.backlight();
  lcd.clear();
}

void showCalibrationMessage() {
  refreshScreen();
  lcd.print("A< >B | C=Zero");
}

void showIdleMessage() {
  refreshScreen();
  lcd.print("Ready.. (1-9)");
  lcd.setCursor(0, 1);
  lcd.print("Auto Store = D");
}

void stopAllRelays() {
  digitalWrite(RELAY_HORIZ,     HIGH);
  digitalWrite(RELAY_VERT_UP,   HIGH);
  digitalWrite(RELAY_VERT_DOWN, HIGH);
}
