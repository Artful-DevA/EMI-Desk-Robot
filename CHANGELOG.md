\n## 2026-10-09 - Add a standalone eyes and petting test

### Added

- added `firmware/tests/c3_eyes_pet_test/c3_eyes_pet_test.ino`
- isolates Emi's SH1106 eyes, natural idle gaze, blinking, TTP223 pet detection, pet-stroke squint, and relaxed petting blink
- uses the verified C3 wiring: OLED SDA GPIO 8, OLED SCL GPIO 7, touch OUT GPIO 10
- deliberately has no Wi-Fi, microphone, Pi, or voice dependency

### Why

- this gives a fast known-simple sketch for checking Emi's face and pet interaction without the current voice/network work affecting the test

# Changelog

## 2026-10-09 - Restore the original Emi-time baseline

### Changed

- restored `firmware/tests/c3_voice_e2e_test/c3_voice_e2e_test.ino` to the exact v2 time-only diagnostic from commit `2137a6c`
- this is the earlier 3-second capture / `HTTPClient::POST()` build that produced a real end-to-end `TIME intent matched` result during physical testing
- no Raspberry Pi service, hub, Vosk, Whisper, or network configuration was changed

### Why

- after timer work introduced longer captures and upload failures, the next diagnostic is to return to the earlier known-success path and test only `Emi time`
- Git history still preserves the newer timer/upload experiments for later comparison


## 2026-10-08 - Stream C3 voice uploads in bounded chunks

### Fixed

- C3 voice/timer diagnostic upgraded to v7
- replaced the single large `HTTPClient::POST()` WAV send with explicit `WiFiClient` streaming in 1024-byte chunks
- partial writes are retried instead of treating one short socket write as a complete request failure
- Serial diagnostics now report upload byte progress, connection closure, stalls, and the returned HTTP status line
- response reads keep the intended 30-second timeout while the Pi runs local recognition
- the authenticated `/device/audio` endpoint, RAM-only audio handling, and server-side recognizers are unchanged

### Why

- packet capture proved that the C3 completed the TCP handshake and sent the HTTP headers, while zero WAV body bytes followed before the ESP32 returned HTTP error `-3`
- the Pi was not resetting or rejecting the connection; it was waiting for the declared request body
- streaming the already-buffered WAV in small writes removes the fragile all-at-once payload send while keeping raw audio in RAM only


This changelog starts from the point where the GitHub repository became writable. Earlier breadboard experiments happened before repository tracking.

## 2026-10-08 - Keep the installed Whisper service in sync

### Fixed

- `install-user-service.sh` now refreshes the installed `emi-whisper.service` before restarting the hub
- this prevents the Pi from running an older user-service definition after the repository has newer Whisper/Silero VAD flags
- the installer still keeps Whisper bound to localhost only

### Why

- the repository service definition included Silero VAD flags, but the actually running systemd user service was still using an older command line without those flags
- updating only EMI Hub was not enough to refresh the separate Whisper service file

## 2026-10-08 - End recording when the audio actually stops

### Changed

- C3 voice/timer diagnostic upgraded to v6
- removed the short-vs-long phrase endpoint heuristic
- while recording, every above-threshold voice-energy block refreshes a `lastVoiceActivity` timestamp
- the clip now ends only after the signal has stayed back at the learned room-noise level for about 800 ms
- ordinary gaps between words no longer depend on how long the sentence has already been
- the 5-second RAM capture ceiling remains only as a safety guard against continuous noise or a stuck detector
- Serial diagnostics now report `audio stopped` plus the measured quiet duration when endpointing normally

### Why

- the desired behavior is simple: once EMI starts a command, keep recording until the speaker stops
- literal zero audio is impossible in a real room, so the practical definition of "stopped" is a sustained return to the calibrated room-noise floor

## 2026-10-08 - Stop chopping longer timer phrases

### Changed

- C3 voice/timer diagnostic upgraded to v5
- increased the maximum RAM-only capture window from 3 seconds to 5 seconds for longer natural commands
- kept the fast ~480 ms end-of-speech timeout for short phrases
- once a capture is clearly a longer phrase, EMI now tolerates about 830 ms of silence before ending the clip
- this specifically prevents natural pauses around words such as `timer` from prematurely splitting `Emi set a timer for 30 seconds`
- added privacy-safe Serial diagnostics showing whether a capture ended by silence or by the hard maximum, plus duration and silence-block counts
- no wake-word, timer-parser, or confidence thresholds were changed in this iteration

### Why

- the C3 does not understand the word `timer`; if capture appeared to stop at that word, the acoustic end-of-speech logic was the likely culprit
- the previous 480 ms silence cutoff and 3-second hard limit were tuned for short commands such as `Emi time`, not longer timer sentences

## 2026-10-08 - Add working voice timers

### Added

- EMI Hub v0.13 now owns one active countdown timer in RAM
- spoken timer creation such as `Emi set a timer for 30 seconds`
- deterministic duration parsing for seconds, minutes, hours, digits, and common spoken number words
- spoken remaining-time queries such as `Emi how much time is left`
- spoken timer cancellation such as `Emi cancel timer`
- authenticated `/device/timer` state endpoint so the C3 can recover an active timer after a C3 reset
- timer expiry queues a deterministic `TIMER_DONE` device command for future normal-firmware integration
- C3 voice diagnostic v4 mirrors the Pi countdown locally, displays timer set/remaining values, and shows a temporary TIMER DONE state on expiry

### Voice architecture

- explicit EMI wake recognition remains a separate Vosk gate
- a third lightweight Vosk gate identifies timer-like speech
- only after the EMI wake gate passes does Whisper transcribe timer wording/duration
- Whisper text never directly executes an action; deterministic parsing decides SET_TIMER, TIMER_LEFT, or TIMER_CANCEL
- raw WAV and ordinary command transcripts remain RAM-only/transient

### Deferred

- timer-to-eyes transition animation
- small corner countdown beside EMI's normal eyes
- animated timer inspection/return transition
- final timer-expired personality animation
- TTS/speaker output

## 2026-10-08 - Reduce voice-command reaction latency

### Changed

- EMI Hub upgraded to v0.12
- the independent Vosk wake-name gate and time-intent gate now run concurrently instead of sequentially
- recognition rules and confidence thresholds are unchanged, so the v0.11 reliability behavior is preserved
- this removes most of the extra backend delay introduced when the recognizer was split into two independent passes

### Why

- v0.11 fixed wake-word position reliability, but doing two full Vosk passes one after another made successful commands feel noticeably slower
- the two passes are independent and can safely use separate recognizer instances against the same in-memory PCM clip

## 2026-10-08 - Make wake-word position independent

### Changed

- EMI Hub upgraded to v0.11
- replaced the single whole-phrase Vosk grammar with two independent constrained recognition passes
- one pass asks only whether the clip contains the explicit EMI wake name (acoustically `Emmy`)
- the second pass asks only whether the clip contains the `time` intent
- wake word and command can now appear in either order without relying on one exact phrase transcription
- natural contractions such as `what's the time` no longer need to match one exact grammar sentence because the intent gate only needs to hear `time`
- retained a short decoder silence tail to improve phrase-final `Emi`
- both gates still have independent confidence thresholds, so a time request without the explicit EMI wake name does not execute

### Why

- wake-first phrases were working while the same commands with `Emi` at the end were unreliable
- whole-phrase constrained recognition was still coupling wake-word placement to sentence shape
- independent wake and intent gates preserve the explicit wake requirement while making word order irrelevant

## 2026-10-08 - Improve long and wake-at-end time phrases

### Changed

- EMI Hub upgraded to v0.10
- expanded the constrained Vosk grammar with more natural time-request variants in both wake-first and wake-last order
- stopped rejecting a whole phrase because one filler word such as `what`, `is`, or `it` had low confidence
- command acceptance now scores the two words that actually matter independently: the `Emmy` acoustic wake token and `time`
- added a short synthetic silence tail before finalizing Vosk so phrase-final `Emi` has time to decode cleanly
- kept the requirement that both the wake token and the time token are present before any action is executed

### Why

- `Emi time` was working much more reliably than `Emi, what's the time?` and wake-at-end forms
- longer phrases naturally contain more low-confidence filler words, so using the minimum confidence across every word unfairly penalized them
- phrase-final wake words also benefit from a little decoder-finalization padding

## 2026-10-08 - Replace short EMI wake KWS with constrained Vosk commands

### Changed

- EMI Hub upgraded to v0.9
- removed sherpa-onnx as the active wake-word gate for the current time-command milestone
- the upstream sherpa-onnx KWS stack has poor recall on very short English keywords; `Emi` / `Emmy` is exactly the kind of short keyword that can fail even after score/threshold tuning
- added a small offline Vosk recognizer on the Raspberry Pi with an intentionally constrained command grammar
- current accepted acoustic forms include `Emmy time`, `Time Emmy`, `Emmy what time is it`, and equivalent time forms
- Vosk's ordinary English spelling `Emmy` is treated as the exact acoustic alias for the robot name EMI
- command execution requires both the wake token and the time token plus a minimum word-confidence floor
- non-command/noise clips fall through to `NO_WAKE_WORD` and do not execute anything
- Whisper remains installed for future free-form timer/reminder text, but it is no longer responsible for recognizing the EMI wake name for this milestone

### Why

- repeatedly tuning a standalone short-keyword detector was the wrong approach for the two-syllable name EMI
- the constrained grammar narrows the recognizer to the exact phrases currently being tested and gives the wake name useful command context
- the goal is now to make `Emi time` / `Time Emi` reliable first, then expand the grammar deliberately

## 2026-10-08 - Add dedicated local EMI keyword spotting

### Changed

- EMI Hub upgraded to v0.8
- wake-word detection no longer depends on Whisper successfully spelling the name `Emi`
- added a local sherpa-onnx open-vocabulary keyword spotter running entirely on the Raspberry Pi
- the keyword spotter checks the in-memory WAV for the explicit EMI wake name before Whisper runs
- if no EMI wake word is detected, the clip is rejected immediately and Whisper is skipped
- after KWS confirms EMI, Whisper only has to recover the command words; it may return `time` without also spelling the wake word correctly
- added a local venv/model installer for the small English int8 keyword-spotting model
- C3 voice diagnostic v3 no longer displays LISTENING/PROCESSING for raw acoustic candidates
- removed the remaining WHISPER and TOO SHORT OLED states so rejected acoustic candidates are fully invisible to the user
- random typing/tapping candidates are now invisible on the OLED unless a real command is accepted
- increased pre-roll to 500 ms, shortened the acoustic candidate delay, and removed 4x PCM gain to reduce wake-word clipping/distortion

### Safety

- raw audio remains in memory during wake detection and transcription
- keyword spotting is deterministic and limited to explicit EMI wake variants; it does not fuzzily authorize commands
- non-wake acoustic events are rejected before command transcription and never become robot actions

## 2026-10-08 - Add real speech VAD before Whisper transcription

### Changed

- local whisper.cpp service now enables its built-in Silero VAD using `ggml-silero-v6.2.0.bin`
- speech gate threshold is set to 0.60 with 180 ms minimum speech and silence windows plus 120 ms speech padding
- Whisper now gets speech-filtered segments instead of blindly transcribing every loud acoustic event
- the installer automatically downloads the official Silero VAD model through whisper.cpp's own model downloader when it is missing
- server startup wait increased to 20 seconds because both Whisper and VAD models now load

### Why

- the ESP32 energy detector can tell that a sound happened, but cannot reliably distinguish voice from typing, tapping, sniffing, or other desk noise
- Silero VAD is now the speech-specific gate on the Pi; only after that gate does Whisper attempt transcription
- deterministic wake-word and command checks remain unchanged after speech recognition

## 2026-10-07 - Remove prompt hallucination risk and allow wake word anywhere

### Fixed

- EMI Hub upgraded to v0.7
- removed the Whisper initial prompt that contained the time command because non-speech sounds such as typing could cause the recognizer to hallucinate the prompted phrase
- the microphone path still requires an explicit exact wake token plus a deterministic command match before any action is queued
- wake-word placement is now flexible: `Emi, what's the time?`, `What's the time, Emi?`, and `Time, Emi` are all valid
- exact wake aliases remain deliberately narrow; fuzzy wake matching is still not allowed

### Safety

- keyboard noise, taps, or other non-speech may still make the VAD upload a clip during diagnostics, but they must not become a TIME action merely because of a Whisper prompt
- restarting the hub clears any stale queued test commands

## 2026-10-07 - Improve wake-word recognition and Whisper latency

### Changed

- EMI Hub upgraded to v0.5
- Whisper requests now include a short prompt that biases recognition toward the proper name Emi and the current time-command phrasing
- added exact deterministic wake aliases for common tiny.en spellings
- disabled timestamp generation and token timestamps for command transcription
- enabled non-speech token suppression for short voice commands
- hub now measures Whisper request duration in milliseconds without logging the transcript
- the VAD itself is left unchanged because the latest physical test shows it is now triggering at the correct time

## 2026-10-07 - Add isolated end-to-end voice test firmware

### Added

- added `firmware/tests/c3_voice_e2e_test/c3_voice_e2e_test.ino` for the first reliable physical voice test
- temporarily removes normal personality behavior and background command polling so microphone/VAD behavior can be measured without unrelated network activity
- connects Wi-Fi first, then calibrates the microphone after the radio has settled
- requires roughly 240 ms of sustained speech-like energy before recording, rather than reacting to short spikes
- keeps a RAM-only pre-roll and short command recording
- uploads directly to the authenticated `/device/audio` endpoint
- classifies the hub result on Serial without printing the transcript
- shows the returned time directly on the OLED when the TIME intent is matched

### Why

- the previous integrated VAD still produced false speech detections in silence
- this diagnostic isolates microphone, VAD, upload, Whisper, wake-word parsing, and time intent before the tuned detector is merged back into normal EMI firmware

## 2026-10-07 - Replace false-triggering VAD with sustained speech detection

### Fixed

- the additive-threshold VAD could trigger on ordinary room-noise spikes even when nobody was speaking
- replaced single-block speech triggering with an approximately 128 ms rolling activity window
- require three consecutive above-threshold window decisions before recording
- made the learned noise floor fall faster when the room gets quieter but rise only very slowly, so speech does not redefine silence
- added a separate lower release threshold and roughly half a second of quiet to end a phrase
- added a 1.2 second post-command cooldown to avoid immediate retriggers from the previous phrase or Wi-Fi activity
- voice capture now waits for Wi-Fi to be connected before it can trigger
- ESP32 Serial diagnostics now classify the Pi response as TIME, no wake word, no speech, or unknown command without printing the transcript

## 2026-10-07 - Tune VAD from real microphone measurements

### Fixed

- first physical voice test proved the microphone and Wi-Fi were alive but the VAD threshold was far too high
- observed noise floor was roughly 4k-6k while live levels reached only around 7k, but the original threshold was about 12k-19k
- changed the trigger from a 3x noise-floor multiplier to an additive margin: learned noise floor + 900, with a 4500 minimum
- this keeps the adaptive room calibration while making normal speech capable of crossing the trigger

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
