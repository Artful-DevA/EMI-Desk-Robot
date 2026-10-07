# Changelog\n\nThis changelog starts from the point where the GitHub repository became writable. Earlier breadboard experiments happened before repository tracking.\n\n## 2026-10-07 - Fix C3 Wi-Fi connection retry loop

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

## 2026-10-07 - More natural time phrasing in EMI Hub

### Changed

- upgraded EMI Hub to v0.2
- replaced exact full-sentence regex matching with a deterministic phrase-normalization step
- accepts casual greetings before EMI such as `yo`, `hey`, `hi`, `hello`, `ok`, and `okay`
- accepts short time requests such as `Yo Emi time` and `Yo Emi time is?`
- keeps the intent allow-listed and deterministic rather than using fuzzy ML for command execution

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

# Changelog

This changelog starts from the point where the GitHub repository became writable. Earlier breadboard experiments happened before repository tracking.

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
