/*
 * Splitflap Display Controller
 *
 * Controls a 28BYJ-48 stepper motor with ULN2003 driver to flip cards
 * until desired character is reached.
 * Uses a magnetic hall-effect sensor on pin D4 for home position detection.
 * Home position (sensor LOW) corresponds to character position 2.
 *
 * Reference: https://github.com/scottbez1/splitflap
 */

// Pin Definitions
#define HALL_SENSOR_PIN   4    // Magnetic sensor - reads LOW at home position

// 28BYJ-48 with ULN2003 driver - 4 control pins
#define MOTOR_PIN_1       6    // IN1 on ULN2003
#define MOTOR_PIN_2       7    // IN2 on ULN2003
#define MOTOR_PIN_3       8    // IN3 on ULN2003
#define MOTOR_PIN_4       9    // IN4 on ULN2003

// Configuration
// 28BYJ-48: 2048 steps per revolution (half-step mode)
// Adjust STEPS_PER_FLAP based on your gear ratio to the flap drum
#define STEPS_PER_REVOLUTION  2048
#define STEPS_PER_FLAP    51   // Steps needed to advance one flap (2048/40 flaps ~ 51)
#define NUM_FLAPS         40   // Total number of flaps/characters on the drum
#define HOME_POSITION     2    // Character position when hall sensor reads LOW
#define STEP_DELAY_MS     2    // Milliseconds between steps (controls speed, min ~2ms for 28BYJ-48)

// Character set - customize to match your flap order
const char CHARACTERS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-?!";
const int NUM_CHARACTERS = sizeof(CHARACTERS) - 1;

// Half-step sequence for 28BYJ-48 (8 steps)
const int STEP_SEQUENCE[8][4] = {
  {1, 0, 0, 0},
  {1, 1, 0, 0},
  {0, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 0},
  {0, 0, 1, 1},
  {0, 0, 0, 1},
  {1, 0, 0, 1}
};

// State variables
int currentPosition = -1;  // Current flap position (-1 = unknown/not homed)
bool isHomed = false;
int stepIndex = 0;         // Current position in step sequence

void setup() {
  Serial.begin(115200);

  // Configure hall sensor pin
  pinMode(HALL_SENSOR_PIN, INPUT_PULLUP);

  // Configure motor pins
  pinMode(MOTOR_PIN_1, OUTPUT);
  pinMode(MOTOR_PIN_2, OUTPUT);
  pinMode(MOTOR_PIN_3, OUTPUT);
  pinMode(MOTOR_PIN_4, OUTPUT);

  // Turn off all motor coils initially
  motorOff();

  Serial.println("Splitflap Controller Ready");
  Serial.println("Motor: 28BYJ-48 with ULN2003");
  Serial.println("Commands:");
  Serial.println("  H - Home the display");
  Serial.println("  G<char> - Go to character (e.g., GA for 'A')");
  Serial.println("  P<num> - Go to position number (e.g., P5)");
  Serial.println("  S - Show current status");
  Serial.println("  T - Test motor (one full revolution)");
  Serial.println();

  // Auto-home on startup
  Serial.println("Auto-homing...");
  homeDisplay();
}

void loop() {
  if (Serial.available()) {
    char cmd = Serial.read();

    switch (cmd) {
      case 'H':
      case 'h':
        homeDisplay();
        break;

      case 'G':
      case 'g':
        // Wait for character
        while (!Serial.available()) delay(1);
        {
          char targetChar = Serial.read();
          goToCharacter(targetChar);
        }
        break;

      case 'P':
      case 'p':
        // Read position number
        {
          int pos = Serial.parseInt();
          goToPosition(pos);
        }
        break;

      case 'S':
      case 's':
        showStatus();
        break;

      case 'T':
      case 't':
        testMotor();
        break;

      case '\n':
      case '\r':
        // Ignore newlines
        break;

      default:
        Serial.print("Unknown command: ");
        Serial.println(cmd);
        break;
    }
  }
}

/**
 * Read the hall effect sensor
 * Returns true if at home position (sensor reads LOW)
 */
bool isAtHome() {
  return digitalRead(HALL_SENSOR_PIN) == LOW;
}

/**
 * Turn off all motor coils (saves power, reduces heat)
 */
void motorOff() {
  digitalWrite(MOTOR_PIN_1, LOW);
  digitalWrite(MOTOR_PIN_2, LOW);
  digitalWrite(MOTOR_PIN_3, LOW);
  digitalWrite(MOTOR_PIN_4, LOW);
}

/**
 * Set motor coils to current step in sequence
 */
void setStep(int step) {
  digitalWrite(MOTOR_PIN_1, STEP_SEQUENCE[step][0]);
  digitalWrite(MOTOR_PIN_2, STEP_SEQUENCE[step][1]);
  digitalWrite(MOTOR_PIN_3, STEP_SEQUENCE[step][2]);
  digitalWrite(MOTOR_PIN_4, STEP_SEQUENCE[step][3]);
}

/**
 * Step the motor one step forward
 */
void stepMotorForward() {
  stepIndex = (stepIndex + 1) % 8;
  setStep(stepIndex);
  delay(STEP_DELAY_MS);
}

/**
 * Step the motor one step backward
 */
void stepMotorBackward() {
  stepIndex = (stepIndex - 1 + 8) % 8;
  setStep(stepIndex);
  delay(STEP_DELAY_MS);
}

/**
 * Move motor by specified number of steps
 * Positive = forward, Negative = backward
 */
void moveSteps(int steps) {
  if (steps > 0) {
    for (int i = 0; i < steps; i++) {
      stepMotorForward();
    }
  } else {
    for (int i = 0; i < -steps; i++) {
      stepMotorBackward();
    }
  }
}

/**
 * Advance the drum by one flap position
 */
void advanceOneFlap() {
  moveSteps(STEPS_PER_FLAP);

  // Update position tracking
  if (currentPosition >= 0) {
    currentPosition = (currentPosition + 1) % NUM_FLAPS;
  }
}

/**
 * Test motor - one full revolution
 */
void testMotor() {
  Serial.println("Testing motor - one full revolution...");
  moveSteps(STEPS_PER_REVOLUTION);
  motorOff();
  Serial.println("Test complete.");
}

/**
 * Home the display by finding the magnetic sensor position
 * The sensor reads LOW when we're at position 2
 */
void homeDisplay() {
  Serial.println("Homing display...");

  // If already at home, move away first
  if (isAtHome()) {
    Serial.println("Already at home sensor, moving away...");
    moveSteps(STEPS_PER_FLAP * 2);
  }

  // Search for home position (max 2 full rotations)
  int maxSteps = STEPS_PER_FLAP * NUM_FLAPS * 2;
  int stepCount = 0;
  bool found = false;

  Serial.println("Searching for home position...");

  while (stepCount < maxSteps) {
    if (isAtHome()) {
      found = true;
      break;
    }
    stepMotorForward();
    stepCount++;
  }

  if (found) {
    // Continue stepping until we exit the sensor zone
    int sensorSteps = 0;
    while (isAtHome() && sensorSteps < STEPS_PER_FLAP) {
      stepMotorForward();
      sensorSteps++;
    }

    // We're now at position HOME_POSITION (which is 2)
    currentPosition = HOME_POSITION;
    isHomed = true;

    Serial.print("Homed successfully! At position ");
    Serial.print(currentPosition);
    Serial.print(" (character '");
    Serial.print(CHARACTERS[currentPosition]);
    Serial.println("')");
  } else {
    Serial.println("ERROR: Could not find home position!");
    Serial.println("Check sensor connection on pin D4");
    isHomed = false;
    currentPosition = -1;
  }

  // Turn off motor coils when done
  motorOff();
}

/**
 * Move to a specific position number (0 to NUM_FLAPS-1)
 */
void goToPosition(int targetPosition) {
  if (!isHomed) {
    Serial.println("Error: Not homed! Run 'H' command first.");
    return;
  }

  if (targetPosition < 0 || targetPosition >= NUM_FLAPS) {
    Serial.print("Error: Position must be 0-");
    Serial.println(NUM_FLAPS - 1);
    return;
  }

  // Calculate flaps needed (always move forward)
  int flapsToMove = (targetPosition - currentPosition + NUM_FLAPS) % NUM_FLAPS;

  if (flapsToMove == 0) {
    Serial.println("Already at target position");
    return;
  }

  Serial.print("Moving from position ");
  Serial.print(currentPosition);
  Serial.print(" to ");
  Serial.print(targetPosition);
  Serial.print(" (");
  Serial.print(flapsToMove);
  Serial.println(" flaps)");

  // Move the required number of flaps
  for (int i = 0; i < flapsToMove; i++) {
    advanceOneFlap();

    // Check for home position crossing for re-sync
    if (isAtHome() && currentPosition != HOME_POSITION) {
      Serial.println("Warning: Detected home position, re-syncing...");
      currentPosition = HOME_POSITION;
      // Recalculate remaining flaps
      int remaining = (targetPosition - currentPosition + NUM_FLAPS) % NUM_FLAPS;
      flapsToMove = i + 1 + remaining;
    }

    delay(20);  // Small delay between flaps for smoother motion
  }

  // Turn off motor coils
  motorOff();

  currentPosition = targetPosition;
  Serial.print("Arrived at position ");
  Serial.print(currentPosition);
  Serial.print(" (character '");
  Serial.print(CHARACTERS[currentPosition]);
  Serial.println("')");
}

/**
 * Move to display a specific character
 */
void goToCharacter(char c) {
  // Convert to uppercase if letter
  if (c >= 'a' && c <= 'z') {
    c = c - 'a' + 'A';
  }

  // Find character in our set
  int targetPosition = -1;
  for (int i = 0; i < NUM_CHARACTERS; i++) {
    if (CHARACTERS[i] == c) {
      targetPosition = i;
      break;
    }
  }

  if (targetPosition < 0) {
    Serial.print("Error: Character '");
    Serial.print(c);
    Serial.println("' not found in character set");
    return;
  }

  Serial.print("Going to character '");
  Serial.print(c);
  Serial.print("' at position ");
  Serial.println(targetPosition);

  goToPosition(targetPosition);
}

/**
 * Show current status
 */
void showStatus() {
  Serial.println("=== Status ===");
  Serial.print("Motor: 28BYJ-48 (pins D6,D7,D8,D9)");
  Serial.println();
  Serial.print("Homed: ");
  Serial.println(isHomed ? "Yes" : "No");
  Serial.print("Current position: ");
  if (currentPosition >= 0) {
    Serial.print(currentPosition);
    Serial.print(" (character '");
    Serial.print(CHARACTERS[currentPosition]);
    Serial.println("')");
  } else {
    Serial.println("Unknown");
  }
  Serial.print("Hall sensor (D4): ");
  Serial.println(isAtHome() ? "HOME (LOW)" : "Not at home (HIGH)");
  Serial.print("Steps per flap: ");
  Serial.println(STEPS_PER_FLAP);
  Serial.print("Total characters: ");
  Serial.println(NUM_CHARACTERS);
  Serial.print("Character set: ");
  Serial.println(CHARACTERS);
  Serial.println("==============");
}
