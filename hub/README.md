# EMI Hub

`emi-hub` is the Raspberry Pi side of EMI.

The current version is deliberately tiny. It does not handle live audio yet. It accepts already-recognized text, maps a small allow-listed set of phrases to the deterministic `TIME` intent, queues the resulting `SHOW_TIME HH:MM` command, and lets the ESP32-C3 retrieve that command over the LAN.

## Current flow

```
recognized text
    |
    v
emi-hub /intent
    |
    v
TIME
    |
    v
SHOW_TIME HH:MM
```

Accepted examples:

- `Emi, what time is it?`
- `Emi, what's the time?`
- `Emi, can you tell me the time?`
- `Emi, tell me the time`
- `Emi, time?`
- `Yo Emi time`
- `Yo Emi time is?`
- `Hey Emi, could you tell me the time?`

The service intentionally does not log the recognized sentence. Ordinary commands remain ephemeral.

## Install on the Raspberry Pi

Clone the repository to `~/EMI-Desk-Robot`, then run:

```bash
cd ~/EMI-Desk-Robot
bash hub/install-user-service.sh
```

Check health:

```bash
curl http://127.0.0.1:17840/health
```

Test the time intent:

```bash
curl -s \
  -H 'Content-Type: application/json' \
  -d '{"text":"Emi, can you tell me the time?"}' \
  http://127.0.0.1:17840/intent
```

Expected shape:

```json
{"ok":true,"intent":"TIME","time":"14:37","command":"SHOW_TIME 14:37"}
```

The exact time will come from the Raspberry Pi system clock.

## Security state

Version 0.3 binds to the Pi's network interfaces so the C3 can reach it, but the speech/intent endpoint remains loopback-only. The C3 command endpoint requires a randomly generated shared token stored outside the repository. The token is never committed to GitHub.

The current transport is suitable for the trusted home-LAN prototype and only carries allow-listed display commands. It is not intended to be exposed directly to the public internet.

## Audio privacy

Live microphone audio is not implemented in this version.

When audio streaming is added:

- raw audio must remain in RAM
- raw audio must never be written to disk
- ordinary command transcripts must not be retained
- only explicit notes mode may retain text


## ESP32-C3 connection

Run the installer again after pulling v0.3:

```bash
cd ~/EMI-Desk-Robot
git pull
bash hub/install-user-service.sh
```

The installer creates a private token in:

```
~/.config/emi-hub/emi-hub.env
```

Get the Pi LAN address:

```bash
hostname -I | awk '{print $1}'
```

Get the private token for copying into the C3's local `secrets.h`:

```bash
grep '^EMI_SHARED_TOKEN=' ~/.config/emi-hub/emi-hub.env | cut -d= -f2-
```

Do not post the Wi-Fi password or token in chat, screenshots, or GitHub.

Copy `firmware/emi_c3/secrets.example.h` to `secrets.h`, then fill in the Wi-Fi SSID/password, Pi LAN IP, and token. `secrets.h` is ignored by Git.
