#!/usr/bin/env bash
set -euo pipefail

WHISPER_BIN="$HOME/whisper.cpp/build/bin/whisper-server"
WHISPER_MODEL="$HOME/whisper.cpp/models/ggml-tiny.en.bin"
SERVICE_SOURCE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/emi-whisper.service"
SERVICE_DIR="$HOME/.config/systemd/user"
SERVICE_DEST="$SERVICE_DIR/emi-whisper.service"

if [[ ! -x "$WHISPER_BIN" ]]; then
  echo "ERROR: whisper-server not found at:"
  echo "  $WHISPER_BIN"
  exit 1
fi

if [[ ! -f "$WHISPER_MODEL" ]]; then
  echo "ERROR: tiny.en model not found at:"
  echo "  $WHISPER_MODEL"
  exit 1
fi

mkdir -p "$SERVICE_DIR"
cp "$SERVICE_SOURCE" "$SERVICE_DEST"

systemctl --user daemon-reload
systemctl --user enable --now emi-whisper.service

sleep 1

if curl -fsS http://127.0.0.1:17841/ >/dev/null; then
  echo
  echo "EMI Whisper server is running."
  echo "Local endpoint: http://127.0.0.1:17841/inference"
  echo "Audio is decoded from the HTTP request in memory."
  echo "Do NOT enable whisper-server --convert; that path may use temp files."
else
  echo
  echo "EMI Whisper server did not answer yet."
  echo "Service status:"
  systemctl --user status emi-whisper.service --no-pager || true
  echo
  echo "Recent logs:"
  journalctl --user -u emi-whisper.service -n 30 --no-pager || true
  exit 1
fi
