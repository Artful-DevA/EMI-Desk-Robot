# Roadmap

## P1 - Breadboard EMI

Goal: make EMI visibly alive and touch-responsive.

- [x] OLED working
- [x] basic eyes
- [x] naturalized idle gaze
- [x] less frequent blinking
- [x] TTP223 touch input
- [x] slow-pat interpretation
- [x] distinct petting behavior
- [ ] validate I2S microphone
- [ ] add SG90 head movement safely

## P2 - Networked EMI

- local Wi-Fi / LAN protocol
- authenticated desktop agent
- event messages from ESP32
- PC speaker playback / local TTS
- safe allow-listed desktop commands
- basic logging

## P3 - Listening EMI

- microphone capture
- send or stream audio to local Raspberry Pi
- local speech-to-text
- push-to-talk / explicit listening state
- no cloud audio dependency

## P4 - Personality

- explicit behavior state machine
- idle
- attentive
- petted
- listening
- thinking
- confused
- sleepy
- expression timing
- head movement language
- short personality-consistent responses

## P5 - Local memory

- SQLite database
- people
- facts
- projects
- memories
- response templates
- explicit remember / forget commands

## P6 - Useful EMI

Possible integrations:

- media control
- volume
- timers
- CPU/system status
- current song
- CamTune
- Linux package-update checking
- GitHub project information
- selected external APIs when explicitly requested

Privileged actions remain narrowly allow-listed.

## P7 - Physical EMI

- final ESP32-C3 pin map
- final power distribution
- servo capacitor
- microphone mounting
- touch-band placement
- serviceable internal wiring
- critical enclosure test prints
- final print and assembly

## P8 - Optional upgrades

- onboard speaker
- MAX98357 amplifier
- battery operation
- physical microphone kill switch
- additional local integrations
