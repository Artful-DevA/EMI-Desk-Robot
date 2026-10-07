# Changelog

This changelog starts from the point where the GitHub repository became writable. Earlier breadboard experiments happened before repository tracking.

## 2026-10-07 - Fix voice firmware include formatting

### Fixed

- corrected the FreeRTOS semaphore include in the new voice-enabled C3 firmware so it is a real preprocessor line instead of a literal escaped newline
- no behavior change; this is a compile fix for the voice-control build

## 2026-10-07 - Prepare first spoken-command test

### Changed

- added one-per-second VAD diagnostics for microphone level, learned noise floor, and trigger threshold
- EMI Hub now accepts both `Emi` and the common Whisper spelling `Emmy` as deterministic wake addresses
- wake matching remains exact and allow-listed; no fuzzy classifier can trigger an action

## 2026-10-07 - Add live C3 voice capture

### Added

- normal ESP32-C3 firmware now initializes the verified I2S microphone at 16 kHz mono
- added a lightweight local voice-activity detector with room-noise calibration
- keeps about 256 ms of pre-roll audio in RAM so the start of `Emi...` is not cut off
- captures short voice commands into a RAM-only PCM/WAV buffer
- posts captured WAV data to the authenticated EMI Hub `/device/audio` endpoint
- added a shared HTTP mutex so command polling pauses cleanly while a voice request is being transcribed
- added a tiny OLED listening indicator: hollow while VAD is armed, filled while capturing/transcribing

### Privacy

- command audio remains in RAM on the ESP32-C3
- the ESP32 does not write microphone audio to flash or filesystem
- the Pi-side hub already discards ordinary command transcripts after wake-word and deterministic intent parsing

### Limits

- v1 voice capture is intentionally capped at about 3 seconds to keep ESP32-C3 RAM usage conservative
- this is sufficient for the first `Emi, what's the time?` test; longer reminder phrases will use a later streaming/chunking design

## 2026-10-07 - Fix Whisper service startup compatibility

### Fixed

- removed nonessential whisper-server startup flags so the service starts with the smallest compatible argument set
- retained localhost-only binding and the existing tiny.en model
- installer now waits up to 15 seconds for model startup instead of assuming one second is enough
- installer prints full untruncated service status if whisper-server exits before becoming ready

## 2026-10-07 - Start live voice-control backend

### Added

- upgraded EMI Hub to v0.4 with authenticated `POST /device/audio` WAV ingestion
- audio uploads are held in RAM and forwarded to local whisper.cpp without EMI Hub writing temporary audio files
- added a localhost-only whisper.cpp service on port 17841 using the existing `tiny.en` model
- added a user-service installer for the Whisper backend
- voice commands must explicitly address `Emi` before the deterministic intent parser may trigger an action
- ordinary voice transcripts are discarded after parsing and are not written to normal logs

### Next

- add ESP32-C3 microphone VAD/capture and authenticated WAV upload to the new voice endpoint

## 2026-10-07 - Synchronize project documentation with the working prototype

### Documentation

- rewrote `docs/DEVLOG.md` in first person from the builder's point of view
- corrected exact active development tracking to exclude the reported **1h30m break**
- exact tracked active development is **03:26:31** at the latest documentation checkpoint; the older ~2-hour estimate remains separate
- refreshed README, setup, roadmap, feature inventory, and connectivity docs to match the active ESP32-C3 prototype
- documented the proven Raspberry Pi -> authenticated Wi-Fi -> physical C3 -> OLED time-command path
- documented the GPIO 7 OLED SCL mapping and the required 8.5 dBm C3 Wi-Fi TX-power workaround
- updated the next milestone to in-memory C3 microphone streaming into local Raspberry Pi Whisper
- cleaned duplicate/malformed changelog headers left by earlier documentation updates

## 2026-10-07 - Align clock with EMI's eyes

### Changed

- made the four clock digits slightly smaller so the clock reads as part of Emi's face instead of a separate full-screen UI
- centered the hour pair on the normal left-eye position and the minute pair on the normal right-eye position
- vertically centered the clock on Emi's normal eye line
- re-centered the colon between both eye-aligned groups
- kept the colon visible during the short clock hold so the display always reads clearly as `HH:MM`

## 2026-10-07 - Polish clock layout and return animation

### Changed

- moved the clock colon 2 pixels to the right for better optical spacing
- removed the flat horizontal eye bars that appeared while the clock was hiding
- eyes now return from a rounded partially-open shape instead of a 3-pixel closed line
- retained the GPIO 7 OLED clock wiring and proven 8.5 dBm Wi-Fi TX-power fix

## 2026-10-07 - Move OLED SCL off the C3 BOOT pin

### Changed

- moved SH1106 OLED SCL from GPIO 9 to GPIO 7 in normal ESP32-C3 firmware
- GPIO 9 is now intentionally left free for the ESP32-C3 BOOT/download strap
- retained the proven 8.5 dBm Wi-Fi TX-power fix
- updated wiring and hardware documentation to match the new breadboard layout

## 2026-10-07 - Fix ESP32-C3 Super Mini Wi-Fi authentication

### Fixed

- confirmed the ESP32-C3 Super Mini can connect reliably when Wi-Fi transmit power is reduced to **8.5 dBm**
- normal EMI C3 firmware now applies `WIFI_POWER_8_5dBm` before starting the network task
- kept the exact 2.4 GHz AP selection and disconnect diagnostics from the previous Wi-Fi debugging pass
- this resolves repeated `AUTH_EXPIRE (2)` failures seen at the board's previous transmit-power setting

## 2026-10-07 - Add ESP32-C3 Super Mini TX-power Wi-Fi test

### Changed

- updated the standalone Wi-Fi connection diagnostic to target the exact scanned 2.4 GHz AP and automatically test lower Wi-Fi transmit powers
- tries 8.5 dBm first, then 11 dBm, 5 dBm, and 13 dBm
- keeps reporting the underlying disconnect reason for every attempt
- the test still reads SSID and password only from the private `secrets.h`

## 2026-10-07 - Add two-stage Wi-Fi connection test

### Added

- added a standalone C3 Wi-Fi connection diagnostic that first tries a normal WPA2 connection
- if the normal attempt fails, it scans for the configured SSID and retries against the exact visible 2.4 GHz channel and BSSID
- both attempts print the underlying ESP-IDF disconnect reason instead of only the coarse Arduino Wi-Fi status
- the test uses the existing private `secrets.h` values and never hardcodes an SSID or password in the public repository

## 2026-10-07 - Pin Wi-Fi connection to the visible 2.4 GHz AP

### Changed

- normal C3 firmware now scans for the configured SSID before each connection attempt
- when the SSID is found, the firmware connects directly to the discovered 2.4 GHz channel and BSSID instead of leaving AP selection ambiguous
- added Wi-Fi disconnect event logging with the underlying ESP-IDF reason code and common reason names
- connection timeout output now includes the last disconnect reason
- disabled Wi-Fi modem sleep while the hub connection is active to simplify connectivity debugging

## 2026-10-07 - Make Wi-Fi scan output impossible to miss

### Changed

- the C3 Wi-Fi diagnostic now waits briefly for USB Serial to appear
- Wi-Fi scans repeat every 10 seconds instead of printing only once during boot
- added clear scan-complete and next-scan messages for easier USB/Serial debugging

## 2026-10-07 - Add C3 Wi-Fi visibility diagnostic

### Added

- added a standalone ESP32-C3 Wi-Fi scan sketch that does not require credentials
- the test prints each 2.4 GHz SSID the C3 can actually see, along with RSSI, channel, and encryption mode
- intended to distinguish a router/radio visibility problem from an authentication or password problem

## 2026-10-07 - Fix C3 Wi-Fi connection retry loop

### Fixed

- stopped calling `WiFi.reconnect()` while the ESP32-C3 was already in the middle of connecting
- moved Wi-Fi connection ownership entirely into the background network task
- disabled the Arduino auto-reconnect path so two reconnect mechanisms do not fight each other
- connection attempts now get up to 15 seconds before being reset and retried
- Serial Monitor now prints a clear connection-attempt message and timeout status instead of repeatedly spamming `wifi:sta is connecting, return error`

## 2026-10-07 - Hub startup check and live time tracking

### Changed

- development log now shows the running exact duration of the active session instead of displaying zero until the session ends
- EMI Hub installer now waits briefly for the service health endpoint after restart
- if the hub fails to start, the installer prints service status and recent logs instead of leaving a confusing immediate connection error

## 2026-10-07 - Authenticated Pi -> C3 command bridge

### Added

- EMI Hub v0.3 queues deterministic display commands for the ESP32-C3
- added authenticated `/device/command` polling endpoint for the C3
- speech/intent requests remain restricted to localhost even though the hub now listens on the LAN
- installer creates a random private shared token outside the repository
- ESP32-C3 normal firmware now joins Wi-Fi and polls the Pi hub in a background task
- network commands are transferred into the normal face loop through a FreeRTOS queue so failed/slow networking does not intentionally block eye animation
- added `secrets.example.h`; real Wi-Fi credentials and token stay in ignored `secrets.h`

### Security

- current transport is restricted to a trusted LAN prototype and allow-listed display commands
- credentials and authentication token are not stored in Git
- the current plain-HTTP LAN transport must not be exposed directly to the internet or reused for privileged desktop actions

## 2026-10-07 - More natural time phrasing in EMI Hub

### Changed

- upgraded EMI Hub to v0.2
- accepts casual greetings before EMI such as `yo`, `hey`, `hi`, `hello`, `ok`, and `okay`
- accepts short requests such as `Yo Emi time` and `Yo Emi time is?`
- keeps the command parser deterministic instead of using fuzzy intent matching for execution

## 2026-10-07 - Faster clock glance

### Changed

- reduced the SHOW_TIME hold from 4.5 seconds to 2.0 seconds
- the full eyes-to-clock and clock-to-eyes morph remains unchanged
- total interaction now feels more like a quick glance at the time instead of a modal screen

## 2026-10-07 - SHOW_TIME clock command on ESP32-C3

### Added

- normal ESP32-C3 firmware now accepts the deterministic serial command `SHOW_TIME HH:MM`
- added strict 24-hour time validation before displaying the clock
- added a custom eye-scale 7-segment clock rather than a font-based clock
- added the full eyes -> thin bars -> clock -> thin bars -> eyes transition
- clock colon blinks while the time is being held on screen
- touching EMI while the clock is visible immediately returns to the face and begins normal petting interaction
- added `FACE` as a simple manual command to dismiss the clock

### Architecture

- this command becomes the display-side endpoint for the upcoming local speech flow
- the Raspberry Pi can later recognize phrases such as "what time is it?" and send the same `SHOW_TIME HH:MM` command without changing the animation code

## 2026-10-07 - Normal Emi moved to ESP32-C3

### Added

- added the normal everyday Emi firmware for the ESP32-C3 Super Mini at `firmware/emi_c3/emi_c3.ino`
- preserved the established smooth eye, gaze, blink, idle-personality, attention-bid, and petting behavior
- applied the verified C3 pin map: OLED GPIO8/9, touch GPIO10, microphone GPIO4/5/6
- set OLED contrast to 100 for a softer daytime brightness
- kept the verified microphone connected in the hardware map without adding diagnostic overlays or microphone UI to normal Emi

### Documentation

- promoted the ESP32-C3 OLED/touch/microphone pin map from conceptual to physically verified
- retained the previous ESP32 DevKit V1 map only as prototype reference
- marked the ESP32-C3 Super Mini as the current prototype controller

## 2026-10-07 - ESP32-C3 OLED, touch, and microphone integration test

### Added

- added a combined ESP32-C3 validation sketch for the OLED, TTP223 touch sensor, and I2S microphone
- C3 test pin map uses OLED SDA GPIO8 / SCL GPIO9, touch GPIO10, and microphone SCK GPIO4 / WS GPIO5 / SD GPIO6
- OLED shows Emi-style eyes, touch feedback, the raw microphone level, and an auto-ranging live microphone meter
- serial output also reports touch state and microphone level for diagnostics

## 2026-10-07 - ESP32-C3 OLED validation test

### Added

- added a dedicated ESP32-C3 OLED validation sketch
- test wiring uses SDA GPIO8 and SCL GPIO9
- test targets the existing SH1106 128x64 I2C OLED
- OLED contrast is set to 100 for a less harsh brightness level

## 2026-10-07 - ESP32-C3 empty upload test

### Added

- added a minimal empty sketch for validating the ESP32-C3 Super Mini upload/reset path
- intended for use with Arduino IDE board selection `ESP32C3 Dev Module`
- no peripherals or pins are used in this test

## 2026-10-07 - OLED microphone meter

### Changed

- microphone validation test now displays live LEVEL and PEAK values on the SH1106 OLED
- added a large horizontal live audio meter for testing the soldered microphone without Serial Monitor
- OLED test uses the existing ESP32 DevKit I2C wiring: SDA GPIO21 and SCL GPIO22
- test applies OLED contrast 100 to reduce excessive brightness

## 2026-10-07 - Soldered microphone validation test

### Added

- added a dedicated ESP32 DevKit I2S microphone validation sketch
- current mic test wiring uses WS GPIO25, SCK GPIO26, and SD/SA GPIO32
- serial output reports DC-corrected audio LEVEL and PEAK values
- intended for confirming the freshly soldered microphone before moving on to ESP32-C3 testing

## 2026-10-07 - Planned night display dimming

### Documentation

- added automatic time-of-day OLED dimming to the feature inventory
- planned gradual late-night dimming rather than abrupt brightness changes
- documented a very dim overnight window around 02:00-07:00
- night behavior should also suppress unnecessary attention bids
- brightness should return gradually in the morning

## 2026-10-07 - Privacy, memory confirmation, and meeting audio policy

### Documentation

- clarified that the user can be EMI's best friend, while EMI is not intended to replace the user's human friendships
- made persistent-memory writes a two-step explicit command with mandatory "confirm save"
- made persistent-memory deletion a two-step explicit command with mandatory "confirm delete"
- casual phrases containing "remember" or "forget" can never modify persistent memory
- raw microphone audio is never written to disk
- ordinary conversations are never stored as transcripts; only explicit note mode retains text
- note-mode Markdown can be written directly into a configured Obsidian vault
- added class/meeting speech privacy behavior
- when headphones are the active output during a class/meeting, EMI may speak
- when speakers are active during a class/meeting, EMI stays silent and uses visual/text feedback
- if the active output cannot be identified confidently, EMI defaults to silence
- Ubuntu audio-output switching should move EMI's TTS with the system default output

## 2026-10-07 - Eye-scale clock morph

### Changed

- kept the timer at its current readable size
- replaced the font-based large clock with a custom 7-segment clock
- clock now occupies roughly the same visual footprint as EMI's eyes
- eyes close into thin bars before the clock appears
- the bars shrink while the clock reveals from the center
- reverse transition turns the clock back into eye-lines and then reopens EMI's eyes
- random valid HH:MM demo time still appears about 12 seconds after boot

### Documentation

- notes are now explicitly session-only transcription
- raw audio is never written to disk
- note-mode Markdown can be written directly to an Obsidian vault
- project-awareness boundaries are explicitly documented

## 2026-10-07 - Larger timer + random clock transition

### Changed

- enlarged the timer so it is readable at a glance
- kept the timer in the top-right corner
- removed the tiny-font rendering that could look like stray punctuation
- clock transition now always shows a clearly visible random HH:MM test time
- large clock draws its own colon dots
- clock reveal/hide expands symmetrically from the center
- feature inventory now uses the privacy principle "functional memory, not intimate memory"
- music-learning design now favors coarse preference categories instead of permanent exact counts

## 2026-10-07 - Attention bid + clock transition test

### Changed

- reduced pet-stroke partial eye closure again to avoid an over-squinted expression
- added a prototype attention bid using two deliberate blinks because blinking is visually noticeable
- attention bids are rate-limited and reset when EMI is petted
- added a one-shot eyes-to-clock-to-eyes animation approximately 12 seconds after boot
- clock animation uses a fixed 12:34 test value until real network time exists
- miniature timer hides during the clock animation
- direct touch interaction cancels the clock animation immediately

### Documentation

- added a canonical docs/FEATURES.md inventory
- documented bounded mood/rest design
- documented privacy boundaries and memory decay
- documented class reminders, phone notifications, multilingual goals, music preference learning, and special-date handling

## 2026-10-07 - Gentler petting + mini timer UI test

### Changed

- reduced per-stroke eye closure so petting looks contented rather than squinty
- pet-stroke closure now starts around 30% and caps around 42%
- shortened the pet-stroke animation slightly
- added a tiny 10-minute countdown in the top-right corner as a display-space test
- the miniature timer automatically starts at boot
- the timer hides while EMI is actively being petted so the face keeps visual priority
- the timer is right-aligned using the tiny 4x6 U8g2 font

## 2026-10-07 - Personality drives + less repetitive idle

### Changed

- added internal curiosity and contentment values that evolve over time
- idle behavior selection is now influenced by EMI's current internal state
- EMI remembers the previous two idle actions and strongly avoids repeating them
- added distinct still, micro-glance, focus, curious-peek, wide-glance, settle, and soft-attention behaviors
- curiosity rises while EMI is left alone and is reduced by exploration
- contentment increases through petting and fades slowly afterward
- high contentment makes calm/still idle behavior more likely after petting
- first pet attention movement is now about 85 ms after touch recognition
- every recognized stroke produces a contented partial eye-close
- repeated strokes gradually deepen the contented eye-close up to a safe limit
- deeper relaxed blink now requires several strokes
- normal blink timing widened to roughly 9-18 seconds
- double blink probability reduced again

### Removed / refined

- reduced purely random idle selection
- avoided immediate reuse of recent idle animations
- no vertical petting bounce
- no per-stroke random gaze jump
