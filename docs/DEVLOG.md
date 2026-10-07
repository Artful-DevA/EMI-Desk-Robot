# EMI Development Log

This is my build diary for EMI. Unlike `CHANGELOG.md`, which is mostly about code changes, this is where I keep the actual story: what I built, what broke, what I tested, what fixed it, and how much active development time I have put into the project.

Timing is exact only from the point where I explicitly started tracking it. Work before that is kept as a separate estimate so I do not pretend to have precision I never recorded.

## Project stats

| Stat | Current value |
| --- | --- |
| **Exact tracked development time** | **03:26:31 tracked so far** |
| **Current exact session** | **ACTIVE - 01:35:43 at latest checkpoint** |
| **Excluded break time** | **01:30:00** |
| **Legacy work before exact tracking** | **~2 hours estimated from the start of the project; not included in the exact total** |
| **Completed exact sessions** | **1** |
| **Current controller** | **ESP32-C3 Super Mini** |
| **Previous controller** | ESP32 DevKit V1 |
| **Verified hardware** | SH1106 OLED, TTP223 touch sensor, I2S microphone |
| **Current physical form** | Cable Engineering Edition |
| **Current software milestone** | **Authenticated Raspberry Pi -> physical C3 command path proven end-to-end** |
| **Current UI milestone** | **Eye-aligned HH:MM clock with clean return animation** |
| **Next major milestone** | **Stream microphone audio to the Pi in RAM and feed local Whisper** |
| **Last updated** | 2026-10-07 |

> **Timing rule:** I publish durations, not private clock timestamps. The 1h30m break I reported is excluded completely from the exact development total. The older ~2-hour estimate stays separate and is never mixed into the exact tracked total.

## Exact session log

| Session | Exact active duration | Notes |
| --- | ---: | --- |
| 1 | **01:50:48** | First tracked work block |
| Break | **01:30:00** | Not development time; excluded from totals |
| 2 | **ACTIVE - 01:35:43** | Work resumed after the break |
| **Total active tracked work** | **03:26:31** | Break excluded |

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

## Running joke counter

- Unsoldered headers mistaken for a software problem: **1**
- Windows driver disappearances: **1**
- ESP32 boards accidentally becoming Ozobots: **1**
- Diagnostic faces immediately declared awful: **1**
- Wi-Fi failures caused by a tiny board needing less transmit power: **1 extremely specific one**
- Structural jumper cables: **many**
