#!/usr/bin/env bash
set -euo pipefail

SERVICE_DIR="$HOME/.config/systemd/user"
CONFIG_DIR="$HOME/.config/emi-hub"
ENV_FILE="$CONFIG_DIR/emi-hub.env"
HEALTH_URL="http://127.0.0.1:17840/health"

mkdir -p "$SERVICE_DIR"
mkdir -p "$CONFIG_DIR"

if [ ! -f "$ENV_FILE" ]; then
  TOKEN="$(python3 -c 'import secrets; print(secrets.token_hex(32))')"

  cat > "$ENV_FILE" <<EOF
EMI_HUB_HOST=0.0.0.0
EMI_HUB_PORT=17840
EMI_SHARED_TOKEN=$TOKEN
EOF

  chmod 600 "$ENV_FILE"
fi

cp "$(dirname "$0")/emi-hub.service" "$SERVICE_DIR/emi-hub.service"

systemctl --user daemon-reload
systemctl --user enable emi-hub.service
systemctl --user restart emi-hub.service

echo
echo "EMI Hub installed/updated."
echo "Waiting for the service to become ready..."

READY=0

for _ in $(seq 1 20); do
  if curl -fsS "$HEALTH_URL" >/dev/null 2>&1; then
    READY=1
    break
  fi

  sleep 0.25
done

if [ "$READY" -ne 1 ]; then
  echo
  echo "EMI Hub did not become healthy."
  echo
  systemctl --user status emi-hub.service --no-pager -l || true
  echo
  echo "Recent EMI Hub logs:"
  journalctl --user -u emi-hub.service -n 30 --no-pager || true
  exit 1
fi

echo
echo "EMI Hub is healthy:"
curl -fsS "$HEALTH_URL"
echo

echo
echo "Private C3 token is stored in:"
echo "  $ENV_FILE"
echo
echo "Do not paste that token into GitHub or chat."
