# EMI Development Log

This is my build diary for EMI. Unlike `CHANGELOG.md`, which is mostly about code changes, this is where I keep the actual story: what I built, what broke, what I tested, what fixed it, and how much active development time I have put into the project.

Timing is exact only from the point where I explicitly started tracking it. Work before that is kept as a separate estimate so I do not pretend to have precision I never recorded.

## Project stats

The previous running total became inaccurate because I treated the second work period as continuously active even though additional breaks were not recorded. I am no longer publishing that inflated number as exact.

| Stat | Current value |
| --- | --- |
| **Verified exact tracked work** | **01:50:48** |
| **Later work period** | **Unverified duration — additional breaks were not captured, so it is not included in an exact total** |
| **Explicitly recorded break** | **01:30:00** |
| **Legacy work before exact tracking** | **~2 hours estimated from the start of the project; kept separate** |
| **Time-tracking status** | **Needs manual reconstruction before another cumulative total is published** |
| **Current controller** | **ESP32-C3 Super Mini** |
| **Previous controller** | ESP32 DevKit V1 |
| **Verified hardware** | SH1106 OLED, TTP223 touch sensor, I2S microphone |
| **Current physical form** | Cable Engineering Edition |
| **Current software milestone** | **Local voice-command pipeline under reliability testing** |
| **Current UI milestone** | **Eye-aligned HH:MM clock with clean return animation** |
| **Next major milestone** | **Make short spoken commands reliable without false interaction from desk noise** |
| **Last updated** | 2026-10-08 |

## Time-tracking correction

| Block | Status | Notes |
| --- | --- | --- |
| Session 1 | **01:50:48 verified** | First tracked work block |
| Recorded break | **01:30:00 excluded** | Explicitly reported break |
| Session 2 | **Duration not trustworthy** | The prior counter assumed uninterrupted work and missed additional breaks |
| **Published exact total** | **01:50:48 verified only** | No cumulative total will include Session 2 until it is reconstructed from reliable information |

---

## Before exact tracking - the first EMI prototype

**Time:** ~2 hours estimated, kept separate from exact tracking.

I started EMI on a full-size **ESP32 DevKit V1** because it was easy to breadboard and easy to debug while I was still figuring out what EMI should feel like.

My first useful physical stack was:

- ESP32 DevKit V1
- SH1106 128x64 OLED
- TTP223 capacitive touch sensor
- I2S MEMS microphone
- a slightly unreasonable amount of jumper wire

The OLED face came first. I did not want EMI to look like a looping GIF, so I gradually added:

- irregular blinking
- quick gaze/saccade movements
- curiosity and contentment values
- recent-action memory to reduce repetition
- several different idle behaviors
- attention bids
- touch recognition
- petting sessions
- partial contented eye-closes
- a deeper relaxed blink after sustained petting

At this point EMI already felt recognizably like EMI, even though the controller was still far too large for the final body.

### The microphone fake-out

The microphone initially looked dead. I changed software, questioned the channel settings, and inspected the readings.

Then I found the actual problem: **the microphone header pins were pushed through the board but had never been soldered.**

After soldering the header properly, the microphone started producing real I2S data.

**Result:** mic hardware and the basic I2S path were proven.

---

# 2026-10-07 - ESP32-C3 migration day

## 1. I proved the microphone before changing controllers

I tested the newly soldered microphone on the known-good DevKit first so I would not introduce two unknowns at once.

The DevKit microphone wiring was:

| Mic | ESP32 DevKit V1 |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| L/R | GND |
| WS | GPIO 25 |
| SCK | GPIO 26 |
| SD / SA | GPIO 32 |

I briefly had L/R on 3.3 V by mistake. Once I put it back on GND, the level readings reacted clearly to speech and nearby sounds.

**Result:** microphone validated before the C3 migration.

## 2. Windows lost the DevKit serial driver

The mic firmware compiled, but COM11 disappeared.

Device Manager showed the CP210x bridge with a Code 28 driver problem. I installed the Silicon Labs CP210x driver manually and the port came back.

**Result:** DevKit upload path restored.

## 3. I brought up the ESP32-C3 Super Mini

I connected the C3 by itself and used:

- board: **ESP32C3 Dev Module**
- its Windows COM port

Windows/Arduino briefly labelled it as an **Ozobot DRVKit**, which it definitely is not.

A minimal empty sketch uploaded successfully.

**Result:** C3 USB/upload/reset path validated.

## 4. I moved the OLED to the C3

The first C3 OLED test used:

| OLED | ESP32-C3 |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 8 |
| SCL | GPIO 9 initially |

The OLED displayed a simple validation screen successfully.

**Result:** OLED worked on the compact controller.

## 5. I validated all of the important I/O together

I connected OLED, touch, and microphone at the same time and ran a deliberately ugly diagnostic firmware.

The original validated C3 test map was:

| Function | ESP32-C3 |
| --- | --- |
| OLED SDA | GPIO 8 |
| OLED SCL | GPIO 9 initially |
| Touch OUT | GPIO 10 |
| Mic SCK / BCLK | GPIO 4 |
| Mic WS / LRCLK | GPIO 5 |
| Mic SD / SA | GPIO 6 |
| Mic L/R | GND |

The OLED, touch input, and live mic meter all worked simultaneously.

**Result:** the C3 was capable of running EMI's full low-power I/O stack.

## 6. I moved normal EMI onto the C3

I replaced the diagnostic UI with the real face again and ported the normal behavior:

- expressive eyes
- smooth gaze
- natural blinking
- curiosity/contentment
- recent-action anti-repetition
- idle behavior sequences
- attention bids
- touch/petting
- relaxed blink behavior

The microphone stayed physically connected and verified, but I intentionally kept speech processing out of the main face loop until the Pi speech pipeline is ready.

**Result:** ESP32-C3 Super Mini became EMI's real controller.

---

# Building the first useful command: "what time is it?"

I wanted the first speech feature to be small, deterministic, and actually useful.

Instead of jumping directly into speech recognition, I created a display-side command:

`SHOW_TIME HH:MM`

The C3 validates the 24-hour time, then handles the visual response locally.

The first animation became:

1. eyes close
2. face transitions into a custom 7-segment time display
3. time holds briefly
4. clock disappears
5. eyes return

I reduced the hold from 4.5 seconds to **2 seconds** because the longer version felt like opening an app instead of glancing at the time.

Touch can cancel the clock and return EMI to normal interaction.

**Result:** the physical display-side endpoint for "what time is it?" existed before speech was connected.

---

# Raspberry Pi speech backend

## Local Whisper

I built `whisper.cpp` on the Raspberry Pi 400 and tested the `tiny.en` model on its bundled JFK sample.

Measured result:

- sample length: about 11 seconds
- total processing: about 4.2-4.4 seconds
- roughly 2.5x faster than real time for that sample

That was fast enough for short command recognition.

## EMI Hub

I built a small local Raspberry Pi service that turns recognized text into deterministic intents.

For time requests, it accepts natural variants such as:

- "Emi, what time is it?"
- "Emi, tell me the time"
- "Yo Emi time"

The parser is deliberately deterministic. It does not let a fuzzy classifier directly trigger privileged behavior.

**Result:** local text -> safe intent -> `SHOW_TIME HH:MM` was working on the Pi.

---

# Raspberry Pi -> EMI network bridge

I added the first real Pi-to-robot command transport.

The Raspberry Pi hub queues an allow-listed display command. EMI makes an outbound authenticated request to the Pi approximately four times per second and receives pending work.

Important properties:

- credentials stay in private `secrets.h`
- shared token stays out of Git
- device commands are allow-listed
- network work runs in a separate FreeRTOS task
- the face loop keeps running independently
- current HTTP transport is only for the trusted home LAN prototype

This direction also fits the future speech pipeline because microphone audio will travel C3 -> Pi.

---

# The Wi-Fi debugging saga

This became by far the most annoying part of the day.

## First problem: my reconnect logic fought itself

The first network firmware repeatedly printed that the station was already connecting.

The retry loop was starting new reconnects while an existing connection attempt was still active.

I changed the network task so it owns the full connection lifecycle:

- one connection attempt at a time
- 15-second timeout
- clean reset
- short pause
- retry

**Result:** the software stopped creating its own Wi-Fi failure.

## I proved the network was really visible

The router broadcasts the same network on both 2.4 GHz and 5 GHz.

The C3 scan could see the correct **2.4 GHz** AP on **channel 1** with a strong signal.

So this was not a "C3 cannot use 5 GHz" mistake and it was not a missing network.

## Uploading the C3 also became unreliable

At one point `esptool` could not get a serial handshake even though Windows still showed the USB serial device.

The recovery sequence that actually worked was:

1. unplug USB
2. hold BOOT
3. plug USB in while still holding BOOT
4. keep holding for a few seconds
5. release BOOT
6. reselect the COM port
7. upload

That became the reliable recovery path.

## I added exact Wi-Fi diagnostics

Instead of trusting Arduino's generic disconnected status, I added event logging for the underlying ESP-IDF disconnect reason.

Then I made a two-stage test:

1. normal connection
2. direct connection to the exact scanned 2.4 GHz channel and BSSID

Both failed with:

`AUTH_EXPIRE (2)`

That proved the C3 could see the correct AP, but authentication itself was timing out.

## Router settings did not solve it

I forced the 2.4 GHz side to:

- channel 1
- 20 MHz width

The exact-BSSID test still returned `AUTH_EXPIRE`.

That made a generic router-channel problem much less convincing.

## The actual fix: 8.5 dBm Wi-Fi TX power

I tested lower transmit powers on the ESP32-C3 Super Mini.

At **8.5 dBm**, the board finally did this:

- associated with the AP
- obtained a LAN IP
- stayed connected

This was the breakthrough.

The same SSID, password, channel, and AP that failed before immediately worked when the C3 transmit power was reduced.

The normal firmware now permanently applies:

`WIFI_POWER_8_5dBm`

**Result:** EMI finally had real working Wi-Fi.

---

# I moved OLED SCL away from the BOOT strap

The OLED originally used GPIO 9 for SCL. GPIO 9 is also an ESP32-C3 BOOT strapping pin, which was a bad combination while I was already fighting uploads.

I moved OLED SCL to **GPIO 7**.

The current verified map is now:

| Function | ESP32-C3 Super Mini |
| --- | --- |
| OLED SDA | GPIO 8 |
| OLED SCL | **GPIO 7** |
| Touch OUT | GPIO 10 |
| Mic SCK / BCLK | GPIO 4 |
| Mic WS / LRCLK | GPIO 5 |
| Mic SD / SA | GPIO 6 |
| Mic L/R | GND |

GPIO 9 is deliberately left free for BOOT/download mode.

**Result:** current breadboard wiring no longer puts the OLED on the C3 BOOT strap.

---

# The first complete physical Pi -> EMI command worked

After the Wi-Fi fix, I ran the real intent request on the Pi.

The chain was:

**text request -> Pi intent parser -> command queue -> authenticated Wi-Fi polling -> ESP32-C3 -> physical OLED**

And it worked.

EMI actually changed from his face into the current time and then returned to his face.

This was the first full end-to-end proof that the Raspberry Pi can make the physical robot do something over the network.

**Result:** authenticated Pi -> physical EMI command path proven.

---

# Clock UI polish

The first working version was functional but visually wrong.

## The return-animation line

When the eyes came back after the time, the transition briefly produced an ugly horizontal line.

I removed the temporary flat return bars and changed the eyes so they reopen from a rounded partially-open shape instead.

## The time layout

The first clock felt like a generic four-digit display that happened to be on EMI's screen.

I changed the geometry so the time now belongs to the face:

- the **hour pair is centered on the normal left-eye position**
- the **minute pair is centered on the normal right-eye position**
- both groups use the normal eye center line
- digits are slightly smaller
- the colon is centered between the two groups
- the colon stays visible during the short hold

That made a very noticeable difference. The clock now feels like **EMI's eyes becoming the time**, rather than the face being replaced by a dashboard.

I also hit one compile error while experimenting with a standalone clock snippet because that code referenced the OLED `display` object before it had been declared. The full canonical firmware fixes this by keeping all display drawing after the global U8g2 object exists.

**Result:** current clock UI is dramatically cleaner and compiles as part of the complete firmware.

---

# 2026-10-07 - I started the real voice-control path

The time command already proved the output side, so I started connecting the input side instead of adding more fake/manual commands.

I added an authenticated `/device/audio` endpoint to the Raspberry Pi hub. The endpoint accepts a short 16 kHz WAV from EMI, keeps the bytes in RAM, and forwards them to a localhost whisper.cpp server.

I deliberately kept whisper.cpp's `--convert` mode disabled. The current server can decode the uploaded WAV directly from memory, which means I do not need to create temporary microphone recordings on disk.

The voice path now has an extra safety gate too: a transcript arriving from the microphone must actually address **Emi** before the deterministic intent parser is allowed to execute anything. Random background speech saying something like "what time is it?" by itself should therefore be ignored.

I also added a dedicated user service for the already-built `whisper-server`, bound only to localhost on port 17841 and using the existing tiny.en model.

At this point the Raspberry Pi side of live voice is ready. The remaining missing piece is the C3 side: continuously read the already-validated I2S mic, detect a short spoken phrase, keep it in RAM, and POST it to `/device/audio`.

**Result:** voice control is no longer just a diagram; the Pi now has a real private in-memory audio ingestion/transcription endpoint.

---

# 2026-10-07 - Whisper service startup fix

My first systemd service for `whisper-server` exited immediately after printing its help text. The binary itself was fine; the service argument set was simply more ambitious than necessary for the installed build.

I reduced the service to the minimum arguments EMI actually needs right now: model path, localhost bind address, and port. I also changed the installer so it waits for the Pi to load the model instead of assuming the HTTP server will be ready after exactly one second.

If the server exits early now, the installer prints the full untruncated service status, which should make any future command-line compatibility problem obvious.

**Result:** the Whisper service setup is simpler and easier to diagnose before I touch the working C3 firmware.

---

# 2026-10-07 - I put the microphone into normal EMI firmware

The Raspberry Pi side was finally proven healthy: EMI Hub v0.4 answered its health endpoint, the Whisper service stayed active, and the bundled JFK sample transcribed successfully through the local HTTP inference endpoint.

With that foundation working, I moved to the real robot.

The normal ESP32-C3 firmware now initializes the already-verified I2S microphone and runs a small local voice-activity detector. It calibrates against the room noise, keeps a short rolling pre-buffer in RAM so the beginning of "Emi..." is not chopped off, and records a short spoken phrase when the level rises above the adaptive threshold.

The captured command stays in RAM. The C3 writes only an in-memory WAV header around that PCM data and sends it to the authenticated `/device/audio` endpoint on the Pi. There is no microphone filesystem path on the C3.

I also added a shared HTTP mutex so the normal command-polling request does not collide with a voice upload. While Whisper is working, command polling simply waits; the face animation remains independent.

For privacy visibility, I added a tiny status dot in the top-right of the OLED:

- hollow: voice monitoring/VAD armed
- filled: capturing or waiting for the voice request to finish

The first version intentionally caps a command at about three seconds. That is enough for "Emi, what's the time?" without spending too much of the C3's RAM. Longer timer/reminder language will need a better streaming or chunked design once the first live command is proven.

**Result:** the full software path for the first hands-free command now exists. The next step is to flash this firmware and tune the VAD/audio level from the real microphone if needed.

---

# 2026-10-07 - I made the first voice test easier to tune

Before flashing, I added low-rate Serial diagnostics for the live VAD. Once per second EMI now reports the current microphone level, learned room-noise floor, and speech trigger threshold. That means if the first test is too sensitive or not sensitive enough, I can tune it from real measurements instead of guessing.

I also accounted for a likely speech-recognition detail: Whisper may spell the robot's spoken name as "Emmy" even when I mean "Emi". The hub now accepts exactly `emi` or `emmy` as wake addresses. It is still deterministic and does not use fuzzy matching.

**Result:** the first spoken-command test should now tell me exactly whether a failure is VAD, upload, transcription, wake-word parsing, or the existing command path.

---

# 2026-10-07 - The Pi voice backend is fully green

I confirmed both services are now healthy at the same time:

- `emi-whisper.service` is active with tiny.en loaded
- `emi-hub.service` is active as v0.4
- `/health` returns HTTP 200 and reports the local Whisper endpoint
- the Whisper HTTP endpoint successfully transcribes the bundled JFK sample

That means I can stop touching the Raspberry Pi backend for this milestone and move to the physical microphone upload test on EMI himself.

**Result:** Raspberry Pi voice backend validated end-to-end; next action is flashing the new C3 voice firmware.

---

# 2026-10-07 - I caught one compile issue before flashing voice control

When I went back to hand over the new voice firmware, I spotted a formatting mistake in the include section: the FreeRTOS semaphore include had been written with a literal escaped newline instead of as its own real preprocessor line.

I fixed that in the canonical firmware before flashing. The voice behavior itself did not change.

**Result:** the current GitHub firmware is the corrected voice-control build I should actually flash.

---

# 2026-10-07 - My first live mic test exposed the VAD threshold problem

I flashed the voice-enabled firmware and the important foundations worked: the microphone initialized, the C3 measured real changing audio levels, Wi-Fi connected, and the normal network path stayed alive.

The voice command itself did not trigger. The Serial data made the reason obvious instead of mysterious: my learned room/noise floor was roughly in the 4k-6k range and live levels were reaching roughly 7k, while the VAD threshold was sitting around 12k-19k because I had initially multiplied the noise floor by three.

That meant ordinary speech simply could not cross the threshold.

I changed the VAD to use an additive margin instead of a huge multiplier. The new trigger is approximately:

`learned noise floor + 900`

with a minimum threshold of 4500.

This is based on the real microphone measurements from the physical robot rather than guessed constants.

**Result:** mic input, Wi-Fi, and the backend are alive; the next flash specifically tests whether speech now reaches `Voice: speech detected.`

---

# 2026-10-07 - The first VAD retune was too sensitive

My first threshold correction went too far in the other direction. EMI started declaring speech when nobody was speaking because a single noisy microphone block could jump above the new threshold.

I replaced that simplistic trigger instead of moving the number up and down again.

The new detector looks at a rolling short-term average over about 128 ms and requires several consecutive above-threshold decisions before it starts recording. I also split the start and release thresholds, made the learned room-noise floor adapt asymmetrically, wait for Wi-Fi before accepting a voice trigger, and added a cooldown after each command.

I also added safe response diagnostics on the ESP32. After a voice upload it can now tell me whether the Pi matched the TIME intent, heard speech without the Emi wake word, heard no speech, or heard an unknown command. It still does not print or store the actual transcript.

**Result:** this version should stop reacting to isolated noise spikes and, if the command still fails, the Serial output will identify the exact stage instead of just saying HTTP 200.

---

# 2026-10-07 - I split voice control into a clean end-to-end diagnostic

The integrated voice build was still falsely detecting speech while the room was quiet. Instead of continuing to change one threshold inside the full personality/network firmware, I made a dedicated end-to-end voice test.

This test keeps the real microphone, the proven 8.5 dBm Wi-Fi workaround, the authenticated Pi audio endpoint, local Whisper, wake-word parsing, and the time intent, but temporarily removes normal personality behavior and background command polling.

The biggest change is the order of operations: I now connect Wi-Fi first, let the radio settle, and only then calibrate the microphone. That matters because radio activity and power noise can contaminate the baseline when calibration and Wi-Fi startup happen at the same time.

The detector also requires about 240 ms of sustained speech-like energy before it starts recording. Short electrical or room-noise spikes should no longer count as speech.

If the Pi matches the TIME intent, the test firmware displays the returned HH:MM directly on the OLED. If it fails, Serial tells me whether the failure was no speech, no Emi wake word, no command match, or HTTP/backend failure without logging the actual transcript.

**Result:** I now have a much cleaner test that can identify the first broken stage without the full EMI runtime getting in the way.

---

# 2026-10-07 - The detector is working; speech recognition is now the bottleneck

The latest physical test finally showed the VAD behaving properly: EMI starts and stops recording at the right times instead of constantly false-triggering.

The failure moved downstream. The Pi returned HTTP 200 and recognized that speech existed, but tiny.en did not recognize the wake name reliably. The delay before getting that failure also felt too long.

I left the working detector alone and changed only the Pi side. EMI Hub v0.5 now gives Whisper a short initial prompt containing the name Emi and the current time-command phrasing. I also added a small exact alias set for common recognizer spellings instead of adding fuzzy wake matching.

For latency, command transcription no longer asks Whisper for timestamps or token timestamps, which are unnecessary for a short command. The hub also records only the inference duration in milliseconds so I can measure the delay without logging the recognized transcript.

**Result:** the next test can reuse the same C3 firmware. I only need to update and restart the Pi hub, then try "Emi, what's the time?" again.

---

# 2026-10-07 - Typing accidentally exposed a serious wake-word design mistake

While I was only typing, EMI started showing the time. That is unacceptable: keyboard noise is not a command.

The VAD was hearing the keyboard, but the more important mistake was on the Pi side. I had added a Whisper prompt containing both the name Emi and the time-command wording to improve recognition. On noisy non-speech clips, that prompt could bias tiny.en into hallucinating exactly the command I was trying to detect.

I removed that prompt completely.

I also changed the deterministic wake parser so the explicit wake name may appear anywhere in the phrase instead of only at the beginning. I can now naturally say things like "Time, Emi" or "What's the time, Emi?" while the hub still requires an exact wake token and a known command form.

I kept fuzzy wake matching disabled.

**Result:** non-speech noise no longer gets a built-in hint that can manufacture an EMI time command, and wake-word placement is more natural.

---

# 2026-10-08 - I moved real speech detection onto the Pi

The C3 energy detector proved useful for finding acoustic activity, but it also proved its limitation: typing, tapping, sniffing, and other desk noises can all be loud enough to look like "speech" if I only compare signal energy.

I stopped treating that energy detector as the authority.

I enabled whisper.cpp's built-in Silero VAD on the Raspberry Pi. The C3 can still cheaply notice that something happened and send a short RAM-only clip, but the Pi now has a speech-specific model that decides whether the clip actually contains human speech before Whisper transcription is used.

The service uses Silero v6.2.0 with a slightly conservative threshold, short minimum speech/silence windows, and speech padding so a short phrase such as "Emi time" is not trimmed too aggressively.

The installer now downloads the official VAD model through whisper.cpp's own downloader if it is missing.

**Result:** keyboard clicks and other non-speech sounds no longer have to be solved with increasingly fragile ESP32 loudness thresholds. The Pi now has a real speech/non-speech gate before command transcription.

---

# 2026-10-08 - I stopped asking Whisper to recognize EMI

The latest test made the architectural problem obvious. I could say "EMI time" clearly and the acoustic detector would capture the phrase, but Whisper would often return "no EMI". At the same time, typing could still make the diagnostic screen say LISTENING even though no one had spoken.

Those are two separate problems, and I removed both assumptions.

First, I added a dedicated local keyword spotter on the Raspberry Pi using sherpa-onnx. It is an open-vocabulary KWS model, so I can explicitly configure the wake name EMI without training a custom neural network. The raw WAV stays in RAM. If the keyword spotter does not hear EMI, the hub rejects the clip before Whisper even runs.

If the keyword spotter does hear EMI, Whisper only has to understand the command portion. This means a transcription of just "time" is now enough because the independent acoustic wake gate has already proven that EMI was spoken.

Second, I changed the C3 diagnostic UI. Its cheap energy detector is now correctly treated as only an "acoustic candidate" trigger. It no longer changes the OLED to LISTENING or WHISPER just because typing, tapping, or another loud sound crossed a threshold. Failed/no-wake candidates remain invisible to the user.

I also increased pre-roll to about half a second, made candidate triggering faster, and removed the 4x PCM gain that could hard-clip loud samples.

**Result:** the wake name is no longer hostage to tiny.en spelling, and random desk noise no longer looks like EMI is actively listening.

---

# 2026-10-08 - I stopped tuning the wrong wake-word tool

The dedicated sherpa-onnx keyword spotter still would not reliably hear "EMI" even when I said it clearly. At that point continuing to move thresholds around was just going in circles.

I checked the upstream KWS behavior and found the important limitation: very short English keywords can have poor recall even when boosting score and trigger threshold are tuned aggressively. EMI/Emmy is exactly that kind of short keyword.

I removed that short-keyword detector from the active command path.

For the current milestone I switched to a constrained offline Vosk recognizer on the Raspberry Pi. Instead of trying to detect the isolated name first, it recognizes the whole short command with context, such as "Emmy time" or "Time Emmy". The English acoustic spelling "Emmy" maps deterministically to the robot name EMI.

The recognizer also requires both the wake token and the command token plus a minimum confidence. Random acoustic candidates still do nothing.

Whisper stays available for later free-form reminder/timer text, but it no longer has to recognize the wake name for the basic time command.

**Result:** I am narrowing the test on purpose: make "Emi time" and "Time Emi" reliable first, with no more C3 threshold changes, then expand from a known-good voice-command base.

---

# 2026-10-08 - I corrected the development-time log

I noticed the development-time total had become obviously inflated. The mistake was in the tracking method: after the recorded break, I kept treating the second session as continuously active even though additional breaks were not explicitly captured.

That made the displayed "exact" total false.

I removed the inflated cumulative figure. The only exact active block I can currently defend is the first tracked session at **01:50:48**. The explicitly reported **01:30:00** break remains recorded, while the later work period is now marked unverified instead of pretending it was continuous work.

I will not publish another cumulative exact total until the later period can be reconstructed from reliable timing information.

**Result:** the devlog now distinguishes verified time from uncertain time instead of overstating development hours.

---

# 2026-10-08 - I made longer time phrases stop losing to filler words

The constrained Vosk recognizer was a clear improvement, but I noticed a pattern: the short form "Emi time" worked much better than longer forms such as "Emi, what's the time?", and putting Emi at the end was less reliable.

The problem was in my acceptance rule. I was taking the minimum confidence across every recognized word. That meant a weak filler word such as "what", "is", or "it" could reject the entire command even when the two important words, "Emmy" and "time", were recognized well.

I changed the gate to score the wake token and the time token independently. I also expanded the constrained grammar in both word orders and added a short silence tail before Vosk finalizes the phrase so a wake word spoken at the very end has enough decoder context.

**Result:** short and natural forms should now be much closer in reliability without weakening the rule that both EMI and the time intent must be heard.

---

# 2026-10-08 - I separated the wake word from the command words

The Vosk change was clearly better: "Emi time", "Emi what time is it", and "Emi tell me the time" were working. But the same requests became unreliable when I put EMI at the end, and the contraction in "Emi, what's the time?" was still awkward.

That showed the remaining coupling in the recognizer. I was still asking one constrained grammar to recognize the complete sentence, so word order and sentence shape mattered more than they should.

I split the recognition into two independent local passes over the same RAM-only clip. One pass only checks for the explicit spoken wake name EMI (using Vosk's acoustic spelling "Emmy"). The other independently checks for the word "time".

The command runs only when both gates pass their own confidence thresholds. That means "Emi time", "Time Emi", "What time is it Emi", and "Emi what's the time" no longer need to map to the same full phrase.

**Result:** wake position is now an independent requirement rather than part of one fragile sentence template.

---

# 2026-10-08 - I parallelized the two voice gates

The independent wake-word and time-intent gates fixed the phrase-order problem, but the response felt slower because I was running two complete Vosk recognizers one after another.

Those recognizers do not depend on each other, so I changed the hub to run them concurrently with separate recognizer instances over the same in-memory audio.

I kept the exact same wake and intent thresholds. This is a latency change, not another recognition retune.

**Result:** I keep the v0.11 reliability improvement while removing most of the backend penalty from having two independent gates.

---

# 2026-10-08 - I added the first real timer

I deliberately did not start with the final timer animation. I first made the timer itself real.

The Raspberry Pi hub now owns one active countdown timer in memory. I can say "Emi set a timer for 30 seconds", ask how much time is left, and cancel it. The explicit EMI wake gate still has to pass first.

For timer phrases I added a separate lightweight Vosk intent gate. Once EMI is confirmed and the audio looks timer-related, Whisper recovers the natural wording and duration, but it still cannot directly trigger an action. A deterministic parser extracts seconds/minutes/hours and chooses SET_TIMER, TIMER_LEFT, or TIMER_CANCEL.

The current C3 diagnostic firmware mirrors the countdown locally after the Pi confirms it. That lets it show TIMER DONE at the correct moment without constantly polling HTTP and disrupting microphone capture. It also asks the Pi for timer state once after connecting, so a C3 reset does not automatically destroy a timer that is still alive on the Pi.

The final visual design is intentionally deferred: later the timer will animate into the main clock position, shrink into a corner beside EMI's normal eyes, expand when I ask how much time is left, and get a proper expiry reaction.

**Result:** timer logic works first; personality animation comes after the behavior is proven.

---

# 2026-10-08 - I stopped the C3 from cutting timer sentences in half

When I tried longer timer commands, it felt like EMI stopped listening around the word "timer". The C3 cannot actually know which word I am saying at that point, so I looked at the acoustic capture rules instead of changing the recognizer again.

The voice test was still using the timing that worked for "Emi time": about 480 ms of silence ended a clip, and the hard maximum was only three seconds. That is too aggressive for a sentence such as "Emi set a timer for 30 seconds", especially if I naturally pause after "timer".

I kept the short-command behavior fast, but made the end-of-speech timeout adaptive. Short captures still close after about 480 ms of quiet. Once the capture is clearly a longer phrase, it allows about 830 ms of quiet, and the hard RAM-only maximum is now five seconds.

I also added Serial-only diagnostics that report whether the recording ended because of silence or the maximum duration. They do not log transcripts or save audio.

**Result:** this iteration fixes the capture boundary first without touching wake-word confidence or adding another recognition framework.

---

# 2026-10-08 - I changed recording to follow the actual end of speech

I simplified the C3 endpoint logic. Instead of deciding that short and long phrases need different silence windows, I now keep a timestamp of the most recent block that still looks like voice energy.

Once recording starts, every above-threshold block pushes that timestamp forward. EMI only closes the clip after the microphone has stayed back at the calibrated room-noise level for about 800 ms.

That is much closer to the behavior I actually want: listen while I am talking, tolerate the gaps between words, and stop when I stop talking.

There is still a five-second hard ceiling because the C3 is buffering the WAV in RAM. That ceiling is only a safety fallback for continuous noise; normal commands should end from the audio-stopped detector first.

**Result:** phrase length no longer decides when EMI stops listening.

---

# 2026-10-08 - I found a stale Whisper service install

The Pi showed both services as active, but the running Whisper command line did not include the Silero VAD flags that are present in the repository's current service file. That meant the server itself was alive, but the installed systemd user service was stale.

I fixed the installer so updating EMI Hub also refreshes the Whisper user service first. This keeps the actual process definition aligned with the repository instead of requiring a separate manual Whisper reinstall.

**Result:** future hub updates will no longer silently leave an older Whisper service definition running.

---

# Current state

Right now I have a real networked EMI prototype with:

- ESP32-C3 Super Mini as the active controller
- SH1106 OLED face
- TTP223 petting/touch input
- physically validated I2S microphone
- OLED SCL moved to GPIO 7
- GPIO 9 free for BOOT
- expressive face/personality firmware
- curiosity/contentment behavior
- non-repetitive idle actions
- touch/petting reactions
- attention bids
- deterministic `SHOW_TIME HH:MM`
- eye-aligned clock UI
- clean clock-to-eyes return
- working 2.4 GHz Wi-Fi
- board-specific 8.5 dBm TX-power fix
- authenticated Raspberry Pi command polling
- local Raspberry Pi intent parser
- local `whisper.cpp` proven on the Pi
- successful physical Pi -> EMI time command
- private credentials/token excluded from Git

The temporary physical build is still the **Cable Engineering Edition**: breadboard, wires, OLED, sensor, mic, and C3.

---

# Next milestone

The next real step is the microphone path:

**EMI mic -> C3 RAM buffer -> Wi-Fi -> Raspberry Pi RAM -> local Whisper -> deterministic intent -> command -> EMI**

The privacy requirement is strict:

- raw microphone audio is **never written to disk**
- ordinary conversation is **not saved as transcript**
- only explicit note mode may retain text later

Once that path is working, the existing time feature can become a real spoken interaction instead of a curl command.

---
