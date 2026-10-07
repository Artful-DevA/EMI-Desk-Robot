# EMI Hub

`emi-hub` is the Raspberry Pi side of EMI.

The current hub handles two paths:

1. deterministic text intents such as `"Yo Emi time"`
2. authenticated in-memory WAV uploads from the ESP32-C3 for local Whisper transcription

Ordinary command audio and transcripts are ephemeral. Raw audio is not written to disk by EMI Hub.

## Current voice architecture

The planned live path is:

```text
ESP32-C3 I2S microphone
        |
        | 16 kHz mono WAV over trusted LAN
        v
POST /device/audio
        |
        | private shared token
        v
EMI Hub
        |
        | in-memory multipart request
        v
whisper-server on 127.0.0.1:17841
        |
        | transient transcript
        v
wake-word check: "Emi ..."
        |
        v
deterministic intent parser
        |
        v
SHOW_TIME HH:MM
        |
        v
C3 /device/command polling
```

The ESP32 microphone uploader is the next firmware step.

## Privacy behavior

For ordinary voice commands:

- raw audio stays in RAM
- EMI Hub does not create WAV files
- EMI Hub does not log transcripts
- recognized text is discarded after intent parsing
- voice commands must address `Emi` before an intent is allowed to execute
- the current device-audio endpoint is authenticated with the existing shared token

The bundled whisper.cpp server is deliberately started **without** `--convert`. Current whisper.cpp can decode uploaded WAV bytes directly from memory; the conversion path may use temporary files.

## EMI Hub endpoints

### `GET /health`

Localhost only.

Returns hub status and the configured local Whisper endpoint.

### `POST /intent`

Localhost only.

Accepts already-recognized text for deterministic testing.

Example:

```bash
curl -s \
  -H 'Content-Type: application/json' \
  -d '{"text":"Yo Emi time"}' \
  http://127.0.0.1:17840/intent
```

### `GET /device/command`

LAN-accessible but requires:

`X-EMI-Token`

The ESP32-C3 polls this endpoint for pending allow-listed commands.

### `POST /device/audio`

LAN-accessible but requires:

`X-EMI-Token`

Expected content type:

`audio/wav`

The request body is held in memory, forwarded to local Whisper in memory, transcribed, checked for an `Emi` wake phrase, parsed deterministically, and then discarded.

Current maximum upload size is 384000 bytes.

## Local Whisper service

The project now includes:

- `hub/emi-whisper.service`
- `hub/install-whisper-service.sh`

The service runs the already-built whisper.cpp server on:

`127.0.0.1:17841`

using:

`~/whisper.cpp/models/ggml-tiny.en.bin`

It is localhost-only; the ESP32 never talks directly to Whisper.

## Install / update on the Raspberry Pi

From the repository:

```bash
cd ~/EMI-Desk-Robot
git pull
chmod +x hub/install-whisper-service.sh
./hub/install-whisper-service.sh
```

Then restart the normal EMI Hub so it loads v0.4:

```bash
systemctl --user restart emi-hub
```

Check both services:

```bash
systemctl --user status emi-whisper --no-pager
systemctl --user status emi-hub --no-pager
```

Check the hub:

```bash
curl -s http://127.0.0.1:17840/health
```

The response should report version `0.4` and Whisper on `127.0.0.1:17841`.

## Test Whisper without EMI

The existing bundled JFK sample can verify the in-memory HTTP inference path:

```bash
curl -s http://127.0.0.1:17841/inference \
  -F file=@$HOME/whisper.cpp/samples/jfk.wav \
  -F response_format=json \
  -F language=en
```

This test sends the WAV file from disk because it is an existing public test sample. For actual EMI microphone requests, the hub receives and forwards the WAV bytes entirely in memory.

## Current deterministic time phrases

Examples include:

- `Emi, what time is it?`
- `Emi, what's the time?`
- `Emi, tell me the time`
- `Yo Emi time`
- `Hey Emi, could you tell me the time?`

For the voice endpoint, the transcript must contain the wake address `Emi` after any simple greeting. The exact alias `Emmy` is also accepted because Whisper may use that spelling for the spoken name. Background speech such as `"what time is it?"` by itself does not trigger a command.

## Security boundary

The current device transport is a trusted-home-LAN prototype.

Do not expose port 17840 or 17841 directly to the internet.

Before EMI gains privileged desktop actions, the transport should move to a stronger encrypted/authenticated design.
