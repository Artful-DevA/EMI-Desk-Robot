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

echo
echo "Waiting for whisper-server to load the model..."

for i in {1..15}; do
  if curl -fsS http://127.0.0.1:17841/ >/dev/null 2>&1; then
    echo
    echo "EMI Whisper server is running."
    echo "Local endpoint: http://127.0.0.1:17841/inference"
    echo "Audio is decoded from the HTTP request in memory."
    exit 0
  fi

  if ! systemctl --user is-active --quiet emi-whisper.service; then
    echo
    echo "EMI Whisper server exited before becoming ready."
    echo
    echo "Full service status:"
    systemctl --user status emi-whisper.service --no-pager -l || true
    exit 1
  fi

  sleep 1
done

echo
echo "EMI Whisper server is still starting or did not answer in time."
echo
echo "Full service status:"
systemctl --user status emi-whisper.service --no-pager -l || true
exit 1
