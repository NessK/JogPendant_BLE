# JogPendant_BLE

BLE HID jog-pendant for ESP32 (LOLIN D32) til CNCjs.

Prosjektfil:
- `JogPendant_BLE/JogPendant_BLE.ino`

## Funksjon

- Roterende encoder for jogging av valgt akse.
- Axis-valg: `X`, `Y`, `Z`, `4` (Axis4 brukes til probe-trigger).
- Step-valg:
  - `1` = small step (ingen modifier)
  - `10` = medium step (`Alt`)
  - `100` = large step (`Ctrl`)
- Smooth jog:
  - `Shift` = smooth high
  - `Shift + Alt` = smooth medium
- Probe trigger:
  - `Alt + P` etter 3 enable-klikk innen tidsvindu når Axis4 er valgt.
- E-STOP lockout:
  - Blokkerer all input og blinker LED mens E-STOP er aktiv.

## HID keymap (oppdatert for CNCjs uten pil/PageUp/PageDown)

- `F13` = `X-`
- `F14` = `X+`
- `F15` = `Y+`
- `F16` = `Y-`
- `F17` = `Z+`
- `F18` = `Z-`

## Kobling (ESP32 / LOLIN D32)

Alle input-pinner er satt til `INPUT_PULLUP`, dvs. signal er normalt `HIGH` og blir aktivt ved å trekke pinnen til `GND` (active LOW), unntatt E-STOP som kan konfigureres via `ESTOP_PRESSED_LEVEL`.

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

### Praktisk kobling

- Encoder:
  - Encoder kanal A -> `GPIO32`
  - Encoder kanal B -> `GPIO22`
  - Encoder GND -> `GND`
- Axis-velger (f.eks. 4-posisjons bryter):
  - Felles -> `GND`
  - Utgang X -> `GPIO25`
  - Utgang Y -> `GPIO27`
  - Utgang Z -> `GPIO14`
  - Utgang 4 -> `GPIO17`
- Step-velger (f.eks. 3-posisjons bryter):
  - Felles -> `GND`
  - Utgang 1 -> `GPIO12`
  - Utgang 10 -> `GPIO13`
  - Utgang 100 -> `GPIO15`
- E-STOP:
  - Koble etter valgt logikk i kode.
  - Standard i filen er `#define ESTOP_PRESSED_LEVEL HIGH`.
  - Hvis du bruker pullup + bryter til GND for "pressed", endre til `LOW`.
- LED:
  - `GPIO16` -> seriemotstand -> LED -> `GND`
  - Eller bruk intern/ekstern LED tilpasset ditt kort.

## Viktige antakelser

- Velg bare én axis-linje aktiv av gangen.
- Velg bare én step-linje aktiv av gangen.
- Felles jord (`GND`) mellom alle brytere/encoder og ESP32.

## Bygg og opplasting

1. Åpne `JogPendant_BLE/JogPendant_BLE.ino` i Arduino IDE.
2. Installer biblioteket `BleKeyboard` (T-vK).
3. Velg riktig ESP32-board (LOLIN D32 / tilsvarende).
4. Kompiler og last opp.

