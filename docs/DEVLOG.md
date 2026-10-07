# EMI Development Log

This is the less-formal companion to `CHANGELOG.md`.

The changelog records what changed in the code. This file records **what actually happened while building EMI**: wrong wires, driver nonsense, hardware migrations, tiny victories, cursed prototypes, and roughly how long milestones took.

Timing is approximate unless a session was explicitly timed. The goal is an honest engineering diary, not fake precision.

## Project stats

| Stat | Current value |
| --- | --- |
| **Exact tracked development time** | **00:00:00 completed** |
| **Exact tracking began** | **2026-10-07 14:39:12 +03:00** |
| **Current session** | **ACTIVE - started 2026-10-07 14:39:12 +03:00** |
| **Legacy work before exact tracking** | ~45 minutes documented separately; not included in the exact total |
| **Tracked exact sessions completed** | 0 |
| **Current controller** | ESP32-C3 Super Mini |
| **Previous controller** | ESP32 DevKit V1 |
| **Verified hardware** | SH1106 OLED, TTP223 touch, I2S microphone |
| **Current physical form** | Cable Engineering Edition |
| **Current software milestone** | `SHOW_TIME HH:MM` with eyes-to-clock morph |
| **Next major milestone** | Spoken "EMI, what time is it?" -> local STT -> `SHOW_TIME` |
| **Last updated** | 2026-10-07 |

> **Exact-time rule from this point onward:** no estimated durations are added to the exact total. Every development session must have an exact start timestamp, exact stop timestamp, exact session duration, and cumulative exact duration. The older ~45-minute estimate stays visible as legacy history but never gets mixed into the exact total.

--- | --- |
| **Recorded hands-on development time** | **~45 minutes** (roughly 40-50 min currently documented) |
| **Historical work before time tracking** | Not reliably timed, so it is **not** added to the total |
| **Tracked build sessions** | 1 |
| **Current controller** | ESP32-C3 Super Mini |
| **Previous controller** | ESP32 DevKit V1 |
| **Verified hardware** | SH1106 OLED, TTP223 touch, I2S microphone |
| **Current physical form** | Cable Engineering Edition |
| **Current software milestone** | `SHOW_TIME HH:MM` with eyes-to-clock morph |
| **Next major milestone** | Spoken "EMI, what time is it?" -> local STT -> `SHOW_TIME` |
| **Last updated** | 2026-10-07 |

> **Time-tracking rule:** only time that can be estimated reasonably from a real build session gets added to the recorded total. Earlier work is kept in the history but does not get fake precision retroactively.

---

## Exact session log

| Session | Start | End | Exact duration | Notes |
| --- | --- | --- | --- | --- |
| 1 | 2026-10-07 14:39:12 +03:00 | ACTIVE | ACTIVE | Exact tracking enabled; continuing EMI development |

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
