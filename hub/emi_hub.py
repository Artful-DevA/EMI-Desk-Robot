#!/usr/bin/env python3

import hmac
import http.client
import json
import os
import queue
import re
import uuid
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HOST = os.environ.get("EMI_HUB_HOST", "127.0.0.1")
PORT = int(os.environ.get("EMI_HUB_PORT", "17840"))
SHARED_TOKEN = os.environ.get("EMI_SHARED_TOKEN", "")

WHISPER_HOST = os.environ.get("EMI_WHISPER_HOST", "127.0.0.1")
WHISPER_PORT = int(os.environ.get("EMI_WHISPER_PORT", "17841"))

MAX_AUDIO_BYTES = 384000

if not SHARED_TOKEN:
    raise RuntimeError("EMI_SHARED_TOKEN is required")

GREETING_WORDS = {"hey", "yo", "hi", "hello", "okay", "ok"}
WAKE_WORDS = {"emi", "emmy"}
POLITE_WORDS = {"please", "just"}

TIME_FORMS = {
    "time",
    "time is",
    "what time",
    "what time is it",
    "what is the time",
    "whats the time",
    "whats time",
    "tell me time",
    "tell me the time",
    "can you tell me time",
    "can you tell me the time",
    "could you tell me time",
    "could you tell me the time",
    "would you tell me time",
    "would you tell me the time",
    "give me time",
    "give me the time",
    "do you know the time",
    "do you know what time it is",
}

COMMANDS = queue.Queue(maxsize=16)


def normalize_text(text: str) -> str:
    text = text.lower().replace("’", "'")
    text = re.sub(r"[^a-z0-9' ]+", " ", text)
    text = re.sub(r"\s+", " ", text).strip()
    text = text.replace("what's", "whats")
    return text


def has_emi_wake_word(text: str) -> bool:
    tokens = normalize_text(text).split()

    while tokens and tokens[0] in GREETING_WORDS:
        tokens.pop(0)

    return bool(tokens) and tokens[0] in WAKE_WORDS


def normalize_command_phrase(text: str) -> str:
    normalized = normalize_text(text)
    tokens = normalized.split()

    while tokens and tokens[0] in GREETING_WORDS:
        tokens.pop(0)

    if tokens and tokens[0] in WAKE_WORDS:
        tokens.pop(0)

    while tokens and tokens[0] in POLITE_WORDS:
        tokens.pop(0)

    while tokens and tokens[-1] in POLITE_WORDS:
        tokens.pop()

    return " ".join(tokens)


def parse_intent(text: str):
    command_phrase = normalize_command_phrase(text)

    if command_phrase in TIME_FORMS:
        now = datetime.now().astimezone()
        hhmm = now.strftime("%H:%M")
        return {
            "ok": True,
            "intent": "TIME",
            "time": hhmm,
            "command": f"SHOW_TIME {hhmm}",
        }

    return {
        "ok": False,
        "intent": None,
        "error": "NO_MATCH",
    }


def queue_command(command: str):
    try:
        COMMANDS.put_nowait(command)
        return
    except queue.Full:
        pass

    try:
        COMMANDS.get_nowait()
    except queue.Empty:
        pass

    COMMANDS.put_nowait(command)


def transcribe_wav_in_memory(wav_bytes: bytes) -> str:
    boundary = "----emi-" + uuid.uuid4().hex

    prefix = (
        f"--{boundary}\r\n"
        'Content-Disposition: form-data; name="file"; filename="emi.wav"\r\n'
        "Content-Type: audio/wav\r\n"
        "\r\n"
    ).encode("ascii")

    fields = (
        f"\r\n--{boundary}\r\n"
        'Content-Disposition: form-data; name="response_format"\r\n'
        "\r\n"
        "json\r\n"
        f"--{boundary}\r\n"
        'Content-Disposition: form-data; name="language"\r\n'
        "\r\n"
        "en\r\n"
        f"--{boundary}--\r\n"
    ).encode("ascii")

    body = prefix + wav_bytes + fields

    conn = http.client.HTTPConnection(
        WHISPER_HOST,
        WHISPER_PORT,
        timeout=30,
    )

    try:
        conn.request(
            "POST",
            "/inference",
            body=body,
            headers={
                "Content-Type": f"multipart/form-data; boundary={boundary}",
                "Content-Length": str(len(body)),
                "Connection": "close",
            },
        )

        response = conn.getresponse()
        response_body = response.read()

        if response.status != 200:
            raise RuntimeError(
                f"whisper HTTP {response.status}"
            )

        payload = json.loads(
            response_body.decode("utf-8")
        )

        text = payload.get("text", "")

        if not isinstance(text, str):
            return ""

        return text.strip()

    finally:
        conn.close()


class Handler(BaseHTTPRequestHandler):
    server_version = "emi-hub/0.4"

    def _send_bytes(self, status: int, body: bytes, content_type: str):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()

        if body:
            self.wfile.write(body)

    def _send_json(self, status: int, payload):
        body = json.dumps(
            payload,
            separators=(",", ":"),
        ).encode("utf-8")

        self._send_bytes(
            status,
            body,
            "application/json",
        )

    def _is_loopback(self):
        return self.client_address[0] in {
            "127.0.0.1",
            "::1",
        }

    def _device_authorized(self):
        supplied = self.headers.get(
            "X-EMI-Token",
            "",
        )

        return (
            bool(supplied)
            and hmac.compare_digest(
                supplied,
                SHARED_TOKEN,
            )
        )

    def _read_content_length(self):
        try:
            return int(
                self.headers.get(
                    "Content-Length",
                    "0",
                )
            )
        except ValueError:
            return -1

    def do_GET(self):
        if self.path == "/health":
            if not self._is_loopback():
                self._send_json(
                    403,
                    {
                        "ok": False,
                        "error": "LOCAL_ONLY",
                    },
                )
                return

            self._send_json(
                200,
                {
                    "ok": True,
                    "service": "emi-hub",
                    "version": "0.4",
                    "whisper": (
                        f"{WHISPER_HOST}:"
                        f"{WHISPER_PORT}"
                    ),
                },
            )
            return

        if self.path == "/device/command":
            if not self._device_authorized():
                self._send_json(
                    401,
                    {
                        "ok": False,
                        "error": "UNAUTHORIZED",
                    },
                )
                return

            try:
                command = COMMANDS.get_nowait()
            except queue.Empty:
                self._send_bytes(
                    204,
                    b"",
                    "text/plain",
                )
                return

            self._send_bytes(
                200,
                command.encode("utf-8"),
                "text/plain",
            )
            return

        self._send_json(
            404,
            {
                "ok": False,
                "error": "NOT_FOUND",
            },
        )

    def do_POST(self):
        if self.path == "/device/audio":
            self._handle_device_audio()
            return

        if self.path == "/intent":
            self._handle_local_intent()
            return

        self._send_json(
            404,
            {
                "ok": False,
                "error": "NOT_FOUND",
            },
        )

    def _handle_device_audio(self):
        if not self._device_authorized():
            self._send_json(
                401,
                {
                    "ok": False,
                    "error": "UNAUTHORIZED",
                },
            )
            return

        content_type = self.headers.get(
            "Content-Type",
            "",
        )

        if not content_type.startswith(
            "audio/wav"
        ):
            self._send_json(
                415,
                {
                    "ok": False,
                    "error": "EXPECTED_AUDIO_WAV",
                },
            )
            return

        content_length = (
            self._read_content_length()
        )

        if (
            content_length <= 44
            or content_length > MAX_AUDIO_BYTES
        ):
            self._send_json(
                400,
                {
                    "ok": False,
                    "error": "BAD_AUDIO_SIZE",
                },
            )
            return

        wav_bytes = self.rfile.read(
            content_length
        )

        if len(wav_bytes) != content_length:
            self._send_json(
                400,
                {
                    "ok": False,
                    "error": "SHORT_AUDIO_BODY",
                },
            )
            return

        try:
            transcript = (
                transcribe_wav_in_memory(
                    wav_bytes
                )
            )
        except Exception as exc:
            print(
                "voice transcription failed: "
                f"{type(exc).__name__}",
                flush=True,
            )

            self._send_json(
                503,
                {
                    "ok": False,
                    "error": "WHISPER_UNAVAILABLE",
                },
            )
            return
        finally:
            wav_bytes = b""

        if not transcript:
            print(
                "voice=no_speech",
                flush=True,
            )

            self._send_json(
                200,
                {
                    "ok": False,
                    "intent": None,
                    "error": "NO_SPEECH",
                },
            )
            return

        if not has_emi_wake_word(
            transcript
        ):
            print(
                "voice=no_wake_word",
                flush=True,
            )

            self._send_json(
                200,
                {
                    "ok": False,
                    "intent": None,
                    "error": "NO_WAKE_WORD",
                },
            )
            return

        result = parse_intent(
            transcript
        )

        transcript = ""

        if result["ok"]:
            queue_command(
                result["command"]
            )

            print(
                "voice intent=TIME matched; "
                "command queued",
                flush=True,
            )
        else:
            print(
                "voice intent=NO_MATCH",
                flush=True,
            )

        self._send_json(
            200,
            result,
        )

    def _handle_local_intent(self):
        if not self._is_loopback():
            self._send_json(
                403,
                {
                    "ok": False,
                    "error": "LOCAL_ONLY",
                },
            )
            return

        content_length = (
            self._read_content_length()
        )

        if (
            content_length <= 0
            or content_length > 4096
        ):
            self._send_json(
                400,
                {
                    "ok": False,
                    "error": "BAD_BODY_SIZE",
                },
            )
            return

        raw = self.rfile.read(
            content_length
        )

        try:
            payload = json.loads(
                raw.decode("utf-8")
            )
        except (
            UnicodeDecodeError,
            json.JSONDecodeError,
        ):
            self._send_json(
                400,
                {
                    "ok": False,
                    "error": "BAD_JSON",
                },
            )
            return

        text = payload.get("text")

        if (
            not isinstance(text, str)
            or not text.strip()
        ):
            self._send_json(
                400,
                {
                    "ok": False,
                    "error": "MISSING_TEXT",
                },
            )
            return

        result = parse_intent(
            text
        )

        text = ""

        if result["ok"]:
            queue_command(
                result["command"]
            )

            print(
                "intent=TIME matched; "
                "command queued",
                flush=True,
            )
        else:
            print(
                "intent=NO_MATCH",
                flush=True,
            )

        self._send_json(
            200,
            result,
        )

    def log_message(
        self,
        format,
        *args
    ):
        # Suppress request logging so recognized speech
        # and request paths never leak into normal logs.
        return


def main():
    server = ThreadingHTTPServer(
        (HOST, PORT),
        Handler,
    )

    print(
        f"emi-hub 0.4 listening on "
        f"{HOST}:{PORT}",
        flush=True,
    )

    print(
        "voice path uses in-memory WAV -> "
        f"whisper-server at "
        f"{WHISPER_HOST}:{WHISPER_PORT}",
        flush=True,
    )

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
