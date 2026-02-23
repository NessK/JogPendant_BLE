# JogPendant_BLE

BLE HID jog pendant for ESP32 (tested on LOLIN D32), designed for CNCjs.

This project turns a physical jog wheel + selector switches into a wireless BLE keyboard pendant with dedicated function-key mapping (`F13`-`F18`) to avoid Arrow/Page key conflicts.

## Highlights

- BLE keyboard pendant for CNC jogging
- Rotary encoder with robust detent decoding
- Axis select: `X`, `Y`, `Z`, `Axis4`
- Step modes: `1` -> no modifier, `10` -> `Alt`, `100` -> `Ctrl`
- Smooth jog modes: `Shift` -> smooth high, `Shift + Alt` -> smooth medium
- Probe trigger: `Alt + P` (Axis4 + 3 enable clicks)
- E-STOP lockout with LED blink and full input block

## HID Mapping

| Action | Key |
|---|---|
| X- | `F13` |
| X+ | `F14` |
| Y+ | `F15` |
| Y- | `F16` |
| Z+ | `F17` |
| Z- | `F18` |

Modifiers are applied by the selected step mode as described above.

## Hardware

- ESP32 dev board (LOLIN D32 or equivalent)
- Incremental rotary encoder (A/B outputs)
- Axis selector switch (X/Y/Z/4)
- Step selector switch (1/10/100)
- E-STOP input switch
- LED + resistor (status)

## Pinout

| Function | GPIO | Code Symbol |
|---|---:|---|
| Encoder A | 32 | `ENC_A` |
| Encoder B | 22 | `ENC_B` |
| Axis X | 25 | `AXIS_X` |
| Axis Y | 27 | `AXIS_Y` |
| Axis Z | 14 | `AXIS_Z` |
| Axis4 | 17 | `AXIS_4` |
| Step 1 | 12 | `STEP_1` |
| Step 10 | 13 | `STEP_10` |
| Step 100 | 15 | `STEP_100` |
| E-STOP | 4 | `ESTOP` |
| Status LED | 16 | `LED_PIN` |

## Wiring Notes

All inputs use `INPUT_PULLUP`.

- Normal state: input reads `HIGH`
- Active state: connect input to `GND` (active LOW)
- Shared `GND` between ESP32, switches, and encoder is required

Suggested wiring:

- Encoder A/B -> `GPIO32` / `GPIO22`
- Selector switch commons -> `GND`
- Axis outputs -> `GPIO25`, `GPIO27`, `GPIO14`, `GPIO17`
- Step outputs -> `GPIO12`, `GPIO13`, `GPIO15`
- LED -> `GPIO16` through resistor to `GND`

E-STOP behavior is controlled by `ESTOP_PRESSED_LEVEL` in code.

- Default in this project: `HIGH`
- If your pressed state pulls to ground, set it to `LOW`

## CNCjs Setup

Map CNCjs jog actions to `F13`-`F18`.

Recommended mapping:

- `F13` -> X-
- `F14` -> X+
- `F15` -> Y+
- `F16` -> Y-
- `F17` -> Z+
- `F18` -> Z-

This keeps jog controls isolated from normal arrow/page navigation keys.

## Quick Start

1. Open `JogPendant_BLE/JogPendant_BLE.ino` in Arduino IDE.
2. Install library: `BleKeyboard` (T-vK).
3. Select your ESP32 board (LOLIN D32 or equivalent).
4. Build and upload.
5. Pair the BLE device named `CNC Jog Pendant`.
6. Configure CNCjs hotkeys for `F13`-`F18`.

## Runtime Behavior

- Jogging only works when both an axis and a step mode are selected.
- Axis4 disables jog and enables probe trigger click counting.
- If enable goes OFF, pendant sends `!` (feed hold).
- During E-STOP, all key output is blocked.

## Troubleshooting

- No key output: confirm BLE pairing and active connection.
- No jog response in CNCjs: verify hotkeys are bound to `F13`-`F18`.
- Unstable behavior: verify only one axis and one step input are active at a time.
- Wrong E-STOP behavior: flip `ESTOP_PRESSED_LEVEL` between `HIGH` and `LOW`.

## Project File

- `JogPendant_BLE/JogPendant_BLE.ino`
