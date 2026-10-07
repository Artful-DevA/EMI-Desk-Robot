# EMI Hub

`emi-hub` is the Raspberry Pi side of EMI.

The current version is deliberately tiny. It does not handle live audio yet. It accepts already-recognized text, maps a small allow-listed set of phrases to the deterministic `TIME` intent, and returns the exact C3 command needed by the display firmware.

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

Version 0.1 binds to `127.0.0.1` only. It is intentionally not exposed to the LAN yet.

The C3 transport will be added separately with authentication rather than opening an unauthenticated HTTP endpoint to the network.

## Audio privacy

Live microphone audio is not implemented in this version.

When audio streaming is added:

- raw audio must remain in RAM
- raw audio must never be written to disk
- ordinary command transcripts must not be retained
- only explicit notes mode may retain text
