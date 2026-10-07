#!/usr/bin/env bash
set -euo pipefail

ROOT="$HOME/.local/share"
VENV="$ROOT/emi-hub-venv"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REQUIREMENTS="$SCRIPT_DIR/requirements-voice.txt"

mkdir -p "$ROOT"

if ! python3 -m venv "$VENV" >/dev/null 2>&1; then
  echo
  echo "ERROR: Python venv support is missing."
  echo "Install it with:"
  echo "  sudo apt install python3-venv"
  exit 1
fi

"$VENV/bin/python" -m pip install --upgrade pip
"$VENV/bin/python" -m pip install -r "$REQUIREMENTS"

echo
echo "Downloading/checking the small English Vosk model..."

"$VENV/bin/python" - <<'PY'
from vosk import Model, SetLogLevel

SetLogLevel(-1)
Model(lang="en-us")
print("Vosk English model is ready.")
PY

MODEL_DIR="$HOME/.cache/vosk/vosk-model-small-en-us-0.15"

if [[ ! -d "$MODEL_DIR" ]]; then
  echo
  echo "ERROR: expected Vosk model directory was not created:"
  echo "  $MODEL_DIR"
  exit 1
fi

echo
echo "EMI constrained command recognizer is ready."
echo "Model:"
echo "  $MODEL_DIR"
