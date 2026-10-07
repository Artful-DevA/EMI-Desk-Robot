#!/usr/bin/env bash
set -euo pipefail

mkdir -p "$HOME/.config/systemd/user"
cp "$(dirname "$0")/emi-hub.service" "$HOME/.config/systemd/user/emi-hub.service"

systemctl --user daemon-reload
systemctl --user enable --now emi-hub.service

echo
echo "EMI Hub installed."
echo "Check it with:"
echo "  systemctl --user status emi-hub.service"
echo "  curl http://127.0.0.1:17840/health"
