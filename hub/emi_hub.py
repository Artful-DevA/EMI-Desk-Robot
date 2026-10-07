#!/usr/bin/env python3

import json
import os
import re
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HOST = os.environ.get("EMI_HUB_HOST", "127.0.0.1")
PORT = int(os.environ.get("EMI_HUB_PORT", "17840"))

TIME_PATTERNS = [
    re.compile(r"^(?:emi\s+)?what time is it$"),
    re.compile(r"^(?:emi\s+)?what is the time$"),
    re.compile(r"^(?:emi\s+)?whats the time$"),
    re.compile(r"^(?:emi\s+)?can you tell me the time$"),
    re.compile(r"^(?:emi\s+)?tell me the time$"),
    re.compile(r"^(?:emi\s+)?time$"),
]


def normalize_text(text: str) -> str:
    text = text.lower().replace("’", "'")
    text = re.sub(r"[^a-z0-9' ]+", " ", text)
    text = re.sub(r"\s+", " ", text).strip()
    text = text.replace("what's", "whats")
    return text


def parse_intent(text: str):
    normalized = normalize_text(text)

    for pattern in TIME_PATTERNS:
        if pattern.fullmatch(normalized):
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
    server_version = "emi-hub/0.1"

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
                    "version": "0.1",
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
    print(f"emi-hub 0.1 listening on {HOST}:{PORT}", flush=True)

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
