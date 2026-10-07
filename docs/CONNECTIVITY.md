# Connectivity Architecture

EMI should use different transports for different jobs instead of forcing everything through Bluetooth.

## Core principle

- **Wi-Fi:** main runtime data path
- **Bluetooth LE:** phone pairing, provisioning, setup, lightweight control
- **Tailscale:** private connection between trusted full computers such as the phone, Raspberry Pi, desktop, and laptop
- **Raspberry Pi:** speech-to-text and central EMI hub
- **Desktop / laptop / phone:** clients and possible audio-output/control targets

The ESP32 itself does not need to run Tailscale.


## Current Wi-Fi hardware note

The active C3 can scan the correct 2.4 GHz AP normally, but at the previous transmit-power behavior it repeatedly failed authentication with `AUTH_EXPIRE (2)`.

The verified board-specific workaround is:

`WiFi.setTxPower(WIFI_POWER_8_5dBm)`

That setting is now part of the normal firmware and should be preserved unless the controller hardware changes.

## Home topology

### Current prototype command transport

The current ESP32-C3 prototype uses an outbound polling connection to the Raspberry Pi hub, and this path has now been **physically validated end-to-end**.

1. The C3 joins the home 2.4 GHz Wi-Fi network.
2. The current board applies `WIFI_POWER_8_5dBm`, which is required for reliable authentication on this specific ESP32-C3 Super Mini.
3. The Pi hub listens on port 17840.
4. The local-only intent parser queues allow-listed commands such as `SHOW_TIME HH:MM`.
5. The C3 polls `/device/command` roughly four times per second.
6. The device endpoint requires a randomly generated shared token.
7. Wi-Fi credentials and the token live only in local secret files and are excluded from Git.
8. The C3 passes received commands into the face loop through a FreeRTOS queue.

The first proven physical flow is:

**Pi intent request -> hub queue -> authenticated C3 poll -> C3 command parser -> OLED clock animation**

This avoids needing the Pi to know the C3's changing DHCP address. The same C3 -> Pi direction is the planned path for live microphone audio.

The current HTTP transport is for the trusted home-LAN prototype and only carries low-risk allow-listed commands. It must not be exposed directly to the internet; stronger encrypted transport should be used before privileged control is added.


At home:

1. EMI joins home Wi-Fi.
2. EMI talks directly to the Raspberry Pi over the LAN.
3. The desktop and laptop both run an EMI Agent and connect to the Raspberry Pi hub.
4. Both computers remain connected simultaneously.
5. The hub chooses which computer is the active action/audio target.

Default priority:

1. desktop
2. laptop
3. phone

If both desktop and laptop are online, desktop wins until the user explicitly switches.

Example future commands:

- `emi target desktop`
- `emi target laptop`
- `emi target phone`
- `emi target auto`

In `auto`, the default priority order is used.

A manual selection should remain selected until the user switches again or the selected device disconnects, at which point EMI can fall back automatically.

## Separate roles

Do not treat "connected device" as one single role.

The hub should track at least:

- **speech backend** - normally the Raspberry Pi
- **control target** - desktop or laptop
- **audio output** - desktop, laptop, or phone
- **setup device** - normally the phone

This allows, for example, the laptop to be the control target while speech still runs on the home Raspberry Pi and EMI speaks through the phone.

## Phone app

Bluetooth LE is best used to establish trust and configure EMI.

The phone app should be able to:

- pair with EMI
- provision Wi-Fi credentials
- store several known Wi-Fi profiles on EMI
- show connection status
- select desktop/laptop/phone target
- select microphone mode
- act as EMI's speaker when no computer is available
- act as a network relay to the home Raspberry Pi while travelling

Runtime speech audio should use Wi-Fi rather than BLE.

## Wi-Fi provisioning

When EMI encounters a new network:

1. Open the phone app.
2. Connect to EMI over BLE.
3. Select or enter the new Wi-Fi network.
4. The app securely transfers the SSID and password to EMI.
5. EMI stores the profile in ESP32 nonvolatile storage.
6. EMI attempts the new Wi-Fi connection and reports success/failure back to the app.

EMI should remember multiple networks instead of replacing the previous one each time.

For hotels with captive portals, using the phone hotspot is usually simpler and more reliable than making an ESP32 deal with the hotel's browser login.

## Travel mode with the Raspberry Pi staying home

The recommended portable architecture is:

1. EMI connects to the phone hotspot or another local Wi-Fi network.
2. The phone app and EMI communicate locally over Wi-Fi.
3. The phone runs Tailscale.
4. The home Raspberry Pi also runs Tailscale.
5. The phone app acts as EMI's edge relay.
6. Microphone audio travels:
   EMI -> local Wi-Fi -> phone -> Tailscale -> home Raspberry Pi.
7. Speech-to-text results travel back through the same route.
8. If no desktop/laptop is available, the phone becomes EMI's speaker and primary client.

This keeps the Raspberry Pi at home while allowing EMI to retain local/private speech processing on hardware controlled by the user.

The main limitation is that remote STT now depends on internet connectivity between the phone and home.

## Friend's house / hotel / class

Recommended default:

- enable phone hotspot
- EMI connects to the known hotspot profile
- phone becomes local gateway and speaker
- phone reaches the home Pi through Tailscale

This avoids captive portals, unknown LAN restrictions, and client-isolation problems.

The phone app can still provision a friend's normal Wi-Fi if desired.

## Desktop and laptop agents

Both Ubuntu machines can run the same small `emi-agent` service.

Each agent registers:

- device ID
- friendly name
- online/offline status
- capabilities
- audio-output availability
- supported commands

Example capabilities:

- media control
- volume
- current song
- CPU temperature
- open an approved application
- lock screen
- timers

No agent should accept arbitrary shell commands from EMI.

## Microphone modes

Recommended states:

- **off** - microphone data is ignored
- **ptt** - speech is streamed only during an explicit listening action
- **always** - continuous listening / VAD mode

These should be controllable from Ubuntu:

- `emi listen off`
- `emi listen ptt`
- `emi listen always`

The phone app should expose the same three-state control.

### Touch sensor behavior

Do not use ordinary pet taps to toggle microphone modes. Petting and privacy controls should not be ambiguous.

Recommended behavior:

- ordinary taps/strokes remain petting
- in PTT mode, a deliberate long hold can temporarily listen while held
- changing to **always listen** should primarily be done from the terminal/app
- if a touch-only toggle is ever added, use a deliberately hard-to-trigger gesture and show an obvious OLED confirmation

## Always-listen and note taking

For class-note mode, use an explicit session:

- `emi notes start`
- `emi notes stop`

During the session:

1. EMI streams audio through the phone/Tailscale route to the home Pi.
2. The Pi performs local speech-to-text.
3. Raw audio is discarded by default unless the user explicitly enables recording.
4. Transcript/notes are stored separately.
5. EMI displays a persistent listening/notes indicator.

Recording or transcribing other people can be subject to school rules, venue rules, and local consent laws, so the listening state should never be hidden.

## Failure behavior

EMI should remain usable when networking fails.

If the Pi cannot be reached:

- eyes and touch personality continue locally
- EMI shows a small disconnected state
- PC-control requests fail safely
- the phone app can explain which link is unavailable

Later, a limited on-device command set could remain available even without the Pi.
