/*
 * Splitflap Display Controller
 *
 * Controls a stepper motor to flip cards until desired character is reached.
 * Uses a magnetic hall-effect sensor on pin D4 for home position detection.
 * Home position (sensor LOW) corresponds to character position 2.
 *
 * Reference: https://github.com/scottbez1/splitflap
 */

// Pin Definitions
#define HALL_SENSOR_PIN   4    // Magnetic sensor - reads LOW at home position
#define MOTOR_STEP_PIN    8    // Stepper motor step pin
#define MOTOR_DIR_PIN     9    // Stepper motor direction pin
#define MOTOR_ENABLE_PIN  10   // Stepper motor enable pin (active LOW)

// Configuration
#define STEPS_PER_FLAP    48   // Steps needed to advance one flap (adjust for your motor/gear ratio)
#define NUM_FLAPS         40   // Total number of flaps/characters on the drum
#define HOME_POSITION     2    // Character position when hall sensor reads LOW
#define STEP_DELAY_US     1500 // Microseconds between steps (controls speed)

// Character set - customize to match your flap order
const char CHARACTERS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-?!";
const int NUM_CHARACTERS = sizeof(CHARACTERS) - 1;

// State variables
int currentPosition = -1;  // Current flap position (-1 = unknown/not homed)
bool isHomed = false;

void setup() {
  Serial.begin(115200);

  // Configure pins
  pinMode(HALL_SENSOR_PIN, INPUT_PULLUP);
  pinMode(MOTOR_STEP_PIN, OUTPUT);
  pinMode(MOTOR_DIR_PIN, OUTPUT);
  pinMode(MOTOR_ENABLE_PIN, OUTPUT);

  // Disable motor initially
  digitalWrite(MOTOR_ENABLE_PIN, HIGH);
  digitalWrite(MOTOR_STEP_PIN, LOW);
  digitalWrite(MOTOR_DIR_PIN, HIGH);  // Forward direction

  Serial.println("Splitflap Controller Ready");
  Serial.println("Commands:");
  Serial.println("  H - Home the display");
  Serial.println("  G<char> - Go to character (e.g., GA for 'A')");
  Serial.println("  P<num> - Go to position number (e.g., P5)");
  Serial.println("  S - Show current status");
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
 * Step the motor one step
 */
void stepMotor() {
  digitalWrite(MOTOR_STEP_PIN, HIGH);
  delayMicroseconds(STEP_DELAY_US / 2);
  digitalWrite(MOTOR_STEP_PIN, LOW);
  delayMicroseconds(STEP_DELAY_US / 2);
}

/**
 * Advance the drum by one flap position
 */
void advanceOneFlap() {
  for (int i = 0; i < STEPS_PER_FLAP; i++) {
    stepMotor();
  }

  // Update position tracking
  if (currentPosition >= 0) {
    currentPosition = (currentPosition + 1) % NUM_FLAPS;
  }
}

/**
 * Home the display by finding the magnetic sensor position
 * The sensor reads LOW when we're at position 2
 */
void homeDisplay() {
  Serial.println("Homing display...");

  // Enable motor
  digitalWrite(MOTOR_ENABLE_PIN, LOW);
  delay(10);

  // If already at home, move away first
  if (isAtHome()) {
    Serial.println("Already at home sensor, moving away...");
    for (int i = 0; i < STEPS_PER_FLAP * 2; i++) {
      stepMotor();
    }
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
    stepMotor();
    stepCount++;
  }

  if (found) {
    // Continue stepping until we exit the sensor zone, then back up to center
    int sensorSteps = 0;
    while (isAtHome() && sensorSteps < STEPS_PER_FLAP) {
      stepMotor();
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

  // Disable motor when done
  digitalWrite(MOTOR_ENABLE_PIN, HIGH);
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

  // Calculate steps needed (always move forward)
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

  // Enable motor
  digitalWrite(MOTOR_ENABLE_PIN, LOW);
  delay(10);

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

    delay(50);  // Small delay between flaps for smoother motion
  }

  // Disable motor
  digitalWrite(MOTOR_ENABLE_PIN, HIGH);

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
  Serial.print("Total characters: ");
  Serial.println(NUM_CHARACTERS);
  Serial.print("Character set: ");
  Serial.println(CHARACTERS);
  Serial.println("==============");
}
