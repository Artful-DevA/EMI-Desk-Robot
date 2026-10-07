# Roadmap

## P1 - Physical breadboard EMI

Goal: make EMI visibly alive, touch-responsive, and reliable on the compact controller.

- [x] OLED working
- [x] expressive eyes
- [x] naturalized idle gaze
- [x] less repetitive behavior
- [x] curiosity/contentment drives
- [x] natural blinking
- [x] TTP223 touch input
- [x] slow-pat interpretation
- [x] distinct petting behavior
- [x] validate I2S microphone
- [x] migrate from ESP32 DevKit V1 to ESP32-C3 Super Mini
- [x] validate OLED + touch + microphone simultaneously on C3
- [x] move OLED SCL from GPIO 9 to GPIO 7
- [ ] add SG90 head movement safely

## P2 - Networked EMI

Goal: make the Raspberry Pi reliably control low-risk physical behaviors over the LAN.

- [x] join home 2.4 GHz Wi-Fi
- [x] diagnose C3 authentication failure
- [x] lock current board to working 8.5 dBm TX power
- [x] scan and select the visible 2.4 GHz AP
- [x] authenticated Raspberry Pi device-command endpoint
- [x] C3 background polling task
- [x] FreeRTOS queue between network task and face loop
- [x] deterministic `SHOW_TIME HH:MM` command
- [x] physical Pi -> C3 -> OLED command proven end-to-end
- [ ] desktop agent
- [ ] laptop agent
- [ ] safe allow-listed desktop commands
- [ ] local TTS / selected speaker output
- [ ] stronger encrypted transport before privileged actions

## P3 - Listening EMI

Goal: turn the already-verified microphone into a private local speech path.

- [x] electrically validate I2S microphone
- [x] build `whisper.cpp` on Raspberry Pi
- [x] benchmark `tiny.en` faster than real time on test audio
- [x] deterministic Pi intent parser
- [x] localhost-only whisper.cpp server
- [x] authenticated in-memory Pi WAV ingestion endpoint
- [x] require `Emi` wake address for microphone-triggered intents
- [ ] capture short C3 microphone frames into RAM
- [ ] stream audio C3 -> Pi
- [ ] keep raw audio entirely out of disk storage
- [ ] feed in-memory audio into local Whisper
- [ ] wake/request flow for "Emi..."
- [ ] push-to-talk mode
- [ ] explicit always-listen mode
- [ ] visible listening state

## P4 - Time, timers, and useful display states

- [x] custom 7-segment clock
- [x] quick 2-second clock glance
- [x] clean eyes -> clock transition
- [x] clean clock -> eyes return without flat-line artifact
- [x] align HH and MM with the normal eye positions
- [x] keep the colon visually stable during the short clock display
- [ ] timer engine on the Pi
- [ ] multiple timers
- [ ] small readable timer overlay
- [ ] reminders
- [ ] time-of-day dimming

## P5 - Personality and movement

- [x] deterministic local face behavior
- [x] curiosity
- [x] contentment
- [x] recent-action anti-repetition
- [x] attention bids
- [x] petting response
- [ ] explicit broader behavior state machine
- [ ] listening state
- [ ] thinking state
- [ ] confused state
- [ ] sleepy/rest state
- [ ] SG90 head movement language
- [ ] bounded time-of-day/rest behavior

## P6 - Local memory and notes

- [ ] SQLite/local structured database
- [ ] explicit persistent-memory command
- [ ] mandatory `confirm save`
- [ ] explicit delete command
- [ ] mandatory `confirm delete`
- [ ] true deletion from usable indexes/caches
- [ ] note mode
- [ ] Markdown note output
- [ ] optional Obsidian vault integration
- [ ] no ordinary-conversation transcript storage
- [ ] memory inspection/export tools

## P7 - Desktop / laptop / phone integration

- [ ] Ubuntu desktop agent
- [ ] Ubuntu laptop agent
- [ ] simultaneous agent connectivity
- [ ] active target selection
- [ ] system audio-output awareness
- [ ] media control
- [ ] volume
- [ ] current song
- [ ] CPU/system status
- [ ] approved app launching
- [ ] CamTune integration
- [ ] phone companion app
- [ ] BLE setup/provisioning
- [ ] phone as travel gateway
- [ ] phone as optional speaker
- [ ] Tailscale travel path

## P8 - Final physical EMI

- [x] compact ESP32-C3 controller selected and validated
- [x] active pin map validated
- [ ] final power distribution
- [ ] servo capacitor
- [ ] microphone mounting
- [ ] touch-band placement
- [ ] serviceable internal wiring
- [ ] critical enclosure test prints
- [ ] final print and assembly

## P9 - Optional upgrades

- [ ] onboard speaker
- [ ] MAX98357 amplifier
- [ ] battery operation
- [ ] physical microphone kill switch
- [ ] additional local integrations
