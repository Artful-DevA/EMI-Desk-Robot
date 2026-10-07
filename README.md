# EMI Desk Robot

EMI is a small, local-first expressive desk companion robot built around an ESP32-C3 Super Mini, a monochrome OLED face, capacitive touch, an I2S microphone, and a Raspberry Pi speech/backend hub.

EMI is designed to feel calm, attentive, curious, and alive without depending on a cloud LLM. His personality comes from deterministic state, timing, eye motion, touch reactions, local speech processing, and explicit integrations.

## Current status

**Active prototype: ESP32-C3 Super Mini + Raspberry Pi hub**

Working now:

- ESP32-C3 Super Mini as the main controller
- SH1106 128x64 OLED face
- OLED SDA on GPIO 8
- OLED SCL on GPIO 7
- GPIO 9 intentionally left free for BOOT/download mode
- TTP223 capacitive touch on GPIO 10
- verified I2S microphone on GPIO 4/5/6
- expressive eye and gaze behavior
- curiosity/contentment-driven idle behavior
- non-repetitive idle action selection
- touch/petting reactions
- attention bids
- deterministic `SHOW_TIME HH:MM` command
- eye-aligned HH:MM clock UI
- clean clock-to-eyes return animation
- working 2.4 GHz Wi-Fi
- ESP32-C3 Super Mini TX power fixed at 8.5 dBm for reliable authentication
- authenticated Raspberry Pi -> ESP32-C3 command polling
- deterministic Raspberry Pi intent parser
- local `whisper.cpp` proven on the Pi
- physical Pi -> EMI time command proven end-to-end

In progress:

- microphone audio streaming from the C3 to the Pi
- in-memory audio handling only
- local speech-to-text command flow
- explicit listening modes
- PC/laptop agent integration

Planned:

- SG90 head movement
- local TTS
- desktop/laptop agents
- phone BLE provisioning
- travel mode through phone + Tailscale
- local structured memory
- note mode
- timers/reminders
- 3D-printed enclosure

## Current verified wiring

| Function | ESP32-C3 Super Mini |
| --- | --- |
| OLED SDA | GPIO 8 |
| OLED SCL | GPIO 7 |
| Touch OUT | GPIO 10 |
| Mic SCK / BCLK | GPIO 4 |
| Mic WS / LRCLK | GPIO 5 |
| Mic SD / SA | GPIO 6 |
| Mic L/R | GND |

OLED, touch, and microphone use the 3.3 V rail and share common ground.

GPIO 9 is intentionally kept free because it is a C3 BOOT strapping pin.

See [docs/WIRING.md](docs/WIRING.md) for the complete wiring notes.

## Current network architecture

The Raspberry Pi runs the EMI Hub.

The current prototype flow is:

1. EMI joins the home 2.4 GHz Wi-Fi network.
2. The Pi queues allow-listed commands.
3. The C3 polls the Pi roughly four times per second.
4. The device endpoint requires a private shared token.
5. The C3 receives commands such as `SHOW_TIME HH:MM`.
6. The C3 handles the physical animation locally.

The first complete physical test is working:

**Pi intent request -> authenticated hub queue -> Wi-Fi -> C3 -> OLED clock animation**

The current plain-HTTP device transport is a trusted-LAN prototype only and must not be exposed directly to the internet.

## ESP32-C3 Wi-Fi note

This particular ESP32-C3 Super Mini could scan the correct 2.4 GHz AP but repeatedly failed authentication with:

`AUTH_EXPIRE (2)`

The working fix was reducing Wi-Fi transmit power to:

`WIFI_POWER_8_5dBm`

That setting is now part of the normal firmware.

## Raspberry Pi speech backend

`whisper.cpp` is built and running locally on the Raspberry Pi 400.

The `tiny.en` model successfully transcribed the bundled test sample faster than real time, which is sufficient for the first short-command milestone.

The next step is to stream microphone audio from EMI to the Pi without ever writing raw audio to disk.

## Privacy direction

EMI is designed to be local-first:

- no camera
- local speech recognition
- no required cloud audio
- raw microphone audio is never written to disk
- ordinary conversation is not stored as transcripts
- only explicit note mode may retain text
- visible listening state
- microphone-off mode
- no arbitrary remote shell access
- desktop actions use explicit allow-lists
- privileged actions require narrow confirmation flows
- no web searching for information about the user

See [docs/SECURITY.md](docs/SECURITY.md) and [docs/FEATURES.md](docs/FEATURES.md).

## Repository layout

- `firmware/emi_c3/emi_c3.ino` - current normal ESP32-C3 firmware
- `firmware/emi_c3/secrets.example.h` - private network configuration template
- `firmware/tests/` - hardware/network diagnostics
- `hub/emi_hub.py` - Raspberry Pi hub
- `docs/WIRING.md` - current verified wiring
- `docs/SETUP.md` - Arduino/C3 setup and upload notes
- `docs/HARDWARE.md` - hardware and power notes
- `docs/FEATURES.md` - canonical feature inventory
- `docs/CONNECTIVITY.md` - network architecture
- `docs/SECURITY.md` - privacy/security design
- `docs/ROADMAP.md` - development roadmap
- `docs/DEVLOG.md` - first-person build diary and tracked development time
- `CHANGELOG.md` - code/documentation change history

## Power rules

The OLED, TTP223, and microphone use 3.3 V.

The planned SG90 servo must **not** be powered from the ESP32 3.3 V rail. It should use a 5 V / VBUS supply path, share ground with the ESP32, and have the planned bulk capacitor close to its supply.

## License

No license has been selected yet.
