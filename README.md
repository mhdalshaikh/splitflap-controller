# Splitflap Display Controller

Arduino-based controller for a split-flap display module. Controls a stepper motor to flip cards until the desired character is reached, using a magnetic hall-effect sensor for position calibration.

## Hardware Requirements

- Arduino (Uno, Nano, or compatible)
- Stepper motor with driver (e.g., A4988, DRV8825)
- Hall-effect magnetic sensor
- Split-flap mechanism with magnet at home position

## Pin Connections

| Component | Pin |
|-----------|-----|
| Hall Sensor | D4 (reads LOW at home) |
| Motor Step | D8 |
| Motor Direction | D9 |
| Motor Enable | D10 |

## How It Works

1. **Homing**: At startup, the motor rotates until the hall sensor detects the magnet (reads LOW), indicating position 2
2. **Navigation**: From the known home position, the controller calculates steps needed to reach any target character
3. **Re-sync**: If the home position is detected during movement, the controller re-syncs automatically

## Serial Commands (115200 baud)

- `H` - Home the display
- `G<char>` - Go to character (e.g., `GA` for 'A')
- `P<num>` - Go to position number (e.g., `P5`)
- `S` - Show current status

## Configuration

Adjust these constants in the code for your setup:

```cpp
#define STEPS_PER_FLAP    48   // Steps to advance one flap
#define NUM_FLAPS         40   // Total flaps on drum
#define HOME_POSITION     2    // Position at sensor LOW
#define STEP_DELAY_US     1500 // Speed control
```

## Character Set

Default: ` ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-?!`

Modify `CHARACTERS[]` array to match your flap order.

## Reference

Based on concepts from [scottbez1/splitflap](https://github.com/scottbez1/splitflap)
