# EMI Desk Robot

EMI is a small, local-first expressive desk companion robot built around an ESP32, an OLED face, capacitive touch, and eventually head movement, local speech recognition, local memory, and carefully permissioned desktop integration.

EMI is meant to feel calm, attentive, curious, and alive without requiring a cloud LLM. His personality comes from deterministic behavior, timing, eye motion, touch reactions, local state, and explicit integrations.

## Current status

**Breadboard prototype**

Working now:

- ESP32 DevKit V1 prototype controller
- 128x64 SH1106 I2C OLED face
- stateful idle personality with curiosity and contentment
- smooth, infrequent blinking
- TTP223 capacitive touch input
- slow-pat session detection
- fast "noticed you" response when touched
- contented partial eye-close on individual pet strokes

In progress:

- I2S MEMS microphone
- microphone soldering and validation
- network / phone architecture

Planned:

- SG90 head movement
- final ESP32-C3 Mini controller
- simultaneous desktop + laptop agents
- phone BLE provisioning and Wi-Fi setup
- phone audio output / travel gateway
- Raspberry Pi local speech-to-text
- Tailscale link for travel use
- structured local memory
- safe allow-listed desktop actions
- 3D-printed enclosure

## Repository layout

- `firmware/emi_breadboard/emi_breadboard.ino` - current prototype firmware
- `docs/WIRING.md` - exact current wiring
- `docs/SETUP.md` - Arduino IDE setup and upload instructions
- `docs/HARDWARE.md` - parts and power notes
- `docs/CONNECTIVITY.md` - desktop, laptop, phone, Wi-Fi, and travel architecture
- `docs/SECURITY.md` - privacy and desktop-control design
- `docs/ROADMAP.md` - development roadmap
- `CHANGELOG.md` - firmware behavior changes

## Current prototype wiring

### OLED

| OLED pin | ESP32 DevKit V1 |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

### TTP223 touch sensor

| TTP223 pin | ESP32 DevKit V1 |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| OUT | GPIO 27 |

### I2S microphone

Current prototype pin plan:

| Mic pin | ESP32 DevKit V1 |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| L/R | GND |
| WS | GPIO 25 |
| SCK | GPIO 26 |
| SA / SD | GPIO 32 |

The microphone wiring is still being validated. Its header pins need proper solder joints before signal testing is considered reliable.

## Software

Current prototype uses:

- Arduino IDE
- ESP32 board package by Espressif Systems
- U8g2 library by oliver
- board selection: **DOIT ESP32 DEVKIT V1**

See [docs/SETUP.md](docs/SETUP.md) for setup details.

## Current behavior

### Idle

EMI no longer selects every idle move independently. He has simple internal curiosity and contentment values, remembers his recent idle actions, and chooses from several behavior sequences. This reduces obvious repetition and allows recent interaction to influence what he does next.

### Petting

A touch starts a petting session. EMI quickly shifts his gaze high toward the touch area. Slow pats remain part of the same session for about 2.2 seconds after the last contact. Every real stroke produces a smooth partial eye-close, and repeated petting gradually makes that reaction a little deeper. After several strokes he can give one deeper relaxed blink.

## Connectivity direction

Wi-Fi is the main runtime transport. BLE is primarily for phone pairing, Wi-Fi provisioning, and lightweight control.

The home Raspberry Pi will act as the speech backend and central hub. Desktop and laptop agents can both remain connected simultaneously; desktop is the default priority until the user selects another target.

When travelling without a computer, the phone can act as EMI's speaker and local gateway. EMI connects to the phone hotspot/local Wi-Fi, while the phone reaches the home Raspberry Pi through Tailscale.

See [docs/CONNECTIVITY.md](docs/CONNECTIVITY.md) for the full design.

## Important power rules

The OLED, TTP223, and I2S mic use 3.3 V in the current prototype.

The SG90 servo must **not** be powered from the ESP32 3.3 V pin. When the servo is added, it should use the 5 V / VBUS side, share ground with the ESP32, and have the planned electrolytic capacitor across servo 5 V and GND near the servo power path.

The currently selected capacitor is approximately:

- 470 uF
- 35 V
- low ESR
- 10 x 17 mm
- 5 mm lead spacing

## Final hardware direction

The finished EMI is planned around an **ESP32-C3 Mini**. The current DevKit V1 exists only because it is convenient for breadboard prototyping.

The final C3 pin map will be verified separately before permanent wiring. Do not copy the DevKit pin numbers blindly into the final enclosure.

## Privacy direction

EMI is designed to be local-first:

- no camera
- local speech recognition
- no continuously required cloud AI
- explicit microphone modes: off, push-to-talk, or always-listen
- visible listening state
- no arbitrary remote shell access
- desktop actions exposed through an explicit allow-list
- privileged actions require narrow helper commands and explicit confirmation

See [docs/SECURITY.md](docs/SECURITY.md).

## License

No license has been selected yet.
