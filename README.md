# JogPendant_BLE

BLE HID jog pendant for ESP32 (LOLIN D32) with CNCjs-oriented key mapping.

Project file:
- `JogPendant_BLE/JogPendant_BLE.ino`

## Features

- Rotary encoder jog control for the selected axis.
- Axis select: `X`, `Y`, `Z`, `4` (`Axis4` is used for probe trigger mode).
- Step select:
- `1` = small step (no modifier)
- `10` = medium step (`Alt`)
- `100` = large step (`Ctrl`)
- Smooth jog:
- `Shift` = smooth high
- `Shift + Alt` = smooth medium
- Probe trigger:
- Sends `Alt + P` after 3 enable clicks within a time window while Axis4 is selected.
- E-STOP lockout:
- Blocks all actions and blinks LED while E-STOP is active.

## HID keymap (no Arrow/PageUp/PageDown)

- `F13` = `X-`
- `F14` = `X+`
- `F15` = `Y+`
- `F16` = `Y-`
- `F17` = `Z+`
- `F18` = `Z-`

## Wiring (ESP32 / LOLIN D32)

All input pins are configured as `INPUT_PULLUP`. This means inputs are normally `HIGH` and become active when pulled to `GND` (active LOW), except E-STOP logic which is controlled by `ESTOP_PRESSED_LEVEL`.

### Pinout

- Encoder A: `GPIO32` (`ENC_A`)
- Encoder B: `GPIO22` (`ENC_B`)
- Axis X: `GPIO25` (`AXIS_X`)
- Axis Y: `GPIO27` (`AXIS_Y`)
- Axis Z: `GPIO14` (`AXIS_Z`)
- Axis4: `GPIO17` (`AXIS_4`)
- Step 1: `GPIO12` (`STEP_1`)
- Step 10: `GPIO13` (`STEP_10`)
- Step 100: `GPIO15` (`STEP_100`)
- E-STOP: `GPIO4` (`ESTOP`)
- Status LED: `GPIO16` (`LED_PIN`)

### Practical wiring

- Encoder channel A -> `GPIO32`
- Encoder channel B -> `GPIO22`
- Encoder GND -> `GND`
- Axis switch common -> `GND`
- Axis X output -> `GPIO25`
- Axis Y output -> `GPIO27`
- Axis Z output -> `GPIO14`
- Axis4 output -> `GPIO17`
- Step switch common -> `GND`
- Step 1 output -> `GPIO12`
- Step 10 output -> `GPIO13`
- Step 100 output -> `GPIO15`
- E-STOP wiring follows your chosen logic in code.
- Default is `#define ESTOP_PRESSED_LEVEL HIGH`.
- If your E-STOP uses pullup + switch to GND for pressed state, set it to `LOW`.
- LED wiring example: `GPIO16` -> resistor -> LED -> `GND`.

## Assumptions

- Only one axis line is active at a time.
- Only one step line is active at a time.
- Common ground (`GND`) is shared between switches/encoder and ESP32.

## Build and upload

1. Open `JogPendant_BLE/JogPendant_BLE.ino` in Arduino IDE.
2. Install the `BleKeyboard` library (T-vK).
3. Select the correct ESP32 board (LOLIN D32 or equivalent).
4. Compile and upload.
