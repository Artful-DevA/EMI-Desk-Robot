#!/usr/bin/env python3

import hmac
import http.client
from concurrent.futures import ThreadPoolExecutor
import io
import json
import os
import queue
import re
import threading
import time
import uuid
import wave

from vosk import KaldiRecognizer, Model, SetLogLevel
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HOST = os.environ.get("EMI_HUB_HOST", "127.0.0.1")
PORT = int(os.environ.get("EMI_HUB_PORT", "17840"))
SHARED_TOKEN = os.environ.get("EMI_SHARED_TOKEN", "")

WHISPER_HOST = os.environ.get("EMI_WHISPER_HOST", "127.0.0.1")
WHISPER_PORT = int(os.environ.get("EMI_WHISPER_PORT", "17841"))

VOSK_MODEL_DIR = os.path.expanduser(
    os.environ.get(
        "EMI_VOSK_MODEL_DIR",
        "~/.cache/vosk/vosk-model-small-en-us-0.15",
    )
)

VOSK_MIN_WAKE_CONFIDENCE = float(
    os.environ.get(
        "EMI_VOSK_MIN_WAKE_CONFIDENCE",
        "0.35",
    )
)

VOSK_MIN_TIME_CONFIDENCE = float(
    os.environ.get(
        "EMI_VOSK_MIN_TIME_CONFIDENCE",
        "0.45",
    )
)

VOSK_MIN_TIMER_CONFIDENCE = float(
    os.environ.get(
        "EMI_VOSK_MIN_TIMER_CONFIDENCE",
        "0.45",
    )
)

# Two independent constrained recognizers are used for the current
# deterministic time-command milestone:
#
# 1. Did the clip contain the explicit spoken wake name "Emi"?
# 2. Did the clip contain the time intent?
#
# This deliberately makes word order irrelevant. "Emi time",
# "Time Emi", and longer natural forms all go through the same gates.
#
# Vosk's ordinary English acoustic spelling for EMI is "Emmy".
VOSK_WAKE_GRAMMAR = [
    "emmy",
    "[unk]",
]

VOSK_TIME_GRAMMAR = [
    "time",
    "what time is it",
    "what is the time",
    "tell me time",
    "tell me the time",
    "give me time",
    "give me the time",
    "do you know the time",
    "do you know what time it is",
    "[unk]",
]

VOSK_TIMER_GRAMMAR = [
    "timer",
    "set a timer",
    "set timer",
    "cancel timer",
    "stop timer",
    "time left",
    "how much time is left",
    "how long is left",
    "remaining time",
    "[unk]",
]


MAX_AUDIO_BYTES = 384000

if not SHARED_TOKEN:
    raise RuntimeError("EMI_SHARED_TOKEN is required")

GREETING_WORDS = {"hey", "yo", "hi", "hello", "okay", "ok"}
WAKE_WORDS = {"emi", "emmy", "emmie", "emmi"}
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

TIMER_LOCK = threading.Lock()
ACTIVE_TIMER_DEADLINE = None
ACTIVE_TIMER_SECONDS = 0
ACTIVE_TIMER_THREAD = None

MAX_TIMER_SECONDS = 24 * 60 * 60


SetLogLevel(-1)

if not os.path.isdir(
    VOSK_MODEL_DIR
):
    raise RuntimeError(
        "Vosk model is missing: "
        + VOSK_MODEL_DIR
    )

VOSK_MODEL = Model(
    VOSK_MODEL_DIR
)


def wav_pcm16_frames(
    wav_bytes: bytes,
):
    with wave.open(
        io.BytesIO(wav_bytes),
        "rb",
    ) as wav:
        if wav.getnchannels() != 1:
            raise ValueError(
                "expected mono WAV"
            )

        if wav.getsampwidth() != 2:
            raise ValueError(
                "expected 16-bit PCM WAV"
            )

        sample_rate = (
            wav.getframerate()
        )

        frames = wav.readframes(
            wav.getnframes()
        )

    return frames, sample_rate


def run_vosk_grammar(
    pcm_bytes: bytes,
    sample_rate: int,
    grammar,
):
    recognizer = KaldiRecognizer(
        VOSK_MODEL,
        sample_rate,
        json.dumps(
            grammar
        ),
    )

    recognizer.SetWords(
        True
    )

    for offset in range(
        0,
        len(pcm_bytes),
        4000,
    ):
        recognizer.AcceptWaveform(
            pcm_bytes[
                offset:
                offset + 4000
            ]
        )

    # Clean decoder tail. This especially helps a phrase-final "Emi".
    recognizer.AcceptWaveform(
        b"\x00\x00"
        * int(
            sample_rate
            * 0.35
        )
    )

    return json.loads(
        recognizer.FinalResult()
    )


def max_word_confidence(
    payload,
    target: str,
) -> float:
    best = 0.0

    for word in payload.get(
        "result",
        [],
    ):
        if not isinstance(
            word,
            dict,
        ):
            continue

        if (
            str(
                word.get(
                    "word",
                    "",
                )
            ).lower()
            != target
        ):
            continue

        best = max(
            best,
            float(
                word.get(
                    "conf",
                    0.0,
                )
            ),
        )

    return best


def max_any_word_confidence(
    payload,
    targets,
) -> float:
    best = 0.0

    for target in targets:
        best = max(
            best,
            max_word_confidence(
                payload,
                target,
            ),
        )

    return best


def recognize_voice_gates(
    wav_bytes: bytes,
):
    pcm_bytes, sample_rate = (
        wav_pcm16_frames(
            wav_bytes
        )
    )

    # Wake, clock-time, and timer-intent gates are independent.
    # Run all three concurrently so timer support does not make the
    # already-working TIME command noticeably slower.
    with ThreadPoolExecutor(
        max_workers=3
    ) as executor:
        wake_future = executor.submit(
            run_vosk_grammar,
            pcm_bytes,
            sample_rate,
            VOSK_WAKE_GRAMMAR,
        )

        time_future = executor.submit(
            run_vosk_grammar,
            pcm_bytes,
            sample_rate,
            VOSK_TIME_GRAMMAR,
        )

        timer_future = executor.submit(
            run_vosk_grammar,
            pcm_bytes,
            sample_rate,
            VOSK_TIMER_GRAMMAR,
        )

        wake_payload = (
            wake_future.result()
        )

        time_payload = (
            time_future.result()
        )

        timer_payload = (
            timer_future.result()
        )

    wake_confidence = (
        max_word_confidence(
            wake_payload,
            "emmy",
        )
    )

    time_confidence = (
        max_word_confidence(
            time_payload,
            "time",
        )
    )

    timer_confidence = (
        max_any_word_confidence(
            timer_payload,
            {
                "timer",
                "left",
                "remaining",
                "cancel",
                "stop",
            },
        )
    )

    return (
        wake_confidence,
        time_confidence,
        timer_confidence,
    )


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

    return any(
        token in WAKE_WORDS
        for token in tokens
    )


def normalize_command_phrase(text: str) -> str:
    normalized = normalize_text(text)
    tokens = normalized.split()

    while tokens and tokens[0] in GREETING_WORDS:
        tokens.pop(0)

    # The explicit wake address may appear at the beginning, end,
    # or naturally inside the phrase:
    #   "Emi, what time is it?"
    #   "What time is it, Emi?"
    #   "Time, Emi."
    tokens = [
        token
        for token in tokens
        if token not in WAKE_WORDS
    ]

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


NUMBER_WORDS = {
    "zero": 0,
    "one": 1,
    "two": 2,
    "three": 3,
    "four": 4,
    "five": 5,
    "six": 6,
    "seven": 7,
    "eight": 8,
    "nine": 9,
    "ten": 10,
    "eleven": 11,
    "twelve": 12,
    "thirteen": 13,
    "fourteen": 14,
    "fifteen": 15,
    "sixteen": 16,
    "seventeen": 17,
    "eighteen": 18,
    "nineteen": 19,
    "twenty": 20,
    "thirty": 30,
    "forty": 40,
    "fifty": 50,
    "sixty": 60,
    "seventy": 70,
    "eighty": 80,
    "ninety": 90,
}

TIMER_UNITS = {
    "second": 1,
    "seconds": 1,
    "sec": 1,
    "secs": 1,
    "minute": 60,
    "minutes": 60,
    "min": 60,
    "mins": 60,
    "hour": 3600,
    "hours": 3600,
}


def parse_number_tokens(tokens) -> int:
    value = 0
    current = 0
    saw_number = False

    for token in tokens:
        if token in {
            "and",
        }:
            continue

        if token in {
            "a",
            "an",
        }:
            current += 1
            saw_number = True
            continue

        if token.isdigit():
            current += int(token)
            saw_number = True
            continue

        if token in NUMBER_WORDS:
            current += NUMBER_WORDS[token]
            saw_number = True
            continue

        if token == "hundred":
            current = max(
                1,
                current,
            ) * 100
            saw_number = True
            continue

        return 0

    if not saw_number:
        return 0

    value += current
    return value


def parse_duration_seconds(
    text: str,
) -> int:
    tokens = normalize_text(
        text
    ).split()

    total = 0

    for index, token in enumerate(
        tokens
    ):
        multiplier = TIMER_UNITS.get(
            token
        )

        if multiplier is None:
            continue

        number_tokens = []
        cursor = index - 1

        while cursor >= 0:
            candidate = tokens[
                cursor
            ]

            if (
                candidate.isdigit()
                or candidate in NUMBER_WORDS
                or candidate in {
                    "a",
                    "an",
                    "and",
                    "hundred",
                }
            ):
                number_tokens.insert(
                    0,
                    candidate,
                )
                cursor -= 1
                continue

            break

        amount = parse_number_tokens(
            number_tokens
        )

        if amount > 0:
            total += (
                amount
                * multiplier
            )

    if (
        total <= 0
        or total > MAX_TIMER_SECONDS
    ):
        return 0

    return total


def parse_timer_intent(
    text: str,
):
    normalized = normalize_text(
        text
    )

    tokens = normalized.split()
    token_set = set(tokens)

    if (
        "timer" in token_set
        and (
            "cancel" in token_set
            or "stop" in token_set
            or "clear" in token_set
        )
    ):
        return {
            "action": "CANCEL_TIMER",
        }

    if (
        "left" in token_set
        or "remaining" in token_set
        or (
            "how" in token_set
            and "long" in token_set
        )
    ):
        return {
            "action": "TIMER_LEFT",
        }

    duration_seconds = (
        parse_duration_seconds(
            normalized
        )
    )

    if (
        "timer" in token_set
        and duration_seconds > 0
    ):
        return {
            "action": "SET_TIMER",
            "duration_seconds": duration_seconds,
        }

    return None


def _timer_finished():
    global ACTIVE_TIMER_DEADLINE
    global ACTIVE_TIMER_SECONDS
    global ACTIVE_TIMER_THREAD

    with TIMER_LOCK:
        ACTIVE_TIMER_DEADLINE = None
        ACTIVE_TIMER_SECONDS = 0
        ACTIVE_TIMER_THREAD = None

    queue_command(
        "TIMER_DONE"
    )

    print(
        "timer=done; command queued",
        flush=True,
    )


def set_active_timer(
    duration_seconds: int,
):
    global ACTIVE_TIMER_DEADLINE
    global ACTIVE_TIMER_SECONDS
    global ACTIVE_TIMER_THREAD

    with TIMER_LOCK:
        if ACTIVE_TIMER_THREAD is not None:
            ACTIVE_TIMER_THREAD.cancel()

        ACTIVE_TIMER_SECONDS = (
            duration_seconds
        )

        ACTIVE_TIMER_DEADLINE = (
            time.monotonic()
            + duration_seconds
        )

        ACTIVE_TIMER_THREAD = (
            threading.Timer(
                duration_seconds,
                _timer_finished,
            )
        )

        ACTIVE_TIMER_THREAD.daemon = True
        ACTIVE_TIMER_THREAD.start()


def cancel_active_timer() -> bool:
    global ACTIVE_TIMER_DEADLINE
    global ACTIVE_TIMER_SECONDS
    global ACTIVE_TIMER_THREAD

    with TIMER_LOCK:
        had_timer = (
            ACTIVE_TIMER_DEADLINE
            is not None
        )

        if ACTIVE_TIMER_THREAD is not None:
            ACTIVE_TIMER_THREAD.cancel()

        ACTIVE_TIMER_DEADLINE = None
        ACTIVE_TIMER_SECONDS = 0
        ACTIVE_TIMER_THREAD = None

    return had_timer


def timer_remaining_seconds():
    with TIMER_LOCK:
        deadline = (
            ACTIVE_TIMER_DEADLINE
        )

    if deadline is None:
        return None

    remaining = int(
        max(
            0,
            (
                deadline
                - time.monotonic()
            )
            + 0.999,
        )
    )

    if remaining <= 0:
        return None

    return remaining


def apply_timer_intent(
    parsed,
):
    action = parsed.get(
        "action"
    )

    if action == "SET_TIMER":
        duration_seconds = int(
            parsed[
                "duration_seconds"
            ]
        )

        set_active_timer(
            duration_seconds
        )

        return {
            "ok": True,
            "intent": "TIMER_SET",
            "duration_seconds": duration_seconds,
            "remaining_seconds": duration_seconds,
        }

    if action == "TIMER_LEFT":
        remaining = (
            timer_remaining_seconds()
        )

        if remaining is None:
            return {
                "ok": False,
                "intent": "TIMER_LEFT",
                "error": "NO_TIMER",
            }

        return {
            "ok": True,
            "intent": "TIMER_LEFT",
            "remaining_seconds": remaining,
        }

    if action == "CANCEL_TIMER":
        cancelled = (
            cancel_active_timer()
        )

        return {
            "ok": cancelled,
            "intent": "TIMER_CANCEL",
            "error": (
                None
                if cancelled
                else "NO_TIMER"
            ),
        }

    return {
        "ok": False,
        "intent": None,
        "error": "NO_MATCH",
    }


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
        f"--{boundary}\r\n"
        'Content-Disposition: form-data; name="no_timestamps"\r\n'
        "\r\n"
        "true\r\n"
        f"--{boundary}\r\n"
        'Content-Disposition: form-data; name="token_timestamps"\r\n'
        "\r\n"
        "false\r\n"
        f"--{boundary}\r\n"
        'Content-Disposition: form-data; name="temperature"\r\n'
        "\r\n"
        "0.0\r\n"
        f"--{boundary}\r\n"
        'Content-Disposition: form-data; name="suppress_non_speech"\r\n'
        "\r\n"
        "true\r\n"
        f"--{boundary}--\r\n"
    ).encode("utf-8")

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
    server_version = "emi-hub/0.13"

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
                    "version": "0.13",
                    "whisper": (
                        f"{WHISPER_HOST}:"
                        f"{WHISPER_PORT}"
                    ),
                    "recognizer": "vosk-wake-time-timer-gates",
                },
            )
            return

        if self.path == "/device/timer":
            if not self._device_authorized():
                self._send_json(
                    401,
                    {
                        "ok": False,
                        "error": "UNAUTHORIZED",
                    },
                )
                return

            remaining = (
                timer_remaining_seconds()
            )

            self._send_json(
                200,
                {
                    "ok": True,
                    "active": (
                        remaining
                        is not None
                    ),
                    "remaining_seconds": (
                        remaining
                        if remaining
                        is not None
                        else 0
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

        recognize_started = time.monotonic()

        try:
            (
                wake_confidence,
                time_confidence,
                timer_confidence,
            ) = recognize_voice_gates(
                wav_bytes
            )

            recognize_ms = int(
                (
                    time.monotonic()
                    - recognize_started
                )
                * 1000
            )

            print(
                "voice grammar_ms="
                f"{recognize_ms} "
                "wake_conf="
                f"{wake_confidence:.2f} "
                "time_conf="
                f"{time_confidence:.2f} "
                "timer_conf="
                f"{timer_confidence:.2f}",
                flush=True,
            )
        except Exception as exc:
            print(
                "voice command recognition failed: "
                f"{type(exc).__name__}",
                flush=True,
            )

            self._send_json(
                503,
                {
                    "ok": False,
                    "error": "COMMAND_RECOGNIZER_UNAVAILABLE",
                },
            )
            return
        if (
            wake_confidence
            < VOSK_MIN_WAKE_CONFIDENCE
        ):
            wav_bytes = b""

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
                    "recognize_ms": recognize_ms,
                },
            )
            return

        # Timer-like phrases use Whisper only after the exact EMI
        # acoustic wake gate has passed. Whisper is used only to recover
        # duration/action text; the action itself is deterministic.
        if (
            timer_confidence
            >= VOSK_MIN_TIMER_CONFIDENCE
        ):
            whisper_started = (
                time.monotonic()
            )

            try:
                transcript = (
                    transcribe_wav_in_memory(
                        wav_bytes
                    )
                )

                whisper_ms = int(
                    (
                        time.monotonic()
                        - whisper_started
                    )
                    * 1000
                )

                parsed_timer = (
                    parse_timer_intent(
                        transcript
                    )
                )

                transcript = ""
            except Exception as exc:
                wav_bytes = b""

                print(
                    "timer transcription failed: "
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

            if parsed_timer is not None:
                wav_bytes = b""

                result = (
                    apply_timer_intent(
                        parsed_timer
                    )
                )

                result[
                    "recognize_ms"
                ] = recognize_ms

                result[
                    "whisper_ms"
                ] = whisper_ms

                print(
                    "voice intent="
                    f"{result.get('intent')} "
                    f"ok={result.get('ok')}",
                    flush=True,
                )

                self._send_json(
                    200,
                    result,
                )
                return

        wav_bytes = b""

        # If the timer gate was a false positive, fall back to the
        # established fast TIME path instead of rejecting the phrase.
        if (
            time_confidence
            < VOSK_MIN_TIME_CONFIDENCE
        ):
            print(
                "voice=no_known_intent",
                flush=True,
            )

            self._send_json(
                200,
                {
                    "ok": False,
                    "intent": None,
                    "error": "NO_MATCH",
                    "recognize_ms": recognize_ms,
                },
            )
            return

        now = datetime.now().astimezone()
        hhmm = now.strftime("%H:%M")

        result = {
            "ok": True,
            "intent": "TIME",
            "time": hhmm,
            "command": f"SHOW_TIME {hhmm}",
            "recognize_ms": recognize_ms,
        }

        queue_command(
            result["command"]
        )

        print(
            "voice intent=TIME matched; "
            "command queued",
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
        f"emi-hub 0.13 listening on "
        f"{HOST}:{PORT}",
        flush=True,
    )

    print(
        "voice path uses in-memory WAV -> "
        "Vosk independent wake + time gates; "
        f"Whisper remains available at "
        f"{WHISPER_HOST}:{WHISPER_PORT} "
        "for future free-form commands",
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
