# EMI Development Log

This is the less-formal companion to `CHANGELOG.md`.

The changelog records what changed in the code. This file records **what actually happened while building EMI**: wrong wires, driver nonsense, hardware migrations, tiny victories, cursed prototypes, and roughly how long milestones took.

Timing is approximate unless a session was explicitly timed. The goal is an honest engineering diary, not fake precision.

## Project stats

| Stat | Current value |
| --- | --- |
| **Exact tracked development time** | **01:45:09 tracked so far** |
| **Current exact session** | **ACTIVE - 01:45:09 at latest checkpoint** |
| **Legacy work before exact tracking** | ~2 hours estimated from the start of the project; not included in the exact total |
| **Completed exact sessions** | 0 |
| **Current controller** | ESP32-C3 Super Mini |
| **Previous controller** | ESP32 DevKit V1 |
| **Verified hardware** | SH1106 OLED, TTP223 touch, I2S microphone |
| **Current physical form** | Cable Engineering Edition |
| **Current software milestone** | Authenticated Pi -> C3 command bridge implemented; hardware test pending |
| **Next major milestone** | Validate physical Pi -> C3 clock trigger, then stream mic audio in RAM |
| **Last updated** | 2026-10-07 |

> **Privacy-safe exact-time rule:** the public dev log records only durations, never the clock time when work started or ended. During an active build session, the running elapsed duration is checkpointed here so the tracker does not misleadingly show zero. Exact timestamps may be used privately to calculate the duration, but only the resulting duration is published. The older ~2-hour estimate remains legacy history and is never mixed into the exact total.

## Exact session log

| Session | Exact duration | Notes |
| --- | --- | --- |
| 1 | ACTIVE - 01:45:09 at latest checkpoint | Exact tracking enabled; session is still running |

## Backstory - before the tracked C3 migration session

**Time tracking:** not recorded.

EMI started as a breadboard prototype around a full-size **ESP32 DevKit V1**. That board was deliberately oversized: it was easy to plug into a breadboard, easy to reach with jumper wires, and much less annoying while the basic behavior was still changing constantly.

The first useful physical stack became:

- ESP32 DevKit V1
- SH1106 128x64 OLED
- TTP223 capacitive touch sensor
- I2S MEMS microphone
- a truly unreasonable quantity of jumper wires

The OLED face came first. Then the eyes stopped being a looping animation and started getting behavior: irregular blinking, gaze changes, curiosity/contentment, reduced repetition, touch recognition, petting sessions, and small attention bids.

At this point EMI was already recognizably EMI, but his brain was still a board roughly the size of his future torso.

### The microphone fake-out

The microphone looked dead during early I2S tests. Software was changed, levels were inspected, channel settings were questioned.

Then the important discovery: **the microphone header pins were only pushed through the PCB holes and had never actually been soldered.**

That is, in hindsight, an extremely efficient way to build a wireless microphone.

The header was soldered properly, and microphone testing resumed.

---

## 2026-10-07 - The C3 migration day

**Tracked session:** roughly 40-50 minutes of hands-on testing/debugging from the first post-solder mic checks through the compact C3 running the real face and clock command. This is a wall-clock estimate from the session timeline, not a stopwatch measurement.

### 1. Prove the microphone on the big ESP32

**Rough time:** 5-10 minutes.

We deliberately tested the newly soldered microphone on the known-good ESP32 DevKit V1 first. The idea was simple: do not introduce the C3 as a second unknown until the mic itself is proven.

Prototype mic map:

| Mic | ESP32 DevKit V1 |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| L/R | GND |
| WS | GPIO 25 |
| SCK | GPIO 26 |
| SD / SA | GPIO 32 |

The first test displayed microphone level and peak values on the OLED.

There was one brief wiring accident where L/R ended up on 3.3 V. Once L/R was returned to GND, the readings were very large and ugly - but they clearly increased when speaking.

That was enough. The mic was alive.

**Result:** microphone, solder joints, and I2S data path validated.

### 2. Windows decided COM11 no longer deserved a driver

**Rough time:** 5-10 minutes.

The mic test compiled, but upload failed because COM11 had vanished.

Device Manager showed:

> Silicon Labs CP210x USB to UART Bridge (COM11)  
> Code 28

Windows had lost the CP210x driver.

Windows Update did not fix it, so the Silicon Labs CP210x Universal Windows Driver was installed manually through `silabser.inf`.

After reconnecting the board, the yellow warning triangle disappeared and COM11 worked again.

**Result:** big ESP32 upload path restored.

### 3. Meet the tiny C3 - and apparently Ozobot

**Rough time:** about 10 minutes.

The ESP32-C3 Super Mini was connected by itself and appeared as COM10.

Arduino was set to:

- board: `ESP32C3 Dev Module`
- port: COM10

Then Windows/Arduino produced one of the better hardware-development moments of the day and labelled the port:

> ESP32 Family Device, Ozobot DRVKit

The board was not, in fact, an Ozobot.

After some board/port picker confusion, the correct `ESP32C3 Dev Module` selection and COM10 were used. An empty sketch uploaded successfully.

**Result:** C3 USB/upload/reset path validated.

### 4. First C3 OLED

**Rough time:** a few minutes.

The OLED moved from the DevKit to the C3:

| OLED | ESP32-C3 |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO 8 |
| SCL | GPIO 9 |

The dedicated OLED test displayed `EMI C3 OLED TEST` and `HELLO`.

This was the first moment the compact controller actually looked viable instead of theoretical.

**Result:** OLED on GPIO 8/9 validated.

### 5. Full compact IO test

**Rough time:** 5-10 minutes.

Touch and microphone were added:

| Function | ESP32-C3 |
| --- | --- |
| OLED SDA | GPIO 8 |
| OLED SCL | GPIO 9 |
| Touch OUT | GPIO 10 |
| Mic SCK / BCLK | GPIO 4 |
| Mic WS / LRCLK | GPIO 5 |
| Mic SD / SA | GPIO 6 |
| Mic L/R | GND |

A deliberately ugly diagnostic firmware showed giant eyes, touch state, and a live mic meter.

It was visually criminal.

It also proved **OLED + touch + mic all worked at the same time on the C3**, which was the only thing that mattered.

**Result:** the C3 pin map graduated from "planned" to **physically verified**.

### 6. Real EMI moves to the C3

The diagnostic face was immediately retired with prejudice.

The normal EMI behavior was ported to the compact controller:

- smooth eyes
- gaze/idle behavior
- natural blinking
- curiosity/contentment
- recent-action memory
- touch/petting
- attention bids
- lower OLED contrast

The microphone stayed connected and verified, but was intentionally left out of the normal face loop until the speech pipeline is built.

At this point the full-size DevKit stopped being the primary EMI controller.

**Result:** EMI's actual brain is now the ESP32-C3 Super Mini.

### 7. Mechanical engineering department: jumper cables

There is no 3D printer yet.

The temporary body is therefore:

- phone stand
- breadboard
- OLED
- C3
- touch sensor
- microphone
- enough jumper wires to qualify as load-bearing structure

This configuration is officially known as the **Cable Engineering Edition**.

It is temporary. It is ridiculous. It works.

### 8. First command-driven clock

The next goal became very specific:

> "EMI, what time is it?"

Before adding speech recognition, the display-side behavior was separated into a deterministic command:

`SHOW_TIME HH:MM`

Example:

`SHOW_TIME 14:37`

The C3 handles the visual behavior itself:

1. eyes close
2. eyes collapse into thin bars
3. a large custom 7-segment clock reveals from the center
4. the clock is held briefly
5. the clock collapses back into eye-lines
6. eyes reopen

Touching EMI during the clock dismisses it and returns directly to interaction.

The initial 4.5-second clock hold felt too long. It was reduced to **2.0 seconds**, making the whole thing feel like a quick glance instead of opening an app.

**Result:** the screen-side half of "what time is it?" exists.

---

## 2026-10-07 - Teaching the time parser to loosen up

The first hub parser was intentionally strict, but it turned out to be too literal. It understood full phrases like "Emi, can you tell me the time?" but rejected perfectly human requests like "Yo Emi time" and "Yo Emi time is?"

EMI Hub v0.2 now strips harmless greeting/wake-word filler and then checks the remaining phrase against a deterministic set of accepted time requests.

**Result:** casual time requests now map to the same safe `TIME` intent.

---

## 2026-10-07 - Local Whisper works on the Pi 400

The Raspberry Pi 400 built current `whisper.cpp` successfully with the ARM CPU backend and BLAS support.

The `tiny.en` model transcribed the bundled 11-second JFK sample correctly.

Measured benchmark:

- source audio: 11.0 seconds
- wall-clock inference command: 4.411 seconds
- whisper-reported processing total: 4.207 seconds
- roughly 2.5x faster than real time for this sample

That is comfortably fast enough for the first short command milestone.

### EMI Hub v0.1

A tiny Raspberry Pi service was added under `hub/`.

For now it intentionally binds only to localhost and accepts recognized text, not audio. It maps a small allow-listed group of time questions to the deterministic `TIME` intent and returns `SHOW_TIME HH:MM`.

Examples include:

- "Emi, what time is it?"
- "Emi, what's the time?"
- "Emi, can you tell me the time?"
- "Emi, tell me the time"
- "Emi, time?"

The service does not log the original sentence.

**Result:** local STT is proven and the text-to-time-command half of the Pi hub now exists.

---

## 2026-10-07 - The Pi can finally hand EMI a command

The project now has its first real Pi-to-robot transport instead of manually typing `SHOW_TIME` into Serial Monitor.

The hub queues the display command, while the C3 makes an authenticated outbound request for pending work. This was chosen instead of making the Pi chase the C3's DHCP address. It also points in the same direction as the next feature: microphone audio travelling from the C3 to the Pi.

The networking runs in a separate FreeRTOS task so a sleepy network connection should not deliberately turn EMI's eye animation into a slideshow.

Private Wi-Fi credentials and the device token live in local files excluded from Git.

**Result:** after local configuration and flashing, the existing curl time test should make the physical OLED perform the clock morph without Serial Monitor.

---

## 2026-10-07 - Wi-Fi tried to reconnect while it was already reconnecting

The first C3 network build produced a repeating ESP-IDF message:

> `wifi:sta is connecting, return error`

The cause was our own retry loop: the C3 started a Wi-Fi connection, then called `WiFi.reconnect()` every few seconds even while the first attempt was still active.

The network task now owns the full connection lifecycle. It waits up to 15 seconds for an attempt, cleanly resets a timed-out attempt, pauses briefly, and then starts a new one.

**Result:** no more self-inflicted reconnect spam; the next test can distinguish a real SSID/password/network problem from a retry-loop bug.

---

## 2026-10-07 - Router says 2.4 GHz exists, so now we ask the C3 directly

The Pi confirmed that the same SSID is being broadcast on both 2.4 GHz and 5 GHz, with the 2.4 GHz copy on 2412 MHz and a very strong signal.

That means the router is not simply "5 GHz only". A dedicated C3 scan diagnostic was added so the next test can answer the only useful question left at this layer: **can the ESP32-C3 itself see that SSID?**

If it can see the SSID, the problem moves to authentication/configuration. If it cannot, the problem is radio compatibility or router settings.

---

## Current state

EMI now has:

- compact ESP32-C3 controller
- verified OLED
- verified capacitive touch
- verified I2S microphone
- normal expressive face/personality firmware
- deterministic `SHOW_TIME HH:MM` command
- eyes-to-clock-to-eyes morph
- temporary Cable Engineering Edition chassis

## Next milestone

Make this happen for real:

> User: "EMI, what time is it?"  
> Mic -> local speech recognition -> deterministic time intent -> current time -> `SHOW_TIME HH:MM` -> clock morph

The Raspberry Pi will do the heavier local speech processing. The C3 remains responsible for the physical face, touch, microphone endpoint, and display animations.

---

## Running joke counter

- Unsoldered headers mistaken for a software problem: **1**
- Windows driver disappearances: **1**
- ESP32 boards accidentally becoming Ozobots: **1**
- Diagnostic faces immediately declared awful: **1**
- Structural jumper cables: **many**
