#!/usr/bin/env python3

import json
import os
import re
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HOST = os.environ.get("EMI_HUB_HOST", "127.0.0.1")
PORT = int(os.environ.get("EMI_HUB_PORT", "17840"))

WAKE_WORDS = {"emi"}
GREETING_WORDS = {"hey", "yo", "hi", "hello", "okay", "ok"}
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


def normalize_text(text: str) -> str:
    text = text.lower().replace("’", "'")
    text = re.sub(r"[^a-z0-9' ]+", " ", text)
    text = re.sub(r"\s+", " ", text).strip()
    text = text.replace("what's", "whats")
    return text


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


class Handler(BaseHTTPRequestHandler):
    server_version = "emi-hub/0.2"

    def _send_json(self, status: int, payload):
        body = json.dumps(payload, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/health":
            self._send_json(
                200,
                {
                    "ok": True,
                    "service": "emi-hub",
                    "version": "0.2",
                },
            )
            return

        self._send_json(404, {"ok": False, "error": "NOT_FOUND"})

    def do_POST(self):
        if self.path != "/intent":
            self._send_json(404, {"ok": False, "error": "NOT_FOUND"})
            return

        try:
            content_length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            self._send_json(400, {"ok": False, "error": "BAD_LENGTH"})
            return

        if content_length <= 0 or content_length > 4096:
            self._send_json(400, {"ok": False, "error": "BAD_BODY_SIZE"})
            return

        raw = self.rfile.read(content_length)

        try:
            payload = json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            self._send_json(400, {"ok": False, "error": "BAD_JSON"})
            return

        text = payload.get("text")

        if not isinstance(text, str) or not text.strip():
            self._send_json(400, {"ok": False, "error": "MISSING_TEXT"})
            return

        result = parse_intent(text)

        if result["ok"]:
            # Do not log the original transcript. Ordinary commands are ephemeral.
            print("intent=TIME matched", flush=True)
            self._send_json(200, result)
        else:
            print("intent=NO_MATCH", flush=True)
            self._send_json(200, result)

    def log_message(self, format, *args):
        # Suppress HTTP request logging so recognized speech text never leaks
        # into logs through URLs or accidental debug output.
        return


def main():
    server = ThreadingHTTPServer((HOST, PORT), Handler)
    print(f"emi-hub 0.2 listening on {HOST}:{PORT}", flush=True)

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
